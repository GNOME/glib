/* GObject - GLib Type, Object, Parameter and Signal Library
 * Copyright (C) 1998, 1999, 2000 Tim Janik and Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifndef __G_GOBJECT_H__
#define __G_GOBJECT_H__

#include	<gobject/gtype.h>
#include	<gobject/gvalue.h>
#include	<gobject/gparam.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/* --- type macros --- */
#define G_TYPE_IS_OBJECT(type)	   (G_TYPE_FUNDAMENTAL (type) == G_TYPE_OBJECT)
#define G_OBJECT(object)	   (G_IS_OBJECT (object) ? ((GObject*) (object)) : \
				    G_TYPE_CHECK_INSTANCE_CAST ((object), G_TYPE_OBJECT, GObject))
#define G_OBJECT_CLASS(class)	   (G_IS_OBJECT_CLASS (class) ? ((GObjectClass*) (class)) : \
				    G_TYPE_CHECK_CLASS_CAST ((class), G_TYPE_OBJECT, GObjectClass))
#define G_IS_OBJECT(object)	   (((GObject*) (object)) != NULL && \
				    G_IS_OBJECT_CLASS (((GTypeInstance*) (object))->g_class))
#define G_IS_OBJECT_CLASS(class)   (((GTypeClass*) (class)) != NULL && \
				    G_TYPE_IS_OBJECT (((GTypeClass*) (class))->g_type))
#define G_OBJECT_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS ((object), G_TYPE_OBJECT, GObjectClass))
#define G_OBJECT_TYPE(object)	   (G_TYPE_FROM_INSTANCE (object))
#define G_OBJECT_TYPE_NAME(object) (g_type_name (G_OBJECT_TYPE (object)))
#define G_OBJECT_CLASS_TYPE(class) (G_TYPE_FROM_CLASS (class))
#define G_OBJECT_CLASS_NAME(class) (g_type_name (G_OBJECT_CLASS_TYPE (class)))
#define G_IS_VALUE_OBJECT(value)   (G_TYPE_CHECK_CLASS_TYPE ((value), G_TYPE_OBJECT))

#define	G_NOTIFY_PRIORITY	   (G_PRIORITY_HIGH_IDLE + 20)


/* --- typedefs & structures --- */
typedef struct _GObject	     GObject;
typedef struct _GObjectClass GObjectClass;
typedef void (*GObjectGetParamFunc)	(GObject     *object,
					 guint	      param_id,
					 GValue	     *value,
					 GParamSpec  *pspec,
					 const gchar *trailer);
typedef void (*GObjectSetParamFunc)	(GObject     *object,
					 guint	      param_id,
					 GValue	     *value,
					 GParamSpec  *pspec,
					 const gchar *trailer);
typedef void (*GObjectFinalizeFunc)	(GObject     *object);
struct	_GObject
{
  GTypeInstance g_type_instance;
  
  /*< private >*/
  guint		ref_count;
  GData	       *qdata;
};
struct	_GObjectClass
{
  GTypeClass   g_type_class;

  guint	       n_param_specs;
  GParamSpec **param_specs;
  
  void	     (*get_param)		(GObject	*object,
					 guint		 param_id,
					 GValue		*value,
					 GParamSpec	*pspec,
					 const gchar	*trailer);
  void	     (*set_param)		(GObject	*object,
					 guint		 param_id,
					 GValue		*value,
					 GParamSpec	*pspec,
					 const gchar	*trailer);
  void	     (*queue_param_changed)	(GObject	*object,
					 GParamSpec	*pspec);
  void	     (*dispatch_param_changed)	(GObject	*object,
					 GParamSpec	*pspec);
  void	     (*shutdown)		(GObject	*object);
  void	     (*finalize)		(GObject	*object);
};


/* --- prototypes --- */
void	    g_object_class_install_param   (GObjectClass   *oclass,
					    guint	    param_id,
					    GParamSpec	   *pspec /* +1 ref */);
GParamSpec* g_object_class_find_param_spec (GObjectClass   *oclass,
					    const gchar	   *param_name);
gpointer    g_object_new		   (GType	    object_type,
					    const gchar	   *first_param_name,
					    ...);
gpointer    g_object_new_valist		   (GType	    object_type,
					    const gchar	   *first_param_name,
					    va_list	    var_args);
void	    g_object_set		   (GObject	   *object,
					    const gchar	   *first_param_name,
					    ...);
void	    g_object_get		   (GObject	   *object,
					    const gchar	   *first_param_name,
					    ...);
void	    g_object_set_valist		   (GObject	   *object,
					    const gchar	   *first_param_name,
					    va_list	    var_args);
void	    g_object_get_valist		   (GObject	   *object,
					    const gchar	   *first_param_name,
					    va_list	    var_args);
void	    g_object_set_param		   (GObject	   *object,
					    const gchar    *param_name,
					    const GValue   *value);
void	    g_object_get_param		   (GObject	   *object,
					    const gchar    *param_name,
					    GValue	   *value);
void	    g_object_queue_param_changed   (GObject	   *object,
					    const gchar	   *param_name);
GObject*    g_object_ref		   (GObject	   *object);
void	    g_object_unref		   (GObject	   *object);
gpointer    g_object_get_qdata		   (GObject	   *object,
					    GQuark	    quark);
void	    g_object_set_qdata		   (GObject	   *object,
					    GQuark	    quark,
					    gpointer	    data);
void	    g_object_set_qdata_full	   (GObject	   *object,
					    GQuark	    quark,
					    gpointer	    data,
					    GDestroyNotify  destroy);
gpointer    g_object_steal_qdata	   (GObject	   *object,
					    GQuark	    quark);
void        g_value_set_object		   (GValue         *value,
					    GObject        *v_object);
GObject*    g_value_get_object		   (GValue         *value);
GObject*    g_value_dup_object		   (GValue         *value);


/* --- implementation macros --- */
#define G_WARN_INVALID_PARAM_ID(object, param_id, pspec) \
G_STMT_START { \
  GObject *_object = (GObject*) (object); \
  GParamSpec *_pspec = (GParamSpec*) (pspec); \
  guint _param_id = (param_id); \
  g_warning ("%s: invalid parameter id %u for \"%s\" of type `%s' in `%s'", \
	     G_STRLOC, \
	     _param_id, \
	     _pspec->name, \
	     g_type_name (G_PARAM_SPEC_TYPE (_pspec)), \
	     BSE_OBJECT_TYPE_NAME (_object)); \
} G_STMT_END



#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __G_GOBJECT_H__ */
