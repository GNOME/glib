#!/bin/bash

set -e

# Ignoring third-party directories that we don't want to parse
# shellcheck disable=SC2046
shellcheck $(git ls-files '*.sh' | grep -Ev "glib/libcharset|glib/dirent")
