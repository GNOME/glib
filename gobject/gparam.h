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
 * +++ OUTDATED +++
 *
 * class functions may not evaluate param->pspec directly,
 * instead, pspec will be passed as argument if required.
 *
 * void	param_init (GParam *param, GParamSpec *pspec):
 *	initialize param's value to default if pspec is given,
 *	and to zero-equivalent (a value that doesn't need to be
 *	free()ed later on) otherwise.
 *
 * void param_free_value (GParam *param):
 *	free param's value if required, zero-reinitialization
 *	of the value is not required. (this class function
 *	may be NULL for param types that don't need to free
 *	values, such as ints or floats).
 *
 * gboolean param_validate (GParam *param, GParamSpec *pspec):
 *	modify param's value in the least destructive way, so
 *	that it complies with pspec's requirements (i.e.
 *	according to minimum/maximum ranges etc...). return
 *	whether modification was necessary.
 *
 * gint param_values_cmp (GParam *param1, GParam *param2, GParamSpec*):
 *	return param1 - param2, i.e. <0 if param1 < param2,
 *	>0 if param1 > param2, and 0 if they are equal
 *	(passing pspec is optional, but recommended)
 *
 * void param_copy_value (GParam *param_src, GParam *param_dest):
 *	copy value from param_src to param_dest, param_dest is
 *	already free()d and zero-initialized, so its value can
 *	simply be overwritten. (may be NULL for memcpy)
 *
 * gchar* param_collect_value ():
 *	class function may be NULL.
 *
 * gchar* param_lcopy_value ():
 *	class function may be NULL.
 *
 */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __G_PARAM_H__ */
