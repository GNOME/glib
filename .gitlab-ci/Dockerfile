FROM fedora:27

RUN dnf -y install \
    desktop-file-utils \
    elfutils-libelf-devel \
    gcc \
    gcc-c++ \
    gettext \
    git \
    glibc-devel \
    glibc-headers \
    gtk-doc \
    itstool \
    libattr-devel \
    libffi-devel \
    libmount-devel \
    libselinux-devel \
    libxslt \
    ninja-build \
    pcre-devel \
    python3 \
    python3-pip \
    python3-wheel \
    systemtap-sdt-devel \
    zlib-devel \
 && dnf clean all

RUN pip3 install meson

ARG HOST_USER_ID=5555
ENV HOST_USER_ID ${HOST_USER_ID}
RUN useradd -u $HOST_USER_ID -ms /bin/bash user

USER user
WORKDIR /home/user

ENV LANG C.UTF-8
