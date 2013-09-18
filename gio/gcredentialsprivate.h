/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright 2013 Red Hat, Inc.
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

#ifndef __G_CREDENTIALS_PRIVATE_H__
#define __G_CREDENTIALS_PRIVATE_H__

#include "gio/gcredentials.h"
#include "gio/gnetworking.h"

#ifdef __linux__
#define G_CREDENTIALS_SUPPORTED 1
#define G_CREDENTIALS_USE_LINUX_UCRED 1
#define G_CREDENTIALS_NATIVE_TYPE G_CREDENTIALS_TYPE_LINUX_UCRED
#define G_CREDENTIALS_NATIVE_SIZE (sizeof (struct ucred))
#define G_CREDENTIALS_UNIX_CREDENTIALS_MESSAGE_SUPPORTED 1
#define G_CREDENTIALS_SOCKET_GET_CREDENTIALS_SUPPORTED 1

#elif defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
#define G_CREDENTIALS_SUPPORTED 1
#define G_CREDENTIALS_USE_FREEBSD_CMSGCRED 1
#define G_CREDENTIALS_NATIVE_TYPE G_CREDENTIALS_TYPE_FREEBSD_CMSGCRED
#define G_CREDENTIALS_NATIVE_SIZE (sizeof (struct cmsgcred))
#define G_CREDENTIALS_UNIX_CREDENTIALS_MESSAGE_SUPPORTED 1

#elif defined(__OpenBSD__)
#define G_CREDENTIALS_SUPPORTED 1
#define G_CREDENTIALS_USE_OPENBSD_SOCKPEERCRED 1
#define G_CREDENTIALS_NATIVE_TYPE G_CREDENTIALS_TYPE_OPENBSD_SOCKPEERCRED
#define G_CREDENTIALS_NATIVE_SIZE (sizeof (struct sockpeercred))
#define G_CREDENTIALS_SOCKET_GET_CREDENTIALS_SUPPORTED 1

#endif

#endif /* __G_CREDENTIALS_PRIVATE_H__ */
