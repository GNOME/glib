#! /bin/sh

fail ()
{
  echo "Test failed: $*"
  exit 1
}

echo_v ()
{
  if [ "$verbose" = "1" ]; then
    echo "$*"
  fi
}

error_out=/dev/null
if [ "$1" = "-v" ]; then
  verbose=1
  error_out=/dev/stderr
fi  
for I in ${srcdir:-.}/bookmarks/fail-*.xbel; do
  echo_v "Parsing $I, should fail"
  ./bookmarkfile-test $I > /dev/null 2> $error_out && fail "failed to generate error on $I"
  if test "$?" != "1"; then
    fail "unexpected error on $I"
  fi  
done

for I in ${srcdir:-.}/bookmarks/valid-*.xbel; do
  echo_v "Parsing $I, should succeed"
  ./bookmarkfile-test $I > /dev/null 2> $error_out || fail "failed on $I"
done

echo_v "All tests passed."
