#include <glib.h>
#include "goption.h"
#include <string.h>

int error_test1_int;
char *error_test2_string;
gboolean error_test3_boolean;

int arg_test1_int;
gchar *arg_test2_string;

gchar **array_test1_array;

gboolean ignore_test1_boolean;
gboolean ignore_test2_boolean;

gchar **
split_string (const char *str, int *argc)
{
  gchar **argv;
  int len;
  
  argv = g_strsplit (str, " ", 0);

  for (len = 0; argv[len] != NULL; len++);

  if (argc)
    *argc = len;
    
  return argv;
}

gchar *
join_stringv (int argc, char **argv)
{
  int i;
  GString *str;

  str = g_string_new (NULL);

  for (i = 0; i < argc; i++)
    {
      g_string_append (str, argv[i]);

      if (i < argc - 1)
	g_string_append_c (str, ' ');
    }

  return g_string_free (str, FALSE);
}

/* Performs a shallow copy */
char **
copy_stringv (char **argv, int argc)
{
  return g_memdup (argv, sizeof (char *) * (argc + 1));
}


static gboolean
error_test1_pre_parse (GOptionContext *context,
		       GOptionGroup   *group,
		       gpointer	       data,
		       GError        **error)
{
  g_assert (error_test1_int == 0x12345678);

  return TRUE;
}

static gboolean
error_test1_post_parse (GOptionContext *context,
			GOptionGroup   *group,
			gpointer	  data,
			GError        **error)
{
  g_assert (error_test1_int == 20);

  /* Set an error in the post hook */
  g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE, NULL);

  return FALSE;
}

void
error_test1 (void)
{
  GOptionContext *context;
  gboolean retval;
  GError *error = NULL;
  gchar **argv;
  int argc;
  GOptionGroup *main_group;
  GOptionEntry entries [] =
    { { "test", 0, 0, G_OPTION_ARG_INT, &error_test1_int, NULL, NULL },
      { NULL } };
  
  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Set pre and post parse hooks */
  main_group = g_option_context_get_main_group (context);
  g_option_group_set_parse_hooks (main_group,
				  error_test1_pre_parse, error_test1_post_parse);
  
  /* Now try parsing */
  argv = split_string ("program --test 20", &argc);

  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert (retval == FALSE);

  /* On failure, values should be reset */
  g_assert (error_test1_int == 0x12345678);
  
  g_strfreev (argv);
  g_option_context_free (context);
  
}

static gboolean
error_test2_pre_parse (GOptionContext *context,
		       GOptionGroup   *group,
		       gpointer	  data,
		       GError        **error)
{
  g_assert (strcmp (error_test2_string, "foo") == 0);

  return TRUE;
}

static gboolean
error_test2_post_parse (GOptionContext *context,
			GOptionGroup   *group,
			gpointer	  data,
			GError        **error)
{
  g_assert (strcmp (error_test2_string, "bar") == 0);

  /* Set an error in the post hook */
  g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE, NULL);

  return FALSE;
}

void
error_test2 (void)
{
  GOptionContext *context;
  gboolean retval;
  GError *error = NULL;
  gchar **argv;
  int argc;
  GOptionGroup *main_group;
  GOptionEntry entries [] =
    { { "test", 0, 0, G_OPTION_ARG_STRING, &error_test2_string, NULL, NULL },
      { NULL } };

  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Set pre and post parse hooks */
  main_group = g_option_context_get_main_group (context);
  g_option_group_set_parse_hooks (main_group,
				  error_test2_pre_parse, error_test2_post_parse);
  
  /* Now try parsing */
  argv = split_string ("program --test bar", &argc);
  retval = g_option_context_parse (context, &argc, &argv, &error);

  g_error_free (error);
  g_assert (retval == FALSE);

  g_assert (strcmp (error_test2_string, "foo") == 0);
  
  g_strfreev (argv);
  g_option_context_free (context);
}

static gboolean
error_test3_pre_parse (GOptionContext *context,
		       GOptionGroup   *group,
		       gpointer	  data,
		       GError        **error)
{
  g_assert (!error_test3_boolean);

  return TRUE;
}

static gboolean
error_test3_post_parse (GOptionContext *context,
			GOptionGroup   *group,
			gpointer	  data,
			GError        **error)
{
  g_assert (error_test3_boolean);

  /* Set an error in the post hook */
  g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE, NULL);

  return FALSE;
}

void
error_test3 (void)
{
  GOptionContext *context;
  gboolean retval;
  GError *error = NULL;
  gchar **argv;
  int argc;
  GOptionGroup *main_group;
  GOptionEntry entries [] =
    { { "test", 0, 0, G_OPTION_ARG_NONE, &error_test3_boolean, NULL, NULL },
      { NULL } };

  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Set pre and post parse hooks */
  main_group = g_option_context_get_main_group (context);
  g_option_group_set_parse_hooks (main_group,
				  error_test3_pre_parse, error_test3_post_parse);
  
  /* Now try parsing */
  argv = split_string ("program --test", &argc);
  retval = g_option_context_parse (context, &argc, &argv, &error);

  g_error_free (error);
  g_assert (retval == FALSE);

  g_assert (!error_test3_boolean);
  
  g_strfreev (argv);
  g_option_context_free (context);
}

void
arg_test1 (void)
{
  GOptionContext *context;
  gboolean retval;
  GError *error = NULL;
  gchar **argv;
  int argc;
  GOptionEntry entries [] =
    { { "test", 0, 0, G_OPTION_ARG_INT, &arg_test1_int, NULL, NULL },
      { NULL } };

  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Now try parsing */
  argv = split_string ("program --test 20 --test 30", &argc);

  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert (retval);

  /* Last arg specified is the one that should be stored */
  g_assert (arg_test1_int == 30);

  g_strfreev (argv);
  g_option_context_free (context);
}

void
arg_test2 (void)
{
  GOptionContext *context;
  gboolean retval;
  GError *error = NULL;
  gchar **argv;
  int argc;
  GOptionEntry entries [] =
    { { "test", 0, 0, G_OPTION_ARG_STRING, &arg_test2_string, NULL, NULL },
      { NULL } };
  
  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Now try parsing */
  argv = split_string ("program --test foo --test bar", &argc);
  
  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert (retval);

  /* Last arg specified is the one that should be stored */
  g_assert (strcmp (arg_test2_string, "bar") == 0);

  g_free (arg_test2_string);
  
  g_strfreev (argv);
  g_option_context_free (context);
}

void
ignore_test1 (void)
{
  GOptionContext *context;
  gboolean retval;
  GError *error = NULL;
  gchar **argv, **argv_copy;
  int argc;
  gchar *arg;
  GOptionEntry entries [] =
    { { "test", 0, 0, G_OPTION_ARG_NONE, &ignore_test1_boolean, NULL, NULL },
      { NULL } };

  context = g_option_context_new (NULL);
  g_option_context_set_ignore_unknown_options (context, TRUE);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Now try parsing */
  argv = split_string ("program --test --hello", &argc);
  argv_copy = copy_stringv (argv, argc);
  
  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert (retval);

  /* Check array */
  arg = join_stringv (argc, argv);
  g_assert (strcmp (arg, "program --hello") == 0);

  g_free (arg);
  g_strfreev (argv_copy);
  g_free (argv);
  g_option_context_free (context);
}

void
ignore_test2 (void)
{
  GOptionContext *context;
  gboolean retval;
  GError *error = NULL;
  gchar **argv;
  int argc;
  gchar *arg;
  GOptionEntry entries [] =
    { { "test", 't', 0, G_OPTION_ARG_NONE, &ignore_test2_boolean, NULL, NULL },
      { NULL } };

  context = g_option_context_new (NULL);
  g_option_context_set_ignore_unknown_options (context, TRUE);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Now try parsing */
  argv = split_string ("program -test", &argc);
  
  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert (retval);

  /* Check array */
  arg = join_stringv (argc, argv);
  g_assert (strcmp (arg, "program -es") == 0);

  g_free (arg);
  g_strfreev (argv);
  g_option_context_free (context);
}

void
array_test1 (void)
{
  GOptionContext *context;
  gboolean retval;
  GError *error = NULL;
  gchar **argv;
  int argc;
  GOptionEntry entries [] =
    { { "test", 0, 0, G_OPTION_ARG_STRING_ARRAY, &array_test1_array, NULL, NULL },
      { NULL } };
        
  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Now try parsing */
  argv = split_string ("program --test foo --test bar", &argc);
  
  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert (retval);

  /* Check array */
  g_assert (strcmp (array_test1_array[0], "foo") == 0);
  g_assert (strcmp (array_test1_array[1], "bar") == 0);
  g_assert (array_test1_array[2] == NULL);

  g_strfreev (array_test1_array);
  
  g_strfreev (argv);
  g_option_context_free (context);
}

void
add_test1 (void)
{
  GOptionContext *context;

  GOptionEntry entries1 [] =
    { { "test1", 0, 0, G_OPTION_ARG_STRING_ARRAY, NULL, NULL, NULL },
      { NULL } };
  GOptionEntry entries2 [] =
    { { "test2", 0, 0, G_OPTION_ARG_STRING_ARRAY, NULL, NULL, NULL },
      { NULL } };

  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries1, NULL);
  g_option_context_add_main_entries (context, entries2, NULL);

  g_option_context_free (context);
}

int
main (int argc, char **argv)
{
  /* Test that restoration on failure works */
  error_test1_int = 0x12345678;
  error_test1 ();
  error_test2_string = "foo";
  error_test2 ();
  error_test3_boolean = FALSE;
  error_test3 ();
  
  /* Test that special argument parsing works */
  arg_test1 ();
  arg_test2 ();

  /* Test string arrays */
  array_test1 ();

  /* Test ignoring options */
  ignore_test1 ();
  ignore_test2 ();

  add_test1 ();
  
  return 0;
}
