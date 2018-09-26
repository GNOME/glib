#!/usr/bin/env python3

import os
import sys
import tempfile
import subprocess

if not 'GLIB_TEST_COMPILATION' in os.environ:
  print('''Test disabled because GLIB_TEST_COMPILATION is not set in the env.
If you wish to run this test, set GLIB_TEST_COMPILATION=1 in the env,
and make sure you have glib build dependencies installed, including
meson.''')
  sys.exit(0)

if len(sys.argv) != 2:
  print('Usage: {} <gio-2.0.pc dir>'.format(os.path.basename(sys.argv[0])))
  sys.exit(1)

test_dir = os.path.dirname(sys.argv[0])

with tempfile.TemporaryDirectory() as builddir:
  env = os.environ.copy()
  env['PKG_CONFIG_PATH'] = sys.argv[1]
  sourcedir = os.path.join(test_dir, 'static-link')

  # Ensure we can static link and run a test app
  subprocess.check_call(['meson', sourcedir, builddir], env=env)
  subprocess.check_call(['ninja', '-C', builddir, 'test'], env=env)
  # FIXME: This probably only works on Linux
  out = subprocess.check_output(['ldd', os.path.join(builddir, 'test-static-link')], env=env).decode()
  if 'libgio' in out:
    print('test-static-link is dynamically linked on libgio')
    exit(1)
