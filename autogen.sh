#!/bin/sh
# Run this to generate all the initial makefiles, etc.

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

ORIGDIR=`pwd`
cd $srcdir
PROJECT=GLib
TEST_TYPE=-f
FILE=glib/glib.h

DIE=0

have_libtool=false
if libtool --version < /dev/null > /dev/null 2>&1 ; then
	libtool_version=`libtoolize --version |  libtoolize --version | sed 's/^[^0-9]*\([0-9.][0-9.]*\).*/\1/'`
	case $libtool_version in
	    1.4*)
		have_libtool=true
		;;
	esac
fi
if $have_libtool ; then : ; else
	echo
	echo "You must have libtool 1.4 installed to compile $PROJECT."
	echo "Install the appropriate package for your distribution,"
	echo "or get the source tarball at ftp://ftp.gnu.org/pub/gnu/"
	DIE=1
fi

(autoconf --version) < /dev/null > /dev/null 2>&1 || {
	echo
	echo "You must have autoconf installed to compile $PROJECT."
	echo "libtool the appropriate package for your distribution,"
	echo "or get the source tarball at ftp://ftp.gnu.org/pub/gnu/"
	DIE=1
}

have_automake=false
if automake --version < /dev/null > /dev/null 2>&1 ; then
	automake_version=`automake --version | grep 'automake (GNU automake)' | sed 's/^[^0-9]*\(.*\)/\1/'`
	case $automake_version in
	   1.2*|1.3*|1.4) 
		;;
	   *)
		have_automake=true
		;;
	esac
fi
if $have_automake ; then : ; else
	echo
	echo "You must have automake 1.4-p1 installed to compile $PROJECT."
	echo "Get ftp://ftp.gnu.org/pub/gnu/automake/automake-1.4-p1.tar.gz"
	echo "(or a newer version if it is available)"
	DIE=1
fi

have_gettext=false
if xgettext --version 2>/dev/null | grep 'GNU' > /dev/null ; then 
        have_gettext=true
fi

if $have_gettext ; then : ; else
       echo
       echo "GNU gettext must be installed to build GLib from CVS"
       echo "GNU gettext is available from http://www.gnu.org/software/gettext/"
       DIE=1
fi

if test "$DIE" -eq 1; then
	exit 1
fi

test $TEST_TYPE $FILE || {
	echo "You must run this script in the top-level $PROJECT directory"
	exit 1
}

if test -z "$AUTOGEN_SUBDIR_MODE"; then
        if test -z "$*"; then
                echo "I am going to run ./configure with no arguments - if you wish "
                echo "to pass any to it, please specify them on the $0 command line."
        fi
fi

case $CC in
*xlc | *xlc\ * | *lcc | *lcc\ *) am_opt=--include-deps;;
esac

aclocal $ACLOCAL_FLAGS

# optionally feature autoheader
(autoheader --version)  < /dev/null > /dev/null 2>&1 && autoheader

automake -a $am_opt
autoconf
cd $ORIGDIR

if test -z "$AUTOGEN_SUBDIR_MODE"; then
        $srcdir/configure --enable-maintainer-mode --enable-gtk-doc "$@"

        echo 
        echo "Now type 'make' to compile $PROJECT."
fi