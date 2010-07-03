#include <gio.h>

static void
test_themed_icon (void)
{
  GIcon *icon1, *icon2, *icon3;
  const gchar *const *names;
  const gchar *names2[] = { "first", "testicon", "last", NULL };
  gchar *str;

  icon1 = g_themed_icon_new ("testicon");

  names = g_themed_icon_get_names (G_THEMED_ICON (icon1));
  g_assert_cmpint (g_strv_length ((gchar **)names), ==, 1);
  g_assert_cmpstr (names[0], ==, "testicon");

  g_themed_icon_prepend_name (G_THEMED_ICON (icon1), "first");
  g_themed_icon_append_name (G_THEMED_ICON (icon1), "last");
  names = g_themed_icon_get_names (G_THEMED_ICON (icon1));
  g_assert_cmpint (g_strv_length ((gchar **)names), ==, 3);
  g_assert_cmpstr (names[0], ==, "first");
  g_assert_cmpstr (names[1], ==, "testicon");
  g_assert_cmpstr (names[2], ==, "last");
  g_assert_cmpuint (g_icon_hash (icon1), ==, 3193088045);

  icon2 = g_themed_icon_new_from_names (names2, -1);
  g_assert (g_icon_equal (icon1, icon2));

  str = g_icon_to_string (icon2);
  icon3 = g_icon_new_for_string (str, NULL);
  g_assert (g_icon_equal (icon2, icon3));
  g_free (str);

  g_object_unref (icon1);
  g_object_unref (icon2);
  g_object_unref (icon3);
}

static void
test_emblemed_icon (void)
{
  GIcon *icon1, *icon2, *icon3, *icon4;
  GEmblem *emblem, *emblem1, *emblem2;
  GList *emblems;

  icon1 = g_themed_icon_new ("testicon");
  icon2 = g_themed_icon_new ("testemblem");
  emblem1 = g_emblem_new (icon2);
  emblem2 = g_emblem_new_with_origin (icon2, G_EMBLEM_ORIGIN_TAG);

  icon3 = g_emblemed_icon_new (icon1, emblem1);
  emblems = g_emblemed_icon_get_emblems (G_EMBLEMED_ICON (icon3));
  g_assert_cmpint (g_list_length (emblems), ==, 1);

  icon4 = g_emblemed_icon_new (icon1, emblem1);
  g_emblemed_icon_add_emblem (G_EMBLEMED_ICON (icon4), emblem2);
  emblems = g_emblemed_icon_get_emblems (G_EMBLEMED_ICON (icon4));
  g_assert_cmpint (g_list_length (emblems), ==, 2);

  g_assert (!g_icon_equal (icon3, icon4));

  emblem = emblems->data;
  g_assert (g_emblem_get_icon (emblem) == icon2);
  g_assert (g_emblem_get_origin (emblem) == G_EMBLEM_ORIGIN_UNKNOWN);

  emblem = emblems->next->data;
  g_assert (g_emblem_get_icon (emblem) == icon2);
  g_assert (g_emblem_get_origin (emblem) == G_EMBLEM_ORIGIN_TAG);

  g_object_unref (icon1);
  g_object_unref (icon2);
  g_object_unref (icon3);
  g_object_unref (icon4);

  g_object_unref (emblem1);
  g_object_unref (emblem2);
}

static void
test_file_icon (void)
{
  GFile *file;
  GIcon *icon;
  GIcon *icon2;
  GError *error;
  GInputStream *stream;
  gchar *str;

  file = g_file_new_for_path (SRCDIR "/icons.c");
  icon = g_file_icon_new (file);
  g_object_unref (file);

  error = NULL;
  stream = g_loadable_icon_load (G_LOADABLE_ICON (icon), 20, NULL, NULL, &error);
  g_assert (stream != NULL);
  g_assert_no_error (error);

  g_object_unref (stream);

  str = g_icon_to_string (icon);
  icon2 = g_icon_new_for_string (str, NULL);
  g_assert (g_icon_equal (icon, icon2));
  g_free (str);

  g_object_unref (icon);
  g_object_unref (icon2);
}

int
main (int argc, char *argv[])
{
  g_type_init ();

  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/icons/themed", test_themed_icon);
  g_test_add_func ("/icons/emblemed", test_emblemed_icon);
  g_test_add_func ("/icons/file", test_file_icon);

  return g_test_run ();
}
