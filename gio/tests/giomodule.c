/* Unit tests for GIOModule
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

#ifdef _MSC_VER
# define MODULE_FILENAME_PREFIX ""
#else
# define MODULE_FILENAME_PREFIX "lib"
#endif

static void
test_module_scan_all (void)
{
  if (g_test_subprocess ())
    {
      GIOExtensionPoint *ep;
      GIOExtension *ext;
      GList *list;
      ep = g_io_extension_point_register ("test-extension-point");
      g_io_modules_scan_all_in_directory (g_test_get_filename (G_TEST_BUILT, "modules", NULL));
      list = g_io_extension_point_get_extensions (ep);
      g_assert_cmpint (g_list_length (list), ==, 2);
      ext = list->data;
      g_assert_cmpstr (g_io_extension_get_name (ext), ==, "test-b");
      ext = list->next->data;
      g_assert_cmpstr (g_io_extension_get_name (ext), ==, "test-a");
      return;
    }
  g_test_trap_subprocess (NULL, 0, 7);
  g_test_trap_assert_passed ();
}

static void
test_module_scan_all_with_scope (void)
{
  if (g_test_subprocess ())
    {
      GIOExtensionPoint *ep;
      GIOModuleScope *scope;
      GIOExtension *ext;
      GList *list;

      ep = g_io_extension_point_register ("test-extension-point");
      scope = g_io_module_scope_new (G_IO_MODULE_SCOPE_BLOCK_DUPLICATES);
      g_io_module_scope_block (scope, MODULE_FILENAME_PREFIX "testmoduleb." G_MODULE_SUFFIX);
      g_io_modules_scan_all_in_directory_with_scope (g_test_get_filename (G_TEST_BUILT, "modules", NULL), scope);
      list = g_io_extension_point_get_extensions (ep);
      g_assert_cmpint (g_list_length (list), ==, 1);
      ext = list->data;
      g_assert_cmpstr (g_io_extension_get_name (ext), ==, "test-a");
      g_io_module_scope_free (scope);
      return;
    }
  g_test_trap_subprocess (NULL, 0, 7);
  g_test_trap_assert_passed ();
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/giomodule/module-scan-all", test_module_scan_all);
  g_test_add_func ("/giomodule/module-scan-all-with-scope", test_module_scan_all_with_scope);

  return g_test_run ();
}
