#!/usr/bin/env python3
#
# Copyright © 2019 Endless Mobile, Inc.
#
# SPDX-License-Identifier: LGPL-2.1-or-later
#
# Original author: Philip Withnall

"""
Checks that none of the commits in a merge request add any instances of the
string ‘todo’ (in uppercase). They may remove instances of it, or move them
around, according to the logic of `git log -S`.
"""

import os
import subprocess
import sys


# We have to specify this keyword obscurely to avoid the script matching
# itself.
BANNED_KEYWORD = 'TO' + 'DO'


def main():
    ci_merge_request_target_branch_name = \
        os.environ.get('CI_MERGE_REQUEST_TARGET_BRANCH_NAME')
    ci_merge_request_project_url = \
        os.environ.get('CI_MERGE_REQUEST_PROJECT_URL')
    ci_commit_sha = os.environ.get('CI_COMMIT_SHA')

    if not ci_merge_request_target_branch_name:
        print('Cannot triage non-merge request; trivially passed')
        sys.exit(0)

    subprocess.run([
        'git', 'fetch',
        ci_merge_request_project_url + '.git',
        ci_merge_request_target_branch_name,
    ], check=True)

    branch_point_process = subprocess.run(
        ['git', 'merge-base', 'HEAD', 'FETCH_HEAD'],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, encoding='utf-8',
        check=True)
    branch_point = branch_point_process.stdout.strip()

    commits_process = subprocess.run(
        ['git', 'log', '-S', BANNED_KEYWORD, '--format=format:%H',
         '{}..{}'.format(branch_point, ci_commit_sha)],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, encoding='utf-8',
        check=True)
    commits = commits_process.stdout.strip()

    # If there are no commits which changed the number of matching comments
    # then the check is trivially passed.
    if not commits:
        sys.exit(0)

    exit_status = 0
    for commit_sha in commits.split('\n'):
        if commit_diff_added_todo(commit_sha):
            print('Added a {} comment in commit {}'.format(
                BANNED_KEYWORD, commit_sha))
            exit_status = 1

    sys.exit(exit_status)


def commit_diff_added_todo(commit_sha):
    """
    Check whether the commit given by `commit_sha` added any banned strings in
    its added lines.
    """
    commit_diff_process = subprocess.run([
        'git', 'show', '--oneline', '-U0', commit_sha],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, encoding='utf-8',
        check=True)
    commit_diff_lines = commit_diff_process.stdout.strip().split('\n')

    for line in commit_diff_lines:
        if line.startswith('+ ') and BANNED_KEYWORD in line:
            return True

    return False

if __name__ == '__main__':
    main()
