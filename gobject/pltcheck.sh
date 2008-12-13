#!/bin/sh

LANG=C

status=0

if ! which readelf 2>/dev/null >/dev/null; then
	echo "'readelf' not found; skipping test"
	exit 0
fi

for so in .libs/lib*.so; do
	echo Checking $so for local PLT entries
	readelf -r $so | grep 'JU\?MP_SLOT\?' | grep '\<g_type_\|\<g_boxed_\|\<g_value_\|\<g_cclosure_\|\<g_closure_\|\<g_signal\|\<g_enum_\|\<g_flags_\|\<g_io_\|\<g_object_\|\<g_param_' && status=1
done

exit $status
