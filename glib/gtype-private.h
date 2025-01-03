/* GLib
 *
 * SPDX-FileCopyrightText: 1998-1999, 2000-2001 Tim Janik and Red Hat, Inc.
 * SPDX-FileCopyrightText: 2025 Emmanuele Bassi
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "gboxed.h"

G_BEGIN_DECLS

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
extern GTypeDebugFlags _g_type_debug_flags;
G_GNUC_END_IGNORE_DEPRECATIONS

gboolean g_type_has_debug_flag (int flag);

void    _g_boxed_type_init       (void); /* sync with gboxed.c */
void    _g_enum_types_init       (void); /* sync with genums.c */
void    _g_value_c_init          (void); /* sync with gvalue.c */
void    _g_value_transforms_init (void); /* sync with gvaluetransform.c */
void    _g_value_types_init      (void); /* sync with gvaluetypes.c */

/* for gboxed.c */
gpointer        _g_type_boxed_copy      (GType          type,
                                         gpointer       value);
void            _g_type_boxed_free      (GType          type,
                                         gpointer       value);
void            _g_type_boxed_init      (GType          type,
                                         GBoxedCopyFunc copy_func,
                                         GBoxedFreeFunc free_func);

G_END_DECLS
