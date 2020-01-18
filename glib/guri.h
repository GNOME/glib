/* GLIB - Library of useful routines for C programming
 * Copyright 2015 Red Hat, Inc.
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
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see
 * <http://www.gnu.org/licenses/>.
 */

#pragma once

#if !defined (__GLIB_H_INSIDE__) && !defined (GLIB_COMPILATION)
#error "Only <glib.h> can be included directly."
#endif

#include <glib/gtypes.h>

G_BEGIN_DECLS

typedef struct _GUri GUri;

GLIB_AVAILABLE_IN_2_64
GUri *       g_uri_ref              (GUri *uri);
GLIB_AVAILABLE_IN_2_64
void         g_uri_unref            (GUri *uri);

typedef enum {
  G_URI_FLAGS_NONE            = 0,
  G_URI_FLAGS_PARSE_STRICT    = 1 << 0,
  G_URI_FLAGS_HAS_PASSWORD    = 1 << 1,
  G_URI_FLAGS_HAS_AUTH_PARAMS = 1 << 2,
  G_URI_FLAGS_ENCODED         = 1 << 3,
  G_URI_FLAGS_NON_DNS         = 1 << 4,
} GUriFlags;

GLIB_AVAILABLE_IN_2_64
gboolean     g_uri_split            (const gchar  *uri_string,
                                     GUriFlags     flags,
                                     gchar       **scheme,
                                     gchar       **userinfo,
                                     gchar       **host,
                                     gint         *port,
                                     gchar       **path,
                                     gchar       **query,
                                     gchar       **fragment,
                                     GError      **error);
GLIB_AVAILABLE_IN_2_64
gboolean     g_uri_split_with_user  (const gchar  *uri_string,
                                     GUriFlags     flags,
                                     gchar       **scheme,
                                     gchar       **user,
                                     gchar       **password,
                                     gchar       **auth_params,
                                     gchar       **host,
                                     gint         *port,
                                     gchar       **path,
                                     gchar       **query,
                                     gchar       **fragment,
                                     GError      **error);
GLIB_AVAILABLE_IN_2_64
gboolean     g_uri_split_network    (const gchar  *uri_string,
                                     GUriFlags     flags,
                                     gchar       **scheme,
                                     gchar       **host,
                                     gint         *port,
                                     GError      **error);

GLIB_AVAILABLE_IN_2_64
gboolean     g_uri_is_valid         (const gchar  *uri_string,
                                     GUriFlags     flags,
                                     GError      **error);

GLIB_AVAILABLE_IN_2_64
gchar *      g_uri_join             (GUriFlags     flags,
                                     const gchar  *scheme,
                                     const gchar  *userinfo,
                                     const gchar  *host,
                                     gint          port,
                                     const gchar  *path,
                                     const gchar  *query,
                                     const gchar  *fragment);
GLIB_AVAILABLE_IN_2_64
gchar *      g_uri_join_with_user   (GUriFlags     flags,
                                     const gchar  *scheme,
                                     const gchar  *user,
                                     const gchar  *password,
                                     const gchar  *auth_params,
                                     const gchar  *host,
                                     gint          port,
                                     const gchar  *path,
                                     const gchar  *query,
                                     const gchar  *fragment);

GLIB_AVAILABLE_IN_2_64
GUri *       g_uri_parse            (const gchar  *uri_string,
                                     GUriFlags     flags,
                                     GError      **error);
GLIB_AVAILABLE_IN_2_64
GUri *       g_uri_parse_relative   (GUri         *base_uri,
                                     const gchar  *uri_string,
                                     GUriFlags     flags,
                                     GError      **error);

GLIB_AVAILABLE_IN_2_64
gchar *      g_uri_resolve_relative (const gchar  *base_uri_string,
                                     const gchar  *uri_string,
                                     GUriFlags     flags,
                                     GError      **error);

GLIB_AVAILABLE_IN_2_64
GUri *       g_uri_build            (GUriFlags     flags,
                                     const gchar  *scheme,
                                     const gchar  *userinfo,
                                     const gchar  *host,
                                     gint          port,
                                     const gchar  *path,
                                     const gchar  *query,
                                     const gchar  *fragment);
GLIB_AVAILABLE_IN_2_64
GUri *       g_uri_build_with_user  (GUriFlags     flags,
                                     const gchar  *scheme,
                                     const gchar  *user,
                                     const gchar  *password,
                                     const gchar  *auth_params,
                                     const gchar  *host,
                                     gint          port,
                                     const gchar  *path,
                                     const gchar  *query,
                                     const gchar  *fragment);

typedef enum {
  G_URI_HIDE_NONE        = 0,
  G_URI_HIDE_USERINFO    = 1 << 0,
  G_URI_HIDE_PASSWORD    = 1 << 1,
  G_URI_HIDE_AUTH_PARAMS = 1 << 2,
  G_URI_HIDE_FRAGMENT    = 1 << 3,
} GUriHideFlags;

GLIB_AVAILABLE_IN_2_64
char *       g_uri_to_string         (GUri          *uri);
GLIB_AVAILABLE_IN_2_64
char *       g_uri_to_string_partial (GUri          *uri,
                                      GUriHideFlags  flags);

GLIB_AVAILABLE_IN_2_64
const gchar *g_uri_get_scheme        (GUri          *uri);
GLIB_AVAILABLE_IN_2_64
const gchar *g_uri_get_userinfo      (GUri          *uri);
GLIB_AVAILABLE_IN_2_64
const gchar *g_uri_get_user          (GUri          *uri);
GLIB_AVAILABLE_IN_2_64
const gchar *g_uri_get_password      (GUri          *uri);
GLIB_AVAILABLE_IN_2_64
const gchar *g_uri_get_auth_params   (GUri          *uri);
GLIB_AVAILABLE_IN_2_64
const gchar *g_uri_get_host          (GUri          *uri);
GLIB_AVAILABLE_IN_2_64
gint         g_uri_get_port          (GUri          *uri);
GLIB_AVAILABLE_IN_2_64
const gchar *g_uri_get_path          (GUri          *uri);
GLIB_AVAILABLE_IN_2_64
const gchar *g_uri_get_query         (GUri          *uri);
GLIB_AVAILABLE_IN_2_64
const gchar *g_uri_get_fragment      (GUri          *uri);
GLIB_AVAILABLE_IN_2_64
GUriFlags    g_uri_get_flags         (GUri          *uri);

GLIB_AVAILABLE_IN_2_64
GHashTable * g_uri_parse_params      (const gchar   *params,
                                      gssize         length,
                                      gchar          separator,
                                      gboolean       case_insensitive);

/**
 * G_URI_ERROR:
 *
 * Error domain for URI methods. Errors in this domain will be from
 * the #GUriError enumeration. See #GError for information on error
 * domains.
 */
#define G_URI_ERROR (g_uri_error_quark ())
GLIB_AVAILABLE_IN_2_64
GQuark g_uri_error_quark (void);

/**
 * GUriError:
 * @G_URI_ERROR_MISC: miscellaneous error
 * @G_URI_ERROR_BAD_SCHEME: the scheme of a URI could not be parsed.
 * @G_URI_ERROR_BAD_USER: the user/userinfo of a URI could not be parsed.
 * @G_URI_ERROR_BAD_PASSWORD: the password of a URI could not be parsed.
 * @G_URI_ERROR_BAD_AUTH_PARAMS: the authentication parameters of a URI could not be parsed.
 * @G_URI_ERROR_BAD_HOST: the host of a URI could not be parsed.
 * @G_URI_ERROR_BAD_PORT: the port of a URI could not be parsed.
 * @G_URI_ERROR_BAD_PATH: the path of a URI could not be parsed.
 * @G_URI_ERROR_BAD_QUERY: the query of a URI could not be parsed.
 * @G_URI_ERROR_BAD_FRAGMENT: the fragment of a URI could not be parsed.
 *
 * Error codes returned by #GUri methods.
 */
typedef enum
{
  G_URI_ERROR_MISC,
  G_URI_ERROR_BAD_SCHEME,
  G_URI_ERROR_BAD_USER,
  G_URI_ERROR_BAD_PASSWORD,
  G_URI_ERROR_BAD_AUTH_PARAMS,
  G_URI_ERROR_BAD_HOST,
  G_URI_ERROR_BAD_PORT,
  G_URI_ERROR_BAD_PATH,
  G_URI_ERROR_BAD_QUERY,
  G_URI_ERROR_BAD_FRAGMENT
} GUriError;

/**
 * G_URI_RESERVED_CHARS_GENERIC_DELIMITERS:
 *
 * Generic delimiters characters as defined in RFC 3986. Includes ":/?#[]@".
 **/
#define G_URI_RESERVED_CHARS_GENERIC_DELIMITERS ":/?#[]@"

/**
 * G_URI_RESERVED_CHARS_SUBCOMPONENT_DELIMITERS:
 *
 * Subcomponent delimiter characters as defined in RFC 3986. Includes "!$&'()*+,;=".
 **/
#define G_URI_RESERVED_CHARS_SUBCOMPONENT_DELIMITERS "!$&'()*+,;="

/**
 * G_URI_RESERVED_CHARS_ALLOWED_IN_PATH_ELEMENT:
 *
 * Allowed characters in path elements. Includes "!$&'()*+,;=:@".
 **/
#define G_URI_RESERVED_CHARS_ALLOWED_IN_PATH_ELEMENT G_URI_RESERVED_CHARS_SUBCOMPONENT_DELIMITERS ":@"

/**
 * G_URI_RESERVED_CHARS_ALLOWED_IN_PATH:
 *
 * Allowed characters in a path. Includes "!$&'()*+,;=:@/".
 **/
#define G_URI_RESERVED_CHARS_ALLOWED_IN_PATH G_URI_RESERVED_CHARS_ALLOWED_IN_PATH_ELEMENT "/"

/**
 * G_URI_RESERVED_CHARS_ALLOWED_IN_USERINFO:
 *
 * Allowed characters in userinfo as defined in RFC 3986. Includes "!$&'()*+,;=:".
 **/
#define G_URI_RESERVED_CHARS_ALLOWED_IN_USERINFO G_URI_RESERVED_CHARS_SUBCOMPONENT_DELIMITERS ":"

GLIB_AVAILABLE_IN_ALL
char *      g_uri_unescape_string  (const char *escaped_string,
                                    const char *illegal_characters);
GLIB_AVAILABLE_IN_ALL
char *      g_uri_unescape_segment (const char *escaped_string,
                                    const char *escaped_string_end,
                                    const char *illegal_characters);

GLIB_AVAILABLE_IN_ALL
char *      g_uri_parse_scheme     (const char *uri);
GLIB_AVAILABLE_IN_2_64
const char *g_uri_peek_scheme      (const char *uri);

GLIB_AVAILABLE_IN_ALL
char *      g_uri_escape_string    (const char *unescaped,
                                    const char *reserved_chars_allowed,
                                    gboolean    allow_utf8);

GLIB_AVAILABLE_IN_2_64
GBytes *    g_uri_unescape_bytes   (const char *escaped_string,
                                    gssize      length);
GLIB_AVAILABLE_IN_2_64
char *      g_uri_escape_bytes     (const guchar *unescaped,
                                    gsize         length,
                                    const char   *reserved_chars_allowed);

G_END_DECLS
