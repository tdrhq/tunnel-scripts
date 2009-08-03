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

#include "io_loop.h"

static io_callback allfds [1<<16];
static void* userdata[1<<16];


static fd_set build_all (int *nfds) 
{
	fd_set d;
	int i;

	FD_ZERO (&d);
	for (i = 0; i < (1<<16); i++) {
		if (allfds [i]) 
			FD_SET(i, &d);
		*nfds = (*nfds < i ? i : *nfds);
	}
	return d;
}

void io_loop_add_fd (int fd, io_callback cb, void* _userdata) 
{
	assert (!allfds [fd]);
	allfds[fd] = cb;
	userdata[fd] = _userdata;
}

void io_loop_remove_fd (int fd)
{
	assert (allfds [fd]);
	allfds [fd] = NULL;
	userdata[fd] = NULL;
}

void io_loop_start ()
{
	for (;;) {
		int nfds = 0;
		fd_set rd = build_all (&nfds);
		fd_set wr;
		fd_set er;
		int i;

		FD_ZERO (&wr);
		FD_ZERO (&er);

		select (nfds + 1, &rd, &wr, &er, NULL);

		for (i = 0; i < (1<<16); i++) {
			if (allfds [i] && FD_ISSET (i, &rd)) {
				(allfds [i]) (i, userdata[i]);
			}
		}
	}
}
