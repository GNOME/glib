#pragma once

#include "gtypes.h"

G_BEGIN_DECLS

typedef struct {
  grefcount ref_count;

  gsize mem_size;

#ifndef G_DISABLE_ASSERT
  /* A "magic" number, used to perform additional integrity
   * checks on the allocated data
   */
  guint32 magic;
#endif
} GRcBox;

typedef struct {
  gatomicrefcount ref_count;

  gsize mem_size;

#ifndef G_DISABLE_ASSERT
  guint32 magic;
#endif
} GArcBox;

#define G_BOX_MAGIC             0x44ae2bf0

/* Keep the two refcounted boxes identical in size */
G_STATIC_ASSERT (sizeof (GRcBox) == sizeof (GArcBox));

#define G_RC_BOX_SIZE sizeof (GRcBox)
#define G_ARC_BOX_SIZE sizeof (GArcBox)

gpointer        g_rc_box_alloc_full     (gsize    block_size,
                                         gboolean atomic,
                                         gboolean clear);

G_END_DECLS
