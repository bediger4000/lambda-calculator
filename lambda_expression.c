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
/* $Id: lambda_expression.c,v 1.28 2011/11/18 13:53:14 bediger Exp $ */
/*
 * Functions dealing with struct lambda_expression: allocation, free-list
 * management, freeing.  Housekeeping.
 * Also functions dealing with lambda calculus abstract syntax trees,
 * which get built up by the yacc parser.
 */

#include <stdio.h>    /* printf() */
#include <stdlib.h>   /* malloc(), free() */
#include <string.h>   /* strlen() */

#include <small_hashtable.h>
#include <buffer.h>
#include <lambda_expression.h>
#include <hashtable.h>
#include <atom.h>

struct lambda_expression *new_node(void);
void find_bound_vars(
	struct lambda_expression *term,
	struct small_hashtable *bindings
);

int real_alpha_equivalent_graphs(
	struct lambda_expression *node1,
	struct small_hashtable *currently_bound_vars1,
	struct lambda_expression *node2,
	struct small_hashtable *currently_bound_vars2,
	int abstraction_count
);

const char *determine_binding(const char *bound_var, int abstraction_count);

/* Communicate with sigint_handler() */
extern int interpreter_interrupted;

/* new_node() and free_expression() use file-scope variable
 * free_list to keep a plain ol' stack of structs lambda_expression,
 * so as to avoid calling malloc/free a lot.
 */
static int free_cnt = 0;
static int alloc_cnt = 0;
static int malloc_cnt = 0;
static struct lambda_expression *free_list = NULL;

/* Default ASCII-character used for lambda, and the string used
 * between binding site and body of an abstraction. */
char lambda_character = '%';
const char *abstraction_delimiter = ".";

struct lambda_expression *
new_node(void)
{
	struct lambda_expression *r = NULL;

	++alloc_cnt;

	if (free_list)
	{
		r = free_list;
		free_list = free_list->next_free;
	} else {
		++malloc_cnt;
		r = malloc(sizeof(*r));
		r->variable = NULL;
		r->bound_variable = NULL;
		r->body = NULL;
		r->rator = NULL;
		r->rand = NULL;
	}

	r->next_free = NULL;
	r->parameterized = 0;

	return r;
}

struct lambda_expression *
new_variable(const char *identifier)
{
	struct lambda_expression *r = new_node();
	r->typ = VARIABLE;
	r->variable = identifier;
	return r;
}

void
free_expression(struct lambda_expression *expression)
{
	++free_cnt;
	if (expression)
	{
		switch (expression->typ)
		{
		case VARIABLE:
			expression->variable = NULL;
			break;
		case APPLICATION:
			if (expression->rator) free_expression(expression->rator);
			expression->rator = NULL;
			free_expression(expression->rand);
			expression->rand = NULL;
			break;
		case ABSTRACTION:
			expression->bound_variable = NULL;
			free_expression(expression->body);
			expression->body = NULL;
			break;
		}
		expression->next_free = free_list;
		free_list = expression;
	} else
		fprintf(stderr, "Freeing a NULL expression node\n");
}

void
buffer_expression(struct lambda_expression *expression, struct buffer *b)
{
	if (expression)
	{
		if (expression->parameterized)
			buffer_append(b, "*(", 2);

		switch (expression->typ)
		{
		case VARIABLE:
			buffer_append(b, expression->variable, strlen(expression->variable));
			break;
		case APPLICATION:
			if (ABSTRACTION == expression->rator->typ) buffer_append(b, "(", 1);
			buffer_expression(expression->rator, b);
			if (ABSTRACTION == expression->rator->typ) buffer_append(b, ")", 1);
			buffer_append(b, " ", 1);
			if (VARIABLE != expression->rand->typ) buffer_append(b, "(", 1);
			buffer_expression(expression->rand, b);
			if (VARIABLE != expression->rand->typ) buffer_append(b, ")", 1);
			break;
		case ABSTRACTION:
			buffer_append(b, &lambda_character, 1);
			buffer_append(b, expression->bound_variable, strlen(expression->bound_variable));
			buffer_append(b, abstraction_delimiter, strlen(abstraction_delimiter));
			buffer_expression(expression->body, b);
			break;
		}

		if (expression->parameterized)
			buffer_append(b, ")", 1);
	} else
		buffer_append(b, "NULL", 4);
}

struct lambda_expression *
new_application(
	struct lambda_expression *rator,
	struct lambda_expression *operand
)
{
	struct lambda_expression *r = new_node();
	r->typ = APPLICATION;
	r->rator = rator;
	r->rand = operand;
	return r;
}

struct lambda_expression *
new_abstraction(
	const char *bound_variable,
	struct lambda_expression *body
)
{
	struct lambda_expression *r = new_node();
	r->typ = ABSTRACTION;
	r->bound_variable = bound_variable;
	r->body = body;
	return r;
}

struct lambda_expression *
copy_expression(struct lambda_expression *e)
{
	struct lambda_expression *new_expression = NULL;
	switch (e->typ)
	{
	case VARIABLE:
		new_expression = new_variable(e->variable);
		break;
	case APPLICATION:
		new_expression = new_application(
			copy_expression(e->rator),
			copy_expression(e->rand)
		);
		break;
	case ABSTRACTION:
		new_expression = new_abstraction(
			e->bound_variable,
			copy_expression(e->body)
		);
		break;
	}
	new_expression->parameterized = e->parameterized;
	return new_expression;
}

void
free_all(void)
{
	int freed_cnt = 0;
	while (free_list)
	{
		struct lambda_expression *tmp = free_list->next_free;
		free_list->variable = NULL;
		free_list->bound_variable = NULL;
		free_list->body = NULL;
		free_list->rator = NULL;
		free_list->rand = NULL;
		free_list->next_free = NULL;
		free(free_list);
		++freed_cnt;
		free_list = tmp;
	}

	if (!interpreter_interrupted && freed_cnt != malloc_cnt)
		printf("malloced %d structs lambda_expression, freed %d\n",
			malloc_cnt, freed_cnt);
}

void
find_free_vars(
	struct lambda_expression *term,
	struct small_hashtable *current_bound_vars,
	struct small_hashtable *dict
)
{
	const void *p = NULL;
	int   previously_bound = 0;

	switch (term->typ)
	{
	case VARIABLE:
		if (NULL == find_node(current_bound_vars, term->variable))
			(void)insert_value(dict, term->variable, term->variable);
		break;
	case APPLICATION:
		find_free_vars(term->rator, current_bound_vars, dict);
		find_free_vars(term->rand, current_bound_vars, dict);
		break;
	case ABSTRACTION:
		p = insert_value(current_bound_vars, term->bound_variable, term->bound_variable);
		previously_bound = (p != NULL);
		find_free_vars(
			term->body,
			current_bound_vars,
			dict
		);
		if (!previously_bound)
			remove_key(current_bound_vars, term->bound_variable);
		
		break;
	}
}

void
find_bound_vars(
	struct lambda_expression *term,
	struct small_hashtable *bindings
)
{
	if (NULL == term) return;

	switch (term->typ)
	{
	case VARIABLE:
		break;
	case APPLICATION:
		find_bound_vars(term->rator, bindings);
		find_bound_vars(term->rand, bindings);
		break;
	case ABSTRACTION:
		(void)insert_value(bindings, term->bound_variable, term->bound_variable);
		find_bound_vars(
			term->body,
			bindings
		);
		break;
	}
}

void
free_vars(struct lambda_expression *term)
{
	struct small_hashtable *free_var_dict = init_small_hashtable(32);
	struct small_hashtable *bindings = init_small_hashtable(32);
	int i;

	find_free_vars(term, bindings, free_var_dict);

	for (i = 0; i < free_var_dict->count; ++i)
	{
		struct small_hashnode *chain = free_var_dict->buckets[i]->next;
		while (NULL != chain)
		{
			if (chain->key)
				printf("\"%s\"\n", chain->key);
			chain = chain->next;
		}
	}
	
	free_small_hashtable(free_var_dict);
	free_small_hashtable(bindings);  /* should have nothing in it here */
}

void
bound_vars(struct lambda_expression *term)
{
	struct small_hashtable *bindings = init_small_hashtable(32);
	int i;

	find_bound_vars(term, bindings);

	for (i = 0; i < bindings->count; ++i)
	{
		struct small_hashnode *chain = bindings->buckets[i]->next;
		while (NULL != chain)
		{
			if (chain->key)
				printf("\"%s\"\n", chain->key);
			chain = chain->next;
		}
	}
	
	free_small_hashtable(bindings);
}

void
print_expression(struct lambda_expression *exp)
{
	struct buffer *b = new_buffer(256);
	buffer_expression(exp, b);
	printf("%s\n", b->buffer);
	delete_buffer(b);
}

int
equivalent_graphs(struct lambda_expression *node1, struct lambda_expression *node2)
{
	int r = 0;

	if (node1->typ == node2->typ)
	{
		switch (node1->typ)
		{
		case APPLICATION:
			r = equivalent_graphs(node1->rator, node2->rator)
				&& equivalent_graphs(node1->rand, node2->rand);
			break;
		case VARIABLE:
			if (node1->variable == node2->variable)
				r = 1;
			break;
		case ABSTRACTION:
			if (node1->bound_variable == node2->bound_variable)
				r = equivalent_graphs(node1->body, node2->body);
			break;
		}

	}

	return r;
}

/* Return 1 if node1 and node2 alpha-equate.  Return 0 otherwise.
 * Globally-visible function.  Sets up for and calls
 * real_alpha_equivalent_graphs(), then cleans up afterward.
 */
int
alpha_equivalent_graphs(struct lambda_expression *node1, struct lambda_expression *node2)
{
	int r = 0;
	struct small_hashtable *map1 = init_small_hashtable(128);
	struct small_hashtable *map2 = init_small_hashtable(128);
	int abstraction_count = 0;
	r = real_alpha_equivalent_graphs(node1, map1, node2, map2, abstraction_count);
	free_small_hashtable(map1);
	free_small_hashtable(map2);
	return r;
}

/* Return 1 if node1 and node2 alpha-equate.  Return 0 otherwise. */
int
real_alpha_equivalent_graphs(
	struct lambda_expression *node1,
	struct small_hashtable *map1,
	struct lambda_expression *node2,
	struct small_hashtable *map2,
	int abstraction_count
)
{
	int r = 0;

	/* Don't even bother checking unless nodes possess the same type. */

	if (node1->typ == node2->typ)
	{
		switch (node1->typ)
		{
		case APPLICATION:
			r = real_alpha_equivalent_graphs(node1->rator, map1, node2->rator, map2, abstraction_count)
				&& real_alpha_equivalent_graphs(node1->rand, map1, node2->rand, map2, abstraction_count);
			break;

		case VARIABLE: {
			const char *x1 = (const char *)find_value(map1, node1->variable);
			const char *x2 = (const char *)find_value(map2, node2->variable);
			if (x1 && x2)
			{
				/* both variables currently bound */
				if (x1 == x2)
					r = 1;
			} else if (!x1 && !x2) {
				/* both variables not  bound */
				if (node1->variable == node2->variable)
					r = 1;
			}
			/* else one variable is currently bound, the other isn't,
			 * the expressions don't alpha-equate. */
			}
			break;

		case ABSTRACTION: {
			/* add bound variables to maps, check bodies for alpha-equivalence,
			 * then dispose of bound variable maps. Don't forget the case of
			 * re-binding an already bound variable, something like:
			 * \x y z. z (\x.y) z */
			const char *mock_bound_var = determine_binding(node1->bound_variable, ++abstraction_count);
			const char *old_mock_bound_var1 = find_value(map1, node1->bound_variable);
			const char *old_mock_bound_var2 = find_value(map2, node2->bound_variable);

			if (old_mock_bound_var1)
			{
				/* An exterior binding exists.  Rebind to the new "mock" bound variable */
				remove_key(map1, node1->bound_variable);
				remove_key(map2, node2->bound_variable);
			}

			insert_value(map1, node1->bound_variable, mock_bound_var);
			insert_value(map2, node2->bound_variable, mock_bound_var);

			r = real_alpha_equivalent_graphs(
				node1->body, map1,
    			node2->body, map2,
				abstraction_count
			);

			remove_key(map1, node1->bound_variable);
			remove_key(map2, node2->bound_variable);

			if (old_mock_bound_var1)
			{
				/* Replace the now-unbound variables with their old bindings */
				insert_value(map1, node1->bound_variable, old_mock_bound_var1);
				insert_value(map2, node2->bound_variable, old_mock_bound_var2);
			}
			
			}
			break;
		}

	}

	return r;
}

const char *
determine_binding(const char *bound_var, int abstraction_count)
{
	const char *nm = NULL;
	size_t len = 1+ strlen(bound_var) + 1 + 11 + 1;
	char *buf = malloc(len);
	snprintf(buf, len, ".%s_%d", bound_var, abstraction_count);
	/* XXX - What if sprintf returns value > len? */
	nm = Atom_string(buf);
	free(buf);
	buf = NULL;
	return nm;
}

struct lambda_expression *
deparameterize(struct lambda_expression *node, int count)
{
	struct lambda_expression *r = NULL;
	switch (node->typ)
	{
	case VARIABLE:
		/* Flow-of-control can get here for parameterized expressions
		 * like (x *y).  The right-most variable (free or bound) gets
		 * duplicated. */
		r = node;
		if (r->parameterized)
		{
			struct lambda_expression *original_node = node;
			r->parameterized = 0;
			while (--count)
				r = new_application(r, copy_expression(original_node));
		}
		break;
	case APPLICATION:
		r = node;
		if (r->parameterized)
		{
			int cnt = count;
			struct lambda_expression *original_application = node;
			r->parameterized = 0;
			while (--cnt)
				r = new_application(r, copy_expression(original_application));
			node = r;
		}
		if (node->rator->parameterized)
		{
			/* construct rator (rator (rator (... (rator rand)...) */
			int cnt = count;
			struct lambda_expression *tree = deparameterize(node->rand, count);
			node->rator->parameterized = 0;
			while (--cnt)
			{
				struct lambda_expression *n = copy_expression(node->rator);
				tree = new_application(n, tree);
			}
			node->rand = tree;
			r = node;
		} else {
			r = node;
			r->rand = deparameterize(node->rand, count);
			r->rator = deparameterize(node->rator, count);
		}
		break;
	case ABSTRACTION:
		r = node;
		r->body = deparameterize(node->body, count);
		if (r->parameterized)
		{
			struct lambda_expression *original_node = r;
			r->parameterized = 0;
			while (--count)
				r = new_application(r, copy_expression(original_node));
		}
		break;
	}
	r->parameterized = 0;
	return r;
}

/* From Torben Mogensen's "Efficient Self Interpretation in Lambda Calculus"
 */
struct lambda_expression *
goedelize(struct lambda_expression *e)
{
	struct lambda_expression *r = NULL;
	struct small_hashtable *term_free_vars = init_small_hashtable(16);
	struct small_hashtable *bnd_vrs = init_small_hashtable(16);
	const char *a, *b, *c;

	find_free_vars(e, bnd_vrs, term_free_vars);

	switch (e->typ)
	{
	case VARIABLE:
		a = find_nonfree_var(term_free_vars);
		insert_value(term_free_vars, a, a);
		b = find_nonfree_var(term_free_vars);
		insert_value(term_free_vars, b, b);
		c = find_nonfree_var(term_free_vars);
		r = new_abstraction(Atom_string(a),
			new_abstraction(Atom_string(b),
				new_abstraction(Atom_string(c),
					new_application(
						new_variable(Atom_string(a)),
						new_variable(Atom_string(e->variable))
					)
				)
			)
		);
		break;
	case APPLICATION:
		a = find_nonfree_var(term_free_vars);
		insert_value(term_free_vars, a, a);
		b = find_nonfree_var(term_free_vars);
		insert_value(term_free_vars, b, b);
		c = find_nonfree_var(term_free_vars);
		r = new_abstraction(Atom_string(a),
			new_abstraction(Atom_string(b),
				new_abstraction(Atom_string(c),
					new_application(
						new_application(
							new_variable(Atom_string(b)),
							goedelize(e->rator)
						),
						goedelize(e->rand)
					)
				)
			)
		);
		break;
	case ABSTRACTION:
		insert_value(term_free_vars, e->bound_variable, e->bound_variable);
		a = find_nonfree_var(term_free_vars);
		insert_value(term_free_vars, a, a);
		b = find_nonfree_var(term_free_vars);
		insert_value(term_free_vars, b, b);
		c = find_nonfree_var(term_free_vars);
		r = new_abstraction(Atom_string(a),
			new_abstraction(Atom_string(b),
				new_abstraction(Atom_string(c),
					new_application(
						new_variable(Atom_string(c)),
						new_abstraction(
							e->bound_variable,
							goedelize(e->body)
						)
					)
				)
			)
		);
		break;
	}

	free_small_hashtable(term_free_vars);
	free_small_hashtable(bnd_vrs);

	return r;
}

/* Based on the keys in a struct small_hashtable, find a variable
 * name that doesn't appear as a key.
 */
const char *
find_nonfree_var(
	struct small_hashtable *free_vrs
)
{
	const char *r = NULL;
	int c, lower = 'a', upper = 'z';

	
	try_again:
	for (c = lower; c <= upper; ++c)
	{
		const char *candidate;
		char buffer[2];
		snprintf(buffer, sizeof(buffer), "%c", c);
		candidate = Atom_string(buffer);
		if (NULL == find_value(free_vrs, candidate))
		{
			r = candidate;
			break;
		}
	}

	if (NULL == r)
	{
		lower = 'A';
		upper = 'Z';
		goto try_again;
	}

	/* try 2, 3, 4... length strings of characters */

	return r;
}
