Title: UNIX Mounts
SPDX-License-Identifier: LGPL-2.1-or-later
SPDX-FileCopyrightText: 2007 Andrew Walton
SPDX-FileCopyrightText: 2008 Matthias Clasen

# UNIX Mounts

Routines for managing mounted UNIX mount points and paths.

Note that `<gio/gunixmounts.h>` belongs to the UNIX-specific GIO
interfaces, thus you have to use the `gio-unix-2.0.pc` pkg-config
file when using it.

There are three main classes:

 * [struct@GioUnix.UnixMountEntry]
 * [struct@GioUnix.UnixMountPoint]
 * [class@GioUnix.UnixMountMonitor]

Various helper functions for querying mounts:

 * [func@GioUnix.unix_mount_points_get]
 * [func@GioUnix.UnixMountPoint.at]
 * [func@GioUnix.unix_mounts_get]
 * [func@GioUnix.unix_mount_at]
 * [func@GioUnix.unix_mount_for]
 * [func@GioUnix.unix_mounts_changed_since]
 * [func@GioUnix.unix_mount_points_changed_since]

And several helper functions for checking the type of a mount or path:

 * [func@GioUnix.unix_is_mount_path_system_internal]
 * [func@GioUnix.unix_is_system_fs_type]
 * [func@GioUnix.unix_is_system_device_path]

