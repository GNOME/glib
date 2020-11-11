#!/bin/bash

set -e

# shellcheck disable=SC2046
flake8 --max-line-length=88 $(git ls-files '*.py')
