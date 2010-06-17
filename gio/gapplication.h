/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright © 2010 Red Hat, Inc
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Colin Walters <walters@verbum.org>
 *          Emmanuele Bassi <ebassi@linux.intel.com>
 */

#if !defined (__GIO_GIO_H_INSIDE__) && !defined (GIO_COMPILATION)
#error "Only <gio/gio.h> can be included directly."
#endif

#ifndef __G_APPLICATION_H__
#define __G_APPLICATION_H__

#include <glib-object.h>
#include <gio/giotypes.h>

G_BEGIN_DECLS

#define G_TYPE_APPLICATION              (g_application_get_type ())
#define G_APPLICATION(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), G_TYPE_APPLICATION, GApplication))
#define G_APPLICATION_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), G_TYPE_APPLICATION, GApplicationClass))
#define G_IS_APPLICATION(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), G_TYPE_APPLICATION))
#define G_IS_APPLICATION_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), G_TYPE_APPLICATION))
#define G_APPLICATION_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), G_TYPE_APPLICATION, GApplicationClass))

typedef struct _GApplication            GApplication;
typedef struct _GApplicationPrivate     GApplicationPrivate;
typedef struct _GApplicationClass       GApplicationClass;

/**
 * GApplication:
 *
 * The <structname>GApplication</structname> structure contains private
 * data and should only be accessed using the provided API
 *
 * Since: 2.26
 */
struct _GApplication
{
  /*< private >*/
  GObject parent_instance;

  GApplicationPrivate *priv;
};

/**
 * GApplicationClass:
 * @action_with_data: class handler for the #GApplication::action-with-data signal
 * @quit_with_data: class handler for the #GApplication::quit-with-data signal
 * @prepare_activation: class handler for the #GApplication::prepare-activation signal
 * @run: virtual function, called by g_application_run()
 *
 * The <structname>GApplicationClass</structname> structure contains
 * private data only
 *
 * Since: 2.26
 */
struct _GApplicationClass
{
  /*< private >*/
  GObjectClass parent_class;

  /*< public >*/
  /* signals */
  void        (* action_with_data) (GApplication *application,
				    const gchar  *action_name,
				    GVariant     *platform_data);
  gboolean    (* quit_with_data)   (GApplication *application,
				    GVariant     *platform_data);
  void        (* prepare_activation)   (GApplication  *application,
                                        GVariant      *arguments,
                                        GVariant      *platform_data);

  /* vfuncs */
  void        (* run)    (GApplication *application);

  /*< private >*/
  /* Padding for future expansion */
  void (*_g_reserved1) (void);
  void (*_g_reserved2) (void);
  void (*_g_reserved3) (void);
  void (*_g_reserved4) (void);
  void (*_g_reserved5) (void);
  void (*_g_reserved6) (void);
};
GType                   g_application_get_type                  (void) G_GNUC_CONST;

GApplication *          g_application_new                       (const gchar      *appid,
								 int               argc,
								 char            **argv);

GApplication *          g_application_try_new                   (const gchar      *appid,
								 int               argc,
								 char            **argv,
								 GError          **error);

GApplication *          g_application_unregistered_try_new      (const gchar      *appid,
								 int               argc,
								 char            **argv,
								 GError          **error);

gboolean                g_application_register                  (GApplication      *application);

GApplication *          g_application_get_instance              (void);
G_CONST_RETURN gchar *  g_application_get_id                    (GApplication      *application);

void                    g_application_add_action                (GApplication      *application,
                                                                 const gchar       *name,
                                                                 const gchar       *description);
void                    g_application_remove_action             (GApplication      *application,
                                                                 const gchar       *name);
gchar **                g_application_list_actions              (GApplication      *application);
void                    g_application_set_action_enabled        (GApplication      *application,
                                                                 const gchar       *name,
                                                                 gboolean           enabled);
gboolean                g_application_get_action_enabled        (GApplication      *application,
                                                                 const gchar       *name);
G_CONST_RETURN gchar *  g_application_get_action_description    (GApplication      *application,
                                                                 const gchar       *name);
void                    g_application_invoke_action             (GApplication      *application,
                                                                 const gchar       *name,
                                                                 GVariant          *platform_data);

void                    g_application_run                       (GApplication      *application);
gboolean                g_application_quit_with_data            (GApplication      *application,
                                                                 GVariant          *platform_data);

gboolean                g_application_is_remote                 (GApplication      *application);

G_END_DECLS

#endif /* __G_APPLICATION_H__ */
