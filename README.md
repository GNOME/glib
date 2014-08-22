GLib with kdbus support
=======================

The main goal of the project is add support for kdbus in GLib. More info about development status and some implementation details you can find here:

https://bugzilla.gnome.org/show_bug.cgi?id=721861

The most important tasks in new implementation are:

- add low-level support for kdbus,

- add support for two new bus types called "user" and "machine" (for kdbus purposes),

- introduce new library API for common tasks such as fetching the list of name
  owners on the bus, checking sender's PID, ..., (because it is not permitted to make
  calls to the org.freedesktop.DBus destination on the new bus types),

What is kdbus?
==============

kdbus is a Linux kernel D-Bus implementation. See kdbus.txt in kdbus repository [1] for kernel-side details. The userspace side is also developed in systemd repo [2].

- [1] https://code.google.com/p/d-bus/
- [2] http://cgit.freedesktop.org/systemd/systemd/tree/src/libsystemd/sd-bus
