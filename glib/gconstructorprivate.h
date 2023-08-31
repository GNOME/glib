/*
 * Copyright © 2023 Luca Bacci
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "gconstructor.h"

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef _WIN32

#ifdef _MSC_VER

#define G_HAS_TLS_CALLBACKS 1

#define G_DEFINE_TLS_CALLBACK(func) \
__pragma (section (".CRT$XLCE", long, read))                                \
                                                                            \
static void NTAPI func (PVOID, DWORD, PVOID);                               \
                                                                            \
__declspec (allocate (".CRT$XLCE"))                                         \
const PIMAGE_TLS_CALLBACK _ptr_##func = func;                               \
                                                                            \
__pragma (comment (linker, "/INCLUDE:" G_MSVC_SYMBOL_PREFIX "_tls_used"))   \
__pragma (comment (linker, "/INCLUDE:" G_MSVC_SYMBOL_PREFIX "_ptr_" #func))

#else

#define G_HAS_TLS_CALLBACKS 1

#define G_DEFINE_TLS_CALLBACK(func)           \
static void NTAPI func (PVOID, DWORD, PVOID); \
                                              \
__attribute__((section(".CRT$XLCE")))         \
const PIMAGE_TLS_CALLBACK _ptr_##func = func;

#endif /* _MSC_VER */

#endif /* _WIN32 */
