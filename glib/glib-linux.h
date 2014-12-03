/*
 * Copyright © 2014 Canonical Limited
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the licence, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#ifndef __GLIB_LINUX_H__
#define __GLIB_LINUX_H__

/* If we know that we are on Linux, add some features, even if they are
 * not (yet) advertised in the glibc or kernel headers.
 *
 * This allows us to use functionality regardless of if it was available
 * when GLib was compiled or not.
 *
 * We take care not to define these things on non-Linux systems where
 * certain numeric values could mean something different.
 *
 * This file is populated on an as-needed basis.
 *
 * As things in this file filter into glibc and the distributions we can
 * remove them from this file and add unconditional dependencies.  Never
 * add a configure.ac check in order to remove something from this file.
 *
 * import: include this header LAST
 */

#ifdef __linux__

#define GLIB_LINUX

#include <sys/syscall.h>

/* futex */
#include <linux/futex.h>

static inline int
glib_linux_futex (int                   *uaddr,
                  int                    op,
                  int                    val,
                  const struct timespec *timeout,
                  int                   *uaddr2,
                  int                    val3)
{
  return syscall (__NR_futex, uaddr, op, val, timeout, uaddr2, val3);
}

/* memfd */
#ifndef MFD_CLOEXEC
#define MFD_CLOEXEC             0x0001U
#endif

#ifndef MFD_ALLOW_SEALING
#define MFD_ALLOW_SEALING       0x0002U
#endif

#ifndef __NR_memfd_create
  #ifdef __x86_64__
    #define __NR_memfd_create 319
  #elif defined __arm__
    #define __NR_memfd_create 385
  #else
    #define __NR_memfd_create 356
  #endif
#endif

static inline int
glib_linux_memfd_create (const char   *name,
                         unsigned int  flags)
{
  return syscall (__NR_memfd_create, name, flags);
}

/* Linux-specific fcntl() operations */
#ifndef F_LINUX_SPECIFIC_BASE
#define F_LINUX_SPECIFIC_BASE 1024
#endif

#ifndef F_ADD_SEALS
#define F_ADD_SEALS (F_LINUX_SPECIFIC_BASE + 9)
#define F_GET_SEALS (F_LINUX_SPECIFIC_BASE + 10)

#define F_SEAL_SEAL     0x0001  /* prevent further seals from being set */
#define F_SEAL_SHRINK   0x0002  /* prevent file from shrinking */
#define F_SEAL_GROW     0x0004  /* prevent file from growing */
#define F_SEAL_WRITE    0x0008  /* prevent writes */
#endif

#endif /* __linux __ */

#endif /* __GLIB_LINUX_H__ */
