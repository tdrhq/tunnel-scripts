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

int localport = 8002;
char *gateway = "eniac.seas.upenn.edu";
int gatewayport = 22;
int speed = 80000;
int sleeptime = 20;
int _servfd;
int enable_iptables = 0;

void end_conn (int fd)
{
	shutdown (fd, SHUT_RDWR);
	close (fd);
	io_loop_remove_fd (fd);
	fprintf (stderr, "A connection was closed\n");
}

static void pause_if_req (int bytes)
{
	static time_t last_clock;
	static int bytes_since_clock = 0;
	int _sleeptime = bytes*1000/speed;
	struct timeval t;

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
	int ws = (int) (fd_to);
	
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
	if (len2 < len) {
		fprintf (stderr, "Oh holy shit, I hoped this couldn't happen\n");
	}

	pause_if_req (len);
}

char* parse (const char* buf, const char* query) {
	char *temp = strdup (buf);
	char *token = strtok (temp, " \t");

	if (strcmp (token, "tcp") != 0) {
		fprintf (stderr, "not tcp\n");
		free (temp);
		return NULL;
	}

	while (token = strtok (NULL, " \t")) {
		char* equal = strchr (token, '=');
		if (!equal) continue;
		*equal = ' ';
	
		char key[100] = "", val[100] = "";
		int num = sscanf (token, "%s %s", key, val);
		if (num < 2) continue;

		if (strcmp (key, query) == 0) {
			char* ret = strdup (val);
			free (temp);
			return ret;
		}
	}

	return NULL;
}

/* connect to the desting that fd was originally bound to */
static int connect_to_dest (int fd)
{
	struct sockaddr_in client;
	char *dst = NULL, *dport = NULL;
	int len = sizeof (client);
	FILE* f;
	char buf [1000];

	assert (0 == getpeername (fd,  (struct sockaddr*) &client, &len));
	fprintf (stderr, "Connection from port no. %d\n", ntohs (client.sin_port));
	
	int port = ntohs (client.sin_port);
	
	/* now figure out which was the original destination */
	f = fopen ("/proc/net/ip_conntrack", "r");
	while (fgets (buf, sizeof(buf), f)) {
		char* src = parse (buf, "sport");
		if (!src) continue;

		if (atoi(src) == port) {
			/* first dst is all we care for */
			dst = parse (buf, "dst");
			dport = parse (buf, "dport");
			
			fprintf (stderr, "Got connection to %s:%s\n", dst, dport);
			free (src);
			break;
		}

		free (src);
	}

	fclose (f);
	int ret = -1;
	if (dst && dport) ret = client2server_socket (dst, atoi(dport));
	
	free (dst);
	free (dport);

	return ret;
}

void acceptconn (int servfd, void* userdata)
{
	int r = accept (servfd, NULL, NULL);
	if (r < 0) {
		perror ("Accept failed");
		return;
	} else {
		int g;
		if (!enable_iptables)
			g = client2server_socket (gateway, gatewayport);
		else {
			g = connect_to_dest (r);
		}
			
		if (g < 0) {
			perror ("connect failed");
			return;
		}
		io_loop_add_fd (g, rw_tunnel_cb, (void*) r);
		io_loop_add_fd (r, rw_tunnel_cb, (void*) g);
		
		fprintf (stderr, "Conn accepted\n");
	}
}

static void kb_command_cb (int fd, void* userdata)
{
	char s [1000];
	fgets (s, sizeof(s), stdin);
	if (atoi(s) == 0) {
		printf ("can't do that\n");
	} else {
		speed = atoi(s)*1000;
		printf ("speed is set to: %d\n", speed);
	}
}

void cleanup ()
{
	shutdown (_servfd, SHUT_RDWR);
	close (_servfd);
	exit (0);
}

static void parsearg (int argc, char* argv[]) 
{
	char opt;
	while ((opt = getopt (argc, argv, "p:h:s:t")) != -1) {
		switch (opt) {
		case 'p': 
			localport = atoi(optarg);
			break;
		case 'h':
			gateway = strdup (optarg);
			break;
		case 's':
			sleeptime = atoi (optarg);
			break;
		case 't':
			enable_iptables = 1;
			break;
		default:
			printf ("bad option");
			exit (1);
		}
	}
}


int main(int argc, char* argv[])
{
	int servfd;
	char *buf;
	int bytes = 0;

	int bytes_since_clock = 0;
	time_t last_clock;
	last_clock = time (NULL);

	parsearg (argc, argv);
	servfd = server_socket (localport);

	_servfd = servfd;

	signal (SIGINT, cleanup);

	io_loop_add_fd (servfd, acceptconn, NULL);
	io_loop_add_fd (0, kb_command_cb, NULL);
 
	io_loop_start ();

}
