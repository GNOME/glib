ARG ARCHITECTURE_PREFIX=
FROM ${ARCHITECTURE_PREFIX}debian:bookworm

RUN apt-get update -qq && apt-get install --no-install-recommends -qq -y \
    bindfs \
    black \
    clang \
    clang-tools \
    clang-format \
    dbus \
    desktop-file-utils \
    elfutils \
    findutils \
    flake8 \
    fuse \
    gcc \
    gdb \
    g++ \
    gettext \
    gi-docgen \
    git \
    libc6-dev \
    gobject-introspection \
    gtk-doc-tools \
    itstool \
    lcov \
    libattr1-dev \
    libdbus-1-dev \
    libelf-dev \
    libffi-dev \
    libgirepository1.0-dev \
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
    reuse \
    shared-mime-info \
    shellcheck \
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
 && locale-gen th_TH.UTF-8 \
 && locale-gen tr_TR.UTF-8

ENV LANG=C.UTF-8 LANGUAGE=C.UTF-8 LC_ALL=C.UTF-8

RUN pip3 install --break-system-packages meson==1.4.2

# ninja-build 1.11.1 didn't build with large file support on 32-bit,
# breaking the i386 image when used with overlayfs.
# The fix from upstream 1.12.0 was backported to Debian in 1.11.1-2,
# but too late for Debian 12. https://bugs.debian.org/1041897
RUN if [ "$(dpkg --print-architecture)" = i386 ]; then \
    apt-get install --no-install-recommends -qq -y \
        debhelper \
        re2c \
    && mkdir /run/build \
    && git clone --depth=1 -b debian/1.11.1-2 https://salsa.debian.org/debian/ninja-build.git /run/build/ninja-build \
    && cd /run/build/ninja-build \
    && git checkout e39b5f01229311916302300449d951735e4a3e3f \
    && dpkg-buildpackage -B -Pnodoc \
    && dpkg -i ../*.deb \
    && cd / \
    && rm -fr /run/build; \
fi

ARG HOST_USER_ID=5555
ENV HOST_USER_ID ${HOST_USER_ID}
RUN useradd -u $HOST_USER_ID -ms /bin/bash user

USER user
WORKDIR /home/user

COPY cache-subprojects.sh .
RUN ./cache-subprojects.sh

ENV LANG=C.UTF-8 LANGUAGE=C.UTF-8 LC_ALL=C.UTF-8
