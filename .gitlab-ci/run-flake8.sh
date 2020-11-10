#!/bin/bash

set -e

flake8 --max-line-length=88 $(git ls-files '*.py')
