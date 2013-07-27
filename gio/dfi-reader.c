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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "dfi-reader.h"

#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <unistd.h>

/* struct dfi_index struct {{{1 */
struct dfi_index
{
  gchar                        *data;
  guint32                       file_size;

  const struct dfi_string_list *app_names;
  const struct dfi_string_list *key_names;
  const struct dfi_string_list *locale_names;
  const struct dfi_string_list *group_names;

  const struct dfi_pointer_array *implementors;    /* id lists, associated with group_names */
  const struct dfi_pointer_array            *text_indexes;    /* text indexes, associated with locale_names */
  const struct dfi_pointer_array            *desktop_files;   /* desktop files, associated with app_names */

  const struct dfi_text_index  *mime_types;
};

/* dfi_uint16, dfi_uint32 {{{1 */

static guint
dfi_uint16_get (dfi_uint16 value)
{
  return GUINT16_FROM_LE (value.le);
}

static guint
dfi_uint32_get (dfi_uint32 value)
{
  return GUINT32_FROM_LE (value.le);
}

/* dfi_string {{{1 */

static gboolean
dfi_string_is_flagged (dfi_string string)
{
  return (dfi_uint32_get(string.offset) & (1u << 31)) != 0;
}

static const gchar *
dfi_string_get (const struct dfi_index *dfi,
                dfi_string              string)
{
  guint32 offset = dfi_uint32_get (string.offset);

  offset &= ~(1u << 31);

  if (offset < dfi->file_size)
    return dfi->data + offset;
  else
    return "";
}

/* dfi_pointer, dfi_pointer_array {{{1 */
static gconstpointer
dfi_pointer_dereference (const struct dfi_index *dfi,
                         dfi_pointer             pointer,
                         guint                   min_size)
{
  guint offset = dfi_uint32_get (pointer.offset);

  /* Check to make sure we don't wrap */
  if (offset + min_size < min_size)
    return NULL;

  /* Check to make sure we don't pass the end of the file */
  if (offset + min_size > dfi->file_size)
    return NULL;

  return dfi->data + offset;
}

static gconstpointer
dfi_pointer_dereference_unchecked (const struct dfi_index *dfi,
                                   dfi_pointer             pointer)
{
  guint offset = dfi_uint32_get (pointer.offset);

  return dfi->data + offset;
}

static const struct dfi_pointer_array *
dfi_pointer_array_from_pointer (const struct dfi_index *dfi,
                                dfi_pointer             pointer)
{
  const struct dfi_pointer_array *array;
  const struct dfi_string_list *keys;
  guint need_size;

  need_size = sizeof (dfi_pointer);

  array = dfi_pointer_dereference (dfi, pointer, need_size);
  if (array == NULL)
    return NULL;

  keys = dfi_string_list_from_pointer (dfi, array->associated_string_list);
  if (keys == NULL)
    return NULL;

  /* string list length is 16bit, so no overflow danger */
  need_size += sizeof (dfi_pointer) * dfi_string_list_get_length (keys);

  return dfi_pointer_dereference (dfi, pointer, need_size);
}

guint
dfi_pointer_array_get_length (const struct dfi_pointer_array *array,
                              const struct dfi_index         *dfi)
{
  const struct dfi_string_list *keys;

  keys = dfi_pointer_dereference_unchecked (dfi, array->associated_string_list);

  return dfi_uint16_get (keys->n_strings);
}

const gchar *
dfi_pointer_array_get_item_key (const struct dfi_pointer_array *array,
                                const struct dfi_index         *dfi,
                                gint                            i)
{
  const struct dfi_string_list *keys;

  keys = dfi_pointer_dereference_unchecked (dfi, array->associated_string_list);

  return dfi_string_get (dfi, keys->strings[i]);
}

dfi_pointer
dfi_pointer_array_get_pointer (const struct dfi_pointer_array *array,
                               gint                            i)
{
  return array->pointers[i];
}

/* dfi_id, dfi_id_list {{{1 */

gboolean
dfi_id_valid (dfi_id id)
{
  return dfi_uint16_get (id) != 0xffff;
}

guint
dfi_id_get (dfi_id id)
{
  return dfi_uint16_get (id);
}

const dfi_id *
dfi_id_list_get_ids (const struct dfi_id_list *list,
                     guint                    *n_ids)
{
  if (list == NULL)
    {
      *n_ids = 0;
      return NULL;
    }

  *n_ids = dfi_uint16_get (list->n_ids);

  return list->ids;
}

const struct dfi_id_list *
dfi_id_list_from_pointer (const struct dfi_index *dfi,
                          dfi_pointer             pointer)
{
  const struct dfi_id_list *list;
  guint need_size;

  need_size = sizeof (dfi_uint16);

  list = dfi_pointer_dereference (dfi, pointer, need_size);

  if (!list)
    return NULL;

  /* n_ids is 16bit, so no overflow danger */
  need_size += sizeof (dfi_id) * dfi_uint16_get (list->n_ids);

  return dfi_pointer_dereference (dfi, pointer, need_size);
}

/* dfi_string_list {{{1 */

const struct dfi_string_list *
dfi_string_list_from_pointer (const struct dfi_index *dfi,
                              dfi_pointer             pointer)
{
  const struct dfi_string_list *list;
  guint need_size;

  need_size = sizeof (dfi_uint16);

  list = dfi_pointer_dereference (dfi, pointer, need_size);

  if (!list)
    return NULL;

  /* n_strings is 16bit, so no overflow danger */
  need_size += sizeof (dfi_string) * dfi_uint16_get (list->n_strings);

  return dfi_pointer_dereference (dfi, pointer, need_size);
}

gint
dfi_string_list_binary_search (const struct dfi_string_list *list,
                               const struct dfi_index       *dfi,
                               const gchar                  *string)
{
  guint l, r;

  l = 0;
  r = dfi_uint16_get (list->n_strings);

  while (l < r)
    {
      guint m;
      gint x;

      m = l + (r - l) / 2;

      x = strcmp (string, dfi_string_get (dfi, list->strings[m]));

      if (x > 0)
        l = m + 1;
      else if (x < 0)
        r = m;
      else
        return m;
    }

  return -1;
}

guint
dfi_string_list_get_length (const struct dfi_string_list *list)
{
  return dfi_uint16_get (list->n_strings);
}

const gchar *
dfi_string_list_get_string_at_index (const struct dfi_string_list *list,
                                     const struct dfi_index       *dfi,
                                     gint                          i)
{
  return dfi_string_get (dfi, list->strings[i]);
}

const gchar *
dfi_string_list_get_string (const struct dfi_string_list *list,
                            const struct dfi_index       *dfi,
                            dfi_id                        id)
{
  gint i = dfi_id_get (id);

  if (list == NULL)
    return NULL;

  if (i == 0xffff)
    return NULL;

  if (i < dfi_uint16_get (list->n_strings))
    return dfi_string_list_get_string_at_index (list, dfi, i);
  else
    return "";
}

/* dfi_text_index, dfi_text_index_item {{{1 */

const struct dfi_text_index *
dfi_text_index_from_pointer (const struct dfi_index *dfi,
                             dfi_pointer             pointer)
{
  const struct dfi_text_index *text_index;
  guint need_size;
  guint n_items;

  need_size = sizeof (dfi_uint16);

  text_index = dfi_pointer_dereference (dfi, pointer, need_size);

  if (!text_index)
    return NULL;

  /* It's 32 bit, so make sure this won't overflow when we multiply */
  n_items = dfi_uint32_get (text_index->n_items);
  if (n_items > (1u << 24))
    return NULL;

  need_size += sizeof (struct dfi_text_index_item) * n_items;

  return dfi_pointer_dereference (dfi, pointer, need_size);
}

const gchar *
dfi_text_index_get_string (const struct dfi_index      *dfi,
                           const struct dfi_text_index *text_index,
                           dfi_id                       id)
{
  guint i = dfi_id_get (id);

  if G_UNLIKELY (text_index == NULL)
    return "";

  if (i < dfi_uint32_get (text_index->n_items))
    return dfi_string_get (dfi, text_index->items[i].key);
  else
    return "";
}

const struct dfi_text_index_item *
dfi_text_index_binary_search (const struct dfi_text_index *text_index,
                              const struct dfi_index      *dfi,
                              const gchar                 *string)
{
  guint l, r;

  if G_UNLIKELY (text_index == NULL)
    return NULL;

  l = 0;
  r = dfi_uint32_get (text_index->n_items);

  while (l < r)
    {
      guint m;
      gint x;

      m = l + (r - l) / 2;

      x = strcmp (string, dfi_string_get (dfi, text_index->items[m].key));

      if (x > 0)
        l = m + 1;
      else if (x < 0)
        r = m;
      else
        return text_index->items + m;
    }

  return NULL;
}

const dfi_id *
dfi_text_index_item_get_ids (const struct dfi_text_index_item *item,
                             const struct dfi_index           *dfi,
                             guint                            *n_results)
{
  if (item == NULL)
    return NULL;

  if (dfi_string_is_flagged (item->key))
    {
      if (!dfi_id_valid (item->value.pair[0]))
        {
          *n_results = 0;
          return NULL;
        }
      else if (!dfi_id_valid (item->value.pair[1]))
        {
          *n_results = 1;
          return item->value.pair;
        }
      else
        {
          *n_results = 2;
          return item->value.pair;
        }
    }
  else
    return dfi_id_list_get_ids (dfi_id_list_from_pointer (dfi, item->value.pointer), n_results);
}

const dfi_id *
dfi_text_index_get_ids_for_exact_match (const struct dfi_index      *dfi,
                                        const struct dfi_text_index *index,
                                        const gchar                 *string,
                                        guint                       *n_results)
{
  const struct dfi_text_index_item *item;

  item = dfi_text_index_binary_search (index, dfi, string);

  return dfi_text_index_item_get_ids (item, dfi, n_results);
}

void
dfi_text_index_prefix_search (const struct dfi_text_index       *text_index,
                              const struct dfi_index            *dfi,
                              const gchar                       *term,
                              const struct dfi_text_index_item **start,
                              const struct dfi_text_index_item **end)
{
  guint l, r, n;
  gint termlen;

  if G_UNLIKELY (text_index == NULL)
    {
      *start = NULL;
      *end = NULL;

      return;
    }

  termlen = strlen (term);

  n = dfi_uint32_get (text_index->n_items);

  l = 0;
  r = n;

  /* Need to find the exact match or the item after where the exact
   * match would have been (ie: where the insertion point would be for
   * the match).
   *
   * We can be sure that this is the case by splitting to the left once
   * we get down to the last item (ie: select the "middle item" by
   * rounding down.
   */
  while (l < r)
    {
      guint m;
      gint x;

      m = l + (r - l) / 2;

      x = strcmp (term, dfi_string_get (dfi, text_index->items[m].key));

      if (x > 0)
        l = m + 1;
      else if (x < 0)
        r = m;
      else
        {
          l = m;
          break;
        }
    }

  /* Now 'l' points at the start item */
  *start = &text_index->items[l];

  /* Just iterate until we find the first item that's not a prefix.  We
   * could binary-search, but this way is easier and probably just as
   * fast in most cases (if not faster).
   */
  for (r = l; r < n; r++)
    if (strncmp (dfi_string_get (dfi, text_index->items[r].key), term, termlen) != 0)
      break;

  *end = &text_index->items[r];
}

/* dfi_keyfile, dfi_keyfile_group, dfi_keyfile_item {{{1 */
const struct dfi_keyfile *
dfi_keyfile_from_pointer (const struct dfi_index *dfi,
                          dfi_pointer             pointer)
{
  const struct dfi_keyfile *file;
  guint need_size;

  need_size = sizeof (struct dfi_keyfile);

  file = dfi_pointer_dereference (dfi, pointer, need_size);

  if (!file)
    return NULL;

  /* All sizes 16bit ints, so no overflow danger */
  need_size += sizeof (struct dfi_keyfile_group) * dfi_uint16_get (file->n_groups);
  need_size += sizeof (struct dfi_keyfile_item) * dfi_uint16_get (file->n_items);

  return dfi_pointer_dereference (dfi, pointer, need_size);
}

guint
dfi_keyfile_get_n_groups (const struct dfi_keyfile *keyfile)
{
  return dfi_uint16_get (keyfile->n_groups);
}

void
dfi_keyfile_get_group_range (const struct dfi_keyfile *keyfile,
                             guint                     group,
                             guint                    *start,
                             guint                    *end)
{
  const struct dfi_keyfile_group *kfg;

  {
    const struct dfi_keyfile_group *start_of_groups = (gpointer) (keyfile + 1);
    kfg = start_of_groups + group;
  }

  *start = dfi_uint16_get (kfg->items_index);

  if (group < dfi_uint16_get (keyfile->n_groups) - 1)
    *end = dfi_uint16_get ((kfg + 1)->items_index);
  else
    *end = dfi_uint16_get (keyfile->n_items);
}

const gchar *
dfi_keyfile_get_group_name (const struct dfi_keyfile *keyfile,
                            const struct dfi_index   *dfi,
                            guint                     group)
{
  const struct dfi_keyfile_group *kfg;

  {
    const struct dfi_keyfile_group *start_of_groups = (gpointer) (keyfile + 1);
    kfg = start_of_groups + group;
  }

  kfg = ((const struct dfi_keyfile_group *) (keyfile + 1)) + group;

  return dfi_string_list_get_string (dfi->group_names, dfi, kfg->name_id);
}

void
dfi_keyfile_get_item (const struct dfi_keyfile  *keyfile,
                      const struct dfi_index    *dfi,
                      guint                      item,
                      const gchar              **key,
                      const gchar              **locale,
                      const gchar              **value)
{
  const struct dfi_keyfile_item *kfi;

  {
    const struct dfi_keyfile_group *start_of_groups = (gpointer) (keyfile + 1);
    const struct dfi_keyfile_group *end_of_groups = start_of_groups + dfi_uint16_get (keyfile->n_groups);
    const struct dfi_keyfile_item *start_of_items = (gpointer) end_of_groups;
    kfi = start_of_items + item;
  }

  *key = dfi_string_list_get_string (dfi->key_names, dfi, kfi->key_id);
  *locale = dfi_string_list_get_string (dfi->locale_names, dfi, kfi->locale_id);
  *value = dfi_string_get (dfi, kfi->value);
}

const struct dfi_keyfile_group *
dfi_keyfile_get_groups (const struct dfi_keyfile *file,
                        const struct dfi_index   *dfi,
                        gint                     *n_groups)
{
  *n_groups = dfi_uint16_get (file->n_groups);

  return G_STRUCT_MEMBER_P (file, sizeof (struct dfi_keyfile));
}

const gchar *
dfi_keyfile_group_get_name (const struct dfi_keyfile_group *group,
                            const struct dfi_index         *dfi)
{
  return dfi_string_list_get_string (dfi->group_names, dfi, group->name_id);
}

const struct dfi_keyfile_item *
dfi_keyfile_group_get_items (const struct dfi_keyfile_group *group,
                             const struct dfi_index         *dfi,
                             const struct dfi_keyfile       *file,
                             gint                           *n_items)
{
  guint start, end;
  guint n_groups;

  G_STATIC_ASSERT (sizeof (struct dfi_keyfile_group) == sizeof (struct dfi_keyfile));

  start = dfi_uint16_get (group->items_index);

  n_groups = dfi_uint16_get (file->n_groups);
  if ((gpointer) (file + n_groups) == (gpointer) group)
    end = dfi_uint16_get (file->n_items);
  else
    end = dfi_uint16_get (group[1].items_index);

  if (start <= end && end <= dfi_uint16_get (file->n_items))
    {
      *n_items = end - start;
      return G_STRUCT_MEMBER_P (file, sizeof (struct dfi_keyfile) +
                                      sizeof (struct dfi_keyfile_group) * dfi_uint16_get (file->n_groups) +
                                      sizeof (struct dfi_keyfile_item) * start);
    }
  else
    {
      *n_items = 0;
      return NULL;
    }
}

const gchar *
dfi_keyfile_item_get_key (const struct dfi_keyfile_item *item,
                          const struct dfi_index        *dfi)
{
  return dfi_string_list_get_string (dfi->key_names, dfi, item->key_id);
}

const gchar *
dfi_keyfile_item_get_locale (const struct dfi_keyfile_item *item,
                             const struct dfi_index        *dfi)
{
  return dfi_string_list_get_string (dfi->locale_names, dfi, item->locale_id);
}

const gchar *
dfi_keyfile_item_get_value (const struct dfi_keyfile_item *item,
                            const struct dfi_index        *dfi)
{
  return dfi_string_get (dfi, item->value);
}

/* dfi_header {{{1 */

static const struct dfi_header *
dfi_header_get (const struct dfi_index *dfi)
{
  dfi_pointer ptr = { };

  return dfi_pointer_dereference (dfi, ptr, sizeof (struct dfi_header));
}

/* struct dfi_index implementation {{{1 */

void
dfi_index_free (struct dfi_index *dfi)
{
  munmap (dfi->data, dfi->file_size);
  free (dfi);
}

static gpointer
dfi_index_map_file (const gchar   *directory,
                    gint          *dir_fd_out,
                    struct stat   *dir_buf,
                    struct stat   *file_buf,
                    gchar       ***extra_dirs)
{
  gpointer mapping = NULL;
  gint file_fd = -1;
  gint dir_fd = -1;

  dir_fd = open (directory, O_DIRECTORY);
  if (dir_fd < 0)
    goto out;

  if (fstat (dir_fd, dir_buf) < 0)
    goto out;

  file_fd = openat (dir_fd, "desktop-file-index", O_RDONLY);

  if (file_fd < 0)
    goto out;

  if (fstat (file_fd, file_buf) < 0)
    goto out;

  if (file_buf->st_size > G_MAXUINT32)
    goto out;

  //if (file_buf->st_mtime < dir_buf->st_mtime)
    //goto out;

  //if (dir_buf->st_nlink != 2)
    //goto out;

  mapping = mmap (NULL, file_buf->st_size, PROT_READ, MAP_SHARED, file_fd, 0);

  if (mapping == MAP_FAILED)
    {
      mapping = NULL;
      goto out;
    }

  madvise (mapping, file_buf->st_size, MADV_RANDOM);

out:
  if (file_fd != -1)
    close (file_fd);

  if (dir_fd_out)
    *dir_fd_out = dir_fd;

  else if (dir_fd != -1)
    close (dir_fd);

  return mapping;
}

struct dfi_index *
dfi_index_new (const gchar *directory)
{
  struct stat dir_buf, file_buf;

  return dfi_index_new_full (directory, NULL, &dir_buf, &file_buf, NULL);
}

struct dfi_index *
dfi_index_new_full (const gchar   *directory,
                    gint          *dir_fd,
                    struct stat   *dir_buf,
                    struct stat   *file_buf,
                    gchar       ***extra_dirs)
{
  const struct dfi_header *header;
  struct dfi_index *dfi;
  gpointer data;

  data = dfi_index_map_file (directory, dir_fd, dir_buf, file_buf, extra_dirs);

  if (data == NULL)
    return NULL;

  dfi = malloc (sizeof (struct dfi_index));
  dfi->data = data;
  dfi->file_size = file_buf->st_size;

  header = dfi_header_get (dfi);
  if (!header)
    goto err;

  dfi->app_names = dfi_string_list_from_pointer (dfi, header->app_names);
  dfi->key_names = dfi_string_list_from_pointer (dfi, header->key_names);
  dfi->locale_names = dfi_string_list_from_pointer (dfi, header->locale_names);
  dfi->group_names = dfi_string_list_from_pointer (dfi, header->group_names);

  if (!dfi->app_names || !dfi->key_names || !dfi->locale_names || !dfi->group_names)
   goto err;

  dfi->implementors = dfi_pointer_array_from_pointer (dfi, header->implementors);
  dfi->text_indexes = dfi_pointer_array_from_pointer (dfi, header->text_indexes);
  dfi->desktop_files = dfi_pointer_array_from_pointer (dfi, header->desktop_files);
  dfi->mime_types = dfi_text_index_from_pointer (dfi, header->mime_types);

 // if (!dfi->mime_types || !dfi->implementors || !dfi->text_indexes || !dfi->desktop_files)
   // goto err;

  return dfi;

err:
  dfi_index_free (dfi);

  return NULL;
}

const struct dfi_pointer_array *
dfi_index_get_desktop_files (const struct dfi_index *dfi)
{
  return dfi->desktop_files;
}

const struct dfi_string_list *
dfi_index_get_app_names (const struct dfi_index *dfi)
{
  return dfi->app_names;
}

const struct dfi_string_list *
dfi_index_get_key_names (const struct dfi_index *dfi)
{
  return dfi->key_names;
}

const struct dfi_string_list *
dfi_index_get_locale_names (const struct dfi_index *dfi)
{
  return dfi->locale_names;
}

const struct dfi_string_list *
dfi_index_get_group_names (const struct dfi_index *dfi)
{
  return dfi->group_names;
}

const struct dfi_pointer_array *
dfi_index_get_text_indexes (const struct dfi_index *dfi)
{
  return dfi->text_indexes;
}

/* Epilogue {{{1 */
/* vim:set foldmethod=marker: */
