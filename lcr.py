#!/usr/bin/env python2
# $Id: lcr.py,v 1.2 2011/11/18 23:47:45 bediger Exp $
# Produce random, yet syntactically-correct lambda calculus terms
import sys
import random

tokens = ['\\', '\\', '\\', '\\', '\\',  '\\', 'a', 'b', 'c', 'd', 'e','x', 'y', 'z', 'A', 'B', 'C', 'D', 'E', 'F', 'G', '(', '(', '(' ]
bound_vars = ['a', 'b', 'c', 'd', 'e', 'x', 'y', 'z', 'w' ]
bound_var_cnt = [1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 4, 4, 5, 7]

def output_binding():
	binding_vars = ['x', 'y', 'z', 'w', 'a']
	token = random.choice(binding_vars)
	binding_string = '\\' + token
	binding_vars.append('.')
	binding_vars.append('.')
	binding_vars.append('.')
	token = random.choice(binding_vars)

	while token != '.':
		binding_string += ' ' + token
		token = random.choice(binding_vars)

	print binding_string + '.',
	

def output_expression(min_output_tokens):
	output_token_cnt = 0
	while output_token_cnt < min_output_tokens:
		token = random.choice(tokens)
		if token == '\\':
			output_binding()
			continue
		if token == '(':
			print '(',
			output_token_cnt += output_expression(min_output_tokens - output_token_cnt)
			print ')',
			continue
		print token,
		output_token_cnt += 1

	return output_token_cnt

default_output_tokens = 3
min_output_tokens = 0
min_output_expressions = 1
try:
	min_output_tokens = int(sys.argv[2])
except:
	min_output_tokens = default_output_tokens
	
try:
	min_output_expressions = int(sys.argv[1])
except:
	min_output_expressions = 1

try:
	consecutive_expressions = int(sys.argv[3])
except:
	consecutive_expressions = 0

for i in range(0, min_output_expressions):
	if consecutive_expressions > 0: print '(',
	z = output_expression(min_output_tokens)
	if consecutive_expressions > 0: print ')  ',
	cnt = consecutive_expressions
	while cnt > 0:
		print '(',
		z = output_expression(min_output_tokens)
		print ')',
		cnt -= 1
	print

sys.exit(0)
