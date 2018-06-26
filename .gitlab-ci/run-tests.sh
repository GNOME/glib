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
        --timeout-multiplier ${MESON_TEST_TIMEOUT_MULTIPLIER} \
        --no-suite flaky \
        "$@"

exit_code=$?

python3 .gitlab-ci/meson-junit-report.py \
        --project-name=glib \
        --job-id "${CI_JOB_NAME}" \
        --output "_build/${CI_JOB_NAME}-report.xml" \
        "${log_file}"

exit $exit_code
