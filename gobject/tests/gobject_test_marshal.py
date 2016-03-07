#!/usr/bin/env python3

# FIXME: where does the #include "marshalers.h" go?

import sys, subprocess

assert(len(sys.argv) == 3)

_, listname, outname = sys.argv

if outname.endswith('.h'):
    arg = '--header'
else:
    arg = '--body'

output = subprocess.check_output(['glib-genmarshal', '--prefix=test', '--valist-marshallers', arg, listname])
open(outname, 'wb').write(output)
