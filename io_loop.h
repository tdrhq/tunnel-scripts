/**
 * @file io_loop.h 
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


typedef void (*io_callback) (int fd, void* userdata);
typedef void (*io_timeout) ();

void io_loop_add_fd (int fd, io_callback cb, void* _userdata);

void io_loop_remove_fd (int fd);

void io_loop_set_timeout (int seconds, io_timeout cb);

void io_loop_start () __attribute__ ((noreturn));
