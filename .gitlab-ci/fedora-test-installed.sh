#! /bin/bash

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
meson gio/tests/static-link _build_static_link
ninja -C _build_static_link test

# Make sure the executable is not dynamically linked on our libraries
if ldd _build_static_link/test-static-link | grep -q libgio; then
  echo "test-static-link is dynamically linked on libgio"
  exit 1
fi
