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
/*
 * A stripped-down "atom" datatype, a la "C Interfaces and Implementation".
 * The idea is that a program only keeps around one instance of a given
 * string, so the input mechanism (lex in this program's case) "interns"
 * every input string, and always gives the sole instance of that string
 * to the rest of the program.  You can get string identity with "="
 * operator, and you don't have to deallocate any string until the program
 * exits.
 */


#include <string.h>
#include <hashtable.h>
#include <atom.h>

/* $Id: atom.c,v 1.8 2011/11/12 17:08:31 bediger Exp $ */

static struct hashtable *atom_table = NULL;

void
setup_atom_table(struct hashtable *h)
{
	atom_table = h;
}

const char *
Atom_new(const char *str)
{
	int len = 0;
	return string_lookup(atom_table, str, &len);
}

const char *
Atom_string(const char *str)
{
	return Atom_new(str);
}
