# Copyright 2024 Collabora Ltd.
# SPDX-License-Identifier: LGPL-2.1-or-later

lint_scripts = [
  'black.sh',
  'check-missing-install-tag.py',
  'flake8.sh',
  'reuse.sh',
  'shellcheck.sh',
]

foreach test_name : lint_scripts
  test(
    test_name, files(test_name),
    env : common_test_env,
    suite : 'lint',
    protocol : 'tap',
  )
endforeach
