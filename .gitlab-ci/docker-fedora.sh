#! /bin/bash

set -ex

COVERITY_SCAN_PROJECT_NAME=$1
COVERITY_SCAN_TOKEN=$2

pip3 install meson==0.56.2

# Set /etc/machine-id as it’s needed for some D-Bus tests
systemd-machine-id-setup

.gitlab-ci/cache-subprojects.sh

# Download Android NDK
ANDROID_NDK_PATH=/opt/android-ndk
ANDROID_NDK_VERSION="r23b"
ANDROID_NDK_SHA1="f47ec4c4badd11e9f593a8450180884a927c330d"
wget --quiet "https://dl.google.com/android/repository/android-ndk-${ANDROID_NDK_VERSION}-linux.zip"
echo "${ANDROID_NDK_SHA1}  android-ndk-${ANDROID_NDK_VERSION}-linux.zip" | sha1sum -c
unzip "android-ndk-${ANDROID_NDK_VERSION}-linux.zip"
rm "android-ndk-${ANDROID_NDK_VERSION}-linux.zip"
mv "android-ndk-${ANDROID_NDK_VERSION}" "${ANDROID_NDK_PATH}"

# Download Coverity analysis
curl https://scan.coverity.com/download/cxx/linux64 \
    -o /tmp/cov-analysis-linux64.tgz \
    --form project="${COVERITY_SCAN_PROJECT_NAME}" \
    --form token="${COVERITY_SCAN_TOKEN}"
tar xfz /tmp/cov-analysis-linux64.tgz -C /opt
rm /tmp/cov-analysis-linux64.tgz
