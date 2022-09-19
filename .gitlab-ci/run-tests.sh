#!/bin/bash

./.gitlab-ci/check-missing-install-tag.py _build

meson test \
        -C _build \
        --timeout-multiplier "${MESON_TEST_TIMEOUT_MULTIPLIER}" \
        --no-suite flaky \
        "$@"
