FROM fedora:29

RUN dnf -y install \
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
    libxslt \
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
COPY cross_file_mingw64.txt /opt

RUN pip3 install meson==0.49.2

ARG HOST_USER_ID=5555
ENV HOST_USER_ID ${HOST_USER_ID}
RUN useradd -u $HOST_USER_ID -ms /bin/bash user

USER user
WORKDIR /home/user

COPY cache-subprojects.sh .
RUN ./cache-subprojects.sh

ENV LANG C.UTF-8
