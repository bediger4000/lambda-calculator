/*
    Copyright (C) 2006-2011, Bruce Ediger

    This file is part of lc.

    lc is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    lc is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with lc; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*/
/* $Id: buffer.c,v 1.10 2011/11/18 14:21:42 bediger Exp $ */
/*
 * Self-resizing text buffer. Doesn't have a pre-set limit on
 * how long the ASCII-Nul terminated string it contains.
 *
 * Does not do any error checking on malloc() or realloc() returns:
 * it's almost impossible to test that code, malloc() just plain doesn't
 * in 2011, and if malloc() fails, the code will cause a crash on the
 * next line.  No good way to recover from a malloc problem exists.
 */

#include <stdlib.h>   /* malloc(), free(), realloc() */
#include <string.h>   /* memcpy() */
#include <stdio.h>    /* fprintf() */

#include <buffer.h>

/* Keep buffer element size a multiple of 8.
 * valgrind's memory check complains about something in glibc
 * reading past the end of the buffer by a few bytes.  It's
 * total paranoia. I don't think that the string operations
 * in glibc actually modify the read-off-end-of-buffer, but
 * why not make certain?
 */
#define roundup(x, y)   ((((x)+((y)-1))/(y))*(y))

struct buffer *
new_buffer(int desired_size)
{
	struct buffer *r = malloc(sizeof *r);
	desired_size= roundup(desired_size, 8);
	r->buffer = malloc(desired_size);
	r->size = desired_size;
	r->offset = 0;
	return r;
}

void
delete_buffer(struct buffer *b)
{
	free(b->buffer);
	b->buffer = NULL;
	b->offset = b->size = 0;
	free(b);
	b = NULL;
}

void
resize_buffer(struct buffer *b, int increment)
{
	increment = roundup(increment, 8);
	b->size += increment;
	b->buffer = realloc(b->buffer, b->size);
}

void
buffer_append(struct buffer *b, const char *bytes, int length)
{
	if (length >= (b->size - b->offset - 1))
		resize_buffer(b, length);

	memcpy(&b->buffer[b->offset], bytes, length);
	b->offset += length;
	b->buffer[b->offset] = '\0';  /* XXX - not too general, now is it? */
}
