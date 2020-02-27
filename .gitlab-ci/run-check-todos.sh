#!/bin/bash

set -e

# We need to add a new remote for the upstream master, since this script could
# be running in a personal fork of the repository which has out of date branches.
git remote add upstream https://gitlab.gnome.org/GNOME/glib.git
git fetch upstream

# Work out the newest common ancestor between the detached HEAD that this CI job
# has checked out, and the upstream target branch (which will typically be
# `upstream/master` or `upstream/glib-2-62`).
# `${CI_MERGE_REQUEST_TARGET_BRANCH_NAME}` is only defined if we’re running in
# a merge request pipeline; fall back to `${CI_DEFAULT_BRANCH}` otherwise.
newest_common_ancestor_sha=$(diff --old-line-format='' --new-line-format='' <(git rev-list --first-parent "upstream/${CI_MERGE_REQUEST_TARGET_BRANCH_NAME:-${CI_DEFAULT_BRANCH}}") <(git rev-list --first-parent HEAD) | head -1)
./.gitlab-ci/check-todos.py "${newest_common_ancestor_sha}"
