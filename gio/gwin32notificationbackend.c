/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright © 2018 Endless Mobile, Inc.
 * Copyright © 2025 Oscar Pernia <oscarperniamoreno@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see
 * <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *  - Philip Withnall <withnall@endlessm.com>
 *  - Oscar Pernia <oscarperniamoreno@gmail.com> - Initial implementation using legacy Shell_NotifyIcon API
 */

#include "config.h"

#include <windows.h>

#include "gnotificationbackend.h"

#include "giomodule-priv.h"
#include "gnotification-private.h"

#define G_TYPE_WIN32_NOTIFICATION_BACKEND  (g_win32_notification_backend_get_type ())
#define G_WIN32_NOTIFICATION_BACKEND(o)    (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_WIN32_NOTIFICATION_BACKEND, GWin32NotificationBackend))

extern IMAGE_DOS_HEADER __ImageBase;

static inline HMODULE
exe_module (void)
{
  return (HMODULE) GetModuleHandle (NULL);
}

static inline HMODULE
this_module (void)
{
  return (HMODULE) &__ImageBase;
}

static HWND hwnd;
static GMutex hwnd_mutex; /* Protects access to `hwnd`, `hwnd_refcount` and `wnd_klass` */
static grefcount hwnd_refcount;
static ATOM wnd_klass;

typedef struct _GWin32NotificationBackend GWin32NotificationBackend;
typedef GNotificationBackendClass         GWin32NotificationBackendClass;

struct _GWin32NotificationBackend
{
  GNotificationBackend parent;

  HWND hwnd;
};

GType g_win32_notification_backend_get_type (void);

G_DEFINE_TYPE_WITH_CODE (GWin32NotificationBackend, g_win32_notification_backend, G_TYPE_NOTIFICATION_BACKEND,
  _g_io_modules_ensure_extension_points_registered ();
  g_io_extension_point_implement (G_NOTIFICATION_BACKEND_EXTENSION_POINT_NAME,
                                  g_define_type_id, "win32", 0))

/* Maximum number of UTF-16 code units that can be written into
 * NOTIFYICONDATA.szInfoTitle array, it does not count one element
 * which is for the NUL-terminator */
#define MAX_TITLE_COUNT (G_N_ELEMENTS (((NOTIFYICONDATA *) 0)->szInfoTitle) - 1)

/* Maximum number of UTF-16 code units that can be written into
 * NOTIFYICONDATA.szInfo array, it does not count one element
 * which is for the NUL-terminator */
#define MAX_BODY_COUNT (G_N_ELEMENTS (((NOTIFYICONDATA *) 0)->szInfo) - 1)

static gboolean
g_win32_notification_backend_is_supported (void)
{
  /* This backend is always supported on Windows. */
  return TRUE;
}

/* Implementing send-and-forget notifications, only supporting W10/11 */
static void
g_win32_notification_backend_send_notification (GNotificationBackend *backend,
                                                const gchar          *id,
                                                GNotification        *notification)
{
  /* GIO users may expect notification actions to work, and expect the actions
   * to be activated, but because we cannot fullfil the request, it's appropriate
   * to warn about it at least once to let them know. */

  if (g_notification_get_n_buttons (notification) > 0)
    {
      g_warning_once ("Notification action buttons are unsupported in Windows");
    }

  /* FIXME: Couldn't make it to work, when clicking the notification no message
     is sent to the dummy_WndProc, I think MS just removed the feature and the removal
     it's just undocumented :-( it makes sense knowing this is a legacy API */
  if (g_notification_get_default_action (notification, NULL, NULL))
    {
      g_warning_once ("Notification default action is unsupported in Windows");
    }

  /* NOTE: Icon is unsupported for W10 or greater, trying to set an icon will
   * make the notification to not show up */
  /* NOTE: There is no category */
  /* NOTE: There is no priority */

  GWin32NotificationBackend *self = G_WIN32_NOTIFICATION_BACKEND (backend);

  /* Return early if backend's initialization failed */
  if (!self->hwnd)
    return;

  NOTIFYICONDATA notify_singleton = {
    .cbSize = sizeof (NOTIFYICONDATA),
    .hWnd = self->hwnd,
    .uFlags = NIF_INFO,
  };

  GError *error = NULL;
  glong items_written;

  const gchar *title_utf8 = g_notification_get_title (notification);
  WCHAR *title_utf16 = g_utf8_to_utf16 (title_utf8, -1, NULL, &items_written, &error);
  if (error)
    {
      g_critical ("Invalid UTF-8 in notification: %s", error->message);
      g_error_free (error);
      return; /* Cannot show a notification without a title */
    }

  if (items_written > (glong) MAX_TITLE_COUNT)
    {
      g_warning ("Notification title too long, truncating title");
      items_written = MAX_TITLE_COUNT;
      if (IS_LOW_SURROGATE (title_utf16[items_written]))
        items_written--;
    }

  if (items_written == 0)
    {
      g_critical ("Notification title is empty");
      g_free (title_utf16);
      return; /* Cannot show a notification without a title */
    }

  memcpy (&notify_singleton.szInfoTitle, title_utf16, items_written * sizeof (WCHAR));
  notify_singleton.szInfoTitle[items_written] = L'\0';
  g_free (title_utf16);

  const gchar *body_utf8 = g_notification_get_body (notification);
  if (!body_utf8 || !*body_utf8)
    {
      /* If you pass an empty body, Shell_NotifyIcon won't show the notification,
       * in order to fix this, here we are setting a string with a single
       * SPACE (' ') character, which makes the body look like empty */
      notify_singleton.szInfo[0] = L' ';
      notify_singleton.szInfo[1] = L'\0';
    }
  else
    {
      WCHAR *body_utf16 = g_utf8_to_utf16 (body_utf8, -1, NULL, &items_written, &error);
      if (error)
        {
          g_critical ("Invalid UTF-8 in notification: %s", error->message);
          g_clear_pointer (&error, g_error_free);
        }
      else
        {
          if (items_written > (glong) MAX_BODY_COUNT)
            {
              g_warning ("Notification body too long, truncating body");
              items_written = MAX_BODY_COUNT;
              if (IS_LOW_SURROGATE (body_utf16[items_written]))
                items_written--;
            }

          memcpy (&notify_singleton.szInfo, body_utf16, items_written * sizeof (WCHAR));
          notify_singleton.szInfo[items_written] = L'\0';
          g_free (body_utf16);
        }
    }

  Shell_NotifyIcon (NIM_MODIFY, &notify_singleton);
}

static void
g_win32_notification_backend_withdraw_notification (GNotificationBackend *backend,
                                                    const gchar          *id)
{
}

static void
g_win32_notification_backend_dispose (GObject *self)
{
  GWin32NotificationBackend *backend = G_WIN32_NOTIFICATION_BACKEND (self);

  g_mutex_lock (&hwnd_mutex);

  if (backend->hwnd && g_ref_count_dec (&hwnd_refcount))
    {
      DestroyWindow (hwnd);
      hwnd = NULL;
      UnregisterClass (MAKEINTATOM (wnd_klass), this_module ());
      wnd_klass = 0;
    }

  backend->hwnd = NULL;

  g_mutex_unlock (&hwnd_mutex);

  G_OBJECT_CLASS (g_win32_notification_backend_parent_class)->dispose (self);
}

static LRESULT CALLBACK
dummy_WndProc (HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
  return DefWindowProc (hwnd, message, wparam, lparam);
}

static void
g_win32_notification_backend_init (GWin32NotificationBackend *backend)
{
  g_mutex_lock (&hwnd_mutex);

  if (hwnd)
    {
      g_ref_count_inc (&hwnd_refcount);
    }
  else
    {
      /* Shell_NotifyIcon API requires a window, we create a hidden one */
      WNDCLASS wclass = {
        .lpszClassName = L"GWin32NotificationBackend",
        .hInstance = this_module (),
        .lpfnWndProc = dummy_WndProc,
      };

      wnd_klass = RegisterClass (&wclass);
      if (!wnd_klass)
        {
          g_critical ("win32-notification: RegisterClass failed: %ld", GetLastError ());
          goto err_out;
        }

      hwnd = CreateWindow (MAKEINTATOM (wnd_klass), NULL, WS_POPUP,
                           0, 0, 0, 0, NULL, NULL, this_module (), NULL);

      if (!hwnd)
        {
          g_critical ("win32-notification: CreateWindow failed: %ld", GetLastError ());
          UnregisterClass (MAKEINTATOM (wnd_klass), this_module ());
          wnd_klass = 0;
          goto err_out;
        }

      NOTIFYICONDATA notify_singleton = {
        .cbSize = sizeof (NOTIFYICONDATA),
        .hWnd = hwnd,
        .uVersion = NOTIFYICON_VERSION_4,
        .uFlags = NIF_ICON,
        .hIcon = LoadIcon (exe_module (), MAKEINTRESOURCE (1))
      };

      if (!notify_singleton.hIcon)
        {
          /* Fallback if the application has no icon */
          notify_singleton.hIcon = LoadIcon (NULL, IDI_APPLICATION);
        }

      if (!Shell_NotifyIcon (NIM_ADD, &notify_singleton))
        {
          g_critical ("win32-notification: Failed to create NotifyIcon singleton");
          DestroyWindow (hwnd);
          hwnd = NULL;
          UnregisterClass (MAKEINTATOM (wnd_klass), this_module ());
          wnd_klass = 0;
          goto err_out;
        }

      /* FIXME: This also may fail, but currently we are not using VERSION_4 or greater features */
      Shell_NotifyIcon (NIM_SETVERSION, &notify_singleton);

      g_ref_count_init (&hwnd_refcount);
    }

  backend->hwnd = hwnd;

err_out:
  g_mutex_unlock (&hwnd_mutex);
}

static void
g_win32_notification_backend_class_init (GWin32NotificationBackendClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GNotificationBackendClass *backend_class = G_NOTIFICATION_BACKEND_CLASS (class);

  object_class->dispose = g_win32_notification_backend_dispose;

  backend_class->is_supported = g_win32_notification_backend_is_supported;
  backend_class->send_notification = g_win32_notification_backend_send_notification;
  backend_class->withdraw_notification = g_win32_notification_backend_withdraw_notification;
}
