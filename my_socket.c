#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netdb.h>
#include <getopt.h>
#include <string.h>

int client2server_socket (const char* gateway, int port)
{
	/* create client socket */
	int  clientfd = socket(AF_INET, SOCK_STREAM, 0);
	struct hostent *server;
	struct sockaddr_in servaddr;

	if (clientfd < 0) {
		perror ("Client Socket");
		return clientfd;
	}


	server = gethostbyname (gateway);
	if (server == NULL) 
		exit (1);

	memset (&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	memcpy ((char*)&servaddr.sin_addr.s_addr, (char*)server->h_addr_list[0], server->h_length);

	servaddr.sin_port = htons (port);

	if (connect (clientfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
		perror ("Client connect");
		return -1;
	}

	return clientfd;
}

int server_socket (int localport)
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
