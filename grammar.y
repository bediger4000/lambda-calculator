%{
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

/* $Id: grammar.y,v 1.30 2011/11/18 05:00:09 bediger Exp $ */

/*
 * Lambda calculus interpreter's grammar.
 * Does two things, somewhat interwoven in the grammar:
 * (1) Bottom-up parse of lambda calculus text input, creating
 * an abstract syntax "parse tree" of the expression.
 * (2) Deal with interpreter commands, things like turning evaluation
 * timer on and off, that a user might want to do during an interactive
 * session.
 *
 * Implements a read-eval-print loop along with some user-convenience
 * functions.  The looping isn't at all obvious, it gets done by the
 * program -> stmnt
 * program -> program stmnt
 * productions.
 */

#include <stdio.h>
#include <stdlib.h>    /* atoi() */
#include <unistd.h>    /* getopt() */
#include <errno.h>     /* errno manifest constant */
#include <string.h>    /* strerror() */
#include <sys/time.h>  /* gettimeofday() */
#include <signal.h>    /* signal(), etc */
#include <setjmp.h>    /* setjmp(), longjmp(), jmp_buf */


#include <parser.h>    /* shared type between lex.l, grammar.y */
#include <buffer.h>
#include <small_hashtable.h>
#include <lambda_expression.h>
#include <hashtable.h>
#include <atom.h>
#include <evaluation.h>
#include <abbreviations.h>

void usage(char *progname);
void top_level_cleanup(void);

void start_clock(void);
void stop_clock(void);
float elapsed_time(struct timeval before, struct timeval after);
void free_lambda_expression(void *data);

enum expressionEvaluationResults {NORMAL_FORM, INTERRUPT, TIMEOUT, REDUCTION_LIMIT};
struct lambda_expression *reduce_expression(struct lambda_expression *e, enum expressionEvaluationResults *eer);

struct lambda_expression *abstraction_from_list(struct lambda_expression * list, struct lambda_expression *body);

extern void push_and_open(const char *filename);

int prompting = 1;
int looking_for_filename = 0;
int found_binary_command = 0;

int perform_timing = 0;
int reduction_timeout = 0;   /* how long to let a graph reduction run, seconds */
int eta_reduction = 1;
int trace_eval = 0;
int single_step = 0;

static struct timeval before, after;

/* from lex.l */
extern void set_yyin_stdin(void);
extern void set_yyin(const char *filename);
extern void reset_yyin(void);

/* keep compilers from complaining */
extern int yylex(void);
int yyerror(const char *s1);
extern int yyparse(void);
extern char *optarg;

struct filename_node {
	const char *filename;
	struct filename_node *next;
};

struct lambda_expression *previous_result = NULL;

#ifdef YYBISON
#define YYERROR_VERBOSE
#endif

/* Signal handling.
 */
void sigint_handler(int signo);
sigjmp_buf in_reduce_expression;
int interpreter_interrupted = 0;  /* communicate with free_all() */
%}

%union{
	const char *string_constant;
	const char *identifier;
	struct lambda_expression *term;
	enum ModifiableCommands cmd;
	int number;
}


%token TK_LPAREN TK_RPAREN 
%token TK_LBRACE TK_RBRACE 
%token <identifier> TK_IDENTIFIER TK_RESULT
%token <string_constant> FILE_NAME
%token TK_LAMBDA TK_DOT TK_STAR
%token TK_EOL
%token TK_DEF TK_NORMALIZE TK_FREE TK_BOUND TK_LOAD
%token TK_TIMER TK_TRACE TK_STEP TK_ETA
%token TK_GOEDELIZE TK_LEXICALLY_EQUIVALENT TK_ALPHA_EQUIVALENT
%token <term> TK_PRINT TK_LAST_RESULT
%token <string_constant> BINARY_MODIFIER
%token <number> NUMBER

%type <term> program error stmnt expression list item
%type <term> abstraction
%type <term> interpreter_command
%type <cmd> modifiable_command

%%

/* "Loop" part of read-eval-print loop. */
program
	: stmnt { top_level_cleanup(); }
	| program stmnt  { top_level_cleanup(); }
	| error  /* magic token - yacc unwinds to here on most syntax errors */
	;

stmnt
	: expression TK_EOL {
			/* Eval and print parts of read-eval-print loop. */
			struct lambda_expression *p = NULL;
			enum expressionEvaluationResults eer = NORMAL_FORM;
			start_clock();
			p = reduce_expression($1, &eer);
			stop_clock();
			if (INTERRUPT != eer)
			{
				print_expression(p);
				if (previous_result)
					free_expression(previous_result);
				previous_result = p;
			} else
				free_expression(p); /* previous_result remains the same */
			if (perform_timing)
				printf("Elapsed: %.3f seconds\n", elapsed_time(before, after));
		}
	| TK_DEF TK_IDENTIFIER expression TK_EOL
		{
			struct lambda_expression *prev = abbreviation_add($2, $3);
			if (prev) free_expression(prev);
		}
	| TK_DEF TK_IDENTIFIER TK_LBRACE TK_STAR TK_RBRACE expression TK_EOL
		{
			struct lambda_expression *prev = abbreviation_add($2, $6);
			if (prev) free_expression(prev);
		}
	| TK_EOL  { $$ = NULL; } /* allow empty line(s) following non-empty-line stmnt */
	| interpreter_command
	;

interpreter_command
	: modifiable_command BINARY_MODIFIER TK_EOL {
			int command = (($2 == Atom_string("on"))? 1: 0);
			found_binary_command = 0;

			switch ($1)
			{
			case CMD_TIMER: perform_timing = command; break;
			case CMD_TRACE: trace_eval     = command; break;
			case CMD_STEP:  single_step    = command; break;
			case CMD_ETA:   eta_reduction  = command; break;
			}
		}
	| modifiable_command TK_EOL {
			const char *phrase = "boojum snark";
			const char *state = "unset";
			found_binary_command = 0;
			switch ($1)
			{
			case CMD_TIMER: 
				phrase = "Evaluation timing";
				state = perform_timing? "on": "off";
				break;
			case CMD_TRACE:
				phrase = "Evaluation tracing";
				state = trace_eval? "on": "off";
				break;
			case CMD_STEP:
				phrase = "Single stepping";
				state = single_step? "on": "off";
				break;
			case CMD_ETA:
				phrase = "Eta reduction";
				state = eta_reduction? "on": "off";
				break;
			}

			printf("%s: %s\n", phrase, state);
		}
	| TK_LOAD {looking_for_filename = 1;} FILE_NAME TK_EOL { looking_for_filename = 0; push_and_open($3); }
	| TK_PRINT expression TK_EOL { print_expression($2); free_expression($2); }
	| TK_FREE TK_IDENTIFIER TK_EOL
		{
			struct lambda_expression *e = abbreviation_lookup($2);
			if (!e)
				e = new_variable($2);
			free_vars(e);
			free_expression(e);
		}
	| TK_BOUND TK_IDENTIFIER TK_EOL
		{
			struct lambda_expression *e = abbreviation_lookup($2);
			if (!e)
				e = new_variable($2);
			bound_vars(e);
			free_expression(e);
		}
	| expression TK_LEXICALLY_EQUIVALENT expression TK_EOL
		{
			if (equivalent_graphs($1, $3))
				printf("Equivalent\n");
			else
				printf("Not equivalent\n");
			free_expression($1);
			free_expression($3);
			$1 = $3 = NULL;
		}
	| expression TK_ALPHA_EQUIVALENT expression TK_EOL
		{
			if (alpha_equivalent_graphs($1, $3))
				printf("Alpha Equivalent\n");
			else
				printf("Not alpha equivalent\n");
			free_expression($1);
			free_expression($3);
			$1 = $3 = NULL;
		}
	;

modifiable_command
	: TK_TIMER { found_binary_command = 1; $$ = CMD_TIMER;}
	| TK_TRACE { found_binary_command = 1; $$ = CMD_TRACE;}
	| TK_STEP  { found_binary_command = 1; $$ = CMD_STEP; }
	| TK_ETA   { found_binary_command = 1; $$ = CMD_ETA; }
	;

expression
	: abstraction { $$ = $1; }
	| list        { $$ = $1; }
	| TK_NORMALIZE expression
		{
			enum expressionEvaluationResults eer;
			$$ = reduce_expression($2, &eer);
		}
	| TK_GOEDELIZE expression
		{ $$ = goedelize($2); free_expression($2); }
	;

abstraction
	: TK_LAMBDA list error
		{
			/* More or less empirically discovered that this can leak. */
			free_expression($2);
			YYERROR;
		}
	| TK_LAMBDA list TK_DOT expression
		{
			$$ = abstraction_from_list($2, $4);
			free_expression($2);
			if (NULL == $$) YYERROR;
		}
	| TK_STAR abstraction
		{
			$$ = $2;
			$$->parameterized = 1;
		}
	;

list
	: item      { $$ = $1; }
	| list item { $$ = new_application($1, $2); }
	| list abstraction { $$ = new_application($1, $2); }
	;

item
	: TK_IDENTIFIER
		{
			$$ = abbreviation_lookup($1);
			if (!$$)
				$$ = new_variable($1);
		}
	| TK_STAR item 
		{
			$$ = $2;
			$$->parameterized = 1;
		}
	| TK_IDENTIFIER TK_LBRACE NUMBER TK_RBRACE
		{
			struct lambda_expression *p = abbreviation_lookup($1);
			if (!p)
				$$ = new_variable($1);
			else {
				$$ = deparameterize(p, $3);
			}
		}
	| TK_RESULT
		{
			if (previous_result)
				$$ = copy_expression(previous_result);
			else
				YYERROR;
		}
	| TK_LPAREN expression TK_RPAREN  { $$ = $2; }
	;


%%

void
usage(char *progname)
{
	fprintf(stderr, "%s: lambda calculater\n", progname);
	fprintf(stderr, "Flags:\n");
	fprintf(stderr, "  -L <filename>   read and evaluate filename before accepting user input.\n");
	fprintf(stderr, "  -p              don't do any prompting.\n");
}

void
top_level_cleanup(void)
{
	if (prompting) printf("LC> ");
}

int
main(int ac, char **av)
{
	int r, c;
	struct hashtable *h = new_hashtable(free_lambda_expression);
	struct filename_node *p;
	struct filename_node *load_files = NULL, *load_tail = NULL;

	/* "Atoms" and abbreviations kept in the same struct hashtable. */
	setup_atom_table(h);
	setup_abbreviation_table(h);

	while (-1 != (c = getopt(ac, av, "L:p")))
	{
		switch (c)
		{
		case 'L':
			p = malloc(sizeof(*p));
			p->filename = Atom_string(optarg);
			p->next = NULL;
			if (load_tail)
				load_tail->next = p;
			load_tail = p;
			if (!load_files)
				load_files = p;
			break;
		case 'p':
			prompting = 0;
			break;
		default:
			usage(av[0]);
			exit(1);
			break;
		}
	}

	if (load_files)
	{
		struct filename_node *t, *z;
		for (z = load_files; z; z = t)
		{
			FILE *fin;

			t = z->next;

			printf("load file named \"%s\"\n",
				z->filename);

			if (!(fin = fopen(z->filename, "r")))
			{
				fprintf(stderr, "Problem reading \"%s\": %s\n",
					z->filename, strerror(errno));
				continue;
			}

			set_yyin(z->filename);

			r = yyparse();

			reset_yyin();

			if (r)
				printf("Problem with file \"%s\"\n", z->filename);

			free(z);
			fin = NULL;
		}
	}

	set_yyin_stdin();

	do {
		if (prompting) printf("LC> ");
		r =  yyparse();
	} while (r);
	if (prompting) printf("\n");

	if (previous_result) free_expression(previous_result);

	free_hashtable(h);
	free_all_small_hashtable();
	free_all();

	reset_yyin();

	return r;
}

int
yyerror(const char *s1)
{
    fprintf(stderr, "%s\n", s1);

    return 0;
}

void
start_clock(void)
{
	gettimeofday(&before, NULL);
}

void
stop_clock(void)
{
	gettimeofday(&after, NULL);
}

float
elapsed_time(struct timeval b4, struct timeval aftr)
{
	float r = 0.0;

    if (b4.tv_usec > aftr.tv_usec)
	{
		aftr.tv_usec += 1000000;
		--aftr.tv_sec;
	}

	r = (float)(aftr.tv_sec - b4.tv_sec)
		+ (1.0E-6)*(float)(aftr.tv_usec - b4.tv_usec);

	return r;
}

/*
 * A wrapper around normal_order_reduction() that sets and unset signal
 * handlers, starts a timer, and handles and longjmps that come back
 * from interrupted or timed-out evaluations.
 */
struct lambda_expression *
reduce_expression(struct lambda_expression *e, enum expressionEvaluationResults *eer)
{
	struct lambda_expression *r = NULL;
	int cc;

	*eer = NORMAL_FORM;

	void (*old_sigint_handler)(int);
	void (*old_sigalm_handler)(int);

	old_sigint_handler = signal(SIGINT, sigint_handler);
	old_sigalm_handler = signal(SIGALRM, sigint_handler);

	if (!(cc = sigsetjmp(in_reduce_expression, 1)))
	{
		alarm(reduction_timeout);
		r = normal_order_reduction(e);
		alarm(0);
	} else {
		const char *phrase = "Unset";
		alarm(0);
		switch (cc)
		{
		case 1:
			phrase = "Interrupt";
			*eer = INTERRUPT;
			break;
		case 2:
			phrase = "Timeout";
			*eer = TIMEOUT;
			break;
		default:
			phrase = "Unknown";
			*eer = INTERRUPT;
			break;
		}
		++interpreter_interrupted;
		printf("%s\n", phrase);
	}

	signal(SIGINT, old_sigint_handler);
	signal(SIGALRM, old_sigalm_handler);

	return r;
}

void
sigint_handler(int signo)
{
	siglongjmp(in_reduce_expression, signo == SIGINT? 1: 2);
}

/*
 * Passed in to the hashtable create & init function, adapts the
 * data type kept in hashtable to the data type of an abstract syntax
 * tree.
 */
void
free_lambda_expression(void *data)
{
	free_expression((struct lambda_expression *)data);
}

/* Based on a list of bound variables, recursively make abstract-syntax
 * abstractions out of a body, and the bound variables.  One complication:
 * the "list" is really a binary tree of structs lambda_exptression *,
 * so you have to do a little bit of work.  The innermost bound variables
 * occupy right-most leaves on the binary tree.
 */
struct lambda_expression *
abstraction_from_list(struct lambda_expression * list, struct lambda_expression *body)
{
	struct lambda_expression *r = NULL;
	switch (list->typ)
	{
	case VARIABLE:
		r = new_abstraction(list->variable , body);
		break;
	case APPLICATION:
		if (VARIABLE != list->rand->typ)
		{
			fprintf(stderr, "Bound variable list incorrect\n");
			free_expression(body);
		} else
			r = abstraction_from_list(
				list->rator,
				new_abstraction(list->rand->variable, body)
			);
		break;
	case ABSTRACTION:
		/* egregious error */
		fprintf(stderr, "Abstraction appearing in bound variable list\n");
		free_expression(body);
		break;
	}
	return r;
}
