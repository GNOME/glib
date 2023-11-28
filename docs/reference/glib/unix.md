Title: Unix-specific Utilities
SPDX-License-Identifier: LGPL-2.1-or-later
SPDX-FileCopyrightText: 2011 Colin Walters

# Unix-specific Utilities

Most of GLib is intended to be portable; in contrast, this set of
functions is designed for programs which explicitly target Unix,
or are using it to build higher level abstractions which would be
conditionally compiled if the platform matches `G_OS_UNIX`.

To use these functions, you must explicitly include the
`glib-unix.h` header.

## File Descriptors

 * [func@GLib.unix_open_pipe]
 * [func@GLib.unix_set_fd_nonblocking]

## Pipes

The [struct@GLib.UnixPipe] structure can be used to conveniently open and
manipulate a Unix pipe.

<!-- FIXME: https://gitlab.gnome.org/GNOME/gi-docgen/-/issues/173 -->
The methods for it are all static inline for efficiency. They are:

 * `g_unix_pipe_open()`
 * `g_unix_pipe_get()`
 * `g_unix_pipe_steal()`
 * `g_unix_pipe_close()`
 * `g_unix_pipe_clear()`

## Signals

 * [func@GLib.unix_signal_add]
 * [func@GLib.unix_signal_add_full]
 * [func@GLib.unix_signal_source_new]

## Polling

 * [func@GLib.unix_fd_add]
 * [func@GLib.unix_fd_add_full]
 * [func@GLib.unix_fd_source_new]

## User Database

 * [func@GLib.unix_get_passwd_entry]
