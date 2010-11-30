Note that all this is rather experimental.

This VS9 solution and the projects it includes are intented to be used
in a GLib source tree unpacked from a tarball. In a git checkout you
first need to use some Unix-like environment or manual work to expand
the .in files needed, mainly config.h.win32.in into config.h.win32 and
glibconfig.h.win32.in into glibconfig.h.win32. You will also need to
expand the .vcprojin files here into .vcproj files.

The dependencies for this package are proxy-libintl and zlib.
Fetch the latest proxy-libintl-dev and zlib-dev zipfiles from
http://ftp.gnome.org/pub/GNOME/binaries/win32/dependencies/ for 32-bit
builds, and correspondingly
http://ftp.gnome.org/pub/GNOME/binaries/win64/dependencies/ for 64-bit
builds.

Optionally, one may use an existing copy of PCRE with the
$(BuildType)_ExtPCRE configurations.  One may get a copy of 32-bit
PCRE at http://sourceforge.net/projects/kde-windows/files/pcre/, be
sure to download versions with the file name like *-vc90-*; you will
need the *-lib.tar.bz2 and *-bin.tar.bz2 at least.  One may also build
one's own PCRE using CMake (compiling PCRE for users wishing to use an
exist copy of PCRE is required for x64 users as x64 MSVC
PCRE binaries are not provided at the moment), it need to be compiled
with the PCRE_SUPPORT_UTF8, PCRE_SUPPORT_UNICODE_PROPERTIES and
BUILD_SHARED_LIBS options selected.  Please also note that PCRE 
must also be built with Visual C++ 2008 to avoid crashes caused by
different CRTs in GLib and GLib-based programs.  Usage of the GLib-
bundled PCRE can be achieved using the $(BuildType) configurations
without the _ExtPCRE suffix.

Set up the source tree as follows under some arbitrary top
folder <root>:

<root>\glib\<this-glib-source-tree>
<root>\vs9\<PlatformName>

*this* file you are now reading is thus located at
<root>\glib\<this-glib-source-tree>\build\win32\vs9\README.

<PlatformName> is either Win32 or x64, as in VS9 project files.

You should unpack the proxy-libintl-dev, zlib-dev and (optionally)
pcre*-vc90-lib.tar.bz2 zip/bzip2 files into <root>\vs9\<PlatformName>,
so that for instance libintl.h end up at 
<root>\vs9\<PlatformName>\include\libintl.h.

The "install" project will copy build results and headers into their
appropriate location under <root>\vs9\<PlatformName>. For instance,
built DLLs go into <root>\vs9\<PlatformName>\bin, built LIBs into
<root>\vs9\<PlatformName>\lib and GLib headers into
<root>\vs9\<PlatformName>\include\glib-2.0. This is then from where
project files higher in the stack are supposed to look for them, not
from a specific GLib source tree.

--Tor Lillqvist <tml@iki.fi>
--Updated by Chun-wei Fan <fanc999 --at-- yahoo _dot_ com _dot_ tw>
