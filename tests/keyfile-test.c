#include <glib.h>
#include <string.h>
#include <stdlib.h>

static GKeyFile *
load_data (const gchar *data)
{
  GKeyFile *keyfile;
  GError *error = NULL;

  keyfile = g_key_file_new ();
  g_key_file_load_from_data (keyfile, data, -1, 0, &error);
  if (error)
    {
      g_print ("Could not load data: %s\n", error->message);
      exit (1);
    }
  
  return keyfile;
}

static void
check_string_value (GKeyFile    *keyfile,
		    const gchar *group,
		    const gchar *key,
		    const gchar *expected) 
{
  GError *error = NULL;
  gchar *value;

  value = g_key_file_get_string (keyfile, group, key, &error);
  if (error) 
    {
      g_print ("Group %s key %s: %s\n", group, key, error->message);      
      exit (1);
    }

  g_assert (value != NULL);

  if (strcmp (value, expected) != 0)
    {
      g_print ("Group %s key %s: "
	       "expected value '%s', actual value '%s'\n",
	       group, key, expected, value);      
      exit (1);
    }
}

static void
test_line_ends (void)
{
  GKeyFile *keyfile;

  const gchar *data = 
    "[group1]\n"
    "key1=value1\n"
    "key2=value2\r\n"
    "[group2]\r\n"
    "key3=value3\r\r\n"
    "key4=value4\n";

  keyfile = load_data (data);

  check_string_value (keyfile, "group1", "key1", "value1");
  check_string_value (keyfile, "group1", "key2", "value2");
  check_string_value (keyfile, "group2", "key3", "value3\r");
  check_string_value (keyfile, "group2", "key4", "value4");
}

static void
test_whitespace (void)
{
  GKeyFile *keyfile;

  const gchar *data = 
    "[group1]\n"
    "key1 = value1\n"
    "key2\t=\tvalue2\n"
    " [ group2 ] \n"
    "key3  =  value3  \n"
    "key4  =  value \t4\n";
  
  keyfile = load_data (data);

  check_string_value (keyfile, "group1", "key1", "value1");
  check_string_value (keyfile, "group1", "key2", "value2");
  check_string_value (keyfile, " group2 ", "key3", "value3  ");
  check_string_value (keyfile, " group2 ", "key4", "value \t4");
}

int 
main (int argc, char *argv[])
{
  test_line_ends ();
  test_whitespace ();
  
  return 0;
}
