/*
 * Copyright © 2010 Codethink Limited
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
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#ifndef __gvdb_reader_h__
#define __gvdb_reader_h__

#include <glib.h>

/* We cannot enable the weak attribute unconditionally here because both
 * gvdb/gvdb-reader.c and tests/dconf-mock-gvdb.c include this file. The
 * intention of using weak symbols here is to allow the latter to override
 * functions defined in the former, so functions in tests/dconf-mock-gvdb.c
 * must have strong bindings. */
#ifdef GVDB_USE_WEAK_SYMBOLS
# ifdef __GNUC__
#  define GVDB_GNUC_WEAK __attribute__((weak))
# else
#  define GVDB_GNUC_WEAK
# endif
#else
# define GVDB_GNUC_WEAK
#endif

typedef struct _GvdbTable GvdbTable;

G_BEGIN_DECLS

G_GNUC_INTERNAL GVDB_GNUC_WEAK
GvdbTable *             gvdb_table_new_from_bytes                       (GBytes       *bytes,
                                                                         gboolean      trusted,
                                                                         GError      **error);
G_GNUC_INTERNAL GVDB_GNUC_WEAK
GvdbTable *             gvdb_table_new                                  (const gchar  *filename,
                                                                         gboolean      trusted,
                                                                         GError      **error);
G_GNUC_INTERNAL GVDB_GNUC_WEAK
void                    gvdb_table_free                                 (GvdbTable    *table);
G_GNUC_INTERNAL GVDB_GNUC_WEAK
gchar **                gvdb_table_get_names                            (GvdbTable    *table,
                                                                         gsize        *length);
G_GNUC_INTERNAL GVDB_GNUC_WEAK
gchar **                gvdb_table_list                                 (GvdbTable    *table,
                                                                         const gchar  *key);
G_GNUC_INTERNAL GVDB_GNUC_WEAK
GvdbTable *             gvdb_table_get_table                            (GvdbTable    *table,
                                                                         const gchar  *key);
G_GNUC_INTERNAL GVDB_GNUC_WEAK
GVariant *              gvdb_table_get_raw_value                        (GvdbTable    *table,
                                                                         const gchar  *key);
G_GNUC_INTERNAL GVDB_GNUC_WEAK
GVariant *              gvdb_table_get_value                            (GvdbTable    *table,
                                                                         const gchar  *key);

G_GNUC_INTERNAL GVDB_GNUC_WEAK
gboolean                gvdb_table_has_value                            (GvdbTable    *table,
                                                                         const gchar  *key);
G_GNUC_INTERNAL GVDB_GNUC_WEAK
gboolean                gvdb_table_is_valid                             (GvdbTable    *table);

G_END_DECLS

#endif /* __gvdb_reader_h__ */
