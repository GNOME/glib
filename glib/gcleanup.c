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

#include "config.h"

#include "gcleanup.h"

#include "glib-init.h"
#include "glib-private.h"

#include "gatomic.h"
#include "gbitlock.h"
#include "ghash.h"
#include "gmacros.h"
#include "gtestutils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * SECTION:gcleanup
 * @short_description: Cleanup on Exit
 *
 * The cleanup facilities allow GLib based libraries to clean up their global
 * variables on exit or unloading of the module. This is useful for verifying
 * that no memory leaks are present, and works well in conjuction with tools
 * like valgrind.
 *
 * To use cleanup, define %G_CLEANUP_SCOPE either in your make files or
 * in a non-public header. This declares the #GCleanupScope that cleanup items
 * will be added to.
 *
 * Create the cleanup scope using the %G_CLEANUP_DEFINE macro in a source
 * file. To push items for cleanup use %G_CLEANUP or %G_CLEANUP_FUNC.
 *
 * The <literal>G_DEBUG</literal> environment variable must contain the word
 * '<literal>cleanup</literal>' for the cleanup to occur.
 *
 * The cleanup is ordered in phases. Cleanup items in lower numbered phases
 * are run before those in higher numbered phases. Several phases are
 * predefined, but you are free to define your own between the integers
 * -1000 and 1000.
 *
 * It is permissible to add or remove other cleanup items at any time, and
 * from any thread.
 *
 * Note that cleanup items registered by a library will run before the items
 * of the libraries it depends on. The phases are only respected within a
 * single %G_CLEANUP_SCOPE.
 */

/**
 * G_CLEANUP_SCOPE:
 *
 * Defines the cleanup scope.
 *
 * Applications or libraries should define this if they wish to make use
 * of the cleanup facilities. If not defined, then cleanup functions will
 * do nothing. Be careful not to define it in any public header files.
 *
 * For example, you might use this in its Makefile.am:
 * |[
 * INCLUDES = -DG_CLEANUP_SCOPE=my_app_cleanup
 * ]|
 *
 * If %G_CLEANUP_SCOPE is not defined, then it will automatically be treated
 * as %NULL, and cleanup facilities will not be compiled in.
 *
 * You can pass %G_CLEANUP_SCOPE as a #GCleanupScope to g_cleanup_list_push()
 * and g_cleanup_list_remove() if you need to.
 */

/**
 * G_CLEANUP_PHASE_EARLY:
 *
 * Cleanup items that run before the main phase. This might be used for cleanup
 * items that stop worker threads.
 */

/**
 * G_CLEANUP_PHASE_DEFAULT:
 *
 * The main set of cleanup items.
 */

/**
 * G_CLEANUP_PHASE_LATE:
 *
 * Cleanup items that run after the main phase. This is used to cleanup items
 * that the main cleanup phase still depends on.
 */

/**
 * G_CLEANUP_PHASE_GRAVEYARD:
 *
 * Special extremely late cleanup items. Rarely used outside of GLib. By
 * convention, cleanup items running in this phase should not only use lower
 * level facilitities, and not run other parts of the library's code.
 */

/*
 * NOTE: most glib functions are off limits in this function without
 * careful consideration. In particular logging functions,
 * use various locks, which would cause issues during cleanup.
 */

/* As good a place as any to put this... */
G_CLEANUP_DEFINE;

static gint marked = 0;

/* GCleanupNode flags */
enum {
  DEREF_CLEAR_POINTER = 1 << 16,
  PHASE_MASK = 0xFFFF
};

typedef struct _GCleanupNode GCleanupNode;
struct _GCleanupNode
{
  /* Lower 16 bits is phase, higher bits are flags */
  guint           phase_and_flags;

  /* @func is accessed atomically. If NULL, then node is removed */
  GCleanupFunc    func;
  gpointer        data;

  /* May drop this if we drop debugging */
  const gchar    *func_name;

  GCleanupNode   *next;
};

typedef struct {
  gint flags;
  GCleanupNode *nodes;
  gint lock;
  gint swept;
} GRealCleanup;

static gboolean
check_if_verbose (void)
{
  const gchar *env = getenv ("G_MESSAGES_DEBUG");
  return (env && (strstr (env, "GLib-Cleanup") || strcmp (env, "all") == 0));
}

/**
 * g_cleanup_is_enabled:
 *
 * Checks if the program should attempt to cleanup allocated memory at
 * exit.
 *
 * This function will return true if the G_DEBUG variable is set to or
 * includes 'cleanup'.
 *
 * See G_CLEANUP() and %G_CLEANUP_DEFINE for the recommended way to
 * deal with memory cleanup.
 *
 * Returns: %TRUE if memory cleanup is enabled
 *
 * Since: 2.40
 **/
gboolean
g_cleanup_is_enabled (void)
{
  return g_cleanup_enabled;
}

static gpointer
cleanup_scope_push (GRealCleanup  *cleanup,
                    gint           phase,
                    guint          flags,
                    GCleanupFunc   cleanup_func,
                    gpointer       user_data)
{
  GCleanupNode *node;
  GCleanupNode **ptr;

  node = NULL;

  /*
   * We use the bit locks, as they don't need cleanup themselves. In theory
   * we could perform all the needed operations in a lock-less manner, but
   * using a simple lock should be more efficient.
   */

  g_bit_lock (&cleanup->lock, 1);

  /*
   * Item removal is optimized for removal during cleanup. However in the case
   * of repeated removal/push during the source of the process (ie: before
   * cleanup has begun), we don't want the GCleanupScope to become a memory
   * leak.
   *
   * So we reuse removed nodes here.
   */
  marked = g_atomic_int_get (&marked);
  if (marked != cleanup->swept)
    {
      for (ptr = &cleanup->nodes; (node = *ptr) != NULL; ptr = &(*ptr)->next)
        {
          /* If the node->func is NULL, steal the node */
          if (g_atomic_pointer_compare_and_exchange (&node->func, NULL, cleanup_func))
            {
              g_atomic_int_add (&marked, -1);
              *ptr = node->next;
              break;
            }
        }

      /* A full pass found nothing, don't try again */
      if (node == NULL)
        cleanup->swept = marked;
    }

  /* Allocate a new one */
  if (node == NULL)
      node = calloc (1, sizeof (GCleanupNode));

  if (node != NULL)
    {
      node->func = cleanup_func;
      node->data = user_data;

      /*
       * We use the first 16 bits of GCleanupNode->phase_and_flags as the phase.
       * However we want callers to be able to specify zero as default phase,
       * negative as early, positive as late. So convert to a unsigned phase
       * here, and combine with flags.
       */

      g_assert ((flags & PHASE_MASK) == 0);
      phase = CLAMP (phase, -1024, 1024) + G_MAXINT16;
      node->phase_and_flags = (guint) phase | flags;

      node->next = cleanup->nodes;
      cleanup->nodes = node;
    }

  g_bit_unlock (&cleanup->lock, 1);

  return node;
}

void
g_cleanup_annotate (gpointer       cleanup_item,
                    const gchar   *func_name)
{
  GCleanupNode *node;

  if (cleanup_item == NULL)
    return;

  node = cleanup_item;
  node->func_name = func_name;

  if (check_if_verbose ())
    {
      fprintf (stderr, "GLib-Cleanup-DEBUG: pushed: %s (%p) at %u\n",
               func_name, node->data, node->phase_and_flags & PHASE_MASK);
    }
}

/** xxxx
 * g_cleanup_list_push:
 * @scope: (allow-none): a #GCleanupScope
 * @phase: a phase to run this cleanup item in
 * @cleanup_func: the cleanup function
 * @user_data: (allow-none): data for the cleanup function
 * @func_name: (allow-none): static string representing name of function
 *
 * Adds a cleanup item to @scope.
 *
 * When g_cleanup_list_clean() is called on @scope, @cleanup_func will be
 * called with @user_data.
 *
 * Most typically, you will not use this function directly.  See
 * G_CLEANUP(), G_CLEANUP_FUNC().
 *
 * This function is threadsafe.  Multiple threads can add to the same
 * scope at the same time.
 *
 * The returned pointer can be used with g_cleanup_list_remove()
 * to later remove the item.
 *
 * Since: 2.40
 *
 * Returns: a tag which can be used for item removal
 * xxxxx
 **/
gpointer
g_cleanup_push (GCleanupScope *cleanup,
                gint           phase,
                GCleanupFunc   cleanup_func,
                gpointer       user_data)
{
  GRealCleanup *real = (GRealCleanup *)cleanup;

  if (cleanup && (real->flags & G_CLEANUP_SCOPE_FORCE || g_cleanup_enabled))
    return cleanup_scope_push (real, phase, 0, cleanup_func, user_data);

  return NULL;
}

gpointer
g_cleanup_push_pointer (GCleanupScope *cleanup,
                        gint           phase,
                        GCleanupFunc   cleanup_func,
                        gpointer      *pointer_to_data)
{
  GRealCleanup *real = (GRealCleanup *)cleanup;

  if (cleanup && (real->flags & G_CLEANUP_SCOPE_FORCE || g_cleanup_enabled))
    {
      return cleanup_scope_push (real, phase, DEREF_CLEAR_POINTER,
                                 cleanup_func, pointer_to_data);
    }
  return NULL;
}

static gboolean
dummy_callback (gpointer user_data)
{
  g_assert_not_reached ();
  return FALSE;
}

void
g_cleanup_push_source (GCleanupScope *cleanup,
                       gint           phase,
                       GSource       *source)
{
  GRealCleanup *real = (GRealCleanup *)cleanup;
  gpointer cleanup_item;
  GSource *child;

  /*
   * So we want to get a callback when a source is destroyed. The only way
   * I've found to do that is by registering a child source. By setting NULL
   * for all the functions, they return FALSE, and don't dispatch().
   */

  static GSourceFuncs funcs = { NULL, };

  if (cleanup && (real->flags & G_CLEANUP_SCOPE_FORCE || g_cleanup_enabled))
    {
      cleanup_item = cleanup_scope_push (real, phase, 0, (GCleanupFunc)g_source_destroy, source);
      if (cleanup_item)
        {
          child = g_source_new (&funcs, sizeof (GSource));
          g_source_set_callback (child, dummy_callback, cleanup_item, g_cleanup_remove);
          g_source_add_child_source (source, child);
          g_source_unref (child);
        }
    }
}

/** xxxx
 * g_cleanup_steal:
 * @scope: (allow-none): a #GCleanupScope
 * @item: (allow-none): the cleanup item
 *
 * Removes an @item in the scope.
 *
 * It is not typically necessary to remove cleanup items, since cleanup is
 * usually done on global or otherwise persistent data.
 *
 * This function reverses a previous call to g_cleanup_list_push(), and takes
 * the item pointer returned by g_cleanup_list_push().
 *
 * This function is threadsafe.  You can call this function one or zero times
 * for an item that was added to the @scope. However you should not call this
 * function after g_cleanup_list_clean() returns.
 *
 * Since: 2.40
 * xxxxx
**/
void
g_cleanup_remove (gpointer cleanup_item)
{
  g_cleanup_steal (cleanup_item, NULL);
}

GCleanupFunc
g_cleanup_steal (gpointer  cleanup_item,
                 gpointer *stolen_data)
{
  GCleanupNode *node;
  GCleanupFunc func;
  gpointer data;

  if (!cleanup_item)
    return NULL;

  /*
   * We optimize the case where items are removed during cleanup, as this
   * happens very often. However, see g_cleanup_list_add().
   */

  node = cleanup_item;

  /*
   * Always access @func atomically. This allows us to not hold the lock
   * while executing the callbacks in g_cleanup_list_clean().
   */
  do
    {
      data = node->data;
      func = g_atomic_pointer_get (&node->func);
      if (!func)
        break;
    }
  while (!g_atomic_pointer_compare_and_exchange (&node->func, func, NULL));

  if (!func)
    return NULL;

  /* Help g_cleanup_push find this removed item */
  g_atomic_int_add (&marked, 1);

  if (check_if_verbose ())
    {
      fprintf (stderr, "GLib-Cleanup-DEBUG: remove: %s (%p) at %d\n",
               node->func_name, data, node->phase_and_flags & PHASE_MASK);
    }

  if (stolen_data)
    *stolen_data = data;
  return func;
}

/** xxxx
 * g_cleanup_list_clean:
 * @scope: a #GCleanupScope
 *
 * Clears @scope.
 *
 * This results in all of the previously-added functions being called.
 *
 * You usually do not need to call this directly. %G_CLEANUP_DEFINE will
 * emit a destructor function to call this when your library or program
 * is being unloaded.
 *
 * This function is threadsafe.  Changes can occur while adds and
 * changes are occuring in other threads.
 *
 * Since: 2.40
 * xxx
 **/
void
g_cleanup_clean (GCleanupScope *scope)
{
  GRealCleanup *cleanup;
  GCleanupNode *later;
  GCleanupNode *nodes;
  GCleanupNode *node;
  GCleanupNode **last;
  GCleanupFunc func;
  gboolean verbose;
  gint phase, node_phase;
  gint next;

  if (!scope)
    return;

  verbose = check_if_verbose ();
  cleanup = (GRealCleanup *)scope;

  later = NULL;
  nodes = NULL;
  phase = 0;
  next = G_MAXUINT16;

  do
    {
      later = nodes;

      g_bit_lock (&cleanup->lock, 1);

      nodes = cleanup->nodes;
      cleanup->nodes = NULL;

      g_bit_unlock (&cleanup->lock, 1);

      /* The current phase */
      phase = next;

      /* No next phase yet */
      next = G_MAXUINT16;

      /* Find a lower phase, and find last node */
      last = &nodes;
      for (node = nodes; node; node = node->next)
        {
          node_phase = node->phase_and_flags & PHASE_MASK;
          if (node_phase < phase)
            phase = node_phase;
          last = &node->next;
        }

      /* Join the two lists */
      *last = later;

      /* Run that phase */
      for (node = nodes; node; node = node->next)
        {
          func = g_atomic_pointer_get (&node->func);
          if (!func)
            continue;

          node_phase = node->phase_and_flags & PHASE_MASK;

          /* Part of this phase */
          if (phase == node_phase)
            {
              if (g_atomic_pointer_compare_and_exchange (&node->func, func, NULL))
                {
                  if (verbose)
                    {
                      fprintf (stderr, "GLib-Cleanup-DEBUG: clean: %s (%p) at %d\n",
                               node->func_name, node->data, phase);
                    }
                  if (node->phase_and_flags & DEREF_CLEAR_POINTER)
                    g_clear_pointer ((gpointer *)node->data, func);
                  else
                    (func) (node->data);
                }
            }

          /* Find the next phase */
          else if (node_phase > phase && node_phase < next)
            {
              next = node_phase;
            }
        }
    }
  while (next != G_MAXUINT16);

  /* Free all the nodes */
  while (nodes)
    {
      node = nodes;
      nodes = node->next;
      free (node);
    }

  if (verbose)
    fprintf (stderr, "GLib-Cleanup-DEBUG: cleanup: done\n");
}

/**
 * G_CLEANUP_DEFINE:
 *
 * Sets up the GLib memory cleanup infrastructure for a shared library
 * or executable program.  This macro should be used once per
 * shared library or executable.
 *
 * The macro defines a linked scope to which cleanup functions will be
 * added if memory cleanup has been enabled.  It also defines a
 * destructor function to free the items in this scope on the current
 * module being unloaded (usually after main() returns).
 *
 * Since: 2.40
 **/

/**
 * G_CLEANUP:
 * @data: the data to free
 * @notify: the function used to free data
 *
 * Marks an item to be freed when performing memory cleanup.
 *
 * If memory cleanup is enabled then @function will be called on @data
 * at the time that destructors are being run for the current module
 * (ie: at program exit or module unload).
 *
 * In order for this to work, G_CLEANUP_DEFINE needs to be used exactly
 * once somewhere in a source file in your module.
 *
 * If you want to call a function to cleanup several static variables
 * then use G_CLEANUP_FUNC() instead.
 *
 * Since: 2.40
 **/

/**
 * G_CLEANUP_FUNC:
 * @cleanup_func: the cleanup function to call
 *
 * Adds a function to be called when performing memory cleanup.
 *
 * If memory cleanup is enabled then @function will be called at the
 * time that destructors are being run for the current module (ie: at
 * program exit or module unload).
 *
 * In order for this to work, G_CLEANUP_DEFINE needs to be used exactly
 * once somewhere in a source file in your module.
 *
 * Since: 2.40
 **/
