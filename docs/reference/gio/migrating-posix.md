Title: Migrating from POSIX to GIO
SPDX-License-Identifier: LGPL-2.1-or-later
SPDX-FileCopyrightText: 2007 Matthias Clasen

# Migrating from POSIX to GIO

## Comparison of POSIX and GIO concepts

| POSIX                 | GIO                                                 |
|-----------------------|-----------------------------------------------------|
| `char *path`          | [iface@Gio.File]                                    |
| `struct stat *buf`    | [class@Gio.FileInfo]                                |
| `struct statvfs *buf` | [class@Gio.FileInfo]                                |
| `int fd`              | [class@Gio.InputStream] or [class@Gio.OutputStream] |
| `DIR *`               | [class@Gio.FileEnumerator]                          |
| `fstab entry`         | [struct@Gio.UnixMountPoint]                         |
| `mtab entry`          | [struct@Gio.UnixMountEntry]                         |
