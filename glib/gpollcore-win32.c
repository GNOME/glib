/*
 * Copyright © 2014 Canonical Limited
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#include "config.h"

#include "gpollcore.h"

#include <windows.h>

static HANDLE
get_thread_handle (void)
{
  static GPrivate this_thread = G_PRIVATE_INIT ((GDestroyNotify) CloseHandle);
  HANDLE thread;

  thread = g_private_get (&this_thread);
  if (thread == NULL)
    {
      gboolean success;

      success = DuplicateHandle (GetCurrentProcess (), GetCurrentThread (), GetCurrentProcess (),
                                 &thread, DUPLICATE_SAME_ACCESS, FALSE, DUPLICATE_SAME_ACCESS);
      if (!success)
        g_error ("gpollcore: DuplicateHandle() fail: %u", (guint) GetLastError ());

      g_private_set (&this_thread, thread);
    }

  return thread;
}

static gboolean
g_poll_core_update_locked (GPollCore *core,
                           HANDLE     handle,
                           guint      old_events,
                           guint      new_events,
                           gpointer   user_data)
{
  /* We only care about this one flag */
  if ((old_events ^ new_events) & G_IO_IN)
    {
      gboolean enabled = !!(new_events & G_IO_IN);

      if (handle == GUINT_TO_POINTER (G_WIN32_MSG_HANDLE))
        {
          core->polling_msgs = enabled;
          core->msgs_user_data = enabled ? user_data : NULL;

          return TRUE;
        }

      if (enabled)
        {
          /* Add an entry */
          gint i;

          /* paranoid checking... */
          for (i = 0; i < core->n_handles; i++)
            g_assert (core->handles[i] != handle);

          if (core->n_handles < MAXIMUM_WAIT_OBJECTS)
            {
              core->handles[core->n_handles] = handle;
              core->user_data[core->n_handles] = user_data;
              core->n_handles++;

              return TRUE;
            }
          else
            {
              g_warning ("Windows can only wait on 64 handles per thread.  Ignoring request to add new handle.");
              return FALSE;
            }
        }
      else
        {
          gint i;

          /* Remove an entry */

          for (i = 0; i < core->n_handles; i++)
            if (core->handles[i] == handle)
              {
                core->n_handles--;

                /* Maybe existing == core->n_handles now, but in that
                 * case this is just a no-op...
                 */
                core->handles[i] = core->handles[core->n_handles];
                core->user_data[i] = core->user_data[core->n_handles];

                return TRUE;
              }

          g_assert_not_reached ();
        }
    }
  else
    return FALSE;
}

static void CALLBACK
user_apc (ULONG_PTR data)
{
  /* Do nothing -- it is enough to wake the sleep. */
}

void
g_poll_core_update (GPollCore *core,
                    HANDLE     handle,
                    guint      old_events,
                    guint      new_events,
                    gpointer   user_data)
{
  gboolean made_change;

  g_mutex_lock (&core->mutex);

  made_change = g_poll_core_update_locked (core, handle, old_events, new_events, user_data);

  if (core->waiting_thread && made_change)
    QueueUserAPC (user_apc, core->waiting_thread, 0);

  g_mutex_unlock (&core->mutex);
}

void
g_poll_core_set_ready_time (GPollCore *core,
                            gint64     ready_time)
{
  g_mutex_lock (&core->mutex);

  /* We want to wake the owner thread if it is sleeping and if the
   * current timeout is greater than the new one.
   */
  if (core->waiting_thread && ready_time >= 0 && ready_time < core->ready_time)
    QueueUserAPC (user_apc, core->waiting_thread, 0);

  core->ready_time = ready_time;

  g_mutex_unlock (&core->mutex);
}

void
g_poll_core_wait (GPollCore *core)
{
  HANDLE handles[MAXIMUM_WAIT_OBJECTS];
  gint n_handles;
  DWORD timeout;
  DWORD result;

  g_mutex_lock (&core->mutex);

again:
  memcpy (handles, core->handles, sizeof (HANDLE) * core->n_handles);
  n_handles = core->n_handles;

  if (core->ready_time > 0)
    {
      gint64 now = g_get_monotonic_time ();

      if (now < core->ready_time)
        timeout = (core->ready_time - now + 999) / 1000;
      else
        timeout = 0;
    }
  else if (core->ready_time == 0)
    timeout = 0;
  else
    timeout = INFINITE;

  core->waiting_thread = get_thread_handle ();

  g_mutex_unlock (&core->mutex);

  /* Wait on all of the objects, ignoring any results.
   * We will collect the results once we have retaken the lock.
   *
   * Set waiting_thread so that we can signal ourselves to wake up
   * if we make changes.
   */
  result = MsgWaitForMultipleObjectsEx (n_handles, handles, timeout, QS_ALLEVENTS, MWMO_ALERTABLE);

  g_mutex_lock (&core->mutex);
  core->waiting_thread = NULL;

  /* We allow APC in case the user wants to do it, but also because
   * this is how we alert ourselves if the timeout or list of
   * handles changes from another thread while we're waiting.
   */
  if (result == WAIT_IO_COMPLETION)
   goto again;

  g_mutex_unlock (&core->mutex);
}

gint
g_poll_core_update_and_collect (GPollCore  *core,
                                GHashTable *updates,
                                gint64     *ready_time_update,
                                GPollEvent *events,
                                gint        max_events)
{
  gint n_collected = 0;
  gint i;

  /* We are protected by the GMainContext lock here, so no need to use
   * our own...
   */

  /* Make sure there is room for timeout and msgs */
  g_assert (max_events >= 2);

  if (ready_time_update)
    core->ready_time = *ready_time_update;

  if (updates)
    {
      GHashTableIter iter;
      gpointer key, value;

      g_hash_table_iter_init (&iter, updates);
      while (g_hash_table_iter_next (&iter, &key, &value))
        {
          GPollUpdate *update = value;

          g_poll_core_update_locked (core, key, update->old_events, update->new_events, update->user_data);
        }
    }

  /* Check for timeout */
  if (core->ready_time < g_get_monotonic_time ())
    events[n_collected++] = NULL;

  /* Check the ready status of the message queue, if we're watching that. */
  if (core->polling_msgs && MsgWaitForMultipleObjects (0, NULL, FALSE, 0, QS_ALLEVENTS) == 0)
    events[n_collected++] = core->msgs_user_data;

  /* Check the ready statuses of all of the handles we're watching.
   * There are 64 of them at most (and typically a good deal fewer), so
   * this shouldn't be too awful...
   */
  for (i = 0; i < core->n_handles; i++)
    if (WaitForSingleObject (core->handles[i], 0) == 0)
      {
        events[n_collected++] = core->user_data[i];

        if (n_collected == max_events)
          /* This will cause us to be re-run... */
          break;
      }

  return n_collected;
}
