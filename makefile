# $Id: makefile,v 1.18 2011/11/03 12:57:58 bediger Exp $
all:
	@echo "Try one of these:"
	@echo "make cc"   "- very generic"
	@echo "make gnu"  "- all GNU"
	@echo "make coverage"  "- all GNU, with gcov options on"
	@echo "make lcc"  "- lcc C compiler and yacc"
	@echo "make tcc"  "- tcc C compiler and yacc"
	@echo "make pcc"  "- pcc C compiler and yacc"

cc:
	make CC=cc YACC='yacc -d -v' LEX=lex CFLAGS='-I. -g ' build
gnu:
	make CC=gcc YACC='bison -d -b y' LEX=flex CFLAGS='-I. -g  -Wall  ' build
mudflap:
	make CC=gcc YACC='bison -d -b y' LEX=flex CFLAGS='-I. -g -fmudflap -Wall' LIBS=-lmudflap build
coverage:
	make CC=gcc YACC='bison -d -b y' LEX=flex CFLAGS='-I. -fprofile-arcs -ftest-coverage' build
lcc:
	make CC=lcc YACC='yacc -d -v' CFLAGS='-I.' build
tcc:
	make CC='tcc -Wall' YACC='yacc -d -v' CFLAGS='-I.' build
pcc:
	make CC=pcc YACC='yacc -d -v' LEX=lex CFLAGS='-I. -g' build
clang:
	make CC=clang YACC='yacc -d -v -t ' LEX=lex CFLAGS='-I. -g -Wall ' build
special:
	make CC=gcc YACC='yacc -d -v' LEX=flex sbuild

sbuild:
	make CFLAGS='-Wunused -Wpointer-arith -Wunused-parameter -Wstrict-prototypes -Wmissing-prototypes -Wpointer-arith -Wreturn-type -Wcast-qual -Wswitch -Wshadow -Wcast-align -Wwrite-strings -Wchar-subscripts -Winline -Wnested-externs -Wshadow -Wsequence-point -Wnonnull -Wstrict-aliasing -Wswitch -Wswitch-enum -O2 -g  -I.'  build

build: lc

OBJS = abbreviations.o atom.o buffer.o evaluation.o hashtable.o \
	lambda_expression.o small_hashtable.o
GENOBJS = y.tab.o lex.yy.o

lc: $(OBJS) $(GENOBJS)
	$(CC) $(CFLAGS) -o lc $(GENOBJS) $(OBJS) $(LIBS)

abbreviations.o: abbreviations.c abbreviations.h hashtable.h \
	small_hashtable.h buffer.h lambda_expression.h
atom.o: atom.c atom.h hashtable.h
buffer.o: buffer.c buffer.h
evaluation.o: evaluation.c small_hashtable.h buffer.h \
	lambda_expression.h evaluation.h hashtable.h atom.h
hashtable.o: hashtable.c hashtable.h small_hashtable.h buffer.h \
	lambda_expression.h
lambda_expression.o: lambda_expression.c small_hashtable.h buffer.h \
	lambda_expression.h hashtable.h atom.h
small_hashtable.o: small_hashtable.c small_hashtable.h

y.tab.o: y.tab.c y.tab.h parser.h
lex.yy.o: lex.yy.c y.tab.h parser.h

y.tab.c y.tab.h: grammar.y
	$(YACC) $(YFLAGS) grammar.y

lex.yy.c: lex.l
	$(LEX) lex.l

clean:
	-rm -rf $(OBJS) $(GENOBJS)
	-rm -rf lc
	-rm -rf *core y.output
	-rm -rf y.tab.c lex.yy.c y.tab.h
	-rm -rf test.out/output.*
	-rm -rf *.gcda *.gcno
