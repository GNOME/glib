#include "config.h"

#include <gio/gio.h>

#ifdef HAVE_LINUX_VM_SOCKETS_H
#include <linux/vm_sockets.h>
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

static void
test_vsock_socket_address_construct (void)
{
  GSocketAddress *addr = NULL;
  GVsockSocketAddress *vaddr;

  addr = g_vsock_socket_address_new_with_flags (G_VSOCK_SOCKET_ADDRESS_CID_HOST,
                                                1234,
                                                7);
  vaddr = G_VSOCK_SOCKET_ADDRESS (addr);

  g_assert_cmpint (g_socket_address_get_family (addr), ==, G_SOCKET_FAMILY_VSOCK);
  g_assert_cmpuint (g_vsock_socket_address_get_cid (vaddr), ==,
                    G_VSOCK_SOCKET_ADDRESS_CID_HOST);
  g_assert_cmpuint (g_vsock_socket_address_get_port (vaddr), ==, 1234);
  g_assert_cmpuint (g_vsock_socket_address_get_flags (vaddr), ==, 7);

  g_object_unref (addr);
}

static void
test_vsock_socket_address_to_string (void)
{
  GSocketAddress *addr = NULL;
  char *str = NULL;

  addr = g_vsock_socket_address_new (G_VSOCK_SOCKET_ADDRESS_CID_HOST, 1234);
  str = g_socket_connectable_to_string (G_SOCKET_CONNECTABLE (addr));

  g_assert_cmpstr (str, ==, "2:1234");

  g_free (str);
  g_object_unref (addr);
}

static void
test_vsock_socket_address_native (void)
{
  GSocketAddress *addr = NULL;
  GSocketAddress *roundtrip = NULL;
  GError *error = NULL;

#ifdef HAVE_LINUX_VM_SOCKETS_H
  struct sockaddr_vm native;

  addr = g_vsock_socket_address_new_with_flags (G_VSOCK_SOCKET_ADDRESS_CID_HOST,
                                                1234,
                                                7);
  g_assert_cmpint (g_socket_address_get_native_size (addr), ==, sizeof (native));
  g_assert_true (g_socket_address_to_native (addr, &native, sizeof (native), &error));
  g_assert_no_error (error);
  g_assert_cmpint (native.svm_family, ==, AF_VSOCK);
  g_assert_cmpuint (native.svm_cid, ==, G_VSOCK_SOCKET_ADDRESS_CID_HOST);
  g_assert_cmpuint (native.svm_port, ==, 1234);
  g_assert_cmpuint (native.svm_flags, ==, 7);

  roundtrip = g_socket_address_new_from_native (&native, sizeof (native));
  g_assert_true (G_IS_VSOCK_SOCKET_ADDRESS (roundtrip));
  g_assert_cmpuint (g_vsock_socket_address_get_cid (G_VSOCK_SOCKET_ADDRESS (roundtrip)),
                    ==, G_VSOCK_SOCKET_ADDRESS_CID_HOST);
  g_assert_cmpuint (g_vsock_socket_address_get_port (G_VSOCK_SOCKET_ADDRESS (roundtrip)),
                    ==, 1234);
  g_assert_cmpuint (g_vsock_socket_address_get_flags (G_VSOCK_SOCKET_ADDRESS (roundtrip)),
                    ==, 7);

  g_object_unref (roundtrip);
  g_object_unref (addr);
#else
  addr = g_vsock_socket_address_new (G_VSOCK_SOCKET_ADDRESS_CID_HOST, 1234);
  g_assert_cmpint (g_socket_address_get_native_size (addr), ==, -1);
  g_assert_false (g_socket_address_to_native (addr, NULL, 0, &error));
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED);
  g_clear_error (&error);
  g_object_unref (addr);
#endif
}

int
main (int    argc,
      char **argv)
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/socket/address/unix/construct", test_unix_socket_address_construct);
  g_test_add_func ("/socket/address/unix/to-string", test_unix_socket_address_to_string);
  g_test_add_func ("/socket/address/vsock/construct", test_vsock_socket_address_construct);
  g_test_add_func ("/socket/address/vsock/to-string", test_vsock_socket_address_to_string);
  g_test_add_func ("/socket/address/vsock/native", test_vsock_socket_address_native);

  return g_test_run ();
}
