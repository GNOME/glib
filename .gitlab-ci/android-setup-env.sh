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

arch=$1
api=$2
toolchain_path=$(pwd)/android-toolchain-$arch-$api
prefix_path=$(pwd)/android-$arch-$api

# Create standalone toolchains
$ANDROID_NDK_PATH/build/tools/make_standalone_toolchain.py --arch $arch --api $api --install-dir $toolchain_path

target_host=aarch64-linux-android
export AR=$target_host-ar
export AS=$target_host-clang
export CC=$target_host-clang
export CXX=$target_host-clang++
export LD=$target_host-ld
export STRIP=$target_host-strip
export PATH=$PATH:$toolchain_path/bin

# Cross build libiconv when using API level <= 28.
# Newer Android has it in its libc already.
if [ "$api" -lt "28" ]; then
  wget --quiet http://ftp.gnu.org/pub/gnu/libiconv/libiconv-1.15.tar.gz
  echo "1233fe3ca09341b53354fd4bfe342a7589181145a1232c9919583a8c9979636855839049f3406f253a9d9829908816bb71fd6d34dd544ba290d6f04251376b1a  libiconv-1.15.tar.gz" | sha512sum -c
  tar xzf libiconv-1.15.tar.gz
  pushd libiconv-1.15
  ./configure --host=$target_host --prefix=$prefix_path  --libdir=$prefix_path/lib64
  make
  make install
  popd
  rm libiconv-1.15.tar.gz
  rm -r libiconv-1.15
fi

# Cross build libffi
wget --quiet https://github.com/libffi/libffi/releases/download/v3.3-rc0/libffi-3.3-rc0.tar.gz
echo "e6e695d32cd6eb7d65983f32986fccdfc786a593d2ea18af30ce741f58cfa1eb264b1a8d09df5084cb916001aea15187b005c2149a0620a44397a4453b6137d4  libffi-3.3-rc0.tar.gz" | sha512sum -c
tar xzf libffi-3.3-rc0.tar.gz
pushd libffi-3.3-rc0
./configure --host=$target_host --prefix=$prefix_path --libdir=$prefix_path/lib64
make
make install
popd
rm libffi-3.3-rc0.tar.gz
rm -r libffi-3.3-rc0

# Create a pkg-config wrapper that won't pick fedora libraries
mkdir -p $prefix_path/bin
export PKG_CONFIG=$prefix_path/bin/pkg-config
cat > $PKG_CONFIG <<- EOM
#!/bin/sh
SYSROOT=${prefix_path}
export PKG_CONFIG_DIR=
export PKG_CONFIG_LIBDIR=\${SYSROOT}/lib64/pkgconfig
export PKG_CONFIG_SYSROOT_DIR=\${SYSROOT}
exec pkg-config "\$@"
EOM
chmod +x $PKG_CONFIG

# Create a cross file that can be passed to meson
cat > cross_file_android_${arch}_${api}.txt <<- EOM
[host_machine]
system = 'android'
cpu_family = 'aarch64'
cpu = 'aarch64'
endian = 'little'

[properties]
c_args = ['-I${prefix_path}/include']
c_link_args = ['-L${prefix_path}/lib64',
               '-fuse-ld=gold']

[binaries]
c = '${toolchain_path}/bin/${CC}'
cpp = '${toolchain_path}/bin/${CXX}'
ar = '${toolchain_path}/bin/${AR}'
ld = '${toolchain_path}/bin/${LD}'
strip = '${toolchain_path}/bin/${STRIP}'
pkgconfig = '${PKG_CONFIG}'
EOM
