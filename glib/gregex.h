/* GRegex -- regular expression API wrapper around PCRE.
 *
 * Copyright (C) 1999, 2000 Scott Wimer
 * Copyright (C) 2004, Matthias Clasen <mclasen@redhat.com>
 * Copyright (C) 2005 - 2006, Marco Barisione <marco@barisione.org>
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef __G_REGEX_H__
#define __G_REGEX_H__

#include <glib.h>

G_BEGIN_DECLS

typedef enum
{
  G_REGEX_ERROR_COMPILE,
  G_REGEX_ERROR_OPTIMIZE,
  G_REGEX_ERROR_REPLACE,
  G_REGEX_ERROR_MATCH
} GRegexError;

#define G_REGEX_ERROR g_regex_error_quark ()

GQuark g_regex_error_quark (void);

/* Remember to update G_REGEX_COMPILE_MASK in gregex.c after
 * adding a new flag. */
typedef enum
{
  G_REGEX_CASELESS          = 1 << 0,
  G_REGEX_MULTILINE         = 1 << 1,
  G_REGEX_DOTALL            = 1 << 2,
  G_REGEX_EXTENDED          = 1 << 3,
  G_REGEX_ANCHORED          = 1 << 4,
  G_REGEX_DOLLAR_ENDONLY    = 1 << 5,
  G_REGEX_UNGREEDY          = 1 << 9,
  G_REGEX_RAW               = 1 << 11,
  G_REGEX_NO_AUTO_CAPTURE   = 1 << 12,
  G_REGEX_DUPNAMES          = 1 << 19,
  G_REGEX_NEWLINE_CR        = 1 << 20,
  G_REGEX_NEWLINE_LF        = 1 << 21,
  G_REGEX_NEWLINE_CRLF      = G_REGEX_NEWLINE_CR | G_REGEX_NEWLINE_LF
} GRegexCompileFlags;

/* Remember to update G_REGEX_MATCH_MASK in gregex.c after
 * adding a new flag. */
typedef enum
{
  G_REGEX_MATCH_ANCHORED      = 1 << 4,
  G_REGEX_MATCH_NOTBOL        = 1 << 7,
  G_REGEX_MATCH_NOTEOL        = 1 << 8,
  G_REGEX_MATCH_NOTEMPTY      = 1 << 10,
  G_REGEX_MATCH_PARTIAL       = 1 << 15,
  G_REGEX_MATCH_NEWLINE_CR    = 1 << 20,
  G_REGEX_MATCH_NEWLINE_LF    = 1 << 21,
  G_REGEX_MATCH_NEWLINE_CRLF  = G_REGEX_MATCH_NEWLINE_CR | G_REGEX_MATCH_NEWLINE_LF,
  G_REGEX_MATCH_NEWLINE_ANY   = 1 << 22,
} GRegexMatchFlags;

typedef struct _GRegex  GRegex;

typedef gboolean (*GRegexEvalCallback) (const GRegex*, const gchar*, GString*, gpointer);


GRegex		 *g_regex_new			(const gchar         *pattern,
						 GRegexCompileFlags   compile_options,
						 GRegexMatchFlags     match_options,
						 GError             **error);
void		  g_regex_free			(GRegex              *regex);
gboolean	  g_regex_optimize		(GRegex              *regex,
						 GError             **error);
GRegex		 *g_regex_copy			(const GRegex        *regex);
const gchar	 *g_regex_get_pattern		(const GRegex        *regex);
void		  g_regex_clear			(GRegex              *regex);
gboolean	  g_regex_match_simple		(const gchar         *pattern,
						 const gchar         *string,
						 GRegexCompileFlags   compile_options,
						 GRegexMatchFlags     match_options);
gboolean	  g_regex_match			(GRegex              *regex,
						 const gchar         *string,
						 GRegexMatchFlags     match_options);
gboolean	  g_regex_match_full		(GRegex              *regex,
						 const gchar         *string,
						 gssize               string_len,
						 gint                 start_position,
						 GRegexMatchFlags     match_options,
						 GError             **error);
gboolean	  g_regex_match_next		(GRegex              *regex,
						 const gchar         *string,
						 GRegexMatchFlags     match_options);
gboolean	  g_regex_match_next_full	(GRegex              *regex,
						 const gchar         *string,
						 gssize               string_len,
						 gint                 start_position,
						 GRegexMatchFlags     match_options,
						 GError             **error);
gboolean	  g_regex_match_all		(GRegex              *regex,
						 const gchar         *string,
						 GRegexMatchFlags     match_options);
gboolean	  g_regex_match_all_full	(GRegex              *regex,
						 const gchar         *string,
						 gssize               string_len,
						 gint                 start_position,
						 GRegexMatchFlags     match_options,
						 GError             **error);
gint		  g_regex_get_match_count	(const GRegex        *regex);
gboolean	  g_regex_is_partial_match	(const GRegex        *regex);
gchar		 *g_regex_fetch			(const GRegex        *regex,
						 gint                 match_num,
						 const gchar         *string);
gboolean	  g_regex_fetch_pos		(const GRegex        *regex,
						 gint                 match_num,
						 gint                *start_pos,
						 gint                *end_pos);
gchar		 *g_regex_fetch_named		(const GRegex        *regex,
						 const gchar         *name,
						 const gchar         *string);
gboolean	  g_regex_fetch_named_pos	(const GRegex        *regex,
						 const gchar         *name,
						 gint                *start_pos,
						 gint                *end_pos);
gchar		**g_regex_fetch_all		(const GRegex        *regex,
						 const gchar         *string);
gint		  g_regex_get_string_number	(const GRegex        *regex, 
						 const gchar         *name);
gchar		**g_regex_split_simple		(const gchar         *pattern,
						 const gchar         *string,
						 GRegexCompileFlags   compile_options,
						 GRegexMatchFlags     match_options);
gchar		**g_regex_split			(GRegex              *regex,
						 const gchar         *string,
						 GRegexMatchFlags     match_options);
gchar		**g_regex_split_full		(GRegex              *regex,
						 const gchar         *string,
						 gssize               string_len,
						 gint                 start_position,
						 GRegexMatchFlags     match_options,
						 gint                 max_tokens,
						 GError             **error);
gchar		 *g_regex_split_next		(GRegex              *regex,
						 const gchar         *string,
						 GRegexMatchFlags     match_options);
gchar		 *g_regex_split_next_full	(GRegex              *regex,
						 const gchar         *string,
						 gssize               string_len,
						 gint                 start_position,
						 GRegexMatchFlags     match_options,
						 GError             **error);
gchar		 *g_regex_expand_references	(GRegex              *regex,
						 const gchar         *string,
						 const gchar         *string_to_expand,
						 GError             **error);
gchar		 *g_regex_replace		(GRegex              *regex,
						 const gchar         *string,
						 gssize               string_len,
						 gint                 start_position,
						 const gchar         *replacement,
						 GRegexMatchFlags     match_options,
						 GError             **error);
gchar		 *g_regex_replace_literal	(GRegex              *regex,
						 const gchar         *string,
						 gssize               string_len,
						 gint                 start_position,
						 const gchar         *replacement,
						 GRegexMatchFlags     match_options,
						 GError             **error);
gchar		 *g_regex_replace_eval		(GRegex              *regex,
						 const gchar         *string,
						 gssize               string_len,
						 gint                 start_position,
						 GRegexMatchFlags     match_options,
						 GRegexEvalCallback   eval,
						 gpointer             user_data,
						 GError             **error);
gchar		 *g_regex_escape_string		(const gchar         *string,
						 gint                 length);


G_END_DECLS


#endif  /*  __G_REGEX_H__ */
