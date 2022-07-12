#!/bin/bash

set -e

export PATH="/c/msys64/$MSYSTEM/bin:$PATH"
if [[ "$MSYSTEM" == "MINGW32" ]]; then
    export MSYS2_ARCH="i686"
else
    export MSYS2_ARCH="x86_64"
fi

pacman --noconfirm -Suy

pacman --noconfirm -S --needed \
    base-devel \
    lcov \
    mingw-w64-$MSYS2_ARCH-ccache \
    mingw-w64-$MSYS2_ARCH-gettext \
    mingw-w64-$MSYS2_ARCH-libffi \
    mingw-w64-$MSYS2_ARCH-meson \
    mingw-w64-$MSYS2_ARCH-pcre2 \
    mingw-w64-$MSYS2_ARCH-python3 \
    mingw-w64-$MSYS2_ARCH-python-pip \
    mingw-w64-$MSYS2_ARCH-toolchain \
    mingw-w64-$MSYS2_ARCH-zlib \
    mingw-w64-$MSYS2_ARCH-libelf

mkdir -p _coverage
mkdir -p _ccache
CCACHE_BASEDIR="$(pwd)"
CCACHE_DIR="${CCACHE_BASEDIR}/_ccache"
export CCACHE_BASEDIR CCACHE_DIR

pip3 install --upgrade --user meson==0.60.3

PATH="$(cygpath "$USERPROFILE")/.local/bin:$HOME/.local/bin:$PATH"
CFLAGS="-coverage -ftest-coverage -fprofile-arcs"
DIR="$(pwd)"
export PATH CFLAGS

meson --werror --buildtype debug _build
cd _build
ninja

lcov \
    --quiet \
    --config-file "${DIR}"/.lcovrc \
    --directory "${DIR}/_build" \
    --capture \
    --initial \
    --output-file "${DIR}/_coverage/${CI_JOB_NAME}-baseline.lcov"

# FIXME: fix the test suite
meson test --timeout-multiplier "${MESON_TEST_TIMEOUT_MULTIPLIER}" --no-suite flaky || true

lcov \
    --quiet \
    --config-file "${DIR}"/.lcovrc \
    --directory "${DIR}/_build" \
    --capture \
    --output-file "${DIR}/_coverage/${CI_JOB_NAME}.lcov"
