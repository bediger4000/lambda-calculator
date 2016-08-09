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
/* $Id: small_hashtable.h,v 1.7 2011/11/12 17:55:36 bediger Exp $ */

/* should the interface expose these two structures? */
struct small_hashnode {
	const char *key;
	const void *value;
	struct small_hashnode *next;
	struct small_hashnode *prev;
};

struct small_hashtable {
	int count;  /* count of buckets */
	int size;   /* number of key/value pairs in entire table */
	struct small_hashnode **buckets;
	struct small_hashtable *next_free;
};

struct small_hashtable *init_small_hashtable(int bucketcount);
const void *find_value(struct small_hashtable   *h, const char *key);
struct small_hashnode *find_node(struct small_hashtable *h, const char *key);
const void *remove_key(struct small_hashtable   *h, const char *key);
const void *insert_value(struct small_hashtable *h, const char *key, const void *value);
void free_small_hashtable(struct small_hashtable *h);
void print_small_hashtable(struct small_hashtable *h, int print_vars_only);
void free_all_small_hashtable(void);

