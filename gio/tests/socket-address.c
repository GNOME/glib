#include <gio/gunixsocketaddress.h>

#include <sys/socket.h>
#include <sys/un.h>

#ifndef UNIX_PATH_MAX
#define UNIX_PATH_MAX G_SIZEOF_MEMBER (struct sockaddr_un, sun_path)
#endif

static void
test_unix_socket_address_construct (void)
{
  GUnixSocketAddress *a;

  a = g_object_new (G_TYPE_UNIX_SOCKET_ADDRESS, NULL);
  g_assert_cmpint (g_unix_socket_address_get_address_type (a), ==, G_UNIX_SOCKET_ADDRESS_PATH);
  g_object_unref (a);

  /* Try passing some default values for the arguments explicitly and
   * make sure it makes no difference.
   */
  a = g_object_new (G_TYPE_UNIX_SOCKET_ADDRESS, "address-type", G_UNIX_SOCKET_ADDRESS_PATH, NULL);
  g_assert_cmpint (g_unix_socket_address_get_address_type (a), ==, G_UNIX_SOCKET_ADDRESS_PATH);
  g_object_unref (a);

  a = g_object_new (G_TYPE_UNIX_SOCKET_ADDRESS, "abstract", FALSE, NULL);
  g_assert_cmpint (g_unix_socket_address_get_address_type (a), ==, G_UNIX_SOCKET_ADDRESS_PATH);
  g_object_unref (a);

  a = g_object_new (G_TYPE_UNIX_SOCKET_ADDRESS,
                    "abstract", FALSE,
                    "address-type", G_UNIX_SOCKET_ADDRESS_PATH,
                    NULL);
  g_assert_cmpint (g_unix_socket_address_get_address_type (a), ==, G_UNIX_SOCKET_ADDRESS_PATH);
  g_object_unref (a);

  a = g_object_new (G_TYPE_UNIX_SOCKET_ADDRESS,
                    "address-type", G_UNIX_SOCKET_ADDRESS_PATH,
                    "abstract", FALSE,
                    NULL);
  g_assert_cmpint (g_unix_socket_address_get_address_type (a), ==, G_UNIX_SOCKET_ADDRESS_PATH);
  g_object_unref (a);

  /* Try explicitly setting abstract to TRUE */
  a = g_object_new (G_TYPE_UNIX_SOCKET_ADDRESS,
                    "abstract", TRUE,
                    NULL);
  g_assert_cmpint (g_unix_socket_address_get_address_type (a), ==, G_UNIX_SOCKET_ADDRESS_ABSTRACT_PADDED);
  g_object_unref (a);

  /* Try explicitly setting a different kind of address */
  a = g_object_new (G_TYPE_UNIX_SOCKET_ADDRESS,
                    "address-type", G_UNIX_SOCKET_ADDRESS_ANONYMOUS,
                    NULL);
  g_assert_cmpint (g_unix_socket_address_get_address_type (a), ==, G_UNIX_SOCKET_ADDRESS_ANONYMOUS);
  g_object_unref (a);

  /* Now try explicitly setting a different type of address after
   * setting abstract to FALSE.
   */
  a = g_object_new (G_TYPE_UNIX_SOCKET_ADDRESS,
                    "abstract", FALSE,
                    "address-type", G_UNIX_SOCKET_ADDRESS_ANONYMOUS,
                    NULL);
  g_assert_cmpint (g_unix_socket_address_get_address_type (a), ==, G_UNIX_SOCKET_ADDRESS_ANONYMOUS);
  g_object_unref (a);

  /* And the other way around */
  a = g_object_new (G_TYPE_UNIX_SOCKET_ADDRESS,
                    "address-type", G_UNIX_SOCKET_ADDRESS_ANONYMOUS,
                    "abstract", FALSE,
                    NULL);
  g_assert_cmpint (g_unix_socket_address_get_address_type (a), ==, G_UNIX_SOCKET_ADDRESS_ANONYMOUS);
  g_object_unref (a);
}

static void
test_unix_socket_address_construct_path (void)
{
  struct {
    GUnixSocketAddressType address_type;
    gsize max_len;
  } sizes[] = {
    { G_UNIX_SOCKET_ADDRESS_ANONYMOUS, 0 },
    { G_UNIX_SOCKET_ADDRESS_PATH, UNIX_PATH_MAX },
    { G_UNIX_SOCKET_ADDRESS_ABSTRACT, UNIX_PATH_MAX - 1 },
    { G_UNIX_SOCKET_ADDRESS_ABSTRACT_PADDED, UNIX_PATH_MAX - 1 },
  };
  gsize lengths[] = { 0, 1, 2, 4, 8, 15, 16, 32, 64, 128, 256,
    UNIX_PATH_MAX - 2, UNIX_PATH_MAX - 1, UNIX_PATH_MAX, UNIX_PATH_MAX + 1, UNIX_PATH_MAX + 2 };
  GByteArray *array;
  GSocketAddress *a[4];
  gsize i, j, k, l, path_len[4];
  const char *path[4];

  array = g_byte_array_new ();

  for (i = 0; i < G_N_ELEMENTS (sizes); i++)
    {
      for (j = 0; j < G_N_ELEMENTS (lengths); j++)
        {
          g_byte_array_set_size (array, lengths[j]);

          for (k = 0; k < G_N_ELEMENTS (lengths); k++)
            {
              gsize stringlen = lengths[k];

              if (stringlen > array->len)
                continue;

              if (stringlen > 0)
                memset (array->data, 'x', stringlen);
              if (stringlen < array->len)
                memset (array->data + stringlen, 0, array->len - stringlen);

              if (stringlen < array->len)
                {
                  a[0] = g_object_new (G_TYPE_UNIX_SOCKET_ADDRESS,
                                       "address-type", sizes[i].address_type,
                                       "path", array->len > 0 ? (char *) array->data : "",
                                       NULL);
                  a[1] = g_object_new (G_TYPE_UNIX_SOCKET_ADDRESS,
                                       "path", array->len > 0 ? (char *) array->data : "",
                                       "address-type", sizes[i].address_type,
                                       NULL);
                }
              else
                {
                  a[0] = NULL;
                  a[1] = NULL;
                }
              a[2] = g_object_new (G_TYPE_UNIX_SOCKET_ADDRESS,
                                   "address-type", sizes[i].address_type,
                                   "path-as-array", array,
                                   NULL);
              a[3] = g_object_new (G_TYPE_UNIX_SOCKET_ADDRESS,
                                   "path-as-array", array,
                                   "address-type", sizes[i].address_type,
                                   NULL);

              for (l = 0; l < G_N_ELEMENTS (a); l++)
                {
                  if (a[l] == NULL)
                    continue;

                  path[l] = g_unix_socket_address_get_path (G_UNIX_SOCKET_ADDRESS (a[l]));
                  path_len[l] = g_unix_socket_address_get_path_len (G_UNIX_SOCKET_ADDRESS (a[l]));
                  if (l < 2)
                    {
                      /* used path */
                      g_assert_cmpuint (path_len[l], ==, MIN (sizes[i].max_len, stringlen));
                    }
                  else
                    {
                      /* used path-as-array */
                      g_assert_cmpuint (path_len[l], ==, MIN (sizes[i].max_len, array->len));
                    }
                  g_assert_cmpuint (strlen (path[l]), <=, MIN (path_len[l], stringlen));
                  /* because there's no g_asert_cmpstrn() */
                  g_assert_cmpint (strncmp (path[l], (char *) array->data, path_len[l]), ==, 0);

                  g_object_unref (a[l]);
                }
            }
        }
    }

  g_byte_array_unref (array);
}

static void
test_unix_socket_address_to_string (void)
{
  GSocketAddress *addr = NULL;
  gchar *str = NULL;

  /* ADDRESS_PATH. */
  addr = g_unix_socket_address_new_with_type ("/some/path", -1,
                                              G_UNIX_SOCKET_ADDRESS_PATH);
  str = g_socket_connectable_to_string (G_SOCKET_CONNECTABLE (addr));
  g_assert_cmpstr (str, ==, "/some/path");
  g_free (str);
  g_object_unref (addr);

  /* ADDRESS_ANONYMOUS. */
  addr = g_unix_socket_address_new_with_type ("", 0,
                                              G_UNIX_SOCKET_ADDRESS_ANONYMOUS);
  str = g_socket_connectable_to_string (G_SOCKET_CONNECTABLE (addr));
  g_assert_cmpstr (str, ==, "anonymous");
  g_free (str);
  g_object_unref (addr);

  /* ADDRESS_ABSTRACT. */
  addr = g_unix_socket_address_new_with_type ("abstract-path\0✋", 17,
                                              G_UNIX_SOCKET_ADDRESS_ABSTRACT);
  str = g_socket_connectable_to_string (G_SOCKET_CONNECTABLE (addr));
  g_assert_cmpstr (str, ==, "abstract-path\\x00\\xe2\\x9c\\x8b");
  g_free (str);
  g_object_unref (addr);

  /* ADDRESS_ABSTRACT_PADDED. */
  addr = g_unix_socket_address_new_with_type ("abstract-path\0✋", 17,
                                              G_UNIX_SOCKET_ADDRESS_ABSTRACT_PADDED);
  str = g_socket_connectable_to_string (G_SOCKET_CONNECTABLE (addr));
  g_assert_cmpstr (str, ==, "abstract-path\\x00\\xe2\\x9c\\x8b");
  g_free (str);
  g_object_unref (addr);
}

int
main (int    argc,
      char **argv)
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/socket/address/unix/construct", test_unix_socket_address_construct);
  g_test_add_func ("/socket/address/unix/construct-path", test_unix_socket_address_construct_path);
  g_test_add_func ("/socket/address/unix/to-string", test_unix_socket_address_to_string);

  return g_test_run ();
}
