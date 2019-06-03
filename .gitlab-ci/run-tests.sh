#!/bin/bash

set +e

gdb --args _build/glib/tests/contenttype <<END
run
quit
END

meson test \
        -C _build \
        --timeout-multiplier ${MESON_TEST_TIMEOUT_MULTIPLIER} \
        --no-suite flaky \
        -v \
        --wrapper 'valgrind' \
        contenttype

exit_code=$?

python3 .gitlab-ci/meson-junit-report.py \
        --project-name=glib \
        --job-id "${CI_JOB_NAME}" \
        --output "_build/${CI_JOB_NAME}-report.xml" \
        _build/meson-logs/testlog.json

exit $exit_code
