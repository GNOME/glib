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

#ifndef __G_VERSION_MACROS_H__
#define __G_VERSION_MACROS_H__

/* Version boundaries checks */

#define G_ENCODE_VERSION(major,minor)   ((major) << 16 | (minor) << 8)

/* XXX: Every new stable minor release bump should add a macro here */

/**
 * GLIB_VERSION_2_26:
 *
 * A macro that evaluates to the 2.26 version of GLib, in a format
 * that can be used by the C pre-processor.
 *
 * Since: 2.32
 */
#define GLIB_VERSION_2_26       (G_ENCODE_VERSION (2, 26))

/**
 * GLIB_VERSION_2_28:
 *
 * A macro that evaluates to the 2.28 version of GLib, in a format
 * that can be used by the C pre-processor.
 *
 * Since: 2.32
 */
#define GLIB_VERSION_2_28       (G_ENCODE_VERSION (2, 28))

/**
 * GLIB_VERSION_2_30:
 *
 * A macro that evaluates to the 2.30 version of GLib, in a format
 * that can be used by the C pre-processor.
 *
 * Since: 2.32
 */
#define GLIB_VERSION_2_30       (G_ENCODE_VERSION (2, 30))

/**
 * GLIB_VERSION_2_32:
 *
 * A macro that evaluates to the 2.32 version of GLib, in a format
 * that can be used by the C pre-processor.
 *
 * Since: 2.32
 */
#define GLIB_VERSION_2_32       (G_ENCODE_VERSION (2, 32))

/* evaluates to the current stable version; for development cycles,
 * this means the next stable target
 */
#if (GLIB_MINOR_VERSION % 2)
#define GLIB_VERSION_CUR_STABLE         (G_ENCODE_VERSION (GLIB_MAJOR_VERSION, GLIB_MINOR_VERSION + 1))
#else
#define GLIB_VERSION_CUR_STABLE         (G_ENCODE_VERSION (GLIB_MAJOR_VERSION, GLIB_MINOR_VERSION))
#endif

/* evaluates to the previous stable version */
#if (GLIB_MINOR_VERSION % 2)
#define GLIB_VERSION_PREV_STABLE        (G_ENCODE_VERSION (GLIB_MAJOR_VERSION, GLIB_MINOR_VERSION - 1))
#else
#define GLIB_VERSION_PREV_STABLE        (G_ENCODE_VERSION (GLIB_MAJOR_VERSION, GLIB_MINOR_VERSION - 2))
#endif

/**
 * GLIB_VERSION_MIN_REQUIRED:
 *
 * A macro that should be defined by the user prior to including
 * the glib.h header.
 * The definition should be one of the predefined GLib version
 * macros: %GLIB_VERSION_2_26, %GLIB_VERSION_2_28,...
 *
 * This macro defines the lower bound for the GLib API to use.
 *
 * If a function has been deprecated in a newer version of GLib,
 * it is possible to use this symbol to avoid the compiler warnings
 * without disabling warning for every deprecated function.
 *
 * Since: 2.32
 */
#ifndef GLIB_VERSION_MIN_REQUIRED
# define GLIB_VERSION_MIN_REQUIRED      (GLIB_VERSION_PREV_STABLE)
#endif

/**
 * GLIB_VERSION_MAX_ALLOWED:
 *
 * A macro that should be defined by the user prior to including
 * the glib.h header.
 * The definition should be one of the predefined GLib version
 * macros: %GLIB_VERSION_2_26, %GLIB_VERSION_2_28,...
 *
 * This macro defines the upper bound for the GLib API to use.
 *
 * If a function has been introduced in a newer version of GLib,
 * it is possible to use this symbol to get compiler warnings when
 * trying to use that function.
 *
 * Since: 2.32
 */
#ifndef GLIB_VERSION_MAX_ALLOWED
# if GLIB_VERSION_MIN_REQUIRED > GLIB_VERSION_PREV_STABLE
#  define GLIB_VERSION_MAX_ALLOWED      GLIB_VERSION_MIN_REQUIRED
# else
#  define GLIB_VERSION_MAX_ALLOWED      GLIB_VERSION_CUR_STABLE
# endif
#endif

/* sanity checks */
#if GLIB_VERSION_MAX_ALLOWED < GLIB_VERSION_MIN_REQUIRED
#error "GLIB_VERSION_MAX_ALLOWED must be >= GLIB_VERSION_MIN_REQUIRED"
#endif
#if GLIB_VERSION_MIN_REQUIRED < GLIB_VERSION_2_26
#error "GLIB_VERSION_MIN_REQUIRED must be >= GLIB_VERSION_2_26"
#endif

/* These macros are used to mark deprecated functions in GLib headers,
 * and thus have to be exposed in installed headers. But please
 * do *not* use them in other projects. Instead, use G_DEPRECATED
 * or define your own wrappers around it.
 */

/* XXX: Every new stable minor release should add a set of macros here */

#if GLIB_VERSION_MIN_REQUIRED >= GLIB_VERSION_2_26
# define GLIB_DEPRECATED_IN_2_26                GLIB_DEPRECATED
# define GLIB_DEPRECATED_IN_2_26_FOR(f)         GLIB_DEPRECATED_FOR(f)
#else
# define GLIB_DEPRECATED_IN_2_26
# define GLIB_DEPRECATED_IN_2_26_FOR(f)
#endif

#if GLIB_VERSION_MAX_ALLOWED < GLIB_VERSION_2_26
# define GLIB_AVAILABLE_IN_2_26                 GLIB_UNAVAILABLE(2, 26)
#else
# define GLIB_AVAILABLE_IN_2_26
#endif

#if GLIB_VERSION_MIN_REQUIRED >= GLIB_VERSION_2_28
# define GLIB_DEPRECATED_IN_2_28                GLIB_DEPRECATED
# define GLIB_DEPRECATED_IN_2_28_FOR(f)         GLIB_DEPRECATED_FOR(f)
#else
# define GLIB_DEPRECATED_IN_2_28
# define GLIB_DEPRECATED_IN_2_28_FOR(f)
#endif

#if GLIB_VERSION_MAX_ALLOWED < GLIB_VERSION_2_28
# define GLIB_AVAILABLE_IN_2_28                 GLIB_UNAVAILABLE(2, 28)
#else
# define GLIB_AVAILABLE_IN_2_28
#endif

#if GLIB_VERSION_MIN_REQUIRED >= GLIB_VERSION_2_30
# define GLIB_DEPRECATED_IN_2_30                GLIB_DEPRECATED
# define GLIB_DEPRECATED_IN_2_30_FOR(f)         GLIB_DEPRECATED_FOR(f)
#else
# define GLIB_DEPRECATED_IN_2_30
# define GLIB_DEPRECATED_IN_2_30_FOR(f)
#endif

#if GLIB_VERSION_MAX_ALLOWED < GLIB_VERSION_2_30
# define GLIB_AVAILABLE_IN_2_30                 GLIB_UNAVAILABLE(2, 30)
#else
# define GLIB_AVAILABLE_IN_2_30
#endif

#if GLIB_VERSION_MIN_REQUIRED >= GLIB_VERSION_2_32
# define GLIB_DEPRECATED_IN_2_32                GLIB_DEPRECATED
# define GLIB_DEPRECATED_IN_2_32_FOR(f)         GLIB_DEPRECATED_FOR(f)
#else
# define GLIB_DEPRECATED_IN_2_32
# define GLIB_DEPRECATED_IN_2_32_FOR(f)
#endif

#if GLIB_VERSION_MAX_ALLOWED < GLIB_VERSION_2_32
# define GLIB_AVAILABLE_IN_2_32                 GLIB_UNAVAILABLE(2, 32)
#else
# define GLIB_AVAILABLE_IN_2_32
#endif

#endif /*  __G_VERSION_MACROS_H__ */
