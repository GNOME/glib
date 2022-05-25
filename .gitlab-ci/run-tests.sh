#!/bin/bash

meson test \
        -C _build \
        --timeout-multiplier "${MESON_TEST_TIMEOUT_MULTIPLIER}" \
        --no-suite flaky \
        "$@"
