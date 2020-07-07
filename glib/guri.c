/* GLIB - Library of useful routines for C programming
 * Copyright © 2020 Red Hat, Inc.
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

#include "config.h"

#include <stdlib.h>
#include <string.h>

#include "glib.h"
#include "glibintl.h"
#include "guriprivate.h"

/**
 * SECTION:guri
 * @short_description: URI-handling utilities
 * @include: glib.h
 *
 * The #GUri type and related functions can be used to parse URIs into
 * their components, and build valid URIs from individual components.
 *
 * ## Parsing URIs
 *
 * The most minimalist APIs for parsing URIs are g_uri_split() and
 * g_uri_split_with_user(). These split a URI into its component
 * parts, and return the parts; the difference between the two is that
 * g_uri_split() treats the "userinfo" component of the URI as a
 * single element, while g_uri_split_with_user() can (depending on the
 * #GUriFlags you pass) treat it as containing a username, password,
 * and authentication parameters. Alternatively, g_uri_split_network()
 * can be used when you are only interested in the components that are
 * needed to initiate a network connection to the service (scheme,
 * host, and port).
 *
 * g_uri_parse() is similar to g_uri_split(), but instead of returning
 * individual strings, it returns a #GUri structure (and it requires
 * that the URI be an absolute URI).
 *
 * g_uri_resolve_relative() and g_uri_parse_relative() allow you to
 * resolve a relative URI relative to a base URI.
 * g_uri_resolve_relative() takes two strings and returns a string,
 * and g_uri_parse_relative() takes a #GUri and a string and returns a
 * #GUri.
 *
 * All of the parsing functions take a #GUriFlags argument describing
 * exactly how to parse the URI; see the documentation for that type
 * for more details on the specific flags that you can pass. If you
 * need to choose different flags based on the type of URI, you can
 * use g_uri_peek_scheme() on the URI string to check the scheme
 * first, and use that to decide what flags to parse it with.
 *
 * ## Building URIs
 *
 * g_uri_join() and g_uri_join_with_user() can be used to construct
 * valid URI strings from a set of component strings; they are the
 * inverse of g_uri_split() and g_uri_split_with_user().
 *
 * Similarly, g_uri_build() and g_uri_build_with_user() can be used to
 * construct a #GUri from a set of component strings.
 *
 * As with the parsing functions, the building functions take a
 * #GUriFlags argument; in particular, it is important to keep in mind
 * whether the URI components you are using have `%`-encoded
 * characters in them or not, and pass the appropriate flags
 * accordingly.
 *
 * ## `file://` URIs
 *
 * Note that Windows and Unix both define special rules for parsing
 * `file://` URIs (involving non-UTF-8 character sets on Unix, and the
 * interpretation of path separators on Windows). #GUri does not
 * implement these rules. Use g_filename_from_uri() and
 * g_filename_to_uri() if you want to properly convert between
 * `file://` URIs and local filenames.
 *
 * ## URI Equality
 *
 * Note that there is no `g_uri_equal ()` function, because comparing
 * URIs usefully requires scheme-specific knowledge that #GUri does
 * not have. For example, "`http://example.com/`" and
 * "`http://EXAMPLE.COM:80`" have exactly the same meaning according
 * to the HTTP specification, and "`data:,foo`" and
 * "`data:;base64,Zm9v`" resolve to the same thing according to the
 * `data:` URI specification.
 *
 * Since: 2.66
 */

/**
 * GUri:
 *
 * A parsed absolute URI.
 *
 * Since #GUri only represents absolute URIs, all #GUris will have a
 * URI scheme, so g_uri_get_scheme() will always return a non-%NULL
 * answer. Likewise, by definition, all URIs have a path component, so
 * g_uri_get_path() will always return non-%NULL (though it may return
 * the empty string).
 *
 * If the URI string has an "authority" component (that is, if the
 * scheme is followed by "`://`" rather than just "`:`"), then the
 * #GUri will contain a hostname, and possibly a port and "userinfo".
 * Additionally, depending on how the #GUri was constructed/parsed,
 * the userinfo may be split out into a username, password, and
 * additional authorization-related parameters.
 *
 * Normally, the components of a #GUri will have all `%`-encoded
 * characters decoded. However, if you construct/parse a #GUri with
 * %G_URI_FLAGS_ENCODED, then the `%`-encoding will be preserved instead in
 * the userinfo, path, and query fields (and in the host field if also
 * created with %G_URI_FLAGS_NON_DNS). In particular, this is necessary if
 * the URI may contain binary data or non-UTF-8 text, or if decoding
 * the components might change the interpretation of the URI.
 *
 * For example, with the encoded flag:
 *
 * |[<!-- language="C" -->
 *   GUri *uri = g_uri_parse ("http://host/path?query=http%3A%2F%2Fhost%2Fpath%3Fparam%3Dvalue", G_URI_FLAGS_ENCODED, &err);
 *   g_assert_cmpstr (g_uri_get_query (uri), ==, "query=http%3A%2F%2Fhost%2Fpath%3Fparam%3Dvalue");
 * ]|
 *
 * While the default `%`-decoding behaviour would give:
 *
 * |[<!-- language="C" -->
 *   GUri *uri = g_uri_parse ("http://host/path?query=http%3A%2F%2Fhost%2Fpath%3Fparam%3Dvalue", G_URI_FLAGS_NONE, &err);
 *   g_assert_cmpstr (g_uri_get_query (uri), ==, "query=http://host/path?param=value");
 * ]|
 *
 * During decoding, if an invalid UTF-8 string is encountered, parsing will fail
 * with an error indicating the bad string location:
 *
 * |[<!-- language="C" -->
 *   GUri *uri = g_uri_parse ("http://host/path?query=http%3A%2F%2Fhost%2Fpath%3Fbad%3D%00alue", G_URI_FLAGS_NONE, &err);
 *   g_assert_error(err, G_URI_ERROR, G_URI_ERROR_BAD_QUERY);
 * ]|
 *
 * You should pass %G_URI_FLAGS_ENCODED or %G_URI_FLAGS_ENCODED_QUERY if you
 * need to handle that case manually. In particular, if the query string
 * contains '=' characters that are '%'-encoded, you should let
 * g_uri_parse_params() do the decoding once of the query.
 *
 * #GUri is immutable once constructed, and can safely be accessed from
 * multiple threads. Its reference counting is atomic.
 *
 * Since: 2.66
 */
struct _GUri {
  gchar     *scheme;
  gchar     *userinfo;
  gchar     *host;
  gint       port;
  gchar     *path;
  gchar     *query;
  gchar     *fragment;

  gchar     *user;
  gchar     *password;
  gchar     *auth_params;

  GUriFlags  flags;
};

/**
 * g_uri_ref: (skip)
 * @uri: a #GUri
 *
 * Increments the reference count of @uri by one.
 *
 * Returns: @uri
 *
 * Since: 2.66
 */
GUri *
g_uri_ref (GUri *uri)
{
  g_return_val_if_fail (uri != NULL, NULL);

  return g_atomic_rc_box_acquire (uri);
}

static void g_uri_clear(GUri *uri)
{
  g_free (uri->scheme);
  g_free (uri->userinfo);
  g_free (uri->host);
  g_free (uri->path);
  g_free (uri->query);
  g_free (uri->fragment);
  g_free (uri->user);
  g_free (uri->password);
  g_free (uri->auth_params);
}

/**
 * g_uri_unref: (skip)
 * @uri: a #GUri
 *
 * Atomically decrements the reference count of @uri by one.
 *
 * When the reference count reaches zero, the resources allocated by
 * @uri are freed
 *
 * Since: 2.66
 */
void
g_uri_unref (GUri *uri)
{
  g_return_if_fail (uri != NULL);

  g_atomic_rc_box_release_full (uri, (GDestroyNotify)g_uri_clear);
}

static gboolean
g_uri_char_is_unreserved (gchar ch)
{
  if (g_ascii_isalnum (ch))
    return TRUE;
  return ch == '-' || ch == '.' || ch == '_' || ch == '~';
}

#define XDIGIT(c) ((c) <= '9' ? (c) - '0' : ((c) & 0x4F) - 'A' + 10)
#define HEXCHAR(s) ((XDIGIT (s[1]) << 4) + XDIGIT (s[2]))

static gssize
uri_decoder (gchar       **out,
             const gchar  *start,
             gsize         length,
             gboolean      just_normalize,
             gboolean      www_form,
             GUriFlags     flags,
             GUriError     parse_error,
             GError      **error)
{
  gchar *decoded, *d, c;
  const gchar *invalid, *s, *end;
  gssize len;

  if (!(flags & G_URI_FLAGS_ENCODED))
    just_normalize = FALSE;

  decoded = g_malloc (length + 1);
  for (s = start, end = s + length, d = decoded; s < end; s++)
    {
      if (*s == '%')
        {
          if (s + 2 >= end ||
              !g_ascii_isxdigit (s[1]) ||
              !g_ascii_isxdigit (s[2]))
            {
              /* % followed by non-hex or the end of the string; this is an error */
              if (flags & G_URI_FLAGS_PARSE_STRICT)
                {
                  g_set_error_literal (error, G_URI_ERROR, parse_error,
                                      /* xgettext: no-c-format */
                                       _("Invalid %-encoding in URI"));
                  g_free (decoded);
                  return -1;
                }

              /* In non-strict mode, just let it through; we *don't*
               * fix it to "%25", since that might change the way that
               * the URI's owner would interpret it.
               */
              *d++ = *s;
              continue;
            }

          c = HEXCHAR (s);
          if (just_normalize && !g_uri_char_is_unreserved (c))
            {
              /* Leave the % sequence there. */
              *d++ = *s;
            }
          else
            {
              *d++ = c;
              s += 2;
            }
        }
      else if (www_form && *s == '+')
        *d++ = ' ';
      else
        *d++ = *s;
    }
  *d = '\0';

  len = d - decoded;

  if (!(flags & G_URI_FLAGS_ENCODED) &&
      !g_utf8_validate (decoded, len, &invalid))
    {
      g_set_error_literal (error, G_URI_ERROR, parse_error,
                           _("Non-UTF-8 characters in URI"));
      g_free (decoded);
      return -1;
    }

  if (out)
    *out = g_steal_pointer (&decoded);

  g_free (decoded);
  return len;
}

static gboolean
uri_decode (gchar       **out,
            const gchar  *start,
            gsize         length,
            gboolean      www_form,
            GUriFlags     flags,
            GUriError     parse_error,
            GError      **error)
{
  return uri_decoder (out, start, length, FALSE, www_form, flags,
                      parse_error, error) != -1;
}

static gboolean
uri_normalize (gchar       **out,
               const gchar  *start,
               gsize         length,
               GUriFlags     flags,
               GUriError     parse_error,
               GError      **error)
{
  return uri_decoder (out, start, length, TRUE, FALSE, flags,
                      parse_error, error) != -1;
}

static gboolean
is_valid (guchar       c,
          const gchar *reserved_chars_allowed)
{
  if (g_uri_char_is_unreserved (c))
    return TRUE;

  if (reserved_chars_allowed && strchr (reserved_chars_allowed, c))
    return TRUE;

  return FALSE;
}

void
_uri_encoder (GString      *out,
              const guchar *start,
              gsize         length,
              const gchar  *reserved_chars_allowed,
              gboolean      allow_utf8)
{
  static const gchar hex[16] = "0123456789ABCDEF";
  const guchar *p = start;
  const guchar *end = p + length;

  while (p < end)
    {
      if (allow_utf8 && *p >= 0x80 &&
          g_utf8_get_char_validated ((gchar *)p, end - p) > 0)
        {
          gint len = g_utf8_skip [*p];
          g_string_append_len (out, (gchar *)p, len);
          p += len;
        }
      else if (is_valid (*p, reserved_chars_allowed))
        {
          g_string_append_c (out, *p);
          p++;
        }
      else
        {
          g_string_append_c (out, '%');
          g_string_append_c (out, hex[*p >> 4]);
          g_string_append_c (out, hex[*p & 0xf]);
          p++;
        }
    }
}

static gboolean
parse_host (const gchar  *start,
            gsize         length,
            GUriFlags     flags,
            gchar       **out,
            GError      **error)
{
  gchar *decoded, *host, *pct;
  gchar *addr = NULL;

  if (*start == '[')
    {
      if (start[length - 1] != ']')
        {
        bad_ipv6_literal:
          g_free (addr);
          g_set_error (error, G_URI_ERROR, G_URI_ERROR_BAD_HOST,
                       _("Invalid IPv6 address '%.*s' in URI"),
                       (gint)length, start);
          return FALSE;
        }

      addr = g_strndup (start + 1, length - 2);

      /* If there's an IPv6 scope id, ignore it for the moment. */
      pct = strchr (addr, '%');
      if (pct)
        *pct = '\0';

      /* addr must be an IPv6 address */
      if (!g_hostname_is_ip_address (addr) || !strchr (addr, ':'))
        goto bad_ipv6_literal;

      if (pct)
        {
          *pct = '%';
          if (strchr (pct + 1, '%'))
            goto bad_ipv6_literal;
          /* If the '%' is encoded as '%25' (which it should be), decode it */
          if (pct[1] == '2' && pct[2] == '5' && pct[3])
            memmove (pct + 1, pct + 3, strlen (pct + 3) + 1);
        }

      host = addr;
      goto ok;
    }

  if (g_ascii_isdigit (*start))
    {
      addr = g_strndup (start, length);
      if (g_hostname_is_ip_address (addr))
        {
          host = addr;
          goto ok;
        }
      g_free (addr);
    }

  if (flags & G_URI_FLAGS_NON_DNS)
    {
      if (!uri_normalize (&decoded, start, length, flags,
                          G_URI_ERROR_BAD_HOST, error))
        return FALSE;
      host = decoded;
      goto ok;
    }

  flags &= ~G_URI_FLAGS_ENCODED;
  if (!uri_decode (&decoded, start, length, FALSE, flags,
                   G_URI_ERROR_BAD_HOST, error))
    return FALSE;

  /* You're not allowed to %-encode an IP address, so if it wasn't
   * one before, it better not be one now.
   */
  if (g_hostname_is_ip_address (decoded))
    {
      g_free (decoded);
      g_set_error (error, G_URI_ERROR, G_URI_ERROR_BAD_HOST,
                   _("Illegal encoded IP address '%.*s' in URI"),
                   (gint)length, start);
      return FALSE;
    }

  if (g_hostname_is_non_ascii (decoded))
    {
      host = g_hostname_to_ascii (decoded);
      g_free (decoded);
    }
  else
    host = decoded;

 ok:
  if (out)
    *out = host;
  else
    g_free (host);
  return TRUE;
}

static gboolean
parse_port (const gchar  *start,
            gsize         length,
            gint         *out,
            GError      **error)
{
  gchar *end;
  gulong parsed_port;

  /* strtoul() allows leading + or -, so we have to check this first. */
  if (!g_ascii_isdigit (*start))
    {
      g_set_error (error, G_URI_ERROR, G_URI_ERROR_BAD_PORT,
                   _("Could not parse port '%.*s' in URI"),
                   (gint)length, start);
      return FALSE;
    }

  /* We know that *(start + length) is either '\0' or a non-numeric
   * character, so strtoul() won't scan beyond it.
   */
  parsed_port = strtoul (start, &end, 10);
  if (end != start + length)
    {
      g_set_error (error, G_URI_ERROR, G_URI_ERROR_BAD_PORT,
                   _("Could not parse port '%.*s' in URI"),
                   (gint)length, start);
      return FALSE;
    }
  else if (parsed_port > 65535)
    {
      g_set_error (error, G_URI_ERROR, G_URI_ERROR_BAD_PORT,
                   _("Port '%.*s' in URI is out of range"),
                   (gint)length, start);
      return FALSE;
    }

  if (out)
    *out = parsed_port;
  return TRUE;
}

static gboolean
parse_userinfo (const gchar  *start,
                gsize         length,
                GUriFlags     flags,
                gchar       **user,
                gchar       **password,
                gchar       **auth_params,
                GError      **error)
{
  const gchar *user_end = NULL, *password_end = NULL, *auth_params_end;

  auth_params_end = start + length;
  if (flags & G_URI_FLAGS_HAS_AUTH_PARAMS)
    password_end = memchr (start, ';', auth_params_end - start);
  if (!password_end)
    password_end = auth_params_end;
  if (flags & G_URI_FLAGS_HAS_PASSWORD)
    user_end = memchr (start, ':', password_end - start);
  if (!user_end)
    user_end = password_end;

  if (!uri_normalize (user, start, user_end - start, flags,
                      G_URI_ERROR_BAD_USER, error))
    return FALSE;

  if (*user_end == ':')
    {
      start = user_end + 1;
      if (!uri_normalize (password, start, password_end - start, flags,
                          G_URI_ERROR_BAD_PASSWORD, error))
        {
          if (user)
            g_clear_pointer (user, g_free);
          return FALSE;
        }
    }
  else if (password)
    *password = NULL;

  if (*password_end == ';')
    {
      start = password_end + 1;
      if (!uri_normalize (auth_params, start, auth_params_end - start, flags,
                          G_URI_ERROR_BAD_AUTH_PARAMS, error))
        {
          if (user)
            g_clear_pointer (user, g_free);
          if (password)
            g_clear_pointer (password, g_free);
          return FALSE;
        }
    }
  else if (auth_params)
    *auth_params = NULL;

  return TRUE;
}

static gchar *
uri_cleanup (const gchar *uri_string)
{
  GString *copy;
  const gchar *end;

  /* Skip leading whitespace */
  while (g_ascii_isspace (*uri_string))
    uri_string++;

  /* Ignore trailing whitespace */
  end = uri_string + strlen (uri_string);
  while (end > uri_string && g_ascii_isspace (*(end - 1)))
    end--;

  /* Copy the rest, encoding unencoded spaces and stripping other whitespace */
  copy = g_string_sized_new (end - uri_string);
  while (uri_string < end)
    {
      if (*uri_string == ' ')
        g_string_append (copy, "%20");
      else if (g_ascii_isspace (*uri_string))
        ;
      else
        g_string_append_c (copy, *uri_string);
      uri_string++;
    }

  return g_string_free (copy, FALSE);
}

static gboolean
g_uri_split_internal (const gchar  *uri_string,
                      GUriFlags     flags,
                      gchar       **scheme,
                      gchar       **userinfo,
                      gchar       **user,
                      gchar       **password,
                      gchar       **auth_params,
                      gchar       **host,
                      gint         *port,
                      gchar       **path,
                      gchar       **query,
                      gchar       **fragment,
                      GError      **error)
{
  const gchar *end, *colon, *at, *path_start, *semi, *question;
  const gchar *p, *bracket, *hostend;
  gchar *cleaned_uri_string = NULL;

  if (scheme)
    *scheme = NULL;
  if (userinfo)
    *userinfo = NULL;
  if (password)
    *password = NULL;
  if (auth_params)
    *auth_params = NULL;
  if (host)
    *host = NULL;
  if (port)
    *port = -1;
  if (path)
    *path = NULL;
  if (query)
    *query = NULL;
  if (fragment)
    *fragment = NULL;

  if (!(flags & G_URI_FLAGS_PARSE_STRICT) && strpbrk (uri_string, " \t\n\r"))
    {
      cleaned_uri_string = uri_cleanup (uri_string);
      uri_string = cleaned_uri_string;
    }

  /* Find scheme */
  p = uri_string;
  while (*p && (g_ascii_isalpha (*p) ||
               (p > uri_string && (g_ascii_isdigit (*p) ||
                                   *p == '.' || *p == '+' || *p == '-'))))
    p++;

  if (p > uri_string && *p == ':')
    {
      if (scheme)
        *scheme = g_ascii_strdown (uri_string, p - uri_string);
      p++;
    }
  else
    {
      if (scheme)
        *scheme = NULL;
      p = uri_string;
    }

  /* Check for authority */
  if (strncmp (p, "//", 2) == 0)
    {
      p += 2;

      path_start = p + strcspn (p, "/?#");
      at = memchr (p, '@', path_start - p);
      if (at)
        {
          if (!(flags & G_URI_FLAGS_PARSE_STRICT))
            {
              gchar *next_at;

              /* Any "@"s in the userinfo must be %-encoded, but
               * people get this wrong sometimes. Since "@"s in the
               * hostname are unlikely (and also wrong anyway), assume
               * that if there are extra "@"s, they belong in the
               * userinfo.
               */
              do
                {
                  next_at = memchr (at + 1, '@', path_start - (at + 1));
                  if (next_at)
                    at = next_at;
                }
              while (next_at);
            }

          if (user || password || auth_params ||
              (flags & (G_URI_FLAGS_HAS_PASSWORD|G_URI_FLAGS_HAS_AUTH_PARAMS)))
            {
              if (!parse_userinfo (p, at - p, flags,
                                   user, password, auth_params,
                                   error))
                goto fail;
            }

          if (!uri_normalize (userinfo, p, at - p, flags,
                              G_URI_ERROR_BAD_USER, error))
            goto fail;

          p = at + 1;
        }

      if (!(flags & G_URI_FLAGS_PARSE_STRICT))
        {
          semi = strchr (p, ';');
          if (semi && semi < path_start)
            {
              /* Technically, semicolons are allowed in the "host"
               * production, but no one ever does this, and some
               * schemes mistakenly use semicolon as a delimiter
               * marking the start of the path. We have to check this
               * after checking for userinfo though, because a
               * semicolon before the "@" must be part of the
               * userinfo.
               */
              path_start = semi;
            }
        }

      /* Find host and port. The host may be a bracket-delimited IPv6
       * address, in which case the colon delimiting the port must come
       * (immediately) after the close bracket.
       */
      if (*p == '[')
        {
          bracket = memchr (p, ']', path_start - p);
          if (bracket && *(bracket + 1) == ':')
            colon = bracket + 1;
          else
            colon = NULL;
        }
      else
        colon = memchr (p, ':', path_start - p);

      hostend = colon ? colon : path_start;
      if (!parse_host (p, hostend - p, flags, host, error))
        goto fail;

      if (colon && colon != path_start - 1)
        {
          p = colon + 1;
          if (!parse_port (p, path_start - p, port, error))
            goto fail;
        }

      p = path_start;
    }

  /* Find fragment. */
  end = p + strcspn (p, "#");
  if (*end == '#')
    {
      if (!uri_decode (fragment, end + 1, strlen (end + 1), FALSE, flags,
                       G_URI_ERROR_BAD_FRAGMENT, error))
        goto fail;
    }

  /* Find query */
  question = memchr (p, '?', end - p);
  if (question)
    {
      if (!uri_normalize (query, question + 1, end - (question + 1),
                          flags | (flags & G_URI_FLAGS_ENCODED_QUERY ? G_URI_FLAGS_ENCODED : 0),
                          G_URI_ERROR_BAD_QUERY, error))
        goto fail;
      end = question;
    }

  if (!uri_normalize (path, p, end - p, flags,
                      G_URI_ERROR_BAD_PATH, error))
    goto fail;

  g_free (cleaned_uri_string);
  return TRUE;

 fail:
  if (scheme)
    g_clear_pointer (scheme, g_free);
  if (userinfo)
    g_clear_pointer (userinfo, g_free);
  if (host)
    g_clear_pointer (host, g_free);
  if (port)
    *port = -1;
  if (path)
    g_clear_pointer (path, g_free);
  if (query)
    g_clear_pointer (query, g_free);
  if (fragment)
    g_clear_pointer (fragment, g_free);

  g_free (cleaned_uri_string);
  return FALSE;
}

/**
 * g_uri_split:
 * @uri_string: a string containing a relative or absolute URI
 * @flags: flags for parsing @uri_string
 * @scheme: (out) (nullable) (optional) (transfer full): on return, contains
 *    the scheme (converted to lowercase), or %NULL
 * @userinfo: (out) (nullable) (optional) (transfer full): on return, contains
 *    the userinfo, or %NULL
 * @host: (out) (nullable) (optional) (transfer full): on return, contains the
 *    host, or %NULL
 * @port: (out) (nullable) (optional) (transfer full): on return, contains the
 *    port, or -1
 * @path: (out) (nullable) (optional) (transfer full): on return, contains the
 *    path
 * @query: (out) (nullable) (optional) (transfer full): on return, contains the
 *    query, or %NULL
 * @fragment: (out) (nullable) (optional) (transfer full): on return, contains
 *    the fragment, or %NULL
 * @error: #GError for error reporting, or %NULL to ignore.
 *
 * Parses @uri_string (which can be an absolute or relative URI)
 * according to @flags, and returns the pieces. Any component that
 * doesn't appear in @uri_string will be returned as %NULL (but note
 * that all URIs always have a path component, though it may be the
 * empty string).
 *
 * If @flags contains %G_URI_FLAGS_ENCODED, then `%`-encoded characters in
 * @uri_string will remain encoded in the output strings. (If not,
 * then all such characters will be decoded.) Note that decoding will
 * only work if the URI components are ASCII or UTF-8, so you will
 * need to use %G_URI_FLAGS_ENCODED if they are not.
 *
 * Note that the %G_URI_FLAGS_HAS_PASSWORD and
 * %G_URI_FLAGS_HAS_AUTH_PARAMS @flags are ignored by g_uri_split(),
 * since it always returns only the full userinfo; use
 * g_uri_split_with_user() if you want it split up.
 *
 * Returns: (skip): %TRUE if @uri_string parsed successfully, %FALSE
 *   on error.
 *
 * Since: 2.66
 */
gboolean
g_uri_split (const gchar  *uri_string,
             GUriFlags     flags,
             gchar       **scheme,
             gchar       **userinfo,
             gchar       **host,
             gint         *port,
             gchar       **path,
             gchar       **query,
             gchar       **fragment,
             GError      **error)
{
  g_return_val_if_fail (uri_string != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return g_uri_split_internal (uri_string, flags,
                               scheme, userinfo, NULL, NULL, NULL,
                               host, port, path, query, fragment,
                               error);
}

/**
 * g_uri_split_with_user:
 * @uri_string: a string containing a relative or absolute URI
 * @flags: flags for parsing @uri_string
 * @scheme: (out) (nullable) (optional) (transfer full): on return, contains
 *    the scheme (converted to lowercase), or %NULL
 * @user: (out) (nullable) (optional) (transfer full): on return, contains
 *    the user, or %NULL
 * @password: (out) (nullable) (optional) (transfer full): on return, contains
 *    the password, or %NULL
 * @auth_params: (out) (nullable) (optional) (transfer full): on return, contains
 *    the auth_params, or %NULL
 * @host: (out) (nullable) (optional) (transfer full): on return, contains the
 *    host, or %NULL
 * @port: (out) (nullable) (optional) (transfer full): on return, contains the
 *    port, or -1
 * @path: (out) (nullable) (optional) (transfer full): on return, contains the
 *    path
 * @query: (out) (nullable) (optional) (transfer full): on return, contains the
 *    query, or %NULL
 * @fragment: (out) (nullable) (optional) (transfer full): on return, contains
 *    the fragment, or %NULL
 * @error: #GError for error reporting, or %NULL to ignore.
 *
 * Parses @uri_string (which can be an absolute or relative URI)
 * according to @flags, and returns the pieces. Any component that
 * doesn't appear in @uri_string will be returned as %NULL (but note
 * that all URIs always have a path component, though it may be the
 * empty string).
 *
 * See g_uri_split(), and the definition of #GUriFlags, for more
 * information on the effect of @flags. Note that @password will only
 * be parsed out if @flags contains %G_URI_FLAGS_HAS_PASSWORD, and
 * @auth_params will only be parsed out if @flags contains
 * %G_URI_FLAGS_HAS_AUTH_PARAMS.
 *
 * Returns: (skip): %TRUE if @uri_string parsed successfully, %FALSE
 *   on error.
 *
 * Since: 2.66
 */
gboolean
g_uri_split_with_user (const gchar  *uri_string,
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
                       GError      **error)
{
  g_return_val_if_fail (uri_string != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return g_uri_split_internal (uri_string, flags,
                               scheme, NULL, user, password, auth_params,
                               host, port, path, query, fragment,
                               error);
}


/**
 * g_uri_split_network:
 * @uri_string: a string containing a relative or absolute URI
 * @flags: flags for parsing @uri_string
 * @scheme: (out) (nullable) (optional) (transfer full): on return, contains
 *    the scheme (converted to lowercase), or %NULL
 * @host: (out) (nullable) (optional) (transfer full): on return, contains the
 *    host, or %NULL
 * @port: (out) (nullable) (optional) (transfer full): on return, contains the
 *    port, or -1
 * @error: #GError for error reporting, or %NULL to ignore.
 *
 * Parses @uri_string (which must be an absolute URI) according to
 * @flags, and returns the pieces relevant to connecting to a host.
 * See the documentation for g_uri_split() for more details; this is
 * mostly a wrapper around that function with simpler arguments.
 * However, it will return an error if @uri_string is a relative URI,
 * or does not contain a hostname component.
 *
 * Returns: (skip): %TRUE if @uri_string parsed successfully,
 *   %FALSE on error.
 *
 * Since: 2.66
 */
gboolean
g_uri_split_network (const gchar  *uri_string,
                     GUriFlags     flags,
                     gchar       **scheme,
                     gchar       **host,
                     gint         *port,
                     GError      **error)
{
  gchar *my_scheme, *my_host;

  g_return_val_if_fail (uri_string != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  if (!g_uri_split_internal (uri_string, flags,
                             &my_scheme, NULL, NULL, NULL, NULL,
                             &my_host, port, NULL, NULL, NULL,
                             error))
    return FALSE;

  if (!my_scheme || !my_host)
    {
      if (!my_scheme)
        {
          g_set_error (error, G_URI_ERROR, G_URI_ERROR_BAD_SCHEME,
                       _("URI '%s' is not an absolute URI"),
                       uri_string);
        }
      else
        {
          g_set_error (error, G_URI_ERROR, G_URI_ERROR_BAD_HOST,
                       _("URI '%s' has no host component"),
                       uri_string);
        }
      g_free (my_scheme);
      g_free (my_host);

      return FALSE;
    }

  if (scheme)
    *scheme = my_scheme;
  else
    g_free (my_scheme);
  if (host)
    *host = my_host;
  else
    g_free (my_host);
  return TRUE;
}

/**
 * g_uri_is_valid:
 * @uri_string: a string containing a relative or absolute URI
 * @flags: flags for parsing @uri_string
 * @error: #GError for error reporting, or %NULL to ignore.
 *
 * Parses @uri_string (which can be an absolute or relative URI)
 * according to @flags, to determine whether it is valid.
 *
 * See g_uri_split(), and the definition of #GUriFlags, for more
 * information on the effect of @flags.
 *
 * Returns: %TRUE if @uri_string parsed successfully, %FALSE on error.
 *
 * Since: 2.66
 */
gboolean
g_uri_is_valid (const gchar  *uri_string,
                GUriFlags     flags,
                GError      **error)
{
  g_return_val_if_fail (uri_string != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return g_uri_split_internal (uri_string, flags,
                               NULL, NULL, NULL, NULL, NULL,
                               NULL, NULL, NULL, NULL, NULL,
                               error);
}


/* This does the "Remove Dot Segments" algorithm from section 5.2.4 of
 * RFC 3986, except that @path is modified in place.
 */
static void
remove_dot_segments (gchar *path)
{
  gchar *p, *q;

  if (!*path)
    return;

  /* Remove "./" where "." is a complete segment. */
  for (p = path + 1; *p; )
    {
      if (*(p - 1) == '/' &&
          *p == '.' && *(p + 1) == '/')
        memmove (p, p + 2, strlen (p + 2) + 1);
      else
        p++;
    }
  /* Remove "." at end. */
  if (p > path + 2 &&
      *(p - 1) == '.' && *(p - 2) == '/')
    *(p - 1) = '\0';

  /* Remove "<segment>/../" where <segment> != ".." */
  for (p = path + 1; *p; )
    {
      if (!strncmp (p, "../", 3))
        {
          p += 3;
          continue;
        }
      q = strchr (p + 1, '/');
      if (!q)
        break;
      if (strncmp (q, "/../", 4) != 0)
        {
          p = q + 1;
          continue;
        }
      memmove (p, q + 4, strlen (q + 4) + 1);
      p = path + 1;
    }
  /* Remove "<segment>/.." at end where <segment> != ".." */
  q = strrchr (path, '/');
  if (q && q != path && !strcmp (q, "/.."))
    {
      p = q - 1;
      while (p > path && *p != '/')
        p--;
      if (strncmp (p, "/../", 4) != 0)
        *(p + 1) = 0;
    }

  /* Remove extraneous initial "/.."s */
  while (!strncmp (path, "/../", 4))
    memmove (path, path + 3, strlen (path) - 2);
  if (!strcmp (path, "/.."))
    path[1] = '\0';
}

/**
 * g_uri_parse:
 * @uri_string: a string representing an absolute URI
 * @flags: flags describing how to parse @uri_string
 * @error: #GError for error reporting, or %NULL to ignore.
 *
 * Parses @uri_string according to @flags. If the result is not a
 * valid absolute URI, it will be discarded, and an error returned.
 *
 * Return value: (transfer full): a new #GUri.
 *
 * Since: 2.66
 */
GUri *
g_uri_parse (const gchar  *uri_string,
             GUriFlags     flags,
             GError      **error)
{
  g_return_val_if_fail (uri_string != NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  return g_uri_parse_relative (NULL, uri_string, flags, error);
}

/**
 * g_uri_parse_relative:
 * @base_uri: (nullable): a base URI
 * @uri_string: a string representing a relative or absolute URI
 * @flags: flags describing how to parse @uri_string
 * @error: #GError for error reporting, or %NULL to ignore.
 *
 * Parses @uri_string according to @flags and, if it is a relative
 * URI, resolves it relative to @base_uri. If the result is not a
 * valid absolute URI, it will be discarded, and an error returned.
 *
 * Return value: (transfer full): a new #GUri.
 *
 * Since: 2.66
 */
GUri *
g_uri_parse_relative (GUri         *base_uri,
                      const gchar  *uri_string,
                      GUriFlags     flags,
                      GError      **error)
{
  GUri *uri = NULL;

  g_return_val_if_fail (uri_string != NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);
  g_return_val_if_fail (base_uri == NULL || base_uri->scheme != NULL, NULL);

  uri = g_atomic_rc_box_new0 (GUri);
  uri->flags = flags;

  if (!g_uri_split_internal (uri_string, flags,
                             &uri->scheme, &uri->userinfo,
                             &uri->user, &uri->password, &uri->auth_params,
                             &uri->host, &uri->port,
                             &uri->path, &uri->query, &uri->fragment,
                             error))
    goto fail;

  if (!uri->scheme && !base_uri)
    {
      g_set_error_literal (error, G_URI_ERROR, G_URI_ERROR_MISC,
                           _("URI is not absolute, and no base URI was provided"));
      goto fail;
    }

  if (base_uri)
    {
      /* This is section 5.2.2 of RFC 3986, except that we're doing
       * it in place in @uri rather than copying from R to T.
       */
      if (uri->scheme)
        remove_dot_segments (uri->path);
      else
        {
          uri->scheme = g_strdup (base_uri->scheme);
          if (uri->host)
            remove_dot_segments (uri->path);
          else
            {
              if (!*uri->path)
                {
                  g_free (uri->path);
                  uri->path = g_strdup (base_uri->path);
                  if (!uri->query)
                    uri->query = g_strdup (base_uri->query);
                }
              else
                {
                  if (*uri->path == '/')
                    remove_dot_segments (uri->path);
                  else
                    {
                      gchar *newpath, *last;

                      last = strrchr (base_uri->path, '/');
                      if (last)
                        {
                          newpath = g_strdup_printf ("%.*s/%s",
                                                     (gint)(last - base_uri->path),
                                                     base_uri->path,
                                                     uri->path);
                        }
                      else
                        newpath = g_strdup_printf ("/%s", uri->path);

                      g_free (uri->path);
                      uri->path = newpath;

                      remove_dot_segments (uri->path);
                    }
                }

              uri->userinfo = g_strdup (base_uri->userinfo);
              uri->user = g_strdup (base_uri->user);
              uri->password = g_strdup (base_uri->password);
              uri->auth_params = g_strdup (base_uri->auth_params);
              uri->host = g_strdup (base_uri->host);
              uri->port = base_uri->port;
            }
        }
    }

  return uri;

 fail:
  if (uri)
    g_uri_unref (uri);
  return NULL;
}

/**
 * g_uri_resolve_relative:
 * @base_uri_string: (nullable): a string representing a base URI
 * @uri_string: a string representing a relative or absolute URI
 * @flags: flags describing how to parse @uri_string
 * @error: #GError for error reporting, or %NULL to ignore.
 *
 * Parses @uri_string according to @flags and, if it is a relative
 * URI, resolves it relative to @base_uri_string. If the result is not
 * a valid absolute URI, it will be discarded, and an error returned.
 *
 * (If @base_uri_string is %NULL, this just returns @uri_string, or
 * %NULL if @uri_string is invalid or not absolute.)
 *
 * Return value: the resolved URI string.
 *
 * Since: 2.66
 */
gchar *
g_uri_resolve_relative (const gchar  *base_uri_string,
                        const gchar  *uri_string,
                        GUriFlags     flags,
                        GError      **error)
{
  GUri *base_uri, *resolved_uri;
  gchar *resolved_uri_string;

  g_return_val_if_fail (uri_string != NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  flags |= G_URI_FLAGS_ENCODED;

  if (base_uri_string)
    {
      base_uri = g_uri_parse (base_uri_string, flags, error);
      if (!base_uri)
        return NULL;
    }
  else
    base_uri = NULL;

  resolved_uri = g_uri_parse_relative (base_uri, uri_string, flags, error);
  if (base_uri)
    g_uri_unref (base_uri);
  if (!resolved_uri)
    return NULL;

  resolved_uri_string = g_uri_to_string (resolved_uri);
  g_uri_unref (resolved_uri);
  return resolved_uri_string;
}

/* userinfo as a whole can contain sub-delims + ":", but split-out
 * user can't contain ":" or ";", and split-out password can't contain
 * ";".
 */
#define USERINFO_ALLOWED_CHARS G_URI_RESERVED_CHARS_ALLOWED_IN_USERINFO
#define USER_ALLOWED_CHARS "!$&'()*+,="
#define PASSWORD_ALLOWED_CHARS "!$&'()*+,=:"
#define AUTH_PARAMS_ALLOWED_CHARS USERINFO_ALLOWED_CHARS
#define IP_ADDR_ALLOWED_CHARS ":"
#define HOST_ALLOWED_CHARS G_URI_RESERVED_CHARS_SUBCOMPONENT_DELIMITERS
#define PATH_ALLOWED_CHARS G_URI_RESERVED_CHARS_ALLOWED_IN_PATH
#define QUERY_ALLOWED_CHARS G_URI_RESERVED_CHARS_ALLOWED_IN_PATH "?"
#define FRAGMENT_ALLOWED_CHARS G_URI_RESERVED_CHARS_ALLOWED_IN_PATH "?"

static gchar *
g_uri_join_internal (GUriFlags    flags,
                     const gchar *scheme,
                     const gchar *user,
                     const gchar *password,
                     const gchar *auth_params,
                     const gchar *host,
                     gint         port,
                     const gchar *path,
                     const gchar *query,
                     const gchar *fragment)
{
  gboolean encoded = (flags & G_URI_FLAGS_ENCODED);
  GString *str;

  str = g_string_new (scheme);
  g_string_append_c (str, ':');

  if (host)
    {
      g_string_append (str, "//");

      if (user)
        {
          if (encoded)
            g_string_append (str, user);
          else
            {
              /* Encode ':' and ';' regardless of whether we have a
               * password or auth params, since it may be parsed later
               * under the assumption that it does.
               */
              g_string_append_uri_escaped (str, user, USER_ALLOWED_CHARS, TRUE);
            }

          if (password)
            {
              g_string_append_c (str, ':');
              if (encoded)
                g_string_append (str, password);
              else
                g_string_append_uri_escaped (str, password,
                                             PASSWORD_ALLOWED_CHARS, TRUE);
            }

          if (auth_params)
            {
              g_string_append_c (str, ';');
              if (encoded)
                g_string_append (str, auth_params);
              else
                g_string_append_uri_escaped (str, auth_params,
                                             AUTH_PARAMS_ALLOWED_CHARS, TRUE);
            }

          g_string_append_c (str, '@');
        }

      if (strchr (host, ':'))
        {
          g_string_append_c (str, '[');
          if (encoded)
            g_string_append (str, host);
          else
            g_string_append_uri_escaped (str, host, IP_ADDR_ALLOWED_CHARS, TRUE);
          g_string_append_c (str, ']');
        }
      else
        {
          if (encoded)
            g_string_append (str, host);
          else
            g_string_append_uri_escaped (str, host, HOST_ALLOWED_CHARS, TRUE);
        }

      if (port != -1)
        g_string_append_printf (str, ":%d", port);
    }

  if (encoded)
    g_string_append (str, path);
  else
    g_string_append_uri_escaped (str, path, PATH_ALLOWED_CHARS, TRUE);

  if (query)
    {
      g_string_append_c (str, '?');
      if (encoded || flags & G_URI_FLAGS_ENCODED_QUERY)
        g_string_append (str, query);
      else
        g_string_append_uri_escaped (str, query, QUERY_ALLOWED_CHARS, TRUE);
    }
  if (fragment)
    {
      g_string_append_c (str, '#');
      if (encoded)
        g_string_append (str, fragment);
      else
        g_string_append_uri_escaped (str, fragment, FRAGMENT_ALLOWED_CHARS, TRUE);
    }

  return g_string_free (str, FALSE);
}

/**
 * g_uri_join:
 * @flags: flags describing how to build the URI string
 * @scheme: the URI scheme
 * @userinfo: (nullable): the userinfo component, or %NULL
 * @host: (nullable): the host component, or %NULL
 * @port: the port, or -1
 * @path: the path component
 * @query: (nullable): the query component, or %NULL
 * @fragment: (nullable): the fragment, or %NULL
 *
 * Joins the given components together according to @flags to create
 * a complete URI string. At least @scheme must be specified, and
 * @path may not be %NULL (though it may be "").
 *
 * See also g_uri_join_with_user(), which allows specifying the
 * components of the "userinfo" separately.
 *
 * Return value: a URI string
 *
 * Since: 2.66
 */
gchar *
g_uri_join (GUriFlags    flags,
            const gchar *scheme,
            const gchar *userinfo,
            const gchar *host,
            gint         port,
            const gchar *path,
            const gchar *query,
            const gchar *fragment)
{
  g_return_val_if_fail (scheme != NULL, NULL);
  g_return_val_if_fail (port >= -1 && port <= 65535, NULL);
  g_return_val_if_fail (path != NULL, NULL);

  return g_uri_join_internal (flags,
                              scheme,
                              userinfo, NULL, NULL,
                              host,
                              port,
                              path,
                              query,
                              fragment);
}

/**
 * g_uri_join_with_user:
 * @flags: flags describing how to build the URI string
 * @scheme: the URI scheme
 * @user: (nullable): the user component of the userinfo, or %NULL
 * @password: (nullable): the password component of the userinfo, or
 *   %NULL
 * @auth_params: (nullable): the auth params of the userinfo, or
 *   %NULL
 * @host: (nullable): the host component, or %NULL
 * @port: the port, or -1
 * @path: the path component
 * @query: (nullable): the query component, or %NULL
 * @fragment: (nullable): the fragment, or %NULL
 *
 * Joins the given components together according to @flags to create
 * a complete URI string. At least @scheme must be specified, and
 * @path may not be %NULL (though it may be "").
 *
 * In constrast to g_uri_join(), this allows specifying the components
 * of the "userinfo" separately.
 *
 * Return value: a URI string
 *
 * Since: 2.66
 */
gchar *
g_uri_join_with_user (GUriFlags    flags,
                      const gchar *scheme,
                      const gchar *user,
                      const gchar *password,
                      const gchar *auth_params,
                      const gchar *host,
                      gint         port,
                      const gchar *path,
                      const gchar *query,
                      const gchar *fragment)
{
  g_return_val_if_fail (scheme != NULL, NULL);
  g_return_val_if_fail (port >= -1 && port <= 65535, NULL);
  g_return_val_if_fail (path != NULL, NULL);

  return g_uri_join_internal (flags,
                              scheme,
                              user, password, auth_params,
                              host,
                              port,
                              path,
                              query,
                              fragment);
}

/**
 * g_uri_build:
 * @flags: flags describing how to build the #GUri
 * @scheme: the URI scheme
 * @userinfo: (nullable): the userinfo component, or %NULL
 * @host: (nullable): the host component, or %NULL
 * @port: the port, or -1
 * @path: the path component
 * @query: (nullable): the query component, or %NULL
 * @fragment: (nullable): the fragment, or %NULL
 *
 * Creates a new #GUri from the given components according to @flags.
 *
 * See also g_uri_build_with_user(), which allows specifying the
 * components of the "userinfo" separately.
 *
 * Return value: (transfer full): a new #GUri
 *
 * Since: 2.66
 */
GUri *
g_uri_build (GUriFlags    flags,
             const gchar *scheme,
             const gchar *userinfo,
             const gchar *host,
             gint         port,
             const gchar *path,
             const gchar *query,
             const gchar *fragment)
{
  GUri *uri;

  g_return_val_if_fail (scheme != NULL, NULL);
  g_return_val_if_fail (port >= -1 && port <= 65535, NULL);
  g_return_val_if_fail (path != NULL, NULL);

  uri = g_atomic_rc_box_new0 (GUri);
  uri->flags = flags;
  uri->scheme = g_ascii_strdown (scheme, -1);
  uri->userinfo = g_strdup (userinfo);
  uri->host = g_strdup (host);
  uri->port = port;
  uri->path = g_strdup (path);
  uri->query = g_strdup (query);
  uri->fragment = g_strdup (fragment);

  return uri;
}

/**
 * g_uri_build_with_user:
 * @flags: flags describing how to build the #GUri
 * @scheme: the URI scheme
 * @user: (nullable): the user component of the userinfo, or %NULL
 * @password: (nullable): the password component of the userinfo, or %NULL
 * @auth_params: (nullable): the auth params of the userinfo, or %NULL
 * @host: (nullable): the host component, or %NULL
 * @port: the port, or -1
 * @path: the path component
 * @query: (nullable): the query component, or %NULL
 * @fragment: (nullable): the fragment, or %NULL
 *
 * Creates a new #GUri from the given components according to @flags.

 * In constrast to g_uri_build(), this allows specifying the components
 * of the "userinfo" field separately. Note that @user must be non-%NULL
 * if either @password or @auth_params is non-%NULL.
 *
 * Return value: (transfer full): a new #GUri
 *
 * Since: 2.66
 */
GUri *
g_uri_build_with_user (GUriFlags    flags,
                       const gchar *scheme,
                       const gchar *user,
                       const gchar *password,
                       const gchar *auth_params,
                       const gchar *host,
                       gint         port,
                       const gchar *path,
                       const gchar *query,
                       const gchar *fragment)
{
  GUri *uri;
  GString *userinfo;

  g_return_val_if_fail (scheme != NULL, NULL);
  g_return_val_if_fail (password == NULL || user != NULL, NULL);
  g_return_val_if_fail (auth_params == NULL || user != NULL, NULL);
  g_return_val_if_fail (port >= -1 && port <= 65535, NULL);
  g_return_val_if_fail (path != NULL, NULL);

  uri = g_atomic_rc_box_new0 (GUri);
  uri->flags = flags;
  uri->scheme = g_ascii_strdown (scheme, -1);
  uri->user = g_strdup (user);
  uri->password = g_strdup (password);
  uri->auth_params = g_strdup (auth_params);
  uri->host = g_strdup (host);
  uri->port = port;
  uri->path = g_strdup (path);
  uri->query = g_strdup (query);
  uri->fragment = g_strdup (fragment);

  if (user)
    {
      userinfo = g_string_new (NULL);
      if (flags & G_URI_FLAGS_ENCODED)
        g_string_append (userinfo, uri->user);
      else
        g_string_append_uri_escaped (userinfo, uri->user, USER_ALLOWED_CHARS, TRUE);
      if (password)
        {
          g_string_append_c (userinfo, ':');
          if (flags & G_URI_FLAGS_ENCODED)
            g_string_append (userinfo, uri->password);
          else
            g_string_append_uri_escaped (userinfo, uri->password,
                                         PASSWORD_ALLOWED_CHARS, TRUE);
        }
      if (auth_params)
        {
          g_string_append_c (userinfo, ';');
          if (flags & G_URI_FLAGS_ENCODED)
            g_string_append (userinfo, uri->auth_params);
          else
            g_string_append_uri_escaped (userinfo,
                                         uri->auth_params, AUTH_PARAMS_ALLOWED_CHARS, TRUE);
        }
      uri->userinfo = g_string_free (userinfo, FALSE);
    }
  else
    uri->userinfo = NULL;

  return uri;
}

/**
 * g_uri_to_string:
 * @uri: a #GUri
 *
 * Returns a string representing @uri.
 *
 * This is not guaranteed to return a string which is identical to the
 * string that @uri was parsed from. However, if the source URI was
 * syntactically correct (according to RFC 3986), and it was parsed
 * with %G_URI_FLAGS_ENCODED, then g_uri_to_string() is guaranteed to return
 * a string which is at least semantically equivalent to the source
 * URI (according to RFC 3986).
 *
 * Return value: a string representing @uri, which the caller must
 * free.
 *
 * Since: 2.66
 */
gchar *
g_uri_to_string (GUri *uri)
{
  g_return_val_if_fail (uri != NULL, NULL);

  return g_uri_to_string_partial (uri, 0);
}

/**
 * g_uri_to_string_partial:
 * @uri: a #GUri
 * @flags: flags describing what parts of @uri to hide
 *
 * Returns a string representing @uri, subject to the options in
 * @flags. See g_uri_to_string() and #GUriHideFlags for more details.

 * Return value: a string representing @uri, which the caller must
 * free.
 *
 * Since: 2.66
 */
gchar *
g_uri_to_string_partial (GUri          *uri,
                         GUriHideFlags  flags)
{
  gboolean hide_user = (flags & G_URI_HIDE_USERINFO);
  gboolean hide_password = (flags & (G_URI_HIDE_USERINFO | G_URI_HIDE_PASSWORD));
  gboolean hide_auth_params = (flags & (G_URI_HIDE_USERINFO | G_URI_HIDE_AUTH_PARAMS));
  gboolean hide_fragment = (flags & G_URI_HIDE_FRAGMENT);

  g_return_val_if_fail (uri != NULL, NULL);

  if (uri->flags & (G_URI_FLAGS_HAS_PASSWORD | G_URI_FLAGS_HAS_AUTH_PARAMS))
    {
      return g_uri_join_with_user (uri->flags,
                                   uri->scheme,
                                   hide_user ? NULL : uri->user,
                                   hide_password ? NULL : uri->password,
                                   hide_auth_params ? NULL : uri->auth_params,
                                   uri->host,
                                   uri->port,
                                   uri->path,
                                   uri->query,
                                   hide_fragment ? NULL : uri->fragment);
    }

  return g_uri_join (uri->flags,
                     uri->scheme,
                     hide_user ? NULL : uri->userinfo,
                     uri->host,
                     uri->port,
                     uri->path,
                     uri->query,
                     hide_fragment ? NULL : uri->fragment);
}

/* This is just a copy of g_str_hash() with g_ascii_toupper() added */
static guint
str_ascii_case_hash (gconstpointer v)
{
  const signed char *p;
  guint32 h = 5381;

  for (p = v; *p != '\0'; p++)
    h = (h << 5) + h + g_ascii_toupper (*p);

  return h;
}

static gboolean
str_ascii_case_equal (gconstpointer v1,
                      gconstpointer v2)
{
  const gchar *string1 = v1;
  const gchar *string2 = v2;

  return g_ascii_strcasecmp (string1, string2) == 0;
}

/**
 * g_uri_parse_params:
 * @params: a `%`-encoded string containing "attribute=value"
 *   parameters
 * @length: the length of @params, or -1 if it is NUL-terminated
 * @separators: the separator byte character set between parameters. (usually
 *   "&", but sometimes ";" or both "&;"). Note that this function works on
 *   bytes not characters, so it can't be used to delimit UTF-8 strings for
 *   anything but ASCII characters. You may pass an empty set, in which case
 *   no splitting will occur.
 * @flags: flags to modify the way the parameters are handled.
 * @error: #GError for error reporting, or %NULL to ignore.
 *
 * Many URI schemes include one or more attribute/value pairs as part of the URI
 * value. This method can be used to parse them into a hash table.
 *
 * The @params string is assumed to still be `%`-encoded, but the returned
 * values will be fully decoded. (Thus it is possible that the returned values
 * may contain '=' or @separators, if the value was encoded in the input.)
 * Invalid `%`-encoding is treated as with the non-%G_URI_FLAGS_PARSE_STRICT
 * rules for g_uri_parse(). (However, if @params is the path or query string
 * from a #GUri that was parsed with %G_URI_FLAGS_PARSE_STRICT and
 * %G_URI_FLAGS_ENCODED, then you already know that it does not contain any
 * invalid encoding.)
 *
 * Return value: (transfer full) (element-type utf8 utf8): a hash table of
 * attribute/value pairs. Both names and values will be fully-decoded. If
 * @params cannot be parsed (eg, it contains two @separators characters in a
 * row), then %NULL is returned.
 *
 * Since: 2.66
 */
GHashTable *
g_uri_parse_params (const gchar     *params,
                    gssize           length,
                    const gchar     *separators,
                    GUriParamsFlags  flags,
                    GError         **error)
{
  GHashTable *hash;
  const gchar *end, *attr, *attr_end, *value, *value_end, *s;
  gchar *decoded_attr, *decoded_value;
  guint8 sep_table[256]; /* 1 = index is a separator; 0 otherwise */
  gboolean www_form = flags & G_URI_PARAMS_WWW_FORM;

  g_return_val_if_fail (length == 0 || params != NULL, NULL);
  g_return_val_if_fail (length >= -1, NULL);
  g_return_val_if_fail (separators != NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  if (flags & G_URI_PARAMS_CASE_INSENSITIVE)
    {
      hash = g_hash_table_new_full (str_ascii_case_hash,
                                    str_ascii_case_equal,
                                    g_free, g_free);
    }
  else
    {
      hash = g_hash_table_new_full (g_str_hash, g_str_equal,
                                    g_free, g_free);
    }

  if (length == -1)
    end = params + strlen (params);
  else
    end = params + length;

  memset (sep_table, FALSE, sizeof (sep_table));
  for (s = separators; *s != '\0'; ++s)
    sep_table[*(guchar *)s] = TRUE;

  attr = params;
  while (attr < end)
    {
      /* Check if each character in @attr is a separator, by indexing by the
       * character value into the @sep_table, which has value 1 stored at an
       * index if that index is a separator. */
      for (value_end = attr; value_end < end; value_end++)
        if (sep_table[*(guchar *)value_end])
          break;

      attr_end = memchr (attr, '=', value_end - attr);
      if (!attr_end)
        {
          g_hash_table_destroy (hash);
          g_set_error_literal (error, G_URI_ERROR, G_URI_ERROR_MISC,
                               _("Missing '=' and parameter value"));
          return NULL;
        }
      if (!uri_decode (&decoded_attr, attr, attr_end - attr,
                       www_form, G_URI_FLAGS_NONE, G_URI_ERROR_MISC, error))
        {
          g_hash_table_destroy (hash);
          return NULL;
        }

      value = attr_end + 1;
      if (!uri_decode (&decoded_value, value, value_end - value,
                       www_form, G_URI_FLAGS_NONE, G_URI_ERROR_MISC, error))
        {
          g_free (decoded_attr);
          g_hash_table_destroy (hash);
          return NULL;
        }

      g_hash_table_insert (hash, decoded_attr, decoded_value);
      attr = value_end + 1;
    }

  return hash;
}

/**
 * g_uri_get_scheme:
 * @uri: a #GUri
 *
 * Gets @uri's scheme. Note that this will always be all-lowercase,
 * regardless of the string or strings that @uri was created from.
 *
 * Return value: @uri's scheme.
 *
 * Since: 2.66
 */
const gchar *
g_uri_get_scheme (GUri *uri)
{
  g_return_val_if_fail (uri != NULL, NULL);

  return uri->scheme;
}

/**
 * g_uri_get_userinfo:
 * @uri: a #GUri
 *
 * Gets @uri's userinfo, which may contain `%`-encoding, depending on
 * the flags with which @uri was created.
 *
 * Return value: @uri's userinfo.
 *
 * Since: 2.66
 */
const gchar *
g_uri_get_userinfo (GUri *uri)
{
  g_return_val_if_fail (uri != NULL, NULL);

  return uri->userinfo;
}

/**
 * g_uri_get_user:
 * @uri: a #GUri
 *
 * Gets the "username" component of @uri's userinfo, which may contain
 * `%`-encoding, depending on the flags with which @uri was created.
 * If @uri was not created with %G_URI_FLAGS_HAS_PASSWORD or
 * %G_URI_FLAGS_HAS_AUTH_PARAMS, this is the same as g_uri_get_userinfo().
 *
 * Return value: @uri's user.
 *
 * Since: 2.66
 */
const gchar *
g_uri_get_user (GUri *uri)
{
  g_return_val_if_fail (uri != NULL, NULL);

  return uri->user;
}

/**
 * g_uri_get_password:
 * @uri: a #GUri
 *
 * Gets @uri's password, which may contain `%`-encoding, depending on
 * the flags with which @uri was created. (If @uri was not created
 * with %G_URI_FLAGS_HAS_PASSWORD then this will be %NULL.)
 *
 * Return value: @uri's password.
 *
 * Since: 2.66
 */
const gchar *
g_uri_get_password (GUri *uri)
{
  g_return_val_if_fail (uri != NULL, NULL);

  return uri->password;
}

/**
 * g_uri_get_auth_params:
 * @uri: a #GUri
 *
 * Gets @uri's authentication parameters, which may contain
 * `%`-encoding, depending on the flags with which @uri was created.
 * (If @uri was not created with %G_URI_FLAGS_HAS_AUTH_PARAMS then this will
 * be %NULL.)
 *
 * Depending on the URI scheme, g_uri_parse_params() may be useful for
 * further parsing this information.
 *
 * Return value: @uri's authentication parameters.
 *
 * Since: 2.66
 */
const gchar *
g_uri_get_auth_params (GUri *uri)
{
  g_return_val_if_fail (uri != NULL, NULL);

  return uri->auth_params;
}

/**
 * g_uri_get_host:
 * @uri: a #GUri
 *
 * Gets @uri's host. This will never have `%`-encoded characters,
 * unless it is non-UTF-8 (which can only be the case if @uri was
 * created with %G_URI_FLAGS_NON_DNS).
 *
 * If @uri contained an IPv6 address literal, this value will be just
 * that address, without the brackets around it that are necessary in
 * the string form of the URI. Note that in this case there may also
 * be a scope ID attached to the address. Eg, "`fe80::1234%``em1`" (or
 * "`fe80::1234%``25em1" if the string is still encoded).
 *
 * Return value: @uri's host.
 *
 * Since: 2.66
 */
const gchar *
g_uri_get_host (GUri *uri)
{
  g_return_val_if_fail (uri != NULL, NULL);

  return uri->host;
}

/**
 * g_uri_get_port:
 * @uri: a #GUri
 *
 * Gets @uri's port.
 *
 * Return value: @uri's port, or -1 if no port was specified.
 *
 * Since: 2.66
 */
gint
g_uri_get_port (GUri *uri)
{
  g_return_val_if_fail (uri != NULL, -1);

  return uri->port;
}

/**
 * g_uri_get_path:
 * @uri: a #GUri
 *
 * Gets @uri's path, which may contain `%`-encoding, depending on the
 * flags with which @uri was created.
 *
 * Return value: @uri's path.
 *
 * Since: 2.66
 */
const gchar *
g_uri_get_path (GUri *uri)
{
  g_return_val_if_fail (uri != NULL, NULL);

  return uri->path;
}

/**
 * g_uri_get_query:
 * @uri: a #GUri
 *
 * Gets @uri's query, which may contain `%`-encoding, depending on the
 * flags with which @uri was created.
 *
 * For queries consisting of a series of "`name=value`" parameters,
 * g_uri_parse_params() may be useful.
 *
 * Return value: @uri's query.
 *
 * Since: 2.66
 */
const gchar *
g_uri_get_query (GUri *uri)
{
  g_return_val_if_fail (uri != NULL, NULL);

  return uri->query;
}

/**
 * g_uri_get_fragment:
 * @uri: a #GUri
 *
 * Gets @uri's fragment, which may contain `%`-encoding, depending on
 * the flags with which @uri was created.
 *
 * Return value: @uri's fragment.
 *
 * Since: 2.66
 */
const gchar *
g_uri_get_fragment (GUri *uri)
{
  g_return_val_if_fail (uri != NULL, NULL);

  return uri->fragment;
}


/**
 * g_uri_get_flags:
 * @uri: a #GUri
 *
 * Gets @uri's flags set upon construction.
 *
 * Return value: @uri's flags.
 *
 * Since: 2.66
 **/
GUriFlags
g_uri_get_flags (GUri *uri)
{
  g_return_val_if_fail (uri != NULL, 0);

  return uri->flags;
}

/**
 * g_uri_unescape_segment:
 * @escaped_string: (nullable): A string, may be %NULL
 * @escaped_string_end: (nullable): Pointer to end of @escaped_string,
 *   may be %NULL
 * @illegal_characters: (nullable): An optional string of illegal
 *   characters not to be allowed, may be %NULL
 *
 * Unescapes a segment of an escaped string.
 *
 * If any of the characters in @illegal_characters or the NUL
 * character appears as an escaped character in @escaped_string, then
 * that is an error and %NULL will be returned. This is useful if you
 * want to avoid for instance having a slash being expanded in an
 * escaped path element, which might confuse pathname handling.
 *
 * Returns: an unescaped version of @escaped_string or %NULL on error.
 * The returned string should be freed when no longer needed.  As a
 * special case if %NULL is given for @escaped_string, this function
 * will return %NULL.
 *
 * Since: 2.16
 **/
gchar *
g_uri_unescape_segment (const gchar *escaped_string,
                        const gchar *escaped_string_end,
                        const gchar *illegal_characters)
{
  gchar *unescaped, *p;
  gsize length;

  if (!escaped_string)
    return NULL;

  if (escaped_string_end)
    length = escaped_string_end - escaped_string;
  else
    length = strlen (escaped_string);

  if (!uri_decode (&unescaped,
                   escaped_string, length,
                   FALSE,
                   G_URI_FLAGS_PARSE_STRICT,
                   0, NULL))
    return NULL;

  if (illegal_characters)
    {
      for (p = unescaped; *p; p++)
        {
          if (strchr (illegal_characters, *p))
            {
              g_free (unescaped);
              return NULL;
            }
        }
    }

  return unescaped;
}

/**
 * g_uri_unescape_string:
 * @escaped_string: an escaped string to be unescaped.
 * @illegal_characters: (nullable): a string of illegal characters
 *   not to be allowed, or %NULL.
 *
 * Unescapes a whole escaped string.
 *
 * If any of the characters in @illegal_characters or the NUL
 * character appears as an escaped character in @escaped_string, then
 * that is an error and %NULL will be returned. This is useful if you
 * want to avoid for instance having a slash being expanded in an
 * escaped path element, which might confuse pathname handling.
 *
 * Returns: an unescaped version of @escaped_string. The returned string
 * should be freed when no longer needed.
 *
 * Since: 2.16
 **/
gchar *
g_uri_unescape_string (const gchar *escaped_string,
                       const gchar *illegal_characters)
{
  return g_uri_unescape_segment (escaped_string, NULL, illegal_characters);
}

/**
 * g_uri_escape_string:
 * @unescaped: the unescaped input string.
 * @reserved_chars_allowed: (nullable): a string of reserved
 *   characters that are allowed to be used, or %NULL.
 * @allow_utf8: %TRUE if the result can include UTF-8 characters.
 *
 * Escapes a string for use in a URI.
 *
 * Normally all characters that are not "unreserved" (i.e. ASCII
 * alphanumerical characters plus dash, dot, underscore and tilde) are
 * escaped. But if you specify characters in @reserved_chars_allowed
 * they are not escaped. This is useful for the "reserved" characters
 * in the URI specification, since those are allowed unescaped in some
 * portions of a URI.
 *
 * Returns: an escaped version of @unescaped. The returned string
 * should be freed when no longer needed.
 *
 * Since: 2.16
 **/
gchar *
g_uri_escape_string (const gchar *unescaped,
                     const gchar *reserved_chars_allowed,
                     gboolean     allow_utf8)
{
  GString *s;

  g_return_val_if_fail (unescaped != NULL, NULL);

  s = g_string_sized_new (strlen (unescaped) * 1.25);

  g_string_append_uri_escaped (s, unescaped, reserved_chars_allowed, allow_utf8);

  return g_string_free (s, FALSE);
}

/**
 * g_uri_unescape_bytes:
 * @escaped_string: A URI-escaped string
 * @length: the length of @escaped_string to escape, or -1 if it
 *   is NUL-terminated.
 *
 * Unescapes a segment of an escaped string as binary data.
 *
 * Note that in contrast to g_uri_unescape_string(), this does allow
 * `NUL` bytes to appear in the output.
 *
 * Returns: (transfer full): an unescaped version of @escaped_string
 * or %NULL on error. The returned #GBytes should be unreffed when no
 * longer needed.
 *
 * Since: 2.66
 **/
GBytes *
g_uri_unescape_bytes (const gchar *escaped_string,
                      gssize       length)
{
  gchar *buf;
  gssize unescaped_length;

  g_return_val_if_fail (escaped_string != NULL, NULL);

  if (length == -1)
    length = strlen (escaped_string);

  unescaped_length = uri_decoder (&buf,
                                  escaped_string, length,
                                  FALSE,
                                  FALSE,
                                  G_URI_FLAGS_PARSE_STRICT|G_URI_FLAGS_ENCODED,
                                  0, NULL);
  if (unescaped_length == -1)
    return NULL;

  return g_bytes_new_take (buf, unescaped_length);
}

/**
 * g_uri_escape_bytes:
 * @unescaped: (array length=length): the unescaped input data.
 * @length: the length of @unescaped
 * @reserved_chars_allowed: (nullable): a string of reserved
 *   characters that are allowed to be used, or %NULL.
 *
 * Escapes arbitrary data for use in a URI.
 *
 * Normally all characters that are not "unreserved" (i.e. ASCII
 * alphanumerical characters plus dash, dot, underscore and tilde) are
 * escaped. But if you specify characters in @reserved_chars_allowed
 * they are not escaped. This is useful for the "reserved" characters
 * in the URI specification, since those are allowed unescaped in some
 * portions of a URI.
 *
 * Though technically incorrect, this will also allow escaping "0"
 * bytes as "`%``00`".
 *
 * Returns: an escaped version of @unescaped. The returned string
 * should be freed when no longer needed.
 *
 * Since: 2.66
 */
gchar *
g_uri_escape_bytes (const guchar *unescaped,
                    gsize         length,
                    const gchar  *reserved_chars_allowed)
{
  GString *string;

  g_return_val_if_fail (unescaped != NULL, NULL);

  string = g_string_sized_new (length * 1.25);

  _uri_encoder (string, unescaped, length,
               reserved_chars_allowed, FALSE);

  return g_string_free (string, FALSE);
}

static gint
g_uri_scheme_length (const gchar *uri)
{
  const gchar *p;

  p = uri;
  if (!g_ascii_isalpha (*p))
    return -1;
  p++;
  while (g_ascii_isalnum (*p) || *p == '.' || *p == '+' || *p == '-')
    p++;

  if (p > uri && *p == ':')
    return p - uri;

  return -1;
}

/**
 * g_uri_parse_scheme:
 * @uri: a valid URI.
 *
 * Gets the scheme portion of a URI string. RFC 3986 decodes the scheme as:
 * |[
 * URI = scheme ":" hier-part [ "?" query ] [ "#" fragment ]
 * ]|
 * Common schemes include "file", "http", "svn+ssh", etc.
 *
 * Returns: The "scheme" component of the URI, or %NULL on error.
 * The returned string should be freed when no longer needed.
 *
 * Since: 2.16
 **/
gchar *
g_uri_parse_scheme (const gchar *uri)
{
  gint len;

  g_return_val_if_fail (uri != NULL, NULL);

  len = g_uri_scheme_length (uri);
  return len == -1 ? NULL : g_strndup (uri, len);
}

/**
 * g_uri_peek_scheme:
 * @uri: a valid URI.
 *
 * Gets the scheme portion of a URI string. RFC 3986 decodes the scheme as:
 * |[
 * URI = scheme ":" hier-part [ "?" query ] [ "#" fragment ]
 * ]|
 * Common schemes include "file", "http", "svn+ssh", etc.
 *
 * Returns: The "scheme" component of the URI, or %NULL on error. The
 * returned string is normalized to all-lowercase, and interned via
 * g_intern_string(), so it does not need to be freed.
 *
 * Since: 2.66
 **/
const gchar *
g_uri_peek_scheme (const gchar *uri)
{
  gint len;
  gchar *lower_scheme;
  const gchar *scheme;

  g_return_val_if_fail (uri != NULL, NULL);

  len = g_uri_scheme_length (uri);
  if (len == -1)
    return NULL;

  lower_scheme = g_ascii_strdown (uri, len);
  scheme = g_intern_string (lower_scheme);
  g_free (lower_scheme);

  return scheme;
}

G_DEFINE_QUARK (g-uri-quark, g_uri_error)
