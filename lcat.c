/**
 * @file lcat.c 
 * 
 * Copyright (C) 2009 Arnold Noronha <arnstein87 AT gmail DOT com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. 
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <string.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netdb.h>
#include <getopt.h>
#include <string.h>
#include <assert.h>
#include <limits.h>
#include <linux/netfilter_ipv4.h>

/* storing an integer in a pointer. Apparently though, the following
 * is not entirely portable. e.g., Glib generates similar functions
 * depending on the system type. Depending on your system you might
 * want to remove the (long) from here. */
#define LCAT_POINTER_TO_INT(p) ((int) (long) p)
#define LCAT_INT_TO_POINTER(i) ((void*) (long) i)

int localport = 8002;
char *gateway = NULL;
int gatewayport = 22;
int speed = 0;
int sleeptime = 20;
int _servfd;
int enable_iptables = 1; /* otherwise we're just running in a single gateway mode */

void end_conn (int fd)
{
	shutdown (fd, SHUT_RDWR);
	close (fd);
	io_loop_remove_fd (fd);
}

static void pause_if_req (int bytes)
{
	static time_t last_clock;
	static int bytes_since_clock = 0;
	int _sleeptime = speed ? bytes*1000/speed : 0;
	struct timeval t;

	if (speed == 0) return;
	if (!last_clock) last_clock = time (NULL);
	
	bytes_since_clock += bytes;
	if (bytes_since_clock > 10*1024*1024) {
		time_t cur = time(NULL);
		fprintf (stderr, "Last %d MB in %d seconds at %d kbps\n", bytes_since_clock/1024/1024, 
			 (int) (cur-last_clock),
			 (int) ((bytes_since_clock/1024)/(cur-last_clock)));
		last_clock = cur;
		bytes_since_clock = 0;
	}
	
	_sleeptime = (_sleeptime > sleeptime ? _sleeptime : sleeptime);
	t.tv_sec = _sleeptime/1000;
	t.tv_usec = (_sleeptime % 1000)*1000;
	select (1, NULL, NULL, NULL, &t); /* basically usleep */
}

static void rw_tunnel_cb (int i, void* fd_to) 
{
	int ws = LCAT_POINTER_TO_INT (fd_to);
	char _buf [20000];
	int len = read (i, _buf, sizeof(_buf));
	int len2;
	
	if (len < 1) {
		end_conn (ws);
		end_conn (i);
		return;
	}
	
	len2 = write (ws, _buf, len);
	if (len2 < 1) {
		end_conn (ws);
		end_conn (i);
		return;
	}
	assert (len2 == len);
	pause_if_req (len);
}

/* connect to the desting that fd was originally bound to */
static int 
connect_to_dest (int fd)
{
	struct sockaddr_in client;
	int len = sizeof (client);

	getsockopt (fd, SOL_IP, SO_ORIGINAL_DST, (struct sockaddr*) &client, &len);
	int ret = socket (AF_INET, SOCK_STREAM, 0);
	if (connect (ret, (struct sockaddr*) &client, sizeof (client)) == 0) {
		return ret;
	}
	else return -1;
}

void 
acceptconn (int servfd, void* userdata)
{
	int r = accept (servfd, NULL, NULL), g;
	if (r < 0) {
		perror ("Accept failed");
		return;
	} else {
		if (!enable_iptables)
			g = client2server_socket (gateway, gatewayport);
		else 
			g = connect_to_dest (r);
			
		if (g < 0) {
			perror ("connect failed");
			close (r);
			return;
		}
		io_loop_add_fd (g, rw_tunnel_cb, LCAT_INT_TO_POINTER (r));
		io_loop_add_fd (r, rw_tunnel_cb, LCAT_INT_TO_POINTER (g));
	}
}

static void kb_command_cb (int fd, void* userdata)
{
	char s [1000];
	fgets (s, sizeof(s), stdin);
	if (atoi(s) == 0) {
		printf ("Speed is set to: unlimited\n");
	} else {
		speed = atoi(s)*1000;
		printf ("Speed is set to: %d\n", speed);
	}
}

void cleanup ()
{
	shutdown (_servfd, SHUT_RDWR);
	close (_servfd);
	exit (0);
}

static void print_help ()
{
	fprintf (stderr,
		"lcat is a tunnelling tool useful for transparently\n"
		"tunnelling TCP connections over a SOCKS proxy, like ssh -D\n"
		"\n"
		"Usage: lcat [-p port] [-t] [-d] [-h] [-s sleeptime]\n"
		"\n"
		"You can use lcat to throttle the amount of bandwidth you\n"
		"use.\n");
}

static void parsearg (int argc, char* argv[]) 
{
	char opt;
	while ((opt = getopt (argc, argv, "p:h:s:tg:")) != -1) {
		switch (opt) {
		case 'p': 
			localport = atoi(optarg);
			break;
		case 'h':
			print_help ();
			break;
		case 's':
			speed = atoi (optarg);
			break;
		case 'g':
			gateway = strdup (strtok (optarg, ":"));
			gatewayport = atoi (strtok (NULL, ":"));
			enable_iptables = 0;
			break;
		case 't':
			enable_iptables = 1;
			break;
		default:
			printf ("Bad option\n");
			print_help ();
			exit (1);
		}
	}
}

int main(int argc, char* argv[])
{
	parsearg (argc, argv);
	_servfd = server_socket (localport);

	signal (SIGINT, cleanup);

	io_loop_add_fd (_servfd, acceptconn, NULL);
	io_loop_add_fd (0, kb_command_cb, NULL);
	io_loop_start ();
	return 0;
}
