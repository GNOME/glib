/*
 * Copyright © 2013 Canonical Limited
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the licence, or (at your option) any later version.
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
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#ifndef __G_CLEANUP_H__
#define __G_CLEANUP_H__

#if !defined (__GLIB_H_INSIDE__) && !defined (GLIB_COMPILATION)
#error "Only <glib.h> can be included directly."
#endif

#include <glib/gconstructor.h>
#include <glib/gtypes.h>

G_BEGIN_DECLS

typedef struct
{
  gpointer priv[4];
} GCleanupList;

typedef void (* GCleanupFunc) (gpointer user_data);

GLIB_AVAILABLE_IN_2_36
gboolean                g_cleanup_is_enabled                            (void);
GLIB_AVAILABLE_IN_2_36
void                    g_cleanup_list_add                              (GCleanupList *list,
                                                                         GCleanupFunc  cleanup_func,
                                                                         gpointer      user_data);
GLIB_AVAILABLE_IN_2_36
void                    g_cleanup_list_remove                           (GCleanupList *list,
                                                                         GCleanupFunc  cleanup_func,
                                                                         gpointer      user_data);
GLIB_AVAILABLE_IN_2_36
void                    g_cleanup_list_clear                            (GCleanupList *list);


#if defined(G_HAS_CONSTRUCTORS) && !defined(G_DEFINE_CONSTRUCTOR_NEEDS_PRAGMA)

#define G_CLEANUP_DEFINE \
  GCleanupList _glib_cleanup_list;                                              \
  G_DEFINE_DESTRUCTOR (_glib_do_cleanup)                                        \
  static void _glib_do_cleanup (void) {                                         \
    g_cleanup_list_clear (&_glib_cleanup_list);                                 \
  }
#define G_CLEANUP_ADD(data, func) \
  G_STMT_START {                                                                \
    extern GCleanupList _glib_cleanup_list;                                     \
    if (0) (func) ((data));                                                     \
    g_warn_if_fail ((data) != NULL);                                            \
    if (data)                                                                   \
      g_cleanup_list_add (&_glib_cleanup_list, (void*) (func), (data));         \
  } G_STMT_END
#define G_CLEANUP_REMOVE(data, func) \
  G_STMT_START {                                                                \
    extern GCleanupList _glib_cleanup_list;                                     \
    if (0) (func) ((data));                                                     \
    g_warn_if_fail ((data) != NULL);                                            \
    if (data)                                                                   \
      g_cleanup_list_remove (&_glib_cleanup_list, (void*) (func), (data));      \
  } G_STMT_END
#define G_CLEANUP_ADD_FUNC(func) \
  G_STMT_START {                                                                \
    extern GCleanupList _glib_cleanup_list;                                     \
    if (0) (func) ();                                                           \
    g_cleanup_list_add (&_glib_cleanup_list, (void*) (func), NULL);             \
  } G_STMT_END

#else

#define G_CLEANUP_DEFINE
#define G_CLEANUP_ADD(data, func) \
  G_STMT_START {                                                        \
    g_warn_if_fail ((data) != NULL);                                    \
    if (0) (func) (data);                                               \
  } G_STMT_END
#define G_CLEANUP_REMOVE(data, func) \
  G_STMT_START {                                                        \
    g_warn_if_fail ((data) != NULL);                                    \
    if (0) (func) (data);                                               \
  } G_STMT_END
#define G_CLEANUP_ADD_FUNC(func) \
  G_STMT_START {                                                        \
    if (0) (func) ();                                                   \
  } G_STMT_END

#endif

G_END_DECLS

#endif /* __G_CLEANUP_H__ */
