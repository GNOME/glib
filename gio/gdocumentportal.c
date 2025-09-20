/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright 2016 Endless Mobile, Inc.
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
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include "gdocumentportal.h"
#include "xdp-dbus.h"
#include "gstdio.h"

#ifdef G_OS_UNIX
#include "gunixfdlist.h"
#endif

#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#else
#define HAVE_O_CLOEXEC 1
#endif

static gboolean
get_document_portal (GXdpDocuments **documents,
                     char          **documents_mountpoint,
                     GError        **error)
{
  GDBusConnection *connection = NULL;

  *documents = NULL;
  *documents_mountpoint = NULL;

  connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, error);
  if (connection == NULL)
    {
      g_prefix_error (error, "Cannot connect to session bus when initializing document portal: ");
      goto out;
    }

  *documents = gxdp_documents_proxy_new_sync (connection,
                                              G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
                                              G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
                                              "org.freedesktop.portal.Documents",
                                              "/org/freedesktop/portal/documents",
                                              NULL, error);
  if (*documents == NULL)
    {
      g_prefix_error (error, "Cannot create document portal proxy: ");
      goto out;
    }

  if (!gxdp_documents_call_get_mount_point_sync (*documents,
                                                 documents_mountpoint,
                                                 NULL, error))
    {
      g_clear_object (documents);
      g_prefix_error (error, "Cannot get document portal mount point: ");
      goto out;
    }

out:
  g_clear_object (&connection);
  return *documents != NULL;
}

/* Flags accepted by org.freedesktop.portal.Documents.AddFull */
enum {
  XDP_ADD_FLAGS_REUSE_EXISTING             =  (1 << 0),
  XDP_ADD_FLAGS_PERSISTENT                 =  (1 << 1),
  XDP_ADD_FLAGS_AS_NEEDED_BY_APP           =  (1 << 2),
  XDP_ADD_FLAGS_FLAGS_ALL                  = ((1 << 3) - 1)
};

/*
 * Assume that opening a file read/write failed with @saved_errno,
 * and return TRUE if opening the same file read-only might succeed.
 */
static gboolean
opening_ro_might_succeed (int saved_errno)
{
  switch (saved_errno)
    {
    case EACCES:
    case EISDIR:
#ifdef EPERM
    case EPERM:
#endif
#ifdef EROFS
    case EROFS:
#endif
#ifdef ETXTBSY
    case ETXTBSY:
#endif
      return TRUE;

    default:
      return FALSE;
    }
}

GList *
g_document_portal_add_documents (GList       *uris,
                                 const char  *app_id,
                                 GError     **error)
{
  GXdpDocuments *documents = NULL;
  char *documents_mountpoint = NULL;
  int length;
  GList *ruris = NULL;
  gboolean *as_is;
  char **basenames;
  GVariantBuilder builder;
  GUnixFDList *fd_list = NULL;
  GList *l;
  gsize i, j;
  const char *permissions[] = { "read", "write", NULL };
  char **doc_ids = NULL;
  GVariant *extra_out = NULL;

  if (!get_document_portal (&documents, &documents_mountpoint, error))
    {
      return NULL;
    }

  length = g_list_length (uris);
  as_is = g_new0 (gboolean, length);
  basenames = g_new0 (char *, length);

  g_variant_builder_init_static (&builder, G_VARIANT_TYPE ("ah"));

  fd_list = g_unix_fd_list_new ();
  for (l = uris, i = 0; l; l = l->next, i++)
    {
      const char *uri = l->data;
      int idx = -1;
      char *path = NULL;

      path = g_filename_from_uri (uri, NULL, NULL);
      if (path != NULL)
        {
          int fd;

          fd = g_open (path, O_CLOEXEC | O_RDWR);
          if (fd == -1 && opening_ro_might_succeed (errno))
            {
              /* If we don't have write access, fall back to read-only,
               * and stop requesting the write permission */
              fd = g_open (path, O_CLOEXEC | O_RDONLY);
              permissions[1] = NULL;
            }
          if (fd >= 0)
            {
              char *fd_path;
              char *filename;
              char *basename;

#ifndef HAVE_O_CLOEXEC
              fcntl (fd, F_SETFD, FD_CLOEXEC);
#endif
              fd_path = g_strdup_printf ("/proc/self/fd/%d", fd);

              if ((filename = g_file_read_link (fd_path, NULL)))
                {
                  basename = g_path_get_basename (filename);
                  g_clear_pointer (&filename, g_free);
                }
              else
                {
                  basename = g_path_get_basename (uri + strlen ("file:"));
                }

              basenames[i] = g_steal_pointer (&basename);
              g_clear_pointer (&fd_path, g_free);

              idx = g_unix_fd_list_append (fd_list, fd, NULL);
              close (fd);
            }
        }

      g_free (path);

      if (idx != -1)
        g_variant_builder_add (&builder, "h", idx);
      else
        as_is[i] = TRUE;
    }

  if (g_unix_fd_list_get_length (fd_list) > 0)
    {
      if (!gxdp_documents_call_add_full_sync (documents,
                                              g_variant_builder_end (&builder),
                                              XDP_ADD_FLAGS_AS_NEEDED_BY_APP,
                                              app_id,
                                              permissions,
                                              fd_list,
                                              &doc_ids,
                                              &extra_out,
                                              NULL,
                                              NULL,
                                              error))
        goto out;

      for (l = uris, i = 0, j = 0; l; l = l->next, i++)
        {
          const char *uri = l->data;
          char *basename = g_steal_pointer (&basenames[i]);
          char *ruri;

          if (as_is[i]) /* use as-is, not a file uri */
            {
              ruri = g_strdup (uri);
            }
          else if (strcmp (doc_ids[j], "") == 0) /* not rewritten */
            {
              ruri = g_strdup (uri);
              j++;
            }
          else
            {
              char *doc_path = g_build_filename (documents_mountpoint, doc_ids[j],
                                                 basename, NULL);
              ruri = g_strconcat ("file:", doc_path, NULL);
              g_free (doc_path);
              j++;
            }

          ruris = g_list_prepend (ruris, ruri);

          g_free (basename);
        }

      ruris = g_list_reverse (ruris);
    }
  else
    {
      ruris = g_list_copy_deep (uris, (GCopyFunc)g_strdup, NULL);
      g_variant_builder_clear (&builder);
    }

out:
  g_clear_object (&documents);
  g_clear_pointer (&documents_mountpoint, g_free);
  g_clear_object (&fd_list);
  g_clear_pointer (&extra_out, g_variant_unref);
  g_clear_pointer (&doc_ids, g_strfreev);
  g_clear_pointer (&basenames, g_free);
  g_free (as_is);

  return ruris;
}
