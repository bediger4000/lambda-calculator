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
/* $Id: small_hashtable.c,v 1.10 2011/11/12 17:55:35 bediger Exp $ */
/*
 * Simple, single-chaining hashtable, Doubly-linked list hash chains,
 * for ease of arbitrary insertion and deletion of hash-chain nodes.
 * Keeps a free-list of not-currently-used structs small_hashtable,
 * for speed in re-allocation.  These structs are used in a throwaway
 * fashion for finding free and bound variables. Because bound variables
 * can exist inside an abstraction (\x.x y (\x.F) z) it has the ability
 * to delete single elements, as well as cleaning out the entire table.
 * Not only does it keep unused structs small_hashtable on a free-list,
 * it has a free-list for unused structs small_hashnode, too.
 */

#include <stdio.h>
#include <stdlib.h>  /* malloc(), free() */
#include <assert.h>  /* assert macro */

#include <small_hashtable.h>

struct small_hashnode *new_small_hashnode(void);
unsigned long hash(const unsigned char *str);
struct small_hashnode *find_node(struct small_hashtable *h, const char *key);
void free_small_hashnode(struct small_hashnode *p);

/* number of buckets has to be a power of 2 for this to work */
/* XXX - need to guarantee power-of-2 number of buckets? */
#define MOD(x,y) ((x)&((y)-1))

struct small_hashnode *free_hashnode_list = NULL;
struct small_hashtable *free_hashtable_list = NULL;

static int hashtables_allocated = 0;
static int hashnodes_allocated = 0;

/* Value of bucketcount needs to constitute a power of 2 - does not
 * use '%' modulus operator, so an arbitrary bucket count won't work. */
struct small_hashtable *
init_small_hashtable(int bucketcount)
{
	unsigned int i;
	struct small_hashtable *h = NULL;

	if (free_hashtable_list)
	{
		/* Giving back a previously allocated struct small_hashtable.
		 * Note that this ignores value of bucketcount. */
		h = free_hashtable_list;
		free_hashtable_list = h->next_free;
	} else {
		/* Heap allocation of an entirely new struct small_hashtable. */
		h = malloc(sizeof(*h));
		h->count = bucketcount;
		h->size  = 0;
		h->buckets = malloc(sizeof(struct small_hashnode *) * h->count);

		++hashtables_allocated;

		for (i = 0; i < h->count; ++i)
		{
			struct small_hashnode *head, *tail;

			head = new_small_hashnode();
			tail = new_small_hashnode();

			head->next = tail;
			tail->prev = head;
			head->prev = tail;
			tail->next = NULL;

			head->value = tail->value = NULL;
			head->key   = tail->key   = NULL;

			h->buckets[i] = head;
		}
	}

	h->next_free = NULL;

	return h;
}

struct small_hashnode *
find_node(struct small_hashtable *h, const char *key)
{
	struct small_hashnode *chain;

	if (h->size)
	{
		unsigned int index;
		unsigned long hv = hash((const unsigned char *)key);

		index = MOD(hv, h->count);
		chain = h->buckets[index]->next;

		while (NULL != chain)
		{
			/* Here's why you have to use "Atoms" as keys: it does
			 * not actually do string comparison, only numerical comparison
			 * of the pointer value. */
			if (chain->key == key)
				break;
			chain = chain->next;
		}
	} else
		chain = NULL;
	
	return chain;
}

/* Give back the value (as a void *) associated with a certain key.
 * Returns NULL if it doesn't find the key at all.
 */
const void *
find_value(struct small_hashtable *h, const char *key)
{
	const void *r = NULL;
	struct small_hashnode *desired;

	desired = find_node(h, key);
	
	/* can set r to dummy tail value member (NULL) */
	if (desired)
		r = desired->value;

	return r;
}

const void *
insert_value(struct small_hashtable *h, const char *key, const void *value)
{
	unsigned long hv;
	unsigned long index;
	struct small_hashnode *head, *n;
	struct small_hashnode *chain = NULL;

	hv = hash((const unsigned char *)key);

	index = MOD(hv, h->count);
	head = h->buckets[index];
	chain = head->next;

	while (NULL != chain)
	{
		if (chain->key == key)
			break;
		chain = chain->next;
	}
	
	if (NULL != chain)
		return chain->key;

	n = new_small_hashnode();

	n->key = key;
	n->value = value;

	n->next = head->next;
	n->prev = head;

	head->next->prev = n;
	head->next = n;

	++h->size;

	return NULL;
}

/* The neccessities of removal causes me to make
 *  the hashchains into doubly-linked lists */
const void *
remove_key(struct small_hashtable *h, const char *key)
{
	const void *r = NULL;
	struct small_hashnode *n;

	if (NULL != (n = find_node(h, key)))
	{
		r = n->value;
		n->next->prev = n->prev;
		n->prev->next = n->next;
		free_small_hashnode(n);
		--h->size;
	}

	return r;
}

void
free_small_hashtable(struct small_hashtable *h)
{
	unsigned int i;

	for (i = 0; i < h->count; ++i)
	{
		struct small_hashnode *head = h->buckets[i];

		if (head->next != head->prev)
		{
			struct small_hashnode *first = head->next;
			struct small_hashnode *last  = head->prev->prev;

			last->next = free_hashnode_list;
			free_hashnode_list = first;

			head->next = head->prev;
			head->prev->prev = head;
		}
	}

	h->size = 0;

	h->next_free = free_hashtable_list;
	free_hashtable_list = h;
}

struct small_hashnode *
new_small_hashnode(void)
{
	struct small_hashnode *r = NULL;

	if (free_hashnode_list)
	{
		r = free_hashnode_list;
		free_hashnode_list = free_hashnode_list->next;
	} else {
		r = malloc(sizeof(*r));
		++hashnodes_allocated;
	}

	r->next = r->prev = NULL;
	r->key = NULL;
	r->value = NULL;

	return r;
}

void
free_small_hashnode(struct small_hashnode *p)
{
	p->prev = NULL;
	p->key = NULL;
	p->value = NULL;
	p->next = free_hashnode_list;
	free_hashnode_list = p;
}

void
free_all_small_hashtable(void)
{
	int hashtables_freed = 0;
	int hashnodes_freed  = 0;

	while (free_hashtable_list)
	{
		unsigned int i;
		struct small_hashtable *tmp = free_hashtable_list->next_free;

		for (i = 0; i < free_hashtable_list->count; ++i)
		{
			struct small_hashnode *head
				= free_hashtable_list->buckets[i];
			head->prev->next = free_hashnode_list;
			free_hashnode_list = head;
		}

		free_hashtable_list->next_free = NULL;
		free(free_hashtable_list->buckets);
		free_hashtable_list->buckets = NULL;
		free(free_hashtable_list);

		free_hashtable_list = tmp;
		++hashtables_freed;
	}

	while (free_hashnode_list)
	{
		struct small_hashnode *tmp = free_hashnode_list->next;
		free_hashnode_list->next = NULL;
		free(free_hashnode_list);
		free_hashnode_list = tmp;
		++hashnodes_freed;
	}

	if (hashtables_freed != hashtables_allocated)
		printf("Allocated %d structs small_hashtables, freed %d\n",
			hashtables_allocated, hashtables_freed);

	if (hashnodes_freed != hashnodes_allocated)
		printf("Allocated %d structs small_hashnode, freed %d\n",
			hashnodes_allocated, hashnodes_freed);
}

/* djb2 hash function */
unsigned long
hash(const unsigned char *str)
{
	unsigned long hv = 5381;
	unsigned int c;

	while ((c = *str++))
		hv = (hv * 33) ^ c;
	/*	hv = ((hv << 5) + hv) ^ c; */

	return hv;
}
