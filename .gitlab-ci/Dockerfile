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
    meson \
    pcre-devel \
    systemtap-sdt-devel \
    zlib-devel

ARG HOST_USER_ID=5555
ENV HOST_USER_ID ${HOST_USER_ID}
RUN useradd -u $HOST_USER_ID -ms /bin/bash user

USER user
WORKDIR /home/user

ENV LANG C.utf8
