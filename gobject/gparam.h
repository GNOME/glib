/* GObject - GLib Type, Object, Parameter and Signal Library
 * Copyright (C) 1997, 1998, 1999, 2000 Tim Janik and Red Hat, Inc.
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
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * gparam.h: GParamSpec base class implementation
 */
#ifndef __G_PARAM_H__
#define __G_PARAM_H__


#include	<gobject/gvalue.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/* --- type macros --- */
#define G_TYPE_IS_PARAM(type)		(G_TYPE_FUNDAMENTAL (type) == G_TYPE_PARAM)
#define G_PARAM_SPEC_TYPE(pspec)	(G_TYPE_FROM_INSTANCE (pspec))
#define G_PARAM_SPEC_TYPE_NAME(pspec)	(g_type_name (G_PARAM_SPEC_TYPE (pspec)))
#define G_PARAM_SPEC(pspec)		(G_TYPE_CHECK_INSTANCE_CAST ((pspec), G_TYPE_PARAM, GParamSpec))
#define G_IS_PARAM_SPEC(pspec)		(G_TYPE_CHECK_INSTANCE_TYPE ((pspec), G_TYPE_PARAM))
#define G_PARAM_SPEC_GET_CLASS(pspec)	(G_TYPE_INSTANCE_GET_CLASS ((pspec), G_TYPE_PARAM, GParamSpecClass))
#define	G_IS_PARAM_VALUE(pspec, value)	(g_type_is_a (G_VALUE_TYPE (value), G_PARAM_SPEC_VALUE_TYPE (pspec))) /* FIXME */
#define	G_PARAM_SPEC_VALUE_TYPE(pspec)	(G_PARAM_SPEC_GET_CLASS (pspec)->value_type)


/* --- flags --- */
typedef enum
{
  G_PARAM_READABLE            = 1 << 0,
  G_PARAM_WRITABLE            = 1 << 1,
  G_PARAM_MASK                = 0x000f,
  /* bits in the range 0xfff0 are reserved for 3rd party usage */
  G_PARAM_USER_MASK           = 0xfff0
} GParamFlags;


/* --- typedefs & structures --- */
typedef struct _GParamSpecClass GParamSpecClass;
typedef struct _GParamSpec      GParamSpec;
struct _GParamSpecClass
{
  GTypeClass      g_type_class;

  GType		  value_type;

  void	        (*finalize)		(GParamSpec   *pspec);

  /* GParam methods */
  void          (*value_set_default)    (GParamSpec   *pspec,
					 GValue       *value);
  gboolean      (*value_validate)       (GParamSpec   *pspec,
					 GValue       *value);
  gint          (*values_cmp)           (GParamSpec   *pspec,
					 const GValue *value1,
					 const GValue *value2);
};
struct _GParamSpec
{
  GTypeInstance  g_instance;

  gchar         *name;
  gchar         *nick;
  gchar         *blurb;
  GParamFlags    flags;

  /*< private >*/
  GType		 owner_type;
  GData		*qdata;
  guint          ref_count;
};


/* --- prototypes --- */
GParamSpec*	g_param_spec_ref		(GParamSpec    *pspec);
void		g_param_spec_unref		(GParamSpec    *pspec);
gpointer        g_param_spec_get_qdata		(GParamSpec    *pspec,
						 GQuark         quark);
void            g_param_spec_set_qdata		(GParamSpec    *pspec,
						 GQuark         quark,
						 gpointer       data);
void            g_param_spec_set_qdata_full	(GParamSpec    *pspec,
						 GQuark         quark,
						 gpointer       data,
						 GDestroyNotify destroy);
gpointer        g_param_spec_steal_qdata	(GParamSpec    *pspec,
						 GQuark         quark);
void		g_param_value_set_default	(GParamSpec    *pspec,
						 GValue	       *value);
gboolean	g_param_value_defaults		(GParamSpec    *pspec,
						 GValue	       *value);
gboolean	g_param_value_validate		(GParamSpec    *pspec,
						 GValue	       *value);
gint		g_param_values_cmp		(GParamSpec    *pspec,
						 const GValue  *value1,
						 const GValue  *value2);


/* --- convenience functions --- */
typedef struct _GParamSpecTypeInfo GParamSpecTypeInfo;
struct _GParamSpecTypeInfo
{
  /* type system portion */
  guint16         instance_size;                               /* obligatory */
  guint16         n_preallocs;                                 /* optional */
  void		(*instance_init)	(GParamSpec   *pspec); /* optional */

  /* class portion */
  GType           value_type;				       /* obligatory */
  void          (*finalize)             (GParamSpec   *pspec); /* optional */
  void          (*value_set_default)    (GParamSpec   *pspec,  /* recommended */
					 GValue       *value);
  gboolean      (*value_validate)       (GParamSpec   *pspec,  /* optional */
					 GValue       *value);
  gint          (*values_cmp)           (GParamSpec   *pspec,  /* recommended */
					 const GValue *value1,
					 const GValue *value2);
};
GType	g_param_type_register_static	(const gchar		  *name,
					 const GParamSpecTypeInfo *pspec_info);


/* --- private --- */
gpointer	g_param_spec_internal		(GType	        param_type,
						 const gchar   *name,
						 const gchar   *nick,
						 const gchar   *blurb,
						 GParamFlags    flags);
GHashTable*	g_param_spec_hash_table_new	(void);
void		g_param_spec_hash_table_insert	(GHashTable    *hash_table,
						 GParamSpec    *pspec,
						 GType		owner_type);
void		g_param_spec_hash_table_remove	(GHashTable    *hash_table,
						 GParamSpec    *pspec);
GParamSpec*	g_param_spec_hash_table_lookup	(GHashTable    *hash_table,
						 const gchar   *param_name,
						 GType		owner_type,
						 gboolean       try_ancestors,
						 const gchar  **trailer);


/* contracts:
 *
 * gboolean value_validate (GParamSpec *pspec,
 *                          GValue     *value):
 *	modify value contents in the least destructive way, so
 *	that it complies with pspec's requirements (i.e.
 *	according to minimum/maximum ranges etc...). return
 *	whether modification was necessary.
 *
 * gint values_cmp (GParamSpec   *pspec,
 *                  const GValue *value1,
 *                  const GValue *value2):
 *	return value1 - value2, i.e. <0 if value1 < value2,
 *	>0 if value1 > value2, and 0 otherwise (they are equal)
 */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __G_PARAM_H__ */
