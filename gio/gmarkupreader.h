/*
 * Copyright © 2013 Canonical Limited
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#ifndef __G_MARKUP_READER_H__
#define __G_MARKUP_READER_H__

#if !defined (__GIO_GIO_H_INSIDE__) && !defined (GIO_COMPILATION)
#error "Only <gio/gio.h> can be included directly."
#endif

#include <gio/giotypes.h>

G_BEGIN_DECLS

#define G_TYPE_MARKUP_READER                                (g_markup_reader_get_type ())
#define G_MARKUP_READER(inst)                               (G_TYPE_CHECK_INSTANCE_CAST ((inst),                     \
                                                             G_TYPE_MARKUP_READER, GMarkupReader))
#define G_IS_MARKUP_READER(inst)                            (G_TYPE_CHECK_INSTANCE_TYPE ((inst),                     \
                                                             G_TYPE_MARKUP_READER))

typedef struct _GMarkupReader                               GMarkupReader;

GLIB_AVAILABLE_IN_2_40
GType                   g_markup_reader_get_type                        (void);

GLIB_AVAILABLE_IN_2_40
GMarkupReader *         g_markup_reader_new                             (GInputStream         *stream,
                                                                         GMarkupParseFlags     flags);

GLIB_AVAILABLE_IN_2_40
gboolean                g_markup_reader_advance                         (GMarkupReader        *reader,
                                                                         GCancellable         *cancellable,
                                                                         GError              **error);

GLIB_AVAILABLE_IN_2_40
gboolean                g_markup_reader_is_eof                          (GMarkupReader        *reader);

GLIB_AVAILABLE_IN_2_40
gboolean                g_markup_reader_is_start_element                (GMarkupReader        *reader,
                                                                         const gchar          *element_name);

GLIB_AVAILABLE_IN_2_40
gboolean                g_markup_reader_is_end_element                  (GMarkupReader        *reader);

GLIB_AVAILABLE_IN_2_40
gboolean                g_markup_reader_is_passthrough                  (GMarkupReader        *reader);

GLIB_AVAILABLE_IN_2_40
gboolean                g_markup_reader_is_text                         (GMarkupReader        *reader);

GLIB_AVAILABLE_IN_2_40
gboolean                g_markup_reader_is_whitespace                   (GMarkupReader        *reader);

GLIB_AVAILABLE_IN_2_40
const gchar *           g_markup_reader_get_element_name                (GMarkupReader        *reader);

GLIB_AVAILABLE_IN_2_40
void                    g_markup_reader_get_attributes                  (GMarkupReader        *reader,
                                                                         const gchar * const **attribute_names,
                                                                         const gchar * const **attribute_values);

GLIB_AVAILABLE_IN_2_40
gboolean                g_markup_reader_collect_attributes              (GMarkupReader        *reader,
                                                                         GError              **error,
                                                                         GMarkupCollectType    first_type,
                                                                         const gchar          *first_name,
                                                                         ...);

GLIB_AVAILABLE_IN_2_40
gboolean                g_markup_reader_collect_elements                (GMarkupReader        *reader,
                                                                         GCancellable         *cancellable,
                                                                         gpointer              user_data,
                                                                         GError              **error,
                                                                         const gchar          *first_name,
                                                                         ...) G_GNUC_NULL_TERMINATED;

GLIB_AVAILABLE_IN_2_40
GBytes *                g_markup_reader_get_content                     (GMarkupReader        *reader);

GLIB_AVAILABLE_IN_2_40
gboolean                g_markup_reader_unexpected                      (GMarkupReader        *reader,
                                                                         GError              **error);

GLIB_AVAILABLE_IN_2_40
gboolean                g_markup_reader_expect_end                      (GMarkupReader        *reader,
                                                                         GCancellable         *cancellable,
                                                                         GError              **error);
GLIB_AVAILABLE_IN_2_40
void                    g_markup_reader_set_error                       (GMarkupReader        *reader,
                                                                         GError              **error,
                                                                         GQuark                domain,
                                                                         gint                  code,
                                                                         const gchar          *format,
                                                                         ...);
GLIB_AVAILABLE_IN_2_40
gchar *                 g_markup_reader_collect_text                    (GMarkupReader        *reader,
                                                                         GCancellable         *cancellable,
                                                                         GError              **error);

G_END_DECLS

#endif /* __G_MARKUP_READER_H__ */
