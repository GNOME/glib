/* GObject - GLib Type, Object, Parameter and Signal Library
 * Copyright (C) 2006 Imendio AB
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include <glib-object.h>

/* --- MySingleton class --- */

struct _MySingleton {
  GObject parent_instance;
};

#define MY_TYPE_SINGLETON my_singleton_get_type ()
G_DECLARE_FINAL_TYPE (MySingleton, my_singleton, MY, SINGLETON, GObject)
G_DEFINE_FINAL_TYPE (MySingleton, my_singleton, G_TYPE_OBJECT)

static MySingleton *the_one_and_only = NULL;

/* --- methods --- */
static GObject*
my_singleton_constructor (GType                  type,
                          guint                  n_construct_properties,
                          GObjectConstructParam *construct_properties)
{
  if (the_one_and_only)
    return g_object_ref (G_OBJECT (the_one_and_only));
  else
    return G_OBJECT_CLASS (my_singleton_parent_class)->constructor (type, n_construct_properties, construct_properties);
}

static void
my_singleton_init (MySingleton *self)
{
  g_assert_null (the_one_and_only);
  the_one_and_only = self;
}

static void
my_singleton_class_init (MySingletonClass *klass)
{
  G_OBJECT_CLASS (klass)->constructor = my_singleton_constructor;
}

static void
test_singleton_construction (void)
{
  MySingleton *singleton, *obj;

  /* create the singleton */
  singleton = g_object_new (MY_TYPE_SINGLETON, NULL);
  g_assert_nonnull (singleton);

  /* assert _singleton_ creation */
  obj = g_object_new (MY_TYPE_SINGLETON, NULL);
  g_assert_true (singleton == obj);
  g_object_unref (obj);

  /* shutdown */
  g_object_unref (singleton);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/gobject/singleton/construction", test_singleton_construction);

  return g_test_run ();
}
