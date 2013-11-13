/*
 * Copyright © 2013 Canonical Limited
 * Copyright © 2013 Red Hat Inc.
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
 *         Stef Walter <stefw@redhat.com>
 */

#ifndef __G_CLEANUP_H__
#define __G_CLEANUP_H__

#if !defined (__GLIB_H_INSIDE__) && !defined (GLIB_COMPILATION)
#error "Only <glib.h> can be included directly."
#endif

#include <glib/gconstructor.h>
#include <glib/gmain.h>
#include <glib/gquark.h>
#include <glib/gtypes.h>

G_BEGIN_DECLS

/* Don't use codes higher than 1000 or lower than -1000 */
enum {
  G_CLEANUP_PHASE_EARLY = -50,
  G_CLEANUP_PHASE_DEFAULT = 0,
  G_CLEANUP_PHASE_LATE = 50,
  G_CLEANUP_PHASE_GRAVEYARD = 100,
};

enum {
  G_CLEANUP_SCOPE_FORCE = 1 << 0,
};

typedef struct
{
  /*< private >*/
  gint value;
  gpointer priv[4];
} GCleanupScope;

typedef void (* GCleanupFunc) (gpointer user_data);

GLIB_AVAILABLE_IN_2_40
gboolean                g_cleanup_is_enabled             (void);
GLIB_AVAILABLE_IN_2_40
gpointer                g_cleanup_push                   (GCleanupScope *cleanup,
                                                          gint           phase,
                                                          GCleanupFunc   cleanup_func,
                                                          gpointer       user_data);
GLIB_AVAILABLE_IN_2_40
gpointer                g_cleanup_push_pointer           (GCleanupScope *cleanup,
                                                          gint           phase,
                                                          GCleanupFunc   cleanup_func,
                                                          gpointer      *pointer_to_data);
GLIB_AVAILABLE_IN_2_40
void                    g_cleanup_push_source            (GCleanupScope *cleanup,
                                                          gint           phase,
                                                          GSource       *source);
GLIB_AVAILABLE_IN_2_40 /* NOTE: annotate() very useful for debugging, but might not merge */
void                    g_cleanup_annotate               (gpointer       cleanup_item,
                                                          const gchar   *func_name);
GLIB_AVAILABLE_IN_2_40
void                    g_cleanup_remove                 (gpointer       cleanup_item);
GLIB_AVAILABLE_IN_2_40
GCleanupFunc            g_cleanup_steal                  (gpointer       cleanup_item,
                                                          gpointer      *user_data);
GLIB_AVAILABLE_IN_2_40
void                    g_cleanup_clean                  (GCleanupScope *cleanup);

#if defined(G_CLEANUP_SCOPE) && defined(G_HAS_CONSTRUCTORS) && !defined(G_DEFINE_CONSTRUCTOR_NEEDS_PRAGMA)

/*
 * G_CLEANUP_SCOPE acts as a pointer with a constant address usable in initializers.
 */

extern GCleanupScope G_CLEANUP_SCOPE[1];

#define G_CLEANUP_DEFINE_WITH_FLAGS(flags) \
  GCleanupScope G_CLEANUP_SCOPE[1] = { { flags, { 0, }, } };                  \
  G_DEFINE_DESTRUCTOR (G_PASTE (G_CLEANUP_SCOPE, _perform))                   \
  static void G_PASTE (G_CLEANUP_SCOPE, _perform) (void) {                    \
    g_cleanup_clean (G_CLEANUP_SCOPE);                                        \
  }
#define G_CLEANUP_IN(data, func, phase) \
  G_STMT_START {                                                              \
    gpointer _it = g_cleanup_push (G_CLEANUP_SCOPE, phase,                    \
                                   (void*) (func), (data));                   \
    g_cleanup_annotate (_it, G_STRINGIFY (func));                             \
    if (0) (func) ((data));                                                   \
  } G_STMT_END
#define G_CLEANUP_FUNC_IN(func, phase) \
  G_STMT_START {                                                              \
    gpointer _it = g_cleanup_push (G_CLEANUP_SCOPE, phase,                    \
                                   (void*) (func), NULL);                     \
    g_cleanup_annotate (_it, G_STRINGIFY (func));                             \
    if (0) (func) ();                                                         \
  } G_STMT_END
#define G_CLEANUP_POINTER_IN(pp, func, phase) \
  G_STMT_START {                                                              \
    gpointer *_pp = (gpointer *)pp;                                           \
    gpointer _it = g_cleanup_push_pointer (G_CLEANUP_SCOPE, phase,            \
                                           (void*) (func), _pp);              \
    g_cleanup_annotate (_it, G_STRINGIFY (func));                             \
    if (0) (func) (*(pp));                                                    \
  } G_STMT_END

#else

#define G_CLEANUP_SCOPE    NULL
#define G_CLEANUP_DEFINE_WITH_FLAGS(flags)
#define G_CLEANUP_IN(data, func, phase) \
  G_STMT_START {                                                        \
    if (0) (func) (data);                                               \
  } G_STMT_END
#define G_CLEANUP_FUNC_IN(func, phase) \
  G_STMT_START {                                                              \
    if (0) (func) ();                                                         \
  } G_STMT_END
#define G_CLEANUP_POINTER_IN(pp, func, phase) \
  G_STMT_START {                                                              \
    if (0) (func) (*(pp));                                                    \
  } G_STMT_END

#endif

#define G_CLEANUP_DEFINE \
  G_CLEANUP_DEFINE_WITH_FLAGS(0)
#define G_CLEANUP(data, func) \
  G_CLEANUP_IN (data, func, G_CLEANUP_PHASE_DEFAULT)
#define G_CLEANUP_FUNC(func) \
  G_CLEANUP_FUNC_IN (func, G_CLEANUP_PHASE_DEFAULT)
#define G_CLEANUP_POINTER(pp, func) \
  G_CLEANUP_POINTER_IN (func, G_CLEANUP_PHASE_DEFAULT)

G_END_DECLS

#endif /* __G_CLEANUP_H__ */
