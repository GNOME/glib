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
for I in $srcdir/markups/fail-*.gmarkup; do
  echo_v "Parsing $I, should fail"
  ./markup-test $I > /dev/null 2> $error_out && fail "failed to generate error on $I"
  if test "$?" != "1"; then
    fail "unexpected error on $I"
  fi  
done

for I in $srcdir/markups/valid-*.gmarkup; do
  echo_v "Parsing $I, should succeed"
  ./markup-test $I > /dev/null 2> $error_out || fail "failed on $I"
done

echo_v "All tests passed."
