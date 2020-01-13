FROM debian:buster

RUN apt-get update -qq && apt-get install --no-install-recommends -qq -y \
    bindfs \
    clang \
    clang-tools-7 \
    clang-format-7 \
    dbus \
    desktop-file-utils \
    elfutils \
    findutils \
    fuse \
    gcc \
    g++ \
    gettext \
    git \
    libc6-dev \
    gtk-doc-tools \
    itstool \
    lcov \
    libattr1-dev \
    libdbus-1-dev \
    libelf-dev \
    libffi-dev \
    libgamin-dev \
    libmount-dev \
    libpcre2-dev \
    libselinux1-dev \
    libxml2-utils \
    libxslt1-dev \
    libz3-dev \
    locales \
    ninja-build \
    python3 \
    python3-pip \
    python3-setuptools \
    python3-wheel \
    shared-mime-info \
    systemtap-sdt-dev \
    unzip \
    wget \
    xsltproc \
    xz-utils \
    zlib1g-dev \
 && rm -rf /usr/share/doc/* /usr/share/man/*

# Locale for our build
RUN locale-gen C.UTF-8 && /usr/sbin/update-locale LANG=C.UTF-8

# Locales for our tests
RUN locale-gen de_DE.UTF-8 \
 && locale-gen el_GR.UTF-8 \
 && locale-gen en_US.UTF-8 \
 && locale-gen es_ES.UTF-8 \
 && locale-gen fa_IR.UTF-8 \
 && locale-gen fr_FR.UTF-8 \
 && locale-gen hr_HR.UTF-8 \
 && locale-gen ja_JP.UTF-8 \
 && locale-gen lt_LT.UTF-8 \
 && locale-gen pl_PL.UTF-8 \
 && locale-gen ru_RU.UTF-8 \
 && locale-gen tr_TR.UTF-8

ENV LANG=C.UTF-8 LANGUAGE=C.UTF-8 LC_ALL=C.UTF-8

RUN pip3 install meson==0.49.2

ARG HOST_USER_ID=5555
ENV HOST_USER_ID ${HOST_USER_ID}
RUN useradd -u $HOST_USER_ID -ms /bin/bash user

USER user
WORKDIR /home/user

COPY cache-subprojects.sh .
RUN ./cache-subprojects.sh

ENV LANG=C.UTF-8 LANGUAGE=C.UTF-8 LC_ALL=C.UTF-8
