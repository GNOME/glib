#! /usr/bin/bash

set -e

prefix=$1

# Verify there is no distro gio-2.0 installed, so we are sure we're going to
# test the built one.
if `pkg-config --exists gio-2.0`; then
  echo "Docker image shouldn't have gio installed"
  exit 1
fi

# Ensure we can static link and run a test app
export PKG_CONFIG_PATH=$prefix/lib64/pkgconfig
gcc -o fedora-test-installed fedora-test-installed.c -static $(pkg-config gio-2.0 --cflags --libs --static)
./fedora-test-installed
