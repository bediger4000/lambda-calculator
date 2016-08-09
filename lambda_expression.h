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
/* $Id: lambda_expression.h,v 1.18 2011/11/18 13:53:14 bediger Exp $ */

/* Lambda Calculus term representation - "abstract syntax" */

/*
 * Abstract syntax of a parsed lambda calculus expression.
 * Created by the yacc/bison grammar during parsing, and also
 * used internally as a destructible graph during normal order
 * evaluation.
 */

enum lambda_expression_type { VARIABLE, APPLICATION, ABSTRACTION };

struct lambda_expression {
	enum lambda_expression_type typ;

	/* typ == VARIABLE */
	const char *variable;

	/* typ == ABSTRACTION */
	const char *bound_variable;
	struct lambda_expression *body;

	/* typ == APPLICATION */
	struct lambda_expression *rator;
	struct lambda_expression *rand;

	int parameterized;

	/* housekeeping */
	struct lambda_expression *next_free;
};

struct lambda_expression *new_variable(const char *identifier);
struct lambda_expression *new_application(
	struct lambda_expression *rator,
	struct lambda_expression *rand
);
struct lambda_expression *new_abstraction(
	const char *bound_variable,
	struct lambda_expression *body
);

struct lambda_expression *copy_expression(struct lambda_expression *le);

void free_expression(struct lambda_expression *expression);

struct lambda_expression *deparameterize(struct lambda_expression *graph, int count);

void buffer_expression(struct lambda_expression *expression, struct buffer *buf);
void print_expression(struct lambda_expression *exp);
int equivalent_graphs(struct lambda_expression *node1, struct lambda_expression *node2);
int alpha_equivalent_graphs(struct lambda_expression *node1, struct lambda_expression *node2);

void free_all(void);

void free_vars(struct lambda_expression *term);
void bound_vars(struct lambda_expression *term);
void find_free_vars(
	struct lambda_expression *term,
	struct small_hashtable *current_bound_vars,
	struct small_hashtable *dict
);

struct lambda_expression *goedelize(struct lambda_expression *e);
const char *find_nonfree_var(struct small_hashtable *free_vars);
