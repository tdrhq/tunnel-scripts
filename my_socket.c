/**
 * @file my_socket.c 
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
	if (server == NULL) {
		perror ("Server not found %s\n", gateway); 
		close (clientfd);
		return -1;
	}

	memset (&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	memcpy ((char*)&servaddr.sin_addr.s_addr, (char*)server->h_addr_list[0], server->h_length);

	servaddr.sin_port = htons (port);

	if (connect (clientfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
		perror ("Client connect");
		close (clientfd);
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

	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) == -1)
		perror ("Unable to set SO_REUSEADDR for bind");

	if (bind (fd, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
		perror ("bind");
		exit(1);
	}


	listen (fd, 1);

	return fd;
}
