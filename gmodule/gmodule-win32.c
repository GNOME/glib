/* GMODULE - GLIB wrapper code for dynamic module loading
 * Copyright (C) 1998, 2000 Tim Janik
 *
 * Win32 GMODULE implementation
 * Copyright (C) 1998 Tor Lillqvist
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GLib Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/. 
 */

/* 
 * MT safe
 */

#include <stdio.h>
#include <windows.h>

#include <tlhelp32.h>

#ifdef G_WITH_CYGWIN
#include <sys/cygwin.h>
#endif

static void
set_error (void)
{
  gchar *error = g_win32_error_message (GetLastError ());

  g_module_set_error (error);
  g_free (error);
}

/* --- functions --- */
static gpointer
_g_module_open (const gchar *file_name,
		gboolean     bind_lazy,
		gboolean     bind_local)
{
  HINSTANCE handle;
#ifdef G_WITH_CYGWIN
  gchar tmp[MAX_PATH];

  cygwin_conv_to_win32_path(file_name, tmp);
  file_name = tmp;
#endif
  
  handle = LoadLibrary (file_name);
  if (!handle)
    set_error ();

  return handle;
}

static gint dummy;
static gpointer null_module_handle = &dummy;
  
static gpointer
_g_module_self (void)
{
  return null_module_handle;
}

static void
_g_module_close (gpointer handle,
		 gboolean is_unref)
{
  if (handle != null_module_handle)
    if (!FreeLibrary (handle))
      set_error ();
}

static gpointer
find_in_any_module_using_toolhelp (const gchar *symbol_name)
{
  typedef HANDLE (WINAPI *PFNCREATETOOLHELP32SNAPSHOT)(DWORD, DWORD);
  static PFNCREATETOOLHELP32SNAPSHOT pfnCreateToolhelp32Snapshot = NULL;

  typedef BOOL (WINAPI *PFNMODULE32FIRST)(HANDLE, MODULEENTRY32*);
  static PFNMODULE32FIRST pfnModule32First= NULL;

  typedef BOOL (WINAPI *PFNMODULE32NEXT)(HANDLE, MODULEENTRY32*);
  static PFNMODULE32NEXT pfnModule32Next = NULL;

  static HMODULE kernel32;

  HANDLE snapshot; 
  MODULEENTRY32 me32;

  gpointer p;

  if (!pfnCreateToolhelp32Snapshot || !pfnModule32First || !pfnModule32Next)
    {
      if (!kernel32)
	if (!(kernel32 = GetModuleHandle ("kernel32.dll")))
	  return NULL;

      if (!(pfnCreateToolhelp32Snapshot = (PFNCREATETOOLHELP32SNAPSHOT) GetProcAddress (kernel32, "CreateToolhelp32Snapshot"))
	  || !(pfnModule32First = (PFNMODULE32FIRST) GetProcAddress (kernel32, "Module32First"))
	  || !(pfnModule32Next = (PFNMODULE32NEXT) GetProcAddress (kernel32, "Module32Next")))
	return NULL;
    }

  if ((snapshot = (*pfnCreateToolhelp32Snapshot) (TH32CS_SNAPMODULE, 0)) == (HANDLE) -1)
    return NULL;

  me32.dwSize = sizeof (me32);
  p = NULL;
  if ((*pfnModule32First) (snapshot, &me32))
    {
      do {
	if ((p = GetProcAddress (me32.hModule, symbol_name)) != NULL)
	  break;
      } while ((*pfnModule32Next) (snapshot, &me32));
    }

  CloseHandle (snapshot);

  return p;
}

static gpointer
find_in_any_module_using_psapi (const gchar *symbol_name)
{
  static HMODULE psapi = NULL;

  typedef BOOL (WINAPI *PFNENUMPROCESSMODULES) (HANDLE, HMODULE *, DWORD, LPDWORD) ;
  static PFNENUMPROCESSMODULES pfnEnumProcessModules = NULL;

  HMODULE *modules;
  HMODULE dummy;
  gint i, size;
  DWORD needed;
  
  gpointer p;

  if (!pfnEnumProcessModules)
    {
      if (!psapi)
	if ((psapi = LoadLibrary ("psapi.dll")) == NULL)
	  return NULL;

      if (!(pfnEnumProcessModules = (PFNENUMPROCESSMODULES) GetProcAddress (psapi, "EnumProcessModules")))
	return NULL;
    }

  if (!(*pfnEnumProcessModules) (GetCurrentProcess (), &dummy,
				 sizeof (HMODULE), &needed))
    return NULL;

  size = needed + 10 * sizeof (HMODULE);
  modules = g_malloc (size);

  if (!(*pfnEnumProcessModules) (GetCurrentProcess (), modules,
				 size, &needed)
      || needed > size)
    {
      g_free (modules);
      return NULL;
    }
  
  p = NULL;
  for (i = 0; i < needed / sizeof (HMODULE); i++)
    if ((p = GetProcAddress (modules[i], symbol_name)) != NULL)
      break;

  g_free (modules);

  return p;
}

static gpointer
find_in_any_module (const gchar *symbol_name)
{
  gpointer result;

  if ((result = find_in_any_module_using_toolhelp (symbol_name)) == NULL
      && (result = find_in_any_module_using_psapi (symbol_name)) == NULL)
    return NULL;
  else
    return result;
}

static gpointer
_g_module_symbol (gpointer     handle,
		  const gchar *symbol_name)
{
  gpointer p;
  
  if (handle == null_module_handle)
    {
      if ((p = GetProcAddress (GetModuleHandle (NULL), symbol_name)) == NULL)
	p = find_in_any_module (symbol_name);
    }
  else
    p = GetProcAddress (handle, symbol_name);

  if (!p)
    set_error ();

  return p;
}

static gchar*
_g_module_build_path (const gchar *directory,
		      const gchar *module_name)
{
  gint k;

  k = strlen (module_name);
    
  if (directory && *directory)
    if (k > 4 && g_ascii_strcasecmp (module_name + k - 4, ".dll") == 0)
      return g_strconcat (directory, G_DIR_SEPARATOR_S, module_name, NULL);
    else if (strncmp (module_name, "lib", 3) == 0)
      return g_strconcat (directory, G_DIR_SEPARATOR_S, module_name, ".dll", NULL);
    else
      return g_strconcat (directory, G_DIR_SEPARATOR_S, "lib", module_name, ".dll", NULL);
  else if (k > 4 && g_ascii_strcasecmp (module_name + k - 4, ".dll") == 0)
    return g_strdup (module_name);
  else if (strncmp (module_name, "lib", 3) == 0)
    return g_strconcat (module_name, ".dll", NULL);
  else
    return g_strconcat ("lib", module_name, ".dll", NULL);
}
