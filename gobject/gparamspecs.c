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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
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

#include	"gparamspecs.h"

#include	"gvaluecollector.h"
#include	<string.h>
#include	"../config.h"	/* for SIZEOF_LONG */

#define	G_FLOAT_EPSILON		(1e-30)
#define	G_DOUBLE_EPSILON	(1e-90)


/* --- param spec functions --- */
static void
param_spec_char_init (GParamSpec *pspec)
{
  GParamSpecChar *cspec = G_PARAM_SPEC_CHAR (pspec);
  
  cspec->minimum = 0x7f;
  cspec->maximum = 0x80;
  cspec->default_value = 0;
}

static void
param_char_set_default (GParamSpec *pspec,
			GValue	   *value)
{
  value->data[0].v_int = G_PARAM_SPEC_CHAR (pspec)->default_value;
}

static gboolean
param_char_validate (GParamSpec *pspec,
		     GValue     *value)
{
  GParamSpecChar *cspec = G_PARAM_SPEC_CHAR (pspec);
  gint oval = value->data[0].v_int;
  
  value->data[0].v_int = CLAMP (value->data[0].v_int, cspec->minimum, cspec->maximum);
  
  return value->data[0].v_int != oval;
}

static void
param_spec_uchar_init (GParamSpec *pspec)
{
  GParamSpecUChar *uspec = G_PARAM_SPEC_UCHAR (pspec);
  
  uspec->minimum = 0;
  uspec->maximum = 0xff;
  uspec->default_value = 0;
}

static void
param_uchar_set_default (GParamSpec *pspec,
			 GValue	    *value)
{
  value->data[0].v_uint = G_PARAM_SPEC_UCHAR (pspec)->default_value;
}

static gboolean
param_uchar_validate (GParamSpec *pspec,
		      GValue     *value)
{
  GParamSpecUChar *uspec = G_PARAM_SPEC_UCHAR (pspec);
  guint oval = value->data[0].v_uint;
  
  value->data[0].v_uint = CLAMP (value->data[0].v_uint, uspec->minimum, uspec->maximum);
  
  return value->data[0].v_uint != oval;
}

static void
param_boolean_set_default (GParamSpec *pspec,
			   GValue     *value)
{
  value->data[0].v_int = G_PARAM_SPEC_BOOLEAN (pspec)->default_value;
}

static gboolean
param_boolean_validate (GParamSpec *pspec,
			GValue     *value)
{
  gint oval = value->data[0].v_int;
  
  value->data[0].v_int = value->data[0].v_int != FALSE;
  
  return value->data[0].v_int != oval;
}

static void
param_spec_int_init (GParamSpec *pspec)
{
  GParamSpecInt *ispec = G_PARAM_SPEC_INT (pspec);
  
  ispec->minimum = 0x7fffffff;
  ispec->maximum = 0x80000000;
  ispec->default_value = 0;
}

static void
param_int_set_default (GParamSpec *pspec,
		       GValue     *value)
{
  value->data[0].v_int = G_PARAM_SPEC_INT (pspec)->default_value;
}

static gboolean
param_int_validate (GParamSpec *pspec,
		    GValue     *value)
{
  GParamSpecInt *ispec = G_PARAM_SPEC_INT (pspec);
  gint oval = value->data[0].v_int;
  
  value->data[0].v_int = CLAMP (value->data[0].v_int, ispec->minimum, ispec->maximum);
  
  return value->data[0].v_int != oval;
}

static gint
param_int_values_cmp (GParamSpec   *pspec,
		      const GValue *value1,
		      const GValue *value2)
{
  if (value1->data[0].v_int < value2->data[0].v_int)
    return -1;
  else
    return value1->data[0].v_int > value2->data[0].v_int;
}

static void
param_spec_uint_init (GParamSpec *pspec)
{
  GParamSpecUInt *uspec = G_PARAM_SPEC_UINT (pspec);
  
  uspec->minimum = 0;
  uspec->maximum = 0xffffffff;
  uspec->default_value = 0;
}

static void
param_uint_set_default (GParamSpec *pspec,
			GValue     *value)
{
  value->data[0].v_uint = G_PARAM_SPEC_UINT (pspec)->default_value;
}

static gboolean
param_uint_validate (GParamSpec *pspec,
		     GValue     *value)
{
  GParamSpecUInt *uspec = G_PARAM_SPEC_UINT (pspec);
  guint oval = value->data[0].v_uint;
  
  value->data[0].v_uint = CLAMP (value->data[0].v_uint, uspec->minimum, uspec->maximum);
  
  return value->data[0].v_uint != oval;
}

static gint
param_uint_values_cmp (GParamSpec   *pspec,
		       const GValue *value1,
		       const GValue *value2)
{
  if (value1->data[0].v_uint < value2->data[0].v_uint)
    return -1;
  else
    return value1->data[0].v_uint > value2->data[0].v_uint;
}

static void
param_spec_long_init (GParamSpec *pspec)
{
  GParamSpecLong *lspec = G_PARAM_SPEC_LONG (pspec);
  
#if SIZEOF_LONG == 4
  lspec->minimum = 0x7fffffff;
  lspec->maximum = 0x80000000;
#else /* SIZEOF_LONG != 4 (8) */
  lspec->minimum = 0x7fffffffffffffff;
  lspec->maximum = 0x8000000000000000;
#endif
  lspec->default_value = 0;
}

static void
param_long_set_default (GParamSpec *pspec,
			GValue     *value)
{
  value->data[0].v_long = G_PARAM_SPEC_LONG (pspec)->default_value;
}

static gboolean
param_long_validate (GParamSpec *pspec,
		     GValue     *value)
{
  GParamSpecLong *lspec = G_PARAM_SPEC_LONG (pspec);
  glong oval = value->data[0].v_long;
  
  value->data[0].v_long = CLAMP (value->data[0].v_long, lspec->minimum, lspec->maximum);
  
  return value->data[0].v_long != oval;
}

static gint
param_long_values_cmp (GParamSpec   *pspec,
		       const GValue *value1,
		       const GValue *value2)
{
  if (value1->data[0].v_long < value2->data[0].v_long)
    return -1;
  else
    return value1->data[0].v_long > value2->data[0].v_long;
}

static void
param_spec_ulong_init (GParamSpec *pspec)
{
  GParamSpecULong *uspec = G_PARAM_SPEC_ULONG (pspec);
  
  uspec->minimum = 0;
#if SIZEOF_LONG == 4
  uspec->maximum = 0xffffffff;
#else /* SIZEOF_LONG != 4 (8) */
  uspec->maximum = 0xffffffffffffffff;
#endif
  uspec->default_value = 0;
}

static void
param_ulong_set_default (GParamSpec *pspec,
			 GValue     *value)
{
  value->data[0].v_ulong = G_PARAM_SPEC_ULONG (pspec)->default_value;
}

static gboolean
param_ulong_validate (GParamSpec *pspec,
		      GValue     *value)
{
  GParamSpecULong *uspec = G_PARAM_SPEC_ULONG (pspec);
  gulong oval = value->data[0].v_ulong;
  
  value->data[0].v_ulong = CLAMP (value->data[0].v_ulong, uspec->minimum, uspec->maximum);
  
  return value->data[0].v_ulong != oval;
}

static gint
param_ulong_values_cmp (GParamSpec   *pspec,
			const GValue *value1,
			const GValue *value2)
{
  if (value1->data[0].v_ulong < value2->data[0].v_ulong)
    return -1;
  else
    return value1->data[0].v_ulong > value2->data[0].v_ulong;
}

static void
param_spec_enum_init (GParamSpec *pspec)
{
  GParamSpecEnum *espec = G_PARAM_SPEC_ENUM (pspec);
  
  espec->enum_class = NULL;
  espec->default_value = 0;
}

static void
param_spec_enum_finalize (GParamSpec *pspec)
{
  GParamSpecEnum *espec = G_PARAM_SPEC_ENUM (pspec);
  GParamSpecClass *parent_class = g_type_class_peek (g_type_parent (G_TYPE_PARAM_ENUM));
  
  if (espec->enum_class)
    {
      g_type_class_unref (espec->enum_class);
      espec->enum_class = NULL;
    }
  
  parent_class->finalize (pspec);
}

static void
param_enum_set_default (GParamSpec *pspec,
			GValue     *value)
{
  value->data[0].v_long = G_PARAM_SPEC_ENUM (pspec)->default_value;
}

static gboolean
param_enum_validate (GParamSpec *pspec,
		     GValue     *value)
{
  GParamSpecEnum *espec = G_PARAM_SPEC_ENUM (pspec);
  glong oval = value->data[0].v_long;
  
  if (!espec->enum_class ||
      !g_enum_get_value (espec->enum_class, value->data[0].v_long))
    value->data[0].v_long = espec->default_value;
  
  return value->data[0].v_long != oval;
}

static void
param_spec_flags_init (GParamSpec *pspec)
{
  GParamSpecFlags *fspec = G_PARAM_SPEC_FLAGS (pspec);
  
  fspec->flags_class = NULL;
  fspec->default_value = 0;
}

static void
param_spec_flags_finalize (GParamSpec *pspec)
{
  GParamSpecFlags *fspec = G_PARAM_SPEC_FLAGS (pspec);
  GParamSpecClass *parent_class = g_type_class_peek (g_type_parent (G_TYPE_PARAM_FLAGS));
  
  if (fspec->flags_class)
    {
      g_type_class_unref (fspec->flags_class);
      fspec->flags_class = NULL;
    }
  
  parent_class->finalize (pspec);
}

static void
param_flags_set_default (GParamSpec *pspec,
			 GValue     *value)
{
  value->data[0].v_ulong = G_PARAM_SPEC_FLAGS (pspec)->default_value;
}

static gboolean
param_flags_validate (GParamSpec *pspec,
		      GValue     *value)
{
  GParamSpecFlags *fspec = G_PARAM_SPEC_FLAGS (pspec);
  gulong oval = value->data[0].v_ulong;
  
  if (fspec->flags_class)
    value->data[0].v_ulong &= fspec->flags_class->mask;
  else
    value->data[0].v_ulong = fspec->default_value;
  
  return value->data[0].v_ulong != oval;
}

static void
param_spec_float_init (GParamSpec *pspec)
{
  GParamSpecFloat *fspec = G_PARAM_SPEC_FLOAT (pspec);
  
  fspec->minimum = G_MINFLOAT;
  fspec->maximum = G_MAXFLOAT;
  fspec->default_value = 0;
  fspec->epsilon = G_FLOAT_EPSILON;
}

static void
param_float_set_default (GParamSpec *pspec,
			 GValue     *value)
{
  value->data[0].v_float = G_PARAM_SPEC_FLOAT (pspec)->default_value;
}

static gboolean
param_float_validate (GParamSpec *pspec,
		      GValue     *value)
{
  GParamSpecFloat *fspec = G_PARAM_SPEC_FLOAT (pspec);
  gfloat oval = value->data[0].v_float;
  
  value->data[0].v_float = CLAMP (value->data[0].v_float, fspec->minimum, fspec->maximum);
  
  return value->data[0].v_float != oval;
}

static gint
param_float_values_cmp (GParamSpec   *pspec,
			const GValue *value1,
			const GValue *value2)
{
  gfloat epsilon = G_PARAM_SPEC_FLOAT (pspec)->epsilon;
  
  if (value1->data[0].v_float < value2->data[0].v_float)
    return - (value2->data[0].v_float - value1->data[0].v_float > epsilon);
  else
    return value1->data[0].v_float - value2->data[0].v_float > epsilon;
}

static void
param_spec_double_init (GParamSpec *pspec)
{
  GParamSpecDouble *dspec = G_PARAM_SPEC_DOUBLE (pspec);
  
  dspec->minimum = G_MINDOUBLE;
  dspec->maximum = G_MAXDOUBLE;
  dspec->default_value = 0;
  dspec->epsilon = G_DOUBLE_EPSILON;
}

static void
param_double_set_default (GParamSpec *pspec,
			  GValue     *value)
{
  value->data[0].v_double = G_PARAM_SPEC_DOUBLE (pspec)->default_value;
}

static gboolean
param_double_validate (GParamSpec *pspec,
		       GValue     *value)
{
  GParamSpecDouble *dspec = G_PARAM_SPEC_DOUBLE (pspec);
  gdouble oval = value->data[0].v_double;
  
  value->data[0].v_double = CLAMP (value->data[0].v_double, dspec->minimum, dspec->maximum);
  
  return value->data[0].v_double != oval;
}

static gint
param_double_values_cmp (GParamSpec   *pspec,
			 const GValue *value1,
			 const GValue *value2)
{
  gdouble epsilon = G_PARAM_SPEC_DOUBLE (pspec)->epsilon;
  
  if (value1->data[0].v_double < value2->data[0].v_double)
    return - (value2->data[0].v_double - value1->data[0].v_double > epsilon);
  else
    return value1->data[0].v_double - value2->data[0].v_double > epsilon;
}

static void
param_spec_string_init (GParamSpec *pspec)
{
  GParamSpecString *sspec = G_PARAM_SPEC_STRING (pspec);
  
  sspec->default_value = NULL;
  sspec->cset_first = NULL;
  sspec->cset_nth = NULL;
  sspec->substitutor = '_';
  sspec->null_fold_if_empty = FALSE;
  sspec->ensure_non_null = FALSE;
}

static void
param_spec_string_finalize (GParamSpec *pspec)
{
  GParamSpecString *sspec = G_PARAM_SPEC_STRING (pspec);
  GParamSpecClass *parent_class = g_type_class_peek (g_type_parent (G_TYPE_PARAM_STRING));
  
  g_free (sspec->default_value);
  g_free (sspec->cset_first);
  g_free (sspec->cset_nth);
  sspec->default_value = NULL;
  sspec->cset_first = NULL;
  sspec->cset_nth = NULL;
  
  parent_class->finalize (pspec);
}

static void
param_string_set_default (GParamSpec *pspec,
			  GValue     *value)
{
  value->data[0].v_pointer = g_strdup (G_PARAM_SPEC_STRING (pspec)->default_value);
}

static gboolean
param_string_validate (GParamSpec *pspec,
		       GValue     *value)
{
  GParamSpecString *sspec = G_PARAM_SPEC_STRING (pspec);
  gchar *string = value->data[0].v_pointer;
  guint changed = 0;
  
  if (string && string[0])
    {
      gchar *s;
      
      if (sspec->cset_first && !strchr (sspec->cset_first, string[0]))
	{
	  string[0] = sspec->substitutor;
	  changed++;
	}
      if (sspec->cset_nth)
	for (s = string + 1; *s; s++)
	  if (!strchr (sspec->cset_nth, *s))
	    {
	      *s = sspec->substitutor;
	      changed++;
	    }
    }
  if (sspec->null_fold_if_empty && string && string[0] == 0)
    {
      g_free (value->data[0].v_pointer);
      value->data[0].v_pointer = NULL;
      changed++;
      string = value->data[0].v_pointer;
    }
  if (sspec->ensure_non_null && !string)
    {
      value->data[0].v_pointer = g_strdup ("");
      changed++;
      string = value->data[0].v_pointer;
    }
  
  return changed;
}

static gint
param_string_values_cmp (GParamSpec   *pspec,
			 const GValue *value1,
			 const GValue *value2)
{
  if (!value1->data[0].v_pointer)
    return value2->data[0].v_pointer != NULL ? -1 : 0;
  else if (!value2->data[0].v_pointer)
    return value1->data[0].v_pointer != NULL;
  else
    return strcmp (value1->data[0].v_pointer, value2->data[0].v_pointer);
}

static void
param_spec_param_init (GParamSpec *pspec)
{
  /* GParamSpecParam *spec = G_PARAM_SPEC_PARAM (pspec); */
}

static void
param_param_set_default (GParamSpec *pspec,
			 GValue     *value)
{
  value->data[0].v_pointer = NULL;
}

static gboolean
param_param_validate (GParamSpec *pspec,
		      GValue     *value)
{
  /* GParamSpecParam *spec = G_PARAM_SPEC_PARAM (pspec); */
  GParamSpec *param = value->data[0].v_pointer;
  guint changed = 0;
  
  if (param && !g_type_is_a (G_PARAM_SPEC_TYPE (param), G_PARAM_SPEC_VALUE_TYPE (pspec)))
    {
      g_param_spec_unref (param);
      value->data[0].v_pointer = NULL;
      changed++;
    }
  
  return changed;
}

static void
param_spec_pointer_init (GParamSpec *pspec)
{
  /* GParamSpecPointer *spec = G_PARAM_SPEC_POINTER (pspec); */
}

static void
param_pointer_set_default (GParamSpec *pspec,
			   GValue     *value)
{
  value->data[0].v_pointer = NULL;
}

static gboolean
param_pointer_validate (GParamSpec *pspec,
			GValue     *value)
{
  /* GParamSpecPointer *spec = G_PARAM_SPEC_POINTER (pspec); */
  guint changed = 0;
  
  return changed;
}

static gint
param_pointer_values_cmp (GParamSpec   *pspec,
			  const GValue *value1,
			  const GValue *value2)
{
  return value1->data[0].v_pointer != value2->data[0].v_pointer;
}

static void
param_spec_ccallback_init (GParamSpec *pspec)
{
  /* GParamSpecCCallback *spec = G_PARAM_SPEC_CCALLBACK (pspec); */
}

static void
param_ccallback_set_default (GParamSpec *pspec,
			     GValue     *value)
{
  value->data[0].v_pointer = NULL;
  value->data[1].v_pointer = NULL;
}

static gboolean
param_ccallback_validate (GParamSpec *pspec,
			  GValue     *value)
{
  /* GParamSpecCCallback *spec = G_PARAM_SPEC_CCALLBACK (pspec); */
  guint changed = 0;
  
  return changed;
}

static gint
param_ccallback_values_cmp (GParamSpec   *pspec,
			    const GValue *value1,
			    const GValue *value2)
{
  return (value1->data[0].v_pointer != value2->data[0].v_pointer ||
	  value1->data[1].v_pointer != value2->data[1].v_pointer);
}

static void
param_spec_boxed_init (GParamSpec *pspec)
{
  /* GParamSpecBoxed *bspec = G_PARAM_SPEC_BOXED (pspec); */
}

static void
param_boxed_set_default (GParamSpec *pspec,
			 GValue     *value)
{
  value->data[0].v_pointer = NULL;
}

static gboolean
param_boxed_validate (GParamSpec *pspec,
		      GValue     *value)
{
  /* GParamSpecBoxed *bspec = G_PARAM_SPEC_BOXED (pspec); */
  guint changed = 0;

  /* can't do a whole lot here since we haven't even G_BOXED_TYPE() */
  
  return changed;
}

static gint
param_boxed_values_cmp (GParamSpec    *pspec,
			 const GValue *value1,
			 const GValue *value2)
{
  return value1->data[0].v_pointer != value2->data[0].v_pointer;
}

static void
param_spec_object_init (GParamSpec *pspec)
{
  /* GParamSpecObject *ospec = G_PARAM_SPEC_OBJECT (pspec); */
}

static void
param_object_set_default (GParamSpec *pspec,
			  GValue     *value)
{
  value->data[0].v_pointer = NULL;
}

static gboolean
param_object_validate (GParamSpec *pspec,
		       GValue     *value)
{
  GParamSpecObject *ospec = G_PARAM_SPEC_OBJECT (pspec);
  GObject *object = value->data[0].v_pointer;
  guint changed = 0;
  
  if (object && !g_type_is_a (G_OBJECT_TYPE (object), G_PARAM_SPEC_VALUE_TYPE (ospec)))
    {
      g_object_unref (object);
      value->data[0].v_pointer = NULL;
      changed++;
    }
  
  return changed;
}

static gint
param_object_values_cmp (GParamSpec   *pspec,
			 const GValue *value1,
			 const GValue *value2)
{
  return value1->data[0].v_pointer != value2->data[0].v_pointer;
}

static void
value_exch_memcpy (GValue *value1,
		   GValue *value2)
{
  GValue tmp_value;
  memcpy (&tmp_value.data, &value1->data, sizeof (value1->data));
  memcpy (&value1->data, &value2->data, sizeof (value1->data));
  memcpy (&value2->data, &tmp_value.data, sizeof (value2->data));
}

static void
value_exch_long_int (GValue *value1,
		     GValue *value2)
{
  glong tmp = value1->data[0].v_long;
  value1->data[0].v_long = value2->data[0].v_int;
  value2->data[0].v_int = tmp;
}

static void
value_exch_long_uint (GValue *value1,
		      GValue *value2)
{
  glong tmp = value1->data[0].v_long;
  value1->data[0].v_long = value2->data[0].v_uint;
  value2->data[0].v_uint = tmp;
}

static void
value_exch_ulong_int (GValue *value1,
		      GValue *value2)
{
  gulong tmp = value1->data[0].v_ulong;
  value1->data[0].v_ulong = value2->data[0].v_int;
  value2->data[0].v_int = tmp;
}

static void
value_exch_ulong_uint (GValue *value1,
		       GValue *value2)
{
  gulong tmp = value1->data[0].v_ulong;
  value1->data[0].v_ulong = value2->data[0].v_uint;
  value2->data[0].v_uint = tmp;
}

static void
value_exch_float_int (GValue *value1,
		      GValue *value2)
{
  gfloat tmp = value1->data[0].v_float;
  value1->data[0].v_float = value2->data[0].v_int;
  value2->data[0].v_int = 0.5 + tmp;
}

static void
value_exch_float_uint (GValue *value1,
		       GValue *value2)
{
  gfloat tmp = value1->data[0].v_float;
  value1->data[0].v_float = value2->data[0].v_uint;
  value2->data[0].v_uint = 0.5 + tmp;
}

static void
value_exch_float_long (GValue *value1,
		       GValue *value2)
{
  gfloat tmp = value1->data[0].v_float;
  value1->data[0].v_float = value2->data[0].v_long;
  value2->data[0].v_long = 0.5 + tmp;
}

static void
value_exch_float_ulong (GValue *value1,
			GValue *value2)
{
  gfloat tmp = value1->data[0].v_float;
  value1->data[0].v_float = value2->data[0].v_ulong;
  value2->data[0].v_ulong = 0.5 + tmp;
}

static void
value_exch_double_int (GValue *value1,
		       GValue *value2)
{
  gdouble tmp = value1->data[0].v_double;
  value1->data[0].v_double = value2->data[0].v_int;
  value2->data[0].v_int = 0.5 + tmp;
}

static void
value_exch_double_uint (GValue *value1,
			GValue *value2)
{
  gdouble tmp = value1->data[0].v_double;
  value1->data[0].v_double = value2->data[0].v_uint;
  value2->data[0].v_uint = 0.5 + tmp;
}

static void
value_exch_double_long (GValue *value1,
			GValue *value2)
{
  gdouble tmp = value1->data[0].v_double;
  value1->data[0].v_double = value2->data[0].v_long;
  value2->data[0].v_long = 0.5 + tmp;
}

static void
value_exch_double_ulong (GValue *value1,
			 GValue *value2)
{
  gdouble tmp = value1->data[0].v_double;
  value1->data[0].v_double = value2->data[0].v_ulong;
  value2->data[0].v_ulong = 0.5 + tmp;
}

static void
value_exch_double_float (GValue *value1,
			 GValue *value2)
{
  gdouble tmp = value1->data[0].v_double;
  value1->data[0].v_double = value2->data[0].v_float;
  value2->data[0].v_float = tmp;
}


/* --- type initialization --- */
void
g_param_spec_types_init (void)	/* sync with gtype.c */
{
  GType type;
  
  /* G_TYPE_PARAM_CHAR
   */
  {
    static const GParamSpecTypeInfo pspec_info = {
      sizeof (GParamSpecChar),	/* instance_size */
      16,			/* n_preallocs */
      param_spec_char_init,	/* instance_init */
      G_TYPE_CHAR,		/* value_type */
      NULL,			/* finalize */
      param_char_set_default,	/* value_set_default */
      param_char_validate,	/* value_validate */
      param_int_values_cmp,	/* values_cmp */
    };
    type = g_param_type_register_static ("GParamChar", &pspec_info);
    g_assert (type == G_TYPE_PARAM_CHAR);
  }
  
  /* G_TYPE_PARAM_UCHAR
   */
  {
    static const GParamSpecTypeInfo pspec_info = {
      sizeof (GParamSpecUChar), /* instance_size */
      16,                       /* n_preallocs */
      param_spec_uchar_init,    /* instance_init */
      G_TYPE_UCHAR,		/* value_type */
      NULL,			/* finalize */
      param_uchar_set_default,	/* value_set_default */
      param_uchar_validate,	/* value_validate */
      param_uint_values_cmp,	/* values_cmp */
    };
    type = g_param_type_register_static ("GParamUChar", &pspec_info);
    g_assert (type == G_TYPE_PARAM_UCHAR);
  }
  
  /* G_TYPE_PARAM_BOOLEAN
   */
  {
    static const GParamSpecTypeInfo pspec_info = {
      sizeof (GParamSpecBoolean), /* instance_size */
      16,                         /* n_preallocs */
      NULL,			  /* instance_init */
      G_TYPE_BOOLEAN,             /* value_type */
      NULL,                       /* finalize */
      param_boolean_set_default,  /* value_set_default */
      param_boolean_validate,     /* value_validate */
      param_int_values_cmp,       /* values_cmp */
    };
    type = g_param_type_register_static ("GParamBoolean", &pspec_info);
    g_assert (type == G_TYPE_PARAM_BOOLEAN);
  }
  
  /* G_TYPE_PARAM_INT
   */
  {
    static const GParamSpecTypeInfo pspec_info = {
      sizeof (GParamSpecInt),   /* instance_size */
      16,                       /* n_preallocs */
      param_spec_int_init,      /* instance_init */
      G_TYPE_INT,		/* value_type */
      NULL,			/* finalize */
      param_int_set_default,	/* value_set_default */
      param_int_validate,	/* value_validate */
      param_int_values_cmp,	/* values_cmp */
    };
    type = g_param_type_register_static ("GParamInt", &pspec_info);
    g_assert (type == G_TYPE_PARAM_INT);
  }
  
  /* G_TYPE_PARAM_UINT
   */
  {
    static const GParamSpecTypeInfo pspec_info = {
      sizeof (GParamSpecUInt),  /* instance_size */
      16,                       /* n_preallocs */
      param_spec_uint_init,     /* instance_init */
      G_TYPE_UINT,		/* value_type */
      NULL,			/* finalize */
      param_uint_set_default,	/* value_set_default */
      param_uint_validate,	/* value_validate */
      param_uint_values_cmp,	/* values_cmp */
    };
    type = g_param_type_register_static ("GParamUInt", &pspec_info);
    g_assert (type == G_TYPE_PARAM_UINT);
  }
  
  /* G_TYPE_PARAM_LONG
   */
  {
    static const GParamSpecTypeInfo pspec_info = {
      sizeof (GParamSpecLong),  /* instance_size */
      16,                       /* n_preallocs */
      param_spec_long_init,     /* instance_init */
      G_TYPE_LONG,		/* value_type */
      NULL,			/* finalize */
      param_long_set_default,	/* value_set_default */
      param_long_validate,	/* value_validate */
      param_long_values_cmp,	/* values_cmp */
    };
    type = g_param_type_register_static ("GParamLong", &pspec_info);
    g_assert (type == G_TYPE_PARAM_LONG);
  }
  
  /* G_TYPE_PARAM_ULONG
   */
  {
    static const GParamSpecTypeInfo pspec_info = {
      sizeof (GParamSpecULong), /* instance_size */
      16,                       /* n_preallocs */
      param_spec_ulong_init,    /* instance_init */
      G_TYPE_ULONG,		/* value_type */
      NULL,			/* finalize */
      param_ulong_set_default,	/* value_set_default */
      param_ulong_validate,	/* value_validate */
      param_ulong_values_cmp,	/* values_cmp */
    };
    type = g_param_type_register_static ("GParamULong", &pspec_info);
    g_assert (type == G_TYPE_PARAM_ULONG);
  }
  
  /* G_TYPE_PARAM_ENUM
   */
  {
    static const GParamSpecTypeInfo pspec_info = {
      sizeof (GParamSpecEnum),  /* instance_size */
      16,                       /* n_preallocs */
      param_spec_enum_init,     /* instance_init */
      G_TYPE_ENUM,		/* value_type */
      param_spec_enum_finalize,	/* finalize */
      param_enum_set_default,	/* value_set_default */
      param_enum_validate,	/* value_validate */
      param_long_values_cmp,	/* values_cmp */
    };
    type = g_param_type_register_static ("GParamEnum", &pspec_info);
    g_assert (type == G_TYPE_PARAM_ENUM);
  }
  
  /* G_TYPE_PARAM_FLAGS
   */
  {
    static const GParamSpecTypeInfo pspec_info = {
      sizeof (GParamSpecFlags), /* instance_size */
      16,                       /* n_preallocs */
      param_spec_flags_init,    /* instance_init */
      G_TYPE_FLAGS,		/* value_type */
      param_spec_flags_finalize,/* finalize */
      param_flags_set_default,	/* value_set_default */
      param_flags_validate,	/* value_validate */
      param_ulong_values_cmp,	/* values_cmp */
    };
    type = g_param_type_register_static ("GParamFlags", &pspec_info);
    g_assert (type == G_TYPE_PARAM_FLAGS);
  }
  
  /* G_TYPE_PARAM_FLOAT
   */
  {
    static const GParamSpecTypeInfo pspec_info = {
      sizeof (GParamSpecFloat), /* instance_size */
      16,                       /* n_preallocs */
      param_spec_float_init,    /* instance_init */
      G_TYPE_FLOAT,		/* value_type */
      NULL,			/* finalize */
      param_float_set_default,	/* value_set_default */
      param_float_validate,	/* value_validate */
      param_float_values_cmp,	/* values_cmp */
    };
    type = g_param_type_register_static ("GParamFloat", &pspec_info);
    g_assert (type == G_TYPE_PARAM_FLOAT);
  }
  
  /* G_TYPE_PARAM_DOUBLE
   */
  {
    static const GParamSpecTypeInfo pspec_info = {
      sizeof (GParamSpecDouble), /* instance_size */
      16,                        /* n_preallocs */
      param_spec_double_init,    /* instance_init */
      G_TYPE_DOUBLE,		 /* value_type */
      NULL,			 /* finalize */
      param_double_set_default,	 /* value_set_default */
      param_double_validate,	 /* value_validate */
      param_double_values_cmp,	 /* values_cmp */
    };
    type = g_param_type_register_static ("GParamDouble", &pspec_info);
    g_assert (type == G_TYPE_PARAM_DOUBLE);
  }
  
  /* G_TYPE_PARAM_STRING
   */
  {
    static const GParamSpecTypeInfo pspec_info = {
      sizeof (GParamSpecString),  /* instance_size */
      16,			  /* n_preallocs */
      param_spec_string_init,	  /* instance_init */
      G_TYPE_STRING,		  /* value_type */
      param_spec_string_finalize, /* finalize */
      param_string_set_default,	  /* value_set_default */
      param_string_validate,	  /* value_validate */
      param_string_values_cmp,	  /* values_cmp */
    };
    type = g_param_type_register_static ("GParamString", &pspec_info);
    g_assert (type == G_TYPE_PARAM_STRING);
  }
  
  /* G_TYPE_PARAM_PARAM
   */
  {
    static const GParamSpecTypeInfo pspec_info = {
      sizeof (GParamSpecParam),  /* instance_size */
      16,                        /* n_preallocs */
      param_spec_param_init,     /* instance_init */
      G_TYPE_PARAM,		 /* value_type */
      NULL,			 /* finalize */
      param_param_set_default,	 /* value_set_default */
      param_param_validate,	 /* value_validate */
      param_pointer_values_cmp,	 /* values_cmp */
    };
    type = g_param_type_register_static ("GParamParam", &pspec_info);
    g_assert (type == G_TYPE_PARAM_PARAM);
  }
  
  /* G_TYPE_PARAM_POINTER
   */
  {
    static const GParamSpecTypeInfo pspec_info = {
      sizeof (GParamSpecPointer),  /* instance_size */
      0,                           /* n_preallocs */
      param_spec_pointer_init,     /* instance_init */
      G_TYPE_POINTER,  		   /* value_type */
      NULL,			   /* finalize */
      param_pointer_set_default,   /* value_set_default */
      param_pointer_validate,	   /* value_validate */
      param_pointer_values_cmp,	   /* values_cmp */
    };
    type = g_param_type_register_static ("GParamPointer", &pspec_info);
    g_assert (type == G_TYPE_PARAM_POINTER);
  }
  
  /* G_TYPE_PARAM_CCALLBACK
   */
  {
    static const GParamSpecTypeInfo pspec_info = {
      sizeof (GParamSpecCCallback), /* instance_size */
      0,                            /* n_preallocs */
      param_spec_ccallback_init,    /* instance_init */
      G_TYPE_CCALLBACK,		    /* value_type */
      NULL,			    /* finalize */
      param_ccallback_set_default,  /* value_set_default */
      param_ccallback_validate,	    /* value_validate */
      param_ccallback_values_cmp,   /* values_cmp */
    };
    type = g_param_type_register_static ("GParamCCallback", &pspec_info);
    g_assert (type == G_TYPE_PARAM_CCALLBACK);
  }
  
  /* G_TYPE_PARAM_BOXED
   */
  {
    static const GParamSpecTypeInfo pspec_info = {
      sizeof (GParamSpecBoxed),  /* instance_size */
      4,                         /* n_preallocs */
      param_spec_boxed_init,     /* instance_init */
      G_TYPE_BOXED,		 /* value_type */
      NULL,			 /* finalize */
      param_boxed_set_default,	 /* value_set_default */
      param_boxed_validate,	 /* value_validate */
      param_boxed_values_cmp,	 /* values_cmp */
    };
    type = g_param_type_register_static ("GParamBoxed", &pspec_info);
    g_assert (type == G_TYPE_PARAM_BOXED);
  }

  /* G_TYPE_PARAM_OBJECT
   */
  {
    static const GParamSpecTypeInfo pspec_info = {
      sizeof (GParamSpecObject), /* instance_size */
      16,                        /* n_preallocs */
      param_spec_object_init,    /* instance_init */
      G_TYPE_OBJECT,		 /* value_type */
      NULL,			 /* finalize */
      param_object_set_default,	 /* value_set_default */
      param_object_validate,	 /* value_validate */
      param_object_values_cmp,	 /* values_cmp */
    };
    type = g_param_type_register_static ("GParamObject", &pspec_info);
    g_assert (type == G_TYPE_PARAM_OBJECT);
  }
  
  g_value_register_exchange_func (G_TYPE_CHAR,    G_TYPE_UCHAR,   value_exch_memcpy);
  g_value_register_exchange_func (G_TYPE_CHAR,    G_TYPE_BOOLEAN, value_exch_memcpy);
  g_value_register_exchange_func (G_TYPE_CHAR,    G_TYPE_INT,     value_exch_memcpy);
  g_value_register_exchange_func (G_TYPE_CHAR,    G_TYPE_UINT,    value_exch_memcpy);
  g_value_register_exchange_func (G_TYPE_CHAR,    G_TYPE_ENUM,    value_exch_memcpy);
  g_value_register_exchange_func (G_TYPE_CHAR,    G_TYPE_FLAGS,   value_exch_memcpy); 
  g_value_register_exchange_func (G_TYPE_UCHAR,   G_TYPE_BOOLEAN, value_exch_memcpy);
  g_value_register_exchange_func (G_TYPE_UCHAR,   G_TYPE_INT,     value_exch_memcpy);
  g_value_register_exchange_func (G_TYPE_UCHAR,   G_TYPE_UINT,    value_exch_memcpy);
  g_value_register_exchange_func (G_TYPE_UCHAR,   G_TYPE_ENUM,    value_exch_memcpy);
  g_value_register_exchange_func (G_TYPE_UCHAR,   G_TYPE_FLAGS,   value_exch_memcpy); 
  g_value_register_exchange_func (G_TYPE_BOOLEAN, G_TYPE_INT,     value_exch_memcpy);
  g_value_register_exchange_func (G_TYPE_BOOLEAN, G_TYPE_UINT,    value_exch_memcpy);
  g_value_register_exchange_func (G_TYPE_BOOLEAN, G_TYPE_ENUM,    value_exch_memcpy);
  g_value_register_exchange_func (G_TYPE_BOOLEAN, G_TYPE_FLAGS,   value_exch_memcpy); 
  g_value_register_exchange_func (G_TYPE_INT,     G_TYPE_UINT,    value_exch_memcpy); 
  g_value_register_exchange_func (G_TYPE_INT,     G_TYPE_ENUM,    value_exch_memcpy); 
  g_value_register_exchange_func (G_TYPE_INT,     G_TYPE_FLAGS,   value_exch_memcpy); 
  g_value_register_exchange_func (G_TYPE_UINT,    G_TYPE_ENUM,    value_exch_memcpy); 
  g_value_register_exchange_func (G_TYPE_UINT,    G_TYPE_FLAGS,   value_exch_memcpy); 
  g_value_register_exchange_func (G_TYPE_LONG,    G_TYPE_CHAR,    value_exch_long_int); 
  g_value_register_exchange_func (G_TYPE_LONG,    G_TYPE_UCHAR,   value_exch_long_uint); 
  g_value_register_exchange_func (G_TYPE_LONG,    G_TYPE_BOOLEAN, value_exch_long_int); 
  g_value_register_exchange_func (G_TYPE_LONG,    G_TYPE_INT,     value_exch_long_int); 
  g_value_register_exchange_func (G_TYPE_LONG,    G_TYPE_UINT,    value_exch_long_uint); 
  g_value_register_exchange_func (G_TYPE_LONG,    G_TYPE_ULONG,   value_exch_memcpy); 
  g_value_register_exchange_func (G_TYPE_LONG,    G_TYPE_ENUM,    value_exch_long_int); 
  g_value_register_exchange_func (G_TYPE_LONG,    G_TYPE_FLAGS,   value_exch_long_uint); 
  g_value_register_exchange_func (G_TYPE_ULONG,   G_TYPE_CHAR,    value_exch_ulong_int); 
  g_value_register_exchange_func (G_TYPE_ULONG,   G_TYPE_UCHAR,   value_exch_ulong_uint); 
  g_value_register_exchange_func (G_TYPE_ULONG,   G_TYPE_BOOLEAN, value_exch_ulong_int); 
  g_value_register_exchange_func (G_TYPE_ULONG,   G_TYPE_INT,     value_exch_ulong_int); 
  g_value_register_exchange_func (G_TYPE_ULONG,   G_TYPE_UINT,    value_exch_ulong_uint); 
  g_value_register_exchange_func (G_TYPE_ULONG,   G_TYPE_ENUM,    value_exch_ulong_int); 
  g_value_register_exchange_func (G_TYPE_ULONG,   G_TYPE_FLAGS,   value_exch_ulong_uint); 
  g_value_register_exchange_func (G_TYPE_ENUM,    G_TYPE_FLAGS,   value_exch_memcpy); 
  g_value_register_exchange_func (G_TYPE_FLOAT,   G_TYPE_CHAR,    value_exch_float_int); 
  g_value_register_exchange_func (G_TYPE_FLOAT,   G_TYPE_UCHAR,   value_exch_float_uint); 
  g_value_register_exchange_func (G_TYPE_FLOAT,   G_TYPE_BOOLEAN, value_exch_float_int); 
  g_value_register_exchange_func (G_TYPE_FLOAT,   G_TYPE_INT,     value_exch_float_int); 
  g_value_register_exchange_func (G_TYPE_FLOAT,   G_TYPE_UINT,    value_exch_float_uint); 
  g_value_register_exchange_func (G_TYPE_FLOAT,   G_TYPE_LONG,    value_exch_float_long); 
  g_value_register_exchange_func (G_TYPE_FLOAT,   G_TYPE_ULONG,   value_exch_float_ulong);
  g_value_register_exchange_func (G_TYPE_FLOAT,   G_TYPE_ENUM,    value_exch_float_int); 
  g_value_register_exchange_func (G_TYPE_FLOAT,   G_TYPE_FLAGS,   value_exch_float_uint); 
  g_value_register_exchange_func (G_TYPE_DOUBLE,  G_TYPE_CHAR,    value_exch_double_int); 
  g_value_register_exchange_func (G_TYPE_DOUBLE,  G_TYPE_UCHAR,   value_exch_double_uint); 
  g_value_register_exchange_func (G_TYPE_DOUBLE,  G_TYPE_BOOLEAN, value_exch_double_int); 
  g_value_register_exchange_func (G_TYPE_DOUBLE,  G_TYPE_INT,     value_exch_double_int); 
  g_value_register_exchange_func (G_TYPE_DOUBLE,  G_TYPE_UINT,    value_exch_double_uint); 
  g_value_register_exchange_func (G_TYPE_DOUBLE,  G_TYPE_LONG,    value_exch_double_long);
  g_value_register_exchange_func (G_TYPE_DOUBLE,  G_TYPE_ULONG,   value_exch_double_ulong);
  g_value_register_exchange_func (G_TYPE_DOUBLE,  G_TYPE_ENUM,    value_exch_double_int); 
  g_value_register_exchange_func (G_TYPE_DOUBLE,  G_TYPE_FLAGS,   value_exch_double_uint);
  g_value_register_exchange_func (G_TYPE_DOUBLE,  G_TYPE_FLOAT,   value_exch_double_float); 
}


/* --- GParamSpec initialization --- */
GParamSpec*
g_param_spec_char (const gchar *name,
		   const gchar *nick,
		   const gchar *blurb,
		   gint8	minimum,
		   gint8	maximum,
		   gint8	default_value,
		   GParamFlags	flags)
{
  GParamSpecChar *cspec = g_param_spec_internal (G_TYPE_PARAM_CHAR,
						 name,
						 nick,
						 blurb,
						 flags);
  
  cspec->minimum = minimum;
  cspec->maximum = maximum;
  cspec->default_value = default_value;
  
  return G_PARAM_SPEC (cspec);
}

GParamSpec*
g_param_spec_uchar (const gchar *name,
		    const gchar *nick,
		    const gchar *blurb,
		    guint8	 minimum,
		    guint8	 maximum,
		    guint8	 default_value,
		    GParamFlags	 flags)
{
  GParamSpecUChar *uspec = g_param_spec_internal (G_TYPE_PARAM_UCHAR,
						  name,
						  nick,
						  blurb,
						  flags);
  
  uspec->minimum = minimum;
  uspec->maximum = maximum;
  uspec->default_value = default_value;
  
  return G_PARAM_SPEC (uspec);
}

GParamSpec*
g_param_spec_boolean (const gchar *name,
		      const gchar *nick,
		      const gchar *blurb,
		      gboolean	   default_value,
		      GParamFlags  flags)
{
  GParamSpecBoolean *bspec = g_param_spec_internal (G_TYPE_PARAM_BOOLEAN,
						    name,
						    nick,
						    blurb,
						    flags);
  
  bspec->default_value = default_value;
  
  return G_PARAM_SPEC (bspec);
}

GParamSpec*
g_param_spec_int (const gchar *name,
		  const gchar *nick,
		  const gchar *blurb,
		  gint	       minimum,
		  gint	       maximum,
		  gint	       default_value,
		  GParamFlags  flags)
{
  GParamSpecInt *ispec = g_param_spec_internal (G_TYPE_PARAM_INT,
						name,
						nick,
						blurb,
						flags);
  
  ispec->minimum = minimum;
  ispec->maximum = maximum;
  ispec->default_value = default_value;
  
  return G_PARAM_SPEC (ispec);
}

GParamSpec*
g_param_spec_uint (const gchar *name,
		   const gchar *nick,
		   const gchar *blurb,
		   guint	minimum,
		   guint	maximum,
		   guint	default_value,
		   GParamFlags	flags)
{
  GParamSpecUInt *uspec = g_param_spec_internal (G_TYPE_PARAM_UINT,
						 name,
						 nick,
						 blurb,
						 flags);
  
  uspec->minimum = minimum;
  uspec->maximum = maximum;
  uspec->default_value = default_value;
  
  return G_PARAM_SPEC (uspec);
}

GParamSpec*
g_param_spec_long (const gchar *name,
		   const gchar *nick,
		   const gchar *blurb,
		   glong	minimum,
		   glong	maximum,
		   glong	default_value,
		   GParamFlags	flags)
{
  GParamSpecLong *lspec = g_param_spec_internal (G_TYPE_PARAM_LONG,
						 name,
						 nick,
						 blurb,
						 flags);
  
  lspec->minimum = minimum;
  lspec->maximum = maximum;
  lspec->default_value = default_value;
  
  return G_PARAM_SPEC (lspec);
}

GParamSpec*
g_param_spec_ulong (const gchar *name,
		    const gchar *nick,
		    const gchar *blurb,
		    gulong	 minimum,
		    gulong	 maximum,
		    gulong	 default_value,
		    GParamFlags	 flags)
{
  GParamSpecULong *uspec = g_param_spec_internal (G_TYPE_PARAM_ULONG,
						  name,
						  nick,
						  blurb,
						  flags);
  
  uspec->minimum = minimum;
  uspec->maximum = maximum;
  uspec->default_value = default_value;
  
  return G_PARAM_SPEC (uspec);
}

GParamSpec*
g_param_spec_enum (const gchar *name,
		   const gchar *nick,
		   const gchar *blurb,
		   GType	enum_type,
		   gint		default_value,
		   GParamFlags	flags)
{
  GParamSpecEnum *espec;
  
  g_return_val_if_fail (G_TYPE_IS_ENUM (enum_type), NULL);
  
  espec = g_param_spec_internal (G_TYPE_PARAM_ENUM,
				 name,
				 nick,
				 blurb,
				 flags);
  
  espec->enum_class = g_type_class_ref (enum_type);
  espec->default_value = default_value;
  G_PARAM_SPEC (espec)->value_type = enum_type;
  
  return G_PARAM_SPEC (espec);
}

GParamSpec*
g_param_spec_flags (const gchar *name,
		    const gchar *nick,
		    const gchar *blurb,
		    GType	 flags_type,
		    guint	 default_value,
		    GParamFlags	 flags)
{
  GParamSpecFlags *fspec;
  
  g_return_val_if_fail (G_TYPE_IS_FLAGS (flags_type), NULL);
  
  fspec = g_param_spec_internal (G_TYPE_PARAM_FLAGS,
				 name,
				 nick,
				 blurb,
				 flags);
  
  fspec->flags_class = g_type_class_ref (flags_type);
  fspec->default_value = default_value;
  G_PARAM_SPEC (fspec)->value_type = flags_type;
  
  return G_PARAM_SPEC (fspec);
}

GParamSpec*
g_param_spec_float (const gchar *name,
		    const gchar *nick,
		    const gchar *blurb,
		    gfloat	 minimum,
		    gfloat	 maximum,
		    gfloat	 default_value,
		    GParamFlags	 flags)
{
  GParamSpecFloat *fspec = g_param_spec_internal (G_TYPE_PARAM_FLOAT,
						  name,
						  nick,
						  blurb,
						  flags);
  
  fspec->minimum = minimum;
  fspec->maximum = maximum;
  fspec->default_value = default_value;
  
  return G_PARAM_SPEC (fspec);
}

GParamSpec*
g_param_spec_double (const gchar *name,
		     const gchar *nick,
		     const gchar *blurb,
		     gdouble	  minimum,
		     gdouble	  maximum,
		     gdouble	  default_value,
		     GParamFlags  flags)
{
  GParamSpecDouble *dspec = g_param_spec_internal (G_TYPE_PARAM_DOUBLE,
						   name,
						   nick,
						   blurb,
						   flags);
  
  dspec->minimum = minimum;
  dspec->maximum = maximum;
  dspec->default_value = default_value;
  
  return G_PARAM_SPEC (dspec);
}

GParamSpec*
g_param_spec_string (const gchar *name,
		     const gchar *nick,
		     const gchar *blurb,
		     const gchar *default_value,
		     GParamFlags  flags)
{
  GParamSpecString *sspec = g_param_spec_internal (G_TYPE_PARAM_STRING,
						   name,
						   nick,
						   blurb,
						   flags);
  g_free (sspec->default_value);
  sspec->default_value = g_strdup (default_value);
  
  return G_PARAM_SPEC (sspec);
}

GParamSpec*
g_param_spec_string_c (const gchar *name,
		       const gchar *nick,
		       const gchar *blurb,
		       const gchar *default_value,
		       GParamFlags  flags)
{
  GParamSpecString *sspec = g_param_spec_internal (G_TYPE_PARAM_STRING,
						   name,
						   nick,
						   blurb,
						   flags);
  g_free (sspec->default_value);
  sspec->default_value = g_strdup (default_value);
  g_free (sspec->cset_first);
  sspec->cset_first = g_strdup (G_CSET_a_2_z "_" G_CSET_A_2_Z);
  g_free (sspec->cset_nth);
  sspec->cset_nth = g_strdup (G_CSET_a_2_z
			      "_0123456789"
			      /* G_CSET_LATINS G_CSET_LATINC */
			      G_CSET_A_2_Z);
  
  return G_PARAM_SPEC (sspec);
}

GParamSpec*
g_param_spec_param (const gchar *name,
		    const gchar *nick,
		    const gchar *blurb,
		    GType	 param_type,
		    GParamFlags  flags)
{
  GParamSpecParam *pspec;
  
  g_return_val_if_fail (G_TYPE_IS_PARAM (param_type), NULL);
  
  pspec = g_param_spec_internal (G_TYPE_PARAM_PARAM,
				 name,
				 nick,
				 blurb,
				 flags);
  G_PARAM_SPEC (pspec)->value_type = param_type;
  
  return G_PARAM_SPEC (pspec);
}

GParamSpec*
g_param_spec_pointer (const gchar *name,
		      const gchar *nick,
		      const gchar *blurb,
		      GParamFlags  flags)
{
  GParamSpecPointer *pspec;
  
  pspec = g_param_spec_internal (G_TYPE_PARAM_POINTER,
				 name,
				 nick,
				 blurb,
				 flags);
  return G_PARAM_SPEC (pspec);
}

GParamSpec*
g_param_spec_ccallback (const gchar *name,
			const gchar *nick,
			const gchar *blurb,
			GParamFlags  flags)
{
  GParamSpecCCallback *cspec;
  
  cspec = g_param_spec_internal (G_TYPE_PARAM_CCALLBACK,
				 name,
				 nick,
				 blurb,
				 flags);
  return G_PARAM_SPEC (cspec);
}

GParamSpec*
g_param_spec_boxed (const gchar *name,
		    const gchar *nick,
		    const gchar *blurb,
		    GType	 boxed_type,
		    GParamFlags  flags)
{
  GParamSpecBoxed *bspec;
  
  g_return_val_if_fail (G_TYPE_IS_BOXED (boxed_type), NULL);
  g_return_val_if_fail (G_TYPE_IS_DERIVED (boxed_type), NULL);
  
  bspec = g_param_spec_internal (G_TYPE_PARAM_BOXED,
				 name,
				 nick,
				 blurb,
				 flags);
  G_PARAM_SPEC (bspec)->value_type = boxed_type;
  
  return G_PARAM_SPEC (bspec);
}

GParamSpec*
g_param_spec_object (const gchar *name,
		     const gchar *nick,
		     const gchar *blurb,
		     GType	  object_type,
		     GParamFlags  flags)
{
  GParamSpecObject *ospec;
  
  g_return_val_if_fail (G_TYPE_IS_OBJECT (object_type), NULL);
  
  ospec = g_param_spec_internal (G_TYPE_PARAM_OBJECT,
				 name,
				 nick,
				 blurb,
				 flags);
  G_PARAM_SPEC (ospec)->value_type = object_type;
  
  return G_PARAM_SPEC (ospec);
}
