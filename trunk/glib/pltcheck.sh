#!/bin/sh

LANG=C

status=0

if ! which readelf 2>/dev/null >/dev/null; then
	echo "'readelf' not found; skipping test"
	exit 0
fi

for so in .libs/lib*.so; do
	echo Checking $so for local PLT entries
	# g_string_insert_c is used in g_string_append_c_inline
	# unaliased.  Couldn't find a way to fix it.
	readelf -r $so | grep 'JU\?MP_SLOT' | grep -v '\<g_string_insert_c\>' | grep '\<g_' && status=1
done

exit $status
