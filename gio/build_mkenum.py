#!/usr/bin/env python3

# This is in its own file rather than inside meson.build
# because a) mixing the two is ugly and b) trying to
# make special characters such as \n go through all
# backends is a fool's errand.

import sys, os, shutil, subprocess

ofilename = sys.argv[1]
template_file_dir = sys.argv[2]
template_file_path = template_file_dir + '/' + ofilename + '.template'
headers = sys.argv[3:]

arg_array = ['--template', template_file_path ]

# FIXME: should use $top_builddir/gobject/glib-mkenums
cmd = [shutil.which('perl'), shutil.which('glib-mkenums')]
pc = subprocess.Popen(cmd + arg_array + headers, stdout=subprocess.PIPE)
(stdo, _) = pc.communicate()
if pc.returncode != 0:
    sys.exit(pc.returncode)
open(ofilename, 'wb').write(stdo)
