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

static struct async_fd {
	io_callback cb [3];
	void* userdata [3];
} allfds [1<<16];

static int nfds = 0;
static io_timeout timeout_cb;
static int timeout_seconds = 0;

void io_loop_set_timeout (int seconds, io_timeout cb)
{
	timeout_seconds = seconds;
	timeout_cb = cb;
}

static fd_set build_all (int _num)
{
	fd_set d;
	int i;

	FD_ZERO (&d);
	for (i = 0; i <= nfds; i++) {
		if (allfds [i].cb[_num]) 
			FD_SET(i, &d);
	}
	return d;
}

static fd_set build_all_read () 
{
	return build_all (0);
}

static fd_set build_all_write ()
{
	return build_all (1);
}

static fd_set build_all_er ()
{
	return build_all (2);
}

static void reduce_nfds ()
{
	int i;
	for (i = nfds; i > 0 && allfds[i].cb [0] == NULL && allfds[i].cb [1] == NULL && allfds[i].cb[2] == NULL; i--);
	nfds = i;
	if (nfds < 0) nfds = 0;
}

static void io_loop_add_fd (int _num, int fd, io_callback cb, void* _userdata) 
{
	assert (!allfds[fd].cb [_num]);
	allfds[fd].cb [_num] = cb;
	allfds[fd].userdata [_num]= _userdata;

	if (fd > nfds) nfds = fd;

#ifdef IO_LOOP_DEBUG
	printf ("added %d %d %d\n", _num, fd, nfds);
#endif
}

void io_loop_add_fd_read (int fd, io_callback cb, void* data)
{
	return io_loop_add_fd (0, fd, cb, data);
}
void io_loop_add_fd_write (int fd, io_callback cb, void* data)
{
	return io_loop_add_fd (1, fd, cb, data);
}
void io_loop_add_fd_er (int fd, io_callback cb, void* data)
{
	return io_loop_add_fd (2, fd, cb, data);
}

/* removes all write and read callbacks associated with this. */
void io_loop_remove_fd_n (int fd, int _num)
{
	allfds [fd].cb [_num] = NULL;
	allfds [fd].userdata [_num]= NULL;

	if (fd == nfds) {
		reduce_nfds ();
	}

#ifdef IO_LOOP_DEBUG
	printf ("removed %d %d\n", fd, nfds);
#endif
}

void io_loop_remove_fd (int fd)
{
	int i;
	for (i = 0; i < 3; i++) io_loop_remove_fd_n (fd, i);
}

void io_loop_start ()
{
	for (;;) {
		fd_set d [3];
		int i, n;
		int num_ready;
		struct timeval* timeout = NULL;

		d [0]  = build_all_read ();
		d [1]  = build_all_write ();
		d [2]  = build_all_er ();

		if (timeout_seconds) {
			timeout = (struct timeval*) malloc (sizeof (struct timeval));
			timeout->tv_sec = timeout_seconds;
			timeout->tv_usec = 0;
		}
		
		num_ready = select (nfds + 1, &d[0], &d[1], &d[2], timeout);

		/* what order makes more sense? to call a read first, or a write? */

		for (n = 0; n < 3; n++) 
			for (i = 0; i <= nfds; i++) {
				if (allfds [i].cb [n] && FD_ISSET (i, &d[n])) {
					(allfds [i].cb [n]) (i, allfds[i].userdata[n]);
				}
			}
		
		if (timeout && !num_ready) {
			timeout_cb ();
		}

		if (timeout) free (timeout);
	}
}
