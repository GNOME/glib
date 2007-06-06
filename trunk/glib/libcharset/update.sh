#!/bin/sh

if test $# = 1 ; then 
  ORIGINAL=$1
else
  echo "Usage: update.sh /path/to/libcharset" 1>&2
  exit 1
fi

if test -f $ORIGINAL/lib/localcharset.c ; then : ; else
  echo "Usage: update.sh /path/to/libcharset" 1>&2
  exit 1
fi

VERSION=`grep VERSION= $ORIGINAL/configure.in | sed s/VERSION=//`

for i in localcharset.c ref-add.sin ref-del.sin config.charset ; do
  cp $ORIGINAL/lib/$i .
done

cp $ORIGINAL/include/libcharset.h.in ./libcharset.h

for i in codeset.m4 glibc21.m4 ; do
  cp $ORIGINAL/m4/$i .
done

patch -p0 < libcharset-glib.patch

echo "dnl From libcharset $VERSION" > ../../aclibcharset.m4


