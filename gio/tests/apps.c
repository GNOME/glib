#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>
#include <locale.h>

static void
print (const gchar *str)
{
  g_print ("%s\n", str ? str : "nil");
}

int
main (int argc, char **argv)
{
  setlocale (LC_ALL, "");

  if (argv[1] == NULL)
    ;
  else if (g_str_equal (argv[1], "list"))
    {
      GList *all, *i;

      all = g_app_info_get_all ();
      for (i = all; i; i = i->next)
        g_print ("%s%s", g_app_info_get_id (i->data), i->next ? " " : "\n");
      g_list_free_full (all, g_object_unref);
    }
  else if (g_str_equal (argv[1], "search"))
    {
      gchar ***results;
      gint i, j;

      results = g_desktop_app_info_search (argv[2]);
      for (i = 0; results[i]; i++)
        {
          for (j = 0; results[i][j]; j++)
            g_print ("%s%s", j ? " " : "", results[i][j]);
          g_print ("\n");
          g_strfreev (results[i]);
        }
      g_free (results);
    }
  else if (g_str_equal (argv[1], "show-info"))
    {
      GAppInfo *info;

      info = (GAppInfo *) g_desktop_app_info_new (argv[2]);
      if (info)
        {
          print (g_app_info_get_id (info));
          print (g_app_info_get_name (info));
          print (g_app_info_get_display_name (info));
          print (g_app_info_get_description (info));
          g_object_unref (info);
        }
    }

  return 0;
}
