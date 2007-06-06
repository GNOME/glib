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
for I in ${srcdir:-.}/markups/fail-*.gmarkup; do
  echo_v "Parsing $I, should fail"
  ./markup-test $I > /dev/null 2> $error_out && fail "failed to generate error on $I"
  if test "$?" != "1"; then
    fail "unexpected error on $I"
  fi  
done

I=1
while test $I -lt 100 ; do
  F=${srcdir:-.}/markups/valid-$I.gmarkup
  if [ -f $F ] ; then
    echo_v "Parsing $F, should succeed"
    ./markup-test $F > actual 2> $error_out || fail "failed on $F"
    diff ${srcdir:-.}/markups/expected-$I actual || fail "unexpected output on $F"
    rm actual
  fi
  I=`expr $I + 1`
done

echo_v "All tests passed."
