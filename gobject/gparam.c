/* GObject - GLib Type, Object, Parameter and Signal Library
 * Copyright (C) 1997-1999, 2000-2001 Tim Janik and Red Hat, Inc.
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

/*
 * MT safe
 */

#include	"gparam.h"
#include        "gparamspecs.h"

#include	"gvaluecollector.h"
#include	"gobjectalias.h"
#include	<string.h>



/* --- defines --- */
#define PARAM_FLOATING_FLAG                     0x2
#define	G_PARAM_USER_MASK			(~0 << G_PARAM_USER_SHIFT)
#define PSPEC_APPLIES_TO_VALUE(pspec, value)	(G_TYPE_CHECK_VALUE_TYPE ((value), G_PARAM_SPEC_VALUE_TYPE (pspec)))
#define	G_SLOCK(mutex)				g_static_mutex_lock (mutex)
#define	G_SUNLOCK(mutex)			g_static_mutex_unlock (mutex)


/* --- prototypes --- */
static void	g_param_spec_class_base_init	 (GParamSpecClass	*class);
static void	g_param_spec_class_base_finalize (GParamSpecClass	*class);
static void	g_param_spec_class_init		 (GParamSpecClass	*class,
						  gpointer               class_data);
static void	g_param_spec_init		 (GParamSpec		*pspec,
						  GParamSpecClass	*class);
static void	g_param_spec_finalize		 (GParamSpec		*pspec);
static void	value_param_init		(GValue		*value);
static void	value_param_free_value		(GValue		*value);
static void	value_param_copy_value		(const GValue	*src_value,
						 GValue		*dest_value);
static void	value_param_transform_value	(const GValue	*src_value,
						 GValue		*dest_value);
static gpointer	value_param_peek_pointer	(const GValue	*value);
static gchar*	value_param_collect_value	(GValue		*value,
						 guint           n_collect_values,
						 GTypeCValue    *collect_values,
						 guint           collect_flags);
static gchar*	value_param_lcopy_value		(const GValue	*value,
						 guint           n_collect_values,
						 GTypeCValue    *collect_values,
						 guint           collect_flags);


/* --- functions --- */
void
g_param_type_init (void)
{
  static const GTypeFundamentalInfo finfo = {
    (G_TYPE_FLAG_CLASSED |
     G_TYPE_FLAG_INSTANTIATABLE |
     G_TYPE_FLAG_DERIVABLE |
     G_TYPE_FLAG_DEEP_DERIVABLE),
  };
  static const GTypeValueTable param_value_table = {
    value_param_init,           /* value_init */
    value_param_free_value,     /* value_free */
    value_param_copy_value,     /* value_copy */
    value_param_peek_pointer,   /* value_peek_pointer */
    "p",			/* collect_format */
    value_param_collect_value,  /* collect_value */
    "p",			/* lcopy_format */
    value_param_lcopy_value,    /* lcopy_value */
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

    &param_value_table,
  };
  GType type;

  type = g_type_register_fundamental (G_TYPE_PARAM, g_intern_static_string ("GParam"), &param_spec_info, &finfo, G_TYPE_FLAG_ABSTRACT);
  g_assert (type == G_TYPE_PARAM);
  g_value_register_transform_func (G_TYPE_PARAM, G_TYPE_PARAM, value_param_transform_value);
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
g_param_spec_init (GParamSpec      *pspec,
		   GParamSpecClass *class)
{
  pspec->name = NULL;
  pspec->_nick = NULL;
  pspec->_blurb = NULL;
  pspec->flags = 0;
  pspec->value_type = class->value_type;
  pspec->owner_type = 0;
  pspec->qdata = NULL;
  g_datalist_init (&pspec->qdata);
  g_datalist_set_flags (&pspec->qdata, PARAM_FLOATING_FLAG);
  pspec->ref_count = 1;
  pspec->param_id = 0;
}

static void
g_param_spec_finalize (GParamSpec *pspec)
{
  g_datalist_clear (&pspec->qdata);

  if (!(pspec->flags & G_PARAM_STATIC_NAME))
    g_free (pspec->name);
  
  if (!(pspec->flags & G_PARAM_STATIC_NICK))
    g_free (pspec->_nick);

  if (!(pspec->flags & G_PARAM_STATIC_BLURB))
    g_free (pspec->_blurb);

  g_type_free_instance ((GTypeInstance*) pspec);
}

GParamSpec*
g_param_spec_ref (GParamSpec *pspec)
{
  g_return_val_if_fail (G_IS_PARAM_SPEC (pspec), NULL);
  g_return_val_if_fail (pspec->ref_count > 0, NULL);

  g_atomic_int_inc (&pspec->ref_count);

  return pspec;
}

void
g_param_spec_unref (GParamSpec *pspec)
{
  gboolean is_zero;

  g_return_if_fail (G_IS_PARAM_SPEC (pspec));
  g_return_if_fail (pspec->ref_count > 0);

  is_zero = g_atomic_int_dec_and_test (&pspec->ref_count);

  if (G_UNLIKELY (is_zero))
    {
      G_PARAM_SPEC_GET_CLASS (pspec)->finalize (pspec);
    }
}

void
g_param_spec_sink (GParamSpec *pspec)
{
  gpointer oldvalue;
  g_return_if_fail (G_IS_PARAM_SPEC (pspec));
  g_return_if_fail (pspec->ref_count > 0);

  do
    oldvalue = g_atomic_pointer_get (&pspec->qdata);
  while (!g_atomic_pointer_compare_and_exchange ((void**) &pspec->qdata, oldvalue,
                                                 (gpointer) ((gsize) oldvalue & ~(gsize) PARAM_FLOATING_FLAG)));
  if ((gsize) oldvalue & PARAM_FLOATING_FLAG)
    g_param_spec_unref (pspec);
}

GParamSpec*
g_param_spec_ref_sink (GParamSpec *pspec)
{
  g_return_val_if_fail (G_IS_PARAM_SPEC (pspec), NULL);
  g_return_val_if_fail (pspec->ref_count > 0, NULL);

  g_param_spec_ref (pspec);
  g_param_spec_sink (pspec);
  return pspec;
}

G_CONST_RETURN gchar*
g_param_spec_get_name (GParamSpec *pspec)
{
  g_return_val_if_fail (G_IS_PARAM_SPEC (pspec), NULL);

  return pspec->name;
}

G_CONST_RETURN gchar*
g_param_spec_get_nick (GParamSpec *pspec)
{
  g_return_val_if_fail (G_IS_PARAM_SPEC (pspec), NULL);

  if (pspec->_nick)
    return pspec->_nick;
  else
    {
      GParamSpec *redirect_target;

      redirect_target = g_param_spec_get_redirect_target (pspec);
      if (redirect_target && redirect_target->_nick)
	return redirect_target->_nick;
    }

  return pspec->name;
}

G_CONST_RETURN gchar*
g_param_spec_get_blurb (GParamSpec *pspec)
{
  g_return_val_if_fail (G_IS_PARAM_SPEC (pspec), NULL);

  if (pspec->_blurb)
    return pspec->_blurb;
  else
    {
      GParamSpec *redirect_target;

      redirect_target = g_param_spec_get_redirect_target (pspec);
      if (redirect_target && redirect_target->_blurb)
	return redirect_target->_blurb;
    }

  return NULL;
}

static void
canonicalize_key (gchar *key)
{
  gchar *p;
  
  for (p = key; *p != 0; p++)
    {
      gchar c = *p;
      
      if (c != '-' &&
	  (c < '0' || c > '9') &&
	  (c < 'A' || c > 'Z') &&
	  (c < 'a' || c > 'z'))
	*p = '-';
    }
}

static gboolean
is_canonical (const gchar *key)
{
  const gchar *p;

  for (p = key; *p != 0; p++)
    {
      gchar c = *p;
      
      if (c != '-' &&
	  (c < '0' || c > '9') &&
	  (c < 'A' || c > 'Z') &&
	  (c < 'a' || c > 'z'))
	return FALSE;
    }

  return TRUE;
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
  g_return_val_if_fail ((name[0] >= 'A' && name[0] <= 'Z') || (name[0] >= 'a' && name[0] <= 'z'), NULL);
  g_return_val_if_fail (!(flags & G_PARAM_STATIC_NAME) || is_canonical (name), NULL);
  
  pspec = (gpointer) g_type_create_instance (param_type);

  if (flags & G_PARAM_STATIC_NAME)
    {
      pspec->name = g_intern_static_string (name);
      if (!is_canonical (pspec->name))
        g_warning ("G_PARAM_STATIC_NAME used with non-canonical pspec name: %s", pspec->name);
    }
  else
    {
      pspec->name = g_strdup (name);
      canonicalize_key (pspec->name);
      g_intern_string (pspec->name);
    }

  if (flags & G_PARAM_STATIC_NICK)
    pspec->_nick = (gchar*) nick;
  else
    pspec->_nick = g_strdup (nick);

  if (flags & G_PARAM_STATIC_BLURB)
    pspec->_blurb = (gchar*) blurb;
  else
    pspec->_blurb = g_strdup (blurb);

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

GParamSpec*
g_param_spec_get_redirect_target (GParamSpec *pspec)
{
  g_return_val_if_fail (G_IS_PARAM_SPEC (pspec), NULL);

  if (G_IS_PARAM_SPEC_OVERRIDE (pspec))
    {
      GParamSpecOverride *ospec = G_PARAM_SPEC_OVERRIDE (pspec);

      return ospec->overridden;
    }
  else
    return NULL;
}

void
g_param_value_set_default (GParamSpec *pspec,
			   GValue     *value)
{
  g_return_if_fail (G_IS_PARAM_SPEC (pspec));
  g_return_if_fail (G_IS_VALUE (value));
  g_return_if_fail (PSPEC_APPLIES_TO_VALUE (pspec, value));

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
  g_return_val_if_fail (PSPEC_APPLIES_TO_VALUE (pspec, value), FALSE);

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
  g_return_val_if_fail (PSPEC_APPLIES_TO_VALUE (pspec, value), FALSE);

  if (G_PARAM_SPEC_GET_CLASS (pspec)->value_validate)
    {
      GValue oval = *value;

      if (G_PARAM_SPEC_GET_CLASS (pspec)->value_validate (pspec, value) ||
	  memcmp (&oval.data, &value->data, sizeof (oval.data)))
	return TRUE;
    }

  return FALSE;
}

gboolean
g_param_value_convert (GParamSpec   *pspec,
		       const GValue *src_value,
		       GValue       *dest_value,
		       gboolean	     strict_validation)
{
  GValue tmp_value = { 0, };

  g_return_val_if_fail (G_IS_PARAM_SPEC (pspec), FALSE);
  g_return_val_if_fail (G_IS_VALUE (src_value), FALSE);
  g_return_val_if_fail (G_IS_VALUE (dest_value), FALSE);
  g_return_val_if_fail (PSPEC_APPLIES_TO_VALUE (pspec, dest_value), FALSE);

  /* better leave dest_value untouched when returning FALSE */

  g_value_init (&tmp_value, G_VALUE_TYPE (dest_value));
  if (g_value_transform (src_value, &tmp_value) &&
      (!g_param_value_validate (pspec, &tmp_value) || !strict_validation))
    {
      g_value_unset (dest_value);
      
      /* values are relocatable */
      memcpy (dest_value, &tmp_value, sizeof (tmp_value));
      
      return TRUE;
    }
  else
    {
      g_value_unset (&tmp_value);
      
      return FALSE;
    }
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
  g_return_val_if_fail (PSPEC_APPLIES_TO_VALUE (pspec, value1), 0);
  g_return_val_if_fail (PSPEC_APPLIES_TO_VALUE (pspec, value2), 0);

  cmp = G_PARAM_SPEC_GET_CLASS (pspec)->values_cmp (pspec, value1, value2);

  return CLAMP (cmp, -1, 1);
}

static void
value_param_init (GValue *value)
{
  value->data[0].v_pointer = NULL;
}

static void
value_param_free_value (GValue *value)
{
  if (value->data[0].v_pointer)
    g_param_spec_unref (value->data[0].v_pointer);
}

static void
value_param_copy_value (const GValue *src_value,
			GValue       *dest_value)
{
  if (src_value->data[0].v_pointer)
    dest_value->data[0].v_pointer = g_param_spec_ref (src_value->data[0].v_pointer);
  else
    dest_value->data[0].v_pointer = NULL;
}

static void
value_param_transform_value (const GValue *src_value,
			     GValue       *dest_value)
{
  if (src_value->data[0].v_pointer &&
      g_type_is_a (G_PARAM_SPEC_TYPE (dest_value->data[0].v_pointer), G_VALUE_TYPE (dest_value)))
    dest_value->data[0].v_pointer = g_param_spec_ref (src_value->data[0].v_pointer);
  else
    dest_value->data[0].v_pointer = NULL;
}

static gpointer
value_param_peek_pointer (const GValue *value)
{
  return value->data[0].v_pointer;
}

static gchar*
value_param_collect_value (GValue      *value,
			   guint        n_collect_values,
			   GTypeCValue *collect_values,
			   guint        collect_flags)
{
  if (collect_values[0].v_pointer)
    {
      GParamSpec *param = collect_values[0].v_pointer;

      if (param->g_type_instance.g_class == NULL)
	return g_strconcat ("invalid unclassed param spec pointer for value type `",
			    G_VALUE_TYPE_NAME (value),
			    "'",
			    NULL);
      else if (!g_value_type_compatible (G_PARAM_SPEC_TYPE (param), G_VALUE_TYPE (value)))
	return g_strconcat ("invalid param spec type `",
			    G_PARAM_SPEC_TYPE_NAME (param),
			    "' for value type `",
			    G_VALUE_TYPE_NAME (value),
			    "'",
			    NULL);
      value->data[0].v_pointer = g_param_spec_ref (param);
    }
  else
    value->data[0].v_pointer = NULL;

  return NULL;
}

static gchar*
value_param_lcopy_value (const GValue *value,
			 guint         n_collect_values,
			 GTypeCValue  *collect_values,
			 guint         collect_flags)
{
  GParamSpec **param_p = collect_values[0].v_pointer;

  if (!param_p)
    return g_strdup_printf ("value location for `%s' passed as NULL", G_VALUE_TYPE_NAME (value));

  if (!value->data[0].v_pointer)
    *param_p = NULL;
  else if (collect_flags & G_VALUE_NOCOPY_CONTENTS)
    *param_p = value->data[0].v_pointer;
  else
    *param_p = g_param_spec_ref (value->data[0].v_pointer);

  return NULL;
}


/* --- param spec pool --- */
struct _GParamSpecPool
{
  GStaticMutex smutex;
  gboolean     type_prefixing;
  GHashTable  *hash_table;
};

static guint
param_spec_pool_hash (gconstpointer key_spec)
{
  const GParamSpec *key = key_spec;
  const gchar *p;
  guint h = key->owner_type;

  for (p = key->name; *p; p++)
    h = (h << 5) - h + *p;

  return h;
}

static gboolean
param_spec_pool_equals (gconstpointer key_spec_1,
			gconstpointer key_spec_2)
{
  const GParamSpec *key1 = key_spec_1;
  const GParamSpec *key2 = key_spec_2;

  return (key1->owner_type == key2->owner_type &&
	  strcmp (key1->name, key2->name) == 0);
}

GParamSpecPool*
g_param_spec_pool_new (gboolean type_prefixing)
{
  static GStaticMutex init_smutex = G_STATIC_MUTEX_INIT;
  GParamSpecPool *pool = g_new (GParamSpecPool, 1);

  memcpy (&pool->smutex, &init_smutex, sizeof (init_smutex));
  pool->type_prefixing = type_prefixing != FALSE;
  pool->hash_table = g_hash_table_new (param_spec_pool_hash, param_spec_pool_equals);

  return pool;
}

void
g_param_spec_pool_insert (GParamSpecPool *pool,
			  GParamSpec     *pspec,
			  GType           owner_type)
{
  gchar *p;
  
  if (pool && pspec && owner_type > 0 && pspec->owner_type == 0)
    {
      G_SLOCK (&pool->smutex);
      for (p = pspec->name; *p; p++)
	{
	  if (!strchr (G_CSET_A_2_Z G_CSET_a_2_z G_CSET_DIGITS "-_", *p))
	    {
	      g_warning (G_STRLOC ": pspec name \"%s\" contains invalid characters", pspec->name);
	      G_SUNLOCK (&pool->smutex);
	      return;
	    }
	}
      
      pspec->owner_type = owner_type;
      g_param_spec_ref (pspec);
      g_hash_table_insert (pool->hash_table, pspec, pspec);
      G_SUNLOCK (&pool->smutex);
    }
  else
    {
      g_return_if_fail (pool != NULL);
      g_return_if_fail (pspec);
      g_return_if_fail (owner_type > 0);
      g_return_if_fail (pspec->owner_type == 0);
    }
}

void
g_param_spec_pool_remove (GParamSpecPool *pool,
			  GParamSpec     *pspec)
{
  if (pool && pspec)
    {
      G_SLOCK (&pool->smutex);
      if (g_hash_table_remove (pool->hash_table, pspec))
	g_param_spec_unref (pspec);
      else
	g_warning (G_STRLOC ": attempt to remove unknown pspec `%s' from pool", pspec->name);
      G_SUNLOCK (&pool->smutex);
    }
  else
    {
      g_return_if_fail (pool != NULL);
      g_return_if_fail (pspec);
    }
}

static inline GParamSpec*
param_spec_ht_lookup (GHashTable  *hash_table,
		      const gchar *param_name,
		      GType        owner_type,
		      gboolean     walk_ancestors)
{
  GParamSpec key, *pspec;

  key.owner_type = owner_type;
  key.name = (gchar*) param_name;
  if (walk_ancestors)
    do
      {
	pspec = g_hash_table_lookup (hash_table, &key);
	if (pspec)
	  return pspec;
	key.owner_type = g_type_parent (key.owner_type);
      }
    while (key.owner_type);
  else
    pspec = g_hash_table_lookup (hash_table, &key);

  if (!pspec && !is_canonical (param_name))
    {
      /* try canonicalized form */
      key.name = g_strdup (param_name);
      key.owner_type = owner_type;
      
      canonicalize_key (key.name);
      if (walk_ancestors)
	do
	  {
	    pspec = g_hash_table_lookup (hash_table, &key);
	    if (pspec)
	      {
		g_free (key.name);
		return pspec;
	      }
	    key.owner_type = g_type_parent (key.owner_type);
	  }
	while (key.owner_type);
      else
	pspec = g_hash_table_lookup (hash_table, &key);
      g_free (key.name);
    }

  return pspec;
}

GParamSpec*
g_param_spec_pool_lookup (GParamSpecPool *pool,
			  const gchar    *param_name,
			  GType           owner_type,
			  gboolean        walk_ancestors)
{
  GParamSpec *pspec;
  gchar *delim;

  if (!pool || !param_name)
    {
      g_return_val_if_fail (pool != NULL, NULL);
      g_return_val_if_fail (param_name != NULL, NULL);
    }

  G_SLOCK (&pool->smutex);

  delim = pool->type_prefixing ? strchr (param_name, ':') : NULL;

  /* try quick and away, i.e. without prefix */
  if (!delim)
    {
      pspec = param_spec_ht_lookup (pool->hash_table, param_name, owner_type, walk_ancestors);
      G_SUNLOCK (&pool->smutex);

      return pspec;
    }

  /* strip type prefix */
  if (pool->type_prefixing && delim[1] == ':')
    {
      guint l = delim - param_name;
      gchar stack_buffer[32], *buffer = l < 32 ? stack_buffer : g_new (gchar, l + 1);
      GType type;
      
      strncpy (buffer, param_name, delim - param_name);
      buffer[l] = 0;
      type = g_type_from_name (buffer);
      if (l >= 32)
	g_free (buffer);
      if (type)		/* type==0 isn't a valid type pefix */
	{
	  /* sanity check, these cases don't make a whole lot of sense */
	  if ((!walk_ancestors && type != owner_type) || !g_type_is_a (owner_type, type))
	    {
	      G_SUNLOCK (&pool->smutex);

	      return NULL;
	    }
	  owner_type = type;
	  param_name += l + 2;
	  pspec = param_spec_ht_lookup (pool->hash_table, param_name, owner_type, walk_ancestors);
	  G_SUNLOCK (&pool->smutex);

	  return pspec;
	}
    }
  /* malformed param_name */

  G_SUNLOCK (&pool->smutex);

  return NULL;
}

static void
pool_list (gpointer key,
	   gpointer value,
	   gpointer user_data)
{
  GParamSpec *pspec = value;
  gpointer *data = user_data;
  GType owner_type = (GType) data[1];

  if (owner_type == pspec->owner_type)
    data[0] = g_list_prepend (data[0], pspec);
}

GList*
g_param_spec_pool_list_owned (GParamSpecPool *pool,
			      GType           owner_type)
{
  gpointer data[2];

  g_return_val_if_fail (pool != NULL, NULL);
  g_return_val_if_fail (owner_type > 0, NULL);
  
  G_SLOCK (&pool->smutex);
  data[0] = NULL;
  data[1] = (gpointer) owner_type;
  g_hash_table_foreach (pool->hash_table, pool_list, &data);
  G_SUNLOCK (&pool->smutex);

  return data[0];
}

static gint
pspec_compare_id (gconstpointer a,
		  gconstpointer b)
{
  const GParamSpec *pspec1 = a, *pspec2 = b;

  return pspec1->param_id < pspec2->param_id ? -1 : pspec1->param_id > pspec2->param_id;
}

static inline GSList*
pspec_list_remove_overridden_and_redirected (GSList     *plist,
					     GHashTable *ht,
					     GType       owner_type,
					     guint      *n_p)
{
  GSList *rlist = NULL;

  while (plist)
    {
      GSList *tmp = plist->next;
      GParamSpec *pspec = plist->data;
      GParamSpec *found;
      gboolean remove = FALSE;

      /* Remove paramspecs that are redirected, and also paramspecs
       * that have are overridden by non-redirected properties.
       * The idea is to get the single paramspec for each name that
       * best corresponds to what the application sees.
       */
      if (g_param_spec_get_redirect_target (pspec))
	remove = TRUE;
      else
	{
	  found = param_spec_ht_lookup (ht, pspec->name, owner_type, TRUE);
	  if (found != pspec)
	    {
	      GParamSpec *redirect = g_param_spec_get_redirect_target (found);
	      if (redirect != pspec)
		remove = TRUE;
	    }
	}

      if (remove)
	{
	  g_slist_free_1 (plist);
	}
      else
	{
	  plist->next = rlist;
	  rlist = plist;
	  *n_p += 1;
	}
      plist = tmp;
    }
  return rlist;
}

static void
pool_depth_list (gpointer key,
		 gpointer value,
		 gpointer user_data)
{
  GParamSpec *pspec = value;
  gpointer *data = user_data;
  GSList **slists = data[0];
  GType owner_type = (GType) data[1];

  if (g_type_is_a (owner_type, pspec->owner_type))
    {
      if (G_TYPE_IS_INTERFACE (pspec->owner_type))
	{
	  slists[0] = g_slist_prepend (slists[0], pspec);
	}
      else
	{
	  guint d = g_type_depth (pspec->owner_type);

	  slists[d - 1] = g_slist_prepend (slists[d - 1], pspec);
	}
    }
}

/* We handle interfaces specially since we don't want to
 * count interface prerequisites like normal inheritance;
 * the property comes from the direct inheritance from
 * the prerequisite class, not from the interface that
 * prerequires it.
 * 
 * also 'depth' isn't a meaningful concept for interface
 * prerequites.
 */
static void
pool_depth_list_for_interface (gpointer key,
			       gpointer value,
			       gpointer user_data)
{
  GParamSpec *pspec = value;
  gpointer *data = user_data;
  GSList **slists = data[0];
  GType owner_type = (GType) data[1];

  if (pspec->owner_type == owner_type)
    slists[0] = g_slist_prepend (slists[0], pspec);
}

GParamSpec** /* free result */
g_param_spec_pool_list (GParamSpecPool *pool,
			GType           owner_type,
			guint          *n_pspecs_p)
{
  GParamSpec **pspecs, **p;
  GSList **slists, *node;
  gpointer data[2];
  guint d, i;

  g_return_val_if_fail (pool != NULL, NULL);
  g_return_val_if_fail (owner_type > 0, NULL);
  g_return_val_if_fail (n_pspecs_p != NULL, NULL);
  
  G_SLOCK (&pool->smutex);
  *n_pspecs_p = 0;
  d = g_type_depth (owner_type);
  slists = g_new0 (GSList*, d);
  data[0] = slists;
  data[1] = (gpointer) owner_type;

  g_hash_table_foreach (pool->hash_table,
			G_TYPE_IS_INTERFACE (owner_type) ?
			   pool_depth_list_for_interface :
			   pool_depth_list,
			&data);
  
  for (i = 0; i < d; i++)
    slists[i] = pspec_list_remove_overridden_and_redirected (slists[i], pool->hash_table, owner_type, n_pspecs_p);
  pspecs = g_new (GParamSpec*, *n_pspecs_p + 1);
  p = pspecs;
  for (i = 0; i < d; i++)
    {
      slists[i] = g_slist_sort (slists[i], pspec_compare_id);
      for (node = slists[i]; node; node = node->next)
	*p++ = node->data;
      g_slist_free (slists[i]);
    }
  *p++ = NULL;
  g_free (slists);
  G_SUNLOCK (&pool->smutex);

  return pspecs;
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

void
g_value_set_param (GValue     *value,
		   GParamSpec *param)
{
  g_return_if_fail (G_VALUE_HOLDS_PARAM (value));
  if (param)
    g_return_if_fail (G_IS_PARAM_SPEC (param));

  if (value->data[0].v_pointer)
    g_param_spec_unref (value->data[0].v_pointer);
  value->data[0].v_pointer = param;
  if (value->data[0].v_pointer)
    g_param_spec_ref (value->data[0].v_pointer);
}

void
g_value_set_param_take_ownership (GValue     *value,
				  GParamSpec *param)
{
  g_value_take_param (value, param);
}

void
g_value_take_param (GValue     *value,
		    GParamSpec *param)
{
  g_return_if_fail (G_VALUE_HOLDS_PARAM (value));
  if (param)
    g_return_if_fail (G_IS_PARAM_SPEC (param));

  if (value->data[0].v_pointer)
    g_param_spec_unref (value->data[0].v_pointer);
  value->data[0].v_pointer = param; /* we take over the reference count */
}

GParamSpec*
g_value_get_param (const GValue *value)
{
  g_return_val_if_fail (G_VALUE_HOLDS_PARAM (value), NULL);

  return value->data[0].v_pointer;
}

GParamSpec*
g_value_dup_param (const GValue *value)
{
  g_return_val_if_fail (G_VALUE_HOLDS_PARAM (value), NULL);

  return value->data[0].v_pointer ? g_param_spec_ref (value->data[0].v_pointer) : NULL;
}

#define __G_PARAM_C__
#include "gobjectaliasdef.c"
