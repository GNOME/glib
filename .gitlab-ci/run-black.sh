#!/bin/bash

set -e

# shellcheck disable=SC2046
black --diff --check $(git ls-files '*.py')
