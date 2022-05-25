#!/bin/bash

set +e

case "$1" in
  --log-file)
    log_file="$2"
    shift
    shift
    ;;
  *)
    log_file="_build/meson-logs/testlog.json"
esac

meson test \
        -C _build \
        --timeout-multiplier "${MESON_TEST_TIMEOUT_MULTIPLIER}" \
        --no-suite flaky \
        "$@"

exit_code=$?

exit $exit_code
