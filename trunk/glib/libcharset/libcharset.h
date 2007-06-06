/* Copyright (C) 2000-2001 Free Software Foundation, Inc.
   This file is part of the GNU CHARSET Library.

   The GNU CHARSET Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The GNU CHARSET Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with the GNU CHARSET Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#ifndef _LIBCHARSET_H
#define _LIBCHARSET_H


#ifdef __cplusplus
extern "C" {
#endif


/* Determine the current locale's character encoding, and canonicalize it
   into one of the canonical names listed in config.charset.
   The result must not be freed; it is statically allocated.
   If the canonical name cannot be determined, the result is a non-canonical
   name.  */
extern const char * _g_locale_charset_raw (void);
extern const char * _g_locale_charset_unalias (const char *codeset);
extern const char * _g_locale_get_charset_aliases (void);

#ifdef __cplusplus
}
#endif


#endif /* _LIBCHARSET_H */
