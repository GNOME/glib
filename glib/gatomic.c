/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * GAtomic: atomic integer operation.
 * Copyright (C) 2003 Sebastian Wilhelmi
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
 
#include <glib.h>

#ifdef G_THREADS_ENABLED
# if !defined (G_ATOMIC_USE_FALLBACK_IMPLEMENTATION)
/* We have an inline implementation, which we can now use for the
 * fallback implementation. This fallback implementation is only used by
 * modules, which are not compliled with gcc 
 */

gint32
g_atomic_int_exchange_and_add_fallback (gint32 *atomic, 
					gint32  val)
{
  return g_atomic_int_exchange_and_add (atomic, val);
}


void
g_atomic_int_add_fallback (gint32 *atomic,
			   gint32  val)
{
  g_atomic_int_add (atomic, val);
}

gboolean
g_atomic_int_compare_and_exchange_fallback (gint32 *atomic, 
					    gint32  oldval, 
					    gint32  newval)
{
  return g_atomic_int_compare_and_exchange (atomic, oldval, newval);
}

gboolean
g_atomic_pointer_compare_and_exchange_fallback (gpointer *atomic, 
						gpointer  oldval, 
						gpointer  newval)
{
  return g_atomic_pointer_compare_and_exchange (atomic, oldval, newval);
}

gint32
g_atomic_int_get_fallback (gint32 *atomic)
{
  return g_atomic_int_get (atomic);
}

gint32
g_atomic_pointer_get_fallback (gpointer *atomic)
{
  return g_atomic_int_get (atomic);
}   

# else /* !G_ATOMIC_USE_FALLBACK_IMPLEMENTATION */
/* We have to use the slow, but safe locking method */
G_LOCK_DEFINE_STATIC (g_atomic_lock);

gint32
g_atomic_int_exchange_and_add_fallback (gint32 *atomic, 
					gint32  val)
{
  gint32 result;
    
  G_LOCK (g_atomic_lock);
  result = *atomic;
  *atomic += val;
  G_UNLOCK (g_atomic_lock);

  return result;
}


void
g_atomic_int_add_fallback (gint32 *atomic,
			   gint32  val)
{
  G_LOCK (g_atomic_lock);
  *atomic += val;
  G_UNLOCK (g_atomic_lock);
}

gboolean
g_atomic_int_compare_and_exchange_fallback (gint32 *atomic, 
					    gint32  oldval, 
					    gint32  newval)
{
  gboolean result;
    
  G_LOCK (g_atomic_lock);
  if (*atomic == oldval)
    {
      result = TRUE;
      *atomic = newval;
    }
  else
    result = FALSE;
  G_UNLOCK (g_atomic_lock);

  return result;
}

gboolean
g_atomic_pointer_compare_and_exchange_fallback (gpointer *atomic, 
						gpointer  oldval, 
						gpointer  newval)
{
  gboolean result;
    
  G_LOCK (g_atomic_lock);
  if (*atomic == oldval)
    {
      result = TRUE;
      *atomic = newval;
    }
  else
    result = FALSE;
  G_UNLOCK (g_atomic_lock);

  return result;
}

static inline gint32
g_atomic_int_get_fallback (gint32 *atomic)
{
  gint32 result;

  G_LOCK (g_atomic_lock);
  result = *atomic;
  G_UNLOCK (g_atomic_lock);

  return result;
}

static inline gpointer
g_atomic_pointer_get_fallback (gpointer *atomic)
{
  gpointer result;

  G_LOCK (g_atomic_lock);
  result = *atomic;
  G_UNLOCK (g_atomic_lock);

  return result;
}   


# endif /* G_ATOMIC_USE_FALLBACK_IMPLEMENTATION */
#else /* !G_THREADS_ENABLED */
gint32 g_atomic_int_exchange_and_add (gint32 *atomic, 
				      gint32  val)
{
  gint32 result = *atomic;
  *atomic += val;
  return result;
}
#endif /* G_THREADS_ENABLED */

