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
/* $Id: evaluation.c,v 1.21 2011/11/18 13:53:14 bediger Exp $ */
/*
 * The function normal_order_reduction() that actually destructively
 * manipulates its input tree to perform normal order reduction. Also
 * some support functions that don't get called anywhere else.
 */

#include <stdio.h>  /* NULL definition */
#include <small_hashtable.h>
#include <buffer.h>
#include <lambda_expression.h>
#include <evaluation.h>
#include <hashtable.h>
#include <atom.h>


enum RedexType {BETA_REDEX, ETA_REDEX};

struct application_data {
	int found;
	enum RedexType typ;
	struct lambda_expression **parent;
	struct lambda_expression *application;
};

struct application_data find_redex(
	struct lambda_expression *expression,
	struct lambda_expression **expression_holder
);

/* these live in grammar.y */
extern int trace_eval;
extern int single_step;
extern int eta_reduction;


/* substitute() and real_substitute() exist so as to have the
 * ability to "single step" and "trace" substitutions.
 * substitute() prints out the current substitution action,
 * and possibly waits for the user to hit return.
 * Then, it calls real_substitute(), which silently does
 * the work of substitution.  real_substitute() recursively
 * calls itself and/or abstraction_substitution().
*/

struct lambda_expression *substitute(
	struct lambda_expression *term,
	const char *for_bound_variable,
	struct lambda_expression *in_expression
);

struct lambda_expression *
real_substitute(
	struct lambda_expression *term,
	const char               *for_variable,
	struct lambda_expression *in_expression
);

/* abstraction_substitution() exists to de-clutter the
 * "case ABSTRACTION:" branch of real_substitute()
 */

struct lambda_expression *
abstraction_substitution(
	struct lambda_expression *term,
	const char               *for_variable,
	struct lambda_expression *in_abstraction
);

void read_line(void);

struct lambda_expression *
substitute(
	struct lambda_expression *term,
	const char *bound_variable,
	struct lambda_expression *exp
)
{
	struct lambda_expression *r = NULL;
	if (trace_eval)
	{
		struct buffer *a = new_buffer(128);
		struct buffer *b = new_buffer(128);
		buffer_expression(term, a);
		buffer_expression(exp, b);
		printf("Substitute (%s) for %s in (%s)\n", a->buffer, bound_variable, b->buffer);
		delete_buffer(a);
		delete_buffer(b);
		a = b = NULL;
	}
	if (single_step) read_line();

	r = real_substitute(term, bound_variable, exp);

	if (trace_eval)
	{
		struct buffer *a = new_buffer(128);
		buffer_expression(r, a);
		printf("Substitution: %s\n", a->buffer);
		delete_buffer(a);
		a = NULL;
	}
	if (single_step) read_line();

	return r;
}

/* Capture avoiding substitution.
 * Make a new term by substituting a copy of "argument"
 * for every ocurrance of the bound variable of "abstraction"
 * in the body of "abstraction".
 */
struct lambda_expression *
real_substitute(
	struct lambda_expression *term,
	const char *variable,
	struct lambda_expression *exp
)
{
	struct lambda_expression *r = NULL;
	switch (exp->typ)
	{
	case VARIABLE:
		if (exp->variable == variable)
			r = copy_expression(term);
		else
			r = new_variable(exp->variable);
		break;
	case APPLICATION:
		r = new_application(
			real_substitute(term, variable, exp->rator),
			real_substitute(term, variable, exp->rand)
		);
		break;
	case ABSTRACTION:
		r = abstraction_substitution(term, variable, exp);
		break;
	}
	return r;
}

/* The "case ABSTRACTION:" branch from real_substitute() */
struct lambda_expression *
abstraction_substitution(
	struct lambda_expression *term,
	const char *bound_variable,
	struct lambda_expression *abstr
)
{
	struct lambda_expression *r = NULL;

	if (abstr->bound_variable == bound_variable)
		/* bound variable of abstraction "abstr" shadows bound_variable */
		r = copy_expression(abstr);
	else {
		struct small_hashtable *term_free_vars = init_small_hashtable(16);
		struct small_hashtable *bnd_vrs = init_small_hashtable(16);
		find_free_vars(term, bnd_vrs, term_free_vars);
		if (NULL == find_value(term_free_vars, abstr->bound_variable))
		{
			r = new_abstraction(
				abstr->bound_variable,
				real_substitute(
					term,
					bound_variable,
					abstr->body
				)
			);
		} else {
			struct lambda_expression *new_body = NULL, *new_bound_var;
			struct lambda_expression *new_abst = NULL;
			const char *new_bound_var_name = NULL;
			find_free_vars(abstr->body, bnd_vrs, term_free_vars);
			new_bound_var_name = find_nonfree_var(term_free_vars);
			new_bound_var = new_variable(new_bound_var_name);
			new_body = real_substitute(
				new_bound_var,
				abstr->bound_variable,
				abstr->body
			);
			new_abst = new_abstraction(new_bound_var_name, new_body);

			r = real_substitute(term, bound_variable, new_abst);
			free_expression(new_bound_var);
			free_expression(new_abst);
		}
		free_small_hashtable(bnd_vrs);
		free_small_hashtable(term_free_vars);
	}
	return r;
}

struct lambda_expression *
normal_order_reduction(struct lambda_expression *e)
{
	int found_reduction = 0;

	do {
		struct application_data ad;
		struct lambda_expression *parent = NULL;

		ad.found = 0;
		ad.parent = NULL;
		ad.application = NULL;

		ad = find_redex(e, &parent);

		if (ad.found)
		{
			if (BETA_REDEX == ad.typ)
			{
				/* substitute the rand for the body of the abstraction */
				struct lambda_expression *r = substitute(
					ad.application->rand,
					ad.application->rator->bound_variable,
					ad.application->rator->body
				);

				/* free the old application */
				free_expression(ad.application);

				/* put the substituted-for abstraction body in for the old application */
				if (ad.parent == &parent)
					e = r;
				else
					*(ad.parent) = r;
			}
			if (ETA_REDEX == ad.typ)
			{
				if (*ad.parent) free_expression(*ad.parent);
				if (ad.parent == &parent)
				{
					free_expression(e);
					e = ad.application;
				} else
					*(ad.parent) = ad.application;
			}

			found_reduction = 1;
		} else
			found_reduction = 0;

	} while (found_reduction);

	return e;
}

void read_line(void)
{
	char buf[128];
	fgets(buf, sizeof(buf), stdin);
}

/* Depth-first traversal of a binary tree of structs lambda_expression,
 * which represents the current state of the term undergoing reductions.
 * Find a Beta or Eta reduction, and fill in a struct application_data
 * appropriately.
 */
struct application_data
find_redex(
	struct lambda_expression *e,
	struct lambda_expression **holder
)
{
	struct application_data r;
	r.found = 0;
	r.parent = NULL;
	switch (e->typ)
	{
	case VARIABLE:
		r.found = 0;
		r.application = NULL;
		r.parent = NULL;
		break;

	case ABSTRACTION:
		if (eta_reduction)
		{
			if (APPLICATION == e->body->typ)
			{
				if (VARIABLE == e->body->rand->typ && e->body->rand->variable == e->bound_variable)
				{
					struct small_hashtable *my_free_vars = init_small_hashtable(16);
					struct small_hashtable *my_bound_vars = init_small_hashtable(16);
					find_free_vars(e->body->rator, my_bound_vars, my_free_vars);
					if (NULL == find_node(my_free_vars, e->bound_variable))
					{
						r.found = 1;
						r.typ = ETA_REDEX;
						r.application = e->body->rator;
						e->body->rator = NULL;
						r.parent = holder;
					}
					free_small_hashtable(my_free_vars);
					free_small_hashtable(my_bound_vars);
				}
			}
		}
		/* Should a complimentary beta_reduction variable and choice exist? */
		if (!r.found) 
			r = find_redex(e->body, &e->body);
		break;

	case APPLICATION:
		if (ABSTRACTION == e->rator->typ)
		{
			r.found = 1;
			r.typ = BETA_REDEX;
			r.application = e;
			r.parent = holder;
		} else {
			r = find_redex(e->rator, &e->rator);
			if (!r.found)
				r = find_redex(e->rand, &e->rand);
		}
		break;
	}
	return r;
}
