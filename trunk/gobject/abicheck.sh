#! /bin/sh

egrep '^#([^i]|if).*[^\]$' "${top_builddir:-..}/glibconfig.h" > glibconfig.cpp

INCLUDES="-include ${top_builddir:-..}/config.h"
INCLUDES="$INCLUDES -include glibconfig.cpp"

cpp -DINCLUDE_VARIABLES -P $INCLUDES -DALL_FILES ${srcdir:-.}/gobject.symbols | sed -e '/^$/d' -e 's/ G_GNUC.*$//' -e 's/ PRIVATE$//' | sort > expected-abi
rm glibconfig.cpp

nm -D -g --defined-only .libs/libgobject-2.0.so | cut -d ' ' -f 3 | sort > actual-abi

diff -u expected-abi actual-abi && rm expected-abi actual-abi
