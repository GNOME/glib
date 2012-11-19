#!/bin/sh
# Process a .symbol file which is a list of symbols, one per line,
# with C preprocessor conditionals.  Some symbols maybe suffixed
# PRIVATE, which is ignored on Unix.

set -e

SYMBOLFILE=$1
OUTPUT=$2

usage () {
    echo "$0: usage: [--win32] SYMBOLFILE OUTPUT"
    exit 1
}

test -n "$SYMBOLFILE" || usage
test -n "$OUTPUT" || usage

egrep '^#([^i]|if).*[^\]$' "${top_builddir:-..}/glib/glibconfig.h" > glibconfig.cpp

if egrep '^#define G_OS_WIN32' glibconfig.cpp; then
    win32_mode=true
else
    win32_mode=false
fi

INCLUDES="-include ${top_builddir:-..}/config.h"
INCLUDES="$INCLUDES -include glibconfig.cpp $GLIB_DEBUG_FLAGS"

rm -f $OUTPUT.tmp
touch $OUTPUT.tmp
if $win32_mode; then
    INCLUDES="$INCLUDES -DG_OS_WIN32"
    SED_ARG='s/^/	/'
    echo EXPORTS >> $OUTPUT.tmp
else
    INCLUDES="$INCLUDES -DG_STDIO_NO_WRAP_ON_UNIX"
    SED_ARG='s/ PRIVATE$//'
fi
    
cpp -P  $INCLUDES $SYMBOLFILE | sed -e '/^$/d' -e "$SED_ARG" >> $OUTPUT.tmp && mv $OUTPUT.tmp $OUTPUT

rm -f glibconfig.cpp

