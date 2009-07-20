#include <stdio.h>
#include <stdlib.h>
#define _BSD_SOURCE
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

int localport = 8002;
char *gateway = "eniac.seas.upenn.edu";
int gatewayport = 22;
int speed = 80000;
int sleeptime = 100;
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

int local2gw [1<<16];
int gw2local [1<<16];


fd_set build_all (int *nfds) 
{
	fd_set d;

	FD_ZERO (&d);
	for (int i = 0; i < (1<<16); i++) {
		if (local2gw [i] || gw2local[i]) 
			FD_SET(i, &d);
		*nfds = (*nfds < i ? i : *nfds);
	}
	return d;
}


void acceptconn (int servfd)
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
		
		local2gw [r] = g;
		gw2local [g] = r;
		
		fprintf (stderr, "Conn accepted\n");
	}
}

void end_conn (int fd)
{
	int otherend = local2gw[fd]? local2gw[fd] : gw2local[fd];
	shutdown (fd, SHUT_RDWR);
	shutdown (otherend, SHUT_RDWR);
	close (fd);
	close (otherend);
	local2gw[fd] = gw2local[fd] = 0;
	local2gw[otherend] = gw2local[otherend] = 0;
	fprintf (stderr, "A connection was closed\n");
}

void cleanup ()
{
	for (int i = 0; i < (1<<16); i++) {
		if (local2gw[i]) end_conn (i);
	}

	shutdown (_servfd, SHUT_RDWR);
	close (_servfd);
	exit (0);
}

int main()
{
	int servfd = server ();
	int bufsize = speed/sleeptime;
	char *buf = (char*) malloc (bufsize);
	
	_servfd = servfd;
	memset (local2gw, 0, sizeof (local2gw));
	memset (gw2local, 0, sizeof (gw2local));

	signal (SIGINT, cleanup);

	for (;;) {
		struct timeval t;
		t.tv_sec = sleeptime/1000;
		t.tv_usec = (sleeptime % 1000)*1000;
		select (1, NULL, NULL, NULL, &t); /* basically usleep */

		int nfds = servfd;
		fd_set rd = build_all (&nfds);
		fd_set wr;
		fd_set er;
		FD_SET (servfd, &rd);
		FD_SET (0, &rd);

		FD_ZERO (&wr);
		FD_ZERO (&er);

		int r = select (nfds + 1, &rd, &wr, &er, NULL);

		if (FD_ISSET (servfd, &rd)) {
			acceptconn (servfd);
		}

		if (FD_ISSET (0, &rd)) {
			char s [1000];
			fgets (s, sizeof(s), stdin);
			if (atoi(s) == 0) {
				printf ("can't do that\n");
			} else {
				speed = atoi(s)*1000;
				printf ("speed is set to: %d\n", speed);
				bufsize = speed/sleeptime;
				free(buf);
				buf = (char*) malloc (bufsize);
			}
		}

		for (int i = 0; i < (1<<16); i++) {
			if ((local2gw[i] || gw2local[i]) && FD_ISSET (i, &rd)) {
				int ws = (local2gw[i] ? local2gw[i] : gw2local[i]);
				int len = read (i, buf, bufsize);
				if (len < 1) {
					end_conn (ws);
					continue;
				}

				len = write (ws, buf, len);
				if (len < 1) {
					end_conn (ws);
					continue;
				}

			}
		}
	}
}
