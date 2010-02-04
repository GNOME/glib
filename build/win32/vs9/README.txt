Note that all this is rather experimental.

This VS9 solution and the projects it includes are intented to be used
in a GLib source tree unpacked from a tarball. In a git checkout you
first need to use some Unix-like environment or manual work to expand
the .in files needed, mainly config.h.win32.in into config.h.win32 and
glibconfig.h.win32.in into glibconfig.h.win32.

The only external dependency is proxy-libintl. Fetch the latest
proxy-libintl-dev zipfile from
http://ftp.gnome.org/pub/GNOME/binaries/win32/dependencies/ for 32-bit
builds, and correspondingly
http://ftp.gnome.org/pub/GNOME/binaries/win64/dependencies/ for 64-bit
builds. Set up the source tree as follows under some arbitrary top
folder <root>:

<root>\glib\<this-glib-source-tree>
<root>\glib\dependencies\<PlatformName>\proxy-libintl

*this* file you are now reading is thus located at
<root>\glib\<this-glib-source-tree>\build\win32\vs9\README.

<PlatformName> is either Win32 or x64, as in VS9 project files.

<root>\glib\dependencies\<PlatformName>\proxy-libintl contains the
unpacked proxy-libintl zip file, so that for instance libintl.h is at
<root>\glib\dependencies\<PlatformName>\proxy-libintl\include\libintl.h.

--Tor Lillqvist <tml@iki.fi>
