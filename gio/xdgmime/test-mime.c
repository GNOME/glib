/* 
 * Copyright (C) 2003,2004  Red Hat, Inc.
 * Copyright (C) 2003,2004  Jonathan Blandford <jrb@alum.mit.edu>
 *
 * Licensed under the Academic Free License version 2.0
 * Or under the following terms:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include "xdgmime.h"
#include "xdgmimeglob.h"
#include <string.h>
#include <stdio.h>


static void
test_individual_glob (const char  *glob,
		      XdgGlobType  expected_type)
{
  XdgGlobType test_type;

  test_type = _xdg_glob_determine_type (glob);
  if (test_type != expected_type)
    {
      printf ("Test Failed: %s is of type %s, but %s is expected\n",
	      glob,
	      ((test_type == XDG_GLOB_LITERAL)?"XDG_GLOB_LITERAL":
	       ((test_type == XDG_GLOB_SIMPLE)?"XDG_GLOB_SIMPLE":"XDG_GLOB_FULL")),
	      ((expected_type == XDG_GLOB_LITERAL)?"XDG_GLOB_LITERAL":
	       ((expected_type == XDG_GLOB_SIMPLE)?"XDG_GLOB_SIMPLE":"XDG_GLOB_COMPLEX")));
    }
}

static void
test_glob_type (void)
{
  test_individual_glob ("*.gif", XDG_GLOB_SIMPLE);
  test_individual_glob ("Foo*.gif", XDG_GLOB_FULL);
  test_individual_glob ("*[4].gif", XDG_GLOB_FULL);
  test_individual_glob ("Makefile", XDG_GLOB_LITERAL);
  test_individual_glob ("sldkfjvlsdf\\\\slkdjf", XDG_GLOB_FULL);
  test_individual_glob ("tree.[ch]", XDG_GLOB_FULL);
}

static void
test_alias (const char *mime_a,
	    const char *mime_b,
	    int         expected)
{
  int actual;

  actual = xdg_mime_mime_type_equal (mime_a, mime_b);

  if (actual != expected)
    {
      printf ("Test Failed: %s is %s to %s\n", 
	      mime_a, actual ? "equal" : "not equal", mime_b);
    }
}

static void
test_aliasing (void)
{
  test_alias ("application/x-wordperfect", "application/vnd.wordperfect", 1);
  test_alias ("application/x-gnome-app-info", "application/x-desktop", 1);
  test_alias ("application/x-wordperfect", "application/vnd.wordperfect", 1);
  test_alias ("application/x-wordperfect", "audio/x-midi", 0);
  test_alias ("/", "vnd/vnd", 0);
  test_alias ("application/octet-stream", "text/plain", 0);
  test_alias ("text/plain", "text/*", 0);
}

static void
test_subclass (const char *mime_a,
	       const char *mime_b,
	       int         expected)
{
  int actual;

  actual = xdg_mime_mime_type_subclass (mime_a, mime_b);

  if (actual != expected)
    {
      printf ("Test Failed: %s is %s of %s\n", 
	      mime_a, actual ? "subclass" : "not subclass", mime_b);
    }
}

static void
test_subclassing (void)
{
  test_subclass ("application/rtf", "text/plain", 1);
  test_subclass ("message/news", "text/plain", 1);
  test_subclass ("message/news", "message/*", 1);
  test_subclass ("message/news", "text/*", 1);
  test_subclass ("message/news", "application/octet-stream", 1);
  test_subclass ("application/rtf", "application/octet-stream", 1);
  test_subclass ("application/x-gnome-app-info", "text/plain", 1);
  test_subclass ("image/x-djvu", "image/vnd.djvu", 1);
  test_subclass ("image/vnd.djvu", "image/x-djvu", 1);
  test_subclass ("image/vnd.djvu", "text/plain", 0);
  test_subclass ("image/vnd.djvu", "text/*", 0);
  test_subclass ("text/*", "text/plain", 1);
}

static void
test_one_match (const char *filename, const char *expected)
{
  const char *actual;

  actual = xdg_mime_get_mime_type_from_file_name (filename);

  if (strcmp (actual, expected) != 0) 
    {
      printf ("Test Failed: mime type of %s is %s, expected %s\n", 
	      filename, actual, expected);
    }  
}

static void
test_matches (void)
{
  test_one_match ("foo.bar.epub", "application/epub+zip");
  test_one_match ("core", "application/x-core");
  test_one_match ("README.in", "text/x-readme");
  test_one_match ("README.gz", "application/x-gzip");
  test_one_match ("blabla.cs", "text/x-csharp");
  test_one_match ("blabla.f90", "text/x-fortran");
  test_one_match ("blabla.F95", "text/x-fortran");
  test_one_match ("tarball.tar.gz", "application/x-compressed-tar");
  test_one_match ("file.gz", "application/x-gzip");
  test_one_match ("file.tar.lzo", "application/x-tzo");
  test_one_match ("file.lzo", "application/x-lzop");
}

static void
test_one_icon (const char *mimetype, const char *expected)
{
  const char *actual;

  actual = xdg_mime_get_generic_icon (mimetype);

  if (actual != expected && strcmp (actual, expected) != 0) 
    {
      printf ("Test Failed: icon of %s is %s, expected %s\n", 
             mimetype, actual, expected);
    }  
}

static void
test_icons (void)
{
  test_one_icon ("application/x-font-ttx", "font-x-generic");
  test_one_icon ("application/mathematica", "x-office-document");
  test_one_icon ("text/plain", NULL);
}

int
main (int argc, char *argv[])
{
  const char *result;
  const char *file_name;
  int i;

  test_glob_type ();
  test_aliasing ();
  test_subclassing ();
  test_matches ();
  test_icons ();

  for (i = 1; i < argc; i++)
    {
      file_name = argv[i];
      result = xdg_mime_get_mime_type_for_file (file_name, NULL);
      printf ("File \"%s\" has a mime-type of %s\n", file_name, result);
    }

#if 0
  xdg_mime_dump ();
#endif
  return 0;
}
     
