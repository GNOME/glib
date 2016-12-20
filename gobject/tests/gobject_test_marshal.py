#!/usr/bin/env python3

# FIXME: where does the #include "marshalers.h" go?

import sys, subprocess

if len(sys.argv) != 3:
    print('Usage: {0} <listname> <outputfile>')
    sys.exit(0)

glib_genmarshal = sys.argv[1]
listname = sys.argv[2]
outname = sys.argv[3]

if outname.endswith('.h'):
    arg = '--header'
else:
    arg = '--body'

output = subprocess.check_output([glib_genmarshal, '--prefix=test', '--valist-marshallers', arg, listname])
open(outname, 'wb').write(output)
