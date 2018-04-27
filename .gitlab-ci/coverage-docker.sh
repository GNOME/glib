#!/bin/bash

set -e

# Fixup Windows paths
python3 ./.gitlab-ci/fixup-cov-paths.py _coverage/*.lcov

# Remove coverage from generated code in the build directory
for path in _coverage/*.lcov; do
    lcov --rc lcov_branch_coverage=1 -r "${path}" '*/_build/*' -o "$(pwd)/${path}"
done

genhtml \
    --ignore-errors=source \
    --rc lcov_branch_coverage=1 \
    _coverage/*.lcov \
    -o _coverage/coverage

cd _coverage
rm -f *.lcov

cat >index.html <<EOL
<html>
<body>
<ul>
<li><a href="coverage/index.html">Coverage</a></li>
</ul>
</body>
</html>
EOL
