#! /bin/sh

cpp -P -DG_OS_UNIX -DINCLUDE_INTERNAL_SYMBOLS ${srcdir:-.}/gobject.symbols | sed -e '/^$/d' | sort > expected-abi
nm -D .libs/libgobject-2.0.so | grep " T " | cut -c12- | sort > actual-abi
diff -u expected-abi actual-abi && rm expected-abi actual-abi
