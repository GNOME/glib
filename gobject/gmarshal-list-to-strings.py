#!/usr/bin/env python3
# Does the same thing as `marshal-genstrings.pl` and the 'gmarshal.strings'
# target in Makefile.am

import re
import sys

prefix = '"g_cclosure_marshal_'
suffix = '",\n'

if len(sys.argv) != 3:
    print('Usage: {0} <input> <output>'.format(sys.argv[0]))

fin = open(sys.argv[1], 'r')
fout = open(sys.argv[2], 'w')

for line in fin:
    if not line[0].isalpha():
        continue
    symbol = line[:-1].replace(':', '__').replace(',', '_')
    fout.write(prefix + symbol + suffix)
