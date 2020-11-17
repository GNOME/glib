#!/bin/bash

set -e

# Disable formatting warnings in flake8, as we use `black` to handle that.
formatting_warnings=E101,E111,E114,E115,E116,E117,E12,E13,E2,E3,E401,E5,E70,W1,W2,W3,W5

# shellcheck disable=SC2046
flake8 --max-line-length=88 --ignore="$formatting_warnings" $(git ls-files '*.py')
