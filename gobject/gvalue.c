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
#include	"gvalue.h"


/* --- defines --- */
#define G_PARAM_SPEC_CLASS(class)    (G_TYPE_CHECK_CLASS_CAST ((class), G_TYPE_PARAM, GParamSpecClass))


/* --- typedefs & structures --- */
typedef struct
{
  GType          value_type1;
  GType          value_type2;
  GValueExchange func;
  GType          first_type;
} ExchangeEntry;


/* --- variables --- */
static GHashTable *param_exchange_ht = NULL;


/* --- functions --- */
void
g_value_init (GValue *value,
	      GType   g_type)
{
  GParamSpecClass *pclass;

  g_return_if_fail (value != NULL);
  g_return_if_fail (G_VALUE_TYPE (value) == 0);
  g_type = g_type_next_base (g_type, G_TYPE_PARAM);
  g_return_if_fail (G_TYPE_IS_VALUE (g_type));

  memset (value, 0, sizeof (*value));
  value->g_type = g_type;

  pclass = g_type_class_ref (G_VALUE_TYPE (value));
  pclass->param_init (value, NULL);
  g_type_class_unref (pclass);
}

void
g_value_init_default (GValue     *value,
		      GParamSpec *pspec)
{
  g_return_if_fail (value != NULL);
  g_return_if_fail (G_VALUE_TYPE (value) == 0);
  g_return_if_fail (G_IS_PARAM_SPEC (pspec));

  memset (value, 0, sizeof (*value));
  value->g_type = g_type_next_base (G_PARAM_SPEC_TYPE (pspec), G_TYPE_PARAM);

  G_PARAM_SPEC_GET_CLASS (pspec)->param_init (value, pspec);
}

gboolean
g_value_validate (GValue     *value,
		  GParamSpec *pspec)
{
  g_return_val_if_fail (G_IS_VALUE (value), FALSE);
  g_return_val_if_fail (G_IS_PARAM_SPEC (pspec), FALSE);
  g_return_val_if_fail (g_type_is_a (G_PARAM_SPEC_TYPE (pspec), G_VALUE_TYPE (value)), FALSE);

  if (G_PARAM_SPEC_GET_CLASS (pspec)->param_validate)
    {
      GValue oval = *value;
      
      if (G_PARAM_SPEC_GET_CLASS (pspec)->param_validate (value, pspec) ||
	  memcmp (&oval.data, &value->data, sizeof (oval.data)))
	return TRUE;
    }
  return FALSE;
}

gboolean
g_value_defaults (const GValue *value,
		  GParamSpec   *pspec)
{
  GValue dflt_value = { 0, };
  gboolean defaults;

  g_return_val_if_fail (G_IS_VALUE (value), FALSE);
  g_return_val_if_fail (G_IS_PARAM_SPEC (pspec), FALSE);
  g_return_val_if_fail (g_type_is_a (G_PARAM_SPEC_TYPE (pspec), G_VALUE_TYPE (value)), FALSE);
  
  dflt_value.g_type = g_type_next_base (G_PARAM_SPEC_TYPE (pspec), G_TYPE_PARAM);
  G_PARAM_SPEC_GET_CLASS (pspec)->param_init (&dflt_value, pspec);
  defaults = g_values_cmp (value, &dflt_value, pspec) == 0;
  g_value_unset (&dflt_value);

  return defaults;
}

void
g_value_set_default (GValue     *value,
		     GParamSpec *pspec)
{
  GValue tmp_value = { 0, };

  g_return_if_fail (G_IS_VALUE (value));
  g_return_if_fail (G_IS_PARAM_SPEC (pspec));
  g_return_if_fail (g_type_is_a (G_PARAM_SPEC_TYPE (pspec), G_VALUE_TYPE (value)));

  /* retrive default value */
  tmp_value.g_type = g_type_next_base (G_PARAM_SPEC_TYPE (pspec), G_TYPE_PARAM);
  G_PARAM_SPEC_GET_CLASS (pspec)->param_init (&tmp_value, pspec);

  /* set default value */
  g_values_exchange (&tmp_value, value);

  g_value_unset (&tmp_value);
}

gint
g_values_cmp (const GValue *value1,
	      const GValue *value2,
	      GParamSpec   *pspec)
{
  GParamSpecClass *pclass;
  gint cmp;

  /* param_values_cmp() effectively does: value1 - value2
   * so the return values are:
   * -1)  value1 < value2
   *  0)  value1 == value2
   *  1)  value1 > value2
   */
  g_return_val_if_fail (G_IS_VALUE (value1), 0);
  g_return_val_if_fail (G_IS_VALUE (value2), 0);
  g_return_val_if_fail (G_VALUE_TYPE (value1) == G_VALUE_TYPE (value2), 0);
  if (pspec)
    {
      g_return_val_if_fail (G_IS_PARAM_SPEC (pspec), 0);
      g_return_val_if_fail (g_type_is_a (G_PARAM_SPEC_TYPE (pspec), G_VALUE_TYPE (value1)), FALSE);
    }

  pclass = g_type_class_ref (G_VALUE_TYPE (value1));
  cmp = pclass->param_values_cmp (value1, value2, pspec);
  g_type_class_unref (pclass);

  return CLAMP (cmp, -1, 1);
}

void
g_value_copy (const GValue *src_value,
	      GValue       *dest_value)
{
  g_return_if_fail (G_IS_VALUE (src_value));
  g_return_if_fail (G_IS_VALUE (dest_value));
  g_return_if_fail (G_VALUE_TYPE (src_value) == G_VALUE_TYPE (dest_value));

  if (src_value != dest_value)
    {
      GParamSpecClass *pclass = g_type_class_ref (G_VALUE_TYPE (src_value));
      
      /* make sure dest_value's value is free()d and zero initialized */
      g_value_reset (dest_value);
      
      if (pclass->param_copy_value)
	pclass->param_copy_value (src_value, dest_value);
      else
	memcpy (&dest_value->data, &src_value->data, sizeof (src_value->data));
      g_type_class_unref (pclass);
    }
}

void
g_value_unset (GValue *value)
{
  GParamSpecClass *pclass;

  g_return_if_fail (G_IS_VALUE (value));

  pclass = g_type_class_ref (G_VALUE_TYPE (value));
  if (pclass->param_free_value)
    pclass->param_free_value (value);
  memset (value, 0, sizeof (*value));
  g_type_class_unref (pclass);
}

void
g_value_reset (GValue *value)
{
  GParamSpecClass *pclass;
  GType g_type;

  g_return_if_fail (G_IS_VALUE (value));

  g_type = G_VALUE_TYPE (value);
  pclass = g_type_class_ref (g_type);

  if (pclass->param_free_value)
    pclass->param_free_value (value);
  memset (value, 0, sizeof (*value));

  value->g_type = g_type;
  pclass->param_init (value, NULL);
  
  g_type_class_unref (pclass);
}

static gint
exchange_entries_equal (gconstpointer v1,
			gconstpointer v2)
{
  const ExchangeEntry *entry1 = v1;
  const ExchangeEntry *entry2 = v2;

  return (entry1->value_type1 == entry2->value_type1 &&
	  entry1->value_type2 == entry2->value_type2);
}

static guint
exchange_entry_hash (gconstpointer key)
{
  const ExchangeEntry *entry = key;

  return entry->value_type1 ^ entry->value_type2;
}

static void
value_exchange_memcpy (GValue *value1,
		       GValue *value2)
{
  GValue tmp_value;

  memcpy (&tmp_value.data, &value1->data, sizeof (value1->data));
  memcpy (&value1->data, &value2->data, sizeof (value1->data));
  memcpy (&value2->data, &tmp_value.data, sizeof (value2->data));
}

static inline GValueExchange
exchange_func_lookup (GType     value_type1,
		      GType     value_type2,
		      gboolean *need_swap)
{
  if (value_type1 == value_type2)
    return value_exchange_memcpy;
  else
    {
      ExchangeEntry entry, *ret;
      
      entry.value_type1 = MIN (value_type1, value_type2);
      entry.value_type2 = MAX (value_type1, value_type2);
      
      ret = g_hash_table_lookup (param_exchange_ht, &entry);
      if (ret)
	{
	  if (need_swap)
	    *need_swap = ret->first_type == value_type1;

	  return ret->func;
	}
    }
  return NULL;
}

void
g_value_register_exchange_func (GType          value_type1,
				GType          value_type2,
				GValueExchange func)
{
  GType type1, type2;

  g_return_if_fail (G_TYPE_IS_VALUE (value_type1));
  g_return_if_fail (G_TYPE_IS_VALUE (value_type2));
  g_return_if_fail (func != NULL);

  type1 = g_type_next_base (value_type1, G_TYPE_PARAM);
  type2 = g_type_next_base (value_type2, G_TYPE_PARAM);

  if (param_exchange_ht && exchange_func_lookup (type1, type2, NULL))
    g_warning (G_STRLOC ": cannot re-register param value exchange function "
	       "for `%s' and `%s'",
	       g_type_name (type1),
	       g_type_name (type2));
  else
    {
      ExchangeEntry *entry = g_new (ExchangeEntry, 1);

      if (!param_exchange_ht)
	param_exchange_ht = g_hash_table_new (exchange_entry_hash, exchange_entries_equal);
      entry->value_type1 = MIN (type1, type2);
      entry->value_type2 = MAX (type1, type2);
      entry->func = func;
      entry->first_type = type1;
      g_hash_table_insert (param_exchange_ht, entry, entry);
    }
}

gboolean
g_value_types_exchangable (GType value_type1,
			   GType value_type2)
{
  GType type1, type2;

  g_return_val_if_fail (G_TYPE_IS_VALUE (value_type1), FALSE);
  g_return_val_if_fail (G_TYPE_IS_VALUE (value_type2), FALSE);

  type1 = g_type_next_base (value_type1, G_TYPE_PARAM);
  type2 = g_type_next_base (value_type2, G_TYPE_PARAM);

  return exchange_func_lookup (type1, type2, NULL) != NULL;
}

gboolean
g_values_exchange (GValue *value1,
		   GValue *value2)
{
  g_return_val_if_fail (G_IS_VALUE (value1), FALSE);
  g_return_val_if_fail (G_IS_VALUE (value2), FALSE);

  if (value1 != value2)
    {
      GType type1 = g_type_next_base (G_VALUE_TYPE (value1), G_TYPE_PARAM);
      GType type2 = g_type_next_base (G_VALUE_TYPE (value2), G_TYPE_PARAM);
      gboolean need_swap;
      GValueExchange value_exchange = exchange_func_lookup (type1,
							    type2,
							    &need_swap);
      if (value_exchange)
	{
	  if (need_swap)
	    value_exchange (value2, value1);
	  else
	    value_exchange (value1, value2);
	}

      return value_exchange != NULL;
    }

  return TRUE;
}

gboolean
g_value_convert (const GValue *src_value,
		 GValue       *dest_value)
{
  gboolean success = TRUE;

  g_return_val_if_fail (G_IS_VALUE (src_value), FALSE);
  g_return_val_if_fail (G_IS_VALUE (dest_value), FALSE);

  if (src_value != dest_value)
    {
      GValue tmp_value = { 0, };

      g_value_init (&tmp_value, G_VALUE_TYPE (src_value));
      g_value_copy (src_value, &tmp_value);
      
      success = g_values_exchange (&tmp_value, dest_value);
      g_value_unset (&tmp_value);
    }

  return success;
}
