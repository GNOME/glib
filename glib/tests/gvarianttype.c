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
static GVariantType *append_tuple_type_string (GString *, GString *, gint);

/* append a random GVariantType to a GString
 * append a description of the type to another GString
 * return what the type is
 */
static GVariantType *
append_type_string (GString *string,
                    GString *description,
                    gint     depth)
{
  if (!depth-- || randomly (0.3))
    {
      gchar b = BASIC[g_test_rand_int_range (0, N_BASIC)];
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

      switch (g_test_rand_int_range (0, 7))
        {
        case 0:
          {
            GVariantType *element;

            g_string_append_c (string, 'a');
            g_string_append (description, "a of ");
            element = append_type_string (string, description, depth);
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
            element = append_type_string (string, description, depth);
            result = g_variant_type_new_maybe (element);
            g_variant_type_free (element);
          }

          g_assert (g_variant_type_is_maybe (result));
          break;

        case 2:
          result = append_tuple_type_string (string, description, depth);

          g_assert (g_variant_type_is_tuple (result));
          break;

        case 3:
          {
            GVariantType *key, *value;

            g_string_append_c (string, '{');
            g_string_append (description, "e of [");
            key = append_type_string (string, description, 0);
            g_string_append (description, ", ");
            value = append_type_string (string, description, depth);
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
append_tuple_type_string (GString *string,
                          GString *description,
                          gint     depth)
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
      types[i] = append_type_string (string, description, depth);

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
    replacement = append_tuple_type_string (result, junk, 3);

  else if (type_string[l] == '?')
    replacement = append_type_string (result, junk, 0);

  else if (type_string[l] == '*')
    replacement = append_type_string (result, junk, 3);

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
      type = append_type_string (type_string, description, 6);

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
      other_type = append_type_string (type_string, description, 2);

      g_string_free (description, TRUE);
      g_string_free (type_string, TRUE);
      g_variant_type_free (other_type);
      g_variant_type_free (type);
    }
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/gvariant/type", test_gvarianttype);

  return g_test_run ();
}
