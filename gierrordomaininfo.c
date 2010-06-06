/* GObject introspection: ErrorDomain implementation
 *
 * Copyright (C) 2005 Matthias Clasen
 * Copyright (C) 2008,2009 Red Hat, Inc.
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

#include <glib.h>

#include <girepository.h>
#include "girepository-private.h"
#include "gitypelib-internal.h"

/**
 * SECTION:gierrordomaininfo
 * @Short_description: Struct representing an error domain
 * @Title: GIErrorDomainInfo
 *
 * A GIErrorDomainInfo struct represents a domain of a #GError.
 * An error domain is associated with a #GQuark and contains a pointer
 * to an enum with all the error codes.
 */

/**
 * g_error_domain_info_get_quark:
 * @info: a #GIErrorDomainInfo
 *
 * Obtain a string representing the quark for this error domain.
 * %NULL will be returned if the type tag is wrong or if a quark is
 * missing in the typelib.
 *
 * Returns: the quark represented as a string or %NULL
 */
const gchar *
g_error_domain_info_get_quark (GIErrorDomainInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ErrorDomainBlob *blob;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_ERROR_DOMAIN_INFO (info), NULL);

  blob = (ErrorDomainBlob *)&rinfo->typelib->data[rinfo->offset];

  return g_typelib_get_string (rinfo->typelib, blob->get_quark);
}

/**
 * g_error_domain_info_get_codes:
 * @info: a #GIErrorDomainInfo
 *
 * Obtain the enum containing all the error codes for this error domain.
 * The return value will have a #GIInfoType of %GI_INFO_TYPE_ERROR_DOMAIN
 *
 * Returns: (transfer full): the error domain or %NULL if type tag is wrong,
 * free the struct with g_base_info_unref() when done.
 */
GIInterfaceInfo *
g_error_domain_info_get_codes (GIErrorDomainInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ErrorDomainBlob *blob;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_ERROR_DOMAIN_INFO (info), NULL);

  blob = (ErrorDomainBlob *)&rinfo->typelib->data[rinfo->offset];

  return (GIInterfaceInfo *) _g_info_from_entry (rinfo->repository,
                                                 rinfo->typelib, blob->error_codes);
}


