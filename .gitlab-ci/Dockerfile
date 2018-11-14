FROM fedora:27

RUN dnf -y install \
    autoconf \
    automake \
    bindfs \
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
    mingw64-gcc \
    mingw64-gcc-c++ \
    mingw64-gettext \
    mingw64-libffi \
    mingw64-zlib \
    ncurses-compat-libs \
    ninja-build \
    pcre-devel \
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

COPY cross_file_mingw64.txt /opt

RUN pip3 install meson==0.48.0

ARG HOST_USER_ID=5555
ENV HOST_USER_ID ${HOST_USER_ID}
RUN useradd -u $HOST_USER_ID -ms /bin/bash user

USER user
WORKDIR /home/user

ENV LANG C.UTF-8
