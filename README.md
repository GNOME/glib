# GLib

GLib is the low-level core library that forms the basis for projects such
as GTK and GNOME. It provides data structure handling for C, portability
wrappers, and interfaces for such runtime functionality as an event loop,
threads, dynamic loading, and an object system.

The official download locations are:
  <https://download.gnome.org/sources/glib>

The official web site is:
  <https://www.gtk.org/>

## Installation

See the file '[INSTALL.in](INSTALL.in)'

## How to report bugs

Bugs should be reported to the GNOME issue tracking system.
(<https://gitlab.gnome.org/GNOME/glib/issues/new>). You will need
to create an account for yourself.

In the bug report please include:

* Information about your system. For instance:
  * What operating system and version
  * For Linux, what version of the C library
  * And anything else you think is relevant.
* How to reproduce the bug.
  * If you can reproduce it with one of the test programs that are built
  in the tests/ subdirectory, that will be most convenient.  Otherwise,
  please include a short test program that exhibits the behavior.
  As a last resort, you can also provide a pointer to a larger piece
  of software that can be downloaded.
* If the bug was a crash, the exact text that was printed out
  when the crash occurred.
* Further information such as stack traces may be useful, but
  is not necessary.

## Patches

Patches should also be submitted as merge requests to gitlab.gnome.org. If the
patch fixes an existing issue, please refer to the issue in your commit message
with the following notation (for issue 123):
Closes: #123

Otherwise, create a new merge request that introduces the change, filing a
separate issue is not required.
