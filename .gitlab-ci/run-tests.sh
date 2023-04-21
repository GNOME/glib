#!/bin/bash

set -ex

./.gitlab-ci/check-missing-install-tag.py _build

git fetch origin main
git fetch --tags
git bisect start
git bisect good 2.76.1
git bisect bad origin/main

cat <<"EOF" > /tmp/bisect.sh
# git show d296e9455976f1bcf62a83750606befff72aa06e | patch -f -p1 || true
echo "int main(void) { return 77; }" > gio/tests/appmonitor.c
meson test -v \
        -C _build \
        --suite=core
ret=$?
#git diff | patch -Rp1 || true
git stash

exit $ret
EOF

chmod +x /tmp/bisect.sh
git bisect run bash -x /tmp/bisect.sh
