#!/bin/bash

set -ex

export PATH="/c/msys64/$MSYSTEM/bin:$PATH"

pacman --noconfirm -Suy

pacman --noconfirm -S --needed \
    base-devel \
    lcov \
    "${MINGW_PACKAGE_PREFIX}"-ccache \
    "${MINGW_PACKAGE_PREFIX}"-gettext \
    "${MINGW_PACKAGE_PREFIX}"-libffi \
    "${MINGW_PACKAGE_PREFIX}"-meson \
    "${MINGW_PACKAGE_PREFIX}"-pcre2 \
    "${MINGW_PACKAGE_PREFIX}"-python3 \
    "${MINGW_PACKAGE_PREFIX}"-python-pip \
    "${MINGW_PACKAGE_PREFIX}"-toolchain \
    "${MINGW_PACKAGE_PREFIX}"-zlib \
    "${MINGW_PACKAGE_PREFIX}"-libelf

mkdir -p _coverage
mkdir -p _ccache
CCACHE_BASEDIR="$(pwd)"
CCACHE_DIR="${CCACHE_BASEDIR}/_ccache"
export CCACHE_BASEDIR CCACHE_DIR

pip3 install --upgrade --user meson==1.0.0

PATH="$(cygpath "$USERPROFILE")/.local/bin:$HOME/.local/bin:$PATH"
DIR="$(pwd)"
export PATH CFLAGS

if [[ "$MSYSTEM" == "CLANG64" ]]; then
    # FIXME: fix the clang build warnings
    # shellcheck disable=SC2086
    meson ${MESON_COMMON_OPTIONS} _build
else
    # shellcheck disable=SC2086
    meson ${MESON_COMMON_OPTIONS} --werror _build
fi

cd _build
ninja

if [[ "$CFLAGS" == *"-coverage"* ]]; then
    lcov \
        --quiet \
        --config-file "${DIR}"/.lcovrc \
        --directory "${DIR}/_build" \
        --capture \
        --initial \
        --output-file "${DIR}/_coverage/${CI_JOB_NAME}-baseline.lcov"
fi

meson test -v --timeout-multiplier "${MESON_TEST_TIMEOUT_MULTIPLIER}"
meson test -v --timeout-multiplier "${MESON_TEST_TIMEOUT_MULTIPLIER}" \
    --setup=unstable_tests --suite=failing --suite=flaky || true

if [[ "$CFLAGS" == *"-coverage"* ]]; then
    lcov \
        --quiet \
        --config-file "${DIR}"/.lcovrc \
        --directory "${DIR}/_build" \
        --capture \
        --output-file "${DIR}/_coverage/${CI_JOB_NAME}.lcov"
fi
