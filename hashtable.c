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
/* $Id: hashtable.c,v 1.13 2011/11/12 17:55:35 bediger Exp $ */
/*
 * Implementation of the dynamic hashtable described in
 * "The Design and Implementation of Dynamic Hashing for Sets and
 * Tables in Icon" by William G. Griswold and Gregg M. Townsend,
 * Software - Pracice and Experience, vol 23(4), 351-367, April 1993
 *
 * Griswold and Townsend say their hashtable is a practical implementation
 * of Per-Ake Larson's dynamically resizeable hashtable, from Communications
 * of the ACM, 1988.
 *
 * A big, heavy-duty hashtable that only grows. Doesn't even have a function
 * for deleting keys from the hashtable.  Used in "lc" to keep "Atoms",
 * const char * values for strings, and to keep abbreviations, lambda calculus
 * abstract syntax trees calculated earlier and kept by key for re-use.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <hashtable.h>

#define SEGLISTSIZE   12
#define INIT_SEG_SIZE 8
#define MAX_AVE_CHAIN 5

/* MOD(x,y) == x % y, if y is a power of 2 */
/* number of buckets (y) has to be a power of 2 for this to work */
#define MOD(x,y)        ((x) & ((y)-1))

/* Hash chains: doubly-linked lists of structs hashnode. Each chain has
 * all the hashnodes whose hashvalue element modulo table->current_size
 * has the same numerical value.  Doubly-linked lists make it easier
 * to consistently insert a hashnode in a list.  Each hash chain also has
 * a dummy head (head == head->prev) and a dummy tail (tail == tail->next).
 * Dummy head has a hashvalue of 0, dummy tail has a hasvalue of 
 * (unsigned int) -1, which should be biggest int value. Hash chains kept
 * in ascending numerical order. */
struct hashnode {
	char            *key;
	int              key_string_length;
	unsigned int     hashvalue;
	void            *data;
	struct hashnode *prev;
	struct hashnode *next;
};

unsigned int hash_djb2(const char *str);

struct hashnode *node_lookup(
	struct hashtable *h,
	const char *string_to_lookup,
   	unsigned int *rhv,   /* hash value of string_to_lookup */
   	int *rseg,           /* index into segment list of hash chain */
   	int *rmseg           /* number of hashchains in indexed segment list */
);

struct hashnode *new_hashnode(unsigned int hashval, const char *key, void *data);
int  find_segment_index(int bucket_no, int *seg_size);
void rehash_hashtable(struct hashtable *);
int  resize_hashtable(struct hashtable *);
void init_new_segment(struct hashtable *h, int segment_no);
void insert_node_in_chain(struct hashnode *chain, struct hashnode *node);
void dummy_data_free(void *data_to_free);
void insert_node(struct hashtable *h, struct hashnode *hn,
	unsigned int hv, int seg, int mseg);

struct segment_indexes {int mseg; int nseg; };

/* This array has to have nseg values starting at INIT_SEG_SIZE,
 * doubling each entry, and entry count of at least SEGLISTSIZE */
static struct segment_indexes segment_idx_finder[] = {
	{  0,   8},
	{  8,  16},
	{ 16,  32},
	{ 32,  64},
	{ 64, 128},
	{128, 256},
	{256, 512},
	{512, 1024},
	{1024, 2048},
	{2048, 4096},
	{4096, 8192},
	{8192, 16384},
	{16384, 32768}
};

struct hashtable *
new_hashtable(free_fcn fn)
{
	struct hashtable *r = NULL;

	r = malloc(sizeof(*r));
	r->L = malloc(SEGLISTSIZE * sizeof(struct hashnode *));
	r->current_size = INIT_SEG_SIZE;
	r->max_ave_load = MAX_AVE_CHAIN;
	r->data_free_fcn = fn? fn: dummy_data_free;
	r->node_count = 0;
	r->L[0] = malloc(r->current_size * sizeof(struct hashnode *));
	r->slot_count = 1;

	init_new_segment(r, 0);

	return r;
}

/* Add dummy head and tail nodes (doubly-linked) to a segment,
 * indexed into h->L (the segment list) by segment_no.
 */
void
init_new_segment(struct hashtable *h, int segment_no)
{
	struct hashnode *heads, *tails;  /* dummy head and tail nodes */
	int i;

	/* Allocate dummy head and tail nodes in blocks.  This makes it
	 * a bit trickier in free_hashtable(), but it keeps calls to
	 * malloc() down for larger segments. */
	heads = malloc(h->current_size * sizeof(struct hashnode));
	tails = malloc(h->current_size * sizeof(struct hashnode));

	for (i = 0; i < h->current_size; ++i)
	{
		h->L[segment_no][i] = &heads[i];
		h->L[segment_no][i]->key = NULL;
		h->L[segment_no][i]->hashvalue = 0;
		h->L[segment_no][i]->prev = h->L[segment_no][i];  /* head->prev == head */

		h->L[segment_no][i]->next = &tails[i];   /* head->next == tail */

		h->L[segment_no][i]->next->key = NULL;
		h->L[segment_no][i]->next->hashvalue = (unsigned int)-1;
		h->L[segment_no][i]->next->prev = h->L[segment_no][i];        /*  tail->prev == head */
		h->L[segment_no][i]->next->next = h->L[segment_no][i]->next;  /*  tail->next == prev */
	}
}

/* Free the entire hashtable, including its contents, the keys and values.
 * The values get freed using a function passed to new_hashtable(). */
void
free_hashtable(struct hashtable *h)
{
	int i;
	int segment_size = h->current_size / 2;
	int factor = 2;

	if (1 == h->slot_count)
		segment_size *= 2;

	for (i = h->slot_count - 1; i >= 0; --i)
	{
		struct hashnode **segment_list = h->L[i];
		struct hashnode *tails = NULL, *heads = segment_list[0];
		int j;

		for (j = 0; j < segment_size; ++j)
		{
			struct hashnode *chain = segment_list[j]->next;  /* skip dummy head */
			/* free the real elements in chain */
			while (chain != chain->next)
			{
				struct hashnode *tmp = chain->next;
				free(chain->key);
				chain->key = NULL;
				if (chain->data)
				{
					(h->data_free_fcn)(chain->data);
					chain->data = NULL;
				}
				chain->next = chain->prev = NULL;
				free(chain);
				chain = tmp;
			}
			if (!j) tails = chain;  /* dummy tails */
			chain = NULL;
			segment_list[j]->next = segment_list[j]->prev = NULL;
			segment_list[j] = NULL;
		}
		free(heads);
		free(tails);
		free(segment_list);
		segment_list = NULL;
		h->L[i] = NULL;
		segment_size /= factor;
		if (1 == i) segment_size = INIT_SEG_SIZE;
	}
	free(h->L);
	h->L = NULL;
	free(h);
	h = NULL;
}

/* Put a key/value pair into the hashtable. */
void *
insert_data(struct hashtable *h, const char *key, void *data)
{
	struct hashnode *hn = NULL;
	void *r = NULL;
	unsigned int hv = 0;
	int seg, mseg;

	hn = node_lookup(h, key, &hv, &seg, &mseg);

	/* key already exists in hashtable. */
	r = hn->data;

	hn->data = data;

	/* If non-null, key's data got replaced. */
	return r;
}

void
insert_node(struct hashtable *h, struct hashnode *hn,
	unsigned int hv, int seg, int mseg)
{
	int bucket_no = MOD(hv, h->current_size);
	struct hashnode *chain = h->L[seg][bucket_no - mseg];

	insert_node_in_chain(chain, hn);

	++h->node_count;

	if (h->node_count/h->current_size >= h->max_ave_load)
	{
		if (resize_hashtable(h))
			rehash_hashtable(h);
	}
}

/* Return the index into segment list (struct hashtable's L member) of
 * a given bucket number. Also return the segment size, the number of
 * hash chains in all the segments before that index. */
int
find_segment_index(int bucket_no, int *seg_size)
{
	int seg = 0, i;

	/* Start at the largest segment: half the buckets are in largest segment.
	 * Also, counting down means you never run off the upper end of the
	 * segment list.
	 */
	for (i = SEGLISTSIZE - 1; i >= 0; --i)
	{
		if (segment_idx_finder[i].mseg <= bucket_no && bucket_no < segment_idx_finder[i].nseg)
		{
			seg = i;
			/* seg_size - the number of hash chains in all the segments
			 * before segment index i */
			*seg_size = segment_idx_finder[i].mseg;
			if (!seg) *seg_size = 0;
			break;
		}
	}

	return seg;
}

/* Given a key (string_to_lookup), find the data stored against that key.
 * Fill passed in pointers with hashvalue of string_to_lookup,
 * index into segment of the hash chain in which the key would reside,
 * size of the segment (in hash chains) of the segment in which the key
 * would reside. */
struct hashnode *
node_lookup(
	struct hashtable *h,
	const char *string_to_lookup,
	unsigned int *rhv,
	int *rseg,
	int *rmseg
)
{
	struct hashnode *chain, *r = NULL;
	unsigned int hashval = hash_djb2(string_to_lookup);
	int bucket_no = MOD(hashval, h->current_size);

	/* Send back hashvalue, index into segment list, size of the segment,
	 * so the calling function can re-use these values instead of re-
	 * calculating them.  Sometimes they don't get used, but setting a
 	 * pointer is cheap. */

	*rhv = hashval;

	*rseg = find_segment_index(bucket_no, rmseg);

	/* Since MOD() clamps the hashvalue to a maximum, don't need to check
	 * here to insure that seg, mseg make sense. */

	chain = h->L[*rseg][bucket_no - *rmseg];

	/* Each hash chain has a dummy tail, which we will always find.
	 * Hopefully, numerical comparisons take less time than string
	 * comparisons.
	 */
	while (chain != chain->next)
	{
		if (hashval == chain->hashvalue)
		{
			if (!strcmp(chain->key, string_to_lookup))
			{
				r = chain;
				break;
			}
		} else if (hashval < chain->hashvalue)
			break;  /* r stays NULL - didn't find key */

		chain = chain->next;
	}

	/* When we get here, either:
	 * (a) found the key in chain.
	 * (b) didn't find key in chain but hit a larger hashvalue.
	 * (c) got to end of chain (chain == chain->next) w/o finding key.
	 */

	return r;
}

/* Find data stored against a key. */
void *
lookup_key(struct hashtable *h, const char *key)
{
	void *r = NULL;
	unsigned int rhv;  /* purely a dummy in this function */
	int seg, mseg;
	struct hashnode *hn = node_lookup(h, key, &rhv, &seg, &mseg);

	if (hn)
		r = hn->data;

	return r;
}

/* Find string stored as a key. Store it with NULL data if
 * it doesn't already appear in the table.  Used to "intern"
 * C-strings as Atoms.
 */
const char *
string_lookup(struct hashtable *h, const char *key, int *length)
{
	const char *r = NULL;
	unsigned int rhv;  /* purely a dummy in this function */
	int seg, mseg;
	struct hashnode *hn = node_lookup(h, key, &rhv, &seg, &mseg);

	if (!hn)
	{
		hn = new_hashnode(rhv, key, NULL);
		insert_node(h, hn, rhv, seg, mseg);
	}

	r = hn->key;
	*length = hn->key_string_length;

	return r;
}

unsigned int
hash_djb2(const char *str)
{
	unsigned long hash = 5381;
	int c;

	while ((c = *str++))
		hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

	return hash;
}

/* Since no function to delete a key from the hashtable exists, no
 * need to accomodate free-lists of structs hashnode */
struct hashnode *
new_hashnode(unsigned int hashval, const char *key, void *data)
{
	struct hashnode *r = malloc(sizeof(struct hashnode));
	size_t len = strlen(key);
	r->key = strcpy(malloc(len + 1), key);
	r->key_string_length = len;
	r->hashvalue = hashval;
	r->data = data;
	r->prev = r->next = NULL;
	return r;
}

/* Add another segment to the segment list, initialize the segment.
 * Returns 1 if it added another segment, 0 if it didn't.
 */
int
resize_hashtable(struct hashtable *h)
{
	int resized = 0;

	if (h->slot_count < SEGLISTSIZE)
	{
		struct hashnode **new_segment
			= malloc(h->current_size * sizeof(struct hashnode *));
		h->L[h->slot_count] = new_segment;
		init_new_segment(h, h->slot_count);
		++h->slot_count;
		h->current_size *= 2;
		resized = 1;
	}

	return resized;
}

/* Rework all the hashchains to take advantage of having expanded
 * the hashtable with a new segment of hash chains. */
void
rehash_hashtable(struct hashtable *h)
{
	int slot_no;
	int segment_size = INIT_SEG_SIZE;
	int current_bucket = 0, factor = 1;
	int max_slot = h->slot_count - 1;  /* only have to rehash "old" segments */

	for (slot_no = 0; slot_no < max_slot; ++slot_no)
	{
		struct hashnode **segment = h->L[slot_no];
		int j;

		for (j = 0; j < segment_size; ++j)
		{
			struct hashnode *chain = segment[j]->next;

			while (chain != chain->next)
			{
				int bucket_no = MOD(chain->hashvalue, h->current_size);
				if (bucket_no != current_bucket)
				{
					struct hashnode *tmp = chain->prev;
					int seg, mseg = 0;

					/* Remove node named "chain" from the doubly-linked list. */
					chain->prev->next = chain->next;
					chain->next->prev = chain->prev;
					chain->prev = chain->next = NULL;

					seg = find_segment_index(bucket_no, &mseg);
					insert_node_in_chain(h->L[seg][bucket_no - mseg], chain);

					chain = tmp;  /* node named chain got moved to new bucket */
				}

				chain = chain->next;
			}

			++current_bucket;
		}

		segment_size *= factor;  /* 1 first time through, 2 thereafter */
		factor = 2;
	}
}

/* Centralize inserting a node in a hash chain. */
void
insert_node_in_chain(struct hashnode *chain, struct hashnode *node)
{
	unsigned int hv = node->hashvalue;

	while (chain != chain->next && chain->hashvalue < hv)
		chain = chain->next;

	/* chain contains the struct hashnode that will reside *after*
	 * node when node gets inserted. Dummy tail stays at tail. */

	node->prev = chain->prev;
	node->next = chain;
	chain->prev->next = node;
	chain->prev = node;
}

void
dummy_data_free(void *data_to_free)
{
	/* XXX - could count number of data frees that take place */
	data_to_free = NULL;
	return;
}
