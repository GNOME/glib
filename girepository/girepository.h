/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 * GObject introspection: Repository
 *
 * Copyright (C) 2005 Matthias Clasen
 * Copyright (C) 2008,2009 Red Hat, Inc.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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

#pragma once

#include <glib-object.h>
#include <gmodule.h>

#include <girepository/gi-visibility.h>

#define __GIREPOSITORY_H_INSIDE__

#include <girepository/giarginfo.h>
#include <girepository/gibaseinfo.h>
#include <girepository/gicallableinfo.h>
#include <girepository/gicallbackinfo.h>
#include <girepository/giconstantinfo.h>
#include <girepository/gienuminfo.h>
#include <girepository/gifieldinfo.h>
#include <girepository/gifunctioninfo.h>
#include <girepository/giinterfaceinfo.h>
#include <girepository/giobjectinfo.h>
#include <girepository/gipropertyinfo.h>
#include <girepository/giregisteredtypeinfo.h>
#include <girepository/gisignalinfo.h>
#include <girepository/gistructinfo.h>
#include <girepository/gitypes.h>
#include <girepository/giunioninfo.h>
#include <girepository/giunresolvedinfo.h>
#include <girepository/givfuncinfo.h>
#include <girepository/gitypeinfo.h>
#include <gitypelib/gitypelib.h>

G_BEGIN_DECLS

#define GI_TYPE_REPOSITORY              (gi_repository_get_type ())
#define GI_REPOSITORY(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GI_TYPE_REPOSITORY, GIRepository))
#define GI_REPOSITORY_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GI_TYPE_REPOSITORY, GIRepositoryClass))
#define GI_IS_REPOSITORY(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GI_TYPE_REPOSITORY))
#define GI_IS_REPOSITORY_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GI_TYPE_REPOSITORY))
#define GI_REPOSITORY_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GI_TYPE_REPOSITORY, GIRepositoryClass))

typedef struct _GIRepository         GIRepository;
typedef struct _GIRepositoryClass    GIRepositoryClass;
typedef struct _GIRepositoryPrivate  GIRepositoryPrivate;

struct _GIRepository
{
  /*< private >*/
  GObject parent;
  GIRepositoryPrivate *priv;
};

struct _GIRepositoryClass
{
  /*< private >*/
  GObjectClass parent;
};

/**
 * GIRepositoryLoadFlags:
 * @GI_REPOSITORY_LOAD_FLAG_LAZY: Lazily load the typelib.
 *
 * Flags that control how a typelib is loaded.
 *
 * Since: 2.80
 */
typedef enum
{
  GI_REPOSITORY_LOAD_FLAG_LAZY = 1 << 0
} GIRepositoryLoadFlags;

/* Repository */

GI_AVAILABLE_IN_ALL
GType         gi_repository_get_type      (void) G_GNUC_CONST;

GI_AVAILABLE_IN_ALL
GIRepository *gi_repository_get_default   (void);

GI_AVAILABLE_IN_ALL
GIRepository *gi_repository_new (void);

GI_AVAILABLE_IN_ALL
void          gi_repository_prepend_search_path (const char *directory);

GI_AVAILABLE_IN_ALL
void          gi_repository_prepend_library_path (const char *directory);

GI_AVAILABLE_IN_ALL
const char * const * gi_repository_get_search_path (size_t *n_paths_out);

GI_AVAILABLE_IN_ALL
const char *  gi_repository_load_typelib  (GIRepository           *repository,
                                           GITypelib              *typelib,
                                           GIRepositoryLoadFlags   flags,
                                           GError                **error);

GI_AVAILABLE_IN_ALL
gboolean      gi_repository_is_registered (GIRepository *repository,
                                           const gchar  *namespace_,
                                           const gchar  *version);

GI_AVAILABLE_IN_ALL
GIBaseInfo *  gi_repository_find_by_name  (GIRepository *repository,
                                           const gchar  *namespace_,
                                           const gchar  *name);

GI_AVAILABLE_IN_ALL
char       ** gi_repository_enumerate_versions (GIRepository *repository,
                                                const gchar  *namespace_,
                                                size_t       *n_versions_out);

GI_AVAILABLE_IN_ALL
GITypelib *    gi_repository_require       (GIRepository           *repository,
                                            const gchar            *namespace_,
                                            const gchar            *version,
                                            GIRepositoryLoadFlags   flags,
                                            GError                **error);

GI_AVAILABLE_IN_ALL
GITypelib *    gi_repository_require_private (GIRepository           *repository,
                                              const gchar            *typelib_dir,
                                              const gchar            *namespace_,
                                              const gchar            *version,
                                              GIRepositoryLoadFlags   flags,
                                              GError                **error);

GI_AVAILABLE_IN_ALL
gchar      ** gi_repository_get_immediate_dependencies (GIRepository *repository,
                                                        const gchar  *namespace_);

GI_AVAILABLE_IN_ALL
gchar      ** gi_repository_get_dependencies (GIRepository *repository,
                                              const gchar  *namespace_);

GI_AVAILABLE_IN_ALL
gchar      ** gi_repository_get_loaded_namespaces (GIRepository *repository);

GI_AVAILABLE_IN_ALL
GIBaseInfo *  gi_repository_find_by_gtype (GIRepository *repository,
                                           GType         gtype);

GI_AVAILABLE_IN_ALL
void          gi_repository_get_object_gtype_interfaces (GIRepository      *repository,
                                                         GType              gtype,
                                                         gsize             *n_interfaces_out,
                                                         GIInterfaceInfo ***interfaces_out);

GI_AVAILABLE_IN_ALL
guint         gi_repository_get_n_infos   (GIRepository *repository,
                                           const gchar  *namespace_);

GI_AVAILABLE_IN_ALL
GIBaseInfo *  gi_repository_get_info      (GIRepository *repository,
                                           const gchar  *namespace_,
                                           guint         idx);

GI_AVAILABLE_IN_ALL
GIEnumInfo *  gi_repository_find_by_error_domain (GIRepository *repository,
                                                  GQuark        domain);

GI_AVAILABLE_IN_ALL
const gchar * gi_repository_get_typelib_path   (GIRepository *repository,
                                                const gchar  *namespace_);
GI_AVAILABLE_IN_ALL
const gchar * gi_repository_get_shared_library (GIRepository *repository,
                                                const gchar  *namespace_);
GI_AVAILABLE_IN_ALL
const gchar * gi_repository_get_c_prefix (GIRepository *repository,
                                          const gchar  *namespace_);
GI_AVAILABLE_IN_ALL
const gchar * gi_repository_get_version (GIRepository *repository,
                                         const gchar  *namespace_);


GI_AVAILABLE_IN_ALL
GOptionGroup * gi_repository_get_option_group (void);


GI_AVAILABLE_IN_ALL
gboolean       gi_repository_dump  (const char  *input_filename,
                                    const char  *output_filename,
                                    GError     **error);

/**
 * GIRepositoryError:
 * @GI_REPOSITORY_ERROR_TYPELIB_NOT_FOUND: the typelib could not be found.
 * @GI_REPOSITORY_ERROR_NAMESPACE_MISMATCH: the namespace does not match the
 *   requested namespace.
 * @GI_REPOSITORY_ERROR_NAMESPACE_VERSION_CONFLICT: the version of the
 *   typelib does not match the requested version.
 * @GI_REPOSITORY_ERROR_LIBRARY_NOT_FOUND: the library used by the typelib
 *   could not be found.
 *
 * An error code used with `GI_REPOSITORY_ERROR` in a [type@GLib.Error]
 * returned from a [class@GIRepository.Repository] routine.
 *
 * Since: 2.80
 */
typedef enum
{
  GI_REPOSITORY_ERROR_TYPELIB_NOT_FOUND,
  GI_REPOSITORY_ERROR_NAMESPACE_MISMATCH,
  GI_REPOSITORY_ERROR_NAMESPACE_VERSION_CONFLICT,
  GI_REPOSITORY_ERROR_LIBRARY_NOT_FOUND
} GIRepositoryError;

/**
 * GI_REPOSITORY_ERROR:
 *
 * Error domain for [class@GIRepository.Repository].
 *
 * Errors in this domain will be from the [enum@GIRepository.Error] enumeration.
 * See [type@GLib.Error] for more information on error domains.
 *
 * Since: 2.80
 */
#define GI_REPOSITORY_ERROR (gi_repository_error_quark ())

GI_AVAILABLE_IN_ALL
GQuark gi_repository_error_quark (void);


/* Global utility functions */

GI_AVAILABLE_IN_ALL
void gi_cclosure_marshal_generic (GClosure       *closure,
                                  GValue         *return_gvalue,
                                  guint           n_param_values,
                                  const GValue   *param_values,
                                  gpointer        invocation_hint,
                                  gpointer        marshal_data);

G_END_DECLS
