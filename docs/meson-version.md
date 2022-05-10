Meson version policy
===

Aims
---

 * Stable versions of GLib should be buildable from source on the majority of
   systems which are still supported by their vendors, without requiring the
   user to manually build a number of dependencies
 * Unstable versions of GLib should be able to take advantage of newer build
   system features where they would make maintenance of GLib easier, without
   prejudicing the other aims

Policy
---

 * Stable branches of GLib will not change their Meson dependency after the
   first release of that stable series
 * Unstable branches of GLib can bump their Meson dependency if
   - at least that version of Meson currently available in Debian Testing; or
   - the Python version required by the new Meson dependency is available in
     Debian Stable *and* the oldest currently-supported Ubuntu LTS
 * The version of Meson used by GLib should be pinned and pre-installed in the
   CI `Dockerfile`s so that GLib is guaranteed to be built against the expected
   version

The reasoning behind allowing a version bump if the Python which Meson depends
on is available in Debian Stable is that it’s [straightforward to install a more
recent Meson version using
`pip`](https://mesonbuild.com/Getting-meson.html#installing-meson-with-pip).
