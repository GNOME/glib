/* Unit tests for GIOExtensionPoint
 * Copyright (C) 2013 Red Hat, Inc
 * Author: Matthias Clasen
 *
 * This work is provided "as is"; redistribution and modification
 * in whole or in part, in any medium, physical or electronic is
 * permitted without restriction.
 *
 * This work is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * In no event shall the authors or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 */

#include <gio/gio.h>

static void
test_extension_point_basic (void)
{
  GIOExtensionPoint *ep, *ep2;
  GIOExtension *ext;
  GList *list;
  GType req;
  GTypeClass *class;

  ep = g_io_extension_point_lookup ("test-extension-point");
  g_assert_null (ep);
  ep = g_io_extension_point_register ("test-extension-point");
  ep2 = g_io_extension_point_lookup ("test-extension-point");
  g_assert (ep2 == ep);

  req = g_io_extension_point_get_required_type (ep);
  g_assert (req == G_TYPE_INVALID);
  g_io_extension_point_set_required_type (ep, G_TYPE_OBJECT);
  req = g_io_extension_point_get_required_type (ep);
  g_assert (req == G_TYPE_OBJECT);

  list = g_io_extension_point_get_extensions (ep);
  g_assert_null (list);

  g_io_extension_point_implement ("test-extension-point",
                                  G_TYPE_VFS,
                                  "extension1",
                                  10);

  g_io_extension_point_implement ("test-extension-point",
                                  G_TYPE_OBJECT,
                                  "extension2",
                                  20);

  list = g_io_extension_point_get_extensions (ep);
  g_assert_cmpint (g_list_length (list), ==, 2);

  ext = list->data;
  g_assert_cmpstr (g_io_extension_get_name (ext), ==, "extension2");
  g_assert (g_io_extension_get_type (ext) == G_TYPE_OBJECT);
  g_assert (g_io_extension_get_priority (ext) == 20);
  class = g_io_extension_ref_class (ext);
  g_assert (class == g_type_class_peek (G_TYPE_OBJECT));
  g_type_class_unref (class);

  ext = list->next->data;
  g_assert_cmpstr (g_io_extension_get_name (ext), ==, "extension1");
  g_assert (g_io_extension_get_type (ext) == G_TYPE_VFS);
  g_assert (g_io_extension_get_priority (ext) == 10);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/extension-point/basic", test_extension_point_basic);
  return g_test_run ();
}
