#! /bin/bash

set -e

# Locale for our build
locale-gen C.UTF-8
/usr/sbin/update-locale LANG=C.UTF-8

# Locales for our tests
locale-gen de_DE.UTF-8
locale-gen el_GR.UTF-8
locale-gen en_US.UTF-8
locale-gen es_ES.UTF-8
locale-gen fa_IR.UTF-8
locale-gen fr_FR.UTF-8
locale-gen hr_HR.UTF-8
locale-gen ja_JP.UTF-8
locale-gen lt_LT.UTF-8
locale-gen pl_PL.UTF-8
locale-gen ru_RU.UTF-8
locale-gen tr_TR.UTF-8

pip3 install meson==0.56.2

.gitlab-ci/cache-subprojects.sh