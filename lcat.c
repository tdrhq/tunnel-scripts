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

#include <fcntl.h>
#include <stdint.h>
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

#define LCAT_POINTER_TO_INT(p) ((int) (intptr_t) (p))
#define LCAT_INT_TO_POINTER(i) ((void*) (intptr_t) (i))

int localport = 8002;
char *gateway = NULL;
int gatewayport = 22;
int speed = 0;
int sleeptime = 20;
int _servfd;
int enable_iptables = 1; /* otherwise we're just running in a single gateway mode */

static char *buffer = NULL;
int bufsize = 0;

void end_conn (int fd)
{
#ifdef IO_LOOP_DEBUG
	printf ("closing fd: %d\n", fd);
#endif
	shutdown (fd, SHUT_RDWR);
	close (fd);
	io_loop_remove_fd (fd);
}

static void speed_statistics (int bytes)
{
	static time_t last_clock;
	static int bytes_since_clock = 0;
	struct timeval t;

	if (!last_clock) last_clock = time (NULL);
	
	bytes_since_clock += bytes;
	if (bytes_since_clock > 10*1000*1000) {
		time_t cur = time(NULL);
		
		fprintf (stderr, "Last %d MB in %d seconds at %d kbps\n", bytes_since_clock/1000/1000, 
			 (int) (cur-last_clock),
			 (int) ((bytes_since_clock/1024)/(cur-last_clock)));
		last_clock = cur;
		bytes_since_clock = 0;
	}
}

static void pause_if_req (int bytes)
{
	static int bytes_since_clock = 0;
	struct timeval t;

	if (speed == 0) return;
	bytes_since_clock += bytes;
	if (bytes_since_clock > 10000) {
		int sleeptime = bytes_since_clock*1000/speed;
		t.tv_sec = sleeptime/1000;
		t.tv_usec = (sleeptime % 1000)*1000;
		select (1, NULL, NULL, NULL, &t); /* basically usleep */
		bytes_since_clock = 0;
	}
}

static void rw_tunnel_cb (int i, void* fd_to) 
{
	int ws = LCAT_POINTER_TO_INT (fd_to);
	int len;
	int len2;

	len = read (i, buffer, bufsize);
	if (len < 1) {
#ifdef IO_LOOP_DEBUG
		printf ("it's a clean death for %d %d\n", i ,ws);
#endif
		end_conn (ws);
		end_conn (i);
		return;
	}
	
	len2 = write (ws, buffer, len);
	if (len2 < len) {
#ifdef IO_LOOP_DEBUG
		printf ("it's a slightly unclean death for %d %d\n", i ,ws);
#endif
		end_conn (ws);
		end_conn (i);
		return;
	}

	speed_statistics (len);
	pause_if_req (len);
}

static void got_connection_er (int fd, void* data)
{
	end_conn (fd);
	end_conn (LCAT_POINTER_TO_INT (data));
}

/* connect to the desting that fd was originally bound to */

static void got_connected (int fd, void *data)
{
	int source = LCAT_POINTER_TO_INT (data);

	int so_error, so_error_len = sizeof (int);

	getsockopt (fd, SOL_SOCKET, SO_ERROR, &so_error, &so_error_len);

	if (so_error != 0) {
#ifdef IO_LOOP_DEBUG
		printf ("A connect failed (%d)\n", fd);
#endif
		end_conn (fd);
		end_conn (source);
		return;
	}

	int flags = fcntl (fd, F_GETFL, 0);


	assert (flags != -1);
	assert (flags & O_NONBLOCK);
	fcntl (fd, F_SETFL, flags & (~O_NONBLOCK));
	
	io_loop_remove_fd (fd);
	io_loop_add_fd_read (fd, rw_tunnel_cb, LCAT_INT_TO_POINTER (source));
	io_loop_add_fd_er (fd, got_connection_er, LCAT_INT_TO_POINTER (source));
	io_loop_add_fd_read (source, rw_tunnel_cb, LCAT_INT_TO_POINTER (fd));
	io_loop_add_fd_er (source, got_connection_er, LCAT_INT_TO_POINTER (fd));
}

static int
connect_to_dest (int fd)
{
	struct sockaddr_in client;
	int len = sizeof (client);

	getsockopt (fd, SOL_IP, SO_ORIGINAL_DST, (struct sockaddr*) &client, &len);
	int ret = socket (AF_INET, SOCK_STREAM, 0);

#ifdef IO_LOOP_DEBUG
	printf ("the new SOCKS fd: %d\n", ret);
#endif
	int flags = fcntl (ret, F_GETFL, 0);
	assert (flags != -1);
	fcntl (ret, F_SETFL, flags | O_NONBLOCK);
	
	io_loop_add_fd_write (ret, got_connected, LCAT_INT_TO_POINTER (fd));
	io_loop_add_fd_er (ret, got_connection_er, LCAT_INT_TO_POINTER (fd));

	if (connect (ret, (struct sockaddr*) &client, sizeof (client)) == 0) {
		end_conn (ret);
		end_conn (fd);
		return ret;
	} 
	
	if (errno != EINPROGRESS) {
		perror ("early case of connect failing");
		end_conn (ret);
		end_conn (fd);
		return -1;
	}
	return ret;
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
		else { 
			g = connect_to_dest (r);
			return;
		}

		if (g < 0) {
			perror ("connect failed");
			close (r);
			return;
		}
		io_loop_add_fd_read (g, rw_tunnel_cb, LCAT_INT_TO_POINTER (r));
		io_loop_add_fd_read (r, rw_tunnel_cb, LCAT_INT_TO_POINTER (g));
	}
}

static void kb_command_cb (int fd, void* userdata)
{
	char s [1000];
	fgets (s, sizeof(s), stdin);
	if (atoi(s) == 0) {
		speed = 0;
		printf ("Speed is set to: unlimited\n");
	} else {
		speed = atoi(s)*1000;
		printf ("Speed is set to: %d\n", speed);
	}
}

void cleanup ()
{
	fprintf (stderr, "SIGINT caught, cleanup...\n");
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

static void
timeout_cb ()
{
	/* make an arbitrary connection to keep the proxy valid! */
	int fd = client2server_socket ("www.google.com", 80);
	fprintf (stderr, "timeout ping\n");
	close (fd);
}

int main(int argc, char* argv[])
{
	parsearg (argc, argv);
	_servfd = server_socket (localport);

	bufsize = 500;
	buffer = (char*) malloc (bufsize);

	signal (SIGINT, cleanup);

	io_loop_add_fd_read (_servfd, acceptconn, NULL);
	io_loop_add_fd_read (0, kb_command_cb, NULL);
	io_loop_set_timeout (60, timeout_cb);
	io_loop_start ();
	return 0;
}
