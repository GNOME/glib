/*
 * Copyright © 2010 Codethink Limited
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the licence, or (at your option) any later version.
 *
 * See the included COPYING file for more information.
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#include <string.h>
#include <glib.h>

#define BASIC "bynqiuxthdsog?"
#define N_BASIC (G_N_ELEMENTS (BASIC) - 1)

#define INVALIDS "cefjklpwz&@^$"
#define N_INVALIDS (G_N_ELEMENTS (INVALIDS) - 1)

static gboolean
randomly (gdouble prob)
{
  return g_test_rand_double_range (0, 1) < prob;
}

/* corecursion */
static GVariantType *
append_tuple_type_string (GString *, GString *, gboolean, gint);

/* append a random GVariantType to a GString
 * append a description of the type to another GString
 * return what the type is
 */
static GVariantType *
append_type_string (GString  *string,
                    GString  *description,
                    gboolean  definite,
                    gint      depth)
{
  if (!depth-- || randomly (0.3))
    {
      gchar b = BASIC[g_test_rand_int_range (0, N_BASIC - definite)];
      g_string_append_c (string, b);
      g_string_append_c (description, b);

      switch (b)
        {
        case 'b':
          return g_variant_type_copy (G_VARIANT_TYPE_BOOLEAN);
        case 'y':
          return g_variant_type_copy (G_VARIANT_TYPE_BYTE);
        case 'n':
          return g_variant_type_copy (G_VARIANT_TYPE_INT16);
        case 'q':
          return g_variant_type_copy (G_VARIANT_TYPE_UINT16);
        case 'i':
          return g_variant_type_copy (G_VARIANT_TYPE_INT32);
        case 'u':
          return g_variant_type_copy (G_VARIANT_TYPE_UINT32);
        case 'x':
          return g_variant_type_copy (G_VARIANT_TYPE_INT64);
        case 't':
          return g_variant_type_copy (G_VARIANT_TYPE_UINT64);
        case 'h':
          return g_variant_type_copy (G_VARIANT_TYPE_HANDLE);
        case 'd':
          return g_variant_type_copy (G_VARIANT_TYPE_DOUBLE);
        case 's':
          return g_variant_type_copy (G_VARIANT_TYPE_STRING);
        case 'o':
          return g_variant_type_copy (G_VARIANT_TYPE_OBJECT_PATH);
        case 'g':
          return g_variant_type_copy (G_VARIANT_TYPE_SIGNATURE);
        case '?':
          return g_variant_type_copy (G_VARIANT_TYPE_BASIC);
        default:
          g_assert_not_reached ();
        }
    }
  else
    {
      GVariantType *result;

      switch (g_test_rand_int_range (0, definite ? 5 : 7))
        {
        case 0:
          {
            GVariantType *element;

            g_string_append_c (string, 'a');
            g_string_append (description, "a of ");
            element = append_type_string (string, description,
                                          definite, depth);
            result = g_variant_type_new_array (element);
            g_variant_type_free (element);
          }

          g_assert (g_variant_type_is_array (result));
          break;

        case 1:
          {
            GVariantType *element;

            g_string_append_c (string, 'm');
            g_string_append (description, "m of ");
            element = append_type_string (string, description,
                                          definite, depth);
            result = g_variant_type_new_maybe (element);
            g_variant_type_free (element);
          }

          g_assert (g_variant_type_is_maybe (result));
          break;

        case 2:
          result = append_tuple_type_string (string, description,
                                             definite, depth);

          g_assert (g_variant_type_is_tuple (result));
          break;

        case 3:
          {
            GVariantType *key, *value;

            g_string_append_c (string, '{');
            g_string_append (description, "e of [");
            key = append_type_string (string, description, definite, 0);
            g_string_append (description, ", ");
            value = append_type_string (string, description, definite, depth);
            g_string_append_c (description, ']');
            g_string_append_c (string, '}');
            result = g_variant_type_new_dict_entry (key, value);
            g_variant_type_free (key);
            g_variant_type_free (value);
          }

          g_assert (g_variant_type_is_dict_entry (result));
          break;

        case 4:
          g_string_append_c (string, 'v');
          g_string_append_c (description, 'V');
          result = g_variant_type_copy (G_VARIANT_TYPE_VARIANT);
          g_assert (g_variant_type_equal (result, G_VARIANT_TYPE_VARIANT));
          break;

        case 5:
          g_string_append_c (string, '*');
          g_string_append_c (description, 'S');
          result = g_variant_type_copy (G_VARIANT_TYPE_ANY);
          g_assert (g_variant_type_equal (result, G_VARIANT_TYPE_ANY));
          break;

        case 6:
          g_string_append_c (string, 'r');
          g_string_append_c (description, 'R');
          result = g_variant_type_copy (G_VARIANT_TYPE_TUPLE);
          g_assert (g_variant_type_is_tuple (result));
          break;

        default:
          g_assert_not_reached ();
        }

      return result;
    }
}

static GVariantType *
append_tuple_type_string (GString  *string,
                          GString  *description,
                          gboolean  definite,
                          gint      depth)
{
  GVariantType *result, *other_result;
  GVariantType **types;
  gint size;
  gint i;

  g_string_append_c (string, '(');
  g_string_append (description, "t of [");

  size = g_test_rand_int_range (0, 20);
  types = g_new (GVariantType *, size + 1);

  for (i = 0; i < size; i++)
    {
      types[i] = append_type_string (string, description, definite, depth);

      if (i < size - 1)
        g_string_append (description, ", ");
    }

  types[i] = NULL;

  g_string_append_c (description, ']');
  g_string_append_c (string, ')');

  result = g_variant_type_new_tuple ((gpointer) types, size);
  other_result = g_variant_type_new_tuple ((gpointer) types, -1);
  g_assert (g_variant_type_equal (result, other_result));
  g_variant_type_free (other_result);
  for (i = 0; i < size; i++)
    g_variant_type_free (types[i]);
  g_free (types);

  return result;
}

/* given a valid type string, make it invalid */
static gchar *
invalid_mutation (const gchar *type_string)
{
  gboolean have_parens, have_braces;

  /* it's valid, so '(' implies ')' and same for '{' and '}' */
  have_parens = strchr (type_string, '(') != NULL;
  have_braces = strchr (type_string, '{') != NULL;

  if (have_parens && have_braces && randomly (0.3))
    {
      /* swap a paren and a brace */
      gchar *pp, *bp;
      gint np, nb;
      gchar p, b;
      gchar *new;

      new = g_strdup (type_string);

      if (randomly (0.5))
        p = '(', b = '{';
      else
        p = ')', b = '}';

      np = nb = 0;
      pp = bp = new - 1;

      /* count number of parens/braces */
      while ((pp = strchr (pp + 1, p))) np++;
      while ((bp = strchr (bp + 1, b))) nb++;

      /* randomly pick one of each */
      np = g_test_rand_int_range (0, np) + 1;
      nb = g_test_rand_int_range (0, nb) + 1;

      /* find it */
      pp = bp = new - 1;
      while (np--) pp = strchr (pp + 1, p);
      while (nb--) bp = strchr (bp + 1, b);

      /* swap */
      g_assert (*bp == b && *pp == p);
      *bp = p;
      *pp = b;

      return new;
    }

  if ((have_parens || have_braces) && randomly (0.3))
    {
      /* drop a paren/brace */
      gchar *new;
      gchar *pp;
      gint np;
      gchar p;

      if (have_parens)
        if (randomly (0.5)) p = '('; else p = ')';
      else
        if (randomly (0.5)) p = '{'; else p = '}';

      new = g_strdup (type_string);

      np = 0;
      pp = new - 1;
      while ((pp = strchr (pp + 1, p))) np++;
      np = g_test_rand_int_range (0, np) + 1;
      pp = new - 1;
      while (np--) pp = strchr (pp + 1, p);
      g_assert (*pp == p);

      while (*pp)
        {
          *pp = *(pp + 1);
          pp++;
        }

      return new;
    }

  /* else, perform a random mutation at a random point */
  {
    gint length, n;
    gchar *new;
    gchar p;

    if (randomly (0.3))
      {
        /* insert a paren/brace */
        if (randomly (0.5))
          if (randomly (0.5)) p = '('; else p = ')';
        else
          if (randomly (0.5)) p = '{'; else p = '}';
      }
    else if (randomly (0.5))
      {
        /* insert junk */
        p = INVALIDS[g_test_rand_int_range (0, N_INVALIDS)];
      }
    else
      {
        /* truncate */
        p = '\0';
      }


    length = strlen (type_string);
    new = g_malloc (length + 2);
    n = g_test_rand_int_range (0, length);
    memcpy (new, type_string, n);
    new[n] = p;
    memcpy (new + n + 1, type_string + n, length - n);
    new[length + 1] = '\0';

    return new;
  }
}

/* describe a type using the same language as is generated
 * while generating the type with append_type_string
 */
static gchar *
describe_type (const GVariantType *type)
{
  gchar *result;

  if (g_variant_type_is_container (type))
    {
      g_assert (!g_variant_type_is_basic (type));

      if (g_variant_type_is_array (type))
        {
          gchar *subtype = describe_type (g_variant_type_element (type));
          result = g_strdup_printf ("a of %s", subtype);
          g_free (subtype);
        }
      else if (g_variant_type_is_maybe (type))
        {
          gchar *subtype = describe_type (g_variant_type_element (type));
          result = g_strdup_printf ("m of %s", subtype);
          g_free (subtype);
        }
      else if (g_variant_type_is_tuple (type))
        {
          if (!g_variant_type_equal (type, G_VARIANT_TYPE_TUPLE))
            {
              const GVariantType *sub;
              GString *string;
              gint length;
              gint i;

              string = g_string_new ("t of [");

              length = g_variant_type_n_items (type);
              sub = g_variant_type_first (type);
              for (i = 0; i < length; i++)
                {
                  gchar *subtype = describe_type (sub);
                  g_string_append (string, subtype);
                  g_free (subtype);

                  if ((sub = g_variant_type_next (sub)))
                    g_string_append (string, ", ");
                }
              g_assert (sub == NULL);
              g_string_append_c (string, ']');

              result = g_string_free (string, FALSE);
            }
          else
            result = g_strdup ("R");
        }
      else if (g_variant_type_is_dict_entry (type))
        {
          gchar *key, *value, *key2, *value2;

          key = describe_type (g_variant_type_key (type));
          value = describe_type (g_variant_type_value (type));
          key2 = describe_type (g_variant_type_first (type));
          value2 = describe_type (
            g_variant_type_next (g_variant_type_first (type)));
          g_assert (g_variant_type_next (g_variant_type_next (
            g_variant_type_first (type))) == NULL);
          g_assert_cmpstr (key, ==, key2);
          g_assert_cmpstr (value, ==, value2);
          result = g_strjoin ("", "e of [", key, ", ", value, "]", NULL);
          g_free (key2);
          g_free (value2);
          g_free (key);
          g_free (value);
        }
      else if (g_variant_type_equal (type, G_VARIANT_TYPE_VARIANT))
        {
          result = g_strdup ("V");
        }
      else
        g_assert_not_reached ();
    }
  else
    {
      if (g_variant_type_is_definite (type))
        {
          g_assert (g_variant_type_is_basic (type));

          if (g_variant_type_equal (type, G_VARIANT_TYPE_BOOLEAN))
            result = g_strdup ("b");
          else if (g_variant_type_equal (type, G_VARIANT_TYPE_BYTE))
            result = g_strdup ("y");
          else if (g_variant_type_equal (type, G_VARIANT_TYPE_INT16))
            result = g_strdup ("n");
          else if (g_variant_type_equal (type, G_VARIANT_TYPE_UINT16))
            result = g_strdup ("q");
          else if (g_variant_type_equal (type, G_VARIANT_TYPE_INT32))
            result = g_strdup ("i");
          else if (g_variant_type_equal (type, G_VARIANT_TYPE_UINT32))
            result = g_strdup ("u");
          else if (g_variant_type_equal (type, G_VARIANT_TYPE_INT64))
            result = g_strdup ("x");
          else if (g_variant_type_equal (type, G_VARIANT_TYPE_UINT64))
            result = g_strdup ("t");
          else if (g_variant_type_equal (type, G_VARIANT_TYPE_HANDLE))
            result = g_strdup ("h");
          else if (g_variant_type_equal (type, G_VARIANT_TYPE_DOUBLE))
            result = g_strdup ("d");
          else if (g_variant_type_equal (type, G_VARIANT_TYPE_STRING))
            result = g_strdup ("s");
          else if (g_variant_type_equal (type, G_VARIANT_TYPE_OBJECT_PATH))
            result = g_strdup ("o");
          else if (g_variant_type_equal (type, G_VARIANT_TYPE_SIGNATURE))
            result = g_strdup ("g");
          else
            g_assert_not_reached ();
        }
      else
        {
          if (g_variant_type_equal (type, G_VARIANT_TYPE_ANY))
            {
              result = g_strdup ("S");
            }
          else if (g_variant_type_equal (type, G_VARIANT_TYPE_BASIC))
            {
              result = g_strdup ("?");
            }
          else
            g_assert_not_reached ();
        }
    }

  return result;
}

/* given a type string, replace one of the indefinite type characters in
 * it with a matching type (possibly the same type).
 */
static gchar *
generate_subtype (const gchar *type_string)
{
  GVariantType *replacement;
  GString *result, *junk;
  gint length, n = 0, l;

  result = g_string_new (NULL);
  junk = g_string_new (NULL);

  /* count the number of indefinite type characters */
  for (length = 0; type_string[length]; length++)
    n += type_string[length] == 'r' ||
         type_string[length] == '?' ||
         type_string[length] == '*';
  /* length now is strlen (type_string) */

  /* pick one at random to replace */
  n = g_test_rand_int_range (0, n) + 1;

  /* find it */
  l = -1;
  while (n--) l += strcspn (type_string + l + 1, "r?*") + 1;
  g_assert (type_string[l] == 'r' ||
            type_string[l] == '?' ||
            type_string[l] == '*');

  /* store up to that point in a GString */
  g_string_append_len (result, type_string, l);

  /* then store the replacement in the GString */
  if (type_string[l] == 'r')
    replacement = append_tuple_type_string (result, junk, FALSE, 3);

  else if (type_string[l] == '?')
    replacement = append_type_string (result, junk, FALSE, 0);

  else if (type_string[l] == '*')
    replacement = append_type_string (result, junk, FALSE, 3);

  else
    g_assert_not_reached ();

  /* ensure the replacement has the proper type */
  g_assert (g_variant_type_is_subtype_of (replacement,
                                          (gpointer) &type_string[l]));

  /* store the rest from the original type string */
  g_string_append (result, type_string + l + 1);

  g_variant_type_free (replacement);
  g_string_free (junk, TRUE);

  return g_string_free (result, FALSE);
}

struct typestack
{
  const GVariantType *type;
  struct typestack *parent;
};

/* given an indefinite type string, replace one of the indefinite
 * characters in it with a matching type and ensure that the result is a
 * subtype of the original.  repeat.
 */
static void
subtype_check (const gchar      *type_string,
               struct typestack *parent_ts)
{
  struct typestack ts, *node;
  gchar *subtype;
  gint depth = 0;

  subtype = generate_subtype (type_string);

  ts.type = G_VARIANT_TYPE (subtype);
  ts.parent = parent_ts;

  for (node = &ts; node; node = node->parent)
    {
      /* this type should be a subtype of each parent type */
      g_assert (g_variant_type_is_subtype_of (ts.type, node->type));

      /* it should only be a supertype when it is exactly equal */
      g_assert (g_variant_type_is_subtype_of (node->type, ts.type) ==
                g_variant_type_equal (ts.type, node->type));

      depth++;
    }

  if (!g_variant_type_is_definite (ts.type) && depth < 5)
    {
      /* the type is still indefinite and we haven't repeated too many
       * times.  go once more.
       */

      subtype_check (subtype, &ts);
    }

  g_free (subtype);
}

static void
test_gvarianttype (void)
{
  gint i;

  for (i = 0; i < 2000; i++)
    {
      GString *type_string, *description;
      GVariantType *type, *other_type;
      const GVariantType *ctype;
      gchar *invalid;
      gchar *desc;

      type_string = g_string_new (NULL);
      description = g_string_new (NULL);

      /* generate a random type, its type string and a description
       *
       * exercises type constructor functions and g_variant_type_copy()
       */
      type = append_type_string (type_string, description, FALSE, 6);

      /* convert the type string to a type and ensure that it is equal
       * to the one produced with the type constructor routines
       */
      ctype = G_VARIANT_TYPE (type_string->str);
      g_assert (g_variant_type_equal (ctype, type));
      g_assert (g_variant_type_is_subtype_of (ctype, type));
      g_assert (g_variant_type_is_subtype_of (type, ctype));

      /* check if the type is indefinite */
      if (!g_variant_type_is_definite (type))
        {
          struct typestack ts = { type, NULL };

          /* if it is indefinite, then replace one of the indefinite
           * characters with a matching type and ensure that the result
           * is a subtype of the original type.  repeat.
           */
          subtype_check (type_string->str, &ts);
        }
      else
        /* ensure that no indefinite characters appear */
        g_assert (strcspn (type_string->str, "r?*") == type_string->len);


      /* describe the type.
       *
       * exercises the type iterator interface
       */
      desc = describe_type (type);

      /* make sure the description matches */
      g_assert_cmpstr (desc, ==, description->str);
      g_free (desc);

      /* make an invalid mutation to the type and make sure the type
       * validation routines catch it */
      invalid = invalid_mutation (type_string->str);
      g_assert (g_variant_type_string_is_valid (type_string->str));
      g_assert (!g_variant_type_string_is_valid (invalid));
      g_free (invalid);

      /* concatenate another type to the type string and ensure that
       * the result is recognised as being invalid
       */
      other_type = append_type_string (type_string, description, FALSE, 2);

      g_string_free (description, TRUE);
      g_string_free (type_string, TRUE);
      g_variant_type_free (other_type);
      g_variant_type_free (type);
    }
}

#undef  G_GNUC_INTERNAL
#define G_GNUC_INTERNAL static

#define DISABLE_VISIBILITY
#define GLIB_COMPILATION
#include <glib/gvarianttypeinfo.c>

#define ALIGNED(x, y)   (((x + (y - 1)) / y) * y)

/* do our own calculation of the fixed_size and alignment of a type
 * using a simple algorithm to make sure the "fancy" one in the
 * implementation is correct.
 */
static void
calculate_type_info (const GVariantType *type,
                     gsize              *fixed_size,
                     guint              *alignment)
{
  if (g_variant_type_is_array (type) ||
      g_variant_type_is_maybe (type))
    {
      calculate_type_info (g_variant_type_element (type), NULL, alignment);

      if (fixed_size)
        *fixed_size = 0;
    }
  else if (g_variant_type_is_tuple (type) ||
           g_variant_type_is_dict_entry (type))
    {
      if (g_variant_type_n_items (type))
        {
          const GVariantType *sub;
          gboolean variable;
          gsize size;
          guint al;

          variable = FALSE;
          size = 0;
          al = 0;

          sub = g_variant_type_first (type);
          do
            {
              gsize this_fs;
              guint this_al;

              calculate_type_info (sub, &this_fs, &this_al);

              al = MAX (al, this_al);

              if (!this_fs)
                {
                  variable = TRUE;
                  size = 0;
                }

              if (!variable)
                {
                  size = ALIGNED (size, this_al);
                  size += this_fs;
                }
            }
          while ((sub = g_variant_type_next (sub)));

          size = ALIGNED (size, al);

          if (alignment)
            *alignment = al;

          if (fixed_size)
            *fixed_size = size;
        }
      else
        {
          if (fixed_size)
            *fixed_size = 1;

          if (alignment)
            *alignment = 1;
        }
    }
  else
    {
      gint fs, al;

      if (g_variant_type_equal (type, G_VARIANT_TYPE_BOOLEAN) ||
          g_variant_type_equal (type, G_VARIANT_TYPE_BYTE))
        {
          al = fs = 1;
        }

      else if (g_variant_type_equal (type, G_VARIANT_TYPE_INT16) ||
               g_variant_type_equal (type, G_VARIANT_TYPE_UINT16))
        {
          al = fs = 2;
        }

      else if (g_variant_type_equal (type, G_VARIANT_TYPE_INT32) ||
               g_variant_type_equal (type, G_VARIANT_TYPE_UINT32) ||
               g_variant_type_equal (type, G_VARIANT_TYPE_HANDLE))
        {
          al = fs = 4;
        }

      else if (g_variant_type_equal (type, G_VARIANT_TYPE_INT64) ||
               g_variant_type_equal (type, G_VARIANT_TYPE_UINT64) ||
               g_variant_type_equal (type, G_VARIANT_TYPE_DOUBLE))
        {
          al = fs = 8;
        }
      else if (g_variant_type_equal (type, G_VARIANT_TYPE_STRING) ||
               g_variant_type_equal (type, G_VARIANT_TYPE_OBJECT_PATH) ||
               g_variant_type_equal (type, G_VARIANT_TYPE_SIGNATURE))
        {
          al = 1;
          fs = 0;
        }
      else if (g_variant_type_equal (type, G_VARIANT_TYPE_VARIANT))
        {
          al = 8;
          fs = 0;
        }
      else
        g_assert_not_reached ();

      if (fixed_size)
        *fixed_size = fs;

      if (alignment)
        *alignment = al;
    }
}

/* same as the describe_type() function above, but iterates over
 * typeinfo instead of types.
 */
static gchar *
describe_info (GVariantTypeInfo *info)
{
  gchar *result;

  switch (g_variant_type_info_get_type_char (info))
    {
    case G_VARIANT_TYPE_INFO_CHAR_MAYBE:
      {
        gchar *element;

        element = describe_info (g_variant_type_info_element (info));
        result = g_strdup_printf ("m of %s", element);
        g_free (element);
      }
      break;

    case G_VARIANT_TYPE_INFO_CHAR_ARRAY:
      {
        gchar *element;

        element = describe_info (g_variant_type_info_element (info));
        result = g_strdup_printf ("a of %s", element);
        g_free (element);
      }
      break;

    case G_VARIANT_TYPE_INFO_CHAR_TUPLE:
      {
        const gchar *sep = "";
        GString *string;
        gint length;
        gint i;

        string = g_string_new ("t of [");
        length = g_variant_type_info_n_members (info);

        for (i = 0; i < length; i++)
          {
            const GVariantMemberInfo *minfo;
            gchar *subtype;

            g_string_append (string, sep);
            sep = ", ";

            minfo = g_variant_type_info_member_info (info, i);
            subtype = describe_info (minfo->type_info);
            g_string_append (string, subtype);
            g_free (subtype);
          }

        g_string_append_c (string, ']');

        result = g_string_free (string, FALSE);
      }
      break;

    case G_VARIANT_TYPE_INFO_CHAR_DICT_ENTRY:
      {
        const GVariantMemberInfo *keyinfo, *valueinfo;
        gchar *key, *value;

        g_assert_cmpint (g_variant_type_info_n_members (info), ==, 2);
        keyinfo = g_variant_type_info_member_info (info, 0);
        valueinfo = g_variant_type_info_member_info (info, 1);
        key = describe_info (keyinfo->type_info);
        value = describe_info (valueinfo->type_info);
        result = g_strjoin ("", "e of [", key, ", ", value, "]", NULL);
        g_free (key);
        g_free (value);
      }
      break;

    case G_VARIANT_TYPE_INFO_CHAR_VARIANT:
      result = g_strdup ("V");
      break;

    default:
      result = g_strdup (g_variant_type_info_get_type_string (info));
      g_assert_cmpint (strlen (result), ==, 1);
      break;
    }

  return result;
}

/* check that the O(1) method of calculating offsets meshes with the
 * results of simple iteration.
 */
static void
check_offsets (GVariantTypeInfo   *info,
               const GVariantType *type)
{
  gint flavour;
  gint length;

  length = g_variant_type_info_n_members (info);
  g_assert_cmpint (length, ==, g_variant_type_n_items (type));

  /* the 'flavour' is the low order bits of the ending point of
   * variable-size items in the tuple.  this lets us test that the type
   * info is correct for various starting alignments.
   */
  for (flavour = 0; flavour < 8; flavour++)
    {
      const GVariantType *subtype;
      gsize last_offset_index;
      gsize last_offset;
      gsize position;
      gint i;

      subtype = g_variant_type_first (type);
      last_offset_index = -1;
      last_offset = 0;
      position = 0;

      /* go through the tuple, keeping track of our position */
      for (i = 0; i < length; i++)
        {
          gsize fixed_size;
          guint alignment;

          calculate_type_info (subtype, &fixed_size, &alignment);

          position = ALIGNED (position, alignment);

          /* compare our current aligned position (ie: the start of this
           * item) to the start offset that would be calculated if we
           * used the type info
           */
          {
            const GVariantMemberInfo *member;
            gsize start;

            member = g_variant_type_info_member_info (info, i);
            g_assert_cmpint (member->i, ==, last_offset_index);

            /* do the calculation using the typeinfo */
            start = last_offset;
            start += member->a;
            start &= member->b;
            start |= member->c;

            /* did we reach the same spot? */
            g_assert_cmpint (start, ==, position);
          }

          if (fixed_size)
            {
              /* fixed size.  add that size. */
              position += fixed_size;
            }
          else
            {
              /* variable size.  do the flavouring. */
              while ((position & 0x7) != flavour)
                position++;

              /* and store the offset, just like it would be in the
               * serialised data.
               */
              last_offset = position;
              last_offset_index++;
            }

          /* next type */
          subtype = g_variant_type_next (subtype);
        }

      /* make sure we used up exactly all the types */
      g_assert (subtype == NULL);
    }
}

static void
test_gvarianttypeinfo (void)
{
  gint i;

  for (i = 0; i < 2000; i++)
    {
      GString *type_string, *description;
      gsize fixed_size1, fixed_size2;
      guint alignment1, alignment2;
      GVariantTypeInfo *info;
      GVariantType *type;
      gchar *desc;

      type_string = g_string_new (NULL);
      description = g_string_new (NULL);

      /* random type */
      type = append_type_string (type_string, description, TRUE, 6);

      /* create a typeinfo for it */
      info = g_variant_type_info_get (type);

      /* make sure the typeinfo has the right type string */
      g_assert_cmpstr (g_variant_type_info_get_type_string (info), ==,
                       type_string->str);

      /* calculate the alignment and fixed size, compare to the
       * typeinfo's calculations
       */
      calculate_type_info (type, &fixed_size1, &alignment1);
      g_variant_type_info_query (info, &alignment2, &fixed_size2);
      g_assert_cmpint (fixed_size1, ==, fixed_size2);
      g_assert_cmpint (alignment1, ==, alignment2 + 1);

      /* test the iteration functions over typeinfo structures by
       * "describing" the typeinfo and verifying equality.
       */
      desc = describe_info (info);
      g_assert_cmpstr (desc, ==, description->str);

      /* do extra checks for containers */
      if (g_variant_type_is_array (type) ||
          g_variant_type_is_maybe (type))
        {
          const GVariantType *element;
          gsize efs1, efs2;
          guint ea1, ea2;

          element = g_variant_type_element (type);
          calculate_type_info (element, &efs1, &ea1);
          g_variant_type_info_query_element (info, &ea2, &efs2);
          g_assert_cmpint (efs1, ==, efs2);
          g_assert_cmpint (ea1, ==, ea2 + 1);

          g_assert_cmpint (ea1, ==, alignment1);
          g_assert_cmpint (0, ==, fixed_size1);
        }
      else if (g_variant_type_is_tuple (type) ||
               g_variant_type_is_dict_entry (type))
        {
          /* make sure the "magic constants" are working */
          check_offsets (info, type);
        }

      g_string_free (type_string, TRUE);
      g_string_free (description, TRUE);
      g_variant_type_info_unref (info);
      g_variant_type_free (type);
      g_free (desc);
    }

  assert_no_type_infos ();
}

#include <glib/gvariant-serialiser.c>

#define MAX_FIXED_MULTIPLIER    256
#define MAX_INSTANCE_SIZE       1024
#define MAX_ARRAY_CHILDREN      128
#define MAX_TUPLE_CHILDREN      128

/* this function generates a random type such that all characteristics
 * that are "interesting" to the serialiser are tested.
 *
 * this basically means:
 *   - test different alignments
 *   - test variable sized items and fixed sized items
 *   - test different fixed sizes
 */
static gchar *
random_type_string (void)
{
  const guchar base_types[] = "ynix";
  guchar base_type;

  base_type = base_types[g_test_rand_int_range (0, 4)];

  if (g_test_rand_bit ())
    /* construct a fixed-sized type */
    {
      char type_string[MAX_FIXED_MULTIPLIER];
      guint multiplier;
      guint i = 0;

      multiplier = g_test_rand_int_range (1, sizeof type_string - 1);

      type_string[i++] = '(';
      while (multiplier--)
        type_string[i++] = base_type;
      type_string[i++] = ')';

      return g_strndup (type_string, i);
    }
  else
    /* construct a variable-sized type */
    {
      char type_string[2] = { 'a', base_type };

      return g_strndup (type_string, 2);
    }
}

typedef struct
{
  GVariantTypeInfo *type_info;
  guint alignment;
  gsize size;
  gboolean is_fixed_sized;

  guint32 seed;

#define INSTANCE_MAGIC    1287582829
  guint magic;
} RandomInstance;

static RandomInstance *
random_instance (GVariantTypeInfo *type_info)
{
  RandomInstance *instance;

  instance = g_slice_new (RandomInstance);

  if (type_info == NULL)
    {
      gchar *str = random_type_string ();
      instance->type_info = g_variant_type_info_get (G_VARIANT_TYPE (str));
      g_free (str);
    }
  else
    instance->type_info = g_variant_type_info_ref (type_info);

  instance->seed = g_test_rand_int ();

  g_variant_type_info_query (instance->type_info,
                             &instance->alignment,
                             &instance->size);

  instance->is_fixed_sized = instance->size != 0;

  if (!instance->is_fixed_sized)
    instance->size = g_test_rand_int_range (0, MAX_INSTANCE_SIZE);

  instance->magic = INSTANCE_MAGIC;

  return instance;
}

static void
random_instance_free (RandomInstance *instance)
{
  g_variant_type_info_unref (instance->type_info);
  g_slice_free (RandomInstance, instance);
}

static void
append_instance_size (RandomInstance *instance,
                      gsize          *offset)
{
  *offset += (-*offset) & instance->alignment;
  *offset += instance->size;
}

static void
random_instance_write (RandomInstance *instance,
                       guchar         *buffer)
{
  GRand *rand;
  gint i;

  g_assert_cmpint ((gsize) buffer & instance->alignment, ==, 0);

  rand = g_rand_new_with_seed (instance->seed);
  for (i = 0; i < instance->size; i++)
    buffer[i] = g_rand_int (rand);
  g_rand_free (rand);
}

static void
append_instance_data (RandomInstance  *instance,
                      guchar         **buffer)
{
  while (((gsize) *buffer) & instance->alignment)
    *(*buffer)++ = '\0';

  random_instance_write (instance, *buffer);
  *buffer += instance->size;
}

static gboolean
random_instance_assert (RandomInstance *instance,
                        guchar         *buffer,
                        gsize           size)
{
  GRand *rand;
  gint i;

  g_assert_cmpint ((gsize) buffer & instance->alignment, ==, 0);
  g_assert_cmpint (size, ==, instance->size);

  rand = g_rand_new_with_seed (instance->seed);
  for (i = 0; i < instance->size; i++)
    {
      guchar byte = g_rand_int (rand);

      g_assert (buffer[i] == byte);
    }
  g_rand_free (rand);

  return i == instance->size;
}

static gboolean
random_instance_check (RandomInstance *instance,
                       guchar         *buffer,
                       gsize           size)
{
  GRand *rand;
  gint i;

  g_assert_cmpint ((gsize) buffer & instance->alignment, ==, 0);

  if (size != instance->size)
    return FALSE;

  rand = g_rand_new_with_seed (instance->seed);
  for (i = 0; i < instance->size; i++)
    if (buffer[i] != (guchar) g_rand_int (rand))
      break;
  g_rand_free (rand);

  return i == instance->size;
}

static void
random_instance_filler (GVariantSerialised *serialised,
                        gpointer            data)
{
  RandomInstance *instance = data;

  g_assert (instance->magic == INSTANCE_MAGIC);

  if (serialised->type_info == NULL)
    serialised->type_info = instance->type_info;

  if (serialised->size == 0)
    serialised->size = instance->size;

  g_assert (serialised->type_info == instance->type_info);
  g_assert (serialised->size == instance->size);

  if (serialised->data)
    random_instance_write (instance, serialised->data);
}

static gsize
calculate_offset_size (gsize body_size,
                       gsize n_offsets)
{
  if (body_size == 0)
    return 0;

  if (body_size + n_offsets <= G_MAXUINT8)
    return 1;

  if (body_size + 2 * n_offsets <= G_MAXUINT16)
    return 2;

  if (body_size + 4 * n_offsets <= G_MAXUINT32)
    return 4;

  /* the test case won't generate anything bigger */
  g_assert_not_reached ();
}

static gpointer
flavoured_malloc (gsize size, gsize flavour)
{
  g_assert (flavour < 8);

  if (size == 0)
    return NULL;

  return g_malloc (size + flavour) + flavour;
}

static void
flavoured_free (gpointer data)
{
  g_free ((gpointer) (((gsize) data) & ~7));
}

static void
append_offset (guchar **offset_ptr,
               gsize    offset,
               guint    offset_size)
{
  union
  {
    guchar bytes[sizeof (gsize)];
    gsize integer;
  } tmpvalue;

  tmpvalue.integer = GSIZE_TO_LE (offset);
  memcpy (*offset_ptr, tmpvalue.bytes, offset_size);
  *offset_ptr += offset_size;
}

static void
prepend_offset (guchar **offset_ptr,
                gsize    offset,
                guint    offset_size)
{
  union
  {
    guchar bytes[sizeof (gsize)];
    gsize integer;
  } tmpvalue;

  *offset_ptr -= offset_size;
  tmpvalue.integer = GSIZE_TO_LE (offset);
  memcpy (*offset_ptr, tmpvalue.bytes, offset_size);
}

static void
test_maybe (void)
{
  GVariantTypeInfo *type_info;
  RandomInstance *instance;
  gsize needed_size;
  guchar *data;

  instance = random_instance (NULL);

  {
    const gchar *element;
    gchar *tmp;

    element = g_variant_type_info_get_type_string (instance->type_info);
    tmp = g_strdup_printf ("m%s", element);
    type_info = g_variant_type_info_get (G_VARIANT_TYPE (tmp));
    g_free (tmp);
  }

  needed_size = g_variant_serialiser_needed_size (type_info,
                                                  random_instance_filler,
                                                  NULL, 0);
  g_assert_cmpint (needed_size, ==, 0);

  needed_size = g_variant_serialiser_needed_size (type_info,
                                                  random_instance_filler,
                                                  (gpointer *) &instance, 1);

  if (instance->is_fixed_sized)
    g_assert_cmpint (needed_size, ==, instance->size);
  else
    g_assert_cmpint (needed_size, ==, instance->size + 1);

  {
    guchar *ptr;

    ptr = data = g_malloc (needed_size);
    append_instance_data (instance, &ptr);

    if (!instance->is_fixed_sized)
      *ptr++ = '\0';

    g_assert_cmpint (ptr - data, ==, needed_size);
  }

  {
    guint alignment;
    guint flavour;

    alignment = instance->alignment + 1;

    for (flavour = 0; flavour < 8; flavour += alignment)
      {
        GVariantSerialised serialised;
        GVariantSerialised child;

        serialised.type_info = type_info;
        serialised.data = flavoured_malloc (needed_size, flavour);
        serialised.size = needed_size;

        g_variant_serialiser_serialise (serialised,
                                        random_instance_filler,
                                        (gpointer *) &instance, 1);
        child = g_variant_serialised_get_child (serialised, 0);
        g_assert (child.type_info == instance->type_info);
        random_instance_assert (instance, child.data, child.size);
        g_variant_type_info_unref (child.type_info);
        flavoured_free (serialised.data);
      }
  }

  g_variant_type_info_unref (type_info);
  random_instance_free (instance);
  g_free (data);
}

static void
test_maybes (void)
{
  guint i;

  for (i = 0; i < 1000; i++)
    test_maybe ();

  assert_no_type_infos ();
}

static void
test_array (void)
{
  GVariantTypeInfo *element_info;
  GVariantTypeInfo *array_info;
  RandomInstance **instances;
  gsize needed_size;
  gsize offset_size;
  guint n_children;
  guchar *data;

  {
    gchar *element_type, *array_type;

    element_type = random_type_string ();
    array_type = g_strdup_printf ("a%s", element_type);

    element_info = g_variant_type_info_get (G_VARIANT_TYPE (element_type));
    array_info = g_variant_type_info_get (G_VARIANT_TYPE (array_type));
    g_assert (g_variant_type_info_element (array_info) == element_info);

    g_free (element_type);
    g_free (array_type);
  }

  {
    guint i;

    n_children = g_test_rand_int_range (0, MAX_ARRAY_CHILDREN);
    instances = g_new (RandomInstance *, n_children);
    for (i = 0; i < n_children; i++)
      instances[i] = random_instance (element_info);
  }

  needed_size = g_variant_serialiser_needed_size (array_info,
                                                  random_instance_filler,
                                                  (gpointer *) instances,
                                                  n_children);

  {
    gsize element_fixed_size;
    gsize body_size = 0;
    guint i;

    for (i = 0; i < n_children; i++)
      append_instance_size (instances[i], &body_size);

    g_variant_type_info_query (element_info, NULL, &element_fixed_size);

    if (!element_fixed_size)
      {
        offset_size = calculate_offset_size (body_size, n_children);

        if (offset_size == 0)
          offset_size = 1;
      }
    else
      offset_size = 0;

    g_assert_cmpint (needed_size, ==, body_size + n_children * offset_size);
  }

  {
    guchar *offset_ptr, *body_ptr;
    guint i;

    body_ptr = data = g_malloc (needed_size);
    offset_ptr = body_ptr + needed_size - offset_size * n_children;

    for (i = 0; i < n_children; i++)
      {
        append_instance_data (instances[i], &body_ptr);
        append_offset (&offset_ptr, body_ptr - data, offset_size);
      }

    g_assert (body_ptr == data + needed_size - offset_size * n_children);
    g_assert (offset_ptr == data + needed_size);
  }

  {
    guint alignment;
    gsize flavour;
    guint i;

    g_variant_type_info_query (array_info, &alignment, NULL);
    alignment++;

    for (flavour = 0; flavour < 8; flavour += alignment)
      {
        GVariantSerialised serialised;

        serialised.type_info = array_info;
        serialised.data = flavoured_malloc (needed_size, flavour);
        serialised.size = needed_size;

        g_variant_serialiser_serialise (serialised, random_instance_filler,
                                        (gpointer *) instances, n_children);

        g_assert (memcmp (serialised.data, data, serialised.size) == 0);
        g_assert (g_variant_serialised_n_children (serialised) == n_children);

        for (i = 0; i < n_children; i++)
          {
            GVariantSerialised child;

            child = g_variant_serialised_get_child (serialised, i);
            g_assert (child.type_info == instances[i]->type_info);
            random_instance_assert (instances[i], child.data, child.size);
            g_variant_type_info_unref (child.type_info);
          }

        flavoured_free (serialised.data);
      }
  }

  {
    guint i;

    for (i = 0; i < n_children; i++)
      random_instance_free (instances[i]);
    g_free (instances);
  }

  g_variant_type_info_unref (element_info);
  g_variant_type_info_unref (array_info);
  g_free (data);
}

static void
test_arrays (void)
{
  guint i;

  for (i = 0; i < 100; i++)
    test_array ();

  assert_no_type_infos ();
}

static void
test_tuple (void)
{
  GVariantTypeInfo *type_info;
  RandomInstance **instances;
  gboolean fixed_size;
  gsize needed_size;
  gsize offset_size;
  guint n_children;
  guint alignment;
  guchar *data;

  n_children = g_test_rand_int_range (0, MAX_TUPLE_CHILDREN);
  instances = g_new (RandomInstance *, n_children);

  {
    GString *type_string;
    guint i;

    fixed_size = TRUE;
    alignment = 0;

    type_string = g_string_new ("(");
    for (i = 0; i < n_children; i++)
      {
        const gchar *str;

        instances[i] = random_instance (NULL);

        alignment |= instances[i]->alignment;
        if (!instances[i]->is_fixed_sized)
          fixed_size = FALSE;

        str = g_variant_type_info_get_type_string (instances[i]->type_info);
        g_string_append (type_string, str);
      }
    g_string_append_c (type_string, ')');

    type_info = g_variant_type_info_get (G_VARIANT_TYPE (type_string->str));
    g_string_free (type_string, TRUE);
  }

  needed_size = g_variant_serialiser_needed_size (type_info,
                                                  random_instance_filler,
                                                  (gpointer *) instances,
                                                  n_children);
  {
    gsize body_size = 0;
    gsize offsets = 0;
    guint i;

    for (i = 0; i < n_children; i++)
      {
        append_instance_size (instances[i], &body_size);

        if (i != n_children - 1 && !instances[i]->is_fixed_sized)
          offsets++;
      }

    if (fixed_size)
      {
        body_size += (-body_size) & alignment;

        g_assert ((body_size == 0) == (n_children == 0));
        if (n_children == 0)
          body_size = 1;
      }

    offset_size = calculate_offset_size (body_size, offsets);
    g_assert_cmpint (needed_size, ==, body_size + offsets * offset_size);
  }

  {
    guchar *body_ptr;
    guchar *ofs_ptr;
    guint i;

    body_ptr = data = g_malloc (needed_size);
    ofs_ptr = body_ptr + needed_size;

    for (i = 0; i < n_children; i++)
      {
        append_instance_data (instances[i], &body_ptr);

        if (i != n_children - 1 && !instances[i]->is_fixed_sized)
          prepend_offset (&ofs_ptr, body_ptr - data, offset_size);
      }

    if (fixed_size)
      {
        while (((gsize) body_ptr) & alignment)
          *body_ptr++ = '\0';

        g_assert ((body_ptr == data) == (n_children == 0));
        if (n_children == 0)
          *body_ptr++ = '\0';

      }


    g_assert (body_ptr == ofs_ptr);
  }

  {
    gsize flavour;
    guint i;

    alignment++;

    for (flavour = 0; flavour < 8; flavour += alignment)
      {
        GVariantSerialised serialised;

        serialised.type_info = type_info;
        serialised.data = flavoured_malloc (needed_size, flavour);
        serialised.size = needed_size;

        g_variant_serialiser_serialise (serialised, random_instance_filler,
                                        (gpointer *) instances, n_children);

        g_assert (memcmp (serialised.data, data, serialised.size) == 0);
        g_assert (g_variant_serialised_n_children (serialised) == n_children);

        for (i = 0; i < n_children; i++)
          {
            GVariantSerialised child;

            child = g_variant_serialised_get_child (serialised, i);
            g_assert (child.type_info == instances[i]->type_info);
            random_instance_assert (instances[i], child.data, child.size);
            g_variant_type_info_unref (child.type_info);
          }

        flavoured_free (serialised.data);
      }
  }

  {
    guint i;

    for (i = 0; i < n_children; i++)
      random_instance_free (instances[i]);
    g_free (instances);
  }

  g_variant_type_info_unref (type_info);
  g_free (data);
}

static void
test_tuples (void)
{
  guint i;

  for (i = 0; i < 100; i++)
    test_tuple ();

  assert_no_type_infos ();
}

static void
test_variant (void)
{
  GVariantTypeInfo *type_info;
  RandomInstance *instance;
  const gchar *type_string;
  gsize needed_size;
  guchar *data;
  gsize len;

  type_info = g_variant_type_info_get (G_VARIANT_TYPE_VARIANT);
  instance = random_instance (NULL);

  type_string = g_variant_type_info_get_type_string (instance->type_info);
  len = strlen (type_string);

  needed_size = g_variant_serialiser_needed_size (type_info,
                                                  random_instance_filler,
                                                  (gpointer *) &instance, 1);

  g_assert_cmpint (needed_size, ==, instance->size + 1 + len);

  {
    guchar *ptr;

    ptr = data = g_malloc (needed_size);
    append_instance_data (instance, &ptr);
    *ptr++ = '\0';
    memcpy (ptr, type_string, len);
    ptr += len;

    g_assert (data + needed_size == ptr);
  }

  {
    /* variants are 8-aligned, so no extra flavouring */
    GVariantSerialised serialised;
    GVariantSerialised child;

    serialised.type_info = type_info;
    serialised.data = flavoured_malloc (needed_size, 0);
    serialised.size = needed_size;

    g_variant_serialiser_serialise (serialised, random_instance_filler,
                                    (gpointer *) &instance, 1);

    g_assert (memcmp (serialised.data, data, serialised.size) == 0);
    g_assert (g_variant_serialised_n_children (serialised) == 1);

    child = g_variant_serialised_get_child (serialised, 0);
    g_assert (child.type_info == instance->type_info);
    random_instance_check (instance, child.data, child.size);

    g_variant_type_info_unref (child.type_info);
    flavoured_free (serialised.data);
  }

  g_variant_type_info_unref (type_info);
  random_instance_free (instance);
  g_free (data);
}

static void
test_variants (void)
{
  guint i;

  for (i = 0; i < 100; i++)
    test_variant ();

  assert_no_type_infos ();
}

static void
test_strings (void)
{
  struct {
    guint flags;
    guint size;
    gconstpointer data;
  } test_cases[] = {
#define is_nval           0
#define is_string         1
#define is_objpath        is_string | 2
#define is_sig            is_string | 4
    { is_sig,       1, "" },
    { is_nval,      0, NULL },
    { is_string,   13, "hello world!" },
    { is_nval,     13, "hello world\0" },
    { is_nval,     13, "hello\0world!" },
    { is_nval,     12, "hello world!" },

    { is_objpath,   2, "/" },
    { is_objpath,   3, "/a" },
    { is_string,    3, "//" },
    { is_objpath,  11, "/some/path" },
    { is_string,   12, "/some/path/" },
    { is_nval,     11, "/some\0path" },
    { is_string,   11, "/some\\path" },
    { is_string,   12, "/some//path" },
    { is_string,   12, "/some-/path" },

    { is_sig,       2, "i" },
    { is_sig,       2, "s" },
    { is_sig,       5, "(si)" },
    { is_string,    4, "(si" },
    { is_string,    2, "*" },
    { is_sig,       3, "ai" },
    { is_string,    3, "mi" },
    { is_string,    2, "r" },
    { is_sig,      15, "(yyy{sv}ssiai)" },
    { is_string,   16, "(yyy{yv}ssiai))" },
    { is_string,   15, "(yyy{vv}ssiai)" },
    { is_string,   15, "(yyy{sv)ssiai}" }
  };
  guint i;

  for (i = 0; i < G_N_ELEMENTS (test_cases); i++)
    {
      guint flags;

      flags = g_variant_serialiser_is_string (test_cases[i].data,
                                              test_cases[i].size)
        ? 1 : 0;

      flags |= g_variant_serialiser_is_object_path (test_cases[i].data,
                                                    test_cases[i].size)
        ? 2 : 0;

      flags |= g_variant_serialiser_is_signature (test_cases[i].data,
                                                  test_cases[i].size)
        ? 4 : 0;

      g_assert (flags == test_cases[i].flags);
    }
}

typedef struct _TreeInstance TreeInstance;
struct _TreeInstance
{
  GVariantTypeInfo *info;

  TreeInstance **children;
  gsize n_children;

  union {
    guint64 integer;
    gchar string[32];
  } data;
  gsize data_size;
};

static GVariantType *
make_random_definite_type (int depth)
{
  GString *description;
  GString *type_string;
  GVariantType *type;

  description = g_string_new (NULL);
  type_string = g_string_new (NULL);
  type = append_type_string (type_string, description, TRUE, depth);
  g_string_free (description, TRUE);
  g_string_free (type_string, TRUE);

  return type;
}

static void
make_random_string (gchar              *string,
                    gsize               size,
                    const GVariantType *type)
{
  gint i;

  /* create strings that are valid signature strings */
#define good_chars "bynqiuxthdsog"

  for (i = 0; i < size - 1; i++)
    string[i] = good_chars[g_test_rand_int_range (0, strlen (good_chars))];
  string[i] = '\0';

  /* in case we need an object path, prefix a '/' */
  if (*g_variant_type_peek_string (type) == 'o')
    string[0] = '/';

#undef good_chars
}

static TreeInstance *
make_tree_instance (const GVariantType *type,
                    int                 depth)
{
  const GVariantType *child_type = NULL;
  GVariantType *mytype = NULL;
  TreeInstance *instance;
  gboolean is_tuple_type;

  if (type == NULL)
    type = mytype = make_random_definite_type (depth);

  instance = g_slice_new (TreeInstance);
  instance->info = g_variant_type_info_get (type);
  instance->children = NULL;
  instance->n_children = 0;
  instance->data_size = 0;

  is_tuple_type = FALSE;

  switch (*g_variant_type_peek_string (type))
    {
    case G_VARIANT_TYPE_INFO_CHAR_MAYBE:
      instance->n_children = g_test_rand_int_range (0, 2);
      child_type = g_variant_type_element (type);
      break;

    case G_VARIANT_TYPE_INFO_CHAR_ARRAY:
      instance->n_children = g_test_rand_int_range (0, MAX_ARRAY_CHILDREN);
      child_type = g_variant_type_element (type);
      break;

    case G_VARIANT_TYPE_INFO_CHAR_DICT_ENTRY:
    case G_VARIANT_TYPE_INFO_CHAR_TUPLE:
      instance->n_children = g_variant_type_n_items (type);
      child_type = g_variant_type_first (type);
      is_tuple_type = TRUE;
      break;

    case G_VARIANT_TYPE_INFO_CHAR_VARIANT:
      instance->n_children = 1;
      child_type = NULL;
      break;

    case 'b':
      instance->data.integer = g_test_rand_int_range (0, 2);
      instance->data_size = 1;
      break;

    case 'y':
      instance->data.integer = g_test_rand_int ();
      instance->data_size = 1;
      break;

    case 'n': case 'q':
      instance->data.integer = g_test_rand_int ();
      instance->data_size = 2;
      break;

    case 'i': case 'u': case 'h':
      instance->data.integer = g_test_rand_int ();
      instance->data_size = 4;
      break;

    case 'x': case 't': case 'd':
      instance->data.integer = g_test_rand_int ();
      instance->data.integer <<= 32;
      instance->data.integer |= (guint32) g_test_rand_int ();
      instance->data_size = 8;
      break;

    case 's': case 'o': case 'g':
      instance->data_size = g_test_rand_int_range (10, 20);
      make_random_string (instance->data.string, instance->data_size, type);
      break;
    }

  if (instance->data_size == 0)
    /* no data -> it is a container */
    {
      guint i;

      instance->children = g_new (TreeInstance *, instance->n_children);

      for (i = 0; i < instance->n_children; i++)
        {
          instance->children[i] = make_tree_instance (child_type, depth - 1);

          if (is_tuple_type)
            child_type = g_variant_type_next (child_type);
        }

      g_assert (!is_tuple_type || child_type == NULL);
    }

  g_variant_type_free (mytype);

  return instance;
}

static void
tree_instance_free (TreeInstance *instance)
{
  gint i;

  g_variant_type_info_unref (instance->info);
  for (i = 0; i < instance->n_children; i++)
    tree_instance_free (instance->children[i]);
  g_free (instance->children);
  g_slice_free (TreeInstance, instance);
}

static gboolean i_am_writing_byteswapped;

static void
tree_filler (GVariantSerialised *serialised,
             gpointer            data)
{
  TreeInstance *instance = data;

  if (serialised->type_info == NULL)
    serialised->type_info = instance->info;

  if (instance->data_size == 0)
    /* is a container */
    {
      if (serialised->size == 0)
        serialised->size =
          g_variant_serialiser_needed_size (instance->info, tree_filler,
                                            (gpointer *) instance->children,
                                            instance->n_children);

      if (serialised->data)
        g_variant_serialiser_serialise (*serialised, tree_filler,
                                        (gpointer *) instance->children,
                                        instance->n_children);
    }
  else
    /* it is a leaf */
    {
      if (serialised->size == 0)
        serialised->size = instance->data_size;

      if (serialised->data)
        {
          switch (instance->data_size)
            {
            case 1:
              *serialised->data = instance->data.integer;
              break;

            case 2:
              {
                guint16 value = instance->data.integer;

                if (i_am_writing_byteswapped)
                  value = GUINT16_SWAP_LE_BE (value);

                *(guint16 *) serialised->data = value;
              }
              break;

            case 4:
              {
                guint32 value = instance->data.integer;

                if (i_am_writing_byteswapped)
                  value = GUINT32_SWAP_LE_BE (value);

                *(guint32 *) serialised->data = value;
              }
              break;

            case 8:
              {
                guint64 value = instance->data.integer;

                if (i_am_writing_byteswapped)
                  value = GUINT64_SWAP_LE_BE (value);

                *(guint64 *) serialised->data = value;
              }
              break;

            default:
              memcpy (serialised->data,
                      instance->data.string,
                      instance->data_size);
              break;
            }
        }
    }
}

static gboolean
check_tree (TreeInstance       *instance,
            GVariantSerialised  serialised)
{
  if (instance->info != serialised.type_info)
    return FALSE;

  if (instance->data_size == 0)
    /* is a container */
    {
      gint i;

      if (g_variant_serialised_n_children (serialised) !=
          instance->n_children)
        return FALSE;

      for (i = 0; i < instance->n_children; i++)
        {
          GVariantSerialised child;
          gpointer data = NULL;
          gboolean ok;

          child = g_variant_serialised_get_child (serialised, i);
          if (child.size && child.data == NULL)
            child.data = data = g_malloc0 (child.size);
          ok = check_tree (instance->children[i], child);
          g_variant_type_info_unref (child.type_info);
          g_free (data);

          if (!ok)
            return FALSE;
        }

      return TRUE;
    }
  else
    /* it is a leaf */
    {
      switch (instance->data_size)
        {
        case 1:
          g_assert (serialised.size == 1);
          return *(guint8 *) serialised.data ==
                  (guint8) instance->data.integer;

        case 2:
          g_assert (serialised.size == 2);
          return *(guint16 *) serialised.data ==
                  (guint16) instance->data.integer;

        case 4:
          g_assert (serialised.size == 4);
          return *(guint32 *) serialised.data ==
                  (guint32) instance->data.integer;

        case 8:
          g_assert (serialised.size == 8);
          return *(guint64 *) serialised.data ==
                  (guint64) instance->data.integer;

        default:
          if (serialised.size != instance->data_size)
            return FALSE;

          return memcmp (serialised.data,
                         instance->data.string,
                         instance->data_size) == 0;
        }
    }
}

static void
serialise_tree (TreeInstance       *tree,
                GVariantSerialised *serialised)
{
  GVariantSerialised empty = {  };

  *serialised = empty;
  tree_filler (serialised, tree);
  serialised->data = g_malloc (serialised->size);
  tree_filler (serialised, tree);
}

static void
test_byteswap (void)
{
  GVariantSerialised one, two;
  TreeInstance *tree;

  tree = make_tree_instance (NULL, 3);
  serialise_tree (tree, &one);

  i_am_writing_byteswapped = TRUE;
  serialise_tree (tree, &two);
  i_am_writing_byteswapped = FALSE;

  g_variant_serialised_byteswap (two);

  g_assert_cmpint (one.size, ==, two.size);
  g_assert (memcmp (one.data, two.data, one.size) == 0);

  tree_instance_free (tree);
  g_free (one.data);
  g_free (two.data);
}

static void
test_byteswaps (void)
{
  int i;

  for (i = 0; i < 200; i++)
    test_byteswap ();

  assert_no_type_infos ();
}

static void
test_fuzz (gdouble *fuzziness)
{
  GVariantSerialised serialised;
  TreeInstance *tree;

  /* make an instance */
  tree = make_tree_instance (NULL, 3);

  /* serialise it */
  serialise_tree (tree, &serialised);

  g_assert (g_variant_serialised_is_normal (serialised));
  g_assert (check_tree (tree, serialised));

  if (serialised.size)
    {
      gboolean fuzzed = FALSE;
      gboolean a, b;

      while (!fuzzed)
        {
          gint i;

          for (i = 0; i < serialised.size; i++)
            if (randomly (*fuzziness))
              {
                serialised.data[i] += g_test_rand_int_range (1, 256);
                fuzzed = TRUE;
              }
        }

      /* at least one byte in the serialised data has changed.
       *
       * this means that at least one of the following is true:
       *
       *    - the serialised data now represents a different value:
       *        check_tree() will return FALSE
       *
       *    - the serialised data is in non-normal form:
       *        g_variant_serialiser_is_normal() will return FALSE
       *
       * we always do both checks to increase exposure of the serialiser
       * to corrupt data.
       */
      a = g_variant_serialised_is_normal (serialised);
      b = check_tree (tree, serialised);

      g_assert (!a || !b);
    }

  tree_instance_free (tree);
  g_free (serialised.data);
}


static void
test_fuzzes (gpointer data)
{
  gdouble fuzziness;
  int i;

  fuzziness = GPOINTER_TO_INT (data) / 100.;

  for (i = 0; i < 200; i++)
    test_fuzz (&fuzziness);

  assert_no_type_infos ();
}

int
main (int argc, char **argv)
{
  gint i;

  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/gvariant/type", test_gvarianttype);
  g_test_add_func ("/gvariant/typeinfo", test_gvarianttypeinfo);
  g_test_add_func ("/gvariant/serialiser/maybe", test_maybes);
  g_test_add_func ("/gvariant/serialiser/array", test_arrays);
  g_test_add_func ("/gvariant/serialiser/tuple", test_tuples);
  g_test_add_func ("/gvariant/serialiser/variant", test_variants);
  g_test_add_func ("/gvariant/serialiser/strings", test_strings);
  g_test_add_func ("/gvariant/serialiser/byteswap", test_byteswaps);

  for (i = 1; i <= 20; i += 4)
    {
      char *testname;

      testname = g_strdup_printf ("/gvariant/serialiser/fuzz/%d%%", i);
      g_test_add_data_func (testname, GINT_TO_POINTER (i),
                            (gpointer) test_fuzzes);
      g_free (testname);
    }

  return g_test_run ();
}
