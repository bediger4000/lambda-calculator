#lambda calculus interpreter

This is the code for `lc`, a lambda calculus interpreter.

`lc` does normal order (leftmost-outermost first) beta and eta reductions.

`lc` will rename bound variables to prevent variable capture.

##BUILDING

I did not do GNU-style `autoconf` scripts.  I did write reasonably strict
ANSI C (C89/C90 version, I hope) that compiles under a number of compilers
and operating systems.

To build the `lc` executable:

    make gnu      # should work on most linuxes that have devel environment
    make cc       # should work on most *BSDs, uses traditionally named tools
    make pcc      # Uses the "pcc" C compiler
    make lcc      # Uses the "lcc" C compiler
    make tcc      # Uses the "tcc" C compiler

Once that finishes (and it only takes a few seconds), you can do:

    ./runtests    # to ensure that all the tests pass.

##INSTALLING

`lc` is a command line, interactive program.  It does not have any implicit
startup files, or location or directory dependencies.  You can cp or mv
the `lc` executable anwhere in your `PATH`, or leave it somewhere and call it
explicitly:

    % $HOME/src/lc-1.0/lc
    LC>

`lc` uses `LC>` as its prompt for interactive input.

##RUNNING

You have to use Control-D (end-of-file) to get it to exit cleanly.  It does
not interpreter a special "exit" or "quit" command.

Long-running or patience-exhausting reductions can be terminated with
Control-C.  The `LC>` prompt should return.  Control-C'ing `lc` at the `LC>`
prompt will cause it to exit.

##LAMBDA CALCULUS TERMS

Variables, bound or free, look like C or Java identifiers: start with a
letter, contain letters, digits or underscores.

The lambda-character prints as '%', but the user can use '%', '$', '^' or
'\'.  The program reads and writes ASCII, so a Unicode lambda won't work.

Binding sites ("\x") get seperated from an abstraction body by "." or "->".
`\x.x x x` is the same as `$x -> x x x`.

Hopefully, this allows some compatibility with other lambda calculators
floating around the net.

##TERM EQUIVALENCE

Alpha equivalence: `term1 = term2`

Total lexical equivalence: `term1 == term2`

##ABBREVIATIONS

    define identifier lambdaterm
    def identifier lambdaterm
    $$

Using a previously-defined identifier in a new lambda term causes the
interpreter to put a copy of the term's definition in the new term.  Be
careful. Abstractions can "capture" identifiers in the abbreviation that
lexically match the bound variable.

The special token `$$` always represents the last normal form calculated.
It changes every time something redues to a normal form.  The user can
put `$$` in the next term input as many times as desired. Copies of the
last normal form get inserted.

##PARAMETERIZED ABBREVIATIONS

    define identifier *term
    define identifier term1 *term2
    define identifier *term1 *term2

Marked subterms of the term denoted by _identifier_ can get expanded.
When the interpreter sees a use of an abbreviation like `identifier{N}`
(where N is greater than zero) it expands the marked terms N times.
This feature can be used to make Church numerals more convenient.

    LC> def C0 \f n.n
    LC> def C  \f n.*f n
    LC> print C{2}
    %f.%n.f (f n)

##INTERPRETER COMMANDS

Print elapsed time of any reduction to normal form, default off:

    timer on
    timer off

Perform eta reductions, default on:

    eta on
    eta off

Print information about each redex found and performed:

    trace on
    trace off

Require user to hit return after each reduction:

    step on
    step off

Read in and evaluate a file full of `lc` input:

    load "some/filename"
