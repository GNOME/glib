/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2015 Chun-wei Fan
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
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Chun-wei Fan <fanc999@yahoo.com.tw>
 *
 */

#include "gwin32filemonitorutils.h"
#include "gio/gfile.h"

#ifndef __G_WIN32_FILE_MONITOR_EVENTS_H__
#define __G_WIN32_FILE_MONITOR_EVENTS_H__

void CALLBACK g_win32_file_monitor_callback (DWORD        error,
                                             DWORD        nBytes,
                                             LPOVERLAPPED lpOverlapped);

#endif /* __G_WIN32_FILE_MONITOR_EVENTS_H__ */
