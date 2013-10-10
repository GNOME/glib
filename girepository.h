/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 * GObject introspection: Repository
 *
 * Copyright (C) 2005 Matthias Clasen
 * Copyright (C) 2008,2009 Red Hat, Inc.
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

#ifndef __G_IREPOSITORY_H__
#define __G_IREPOSITORY_H__

#include <glib-object.h>
#include <gmodule.h>

#define __GIREPOSITORY_H_INSIDE__

#include <giarginfo.h>
#include <gibaseinfo.h>
#include <gicallableinfo.h>
#include <giconstantinfo.h>
#include <gienuminfo.h>
#include <gifieldinfo.h>
#include <gifunctioninfo.h>
#include <giinterfaceinfo.h>
#include <giobjectinfo.h>
#include <gipropertyinfo.h>
#include <giregisteredtypeinfo.h>
#include <gisignalinfo.h>
#include <gistructinfo.h>
#include <gitypeinfo.h>
#include <gitypelib.h>
#include <gitypes.h>
#include <giunioninfo.h>
#include <givfuncinfo.h>

G_BEGIN_DECLS

#define G_TYPE_IREPOSITORY              (g_irepository_get_type ())
#define G_IREPOSITORY(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), G_TYPE_IREPOSITORY, GIRepository))
#define G_IREPOSITORY_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), G_TYPE_IREPOSITORY, GIRepositoryClass))
#define G_IS_IREPOSITORY(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), G_TYPE_IREPOSITORY))
#define G_IS_IREPOSITORY_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), G_TYPE_IREPOSITORY))
#define G_IREPOSITORY_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), G_TYPE_IREPOSITORY, GIRepositoryClass))

/**
 * GIRepository:
 *
 * The GIRepository structure contains private data and should only be
 * accessed using the provided API.
 */
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
 * @G_IREPOSITORY_LOAD_FLAG_LAZY: Lazily load the typelib.
 *
 * Flags that control how a typelib is loaded.
 */
typedef enum
{
  G_IREPOSITORY_LOAD_FLAG_LAZY = 1 << 0
} GIRepositoryLoadFlags;

/* Repository */

GType         g_irepository_get_type      (void) G_GNUC_CONST;
GIRepository *g_irepository_get_default   (void);
void          g_irepository_prepend_search_path (const char *directory);
void          g_irepository_prepend_library_path (const char *directory);
GSList *      g_irepository_get_search_path     (void);
const char *  g_irepository_load_typelib  (GIRepository *repository,
					   GITypelib     *typelib,
					   GIRepositoryLoadFlags flags,
					   GError      **error);
gboolean      g_irepository_is_registered (GIRepository *repository,
					   const gchar  *namespace_,
					   const gchar  *version);
GIBaseInfo *  g_irepository_find_by_name  (GIRepository *repository,
					   const gchar  *namespace_,
					   const gchar  *name);
GList *       g_irepository_enumerate_versions (GIRepository *repository,
					        const gchar  *namespace_);
GITypelib *    g_irepository_require       (GIRepository *repository,
					   const gchar  *namespace_,
					   const gchar  *version,
					   GIRepositoryLoadFlags flags,
					   GError      **error);
GITypelib *    g_irepository_require_private (GIRepository  *repository,
					     const gchar   *typelib_dir,
					     const gchar   *namespace_,
					     const gchar   *version,
					     GIRepositoryLoadFlags flags,
					     GError       **error);
gchar      ** g_irepository_get_dependencies (GIRepository *repository,
					      const gchar  *namespace_);
gchar      ** g_irepository_get_loaded_namespaces (GIRepository *repository);
GIBaseInfo *  g_irepository_find_by_gtype (GIRepository *repository,
					   GType         gtype);
gint          g_irepository_get_n_infos   (GIRepository *repository,
					   const gchar  *namespace_);
GIBaseInfo *  g_irepository_get_info      (GIRepository *repository,
					   const gchar  *namespace_,
					   gint          index);
GIEnumInfo *  g_irepository_find_by_error_domain (GIRepository *repository,
						  GQuark        domain);
const gchar * g_irepository_get_typelib_path   (GIRepository *repository,
						const gchar  *namespace_);
const gchar * g_irepository_get_shared_library (GIRepository *repository,
						const gchar  *namespace_);
const gchar * g_irepository_get_c_prefix (GIRepository *repository,
                                          const gchar  *namespace_);
const gchar * g_irepository_get_version (GIRepository *repository,
					 const gchar  *namespace_);

GOptionGroup * g_irepository_get_option_group (void);

gboolean       g_irepository_dump  (const char *arg, GError **error);

/**
 * GIRepositoryError:
 * @G_IREPOSITORY_ERROR_TYPELIB_NOT_FOUND: the typelib could not be found.
 * @G_IREPOSITORY_ERROR_NAMESPACE_MISMATCH: the namespace does not match the
 *   requested namespace.
 * @G_IREPOSITORY_ERROR_NAMESPACE_VERSION_CONFLICT: the version of the
 *   typelib does not match the requested version.
 * @G_IREPOSITORY_ERROR_LIBRARY_NOT_FOUND: the library used by the typelib
 *   could not be found.
 *
 * An error code used with #G_IREPOSITORY_ERROR in a #GError returned
 * from a #GIRepository routine.
 */
typedef enum
{
  G_IREPOSITORY_ERROR_TYPELIB_NOT_FOUND,
  G_IREPOSITORY_ERROR_NAMESPACE_MISMATCH,
  G_IREPOSITORY_ERROR_NAMESPACE_VERSION_CONFLICT,
  G_IREPOSITORY_ERROR_LIBRARY_NOT_FOUND
} GIRepositoryError;

/**
 * G_IREPOSITORY_ERROR:
 *
 * Error domain for #GIRepository. Errors in this domain will be from the
 * #GIRepositoryError enumeration. See #GError for more information on
 * error domains.
 */
#define G_IREPOSITORY_ERROR (g_irepository_error_quark ())

GQuark g_irepository_error_quark (void);


/* Global utility functions */

void gi_cclosure_marshal_generic (GClosure       *closure,
                                  GValue         *return_gvalue,
                                  guint           n_param_values,
                                  const GValue   *param_values,
                                  gpointer        invocation_hint,
                                  gpointer        marshal_data);

G_END_DECLS


#endif  /* __G_IREPOSITORY_H__ */

