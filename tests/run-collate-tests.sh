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

LC_ALL=en_US
export LC_ALL

error_out=/dev/null
if [ "$1" = "-v" ]; then
  verbose=1
  error_out=/dev/stderr
fi  
for I in ${srcdir:-.}/collate/*.in; do
  echo_v "Sorting $I"
  name=`basename $I .in`
  ./unicode-collate $I > collate.out
  if ! diff collate.out ${srcdir:-.}/collate/$name.unicode; then 
    fail "unexpected error when using g_utf8_collate() on $I"
  fi  
  ./unicode-collate --key $I > collate.out
  if ! diff collate.out ${srcdir:-.}/collate/$name.unicode; then 
    fail "unexpected error when using g_utf8_collate_key() on $I"
  fi  
  ./unicode-collate --file $I > collate.out
  if ! diff collate.out ${srcdir:-.}/collate/$name.file; then 
    fail "unexpected error when using g_utf8_collate_key_for_filename() on $I"
  fi  
done

echo_v "All tests passed."
