#include <glib.h>
#include <string.h>

int error_test1_int;
char *error_test2_string;
gboolean error_test3_boolean;

int arg_test1_int;
gchar *arg_test2_string;
gchar *arg_test3_filename;

gchar *callback_test1_string;
gboolean callback_test2_int;

gchar *callback_test_optional_string;
gboolean callback_test_optional_boolean;

gchar **array_test1_array;

gboolean ignore_test1_boolean;
gboolean ignore_test2_boolean;
gchar *ignore_test3_string;

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
  g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE, "");

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
  g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE, "");

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
  g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE, "");

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
arg_test3 (void)
{
  GOptionContext *context;
  gboolean retval;
  GError *error = NULL;
  gchar **argv;
  int argc;
  GOptionEntry entries [] =
    { { "test", 0, 0, G_OPTION_ARG_FILENAME, &arg_test3_filename, NULL, NULL },
      { NULL } };
  
  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Now try parsing */
  argv = split_string ("program --test foo.txt", &argc);
  
  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert (retval);

  /* Last arg specified is the one that should be stored */
  g_assert (strcmp (arg_test3_filename, "foo.txt") == 0);

  g_free (arg_test3_filename);
  
  g_strfreev (argv);
  g_option_context_free (context);
}

static gboolean
callback_parse1 (const gchar *option_name, const gchar *value,
		 gpointer data, GError **error)
{
	callback_test1_string = g_strdup (value);
	return TRUE;
}

void
callback_test1 (void)
{
  GOptionContext *context;
  gboolean retval;
  GError *error = NULL;
  gchar **argv;
  int argc;
  GOptionEntry entries [] =
    { { "test", 0, 0, G_OPTION_ARG_CALLBACK, callback_parse1, NULL, NULL },
      { NULL } };
  
  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Now try parsing */
  argv = split_string ("program --test foo.txt", &argc);
  
  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert (retval);

  g_assert (strcmp (callback_test1_string, "foo.txt") == 0);

  g_free (callback_test1_string);
  
  g_strfreev (argv);
  g_option_context_free (context);
}

static gboolean
callback_parse2 (const gchar *option_name, const gchar *value,
		 gpointer data, GError **error)
{
	callback_test2_int++;
	return TRUE;
}

void
callback_test2 (void)
{
  GOptionContext *context;
  gboolean retval;
  GError *error = NULL;
  gchar **argv;
  int argc;
  GOptionEntry entries [] =
    { { "test", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, callback_parse2, NULL, NULL },
      { NULL } };
  
  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Now try parsing */
  argv = split_string ("program --test --test", &argc);
  
  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert (retval);

  g_assert (callback_test2_int == 2);
  
  g_strfreev (argv);
  g_option_context_free (context);
}

static gboolean
callback_parse_optional (const gchar *option_name, const gchar *value,
		 gpointer data, GError **error)
{
	callback_test_optional_boolean = TRUE;
	if (value)
		callback_test_optional_string = g_strdup (value);
	else
		callback_test_optional_string = NULL;
	return TRUE;
}

void
callback_test_optional_1 (void)
{
  GOptionContext *context;
  gboolean retval;
  GError *error = NULL;
  gchar **argv;
  int argc;
  GOptionEntry entries [] =
    { { "test", 0, G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_CALLBACK, 
	callback_parse_optional, NULL, NULL },
      { NULL } };
  
  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Now try parsing */
  argv = split_string ("program --test foo.txt", &argc);
  
  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert (retval);

  g_assert (strcmp (callback_test_optional_string, "foo.txt") == 0);
  
  g_assert (callback_test_optional_boolean);

  g_free (callback_test_optional_string);
  
  g_strfreev (argv);
  g_option_context_free (context);
}

void
callback_test_optional_2 (void)
{
  GOptionContext *context;
  gboolean retval;
  GError *error = NULL;
  gchar **argv;
  int argc;
  GOptionEntry entries [] =
    { { "test", 0, G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_CALLBACK, 
	callback_parse_optional, NULL, NULL },
      { NULL } };
  
  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Now try parsing */
  argv = split_string ("program --test", &argc);
  
  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert (retval);

  g_assert (callback_test_optional_string == NULL);
  
  g_assert (callback_test_optional_boolean);

  g_free (callback_test_optional_string);
  
  g_strfreev (argv);
  g_option_context_free (context);
}

void
callback_test_optional_3 (void)
{
  GOptionContext *context;
  gboolean retval;
  GError *error = NULL;
  gchar **argv;
  int argc;
  GOptionEntry entries [] =
    { { "test", 't', G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_CALLBACK, 
	callback_parse_optional, NULL, NULL },
      { NULL } };
  
  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Now try parsing */
  argv = split_string ("program -t foo.txt", &argc);
  
  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert (retval);

  g_assert (strcmp (callback_test_optional_string, "foo.txt") == 0);
  
  g_assert (callback_test_optional_boolean);

  g_free (callback_test_optional_string);
  
  g_strfreev (argv);
  g_option_context_free (context);
}


void
callback_test_optional_4 (void)
{
  GOptionContext *context;
  gboolean retval;
  GError *error = NULL;
  gchar **argv;
  int argc;
  GOptionEntry entries [] =
    { { "test", 't', G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_CALLBACK, 
	callback_parse_optional, NULL, NULL },
      { NULL } };
  
  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Now try parsing */
  argv = split_string ("program -t", &argc);
  
  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert (retval);

  g_assert (callback_test_optional_string == NULL);
  
  g_assert (callback_test_optional_boolean);

  g_free (callback_test_optional_string);
  
  g_strfreev (argv);
  g_option_context_free (context);
}

void
callback_test_optional_5 (void)
{
  GOptionContext *context;
  gboolean dummy;
  gboolean retval;
  GError *error = NULL;
  gchar **argv;
  int argc;
  GOptionEntry entries [] =
    { { "dummy", 'd', 0, G_OPTION_ARG_NONE, &dummy, NULL },
      { "test", 't', G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_CALLBACK, 
	callback_parse_optional, NULL, NULL },
      { NULL } };
  
  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Now try parsing */
  argv = split_string ("program --test --dummy", &argc);
  
  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert (retval);

  g_assert (callback_test_optional_string == NULL);
  
  g_assert (callback_test_optional_boolean);

  g_free (callback_test_optional_string);
  
  g_strfreev (argv);
  g_option_context_free (context);
}

void
callback_test_optional_6 (void)
{
  GOptionContext *context;
  gboolean dummy;
  gboolean retval;
  GError *error = NULL;
  gchar **argv;
  int argc;
  GOptionEntry entries [] =
    { { "dummy", 'd', 0, G_OPTION_ARG_NONE, &dummy, NULL },
      { "test", 't', G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_CALLBACK, 
	callback_parse_optional, NULL, NULL },
      { NULL } };
  
  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Now try parsing */
  argv = split_string ("program -t -d", &argc);
  
  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert (retval);

  g_assert (callback_test_optional_string == NULL);
  
  g_assert (callback_test_optional_boolean);

  g_free (callback_test_optional_string);
  
  g_strfreev (argv);
  g_option_context_free (context);
}

void
callback_test_optional_7 (void)
{
  GOptionContext *context;
  gboolean dummy;
  gboolean retval;
  GError *error = NULL;
  gchar **argv;
  int argc;
  GOptionEntry entries [] =
    { { "dummy", 'd', 0, G_OPTION_ARG_NONE, &dummy, NULL },
      { "test", 't', G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_CALLBACK, 
	callback_parse_optional, NULL, NULL },
      { NULL } };
  
  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Now try parsing */
  argv = split_string ("program -td", &argc);
  
  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert (retval);

  g_assert (callback_test_optional_string == NULL);
  
  g_assert (callback_test_optional_boolean);

  g_free (callback_test_optional_string);
  
  g_strfreev (argv);
  g_option_context_free (context);
}

void
callback_test_optional_8 (void)
{
  GOptionContext *context;
  gboolean dummy;
  gboolean retval;
  GError *error = NULL;
  gchar **argv;
  int argc;
  GOptionEntry entries [] =
    { { "dummy", 'd', 0, G_OPTION_ARG_NONE, &dummy, NULL },
      { "test", 't', G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_CALLBACK, 
	callback_parse_optional, NULL, NULL },
      { NULL } };
  
  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Now try parsing */
  argv = split_string ("program -dt foo.txt", &argc);
  
  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert (retval);

  g_assert (callback_test_optional_string);
  
  g_assert (callback_test_optional_boolean);

  g_free (callback_test_optional_string);
  
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
ignore_test3 (void)
{
  GOptionContext *context;
  gboolean retval;
  GError *error = NULL;
  gchar **argv, **argv_copy;
  int argc;
  gchar *arg;
  GOptionEntry entries [] =
    { { "test", 0, 0, G_OPTION_ARG_STRING, &ignore_test3_string, NULL, NULL },
      { NULL } };

  context = g_option_context_new (NULL);
  g_option_context_set_ignore_unknown_options (context, TRUE);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Now try parsing */
  argv = split_string ("program --test foo --hello", &argc);
  argv_copy = copy_stringv (argv, argc);
  
  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert (retval);

  /* Check array */
  arg = join_stringv (argc, argv);
  g_assert (strcmp (arg, "program --hello") == 0);

  g_assert (strcmp (ignore_test3_string, "foo") == 0);
  g_free (ignore_test3_string);

  g_free (arg);
  g_strfreev (argv_copy);
  g_free (argv);
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

void
empty_test1 (void)
{
  GOptionContext *context;
  GOptionEntry entries [] =
    { { NULL } };

  context = g_option_context_new (NULL);

  g_option_context_add_main_entries (context, entries, NULL);
  
  g_option_context_parse (context, NULL, NULL, NULL);

  g_assert (strcmp (g_get_prgname (), "<unknown>") == 0);
  
  g_option_context_free (context);
}

void
empty_test2 (void)
{
  GOptionContext *context;

  context = g_option_context_new (NULL);
  g_option_context_parse (context, NULL, NULL, NULL);
  
  g_option_context_free (context);
}

void
empty_test3 (void)
{
  GOptionContext *context;
  gint argc;
  gchar **argv;

  argc = 0;
  argv = NULL;

  context = g_option_context_new (NULL);
  g_option_context_parse (context, &argc, &argv, NULL);
  
  g_option_context_free (context);
}

/* check that non-option arguments are left in argv by default */
void
rest_test1 (void)
{
  GOptionContext *context;
  gboolean retval;
  GError *error = NULL;
  gchar **argv;
  int argc;
  GOptionEntry entries [] = { 
      { "test", 0, 0, G_OPTION_ARG_NONE, &ignore_test1_boolean, NULL, NULL },
      { NULL } 
  };
        
  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Now try parsing */
  argv = split_string ("program foo --test bar", &argc);
  
  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert (retval);

  /* Check array */
  g_assert (ignore_test1_boolean);
  g_assert (strcmp (argv[0], "program") == 0);
  g_assert (strcmp (argv[1], "foo") == 0);
  g_assert (strcmp (argv[2], "bar") == 0);
  g_assert (argv[3] == NULL);

  g_strfreev (argv);
  g_option_context_free (context);
}

/* check that -- works */
void
rest_test2 (void)
{
  GOptionContext *context;
  gboolean retval;
  GError *error = NULL;
  gchar **argv;
  int argc;
  GOptionEntry entries [] = { 
      { "test", 0, 0, G_OPTION_ARG_NONE, &ignore_test1_boolean, NULL, NULL },
      { NULL } 
  };
        
  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Now try parsing */
  argv = split_string ("program foo --test -- -bar", &argc);
  
  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert (retval);

  /* Check array */
  g_assert (ignore_test1_boolean);
  g_assert (strcmp (argv[0], "program") == 0);
  g_assert (strcmp (argv[1], "foo") == 0);
  g_assert (strcmp (argv[2], "--") == 0);
  g_assert (strcmp (argv[3], "-bar") == 0);
  g_assert (argv[4] == NULL);

  g_strfreev (argv);
  g_option_context_free (context);
}

/* check that -- stripping works */
void
rest_test2a (void)
{
  GOptionContext *context;
  gboolean retval;
  GError *error = NULL;
  gchar **argv;
  int argc;
  GOptionEntry entries [] = { 
      { "test", 0, 0, G_OPTION_ARG_NONE, &ignore_test1_boolean, NULL, NULL },
      { NULL } 
  };
        
  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Now try parsing */
  argv = split_string ("program foo --test -- bar", &argc);
  
  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert (retval);

  /* Check array */
  g_assert (ignore_test1_boolean);
  g_assert (strcmp (argv[0], "program") == 0);
  g_assert (strcmp (argv[1], "foo") == 0);
  g_assert (strcmp (argv[2], "bar") == 0);
  g_assert (argv[3] == NULL);

  g_strfreev (argv);
  g_option_context_free (context);
}

void
rest_test2b (void)
{
  GOptionContext *context;
  gboolean retval;
  GError *error = NULL;
  gchar **argv;
  int argc;
  GOptionEntry entries [] = { 
      { "test", 0, 0, G_OPTION_ARG_NONE, &ignore_test1_boolean, NULL, NULL },
      { NULL } 
  };
        
  context = g_option_context_new (NULL);
  g_option_context_set_ignore_unknown_options (context, TRUE);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Now try parsing */
  argv = split_string ("program foo --test -bar --", &argc);
  
  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert (retval);

  /* Check array */
  g_assert (ignore_test1_boolean);
  g_assert (strcmp (argv[0], "program") == 0);
  g_assert (strcmp (argv[1], "foo") == 0);
  g_assert (strcmp (argv[2], "-bar") == 0);
  g_assert (argv[3] == NULL);

  g_strfreev (argv);
  g_option_context_free (context);
}

void
rest_test2c (void)
{
  GOptionContext *context;
  gboolean retval;
  GError *error = NULL;
  gchar **argv;
  int argc;
  GOptionEntry entries [] = { 
      { "test", 0, 0, G_OPTION_ARG_NONE, &ignore_test1_boolean, NULL, NULL },
      { NULL } 
  };
        
  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Now try parsing */
  argv = split_string ("program --test foo -- bar", &argc);
  
  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert (retval);

  /* Check array */
  g_assert (ignore_test1_boolean);
  g_assert (strcmp (argv[0], "program") == 0);
  g_assert (strcmp (argv[1], "foo") == 0);
  g_assert (strcmp (argv[2], "bar") == 0);
  g_assert (argv[3] == NULL);

  g_strfreev (argv);
  g_option_context_free (context);
}

void
rest_test2d (void)
{
  GOptionContext *context;
  gboolean retval;
  GError *error = NULL;
  gchar **argv;
  int argc;
  GOptionEntry entries [] = { 
      { "test", 0, 0, G_OPTION_ARG_NONE, &ignore_test1_boolean, NULL, NULL },
      { NULL } 
  };
        
  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Now try parsing */
  argv = split_string ("program --test -- -bar", &argc);
  
  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert (retval);

  /* Check array */
  g_assert (ignore_test1_boolean);
  g_assert (strcmp (argv[0], "program") == 0);
  g_assert (strcmp (argv[1], "--") == 0);
  g_assert (strcmp (argv[2], "-bar") == 0);
  g_assert (argv[3] == NULL);

  g_strfreev (argv);
  g_option_context_free (context);
}


/* check that G_OPTION_REMAINING collects non-option arguments */
void
rest_test3 (void)
{
  GOptionContext *context;
  gboolean retval;
  GError *error = NULL;
  gchar **argv;
  int argc;
  GOptionEntry entries [] = { 
      { "test", 0, 0, G_OPTION_ARG_NONE, &ignore_test1_boolean, NULL, NULL },
      { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &array_test1_array, NULL, NULL },
      { NULL } 
  };
        
  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Now try parsing */
  argv = split_string ("program foo --test bar", &argc);
  
  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert (retval);

  /* Check array */
  g_assert (ignore_test1_boolean);
  g_assert (strcmp (array_test1_array[0], "foo") == 0);
  g_assert (strcmp (array_test1_array[1], "bar") == 0);
  g_assert (array_test1_array[2] == NULL);

  g_strfreev (array_test1_array);
  
  g_strfreev (argv);
  g_option_context_free (context);
}


/* check that G_OPTION_REMAINING and -- work together */
void
rest_test4 (void)
{
  GOptionContext *context;
  gboolean retval;
  GError *error = NULL;
  gchar **argv;
  int argc;
  GOptionEntry entries [] = { 
      { "test", 0, 0, G_OPTION_ARG_NONE, &ignore_test1_boolean, NULL, NULL },
      { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &array_test1_array, NULL, NULL },
      { NULL } 
  };
        
  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Now try parsing */
  argv = split_string ("program foo --test -- -bar", &argc);
  
  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert (retval);

  /* Check array */
  g_assert (ignore_test1_boolean);
  g_assert (strcmp (array_test1_array[0], "foo") == 0);
  g_assert (strcmp (array_test1_array[1], "-bar") == 0);
  g_assert (array_test1_array[2] == NULL);

  g_strfreev (array_test1_array);
  
  g_strfreev (argv);
  g_option_context_free (context);
}

/* test that G_OPTION_REMAINING works with G_OPTION_ARG_FILENAME_ARRAY */
void
rest_test5 (void)
{
  GOptionContext *context;
  gboolean retval;
  GError *error = NULL;
  gchar **argv;
  int argc;
  GOptionEntry entries [] = { 
      { "test", 0, 0, G_OPTION_ARG_NONE, &ignore_test1_boolean, NULL, NULL },
      { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &array_test1_array, NULL, NULL },
      { NULL } 
  };
        
  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Now try parsing */
  argv = split_string ("program foo --test bar", &argc);
  
  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert (retval);

  /* Check array */
  g_assert (ignore_test1_boolean);
  g_assert (strcmp (array_test1_array[0], "foo") == 0);
  g_assert (strcmp (array_test1_array[1], "bar") == 0);
  g_assert (array_test1_array[2] == NULL);

  g_strfreev (array_test1_array);
  
  g_strfreev (argv);
  g_option_context_free (context);
}

void
unknown_short_test (void)
{
  GOptionContext *context;
  gboolean retval;
  GError *error = NULL;
  gchar **argv;
  int argc;
  GOptionEntry entries [] = { { NULL } };

  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Now try parsing */
  argv = split_string ("program -0", &argc);

  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert (!retval);

  g_strfreev (argv);
  g_option_context_free (context);
}

/* test that lone dashes are treated as non-options */
void lonely_dash_test (void)
{
  GOptionContext *context;
  gboolean retval;
  GError *error = NULL;
  gchar **argv;
  int argc;

  context = g_option_context_new (NULL);

  /* Now try parsing */
  argv = split_string ("program -", &argc);

  retval = g_option_context_parse (context, &argc, &argv, &error);

  g_assert (retval);

  g_assert (argv[1] && strcmp (argv[1], "-") == 0);

  g_strfreev (argv);
  g_option_context_free (context);
}

void
missing_arg_test (void)
{
  GOptionContext *context;
  gboolean retval;
  GError *error = NULL;
  gchar **argv;
  int argc;
  gchar *arg = NULL;
  GOptionEntry entries [] =
    { { "test", 't', 0, G_OPTION_ARG_STRING, &arg, NULL, NULL },
      { NULL } };

  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);

  /* Now try parsing */
  argv = split_string ("program --test", &argc);

  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert (retval == FALSE);
  g_clear_error (&error);

  g_strfreev (argv);

  /* Try parsing again */
  argv = split_string ("program --t", &argc);

  retval = g_option_context_parse (context, &argc, &argv, &error);
  g_assert (retval == FALSE);

  g_strfreev (argv);
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
  arg_test3 ();

  /* Test string arrays */
  array_test1 ();

  /* Test callback args */
  callback_test1 ();
  callback_test2 ();

  /* Test optional arg flag for callback */
  callback_test_optional_1 ();
  callback_test_optional_2 ();
  callback_test_optional_3 ();
  callback_test_optional_4 ();
  callback_test_optional_5 ();
  callback_test_optional_6 ();
  callback_test_optional_7 ();
  callback_test_optional_8 ();
  
  /* Test ignoring options */
  ignore_test1 ();
  ignore_test2 ();
  ignore_test3 ();

  add_test1 ();

  /* Test parsing empty args */
  empty_test1 ();
  empty_test2 ();
  empty_test3 ();

  /* Test handling of rest args */
  rest_test1 ();
  rest_test2 ();
  rest_test2a ();
  rest_test2b ();
  rest_test2c ();
  rest_test2d ();
  rest_test3 ();
  rest_test4 ();
  rest_test5 ();

  /* test for bug 166609 */
  unknown_short_test ();

  /* test for bug 168008 */
  lonely_dash_test ();

  /* test for bug 305576 */
  missing_arg_test ();

  return 0;
}
