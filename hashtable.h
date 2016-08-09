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
/* $Id: hashtable.h,v 1.6 2011/11/12 04:50:28 bediger Exp $ */

typedef void (*free_fcn)(void *);

struct hashtable {
	free_fcn         data_free_fcn;
	int max_ave_load;
	int current_size;   /* number of valid hash buckets */
	int node_count;     /* nodes (key,value pairs) in hashtable */
	int slot_count;     /* count of filled-in slots in segment list */
	struct hashnode ***L; /* segment list */
};

struct hashtable *new_hashtable(free_fcn);
void free_hashtable(struct hashtable *);
void *insert_data(struct hashtable *, const char *key, void *data);
void *lookup_key(struct hashtable *, const char *key);
const char *string_lookup(struct hashtable *, const char *key, int *length);
void count_hashtable(struct hashtable *h);
