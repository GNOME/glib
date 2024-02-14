#!/bin/bash

set -ex

export PATH="/c/msys64/$MSYSTEM/bin:$PATH"

pacman --noconfirm -Suy

pacman --noconfirm -S --needed \
    base-devel \
    lcov \
    "${MINGW_PACKAGE_PREFIX}"-ccache \
    "${MINGW_PACKAGE_PREFIX}"-gettext \
    "${MINGW_PACKAGE_PREFIX}"-gi-docgen \
    "${MINGW_PACKAGE_PREFIX}"-gobject-introspection \
    "${MINGW_PACKAGE_PREFIX}"-libffi \
    "${MINGW_PACKAGE_PREFIX}"-meson \
    "${MINGW_PACKAGE_PREFIX}"-pcre2 \
    "${MINGW_PACKAGE_PREFIX}"-python3 \
    "${MINGW_PACKAGE_PREFIX}"-python-docutils \
    "${MINGW_PACKAGE_PREFIX}"-python-pip \
    "${MINGW_PACKAGE_PREFIX}"-toolchain \
    "${MINGW_PACKAGE_PREFIX}"-zlib \
    "${MINGW_PACKAGE_PREFIX}"-libelf

mkdir -p _coverage
mkdir -p _ccache
CCACHE_BASEDIR="$(pwd)"
CCACHE_DIR="${CCACHE_BASEDIR}/_ccache"
export CCACHE_BASEDIR CCACHE_DIR

pip3 install --upgrade --user meson==1.2.3 packaging==23.2

PATH="$(cygpath "$USERPROFILE")/.local/bin:$HOME/.local/bin:$PATH"
DIR="$(pwd)"
export PATH CFLAGS

# shellcheck disable=SC2086
meson setup ${MESON_COMMON_OPTIONS} \
    --werror \
    -Ddocumentation=true \
    -Dintrospection=enabled \
    -Dman-pages=enabled \
    _build

meson compile -C _build

if [[ "$CFLAGS" == *"-coverage"* ]]; then
    lcov \
        --quiet \
        --config-file "${DIR}"/.lcovrc \
        --directory "${DIR}/_build" \
        --capture \
        --initial \
        --output-file "${DIR}/_coverage/${CI_JOB_NAME}-baseline.lcov"
fi

# FIXME: Workaround for https://github.com/mesonbuild/meson/issues/12330
for tests in \
    gio/tests \
    girepository/tests \
    glib/tests \
    gmodule/tests \
    gobject/tests \
    gthread/tests \
; do
    cp _build/glib/libglib-2.0-0.dll "_build/$tests/"
    cp _build/gmodule/libgmodule-2.0-0.dll "_build/$tests/"
    cp _build/gobject/libgobject-2.0-0.dll "_build/$tests/"
    cp _build/gthread/libgthread-2.0-0.dll "_build/$tests/"
    cp _build/gio/libgio-2.0-0.dll "_build/$tests/"
    cp _build/girepository/libgirepository-2.0-0.dll "_build/$tests/"
done

meson test -C _build -v --timeout-multiplier "${MESON_TEST_TIMEOUT_MULTIPLIER}"
meson test -C _build -v --timeout-multiplier "${MESON_TEST_TIMEOUT_MULTIPLIER}" \
    --setup=unstable_tests --suite=failing --suite=flaky || true

if [[ "$CFLAGS" == *"-coverage"* ]]; then
    lcov \
        --quiet \
        --config-file "${DIR}"/.lcovrc \
        --directory "${DIR}/_build" \
        --capture \
        --output-file "${DIR}/_coverage/${CI_JOB_NAME}.lcov"
fi

# Copy the built documentation to an artifact directory. The build for docs.gtk.org
# can then pull it from there — see https://gitlab.gnome.org/GNOME/gtk/-/blob/docs-gtk-org/README.md
mkdir -p _reference/
mv _build/docs/reference/glib/glib-win32/ _reference/glib-win32/
mv _build/docs/reference/gio/gio-win32/ _reference/gio-win32/
