#!/bin/bash

set -e

git clone --depth 1 --no-tags https://gitlab.gnome.org/GNOME/glib.git
meson subprojects download --sourcedir glib
rm glib/subprojects/*.wrap
mv glib/subprojects/ .
rm -rf glib
