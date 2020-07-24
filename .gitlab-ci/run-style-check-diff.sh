#!/bin/bash

set -e

ancestor_horizon=28  # days (4 weeks)

# Wrap everything in a subshell so we can propagate the exit status.
(

# We need to add a new remote for the upstream target branch, since this script
# could be running in a personal fork of the repository which has out of date
# branches.
#
# Limit the fetch to a certain date horizon to limit the amount of data we get.
# If the branch was forked from origin/master before this horizon, it should
# probably be rebased.
git remote add upstream https://gitlab.gnome.org/GNOME/glib.git
git fetch --shallow-since="$(date --date="${ancestor_horizon} days ago" +%Y-%m-%d)" upstream

# Work out the newest common ancestor between the detached HEAD that this CI job
# has checked out, and the upstream target branch (which will typically be
# `upstream/master` or `upstream/glib-2-62`).
#
# `${CI_MERGE_REQUEST_TARGET_BRANCH_NAME}` is only defined if we’re running in
# a merge request pipeline; fall back to `${CI_DEFAULT_BRANCH}` otherwise.
newest_common_ancestor_sha=$(diff --old-line-format='' --new-line-format='' <(git rev-list --first-parent "upstream/${CI_MERGE_REQUEST_TARGET_BRANCH_NAME:-${CI_DEFAULT_BRANCH}}") <(git rev-list --first-parent HEAD) | head -1)
if [ -z "${newest_common_ancestor_sha}" ]; then
    echo "Couldn’t find common ancestor with upstream master. This typically"
    echo "happens if you branched from master a long time ago. Please update"
    echo "your clone, rebase, and re-push your branch."
    exit 1
fi

git diff -U0 --no-color "${newest_common_ancestor_sha}" | ./clang-format-diff.py -binary "clang-format-7" -p1

)
exit_status=$?

# The style check is not infallible. The clang-format configuration cannot
# perfectly describe GLib’s coding style: in particular, it cannot align
# function arguments. The documented coding style for GLib takes priority over
# clang-format suggestions. Hopefully we can eventually improve clang-format to
# be configurable enough for our coding style. That’s why this CI check is OK
# to fail: the idea is that people can look through the output and ignore it if
# it’s wrong. (That situation can also happen if someone touches pre-existing
# badly formatted code and it doesn’t make sense to tidy up the wider coding
# style with the changes they’re making.)
echo ""
echo "Note that clang-format output is advisory and cannot always match the GLib coding style, documented at"
echo "   https://gitlab.gnome.org/GNOME/gtk/blob/master/docs/CODING-STYLE"
echo "Warnings from this tool can be ignored in favour of the documented coding style,"
echo "or in favour of matching the style of existing surrounding code."

exit ${exit_status}
