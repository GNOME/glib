/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2008 Red Hat, Inc.
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

#ifndef __G_NETWORKINGPRIVATE_H__
#define __G_NETWORKINGPRIVATE_H__

#ifdef G_OS_WIN32

#define WINVER 0x0501 // FIXME?
#include <winsock2.h>
#undef interface
#include <ws2tcpip.h>
#include <windns.h>

#else /* !G_OS_WIN32 */

#define BIND_4_COMPAT

#include <arpa/inet.h>
#include <arpa/nameser.h>
/* We're supposed to define _GNU_SOURCE to get EAI_NODATA, but that
 * won't actually work since <features.h> has already been included at
 * this point. So we define __USE_GNU instead.
 */
#define __USE_GNU
#include <netdb.h>
#undef __USE_GNU
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <resolv.h>
#include <sys/socket.h>
#include <sys/un.h>

#endif

#endif /* __G_NETWORKINGPRIVATE_H__ */
