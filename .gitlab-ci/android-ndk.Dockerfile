FROM fedora:33

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
    gamin-devel \
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
RUN ./android-setup-env.sh arm64 21
RUN ./android-setup-env.sh arm64 28
RUN rm -rf $ANDROID_NDK_PATH

RUN pip3 install meson==0.52.0

ARG HOST_USER_ID=5555
ENV HOST_USER_ID ${HOST_USER_ID}
RUN useradd -u $HOST_USER_ID -ms /bin/bash user

USER user
WORKDIR /home/user

COPY cache-subprojects.sh .
RUN ./cache-subprojects.sh

ENV LANG C.UTF-8
