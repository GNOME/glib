/* Copyright © 2013 Canonical Limited
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
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#ifndef _thumbnail_verify_h_
#define _thumbnail_verify_h_

#include <glib.h>
#include <gstdio.h>
#include "glocalfileinfo.h"

gboolean   thumbnail_verify                   (const gchar            *thumbnail_path,
                                               const gchar            *file_uri,
#ifdef HAVE_STATX
                                               const struct statx     *file_stat_buf);
#else
                                               const GLocalFileStat   *file_stat_buf);
#endif

#endif /* _thumbnail_verify_h_ */
