#!/bin/bash

set -eux -o pipefail

id || :
capsh --print || :
env -0 | sort -z | perl -pe 's/\0/\n/g' || :
setpriv --dump || :
ulimit -a || :
cat /proc/self/status || :
cat /proc/self/mountinfo || :


wget -t 1 http://localhost/metrics || true
wget -t 1 https://localhost/metrics || true

cat /proc/loadavg
nproc
ip -s link ls
