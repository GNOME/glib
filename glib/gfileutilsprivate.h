/*
 * Copyright © 2023 Red Hat, Inc
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Matthias Clasen
 */

#ifndef __G_FILE_UTILS_PRIVATE_H__
#define __G_FILE_UTILS_PRIVATE_H__

#include "glibconfig.h"
#include "gstring.h"

G_BEGIN_DECLS

void g_build_path_va (GString     *result,
                      const char  *separator,
                      const char  *first_element,
                      va_list     *args,
                      char       **str_array);

#ifdef G_OS_WIN32

void g_build_pathname_va (GString     *result,
                          const char  *first_element,
                          va_list     *args,
                          char       **str_array);

#endif


G_END_DECLS

#endif /* __G_FILE_UTILS_PRIVATE_H__ */
