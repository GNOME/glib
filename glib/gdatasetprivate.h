/* GLIB - Library of useful routines for C programming
 * gdataset-private.h: Internal macros for accessing dataset values
 * Copyright (C) 2005  Red Hat
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GLib Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/. 
 */

#ifndef __G_DATASETPRIVATE_H__
#define __G_DATASETPRIVATE_H__

#include <glib/gdataset.h>

G_BEGIN_DECLS

#define G_DATALIST_GET_FLAGS(datalist)				\
  ((gsize)*(datalist) & G_DATALIST_FLAGS_MASK)
#define G_DATALIST_SET_FLAGS(datalist, flags) G_STMT_START {	\
  *datalist = (GData *)((flags) | (gsize)*(datalist));		\
} G_STMT_END
#define G_DATALIST_UNSET_FLAGS(datalist, flags) G_STMT_START {	\
  *datalist = (GData *)(~(gsize)(flags) & (gsize)*(datalist));	\
} G_STMT_END

#define G_DATALIST_GET_POINTER(datalist)						\
  ((GData *)((gsize)*(datalist) & ~(gsize)G_DATALIST_FLAGS_MASK))
#define G_DATALIST_SET_POINTER(datalist,pointer) G_STMT_START {				\
  *(datalist) = (GData *)(G_DATALIST_GET_FLAGS (datalist) |				\
			  (gsize)pointer);						\
} G_STMT_END

G_END_DECLS

#endif /* __G_DATASETPRIVATE_H__ */
