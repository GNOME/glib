FROM fedora:34

RUN dnf -y install \
    autoconf \
    automake \
    bindfs \
    clang \
    clang-analyzer \
    desktop-file-utils \
    elfutils-libelf-devel \
    findutils \
    fuse \
    gcc \
    gcc-c++ \
    gettext \
    git \
    glibc-devel \
    glibc-headers \
    glibc-langpack-de \
    glibc-langpack-el \
    glibc-langpack-el \
    glibc-langpack-en \
    glibc-langpack-es \
    glibc-langpack-es \
    glibc-langpack-fa \
    glibc-langpack-fr \
    glibc-langpack-hr \
    glibc-langpack-ja \
    glibc-langpack-lt \
    glibc-langpack-pl \
    glibc-langpack-ru \
    glibc-langpack-tr \
    gtk-doc \
    itstool \
    lcov \
    libattr-devel \
    libffi-devel \
    libmount-devel \
    libselinux-devel \
    libtool \
    libxslt \
    make \
    ncurses-compat-libs \
    ninja-build \
    pcre-devel \
    python-unversioned-command \
    python3 \
    python3-pip \
    python3-wheel \
    systemtap-sdt-devel \
    unzip \
    wget \
    xz \
    zlib-devel \
 && dnf clean all

WORKDIR /opt
ENV ANDROID_NDK_PATH /opt/android-ndk
COPY android-download-ndk.sh .
RUN ./android-download-ndk.sh
COPY android-setup-env.sh .
RUN ./android-setup-env.sh arm64 28
# Explicitly remove some directories first to fix symlink traversal problems
RUN rm -rf \
  $ANDROID_NDK_PATH/sources/third_party/vulkan/src/tests/layers \
  $ANDROID_NDK_PATH/sources/cxx-stl/llvm-libc++/test/std/containers/unord/unord.multimap/unord.multimap.modifiers \
  $ANDROID_NDK_PATH/sources/cxx-stl/llvm-libc++/test/std/containers/unord/unord.multiset/unord.multiset.cnstr \
  $ANDROID_NDK_PATH/sources/cxx-stl/llvm-libc++/test/std/iterators/predef.iterators/reverse.iterators/reverse.iter.ops/reverse.iter.opsum \
  $ANDROID_NDK_PATH/sources/cxx-stl/llvm-libc++/test/std/experimental/filesystem/fs.op.funcs/fs.op.create_directory_symlink \
  $ANDROID_NDK_PATH/sources/cxx-stl/llvm-libc++/test/std/experimental/filesystem/fs.op.funcs/fs.op.is_directory \
  $ANDROID_NDK_PATH/sources/cxx-stl/llvm-libc++/test/std/experimental/filesystem/fs.op.funcs/fs.op.create_hard_link \
  $ANDROID_NDK_PATH/sources/cxx-stl/llvm-libc++/test/std/experimental/filesystem/fs.op.funcs/fs.op.create_directory \
  $ANDROID_NDK_PATH

RUN pip3 install meson==0.52.0

ARG HOST_USER_ID=5555
ENV HOST_USER_ID ${HOST_USER_ID}
RUN useradd -u $HOST_USER_ID -ms /bin/bash user

USER user
WORKDIR /home/user

COPY cache-subprojects.sh .
RUN ./cache-subprojects.sh

ENV LANG C.UTF-8
