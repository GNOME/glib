Title: Version Information
SPDX-License-Identifier: LGPL-2.1-or-later
SPDX-FileCopyrightText: 2004 Matthias Clasen
SPDX-FileCopyrightText: 2012 Emmanuele Bassi

# Version Information

GLib provides version information, primarily useful in configure
checks for builds that have a configure script. Applications will
not typically use the features described here.

## Run-time Version Numbers

The variables `glib_major_version`, `glib_minor_version`, `glib_micro_version`,
`glib_binary_age` and `glib_interface_age` are all available to check.

They can be compared using the function [func@GLib.check_version].

## Compile-time Version Numbers

 * [const@GLib.MAJOR_VERSION]
 * [const@GLib.MINOR_VERSION]
 * [const@GLib.MICRO_VERSION]
 * [func@GLib.CHECK_VERSION]

## Version Numbers

The GLib headers annotate deprecated APIs in a way that produces
compiler warnings if these deprecated APIs are used. The warnings
can be turned off by defining the macro `GLIB_DISABLE_DEPRECATION_WARNINGS`
before including the `glib.h` header.

GLib also provides support for building applications against
defined subsets of deprecated or new GLib APIs. Define the macro
`GLIB_VERSION_MIN_REQUIRED` to specify up to what version of GLib
you want to receive warnings about deprecated APIs. Define the
macro `GLIB_VERSION_MAX_ALLOWED` to specify the newest version of
GLib whose API you want to use.

The macros `GLIB_VERSION_2_2`, `GLIB_VERSION_2_4`, …, `GLIB_VERSION_2_80`, etc.
are defined automatically in each release, and can be used to set the value
of macros like `GLIB_VERSION_MIN_REQUIRED`.

The macros `GLIB_VERSION_CUR_STABLE` and `GLIB_VERSION_PREV_STABLE` are also
automatically defined to point to the right version definitions.

