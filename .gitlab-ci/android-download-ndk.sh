#!/bin/bash

#
# Copyright 2018 Collabora ltd.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, see <http://www.gnu.org/licenses/>.
#
# Author: Xavier Claessens <xavier.claessens@collabora.com>
#

set -e

# Download Android NDK
ANDROID_NDK_VERSION="r17b"
ANDROID_NDK_SHA512="062fac12f747730f5563995089a8b4abab683fbbc621aa8582fdf35fe327daee5d69ed2437af257c10ec4ef54ecd3805a8f134a1400eb8f34ee76f55c8dc9ae9"
wget --quiet "https://dl.google.com/android/repository/android-ndk-${ANDROID_NDK_VERSION}-linux-x86_64.zip"
echo "${ANDROID_NDK_SHA512}  android-ndk-${ANDROID_NDK_VERSION}-linux-x86_64.zip" | sha512sum -c
unzip "android-ndk-${ANDROID_NDK_VERSION}-linux-x86_64.zip"
rm "android-ndk-${ANDROID_NDK_VERSION}-linux-x86_64.zip"
mv "android-ndk-${ANDROID_NDK_VERSION}" "${ANDROID_NDK_PATH}"
