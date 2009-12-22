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

echo_v "Running assert-msg-test"
OUT=$(./assert-msg-test 2>&1) && fail "assert-msg-test should abort"
echo "$OUT" | grep -q '^ERROR:assert-msg-test.c:.*:main: assertion failed: (42 < 0)' || \
  fail "does not print assertion message"

if ! type gdb >/dev/null 2>&1; then
  echo_v "Skipped (no gdb installed)"
  exit 0
fi

# do we use libc's or our own variable?
if grep -q '^#define HAVE_LIBC_ABORT_MSG' $(dirname $0)/../config.h; then
  VAR=__abort_msg
else
  VAR=__glib_assert_msg
fi

echo_v "Running gdb on assert-msg-test"
OUT=$(gdb --batch --ex run --ex "print (char*) $VAR" .libs/lt-assert-msg-test 2> $error_out) || \
  fail "failed to run gdb"

echo_v "Checking if assert message is in $VAR"
if ! echo "$OUT" | grep -q '^$1.*"ERROR:assert-msg-test.c:.*:main: assertion failed: (42 < 0)"'; then
  fail "$VAR does not have assertion message"
fi

echo_v "All tests passed."
