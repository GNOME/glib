#include <gio/gio.h>

static void
encoder_binary (void)
{
  GEncoder *encoder = g_binary_encoder_new ();
  GError *error = NULL;
  GBytes *buffer;
  gboolean bool_value;
  char *str_value;

  g_object_add_weak_pointer (G_OBJECT (encoder), (gpointer *) &encoder);

  g_encoder_add_key_bool (encoder, "BoolValue", TRUE);
  g_encoder_add_key_string (encoder, "StringValue", "Hello");

  buffer = g_encoder_write_to_bytes (encoder, &error);
  g_assert_no_error (error);
  g_assert (buffer != NULL);

  g_object_unref (encoder);
  g_assert (encoder == NULL);

  if (g_test_verbose ())
    g_print ("*** buffer (len: %d) = ***\n%s\n",
             (int) g_bytes_get_size (buffer),
             (const char *) g_bytes_get_data (buffer, NULL));

  encoder = g_binary_encoder_new ();

  g_object_add_weak_pointer (G_OBJECT (encoder), (gpointer *) &encoder);

  g_encoder_read_from_bytes (encoder, buffer, &error);
  g_assert_no_error (error);

  g_encoder_get_key_bool (encoder, "BoolValue", &bool_value);
  g_assert (bool_value);

  g_encoder_get_key_string (encoder, "StringValue", &str_value);
  g_assert_cmpstr (str_value, ==, "Hello");
  g_free (str_value);

  g_bytes_unref (buffer);

  g_object_unref (encoder);
  g_assert (encoder == NULL);
}

static void
encoder_keyfile (void)
{
  GEncoder *encoder = g_keyfile_encoder_new ();
  GError *error = NULL;
  GBytes *buffer;
  gboolean res;

  g_keyfile_encoder_set_section_name (G_KEYFILE_ENCODER (encoder), "Test");
  g_encoder_add_key_bool (encoder, "BoolValue", TRUE);

  buffer = g_encoder_write_to_bytes (encoder, &error);
  g_assert_no_error (error);
  g_assert (buffer != NULL);

  g_object_unref (encoder);

  if (g_test_verbose ())
    g_print ("*** buffer (len: %d) = ***\n%s",
             (int) g_bytes_get_size (buffer),
             (const char *) g_bytes_get_data (buffer, NULL));

  encoder = g_keyfile_encoder_new ();
  g_keyfile_encoder_set_section_name (G_KEYFILE_ENCODER (encoder), "Test");

  g_encoder_read_from_bytes (encoder, buffer, &error);
  g_assert_no_error (error);

  g_encoder_get_key_bool (encoder, "BoolValue", &res);
  g_assert (res);

  g_bytes_unref (buffer);
  g_object_unref (encoder);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/encoder/binary", encoder_binary);
  g_test_add_func ("/encoder/key-file", encoder_keyfile);

  return g_test_run ();
}
