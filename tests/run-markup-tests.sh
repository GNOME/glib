#! /bin/sh

fail ()
{
  echo "Test failed: $*"
  exit 1
}

for I in $srcdir/markups/fail-*.gmarkup; do
  echo "Parsing $I, should fail"
  ./markup-test $I > /dev/null && fail "failed to generate error on $I"
done

for I in $srcdir/markups/valid-*.gmarkup; do
  echo "Parsing $I, should succeed"
  ./markup-test $I > /dev/null || fail "failed on $I"
done

echo "All tests passed."
