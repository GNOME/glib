#!/bin/sh

set -e
set -u

if ! command -v aclocal >/dev/null; then
    echo "1..0 # SKIP aclocal not available in PATH"
    exit 0
fi

if ! command -v autoconf >/dev/null; then
    echo "1..0 # SKIP autoconf not available in PATH"
    exit 0
fi

if ! command -v mktemp >/dev/null; then
    echo "1..0 # SKIP mktemp not available in PATH"
    exit 0
fi

ACLOCAL_FLAGS=

if [ -n "$G_TEST_SRCDIR" ]; then
    ACLOCAL_FLAGS="-I $G_TEST_SRCDIR/.."
fi

tmpdir="$(mktemp -d)"
test_num=0
fail=0

cat > "$tmpdir/result.txt.in" <<'EOF'
HAVE_GLIB=@HAVE_GLIB@
EOF

for ver in "" "2.2.0" "666.0.0"; do
    for modules in "" "gio" "gio gobject"; do
        test_num=$(( $test_num + 1 ))
        echo "# Testing: version '$ver', modules '$modules'"

        cat > "$tmpdir/configure.ac" <<EOF
AC_INIT([Project], [0])
AM_PATH_GLIB_2_0([$ver], [HAVE_GLIB=true], [HAVE_GLIB=false], [$modules])
AC_SUBST([HAVE_GLIB])
AC_CONFIG_FILES([result.txt])
AC_OUTPUT
EOF
        (
            cd "$tmpdir"
            aclocal $ACLOCAL_FLAGS
            autoconf
            ./configure
        ) >&2

        if [ "$ver" = 666.0.0 ]; then
            if grep '^HAVE_GLIB=false$' "$tmpdir/result.txt" >/dev/null; then
                echo "ok $test_num - did not find '$modules' version '$ver'"
            else
                echo "not ok $test_num - should not have found '$modules' version '$ver'"
                fail=1
            fi
        else
            if grep '^HAVE_GLIB=true$' "$tmpdir/result.txt" >/dev/null; then
                echo "ok $test_num - found '$modules' version '$ver'"
            else
                echo "not ok $test_num - should have found '$modules' version '$ver'"
                fail=1
            fi
        fi
    done
done

rm -fr "$tmpdir"
echo "1..$test_num"
exit "$fail"
