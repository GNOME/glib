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
 */
#include	"gparam.h"


#include	<string.h>



/* --- defines --- */
#define G_PARAM_SPEC_CLASS(class)    (G_TYPE_CHECK_CLASS_CAST ((class), G_TYPE_PARAM, GParamSpecClass))


/* --- prototypes --- */
static void	g_param_spec_class_base_init	 (GParamSpecClass	*class);
static void	g_param_spec_class_base_finalize (GParamSpecClass	*class);
static void	g_param_spec_class_init		 (GParamSpecClass	*class,
						  gpointer               class_data);
static void	g_param_spec_init		 (GParamSpec		*pspec);
static void	g_param_spec_finalize		 (GParamSpec		*pspec);


/* --- functions --- */
void
g_param_type_init (void)	/* sync with gtype.c */
{
  static const GTypeFundamentalInfo finfo = {
    (G_TYPE_FLAG_CLASSED |
     G_TYPE_FLAG_INSTANTIATABLE |
     G_TYPE_FLAG_DERIVABLE |
     G_TYPE_FLAG_DEEP_DERIVABLE),
  };
  static const GTypeInfo param_spec_info = {
    sizeof (GParamSpecClass),

    (GBaseInitFunc) g_param_spec_class_base_init,
    (GBaseFinalizeFunc) g_param_spec_class_base_finalize,
    (GClassInitFunc) g_param_spec_class_init,
    (GClassFinalizeFunc) NULL,
    NULL,	/* class_data */

    sizeof (GParamSpec),
    0,		/* n_preallocs */
    (GInstanceInitFunc) g_param_spec_init,

    NULL,	/* value_table */
  };
  GType type;

  type = g_type_register_fundamental (G_TYPE_PARAM, "GParam", &param_spec_info, &finfo, G_TYPE_FLAG_ABSTRACT);
  g_assert (type == G_TYPE_PARAM);
}

static void
g_param_spec_class_base_init (GParamSpecClass *class)
{
}

static void
g_param_spec_class_base_finalize (GParamSpecClass *class)
{
}

static void
g_param_spec_class_init (GParamSpecClass *class,
			 gpointer         class_data)
{
  class->value_type = G_TYPE_NONE;
  class->finalize = g_param_spec_finalize;
  class->value_set_default = NULL;
  class->value_validate = NULL;
  class->values_cmp = NULL;
}

static void
g_param_spec_init (GParamSpec *pspec)
{
  pspec->name = NULL;
  pspec->nick = NULL;
  pspec->blurb = NULL;
  pspec->flags = 0;
  pspec->owner_type = 0;
  pspec->qdata = NULL;
  pspec->ref_count = 1;
}

static void
g_param_spec_finalize (GParamSpec *pspec)
{
  g_datalist_clear (&pspec->qdata);
  
  g_free (pspec->name);
  g_free (pspec->nick);
  g_free (pspec->blurb);

  g_type_free_instance ((GTypeInstance*) pspec);
}

GParamSpec*
g_param_spec_ref (GParamSpec *pspec)
{
  g_return_val_if_fail (G_IS_PARAM_SPEC (pspec), NULL);
  g_return_val_if_fail (pspec->ref_count > 0, NULL);

  pspec->ref_count += 1;

  return pspec;
}

void
g_param_spec_unref (GParamSpec *pspec)
{
  g_return_if_fail (G_IS_PARAM_SPEC (pspec));
  g_return_if_fail (pspec->ref_count > 0);

  pspec->ref_count -= 1;
  if (pspec->ref_count == 0)
    G_PARAM_SPEC_GET_CLASS (pspec)->finalize (pspec);
}

gpointer
g_param_spec_internal (GType        param_type,
		       const gchar *name,
		       const gchar *nick,
		       const gchar *blurb,
		       GParamFlags  flags)
{
  GParamSpec *pspec;

  g_return_val_if_fail (G_TYPE_IS_PARAM (param_type) && param_type != G_TYPE_PARAM, NULL);
  g_return_val_if_fail (name != NULL, NULL);

  pspec = (gpointer) g_type_create_instance (param_type);
  pspec->name = g_strdup (name);
  g_strcanon (pspec->name, G_CSET_A_2_Z G_CSET_a_2_z G_CSET_DIGITS "-", '-');
  pspec->nick = g_strdup (nick ? nick : pspec->name);
  pspec->blurb = g_strdup (blurb);
  pspec->flags = (flags & G_PARAM_USER_MASK) | (flags & G_PARAM_MASK);

  return pspec;
}

gpointer
g_param_spec_get_qdata (GParamSpec *pspec,
			GQuark      quark)
{
  g_return_val_if_fail (G_IS_PARAM_SPEC (pspec), NULL);
  
  return quark ? g_datalist_id_get_data (&pspec->qdata, quark) : NULL;
}

void
g_param_spec_set_qdata (GParamSpec *pspec,
			GQuark      quark,
			gpointer    data)
{
  g_return_if_fail (G_IS_PARAM_SPEC (pspec));
  g_return_if_fail (quark > 0);

  g_datalist_id_set_data (&pspec->qdata, quark, data);
}

void
g_param_spec_set_qdata_full (GParamSpec    *pspec,
			     GQuark         quark,
			     gpointer       data,
			     GDestroyNotify destroy)
{
  g_return_if_fail (G_IS_PARAM_SPEC (pspec));
  g_return_if_fail (quark > 0);

  g_datalist_id_set_data_full (&pspec->qdata, quark, data, data ? destroy : (GDestroyNotify) NULL);
}

gpointer
g_param_spec_steal_qdata (GParamSpec *pspec,
			  GQuark      quark)
{
  g_return_val_if_fail (G_IS_PARAM_SPEC (pspec), NULL);
  g_return_val_if_fail (quark > 0, NULL);
  
  return g_datalist_id_remove_no_notify (&pspec->qdata, quark);
}

void
g_param_value_set_default (GParamSpec *pspec,
			   GValue     *value)
{
  g_return_if_fail (G_IS_PARAM_SPEC (pspec));
  g_return_if_fail (G_IS_VALUE (value));
  g_return_if_fail (G_IS_PARAM_VALUE (pspec, value));

  g_value_reset (value);
  G_PARAM_SPEC_GET_CLASS (pspec)->value_set_default (pspec, value);
}

gboolean
g_param_value_defaults (GParamSpec *pspec,
			GValue     *value)
{
  GValue dflt_value = { 0, };
  gboolean defaults;

  g_return_val_if_fail (G_IS_PARAM_SPEC (pspec), FALSE);
  g_return_val_if_fail (G_IS_VALUE (value), FALSE);
  g_return_val_if_fail (G_IS_PARAM_VALUE (pspec, value), FALSE);

  g_value_init (&dflt_value, G_PARAM_SPEC_VALUE_TYPE (pspec));
  G_PARAM_SPEC_GET_CLASS (pspec)->value_set_default (pspec, &dflt_value);
  defaults = G_PARAM_SPEC_GET_CLASS (pspec)->values_cmp (pspec, value, &dflt_value) == 0;
  g_value_unset (&dflt_value);

  return defaults;
}

gboolean
g_param_value_validate (GParamSpec *pspec,
			GValue     *value)
{
  g_return_val_if_fail (G_IS_PARAM_SPEC (pspec), FALSE);
  g_return_val_if_fail (G_IS_VALUE (value), FALSE);
  g_return_val_if_fail (G_IS_PARAM_VALUE (pspec, value), FALSE);

  if (G_PARAM_SPEC_GET_CLASS (pspec)->value_validate)
    {
      GValue oval = *value;

      if (G_PARAM_SPEC_GET_CLASS (pspec)->value_validate (pspec, value) ||
	  memcmp (&oval.data, &value->data, sizeof (oval.data)))
	return TRUE;
    }

  return FALSE;
}

gint
g_param_values_cmp (GParamSpec   *pspec,
		    const GValue *value1,
		    const GValue *value2)
{
  gint cmp;

  /* param_values_cmp() effectively does: value1 - value2
   * so the return values are:
   * -1)  value1 < value2
   *  0)  value1 == value2
   *  1)  value1 > value2
   */
  g_return_val_if_fail (G_IS_PARAM_SPEC (pspec), 0);
  g_return_val_if_fail (G_IS_VALUE (value1), 0);
  g_return_val_if_fail (G_IS_VALUE (value2), 0);
  g_return_val_if_fail (G_IS_PARAM_VALUE (pspec, value1), 0);
  g_return_val_if_fail (G_IS_PARAM_VALUE (pspec, value2), 0);

  cmp = G_PARAM_SPEC_GET_CLASS (pspec)->values_cmp (pspec, value1, value2);

  return CLAMP (cmp, -1, 1);
}

static guint
param_spec_hash (gconstpointer key_spec)
{
  const GParamSpec *key = key_spec;
  const gchar *p;
  guint h = key->owner_type;

  for (p = key->name; *p; p++)
    h = (h << 5) - h + *p;

  return h;
}

static gint
param_spec_equals (gconstpointer key_spec_1,
		   gconstpointer key_spec_2)
{
  const GParamSpec *key1 = key_spec_1;
  const GParamSpec *key2 = key_spec_2;

  return (key1->owner_type == key2->owner_type &&
	  strcmp (key1->name, key2->name) == 0);
}

GHashTable*
g_param_spec_hash_table_new (void)
{
  return g_hash_table_new (param_spec_hash, param_spec_equals);
}

void
g_param_spec_hash_table_insert (GHashTable *hash_table,
				GParamSpec *pspec,
				GType       owner_type)
{
  g_return_if_fail (hash_table != NULL);
  g_return_if_fail (G_IS_PARAM_SPEC (pspec));
  g_return_if_fail (pspec->name != NULL);
  if (pspec->owner_type != owner_type)
    g_return_if_fail (pspec->owner_type == 0);

  if (strchr (pspec->name, ':'))
    g_warning (G_STRLOC ": parameter name `%s' contains field-delimeter",
	       pspec->name);
  else
    {
      pspec->owner_type = owner_type;
      g_hash_table_insert (hash_table, pspec, pspec);
    }
}

void
g_param_spec_hash_table_remove (GHashTable *hash_table,
				GParamSpec *pspec)
{
  g_return_if_fail (hash_table != NULL);
  g_return_if_fail (G_IS_PARAM_SPEC (pspec));

  g_assert (g_param_spec_hash_table_lookup (hash_table, pspec->name, pspec->owner_type, FALSE, NULL) != NULL); /* FIXME: paranoid */

  g_hash_table_remove (hash_table, pspec);
  g_assert (g_param_spec_hash_table_lookup (hash_table, pspec->name, pspec->owner_type, FALSE, NULL) == NULL); /* FIXME: paranoid */
  pspec->owner_type = 0;
}

GParamSpec*
g_param_spec_hash_table_lookup (GHashTable   *hash_table,
				const gchar  *param_name,
				GType         owner_type,
				gboolean      try_ancestors,
				const gchar **trailer)
{
  GParamSpec *pspec;
  GParamSpec key;
  gchar *delim;
  
  g_return_val_if_fail (hash_table != NULL, NULL);
  g_return_val_if_fail (param_name != NULL, NULL);
  
  key.owner_type = owner_type;
  delim = strchr (param_name, ':');
  if (delim)
    key.name = g_strndup (param_name, delim - param_name);
  else
    key.name = g_strdup (param_name);
  g_strcanon (key.name, G_CSET_A_2_Z G_CSET_a_2_z G_CSET_DIGITS "-", '-');

  if (trailer)
    *trailer = delim;
  
  pspec = g_hash_table_lookup (hash_table, &key);
  if (!pspec && try_ancestors)
    {
      key.owner_type = g_type_parent (key.owner_type);
      while (key.owner_type)
	{
	  pspec = g_hash_table_lookup (hash_table, &key);
	  if (pspec)
	    break;
	  key.owner_type = g_type_parent (key.owner_type);
	}
    }
  
  g_free (key.name);
  
  return pspec;
}


/* --- auxillary functions --- */
typedef struct
{
  /* class portion */
  GType           value_type;
  void          (*finalize)             (GParamSpec   *pspec);
  void          (*value_set_default)    (GParamSpec   *pspec,
					 GValue       *value);
  gboolean      (*value_validate)       (GParamSpec   *pspec,
					 GValue       *value);
  gint          (*values_cmp)           (GParamSpec   *pspec,
					 const GValue *value1,
					 const GValue *value2);
} ParamSpecClassInfo;

static void
param_spec_generic_class_init (gpointer g_class,
			       gpointer class_data)
{
  GParamSpecClass *class = g_class;
  ParamSpecClassInfo *info = class_data;

  class->value_type = info->value_type;
  if (info->finalize)
    class->finalize = info->finalize;			/* optional */
  class->value_set_default = info->value_set_default;
  if (info->value_validate)
    class->value_validate = info->value_validate;	/* optional */
  class->values_cmp = info->values_cmp;
  g_free (class_data);
}

static void
default_value_set_default (GParamSpec *pspec,
			   GValue     *value)
{
  /* value is already zero initialized */
}

static gint
default_values_cmp (GParamSpec   *pspec,
		    const GValue *value1,
		    const GValue *value2)
{
  return memcmp (&value1->data, &value2->data, sizeof (value1->data));
}

GType
g_param_type_register_static (const gchar              *name,
			      const GParamSpecTypeInfo *pspec_info)
{
  GTypeInfo info = {
    sizeof (GParamSpecClass),      /* class_size */
    NULL,                          /* base_init */
    NULL,                          /* base_destroy */
    param_spec_generic_class_init, /* class_init */
    NULL,                          /* class_destroy */
    NULL,                          /* class_data */
    0,                             /* instance_size */
    16,                            /* n_preallocs */
    NULL,                          /* instance_init */
  };
  ParamSpecClassInfo *cinfo;

  g_return_val_if_fail (name != NULL, 0);
  g_return_val_if_fail (pspec_info != NULL, 0);
  g_return_val_if_fail (g_type_from_name (name) == 0, 0);
  g_return_val_if_fail (pspec_info->instance_size >= sizeof (GParamSpec), 0);
  g_return_val_if_fail (g_type_name (pspec_info->value_type) != NULL, 0);
  /* default: g_return_val_if_fail (pspec_info->value_set_default != NULL, 0); */
  /* optional: g_return_val_if_fail (pspec_info->value_validate != NULL, 0); */
  /* default: g_return_val_if_fail (pspec_info->values_cmp != NULL, 0); */

  info.instance_size = pspec_info->instance_size;
  info.n_preallocs = pspec_info->n_preallocs;
  info.instance_init = (GInstanceInitFunc) pspec_info->instance_init;
  cinfo = g_new (ParamSpecClassInfo, 1);
  cinfo->value_type = pspec_info->value_type;
  cinfo->finalize = pspec_info->finalize;
  cinfo->value_set_default = pspec_info->value_set_default ? pspec_info->value_set_default : default_value_set_default;
  cinfo->value_validate = pspec_info->value_validate;
  cinfo->values_cmp = pspec_info->values_cmp ? pspec_info->values_cmp : default_values_cmp;
  info.class_data = cinfo;

  return g_type_register_static (G_TYPE_PARAM, name, &info, 0);
}
