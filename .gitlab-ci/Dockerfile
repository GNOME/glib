FROM fedora:27

RUN dnf -y install \
    desktop-file-utils \
    elfutils-libelf-devel \
    findutils \
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
    libxslt \
    make \
    mingw64-gcc \
    mingw64-gcc-c++ \
    mingw64-gettext \
    mingw64-libffi \
    mingw64-zlib \
    ninja-build \
    pcre-devel \
    python3 \
    python3-pip \
    python3-wheel \
    systemtap-sdt-devel \
    unzip \
    wget \
    zlib-devel \
 && dnf clean all

COPY setup-android-ndk.sh .
RUN ./setup-android-ndk.sh

COPY cross_file_mingw64.txt /opt

RUN pip3 install meson==0.46.1

ARG HOST_USER_ID=5555
ENV HOST_USER_ID ${HOST_USER_ID}
RUN useradd -u $HOST_USER_ID -ms /bin/bash user

USER user
WORKDIR /home/user

ENV LANG C.UTF-8
