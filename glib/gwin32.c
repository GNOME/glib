/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1998  Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 1998-1999  Tor Lillqvist
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
 * MT safe for the unix part, FIXME: make the win32 part MT safe as well.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "glibconfig.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define STRICT			/* Strict typing, please */
#include <windows.h>
#undef STRICT
#ifndef G_WITH_CYGWIN
#include <direct.h>
#endif
#include <errno.h>
#include <ctype.h>
#ifdef _MSC_VER
#  include <io.h>
#endif /* _MSC_VER */

#include "glib.h"

#ifdef G_WITH_CYGWIN
#include <sys/cygwin.h>
#endif

#ifndef G_WITH_CYGWIN

gint
g_win32_ftruncate (gint  fd,
		   guint size)
{
  HANDLE hfile;
  guint curpos;

  g_return_val_if_fail (fd >= 0, -1);
  
  hfile = (HANDLE) _get_osfhandle (fd);
  curpos = SetFilePointer (hfile, 0, NULL, FILE_CURRENT);
  if (curpos == 0xFFFFFFFF
      || SetFilePointer (hfile, size, NULL, FILE_BEGIN) == 0xFFFFFFFF
      || !SetEndOfFile (hfile))
    {
      gint error = GetLastError ();

      switch (error)
	{
	case ERROR_INVALID_HANDLE:
	  errno = EBADF;
	  break;
	default:
	  errno = EIO;
	  break;
	}

      return -1;
    }

  return 0;
}

#endif

/**
 * g_win32_getlocale:
 *
 * The setlocale in the Microsoft C library uses locale names of the
 * form "English_United States.1252" etc. We want the UNIXish standard
 * form "en_US", "zh_TW" etc. This function gets the current thread
 * locale from Windows - without any encoding info - and returns it as
 * a string of the above form for use in forming file names etc. The
 * returned string should be deallocated with g_free().
 *
 * Returns: newly-allocated locale name.
 **/

gchar *
g_win32_getlocale (void)
{
  LCID lcid;
  LANGID langid;
  gchar *ev;
  gint primary, sub;
  gchar *l = NULL, *sl = NULL;
  gchar bfr[20];

  /* Let the user override the system settings through environment
     variables, as on POSIX systems.  */
  if (((ev = getenv ("LC_ALL")) != NULL && ev[0] != '\0')
      || ((ev = getenv ("LC_MESSAGES")) != NULL && ev[0] != '\0')
      || ((ev = getenv ("LANG")) != NULL && ev[0] != '\0'))
    return g_strdup (ev);

  /* Use native Win32 API locale ID.  */
  lcid = GetThreadLocale ();

  /* Strip off the sorting rules, keep only the language part.  */
  langid = LANGIDFROMLCID (lcid);

  /* Split into language and territory part.  */
  primary = PRIMARYLANGID (langid);
  sub = SUBLANGID (langid);
  switch (primary)
    {
    case LANG_AFRIKAANS: l = "af"; sl = "ZA"; break;
    case LANG_ALBANIAN: l = "sq"; sl = "AL"; break;
    case LANG_ARABIC:
      l = "ar";
      switch (sub)
	{
	case SUBLANG_ARABIC_SAUDI_ARABIA: sl = "SA"; break;
	case SUBLANG_ARABIC_IRAQ: sl = "IQ"; break;
	case SUBLANG_ARABIC_EGYPT: sl = "EG"; break;
	case SUBLANG_ARABIC_LIBYA: sl = "LY"; break;
	case SUBLANG_ARABIC_ALGERIA: sl = "DZ"; break;
	case SUBLANG_ARABIC_MOROCCO: sl = "MA"; break;
	case SUBLANG_ARABIC_TUNISIA: sl = "TN"; break;
	case SUBLANG_ARABIC_OMAN: sl = "OM"; break;
	case SUBLANG_ARABIC_YEMEN: sl = "YE"; break;
	case SUBLANG_ARABIC_SYRIA: sl = "SY"; break;
	case SUBLANG_ARABIC_JORDAN: sl = "JO"; break;
	case SUBLANG_ARABIC_LEBANON: sl = "LB"; break;
	case SUBLANG_ARABIC_KUWAIT: sl = "KW"; break;
	case SUBLANG_ARABIC_UAE: sl = "AE"; break;
	case SUBLANG_ARABIC_BAHRAIN: sl = "BH"; break;
	case SUBLANG_ARABIC_QATAR: sl = "QA"; break;
	}
      break;
#ifdef LANG_ARMENIAN
    case LANG_ARMENIAN: l = "hy"; sl = "AM"; break;
#endif
#ifdef LANG_ASSAMESE
    case LANG_ASSAMESE: l = "as"; sl = "IN"; break;
#endif
#ifdef LANG_AZERI
    case LANG_AZERI:
      l = "az";
#if defined (SUBLANG_AZERI_LATIN) && defined (SUBLANG_AZERI_CYRILLIC)
      switch (sub)
	{
	/* FIXME: Adjust this when Azerbaijani locales appear on Unix.  */
	case SUBLANG_AZERI_LATIN: sl = "@latin"; break;
	case SUBLANG_AZERI_CYRILLIC: sl = "@cyrillic"; break;
	}
#endif
      break;
#endif
    case LANG_BASQUE:
      l = "eu"; /* sl could be "ES" or "FR".  */
      break;
    case LANG_BELARUSIAN: l = "be"; sl = "BY"; break;
#ifdef LANG_BENGALI
    case LANG_BENGALI: l = "bn"; sl = "IN"; break;
#endif
    case LANG_BULGARIAN: l = "bg"; sl = "BG"; break;
    case LANG_CATALAN: l = "ca"; sl = "ES"; break;
    case LANG_CHINESE:
      l = "zh";
      switch (sub)
	{
	case SUBLANG_CHINESE_TRADITIONAL: sl = "TW"; break;
	case SUBLANG_CHINESE_SIMPLIFIED: sl = "CN"; break;
	case SUBLANG_CHINESE_HONGKONG: sl = "HK"; break;
	case SUBLANG_CHINESE_SINGAPORE: sl = "SG"; break;
#ifdef SUBLANG_CHINESE_MACAU
	case SUBLANG_CHINESE_MACAU: sl = "MO"; break;
#endif
	}
      break;
    case LANG_CROATIAN:		/* LANG_CROATIAN == LANG_SERBIAN
				 * What used to be called Serbo-Croatian
				 * should really now be two separate
				 * languages because of political reasons.
				 * (Says tml, who knows nothing about Serbian
				 * or Croatian.)
				 * (I can feel those flames coming already.)
				 */
      switch (sub)
	{
	/* FIXME: How to distinguish Croatian and Latin Serbian locales?  */
	case SUBLANG_SERBIAN_LATIN: l = "sr"; break;
	case SUBLANG_SERBIAN_CYRILLIC: l = "sr"; sl = "@cyrillic"; break;
	default: l = "hr"; sl = "HR";
	}
      break;
    case LANG_CZECH: l = "cs"; sl = "CZ"; break;
    case LANG_DANISH: l = "da"; sl = "DK"; break;
#ifdef LANG_DIVEHI
    case LANG_DIVEHI: l = "div"; sl = "MV"; break;
#endif
    case LANG_DUTCH:
      l = "nl";
      switch (sub)
	{
	case SUBLANG_DUTCH: sl = "NL"; break;
	case SUBLANG_DUTCH_BELGIAN: sl = "BE"; break;
	}
      break;
    case LANG_ENGLISH:
      l = "en";
      switch (sub)
	{
	case SUBLANG_ENGLISH_US: sl = "US"; break;
	case SUBLANG_ENGLISH_UK: sl = "GB"; break;
	case SUBLANG_ENGLISH_AUS: sl = "AU"; break;
	case SUBLANG_ENGLISH_CAN: sl = "CA"; break;
	case SUBLANG_ENGLISH_NZ: sl = "NZ"; break;
	case SUBLANG_ENGLISH_EIRE: sl = "IE"; break;
	case SUBLANG_ENGLISH_SOUTH_AFRICA: sl = "ZA"; break;
	case SUBLANG_ENGLISH_JAMAICA: sl = "JM"; break;
	case SUBLANG_ENGLISH_CARIBBEAN: sl = "GD"; break; /* Grenada? */
	case SUBLANG_ENGLISH_BELIZE: sl = "BZ"; break;
	case SUBLANG_ENGLISH_TRINIDAD: sl = "TT"; break;
#ifdef SUBLANG_ENGLISH_ZIMBABWE
	case SUBLANG_ENGLISH_ZIMBABWE: sl = "ZW"; break;
#endif
#ifdef SUBLANG_ENGLISH_PHILIPPINES
	case SUBLANG_ENGLISH_PHILIPPINES: sl = "PH"; break;
#endif
	}
      break;
    case LANG_ESTONIAN: l = "et"; sl = "EE"; break;
    case LANG_FAEROESE: l = "fo"; sl = "FO"; break;
    case LANG_FARSI: l = "fa"; sl = "IR"; break;
    case LANG_FINNISH: l = "fi"; sl = "FI"; break;
    case LANG_FRENCH:
      l = "fr";
      switch (sub)
	{
	case SUBLANG_FRENCH: sl = "FR"; break;
	case SUBLANG_FRENCH_BELGIAN: sl = "BE"; break;
	case SUBLANG_FRENCH_CANADIAN: sl = "CA"; break;
	case SUBLANG_FRENCH_SWISS: sl = "CH"; break;
	case SUBLANG_FRENCH_LUXEMBOURG: sl = "LU"; break;
#ifdef SUBLANG_FRENCH_MONACO
	case SUBLANG_FRENCH_MONACO: sl = "MC"; break;
#endif
	}
      break;
      /* FIXME: LANG_GALICIAN: What's the code for Galician? */
#ifdef LANG_GEORGIAN
    case LANG_GEORGIAN: l = "ka"; sl = "GE"; break;
#endif
    case LANG_GERMAN:
      l = "de";
      switch (sub)
	{
	case SUBLANG_GERMAN: sl = "DE"; break;
	case SUBLANG_GERMAN_SWISS: sl = "CH"; break;
	case SUBLANG_GERMAN_AUSTRIAN: sl = "AT"; break;
	case SUBLANG_GERMAN_LUXEMBOURG: sl = "LU"; break;
	case SUBLANG_GERMAN_LIECHTENSTEIN: sl = "LI"; break;
	}
      break;
    case LANG_GREEK: l = "el"; sl = "GR"; break;
#ifdef LANG_GUJARATI
    case LANG_GUJARATI: l = "gu"; sl = "IN"; break;
#endif
    case LANG_HEBREW: l = "he"; sl = "IL"; break;
#ifdef LANG_HINDI
    case LANG_HINDI: l = "hi"; sl = "IN"; break;
#endif
    case LANG_HUNGARIAN: l = "hu"; sl = "HU"; break;
    case LANG_ICELANDIC: l = "is"; sl = "IS"; break;
    case LANG_INDONESIAN: l = "id"; sl = "ID"; break;
    case LANG_ITALIAN:
      l = "it";
      switch (sub)
	{
	case SUBLANG_ITALIAN: sl = "IT"; break;
	case SUBLANG_ITALIAN_SWISS: sl = "CH"; break;
	}
      break;
    case LANG_JAPANESE: l = "ja"; sl = "JP"; break;
#ifdef LANG_KANNADA
    case LANG_KANNADA: l = "kn"; sl = "IN"; break;
#endif
#ifdef LANG_KASHMIRI
    case LANG_KASHMIRI:
      l = "ks";
      switch (sub)
	{
	case SUBLANG_DEFAULT: sl = "PK"; break;
#ifdef SUBLANG_KASHMIRI_INDIA
	case SUBLANG_KASHMIRI_INDIA: sl = "IN"; break;
#endif
	}
      break;
#endif
#ifdef LANG_KAZAK
    case LANG_KAZAK: l = "kk"; sl = "KZ"; break;
#endif
#ifdef LANG_KONKANI
    case LANG_KONKANI:
      /* FIXME: Adjust this when such locales appear on Unix.  */
      l = "kok"; sl = "IN";
      break;
#endif
    case LANG_KOREAN: l = "ko"; sl = "KR"; break;
#ifdef LANG_KYRGYZ
    case LANG_KYRGYZ: l = "ky"; sl = "KG"; /* ??? */ break; 
#endif
    case LANG_LATVIAN: l = "lv"; sl = "LV"; break;
    case LANG_LITHUANIAN: l = "lt"; sl = "LT"; break;
#ifdef LANG_MACEDONIAN
    case LANG_MACEDONIAN: l = "mk"; sl = "MK"; break;
#endif
#ifdef LANG_MALAY
    case LANG_MALAY:
      l = "ms";
      switch (sub)
	{
#ifdef SUBLANG_MALAY_MALAYSIA
	case SUBLANG_MALAY_MALAYSIA: sl = "MY"; break;
#endif
#ifdef SUBLANG_MALAY_BRUNEI_DARUSSALAM
	case SUBLANG_MALAY_BRUNEI_DARUSSALAM: sl = "BN"; break;
#endif
	}
      break;
#endif
#ifdef LANG_MALAYALAM
    case LANG_MALAYALAM: l = "ml"; sl = "IN"; break;
#endif
#ifdef LANG_MANIPURI
    case LANG_MANIPURI:
      /* FIXME: Adjust this when such locales appear on Unix.  */
      l = "mni"; sl = "IN";
      break;
#endif
#ifdef LANG_MARATHI
    case LANG_MARATHI: l = "mr"; sl = "IN"; break;
#endif
#ifdef LANG_MONGOLIAN
    case LANG_MONGOLIAN: l = "mn"; sl = "MN"; break;
#endif
#ifdef LANG_NEPALI
    case LANG_NEPALI:
      l = "ne";
      switch (sub)
	{
	case SUBLANG_DEFAULT: sl = "NP"; break;
#ifdef SUBLANG_NEPALI_INDIA
	case SUBLANG_NEPALI_INDIA: sl = "IN"; break;
#endif
	}
      break;
#endif
    case LANG_NORWEGIAN:
      l = "no";
      switch (sub)
	{
	case SUBLANG_NORWEGIAN_BOKMAL: sl = "NO"; break;
	case SUBLANG_NORWEGIAN_NYNORSK: l = "nn"; sl = "NO"; break;
	}
      break;
#ifdef LANG_ORIYA
    case LANG_ORIYA: l = "or"; sl = "IN"; break;
#endif
    case LANG_POLISH: l = "pl"; sl = "PL"; break;
    case LANG_PORTUGUESE:
      l = "pt";
      switch (sub)
	{
	case SUBLANG_PORTUGUESE: sl = "PT"; break;
	case SUBLANG_PORTUGUESE_BRAZILIAN: sl = "BR"; break;
	}
      break;
#ifdef LANG_PUNJABI
    case LANG_PUNJABI: l = "pa"; sl = "IN"; break;
#endif
    case LANG_ROMANIAN: l = "ro"; sl = "RO"; break;
    case LANG_RUSSIAN:
      l = "ru"; /* sl could be "RU" or "UA".  */
      break;
#ifdef LANG_SANSKRIT
    case LANG_SANSKRIT: l = "sa"; sl = "IN"; break;
#endif
#ifdef LANG_SINDHI
    case LANG_SINDHI: l = "sd"; break;
#endif
    case LANG_SLOVAK: l = "sk"; sl = "SK"; break;
    case LANG_SLOVENIAN: l = "sl"; sl = "SI"; break;
#ifdef LANG_SORBIAN
    case LANG_SORBIAN:
      /* FIXME: Adjust this when such locales appear on Unix.  */
      l = "wen"; sl = "DE";
      break;
#endif
    case LANG_SPANISH:
      l = "es";
      switch (sub)
	{
	case SUBLANG_SPANISH: sl = "ES"; break;
	case SUBLANG_SPANISH_MEXICAN: sl = "MX"; break;
	case SUBLANG_SPANISH_MODERN:
	  sl = "ES@modern"; break;	/* not seen on Unix */
	case SUBLANG_SPANISH_GUATEMALA: sl = "GT"; break;
	case SUBLANG_SPANISH_COSTA_RICA: sl = "CR"; break;
	case SUBLANG_SPANISH_PANAMA: sl = "PA"; break;
	case SUBLANG_SPANISH_DOMINICAN_REPUBLIC: sl = "DO"; break;
	case SUBLANG_SPANISH_VENEZUELA: sl = "VE"; break;
	case SUBLANG_SPANISH_COLOMBIA: sl = "CO"; break;
	case SUBLANG_SPANISH_PERU: sl = "PE"; break;
	case SUBLANG_SPANISH_ARGENTINA: sl = "AR"; break;
	case SUBLANG_SPANISH_ECUADOR: sl = "EC"; break;
	case SUBLANG_SPANISH_CHILE: sl = "CL"; break;
	case SUBLANG_SPANISH_URUGUAY: sl = "UY"; break;
	case SUBLANG_SPANISH_PARAGUAY: sl = "PY"; break;
	case SUBLANG_SPANISH_BOLIVIA: sl = "BO"; break;
	case SUBLANG_SPANISH_EL_SALVADOR: sl = "SV"; break;
	case SUBLANG_SPANISH_HONDURAS: sl = "HN"; break;
	case SUBLANG_SPANISH_NICARAGUA: sl = "NI"; break;
	case SUBLANG_SPANISH_PUERTO_RICO: sl = "PR"; break;
	}
      break;
#ifdef LANG_SWAHILI
    case LANG_SWAHILI: l = "sw"; break;
#endif
    case LANG_SWEDISH:
      l = "sv";
      switch (sub)
	{
	case SUBLANG_DEFAULT: sl = "SE"; break;
	case SUBLANG_SWEDISH_FINLAND: sl = "FI"; break;
	}
      break;
#ifdef LANG_SYRIAC
    case LANG_SYRIAC: l = "syr"; break;
#endif
#ifdef LANG_TAMIL
    case LANG_TAMIL:
      l = "ta"; /* sl could be "IN" or "LK".  */
      break;
#endif
#ifdef LANG_TATAR
    case LANG_TATAR: l = "tt"; break;
#endif
#ifdef LANG_TELUGU
    case LANG_TELUGU: l = "te"; sl = "IN"; break;
#endif
    case LANG_THAI: l = "th"; sl = "TH"; break;
    case LANG_TURKISH: l = "tr"; sl = "TR"; break;
    case LANG_UKRAINIAN: l = "uk"; sl = "UA"; break;
#ifdef LANG_URDU
    case LANG_URDU:
      l = "ur";
      switch (sub)
	{
#ifdef SUBLANG_URDU_PAKISTAN
	case SUBLANG_URDU_PAKISTAN: sl = "PK"; break;
#endif
#ifdef SUBLANG_URDU_INDIA
	case SUBLANG_URDU_INDIA: sl = "IN"; break;
#endif
	}
      break;
#endif
#ifdef LANG_UZBEK
    case LANG_UZBEK:
      l = "uz";
      switch (sub)
	{
	/* FIXME: Adjust this when Uzbek locales appear on Unix.  */
#ifdef SUBLANG_UZBEK_LATIN
	case SUBLANG_UZBEK_LATIN: sl = "UZ@latin"; break;
#endif
#ifdef SUBLANG_UZBEK_CYRILLIC
	case SUBLANG_UZBEK_CYRILLIC: sl = "UZ@cyrillic"; break;
#endif
	}
      break;
#endif
    case LANG_VIETNAMESE: l = "vi"; sl = "VN"; break;
    default: l = "xx"; break;
    }
  strcpy (bfr, l);
  if (sl != NULL)
    {
      if (sl[0] != '@')
	strcat (bfr, "_");
      strcat (bfr, sl);
    }

  return g_strdup (bfr);
}

/**
 * g_win32_error_message:
 * @error: error code.
 *
 * Translate a Win32 error code (as returned by GetLastError()) into
 * the corresponding message. The message is either language neutral,
 * or in the thread's language, or the user's language, the system's
 * language, or US English (see docs for <function>FormatMessage()</function>). *
 * The returned string should be deallocated with g_free().
 *
 * Returns: newly-allocated error message
 **/
gchar *
g_win32_error_message (gint error)
{
  gchar *msg;
  gchar *retval;
  int nbytes;

  FormatMessage (FORMAT_MESSAGE_ALLOCATE_BUFFER
		 |FORMAT_MESSAGE_IGNORE_INSERTS
		 |FORMAT_MESSAGE_FROM_SYSTEM,
		 NULL, error, 0,
		 (LPTSTR) &msg, 0, NULL);
  nbytes = strlen (msg);

  if (nbytes > 2 && msg[nbytes-1] == '\n' && msg[nbytes-2] == '\r')
    msg[nbytes-2] = '\0';
  
  retval = g_strdup (msg);

  if (msg != NULL)
    LocalFree (msg);

  return retval;
}

static gchar *
get_package_directory_from_module (gchar *module_name)
{
  static GHashTable *module_dirs = NULL;
  G_LOCK_DEFINE_STATIC (module_dirs);
  HMODULE hmodule = NULL;
  gchar *fn;
  gchar *p;
  gchar *result;

  G_LOCK (module_dirs);

  if (module_dirs == NULL)
    module_dirs = g_hash_table_new (g_str_hash, g_str_equal);
  
  result = g_hash_table_lookup (module_dirs, module_name ? module_name : "");
      
  if (result)
    {
      G_UNLOCK (module_dirs);
      return g_strdup (result);
    }

  if (module_name)
    {
      hmodule = GetModuleHandle (module_name);
      if (!hmodule)
	return NULL;
    }

  fn = g_malloc (MAX_PATH);
  if (!GetModuleFileName (hmodule, fn, MAX_PATH))
    {
      G_UNLOCK (module_dirs);
      g_free (fn);
      return NULL;
    }

  if ((p = strrchr (fn, G_DIR_SEPARATOR)) != NULL)
    *p = '\0';

  p = strrchr (fn, G_DIR_SEPARATOR);
  if (p && (g_ascii_strcasecmp (p + 1, "bin") == 0 ||
	    g_ascii_strcasecmp (p + 1, "lib") == 0))
    *p = '\0';

#ifdef G_WITH_CYGWIN
  /* In Cygwin we need to have POSIX paths */
  {
    gchar tmp[MAX_PATH];

    cygwin_conv_to_posix_path(fn, tmp);
    g_free(fn);
    fn = g_strdup(tmp);
  }
#endif

  g_hash_table_insert (module_dirs, module_name ? module_name : "", fn);

  G_UNLOCK (module_dirs);

  return g_strdup (fn);
}

/**
 * g_win32_get_package_installation_directory:
 * @package: An identifier for a software package, or %NULL
 * @dll_name: The name of a DLL that a package provides, or %NULL.
 *
 * Try to determine the installation directory for a software package.
 * Typically used by GNU software packages.
 *
 * @package should be a short identifier for the package. Typically it
 * is the same identifier as used for
 * <literal>GETTEXT_PACKAGE</literal> in software configured according
 * to GNU standards. The function first looks in the Windows Registry
 * for the value <literal>#InstallationDirectory</literal> in the key
 * <literal>#HKLM\Software\@package</literal>, and if that value
 * exists and is a string, returns that.
 *
 * If @package is %NULL, or the above value isn't found in the
 * Registry, but @dll_name is non-%NULL, it should name a DLL loaded
 * into the current process. Typically that would be the name of the
 * DLL calling this function, looking for its installation
 * directory. The function then asks Windows what directory that DLL
 * was loaded from. If that directory's last component is "bin" or
 * "lib", the parent directory is returned, otherwise the directory
 * itself. If that DLL isn't loaded, the function proceeds as if
 * @dll_name was %NULL.
 *
 * If both @package and @dll_name are %NULL, the directory from where
 * the main executable of the process was loaded is uses instead in
 * the same way as above.
 *
 * Returns: a string containing the installation directory for @package.
 * The return value should be freed with g_free() when not needed any longer.
 **/

gchar *
g_win32_get_package_installation_directory (gchar *package,
					    gchar *dll_name)
{
  static GHashTable *package_dirs = NULL;
  G_LOCK_DEFINE_STATIC (package_dirs);
  gchar *result = NULL;
  gchar *key;
  HKEY reg_key = NULL;
  DWORD type;
  DWORD nbytes;

  if (package != NULL)
    {
      G_LOCK (package_dirs);
      
      if (package_dirs == NULL)
	package_dirs = g_hash_table_new (g_str_hash, g_str_equal);
      
      result = g_hash_table_lookup (package_dirs, package);
      
      if (result && result[0])
	{
	  G_UNLOCK (package_dirs);
	  return g_strdup (result);
	}
      
      key = g_strconcat ("Software\\", package, NULL);
      
      nbytes = 0;
      if ((RegOpenKeyEx (HKEY_CURRENT_USER, key, 0,
			 KEY_QUERY_VALUE, &reg_key) == ERROR_SUCCESS
	   && RegQueryValueEx (reg_key, "InstallationDirectory", 0,
			       &type, NULL, &nbytes) == ERROR_SUCCESS)
	  ||
	  ((RegOpenKeyEx (HKEY_LOCAL_MACHINE, key, 0,
			 KEY_QUERY_VALUE, &reg_key) == ERROR_SUCCESS
	   && RegQueryValueEx (reg_key, "InstallationDirectory", 0,
			       &type, NULL, &nbytes) == ERROR_SUCCESS)
	   && type == REG_SZ))
	{
	  result = g_malloc (nbytes + 1);
	  RegQueryValueEx (reg_key, "InstallationDirectory", 0,
			   &type, result, &nbytes);
	  result[nbytes] = '\0';
	}

      if (reg_key != NULL)
	RegCloseKey (reg_key);
      
      g_free (key);

      if (result)
	{
	  g_hash_table_insert (package_dirs, package, result);
	  G_UNLOCK (package_dirs);
	  return g_strdup (result);
	}
      G_UNLOCK (package_dirs);
    }

  if (dll_name != NULL)
    result = get_package_directory_from_module (dll_name);

  if (result == NULL)
    result = get_package_directory_from_module (NULL);

  return result;
}

/**
 * g_win32_get_package_installation_subdirectory:
 * @package: An identifier for a software package, or %NULL.
 * @dll_name: The name of a DLL that a package provides, or %NULL.
 * @subdir: A subdirectory of the package installation directory.
 *
 * Returns a newly-allocated string containing the path of the
 * subdirectory @subdir in the return value from calling
 * g_win32_get_package_installation_directory() with the @package and
 * @dll_name parameters. 
 *
 * Returns: a string containing the complete path to @subdir inside the 
 * installation directory of @package. The return value should be freed with
 * g_free() when no longer needed.
 **/

gchar *
g_win32_get_package_installation_subdirectory (gchar *package,
					       gchar *dll_name,
					       gchar *subdir)
{
  gchar *prefix;

  prefix = g_win32_get_package_installation_directory (package, dll_name);

  return g_build_filename (prefix, subdir, NULL);
}
