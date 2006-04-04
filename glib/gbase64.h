/* gbase64.h - Base64 coding functions
 *
 *  Copyright (C) 2005  Alexander Larsson <alexl@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __G_BASE64_H__
#define __G_BASE64_H__

#include <glib/gtypes.h>

G_BEGIN_DECLS

gsize   g_base64_encode_step  (const guchar *in,
			       gsize         len,
			       gboolean      break_lines,
			       char         *out,
			       int          *state,
			       int          *save);
gsize   g_base64_encode_close (gboolean      break_lines,
			       char         *out,
			       int          *state,
			       int          *save);
char *  g_base64_encode       (const guchar *data,
			       gsize         len);
gsize   g_base64_decode_step  (const char   *in,
			       gsize         len,
			       guchar       *out,
			       int          *state,
			       guint        *save);
guchar *g_base64_decode       (const char   *text,
			       gsize        *out_len);

G_END_DECLS

#endif /* __G_BASE64_H__ */
