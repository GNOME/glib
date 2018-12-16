#include "gstrfuncs.h"
#include <stdlib.h>

gsize
g_strlcpy_chk (gchar        *dest,
               const gchar  *src,
               gsize        size,
               gsize        dest_size)
{
  if (size > dest_size)
    abort ();
  return glib_g_strlcpy (dest, src, size);
}

