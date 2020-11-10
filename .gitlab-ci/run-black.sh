#!/bin/bash

set -e

black --diff --check $(git ls-files '*.py')
