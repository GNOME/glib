/* GIO - GLib Input, Output and Certificateing Library
 *
 * Copyright (C) 2010 Red Hat, Inc.
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
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include "gtlscertificate.h"

#include <string.h>
#include "ginitable.h"
#include "gtlsbackend.h"
#include "gtlsconnection.h"
#include "glibintl.h"

/**
 * SECTION:gtlscertificate
 * @title: GTlsCertificate
 * @short_description: TLS certificate
 * @see_also: #GTlsConnection
 *
 * A certificate used for TLS authentication and encryption.
 * This can represent either a public key only (eg, the certificate
 * received by a client from a server), or the combination of
 * a public key and a private key (which is needed when acting as a
 * #GTlsServerConnection).
 *
 * Since: 2.28
 */

/**
 * GTlsCertificate:
 *
 * Abstract base class for TLS certificate types.
 *
 * Since: 2.28
 */

G_DEFINE_ABSTRACT_TYPE (GTlsCertificate, g_tls_certificate, G_TYPE_OBJECT);

enum
{
  PROP_0,

  PROP_CERTIFICATE,
  PROP_CERTIFICATE_PEM,
  PROP_PRIVATE_KEY,
  PROP_PRIVATE_KEY_PEM,
  PROP_ISSUER
};

static void
g_tls_certificate_init (GTlsCertificate *cert)
{
}

static void
g_tls_certificate_get_property (GObject    *object,
				guint       prop_id,
				GValue     *value,
				GParamSpec *pspec)
{
  G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
}

static void
g_tls_certificate_set_property (GObject      *object,
				guint         prop_id,
				const GValue *value,
				GParamSpec   *pspec)
{
  G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
}

static void
g_tls_certificate_class_init (GTlsCertificateClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->set_property = g_tls_certificate_set_property;
  gobject_class->get_property = g_tls_certificate_get_property;

  /**
   * GTlsCertificate:certificate:
   *
   * The DER (binary) encoded representation of the certificate's
   * public key. This property and the
   * #GTlsCertificate:certificate-pem property represent the same
   * data, just in different forms.
   *
   * Since: 2.28
   */
  g_object_class_install_property (gobject_class, PROP_CERTIFICATE,
				   g_param_spec_boxed ("certificate",
						       P_("Certificate"),
						       P_("The DER representation of the certificate"),
						       G_TYPE_BYTE_ARRAY,
						       G_PARAM_READWRITE |
						       G_PARAM_CONSTRUCT_ONLY |
						       G_PARAM_STATIC_STRINGS));
  /**
   * GTlsCertificate:certificate-pem:
   *
   * The PEM (ASCII) encoded representation of the certificate's
   * public key. This property and the #GTlsCertificate:certificate
   * property represent the same data, just in different forms.
   *
   * Since: 2.28
   */
  g_object_class_install_property (gobject_class, PROP_CERTIFICATE_PEM,
				   g_param_spec_string ("certificate-pem",
							P_("Certificate (PEM)"),
							P_("The PEM representation of the certificate"),
							NULL,
							G_PARAM_READWRITE |
							G_PARAM_CONSTRUCT_ONLY |
							G_PARAM_STATIC_STRINGS));
  /**
   * GTlsCertificate:private-key:
   *
   * The DER (binary) encoded representation of the certificate's
   * private key. This property (or the
   * #GTlsCertificate:private-key-pem property) can be set when
   * constructing a key (eg, from a file), but cannot be read.
   *
   * Since: 2.28
   */
  g_object_class_install_property (gobject_class, PROP_PRIVATE_KEY,
				   g_param_spec_boxed ("private-key",
						       P_("Private key"),
						       P_("The DER representation of the certificate's private key"),
						       G_TYPE_BYTE_ARRAY,
						       G_PARAM_WRITABLE |
						       G_PARAM_CONSTRUCT_ONLY |
						       G_PARAM_STATIC_STRINGS));
  /**
   * GTlsCertificate:private-key-pem:
   *
   * The PEM (ASCII) encoded representation of the certificate's
   * private key. This property (or the #GTlsCertificate:private-key
   * property) can be set when constructing a key (eg, from a file),
   * but cannot be read.
   *
   * Since: 2.28
   */
  g_object_class_install_property (gobject_class, PROP_PRIVATE_KEY_PEM,
				   g_param_spec_string ("private-key-pem",
							P_("Private key (PEM)"),
							P_("The PEM representation of the certificate's private key"),
							NULL,
							G_PARAM_WRITABLE |
							G_PARAM_CONSTRUCT_ONLY |
							G_PARAM_STATIC_STRINGS));
  /**
   * GTlsCertificate:issuer:
   *
   * A #GTlsCertificate representing the entity that issued this
   * certificate. If %NULL, this means that the certificate is either
   * self-signed, or else the certificate of the issuer is not
   * available.
   *
   * Since: 2.28
   */
  g_object_class_install_property (gobject_class, PROP_ISSUER,
				   g_param_spec_object ("issuer",
							P_("Issuer"),
							P_("The certificate for the issuing entity"),
							G_TYPE_TLS_CERTIFICATE,
							G_PARAM_READWRITE |
							G_PARAM_CONSTRUCT_ONLY |
							G_PARAM_STATIC_STRINGS));
}

static GTlsCertificate *
g_tls_certificate_new_internal (const gchar  *certificate_pem,
				const gchar  *private_key_pem,
				GError      **error)
{
  GObject *cert;
  GTlsBackend *backend;

  backend = g_tls_backend_get_default ();

  cert = g_initable_new (g_tls_backend_get_certificate_type (backend),
			 NULL, error,
			 "certificate-pem", certificate_pem,
			 "private-key-pem", private_key_pem,
			 NULL);
  return G_TLS_CERTIFICATE (cert);
}

#define PEM_CERTIFICATE_HEADER "-----BEGIN CERTIFICATE-----"
#define PEM_CERTIFICATE_FOOTER "-----END CERTIFICATE-----"
#define PEM_PRIVKEY_HEADER     "-----BEGIN RSA PRIVATE KEY-----"
#define PEM_PRIVKEY_FOOTER     "-----END RSA PRIVATE KEY-----"

static GTlsCertificate *
parse_next_pem_certificate (const gchar **data,
			    const gchar  *data_end,
			    gboolean      required,
			    GError      **error)
{
  const gchar *start, *end, *next;
  gchar *cert_pem, *privkey_pem = NULL;
  GTlsCertificate *cert;

  start = g_strstr_len (*data, data_end - *data, PEM_CERTIFICATE_HEADER);
  if (!start)
    {
      if (required)
	{
	  g_set_error_literal (error, G_TLS_ERROR, G_TLS_ERROR_BAD_CERTIFICATE,
			       _("No PEM-encoded certificate found"));
	}
      return NULL;
    }

  end = g_strstr_len (start, data_end - start, PEM_CERTIFICATE_FOOTER);
  if (!end)
    {
      g_set_error_literal (error, G_TLS_ERROR, G_TLS_ERROR_BAD_CERTIFICATE,
			   _("Could not parse PEM-encoded certificate"));
      return NULL;
    }
  end += strlen (PEM_CERTIFICATE_FOOTER);
  while (*end == '\r' || *end == '\n')
    end++;

  cert_pem = g_strndup (start, end - start);

  *data = end;

  next = g_strstr_len (*data, data_end - *data, PEM_CERTIFICATE_HEADER);
  start = g_strstr_len (*data, data_end - *data, PEM_PRIVKEY_HEADER);
  if (start)
    end = g_strstr_len (start, data_end - start, PEM_PRIVKEY_FOOTER);

  if (start && (!next || start < next))
    {
      if (!end || (next && end > next))
	{
	  g_set_error_literal (error, G_TLS_ERROR, G_TLS_ERROR_BAD_CERTIFICATE,
			       _("Could not parse PEM-encoded private key"));
	  return NULL;
	}

      end += strlen (PEM_PRIVKEY_FOOTER);
      while (*end == '\r' || *end == '\n')
	end++;

      privkey_pem = g_strndup (start, end - start);

      *data = end + strlen (PEM_PRIVKEY_FOOTER);
    }

  cert = g_tls_certificate_new_internal (cert_pem, privkey_pem, error);
  g_free (cert_pem);
  g_free (privkey_pem);

  return cert;
}

/**
 * g_tls_certificate_new_from_pem:
 * @data: PEM-encoded certificate data
 * @length: the length of @data, or -1 if it's 0-terminated.
 * @error: #GError for error reporting, or %NULL to ignore.
 *
 * Creates a new #GTlsCertificate from the PEM-encoded data in @data.
 * If @data includes both a certificate and a private key, then the
 * returned certificate will include the private key data as well.
 *
 * If @data includes multiple certificates, only the first one will be
 * parsed.
 *
 * Return value: the new certificate, or %NULL if @data is invalid
 *
 * Since: 2.28
 */
GTlsCertificate *
g_tls_certificate_new_from_pem  (const gchar  *data,
				 gssize        length,
				 GError      **error)
{
  const gchar *data_end;

  g_return_val_if_fail (data != NULL, NULL);

  if (length == -1)
    data_end = data + strlen (data);
  else
    data_end = data + length;
  return parse_next_pem_certificate (&data, data_end, TRUE, error);
}

/**
 * g_tls_certificate_new_from_file:
 * @file: file containing a PEM-encoded certificate to import
 * @error: #GError for error reporting, or %NULL to ignore.
 *
 * Creates a #GTlsCertificate from the PEM-encoded data in @file. If
 * @file cannot be read or parsed, the function will return %NULL and
 * set @error. Otherwise, this behaves like g_tls_certificate_new().
 *
 * Return value: the new certificate, or %NULL on error
 *
 * Since: 2.28
 */
GTlsCertificate *
g_tls_certificate_new_from_file (const gchar  *file,
				 GError      **error)
{
  GTlsCertificate *cert;
  gchar *contents;
  gsize length;

  if (!g_file_get_contents (file, &contents, &length, error))
    return NULL;

  cert = g_tls_certificate_new_from_pem (contents, length, error);
  g_free (contents);
  return cert;
}

/**
 * g_tls_certificate_new_from_files:
 * @cert_file: file containing a PEM-encoded certificate to import
 * @key_file: file containing a PEM-encoded private key to import
 * @error: #GError for error reporting, or %NULL to ignore.
 *
 * Creates a #GTlsCertificate from the PEM-encoded data in @cert_file
 * and @key_file. If either file cannot be read or parsed, the
 * function will return %NULL and set @error. Otherwise, this behaves
 * like g_tls_certificate_new().
 *
 * Return value: the new certificate, or %NULL on error
 *
 * Since: 2.28
 */
GTlsCertificate *
g_tls_certificate_new_from_files (const gchar  *cert_file,
				  const gchar  *key_file,
				  GError      **error)
{
  GTlsCertificate *cert;
  gchar *cert_data, *key_data;

  if (!g_file_get_contents (cert_file, &cert_data, NULL, error))
    return NULL;
  if (!g_file_get_contents (key_file, &key_data, NULL, error))
    {
      g_free (cert_data);
      return NULL;
    }

  cert = g_tls_certificate_new_internal (cert_data, key_data, error);
  g_free (cert_data);
  g_free (key_data);
  return cert;
}

/**
 * g_tls_certificate_list_new_from_file:
 * @file: file containing PEM-encoded certificates to import
 * @error: #GError for error reporting, or %NULL to ignore.
 *
 * Creates one or more #GTlsCertificate<!-- -->s from the PEM-encoded
 * data in @file. If @file cannot be read or parsed, the function will
 * return %NULL and set @error. If @file does not contain any
 * PEM-encoded certificates, this will return an empty list and not
 * set @error.
 *
 * Return value: (element-type Gio.TlsCertificate) (transfer full): a
 * #GList containing #GTlsCertificate objects. You must free the list
 * and its contents when you are done with it.
 *
 * Since: 2.28
 */
GList *
g_tls_certificate_list_new_from_file (const gchar  *file,
				      GError      **error)
{
  GTlsCertificate *cert;
  GList *list, *l;
  gchar *contents, *end;
  const gchar *p;
  gsize length;

  if (!g_file_get_contents (file, &contents, &length, error))
    return NULL;

  list = NULL;
  end = contents + length;
  p = contents;
  while (p && *p)
    {
      cert = parse_next_pem_certificate (&p, end, FALSE, error);
      if (!cert)
	{
	  for (l = list; l; l = l->next)
	    g_object_unref (l->data);
	  g_list_free (list);
	  list = NULL;
	  break;
	}
      list = g_list_prepend (list, cert);
    }

  return g_list_reverse (list);
}


/**
 * g_tls_certificate_get_issuer:
 * @cert: a #GTlsCertificate
 *
 * Gets the #GTlsCertificate representing @cert's issuer, if known
 *
 * Return value: (transfer none): The certificate of @cert's issuer,
 * or %NULL if @cert is self-signed or signed with an unknown
 * certificate.
 *
 * Since: 2.28
 */
GTlsCertificate *
g_tls_certificate_get_issuer (GTlsCertificate  *cert)
{
  GTlsCertificate *issuer;

  g_object_get (G_OBJECT (cert), "issuer", &issuer, NULL);
  if (issuer)
    g_object_unref (issuer);

  return issuer;
}

/**
 * g_tls_certificate_verify:
 * @cert: a #GTlsCertificate
 * @identity: (allow-none): the expected peer identity
 * @trusted_ca: (allow-none): the certificate of a trusted authority
 *
 * This verifies @cert and returns a set of #GTlsCertificateFlags
 * indicating any problems found with it. This can be used to verify a
 * certificate outside the context of making a connection, or to
 * check a certificate against a CA that is not part of the system
 * CA database.
 *
 * If @identity is not %NULL, @cert's name(s) will be compared against
 * it, and %G_TLS_CERTIFICATE_BAD_IDENTITY will be set in the return
 * value if it does not match. If @identity is %NULL, that bit will
 * never be set in the return value.
 *
 * If @trusted_ca is not %NULL, then @cert (or one of the certificates
 * in its chain) must be signed by it, or else
 * %G_TLS_CERTIFICATE_UNKNOWN_CA will be set in the return value. If
 * @trusted_ca is %NULL, that bit will never be set in the return
 * value.
 *
 * (All other #GTlsCertificateFlags values will always be set or unset
 * as appropriate.)
 *
 * Return value: the appropriate #GTlsCertificateFlags
 *
 * Since: 2.28
 */
GTlsCertificateFlags
g_tls_certificate_verify (GTlsCertificate     *cert,
			  GSocketConnectable  *identity,
			  GTlsCertificate     *trusted_ca)
{
  return G_TLS_CERTIFICATE_GET_CLASS (cert)->verify (cert, identity, trusted_ca);
}
