# Note that this is NOT a relocatable package
%define ver      1.0.4
%define rel      SNAP
%define prefix   /usr

Summary: Handy library of utility functions
Name: glib
Version: %ver
Release: %rel
Copyright: LGPL
Group: Libraries
Source: ftp://ftp.gimp.org/pub/gtk/v1.0/glib-%{ver}.tar.gz
BuildRoot: /tmp/glib-root
Packager: Dick Porter <dick@cymru.net>
URL: http://www.gtk.org
Docdir: %{prefix}/doc

%description
Handy library of utility functions.  Development libs and headers
are in glib-devel.

%package devel
Summary: GIMP Toolkit and GIMP Drawing Kit support library
Group: X11/Libraries
Requires: gtk+
Obsoletes: gtk-devel

%description devel
Static libraries and header files for the support library for the GIMP's X
libraries, which are available as public libraries.  GLIB includes generally
useful data structures.


%changelog

* Mon Apr 13 1998 Marc Ewing <marc@redhat.com>

- Split out glib package

%prep
%setup

%build
# Needed for snapshot releases.
if [ ! -f configure ]; then
  CFLAGS="$RPM_OPT_FLAGS" ./autogen.sh --prefix=%prefix
else
  CFLAGS="$RPM_OPT_FLAGS" ./configure --prefix=%prefix
fi

if [ "$SMP" != "" ]; then
  (make "MAKE=make -k -j $SMP"; exit 0)
  make
else
  make
fi

%install
rm -rf $RPM_BUILD_ROOT

make prefix=$RPM_BUILD_ROOT%{prefix} install

%clean
rm -rf $RPM_BUILD_ROOT

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%defattr(-, root, root)

%doc AUTHORS COPYING ChangeLog NEWS README
%{prefix}/lib/libglib-1.1.so.*

%files devel
%defattr(-, root, root)

%{prefix}/lib/lib*.so
%{prefix}/lib/*a
%{prefix}/lib/glib
%{prefix}/include/*
%{prefix}/share/aclocal/*
%{prefix}/bin/*
