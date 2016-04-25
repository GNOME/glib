/*
 * Copyright © 2009, 2010 Codethink Limited
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

#include "gdelayedsettingsbackend.h"
#include "gsettingsbackendinternal.h"

#include <string.h>


struct _GDelayedSettingsBackend
{
  GSettingsBackend parent_instance;

  GSettingsBackendChangeset *changeset;
  GSettingsBackend *backend;
  gboolean has_unapplied;
  GMutex lock;

  GMainContext *owner_context;
  GWeakRef owner;
};

G_DEFINE_TYPE (GDelayedSettingsBackend, g_delayed_settings_backend, G_TYPE_SETTINGS_BACKEND)

/* {{{1 Handling of GSettings::has-unapplied property */
static void
g_delayed_settings_backend_lock_for_write (GDelayedSettingsBackend *self)
{
  g_mutex_lock (&self->lock);
}

static gboolean
g_delayed_settings_backend_notify_unapplied (gpointer user_data)
{
  g_object_notify (user_data, "has-unapplied");

  return FALSE;
}

static void
g_delayed_settings_backend_unlock_for_write (GDelayedSettingsBackend   *self,
                                             const gchar               *key,
                                             GSettingsBackendChangeset *changeset,
                                             gpointer                   origin_tag)
{
  gboolean has_unapplied;
  gboolean need_notify;

  has_unapplied = g_settings_backend_changeset_is_empty (self->changeset) == FALSE;
  need_notify = self->has_unapplied != has_unapplied;
  self->has_unapplied = has_unapplied;

  g_mutex_unlock (&self->lock);

  if (key)
    g_settings_backend_changed (G_SETTINGS_BACKEND (self), key, origin_tag);

  if (changeset)
    g_settings_backend_changeset_applied (G_SETTINGS_BACKEND (self), changeset, origin_tag);

  if (need_notify)
    {
      gpointer owner = g_weak_ref_get (&self->owner);

      if (owner)
        g_main_context_invoke_full (self->owner_context, G_PRIORITY_DEFAULT,
                                    g_delayed_settings_backend_notify_unapplied, owner, g_object_unref);
    }
}

gboolean
g_delayed_settings_backend_get_has_unapplied (GDelayedSettingsBackend *self)
{
  gboolean has_unapplied;

  g_mutex_lock (&self->lock);
  has_unapplied = self->has_unapplied;
  g_mutex_unlock (&self->lock);

  return has_unapplied;
}

/* {{{1 GSettingsBackend method calls */
static GVariant *
g_delayed_settings_backend_read_value (GSettingsBackend          *backend,
                                       const gchar               *key,
                                       GSettingsBackendReadFlags  flags,
                                       GQueue                    *read_through,
                                       const GVariantType        *expected_type)
{
  GDelayedSettingsBackend *self = G_DELAYED_SETTINGS_BACKEND (backend);
  GVariant *value;

  if (read_through == NULL)
    {
      read_through = g_newa (GQueue, 1);
      memset (read_through, 0, sizeof (GQueue));
    }

  g_mutex_lock (&self->lock);
  {
    GList link = { self->changeset, NULL, NULL };

    g_queue_push_head_link (read_through, &link);
    value = g_settings_backend_read_value (self->backend, key, flags, read_through, expected_type);
    g_queue_pop_head_link (read_through);
  }
  g_mutex_unlock (&self->lock);

  return value;
}

static gboolean
g_delayed_settings_backend_get_writable (GSettingsBackend *backend,
                                         const gchar      *name)
{
  GDelayedSettingsBackend *delayed = G_DELAYED_SETTINGS_BACKEND (backend);

  return g_settings_backend_get_writable (delayed->backend, name);
}

static gboolean
g_delayed_settings_backend_write (GSettingsBackend *backend,
                                  const gchar      *key,
                                  GVariant         *value,
                                  gpointer          origin_tag)
{
  GDelayedSettingsBackend *self = G_DELAYED_SETTINGS_BACKEND (backend);

  g_delayed_settings_backend_lock_for_write (self);

  g_settings_backend_changeset_set (self->changeset, key, value);

  g_delayed_settings_backend_unlock_for_write (self, key, NULL, origin_tag);

  return TRUE;
}

static void
g_delayed_settings_backend_reset (GSettingsBackend *backend,
                                  const gchar      *key,
                                  gpointer          origin_tag)
{
  GDelayedSettingsBackend *self = G_DELAYED_SETTINGS_BACKEND (backend);

  g_delayed_settings_backend_lock_for_write (self);

  g_settings_backend_changeset_set (self->changeset, key, NULL);

  g_delayed_settings_backend_unlock_for_write (self, key, NULL, origin_tag);
}

static gboolean
g_delayed_settings_backend_write_changeset (GSettingsBackend          *backend,
                                            GSettingsBackendChangeset *changeset,
                                            gpointer                   origin_tag)
{
  GDelayedSettingsBackend *self = G_DELAYED_SETTINGS_BACKEND (backend);

  g_delayed_settings_backend_lock_for_write (self);

  g_settings_backend_changeset_change (self->changeset, changeset);

  g_delayed_settings_backend_unlock_for_write (self, NULL, changeset, origin_tag);

  return TRUE;
}

static void
g_delayed_settings_backend_subscribe (GSettingsBackend *backend,
                                      const char       *name)
{
  GDelayedSettingsBackend *delayed = G_DELAYED_SETTINGS_BACKEND (backend);

  g_settings_backend_subscribe (delayed->backend, name);
}

static void
g_delayed_settings_backend_unsubscribe (GSettingsBackend *backend,
                                        const char       *name)
{
  GDelayedSettingsBackend *delayed = G_DELAYED_SETTINGS_BACKEND (backend);

  g_settings_backend_unsubscribe (delayed->backend, name);
}

/* {{{1 apply() and revert() method calls */
void
g_delayed_settings_backend_apply (GDelayedSettingsBackend *self)
{
  GSettingsBackendChangeset *changeset = NULL;

  g_delayed_settings_backend_lock_for_write (self);

  if (!g_settings_backend_changeset_is_empty (self->changeset))
    {
      changeset = self->changeset;
      self->changeset = g_settings_backend_changeset_new ();

      /* If the write is successful then we should not notify since our
       * children already see the new value and that is the value that
       * was just applied.
       *
       * In case of failure, we must notify so that they see the
       * previous (old) value again.
       */
      if (g_settings_backend_write_changeset (self->backend, changeset, self))
        g_clear_pointer (&changeset, g_settings_backend_changeset_unref);
    }

  g_delayed_settings_backend_unlock_for_write (self, NULL, changeset, NULL);

  g_clear_pointer (&changeset, g_settings_backend_changeset_unref);
}

void
g_delayed_settings_backend_revert (GDelayedSettingsBackend *self)
{
  GSettingsBackendChangeset *changeset = NULL;

  g_delayed_settings_backend_lock_for_write (self);

  if (!g_settings_backend_changeset_is_empty (self->changeset))
    {
      changeset = self->changeset;
      self->changeset = g_settings_backend_changeset_new ();
    }

  g_delayed_settings_backend_unlock_for_write (self, NULL, changeset, NULL);

  g_clear_pointer (&changeset, g_settings_backend_changeset_unref);
}

/* {{{1 change notification */
static void
delayed_backend_changed (GObject          *target,
                         GSettingsBackend *backend,
                         const gchar      *key,
                         gpointer          origin_tag)
{
  GDelayedSettingsBackend *self = G_DELAYED_SETTINGS_BACKEND (target);

  if (origin_tag != self)
    g_settings_backend_changed (G_SETTINGS_BACKEND (self), key, origin_tag);
}

static void
delayed_backend_keys_changed (GObject             *target,
                              GSettingsBackend    *backend,
                              const gchar         *path,
                              gpointer             origin_tag,
                              const gchar * const *items)
{
  GDelayedSettingsBackend *self = G_DELAYED_SETTINGS_BACKEND (target);

  if (origin_tag != self)
    g_settings_backend_keys_changed (G_SETTINGS_BACKEND (self), path, items, origin_tag);
}

static void
delayed_backend_path_changed (GObject          *target,
                              GSettingsBackend *backend,
                              const gchar      *path,
                              gpointer          origin_tag)
{
  GDelayedSettingsBackend *self = G_DELAYED_SETTINGS_BACKEND (target);

  if (origin_tag != self)
    g_settings_backend_path_changed (G_SETTINGS_BACKEND (self), path, origin_tag);
}

static void
delayed_backend_writable_changed (GObject          *target,
                                  GSettingsBackend *backend,
                                  const gchar      *key)
{
  GDelayedSettingsBackend *self = G_DELAYED_SETTINGS_BACKEND (target);

  g_settings_backend_writable_changed (G_SETTINGS_BACKEND (self), key);
}

static void
delayed_backend_path_writable_changed (GObject          *target,
                                       GSettingsBackend *backend,
                                       const gchar      *path)
{
  GDelayedSettingsBackend *self = G_DELAYED_SETTINGS_BACKEND (target);

  g_settings_backend_path_writable_changed (G_SETTINGS_BACKEND (self), path);
}

static void
g_delayed_settings_backend_finalize (GObject *object)
{
  GDelayedSettingsBackend *self = G_DELAYED_SETTINGS_BACKEND (object);

  g_settings_backend_changeset_unref (self->changeset);
  if (self->owner_context)
    g_main_context_unref (self->owner_context);
  g_object_unref (self->backend);
  g_mutex_clear (&self->lock);

  /* if our owner is still alive, why are we finalizing? */
  g_assert (g_weak_ref_get (&self->owner) == NULL);

  G_OBJECT_CLASS (g_delayed_settings_backend_parent_class)->finalize (object);
}

static void
g_delayed_settings_backend_class_init (GDelayedSettingsBackendClass *class)
{
  GSettingsBackendClass *backend_class = G_SETTINGS_BACKEND_CLASS (class);
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  backend_class->read_value = g_delayed_settings_backend_read_value;
  backend_class->write = g_delayed_settings_backend_write;
  backend_class->reset = g_delayed_settings_backend_reset;
  backend_class->write_changeset = g_delayed_settings_backend_write_changeset;
  backend_class->get_writable = g_delayed_settings_backend_get_writable;
  backend_class->subscribe = g_delayed_settings_backend_subscribe;
  backend_class->unsubscribe = g_delayed_settings_backend_unsubscribe;

  object_class->finalize = g_delayed_settings_backend_finalize;
}

static void
g_delayed_settings_backend_init (GDelayedSettingsBackend *self)
{
  self->changeset = g_settings_backend_changeset_new ();
  g_mutex_init (&self->lock);
}

GDelayedSettingsBackend *
g_delayed_settings_backend_new (GSettingsBackend *backend,
                                gpointer          owner,
                                GMainContext     *owner_context)
{
  static GSettingsListenerVTable vtable = {
    delayed_backend_changed,
    delayed_backend_path_changed,
    delayed_backend_keys_changed,
    delayed_backend_writable_changed,
    delayed_backend_path_writable_changed
  };
  GDelayedSettingsBackend *self;

  self = g_object_new (G_TYPE_DELAYED_SETTINGS_BACKEND, NULL);
  self->backend = g_object_ref (backend);

  if (owner_context)
    self->owner_context = g_main_context_ref (owner_context);

  g_weak_ref_init (&self->owner, owner);

  g_settings_backend_watch (self->backend, &vtable, G_OBJECT (self), NULL);

  return self;
}
