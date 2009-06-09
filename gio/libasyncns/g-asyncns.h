/* GLIB - Library of useful routines for C programming
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifndef __G_ASYNCNS_H__

#include "config.h"

#define _GNU_SOURCE

/* We want to build the fork-based version, not the threaded version. */
#undef HAVE_PTHREAD

/* For old OS X, #580301. Remove if these are added to asyncns.c
 * in the future.
 */
#include <sys/time.h>
#include <stdint.h>

/* Some BSDs require this for getrlimit */
#include <sys/resource.h>

#include "asyncns.h"

#endif
