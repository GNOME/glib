/* constructor.c - Test for constructors
 *
 * Copyright © 2023 Luca Bacci
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include <glib.h>
#include "../gconstructorprivate.h"

#ifndef _WIN32
#include <dlfcn.h>
#else
#include <windows.h>
#endif

#define LIB_PREFIX "lib_"
#define APP_PREFIX "app_"

#ifdef BUILD_LIBRARY
#define PREFIX LIB_PREFIX
#else
#define PREFIX APP_PREFIX
#endif

#if G_HAS_CONSTRUCTORS

#ifdef G_DEFINE_CONSTRUCTOR_NEEDS_PRAGMA
#pragma G_DEFINE_CONSTRUCTOR_PRAGMA_ARGS (ctor)
#endif

G_DEFINE_CONSTRUCTOR (ctor)

static void
ctor (void)
{
  const char *string = PREFIX "ctor";

  g_assert_null (g_getenv (string));
  g_setenv (string, "1", FALSE);
}

#ifdef G_DEFINE_DESTRUCTOR_NEEDS_PRAGMA
#pragma G_DEFINE_DESTRUCTOR_PRAGMA_ARGS (dtor)
#endif

G_DEFINE_DESTRUCTOR (dtor)

static void
dtor (void)
{
  const char *string = PREFIX "dtor";

  g_assert_null (g_getenv (string));
  g_setenv (string, "1", FALSE);
}

#endif /* G_HAS_CONSTRUCTORS */

#if defined (_WIN32) && defined (G_HAS_TLS_CALLBACKS)

extern IMAGE_DOS_HEADER __ImageBase;
static inline HMODULE
this_module (void)
{
  return (HMODULE) &__ImageBase;
}

G_DEFINE_TLS_CALLBACK (tls_callback)

static void NTAPI
tls_callback (PVOID     hInstance,
              DWORD     dwReason,
              LPVOID    lpvReserved)
{
  /* The HINSTANCE we get must match the address of __ImageBase */
  g_assert_true (hInstance == this_module ());

#ifndef BUILD_LIBRARY
  /* Yes, we can call GetModuleHandle (NULL) with the loader lock */
  g_assert_true (hInstance == GetModuleHandle (NULL));
#endif

  switch (dwReason)
    {
    case DLL_PROCESS_ATTACH:
      {
        const char *string = PREFIX "tls_cb_process_attach";

#ifdef BUILD_LIBRARY
        /* the library is explicitly loaded */
        g_assert_null (lpvReserved);
#endif

        g_assert_null (g_getenv (string));
        g_setenv (string, "1", FALSE);
      }
      break;

    case DLL_THREAD_ATTACH:
      break;

    case DLL_THREAD_DETACH:
      break;

    case DLL_PROCESS_DETACH:
#ifdef BUILD_LIBRARY
      {
        const char *string = PREFIX "tls_cb_process_detach";

        /* the library is explicitly unloaded */
        g_assert_null (lpvReserved);

        g_assert_null (g_getenv (string));
        g_setenv (string, "1", FALSE);
      }
#endif
      break;

    default:
      g_assert_not_reached ();
      break;
    }
}

#endif /* _WIN32 && G_HAS_TLS_CALLBACKS */

#ifndef BUILD_LIBRARY

void *library;

static void
load_library (const char *library_path)
{
#ifndef _WIN32
  library = dlopen (library_path, RTLD_LAZY);
  if (!library)
    g_error ("%s (%s) failed: %s", "dlopen", library_path, dlerror ());
#else
  wchar_t *library_path_u16 = g_utf8_to_utf16 (library_path, -1, NULL, NULL, NULL);
  g_assert_nonnull (library_path_u16);

  library = LoadLibraryW (library_path_u16);
  if (!library)
    g_error ("%s (%s) failed with error code %u",
             "FreeLibrary", library_path, (unsigned int) GetLastError ());

  g_free (library_path_u16);
#endif
}

static void
unload_library (void)
{
#ifndef _WIN32
  if (dlclose (library) != 0)
    g_error ("%s failed: %s", "dlclose", dlerror ());
#else
  if (!FreeLibrary (library))
    g_error ("%s failed with error code %u",
             "FreeLibrary", (unsigned int) GetLastError ());
#endif
}

static void
test_app (void)
{
#if G_HAS_CONSTRUCTORS
  g_assert_cmpstr (g_getenv (APP_PREFIX "ctor"), ==, "1");
#endif
#if defined (_WIN32) && defined (G_HAS_TLS_CALLBACKS)
  g_assert_cmpstr (g_getenv (APP_PREFIX "tls_cb_process_attach"), ==, "1");
#endif
}

static void
test_lib (gconstpointer data)
{
  const char *library_path = (const char*) data;

  /* Check that the environment is clean */
#if G_HAS_CONSTRUCTORS
  g_assert_null (g_getenv (LIB_PREFIX "ctor"));
#endif
#if G_HAS_DESTRUCTORS
  g_assert_null (g_getenv (LIB_PREFIX "dtor"));
#endif
#if defined (_WIN32) && defined (G_HAS_TLS_CALLBACKS)
  g_assert_null (g_getenv (LIB_PREFIX "tls_cb_process_attach"));
  g_assert_null (g_getenv (LIB_PREFIX "tls_cb_process_detach"));
#endif

  /* Constructors */
  load_library (library_path);

#if G_HAS_CONSTRUCTORS
  g_assert_cmpstr (g_getenv (LIB_PREFIX "ctor"), ==, "1");
#endif
#if defined (_WIN32) && defined (G_HAS_TLS_CALLBACKS)
  g_assert_cmpstr (g_getenv (LIB_PREFIX "tls_cb_process_attach"), ==, "1");
#endif

  /* Destructors */
  unload_library ();

#if G_HAS_DESTRUCTORS
  g_assert_cmpstr (g_getenv (LIB_PREFIX "dtor"), ==, "1");
#endif
#if defined (_WIN32) && defined (G_HAS_TLS_CALLBACKS)
  g_assert_cmpstr (g_getenv (LIB_PREFIX "tls_cb_process_detach"), ==, "1");
#endif
}

int
main (int argc, char *argv[])
{

  const char *libname = G_STRINGIFY (LIB_NAME);
  const char *builddir;
  char *path;
  int ret;

  g_assert_nonnull ((builddir = g_getenv ("G_TEST_BUILDDIR")));

  path = g_build_filename (builddir, libname, NULL);

  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/constructor/application", test_app);
  g_test_add_data_func ("/constructor/library", path, test_lib);

  ret = g_test_run ();

  g_free (path);
  return ret;
}

#endif /* !BUILD_LIBRARY */
