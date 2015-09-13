/* garraylist.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <string.h>

#include "garraylist.h"
#include "gmessages.h"
#include "gslice.h"

/**
 * SECTION:garraylist
 * @title: GArrayList
 * @short_description: Linked lists implemented as arrays.
 *
 * Sometimes, when building APIs we make mistakes about the undelrying
 * data structure that should have been used. GArrayList is a data structure
 * that allows read-only compatability with #GList but is implemented as an
 * array underneath. This means fast forward and backward iteration using
 * typical array style index access, while providing read-only #GList access
 * for compatability APIs.
 *
 * There is a cost associated with doing this, but the trade-off can be worth
 * it under certain scenarios.
 *
 * Mutating the array list is potentially more expensive than a #GList.
 * However, given no array growth is required, appending to the array list
 * can be performed in O(1).
 *
 * The ideal use case for this data structure is a read-heavy data set
 * where reverse iteration may be necessary and compatability with #GList
 * must be maintained.
 *
 * It is unlikely that you should be writing new APIs that use this
 * data structure.
 *
 * Since: 2.48
 */

#if 0
# define DEBUG_ASSERT(x) g_assert(x)
#else
# define DEBUG_ASSERT(x)
#endif

#define G_ARRAY_LIST_DEFAULT_ALLOC 8

/*
 * The following structures give us a data-structure that can be dynamic,
 * by changing it's implementation based on the number of items in the
 * #GArrayList. When we have 2 or fewer items, we simply embed the #GList
 * items in the toplevel structure.
 *
 * When this grows, we allocate an array to contain the #GList items and
 * update prev/next pionters as necessary.
 *
 * This structure allows for fast iteration forwards and backwards from any
 * arbitrary point in the structure, just like an array. It also allows for
 * returning a read-only #GList for when API compatability is needed.
 */

typedef struct
{
  guint          len;
  guint          flags;
  GDestroyNotify destroy;
  GList          items[2];
} GArrayListEmbed;

typedef struct
{
  guint           len;
  guint           flags;
  GDestroyNotify  destroy;

  /*
   * This is an array of GList items, which we keep in sync by updating
   * the next/prev pointers when necessary.
   */
  GList          *items;
  gsize           items_len;

  gpointer        padding[4];
} GArrayListAlloc;

typedef struct
{
  guint          len;
  guint          on_heap : 1;
  guint          mode : 31;
  GDestroyNotify destroy;
  gpointer       padding[6];
} GArrayListAny;

G_STATIC_ASSERT (sizeof (GArrayListAny) == sizeof (GArrayList));
G_STATIC_ASSERT (sizeof (GArrayListAny) == sizeof (GArrayListAlloc));
G_STATIC_ASSERT (sizeof (GArrayListAny) == sizeof (GArrayListEmbed));

enum {
  MODE_EMBED,
  MODE_ALLOC,
};

GArrayList *
g_array_list_new (GDestroyNotify destroy)
{
  GArrayListAny *any;

  any = g_slice_new0 (GArrayListAny);

  any->mode = MODE_EMBED;
  any->len = 0;
  any->destroy = destroy;
  any->on_heap = TRUE;

  return (GArrayList *)any;
}

void
g_array_list_init (GArrayList     *self,
                   GDestroyNotify  destroy)
{
  GArrayListAny *any = (GArrayListAny *)self;

  g_return_if_fail (self != NULL);

  memset (any, 0, sizeof *any);

  any->mode = MODE_EMBED;
  any->len = 0;
  any->destroy = destroy;
  any->on_heap = FALSE;
}

const GList *
g_array_list_peek (GArrayList *self)
{
  GArrayListAny *any = (GArrayListAny *)self;
  GArrayListEmbed *embed = (GArrayListEmbed *)self;
  GArrayListAlloc *alloc = (GArrayListAlloc *)self;

  g_return_val_if_fail (self != NULL, NULL);

  if (self->len == 0)
    return NULL;

  return (any->mode == MODE_EMBED) ? embed->items : (GList *)alloc->items;
}

gpointer
g_array_list_index (GArrayList *self,
                    gsize       index)
{
  GArrayListAny *any = (GArrayListAny *)self;
  GArrayListEmbed *embed = (GArrayListEmbed *)self;
  GArrayListAlloc *alloc = (GArrayListAlloc *)self;
  GList *items;

  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (index < self->len, NULL);

  DEBUG_ASSERT ((any->len < G_N_ELEMENTS (embed->items)) ||
                (any->mode == MODE_ALLOC));

  items = (any->mode == MODE_EMBED) ? embed->items : (GList *)alloc->items;

  return items [index].data;
}

static inline void
_g_array_list_update_pointers (GList *array,
                               gsize  array_len)
{
  gsize i;

  if (array_len > 0)
    array [0].next = &array [1];

  for (i = 1; i < (array_len - 1); i++)
    {
      array [i].prev = &array [i - 1];
      array [i].next = &array [i + 1];
    }

  if (array_len > 1)
    array [array_len- 1].prev = &array [array_len - 1 - 1];
}

static void
_g_array_list_grow (GArrayList *self,
                    gboolean    update_pointers)
{
  GArrayListAlloc *alloc = (GArrayListAlloc *)self;
  gpointer old_items;

  DEBUG_ASSERT (self != NULL);
  DEBUG_ASSERT (alloc->mode == MODE_ALLOC);
  DEBUG_ASSERT (alloc->len < G_MAXINT);
  DEBUG_ASSERT (alloc->len > 0);

  old_items = alloc->items;

  alloc->items_len <<= 1;
  alloc->items = g_realloc_n (alloc->items, alloc->items_len, sizeof (GList));

  if (G_LIKELY (old_items == alloc->items))
    return;

  if (!update_pointers)
    return;

  _g_array_list_update_pointers (alloc->items, alloc->len);
}

static void
_g_array_list_transition (GArrayList *self)
{
  GArrayListAny *any = (GArrayListAny *)self;
  GArrayListEmbed *embed = (GArrayListEmbed *)self;
  GArrayListAlloc *alloc = (GArrayListAlloc *)self;
  GList *items;
  gsize i;

  DEBUG_ASSERT (self != NULL);
  DEBUG_ASSERT (embed->mode == MODE_EMBED);
  DEBUG_ASSERT (embed->len == G_N_ELEMENTS (embed->items));

  items = g_malloc_n (G_ARRAY_LIST_DEFAULT_ALLOC, sizeof (GList));

  items [0].prev = NULL;
  items [0].data = embed->items [0].data;
  items [0].next = &items [1];

  for (i = 1; i < (G_N_ELEMENTS (embed->items) - 1); i++)
    {
      items [i].prev = &items [i - 1];
      items [i].next = &items [i + 1];
      items [i].data = &embed->items [i].data;
    }

  items [G_N_ELEMENTS (embed->items) - 1].prev = &items [G_N_ELEMENTS (embed->items) - 1 - 1];
  items [G_N_ELEMENTS (embed->items) - 1].next = NULL;
  items [G_N_ELEMENTS (embed->items) - 1].data = embed->items [G_N_ELEMENTS (embed->items) - 1].data;

  any->mode = MODE_ALLOC;

  alloc->items_len = G_ARRAY_LIST_DEFAULT_ALLOC;
  alloc->items = items;
}

void
g_array_list_add (GArrayList *self,
                  gpointer    data)
{
  GArrayListAny *any = (GArrayListAny *)self;
  GArrayListEmbed *embed = (GArrayListEmbed *)self;
  GArrayListAlloc *alloc = (GArrayListAlloc *)self;
  GList *item;
  GList *prev;

  g_return_if_fail (self != NULL);

  if (G_LIKELY (any->mode == MODE_EMBED))
    {
      if (G_LIKELY (embed->len < G_N_ELEMENTS (embed->items)))
        {
          if (G_LIKELY (embed->len == 0))
            {
              embed->items [0].prev = NULL;
              embed->items [0].next = NULL;
              embed->items [0].data = data;
            }
          else
            {
              prev = &embed->items [embed->len - 1];
              item = &embed->items [embed->len];

              item->prev = prev;
              item->next = NULL;
              item->data = data;

              prev->next = item;
            }

          embed->len++;

          DEBUG_ASSERT (embed->len <= G_N_ELEMENTS (embed->items));
          DEBUG_ASSERT (embed->len > 0);
          DEBUG_ASSERT (embed->items [embed->len - 1].data == data);

          return;
        }

      _g_array_list_transition (self);
    }

  DEBUG_ASSERT (any->mode == MODE_ALLOC);

  if (G_UNLIKELY (alloc->len == alloc->items_len))
    _g_array_list_grow (self, TRUE);

  item = &alloc->items [alloc->len];
  prev = (alloc->len > 0) ? &alloc->items [alloc->len - 1] : NULL;

  item->data = data;
  item->prev = NULL;
  item->next = NULL;

  if (prev)
    prev->next = item;

  alloc->len++;

  DEBUG_ASSERT (alloc->len <= alloc->items_len);
  DEBUG_ASSERT (alloc->len > 0);
  DEBUG_ASSERT (alloc->items [alloc->len - 1].data == data);
}

void
g_array_list_prepend (GArrayList *self,
                      gpointer    data)
{
  GArrayListAny *any = (GArrayListAny *)self;
  GArrayListEmbed *embed = (GArrayListEmbed *)self;
  GArrayListAlloc *alloc = (GArrayListAlloc *)self;

  g_return_if_fail (self != NULL);

  if (G_LIKELY (any->mode == MODE_EMBED))
    {
      if (G_LIKELY (embed->len < G_N_ELEMENTS (embed->items)))
        {
          memmove (&embed->items [1], &embed->items [0], sizeof (GList) * embed->len);
          embed->items [0].prev = NULL;
          embed->items [0].next = &embed->items [1];
          embed->items [0].data = data;
          embed->len++;
          return;
        }

      _g_array_list_transition (self);
    }

  DEBUG_ASSERT (any->mode == MODE_ALLOC);

  if (G_UNLIKELY (alloc->len == alloc->items_len))
    _g_array_list_grow (self, FALSE);

  memmove (&alloc->items [1], &alloc->items [0], sizeof (GList) * alloc->len);

  alloc->items [0].data = data;

  alloc->len++;

  _g_array_list_update_pointers (alloc->items, alloc->len);

  DEBUG_ASSERT (alloc->len <= alloc->items_len);
  DEBUG_ASSERT (alloc->len > 0);
  DEBUG_ASSERT (alloc->items [0].data == data);
}

void
g_array_list_remove_index (GArrayList *self,
                           guint       index)
{
  GArrayListAny *any = (GArrayListAny *)self;
  GArrayListEmbed *embed = (GArrayListEmbed *)self;
  GArrayListAlloc *alloc = (GArrayListAlloc *)self;
  GList *items;
  gpointer data = NULL;
  gsize i;

  g_return_if_fail (self != NULL);
  g_return_if_fail (index < self->len);

  if (G_LIKELY (any->mode == MODE_EMBED))
    {
      DEBUG_ASSERT (index < G_N_ELEMENTS (embed->items));

      data = embed->items [index].data;
      embed->len--;
      items = embed->items;
    }
  else
    {
      DEBUG_ASSERT (any->mode == MODE_ALLOC);
      DEBUG_ASSERT (index < alloc->items_len);

      data = alloc->items [index].data;
      alloc->len--;
      items = alloc->items;
    }

  if (index != alloc->len)
    memmove (&items [index],
             &items [index + 1],
             sizeof (GList) * (any->len - index));

  for (i = index; i < any->len; i++)
    {
      if (i > 0)
        items [i].prev = &items [i - 1];
      else
        items [i].prev = NULL;

      if (i < (any->len - 1))
        items [i].next = &items [i + 1];
      else
        items [i].next = NULL;
    }

  if (any->destroy)
    any->destroy (data);
}

gssize
g_array_list_find (GArrayList *self,
                   gpointer    data)
{
  GArrayListAny *any = (GArrayListAny *)self;
  GArrayListEmbed *embed = (GArrayListEmbed *)self;
  GArrayListAlloc *alloc = (GArrayListAlloc *)self;
  GList *items;
  gsize i;

  g_return_if_fail (self != NULL);

  items = (any->mode == MODE_EMBED) ? embed->items : alloc->items;

  for (i = 0; i < any->len; i++)
    {
      if (items [i].data == data)
        return i;
    }

  return -1;
}

void
g_array_list_remove (GArrayList *self,
                     gpointer    data)
{
  GArrayListAny *any = (GArrayListAny *)self;
  GArrayListEmbed *embed = (GArrayListEmbed *)self;
  GArrayListAlloc *alloc = (GArrayListAlloc *)self;
  GList *items;
  gsize i;

  g_return_if_fail (self != NULL);

  items = (any->mode == MODE_EMBED) ? embed->items : alloc->items;

  for (i = 0; i < any->len; i++)
    {
      if (items [i].data == data)
        {
          g_array_list_remove_index (self, i);
          return;
        }
    }

  g_warning ("Failed to locate %p in GArrayList", data);
}

void
g_array_list_destroy (GArrayList *self)
{
  GArrayListAny *any = (GArrayListAny *)self;
  GArrayListEmbed *embed = (GArrayListEmbed *)self;
  GArrayListAlloc *alloc = (GArrayListAlloc *)self;
  GList *items;
  gsize i;

  g_return_if_fail (self != NULL);

  if (any->mode == MODE_EMBED)
    items = embed->items;
  else
    items = alloc->items;

  if (any->destroy)
    for (i = 0; i < any->len; i++)
      any->destroy (items [i].data);

  if (any->mode == MODE_ALLOC)
    g_free (alloc->items);

  if (any->on_heap)
    g_slice_free (GArrayListAny, any);
}

const GList *
g_array_list_last_link (GArrayList *self)
{
  GArrayListAny *any = (GArrayListAny *)self;
  GArrayListEmbed *embed = (GArrayListEmbed *)self;
  GArrayListAlloc *alloc = (GArrayListAlloc *)self;
  GList *items;

  g_return_val_if_fail (self != NULL, NULL);

  if (self->len == 0)
    return NULL;

  if (any->mode == MODE_EMBED)
    items = embed->items;
  else
    items = alloc->items;

  return &items [self->len - 1];
}

void
g_array_list_clear (GArrayList *self)
{
  GArrayListAny *any = (GArrayListAny *)self;
  GArrayListEmbed *embed = (GArrayListEmbed *)self;
  GArrayListAlloc *alloc = (GArrayListAlloc *)self;

  g_return_if_fail (self != NULL);

  if (any->destroy != NULL)
    {
      GList *items;
      gsize i;

      items = (any->mode == MODE_EMBED) ? embed->items : alloc->items;

      for (i = 0; i < any->len; i++)
        any->destroy (items [i].data);
    }

  if (any->mode == MODE_ALLOC)
    g_free (alloc->items);

  any->len = 0;
  any->mode = MODE_EMBED;
}

/**
 * g_array_list_copy:
 * @self: A #GArrayList.
 * @copy_func: (scope call): A #GCopyFunc
 * @copy_data: (allow-none): data for @copy_func or %NULL.
 *
 * Creates a new array containing the data found within the #GListArray.
 * The result should be freed with g_free() when no longer used. If you
 * incremented a reference count or allocated a new structure in your
 * copy func, you are responsible for freeing it as well.
 *
 * Returns: (transfer full): A newly allocated array that should be freed
 *   with g_free().
 */
gpointer *
g_array_list_copy (GArrayList *self,
                   GCopyFunc   copy_func, 
                   gpointer    copy_data)
{
  GArrayListAny *any = (GArrayListAny *)self;
  GArrayListEmbed *embed = (GArrayListEmbed *)self;
  GArrayListAlloc *alloc = (GArrayListAlloc *)self;
  const GList *items;
  gpointer *ret;
  gsize i;

  g_return_if_fail (self != NULL);

  ret = g_malloc_n (self->len, sizeof (gpointer));
  items = (any->mode == MODE_EMBED) ? embed->items : alloc->items;

  if (!copy_func)
    {
      for (i = 0; i < self->len; i++)
        ret [i] = items [i].data;
    }
  else
    {
      for (i = 0; i < self->len; i++)
        ret [i] = copy_func (items [i].data, copy_data);
    }

  return ret;
}

/**
 * g_array_list_copy_reversed:
 * @self: A #GArrayList.
 * @copy_func: (scope call): A #GCopyFunc
 * @copy_data: (allow-none): data for @copy_func or %NULL.
 *
 * Creates a new array containing the data found within the #GListArray.
 * The result should be freed with g_free() when no longer used. If you
 * incremented a reference count or allocated a new structure in your
 * copy func, you are responsible for freeing it as well.
 *
 * This function creates the array in reverse order.
 *
 * Returns: (transfer full): A newly allocated array that should be freed
 *   with g_free().
 */
gpointer *
g_array_list_copy_reversed (GArrayList *self,
                            GCopyFunc   copy_func, 
                            gpointer    copy_data)
{
  GArrayListAny *any = (GArrayListAny *)self;
  GArrayListEmbed *embed = (GArrayListEmbed *)self;
  GArrayListAlloc *alloc = (GArrayListAlloc *)self;
  const GList *items;
  gpointer *ret;
  gsize i;

  g_return_if_fail (self != NULL);

  ret = g_malloc_n (self->len, sizeof (gpointer));
  items = (any->mode == MODE_EMBED) ? embed->items : alloc->items;

  if (!copy_func)
    {
      for (i = self->len; i > 0; i--)
        ret [i-1] = items [i-1].data;
    }
  else
    {
      for (i = self->len; i > 0; i--)
        ret [i-1] = copy_func (items [i-1].data, copy_data);
    }

  return ret;
}
