/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GLib Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/.
 */

#if !defined (__GLIB_H_INSIDE__) && !defined (GLIB_COMPILATION)
#error "Only <glib.h> can be included directly."
#endif

#ifndef __G_VERSION_H__
#define __G_VERSION_H__

#include <glib/gtypes.h>

G_BEGIN_DECLS

GLIB_VAR const guint glib_major_version;
GLIB_VAR const guint glib_minor_version;
GLIB_VAR const guint glib_micro_version;
GLIB_VAR const guint glib_interface_age;
GLIB_VAR const guint glib_binary_age;

const gchar * glib_check_version (guint required_major,
                                  guint required_minor,
                                  guint required_micro);

/**
 * GLIB_CHECK_VERSION:
 * @major: the major version to check for
 * @minor: the minor version to check for
 * @micro: the micro version to check for
 *
 * Checks the version of the GLib library that is being compiled
 * against.
 *
 * <example>
 * <title>Checking the version of the GLib library</title>
 * <programlisting>
 *   if (!GLIB_CHECK_VERSION (1, 2, 0))
 *     g_error ("GLib version 1.2.0 or above is needed");
 * </programlisting>
 * </example>
 *
 * See glib_check_version() for a runtime check.
 *
 * Returns: %TRUE if the version of the GLib header files
 * is the same as or newer than the passed-in version.
 */
#define GLIB_CHECK_VERSION(major,minor,micro)    \
    (GLIB_MAJOR_VERSION > (major) || \
     (GLIB_MAJOR_VERSION == (major) && GLIB_MINOR_VERSION > (minor)) || \
     (GLIB_MAJOR_VERSION == (major) && GLIB_MINOR_VERSION == (minor) && \
      GLIB_MICRO_VERSION >= (micro)))

G_END_DECLS

#endif /*  __G_VERSION_H__ */
