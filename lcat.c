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

int localport = 8002;
char *gateway = "eniac.seas.upenn.edu";
int gatewayport = 22;
int speed = 80000;
int sleeptime = 20;
int _servfd;

int client2gw ()
{
	/* create client socket */
	int  clientfd = socket(AF_INET, SOCK_STREAM, 0);
	struct hostent *server;
	struct sockaddr_in servaddr;

	if (clientfd < 0)
		perror ("Client Socket");

	server = gethostbyname (gateway);
	if (server == NULL) 
		exit (1);

	memset (&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	memcpy ((char*)&servaddr.sin_addr.s_addr, (char*)server->h_addr_list[0], server->h_length);

	servaddr.sin_port = htons (gatewayport);

	if (connect (clientfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
		perror ("Client connect");
		exit (0);
	}

	return clientfd;
}

int server ()
{
	int fd = socket (AF_INET, SOCK_STREAM, 0), val = 1;
	struct sockaddr_in addr;
	
	if (fd < 0) {
		perror ("server1");
		exit (1);
	}

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(localport);

	if (bind (fd, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
		perror ("bind");
		exit(1);
	}

	/* the following line is copy pasted, recheck and verify */
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

	listen (fd, 1);

	return fd;
}

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

void acceptconn (int servfd, void* userdata)
{
	int r = accept (servfd, NULL, NULL);
	if (r < 0) {
		perror ("Accept failed");
		return;
	} else {
		int g = client2gw ();
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
	while ((opt = getopt (argc, argv, "p:h:s:")) != -1) {
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
	servfd = server();

	_servfd = servfd;

	signal (SIGINT, cleanup);

	io_loop_add_fd (servfd, acceptconn, NULL);
	io_loop_add_fd (0, kb_command_cb, NULL);
 
	io_loop_start ();

}
