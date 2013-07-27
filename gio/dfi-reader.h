/*
 * Copyright © 2013 Canonical Limited
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Ryan Lortie <desrt@desrt.ca>
 */

#include "common.h"

#include <sys/stat.h>

struct dfi_index;

struct dfi_index *                      dfi_index_new                                   (const gchar *directory);

struct dfi_index *                      dfi_index_new_full                              (const gchar                 *directory,
                                                                                         gint                        *dir_fd,
                                                                                         struct stat                 *dir_buf,
                                                                                         struct stat                 *file_buf,
                                                                                         gchar                     ***extra_dirs);


void                                    dfi_index_free                                  (struct dfi_index            *index);

const struct dfi_string_list *          dfi_index_get_app_names                         (const struct dfi_index      *index);
const struct dfi_string_list *          dfi_index_get_key_names                         (const struct dfi_index      *index);
const struct dfi_string_list *          dfi_index_get_locale_names                      (const struct dfi_index      *index);
const struct dfi_string_list *          dfi_index_get_group_names                       (const struct dfi_index      *index);
const struct dfi_pointer_array *        dfi_index_get_implementors                      (const struct dfi_index      *index);
const struct dfi_pointer_array *        dfi_index_get_text_indexes                      (const struct dfi_index      *index);
const struct dfi_pointer_array *        dfi_index_get_desktop_files                     (const struct dfi_index      *index);
const struct dfi_pointer_array *        dfi_index_get_mime_types                        (const struct dfi_index      *index);

gboolean                                dfi_id_valid                                    (dfi_id                       id);
guint                                   dfi_id_get                                      (dfi_id                       id);

guint                                   dfi_pointer_array_get_length                    (const struct dfi_pointer_array   *array,
                                                                                         const struct dfi_index           *dfi);
const gchar *                           dfi_pointer_array_get_item_key                  (const struct dfi_pointer_array   *array,
                                                                                         const struct dfi_index           *dfi,
                                                                                         gint                              i);
dfi_pointer                             dfi_pointer_array_get_pointer                   (const struct dfi_pointer_array   *array,
                                                                                         gint                              i);


const dfi_id *                          dfi_id_list_get_ids                             (const struct dfi_id_list         *list,
                                                                                         guint                            *n_ids);
const struct dfi_id_list *              dfi_id_list_from_pointer                        (const struct dfi_index           *index,
                                                                                         dfi_pointer                       pointer);

const struct dfi_string_list *          dfi_string_list_from_pointer                    (const struct dfi_index           *index,
                                                                                         dfi_pointer                       pointer);
gint                                    dfi_string_list_binary_search                   (const struct dfi_string_list     *list,
                                                                                         const struct dfi_index           *index,
                                                                                         const gchar                      *string);
guint                                   dfi_string_list_get_length                      (const struct dfi_string_list     *list);

const gchar *                           dfi_string_list_get_string                      (const struct dfi_string_list     *list,
                                                                                         const struct dfi_index           *dfi,
                                                                                         dfi_id                            id);

const gchar *                           dfi_string_list_get_string_at_index             (const struct dfi_string_list     *list,
                                                                                         const struct dfi_index           *dfi,
                                                                                         gint                              i);

const struct dfi_text_index *           dfi_text_index_from_pointer                     (const struct dfi_index           *dfi,
                                                                                         dfi_pointer                       pointer);

const gchar *                           dfi_text_index_get_string                       (const struct dfi_index           *dfi,
                                                                                         const struct dfi_text_index      *text_index,
                                                                                         dfi_id                            id);

const struct dfi_text_index_item *      dfi_text_index_binary_search                    (const struct dfi_text_index      *text_index,
                                                                                         const struct dfi_index           *dfi,
                                                                                         const gchar                      *string);
const dfi_id *                          dfi_text_index_item_get_ids                     (const struct dfi_text_index_item *item,
                                                                                         const struct dfi_index           *dfi,
                                                                                         guint                            *n_results);

const dfi_id *                          dfi_text_index_get_ids_for_exact_match          (const struct dfi_index           *dfi,
                                                                                         const struct dfi_text_index      *index,
                                                                                         const gchar                      *string,
                                                                                         guint                            *n_results);
void                                    dfi_text_index_prefix_search                    (const struct dfi_text_index      *text_index,
                                                                                         const struct dfi_index           *dfi,
                                                                                         const gchar                      *term,
                                                                                         const struct dfi_text_index_item **start,
                                                                                         const struct dfi_text_index_item **end);

const struct dfi_keyfile *              dfi_keyfile_from_pointer                        (const struct dfi_index           *dfi,
                                                                                         dfi_pointer                       pointer);

const struct dfi_keyfile_group *        dfi_keyfile_get_groups                          (const struct dfi_keyfile         *file,
                                                                                         const struct dfi_index           *dfi,
                                                                                         gint                             *n_groups);
const gchar *                           dfi_keyfile_group_get_name                      (const struct dfi_keyfile_group   *group,
                                                                                         const struct dfi_index           *dfi);

const struct dfi_keyfile_item *         dfi_keyfile_group_get_items                     (const struct dfi_keyfile_group   *group,
                                                                                         const struct dfi_index           *dfi,
                                                                                         const struct dfi_keyfile         *file,
                                                                                         gint                             *n_items);
const gchar *                           dfi_keyfile_item_get_key                        (const struct dfi_keyfile_item    *item,
                                                                                         const struct dfi_index           *dfi);
const gchar *                           dfi_keyfile_item_get_locale                     (const struct dfi_keyfile_item    *item,
                                                                                         const struct dfi_index           *dfi);
const gchar *                           dfi_keyfile_item_get_value                      (const struct dfi_keyfile_item    *item,
                                                                                         const struct dfi_index           *dfi);

guint                                   dfi_keyfile_get_n_groups                        (const struct dfi_keyfile         *keyfile);

void                                    dfi_keyfile_get_group_range                     (const struct dfi_keyfile         *keyfile,
                                                                                         guint                             group,
                                                                                         guint                            *start,
                                                                                         guint                            *end);

const gchar *                           dfi_keyfile_get_group_name                      (const struct dfi_keyfile         *keyfile,
                                                                                         const struct dfi_index           *dfi,
                                                                                         guint                             group);

void                                    dfi_keyfile_get_item                            (const struct dfi_keyfile         *keyfile,
                                                                                         const struct dfi_index           *dfi,
                                                                                         guint                             item,
                                                                                         const gchar                     **key,
                                                                                         const gchar                     **locale,
                                                                                         const gchar                     **value);
