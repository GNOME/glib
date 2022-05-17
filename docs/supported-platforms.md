Supported platforms
===

GLib’s approach to portability is that we only support systems that we can test.
That means that either a large number of GLib developers are regularly using
GLib on a particular system, or we have regular builds of GLib on that system.

Minimum versions
---

 * macOS: minimum version OS X 10.7 (we
   [don’t support universal binaries](https://bugzilla.gnome.org/show_bug.cgi?id=780238);
   some features (like notification support)
   [require OS X 10.9](https://bugzilla.gnome.org/show_bug.cgi?id=747146)
 * Windows:
   [minimum version is Windows 8](https://gitlab.gnome.org/GNOME/glib/-/merge_requests/1970),
   minimum build chain is Visual Studio 2012
   * Static builds are only supported with MinGW-based toolchains (cf
     [this comment](https://gitlab.gnome.org/GNOME/glib/-/merge_requests/2384#note_1336662))
 * Android: [minimum NDK version 15](https://gitlab.gnome.org/GNOME/glib/issues/1113)
 * Linux: glibc newer than 2.5 (if using glibc; other forms of libc are supported)

Tested platforms
---

GLib is regularly built on at least the following systems:

 * GNOME OS Nightly: https://os.gnome.org/
 * Fedora: http://koji.fedoraproject.org/koji/packageinfo?packageID=382
 * Ubuntu: http://packages.ubuntu.com/source/glib2.0
 * Debian: https://packages.debian.org/experimental/libglib2.0-0
 * FreeBSD: https://wiki.gnome.org/Projects/Jhbuild/FreeBSD
 * openSUSE: https://build.opensuse.org/package/show/GNOME:Factory/glib2
 * CI runners, https://gitlab.gnome.org/GNOME/glib/blob/main/.gitlab-ci.yml:
  * Fedora (34, https://gitlab.gnome.org/GNOME/glib/-/blob/main/.gitlab-ci/fedora.Dockerfile)
  * Debian (Bullseye, https://gitlab.gnome.org/GNOME/glib/-/blob/main/.gitlab-ci/debian-stable.Dockerfile)
  * Windows (MinGW64)
  * Windows (msys2-mingw32)
  * Windows (Visual Studio 2017, and a static linking version)
  * Android (NDK r23b, API 31, arm64, https://gitlab.gnome.org/GNOME/glib/-/blob/main/.gitlab-ci/android-ndk.sh)
  * FreeBSD (12 and 13)
  * macOS

If other platforms are to be supported, we need to set up regular CI testing for
them. Please contact us if you want to help.

Policy and rationale
---

Due to their position in the market, we consider supporting GNU/Linux, Windows
and macOS to be the highest priorities and we will go out of our way to
accommodate these systems, even in places that they are contravening standards.

In general, we are open to the idea of supporting any Free Software UNIX-like
system with good POSIX compliance.  We are always interested in receiving
patches that improve our POSIX compliance — if there is a good POSIX equivalent
for a platform-specific API that we’re using, then all other things equal, we
prefer the POSIX one.

We may use a non-POSIX API available on one or more of our supported systems in
the case that it provides some advantage over the POSIX equivalent (such as the
case with `pipe2()` solving the `O_CLOEXEC` race).  In these cases, we will try
to provide a fallback to the pure POSIX approach.  If we’ve used a
system-specific API without providing a fallback to a largely-equivalent POSIX
API then it is likely a mistake, and we’re happy to receive a patch to fix it.

We are not interested in supporting other systems if it involves adding code
paths that we cannot test.  Specifically, this means that we will reject patches
that introduce platform-specific `#ifdef` sections in the code unless we are
actively doing builds of GLib on this platform (ie: see the lists above).  We’ve
historically accepted such patches from users of these systems on an ad hoc
basis, but it created an unsustainable situation.  Patches that fix
platform-specific build issues in such a way that the code is improved in the
general case are of course welcome.

Although we aim to support all systems with good POSIX compliance, we are not
interested in adhering to “pure POSIX and nothing else”.  If we need to add a
feature and we can provide good support on all of the platforms that we support
(above), we will not hold back for other systems.  We will always try to provide
a fallback to a POSIX-specified approach, if possible, or to simply replace a
given functionality with a no-op, but even this may not be possible in cases of
critical functionality.

Specific notes
---

Note that we currently depend on a number of features specified in POSIX, but
listed as optional:

 * [`CLOCK_MONOTONIC`](http://pubs.opengroup.org/onlinepubs/009695399/functions/clock_gettime.html)
   is expected to be present and working
 * [`pthread_condattr_setclock()`](http://pubs.opengroup.org/onlinepubs/7999959899/functions/pthread_condattr_setclock.html)
   is expected to be present and working

Also see [toolchain requirements](./toolchain-requirements.md).
