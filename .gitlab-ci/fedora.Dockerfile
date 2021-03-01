FROM fedora:31

# Set /etc/machine-id as it’s needed for some D-Bus tests
RUN systemd-machine-id-setup

RUN dnf -y update \
 && dnf -y install \
    bindfs \
    clang \
    clang-analyzer \
    dbus-daemon \
    dbus-devel \
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
    "gnome-desktop-testing >= 2018.1" \
    gtk-doc \
    itstool \
    lcov \
    libattr-devel \
    libffi-devel \
    libmount-devel \
    libselinux-devel \
    libxslt \
    ncurses-compat-libs \
    ninja-build \
    pcre-devel \
    "python3-dbusmock >= 0.18.3-2" \
    python3-pip \
    python3-pygments \
    python3-wheel \
    shared-mime-info \
    systemtap-sdt-devel \
    unzip \
    valgrind \
    wget \
    xdg-desktop-portal \
    xz \
    zlib-devel \
 && dnf -y install \
    meson \
    flex \
    bison \
    python3-devel \
    autoconf \
    automake \
    gettext-devel \
    libtool \
    diffutils \
    fontconfig-devel \
    json-glib-devel \
    geoclue2-devel \
    pipewire-devel \
    fuse-devel \
    make \
 && dnf clean all

RUN pip3 install meson==0.52.1

# Enable sudo for wheel users
RUN sed -i -e 's/# %wheel/%wheel/' -e '0,/%wheel/{s/%wheel/# %wheel/}' /etc/sudoers

ARG HOST_USER_ID=5555
ENV HOST_USER_ID ${HOST_USER_ID}
RUN useradd -u $HOST_USER_ID -G wheel -ms /bin/bash user

USER user
WORKDIR /home/user

COPY cache-subprojects.sh .
RUN ./cache-subprojects.sh

ENV LANG C.UTF-8
