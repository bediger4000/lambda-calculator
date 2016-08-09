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

/* $Id: abbreviations.c,v 1.8 2011/11/12 04:50:27 bediger Exp $ */

#include <stdio.h>
#include <hashtable.h>
#include <small_hashtable.h>
#include <buffer.h>
#include <lambda_expression.h>
#include <abbreviations.h>

struct hashtable *abbr_table = NULL;

void
setup_abbreviation_table(struct hashtable *h)
{
	abbr_table = h;
}

struct lambda_expression *
abbreviation_lookup(const char *id)
{
	struct lambda_expression *r = NULL;
	void *p = lookup_key(abbr_table, id);
	if (p) r = copy_expression(p);
	return r;
}

struct lambda_expression *
abbreviation_add(const char *id, struct lambda_expression *exp)
{
	return (struct lambda_expression *)insert_data(abbr_table, id, exp);
}

