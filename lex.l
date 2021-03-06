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

/* $Id: lex.l,v 1.26 2011/11/12 04:50:28 bediger Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <parser.h>
#include <hashtable.h>
#include <atom.h>

#include "y.tab.h"

int lineno = 0;

const char *current_input_stream;

char *unescape_string(char *s);
void  push_and_open(const char *filename);

struct stream_node {  
#ifdef FLEX_SCANNER   
	YY_BUFFER_STATE stream;
#else
	FILE *stream;
#endif
	struct stream_node *next;
	const char *old_filename;
	int old_lineno;
};

struct stream_node *file_stack = NULL;

void set_yyin_stdin(void);
void reset_yyin(void);
void set_yyin(const char *filename);

extern int looking_for_filename;  /* from grammar.y */
extern int found_binary_command;  /* from grammar.y */

extern FILE *yyin;
extern const char *current_input_stream;

%}

%%

\#..*$		{ return TK_EOL; }
"def"	    { return TK_DEF; }
"define"    { return TK_DEF; }
"normalize"	{ return TK_NORMALIZE; }
"godelize"	{ return TK_GOEDELIZE; }
"load"  { return TK_LOAD; }
"free"	{ return TK_FREE; }
"bound"	{ return TK_BOUND; }

"timer"	{ return TK_TIMER; }
"=="	{ return TK_LEXICALLY_EQUIVALENT; }
"="	{ return TK_ALPHA_EQUIVALENT; }
"step"	{ return TK_STEP; }
"trace"	{ return TK_TRACE; }
"eta"	{ return TK_ETA; }

"print"	{ return TK_PRINT; }
\"(\\.|[^\\"])*\" {
	char *tmp;
	tmp = unescape_string(yytext);
	yylval.string_constant = Atom_string(tmp);
	free(tmp);
	return FILE_NAME;
}
"on"|"off" {
	const char *p = Atom_string(yytext);
	yylval.string_constant = p;
	if (found_binary_command)
		return BINARY_MODIFIER;
	else if (looking_for_filename)
		return FILE_NAME;
	else {
		yylval.identifier = p;
		return TK_IDENTIFIER;
	}
}
[a-zA-Z_][a-zA-Z0-9_-]*	{
	if (looking_for_filename)
	{
		yylval.string_constant = Atom_string(yytext);
		return FILE_NAME;
	} else {
		yylval.identifier = Atom_string(yytext);
		return TK_IDENTIFIER;
	}
}
"$$" {
	return TK_RESULT;
}
[1-9][0-9]* {
	/* So, 1 is a NUMBER, 2 is a NUMBER, but 0 doesn't fit.
	 * This keeps the de-parameterization code in lambda_expression.c
	 * from having to deal with expr{0} as some weird-beard special case.
	 */
	yylval.number = strtol(yytext, NULL, 10);
	return NUMBER;
}
\*          { return TK_STAR; }
\n		    { return TK_EOL; }
\(		    { return TK_LPAREN; }
\)		    { return TK_RPAREN; }
\.		    { return TK_DOT; }
"->"	    { return TK_DOT; }
\%		    { return TK_LAMBDA; }
\$		    { return TK_LAMBDA; }
\^		    { return TK_LAMBDA; }
\\		    { return TK_LAMBDA; }
\{		    { return TK_LBRACE; }
\}		    { return TK_RBRACE; }
.		    { /* Just eat it. */ }

%%

char *
unescape_string(char *s)
{
	char *n = NULL;
	if (s)
	{
		int i, j, l;
		if ('"' == s[0])
			++s;
		if ('"' == s[strlen(s) - 1])
			s[strlen(s) - 1] = '\0';
		n = malloc(strlen(s) + 1);
		l = strlen(s);
		for (i = 0, j = 0; i < l; ++i)
		{
			if ('\\' == s[i])
			{
				++i;
				switch (s[i])
				{
				/* XXX - doesn't do '\0' or other numerical escapes */
				case 't': n[j++] = '\t';  break;
				case 'r': n[j++] = '\r';  break;
				case 'n': n[j++] = '\n';  break;
				case '\\': n[j++] = '\\'; break;
				default:
					n[j++] = s[i];
					break;
				}
			} else {
				n[j++] = s[i];
			}
		}
		n[j] = '\0';
	}
	return n;
}

void
push_and_open(const char *filename)
{
	FILE *fin;

	if (NULL != (fin = fopen(filename, "r")))
	{
		struct stream_node *n;
		n = malloc(sizeof(*n));
#ifdef FLEX_SCANNER
		n->stream = YY_CURRENT_BUFFER;
		yyin = fin;
		yy_switch_to_buffer(yy_create_buffer(yyin, YY_BUF_SIZE));
#else
		n->stream = yyin;
		yyin = fin;
#endif
		n->next = file_stack;
		n->old_filename = current_input_stream;
		n->old_lineno = lineno;
		current_input_stream = filename;
		file_stack = n;
		lineno = 0;
	} else {
		fprintf(stderr, "Could not open \"%s\" for read: %s\n",
			filename, strerror(errno));
	}
}

void
set_yyin_stdin(void)
{
		yyin = stdin;
#ifdef FLEX_SCANNER
		yy_delete_buffer(YY_CURRENT_BUFFER);
		yy_switch_to_buffer(yy_create_buffer(yyin, YY_BUF_SIZE));
#endif
}

void
set_yyin(const char *filename)
{
	FILE *fin;

	if (NULL != (fin = fopen(filename, "r")))
	{
		yyin = fin;
#ifdef FLEX_SCANNER
		yy_delete_buffer(YY_CURRENT_BUFFER);
		yy_switch_to_buffer(yy_create_buffer(yyin, YY_BUF_SIZE));
#endif
		current_input_stream = filename;
		lineno = 0;
	} else {
		fprintf(stderr, "Could not open \"%s\" for read: %s\n",
			filename, strerror(errno));
	}
}

void
reset_yyin(void)
{
	if (yyin)
		fclose(yyin);
	yyin = NULL;
#ifdef FLEX_SCANNER
	yy_delete_buffer(YY_CURRENT_BUFFER);
#endif
}

int
yywrap()
{
	int r = 1;
	if (file_stack)
	{
		struct stream_node *tmp = file_stack->next;
		fclose(yyin);
#ifdef FLEX_SCANNER
		yy_delete_buffer(YY_CURRENT_BUFFER);
		yy_switch_to_buffer(file_stack->stream);
#else
		yyin = file_stack->stream;
#endif
		current_input_stream = file_stack->old_filename;
		lineno = file_stack->old_lineno;
		file_stack->stream = NULL;
		file_stack->next = NULL;
		free(file_stack);
		file_stack = tmp;
		r = 0;
	}
	return r;
}
