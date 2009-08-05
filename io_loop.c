/**
 * @file io_loop.c 
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

#include <assert.h>
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
static int nfds = 0;

static fd_set build_all () 
{
	fd_set d;
	int i;

	FD_ZERO (&d);
	for (i = 0; i <= nfds; i++) {
		if (allfds [i]) 
			FD_SET(i, &d);
	}
	return d;
}

void io_loop_add_fd (int fd, io_callback cb, void* _userdata) 
{
	assert (!allfds [fd]);
	allfds[fd] = cb;
	userdata[fd] = _userdata;

	if (fd > nfds) nfds = fd;

#ifdef IO_LOOP_DEBUG
	printf ("added %d %d\n", fd, nfds);
#endif
}

void io_loop_remove_fd (int fd)
{
	assert (allfds [fd]);
	allfds [fd] = NULL;
	userdata[fd] = NULL;

	if (fd == nfds) {
		int i;
		for (i = nfds - 1; i > 0 && allfds[i] == NULL; i--);
		nfds = i;
		if (nfds < 0) nfds = 0;
	}

#ifdef IO_LOOP_DEBUG
	printf ("removed %d %d\n", fd, nfds);
#endif
}

void io_loop_start ()
{
	for (;;) {
		fd_set rd = build_all ();
		fd_set wr;
		fd_set er;
		int i;

		FD_ZERO (&wr);
		FD_ZERO (&er);

		select (nfds + 1, &rd, &wr, &er, NULL);

		for (i = 0; i <= nfds; i++) {
			if (allfds [i] && FD_ISSET (i, &rd)) {
				(allfds [i]) (i, userdata[i]);
			}
		}
	}
}
