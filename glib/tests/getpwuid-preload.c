/* GLIB - Library of useful routines for C programming
 *
 * Copyright (C) 2020 Red Hat, Inc.
 *
 * Author: Jakub Jelen <jjelen@redhat.com>
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
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include <dlfcn.h>
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>

#undef getpwuid

static struct passwd my_pw;

/* This is used in gutils.c used to make sure utility functions
 * handling user information do not crash on bad data (for example
 * caused by getpwuid returning some NULL elements.
 */
struct passwd *
getpwuid (uid_t uid)
{
  static struct passwd *(*real_getpwuid) (uid_t);
  struct passwd *pw;

  if (real_getpwuid == NULL)
    real_getpwuid = dlsym (RTLD_NEXT, "getpwuid");

  pw = real_getpwuid (uid);
  my_pw = *pw;
  my_pw.pw_name = NULL;
  return &my_pw;
}

int
getpwnam_r (const char *name, struct passwd *pwd, char buf[], size_t buflen, struct passwd **result)
{
  int code;

  int (*real_getpwnam_r) (const char *,
                          struct passwd *,
                          char[],
                          size_t,
                          struct passwd **) = dlsym (RTLD_NEXT, "getpwnam_r");

  code = real_getpwnam_r (name, pwd, buf, buflen, result);
  pwd->pw_name = NULL;
  return code;
}

int
getpwuid_r (uid_t uid, struct passwd *restrict pwd, char buf[], size_t buflen, struct passwd **result)
{
  int code;

  int (*real_getpwuid_r) (uid_t,
                          struct passwd *,
                          char[],
                          size_t,
                          struct passwd **) = dlsym (RTLD_NEXT, "getpwuid_r");

  code = real_getpwuid_r (uid, pwd, buf, buflen, result);
  pwd->pw_name = NULL;
  return code;
}
