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

#include "gcleanup.h"

#include "glib-init.h"
#include "gthread.h"

#include <stdlib.h>

typedef struct _GCleanupNode GCleanupNode;
struct _GCleanupNode
{
  GDestroyNotify  func;
  gpointer        data;
  GCleanupNode   *next;
};

static GMutex lock;

/**
 * g_cleanup_is_enabled:
 *
 * Checks if the program should attempt to cleanup allocated memory at
 * exit.
 *
 * This function will return true if the G_DEBUG variable is set to or
 * includes 'cleanup'.
 *
 * See G_CLEANUP_ADD() and %G_CLEANUP_DEFINE for the recommended way to
 * deal with memory cleanup.
 *
 * Returns: %TRUE if memory cleanup is enabled
 *
 * Since: 2.38
 **/
gboolean
g_cleanup_is_enabled (void)
{
  return g_cleanup_enabled;
}

/**
 * g_cleanup_list_add:
 * @list: a #GCleanupList
 * @cleanup_func: the cleanup function
 * @user_data: data for the cleanup function
 *
 * Adds a function to @list.
 *
 * When g_cleanup_list_clear() is called on @list, @cleanup_func will be
 * called with @user_data.
 *
 * Most typically, you will not use this function directly.  See
 * G_CLEANUP_ADD() or G_CLEANUP_ADD_FUNC().
 *
 * This function is threadsafe.  Multiple threads can add to the same
 * list at the same time.
 *
 * Since: 2.38
 **/
void
g_cleanup_list_add (GCleanupList *list,
                    GCleanupFunc  cleanup_func,
                    gpointer      user_data)
{
  GCleanupNode *node;

  if (!g_cleanup_enabled)
    return;

  node = malloc (sizeof (GCleanupNode));
  node->func = cleanup_func;
  node->data = user_data;

  g_mutex_lock (&lock);
  node->next = list->priv[0];
  list->priv[0] = node;
  g_mutex_unlock (&lock);
}

/**
 * g_cleanup_list_remove:
 * @list: a #GCleanupList
 * @cleanup_func: the cleanup function
 * @user_data: data for the cleanup function
 *
 * Removes an item in the list.
 *
 * This function reverses a previous call to g_cleanup_list_add().
 *
 * Most typically, you will not use this function directly.  See
 * G_CLEANUP_REMOVE().
 *
 * This function is threadsafe.  Changes can occur while adds and
 * changes are occuring in other threads.
 *
 * Since: 2.38
 **/
void
g_cleanup_list_remove (GCleanupList *list,
                       GCleanupFunc  cleanup_func,
                       gpointer      user_data)
{
  GCleanupNode *node = NULL;
  GCleanupNode **ptr;

  if (!g_cleanup_enabled)
    return;

  g_mutex_lock (&lock);
  for (ptr = (GCleanupNode **) &list->priv[0]; *ptr; ptr = &(*ptr)->next)
    {
      if ((*ptr)->data == user_data && (*ptr)->func == cleanup_func)
        {
          node = *ptr;
          *ptr = node->next;
          break;
        }
    }
  g_mutex_unlock (&lock);

  if (node)
    free (node);
}

/**
 * g_cleanup_list_clear:
 * @list: a #GCleanupList
 *
 * Clears @list.
 *
 * This results in all of the previously-added functions being called.
 *
 * This function is not threadsafe.  Nothing else may be accessing the
 * list at the time that this function is called.
 *
 * You usually do not need to call this directly.  G_CLEANUP_DEFINE will
 * emit a destructor function to call this when your library or program
 * is being unloaded.
 *
 * Since: 2.38
 **/
void
g_cleanup_list_clear (GCleanupList *list)
{
  if (!g_cleanup_enabled)
    return;

  while (list->priv[0])
    {
      GCleanupNode *node = list->priv[0];

      (* node->func) (node->data);
      list->priv[0] = node->next;
      free (node);
    }
}

/**
 * G_CLEANUP_DEFINE:
 *
 * Sets up the GLib memory cleanup infrastructure for a shared library
 * or executable program.  This macro should be used exactly once per
 * shared library or executable.
 *
 * The macro defines a linked list to which cleanup functions will be
 * added if memory cleanup has been enabled.  It also defines a
 * destructor function to free the items in this list on the current
 * module being unloaded (usually after main() returns).
 *
 * The list is declared with a visibility that makes it accessible to
 * other files in the same module but not visible outside of the module
 * (ie: G_GNUC_INTERNAL).  The exact name or type used for the list is
 * an implementation detail that may be changed; the only supported way
 * to add items to the list is by way of G_CLEANUP_ADD() or
 * G_CLEANUP_ADD_FUNC().
 *
 * Since: 2.38
 **/

/**
 * G_CLEANUP_ADD:
 * @data: the data to free, non-%NULL
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
 * then use G_CLEANUP_ADD_FUNC() instead.
 *
 * Adding the same @data more than once is undefined.
 *
 * Since: 2.38
 **/

/**
 * G_CLEANUP_ADD_FUNC:
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
 * Since: 2.38
 **/

/**
 * G_CLEANUP_READD:
 * @old_data: the data that would have been freed, non-%NULL
 * @new_data: the data that should now be freed, non-%NULL
 *
 * Modifies the data pointer used in a previous call to G_CLEANUP_ADD().
 *
 * As an example, you might need to use this in the case of needing to
 * realloc() something that had been previously marked for cleanup.
 *
 * Since: 2.38
 **/
