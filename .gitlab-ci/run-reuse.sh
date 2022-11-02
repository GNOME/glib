#!/bin/bash
#
# Copyright 2022 Endless OS Foundation, LLC
#
# SPDX-License-Identifier: LGPL-2.1-or-later
#
# Original author: Philip Withnall

set -e

# We need to make sure the submodules are up to date, or `reuse lint` will fail
# when it tries to run `git status` internally
git submodule update --init

# Run `reuse lint` on the code base and see if the number of files without
# suitable copyright/licensing information has increased from a baseline
# FIXME: Eventually this script can check whether *any* files are missing
# information. But for now, let’s slowly improve the baseline.
files_without_copyright_information_max=407
files_without_license_information_max=559

# The || true is because `reuse lint` will exit with status 1 if the project is not compliant
# FIXME: Once https://github.com/fsfe/reuse-tool/issues/512 or
# https://github.com/fsfe/reuse-tool/issues/183 land, we can check only files
# which have changed in this merge request, and confidently parse structured
# output rather than the current human-readable output.
lint_output="$(reuse lint || true)"

files_with_copyright_information="$(echo "${lint_output}" | awk '/^\* Files with copyright information: / { print $6 }')"
files_with_license_information="$(echo "${lint_output}" | awk '/^\* Files with license information: / { print $6 }')"
total_files="$(echo "${lint_output}" | awk '/^\* Files with copyright information: / { print $8 }')"
error=0

files_without_copyright_information="$(( total_files - files_with_copyright_information ))"
files_without_license_information="$(( total_files - files_with_license_information ))"

if [ "${files_without_copyright_information}" -gt "${files_without_copyright_information_max}" ] || \
   [ "${files_without_license_information}" -gt "${files_without_license_information_max}" ]; then
  echo "${lint_output}"
fi

if [ "${files_without_copyright_information}" -gt "${files_without_copyright_information_max}" ]; then
  echo ""
  echo "Error: New files added without REUSE-compliant copyright information"
  echo "Please make sure that all files added in this branch/merge request have correct copyright information"
  error=1
fi

if [ "${files_without_license_information}" -gt "${files_without_license_information_max}" ]; then
  echo ""
  echo "Error: New files added without REUSE-compliant licensing information"
  echo "Please make sure that all files added in this branch/merge request have correct license information"
  error=1
fi

if [ "${error}" -eq "1" ]; then
  echo ""
  echo "See https://reuse.software/tutorial/#step-2 for information on how to add REUSE information"
  echo "Also see https://gitlab.gnome.org/GNOME/glib/-/issues/1415"
fi

exit "${error}"