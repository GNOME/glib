#! /bin/sh

cpp -P -DG_OS_UNIX -DINCLUDE_INTERNAL_SYMBOLS ${srcdir:-.}/glib.symbols | sed -e '/^$/d' -e 's/ G_GNUC.*$//' -e 's/ PRIVATE$//' | sort > expected-abi
nm -D .libs/libglib-2.0.so | grep " T " | cut -d ' ' -f 3 | sort > actual-abi
diff -u expected-abi actual-abi && rm expected-abi actual-abi
