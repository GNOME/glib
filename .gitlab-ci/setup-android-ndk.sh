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

cd /opt

# Download Android NDK
ANDROID_NDK_VERSION="r17b"
ANDROID_NDK_SHA512="062fac12f747730f5563995089a8b4abab683fbbc621aa8582fdf35fe327daee5d69ed2437af257c10ec4ef54ecd3805a8f134a1400eb8f34ee76f55c8dc9ae9"
wget --quiet https://dl.google.com/android/repository/android-ndk-$ANDROID_NDK_VERSION-linux-x86_64.zip
echo "$ANDROID_NDK_SHA512  android-ndk-$ANDROID_NDK_VERSION-linux-x86_64.zip" | sha512sum -c
unzip android-ndk-$ANDROID_NDK_VERSION-linux-x86_64.zip
rm android-ndk-$ANDROID_NDK_VERSION-linux-x86_64.zip

# Setup cross build env
export ANDROID_HOST=aarch64-linux-android
export ANDROID_BUILD=linux-x86_64
export ANDROID_ARCH=arm64
export ANDROID_NDK=/opt/android-ndk-$ANDROID_NDK_VERSION
export ANDROID_VERSION=21
export ANDROID_TOOLCHAIN_VERSION=4.9
export ANDROID_SYSROOT=$ANDROID_NDK/platforms/android-$ANDROID_VERSION/arch-$ANDROID_ARCH
export ANDROID_PREBUILT=$ANDROID_NDK/toolchains/$ANDROID_HOST-$ANDROID_TOOLCHAIN_VERSION/prebuilt/$ANDROID_BUILD/bin
export CPPFLAGS="--sysroot=$ANDROID_SYSROOT -isystem $ANDROID_NDK/sysroot/usr/include/  -isystem $ANDROID_NDK/sysroot/usr/include/$ANDROID_HOST"
export CFLAGS="$CPPFLAGS -D__ANDROID_API__=$ANDROID_VERSION"
export AR=$ANDROID_HOST-ar
export RANLIB=$ANDROID_HOST-ranlib
export CPP=$ANDROID_HOST-cpp
export PATH=$ANDROID_PREBUILT:$PATH

# Cross build libiconv
wget --quiet http://ftp.gnu.org/pub/gnu/libiconv/libiconv-1.15.tar.gz
echo "1233fe3ca09341b53354fd4bfe342a7589181145a1232c9919583a8c9979636855839049f3406f253a9d9829908816bb71fd6d34dd544ba290d6f04251376b1a  libiconv-1.15.tar.gz" | sha512sum -c
tar xzf libiconv-1.15.tar.gz
rm libiconv-1.15.tar.gz
pushd libiconv-1.15
./configure --host=$ANDROID_HOST --prefix=/opt/$ANDROID_HOST  --libdir=/opt/$ANDROID_HOST/lib64
make
make install
popd

# Cross build libffi
wget --quiet https://github.com/libffi/libffi/releases/download/v3.3-rc0/libffi-3.3-rc0.tar.gz
echo "e6e695d32cd6eb7d65983f32986fccdfc786a593d2ea18af30ce741f58cfa1eb264b1a8d09df5084cb916001aea15187b005c2149a0620a44397a4453b6137d4  libffi-3.3-rc0.tar.gz" | sha512sum -c
tar xzf libffi-3.3-rc0.tar.gz
rm libffi-3.3-rc0.tar.gz
pushd libffi-3.3-rc0
./configure --host=$ANDROID_HOST --prefix=/opt/$ANDROID_HOST --libdir=/opt/$ANDROID_HOST/lib64
make
make install
popd

# Create a pkg-config wrapper that won't pick fedora libraries
export PKG_CONFIG=/opt/${ANDROID_HOST}/bin/pkg-config
cat > $PKG_CONFIG <<- EOM
#!/bin/sh
SYSROOT=/opt/${ANDROID_HOST}
export PKG_CONFIG_DIR=
export PKG_CONFIG_LIBDIR=\${SYSROOT}/lib64/pkgconfig
export PKG_CONFIG_SYSROOT_DIR=\${SYSROOT}
exec pkg-config "\$@"
EOM
chmod +x $PKG_CONFIG

# Create a cross file that can be passed to meson
cat > /opt/cross_file_android_api21_arm64.txt <<- EOM
[host_machine]
system = 'android'
cpu_family = 'arm64'
cpu = 'arm64'
endian = 'little'

[properties]
c_args = ['--sysroot=${ANDROID_SYSROOT}',
          '-isystem', '/opt/${ANDROID_HOST}/include',
          '-isystem', '${ANDROID_NDK}/sysroot/usr/include/',
          '-isystem', '${ANDROID_NDK}/sysroot/usr/include/${ANDROID_HOST}',
          '-D__ANDROID_API__=${ANDROID_VERSION}']
c_link_args = ['--sysroot=${ANDROID_SYSROOT}',
               '-L/opt/${ANDROID_HOST}/lib64',
               '-fuse-ld=gold']

[binaries]
c = '${ANDROID_PREBUILT}/${ANDROID_HOST}-gcc'
cpp = '${ANDROID_PREBUILT}/${ANDROID_HOST}-g++'
ar = '${ANDROID_PREBUILT}/${ANDROID_HOST}-ar'
strip = '${ANDROID_PREBUILT}/${ANDROID_HOST}-strip'
pkgconfig = '${PKG_CONFIG}'
EOM
