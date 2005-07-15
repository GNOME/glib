/* GObject - GLib Type, Object, Parameter and Signal Library
 * Copyright (C) 2000-2001 Red Hat, Inc.
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
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include	"gclosure.h"

#include	"gvalue.h"
#include 	"gobjectalias.h"
#include	<string.h>


/* FIXME: need caching allocators
 */

#define	CLOSURE_MAX_REF_COUNT		((1 << 15) - 1)
#define	CLOSURE_MAX_N_GUARDS		((1 << 1) - 1)
#define	CLOSURE_MAX_N_FNOTIFIERS	((1 << 2) - 1)
#define	CLOSURE_MAX_N_INOTIFIERS	((1 << 8) - 1)
#define	CLOSURE_N_MFUNCS(cl)		((cl)->meta_marshal + \
                                         ((cl)->n_guards << 1L))
/* same as G_CLOSURE_N_NOTIFIERS() (keep in sync) */
#define	CLOSURE_N_NOTIFIERS(cl)		(CLOSURE_N_MFUNCS (cl) + \
                                         (cl)->n_fnotifiers + \
                                         (cl)->n_inotifiers)

/* union of first int we need to make atomic */
typedef union {
  GClosure bits;
  gint atomic;
} GAtomicClosureBits;

#define BITS_AS_INT(b)	(((GAtomicClosureBits*)(b))->atomic)

#define CLOSURE_READ_BITS(cl,bits)	(BITS_AS_INT(bits) = g_atomic_int_get ((gint*)(cl)))
#define CLOSURE_READ_BITS2(cl,old,new)	(BITS_AS_INT(old) = CLOSURE_READ_BITS (cl, new))
#define CLOSURE_SWAP_BITS(cl,old,new)	(g_atomic_int_compare_and_exchange ((gint*)(cl),	\
						BITS_AS_INT(old),BITS_AS_INT(new)))

#define CLOSURE_REF(closure)  							\
G_STMT_START {									\
  GClosure old, new;								\
  do {										\
    CLOSURE_READ_BITS2 (closure, &old, &new);					\
    new.ref_count++;								\
  }										\
  while (!CLOSURE_SWAP_BITS (closure, &old, &new));				\
} G_STMT_END


#define CLOSURE_UNREF(closure, is_zero)  					\
G_STMT_START {									\
  GClosure old, new;								\
  do {										\
    CLOSURE_READ_BITS2 (closure, &old, &new);					\
    if (old.ref_count == 1)	/* last unref, invalidate first */		\
      g_closure_invalidate ((closure));						\
    new.ref_count--;								\
    is_zero = (new.ref_count == 0);						\
  }										\
  while (!CLOSURE_SWAP_BITS (closure, &old, &new));				\
} G_STMT_END

enum {
  FNOTIFY,
  INOTIFY,
  PRE_NOTIFY,
  POST_NOTIFY
};


/* --- functions --- */
GClosure*
g_closure_new_simple (guint           sizeof_closure,
		      gpointer        data)
{
  GClosure *closure;

  g_return_val_if_fail (sizeof_closure >= sizeof (GClosure), NULL);

  closure = g_malloc (sizeof_closure);
  closure->ref_count = 1;
  closure->meta_marshal = 0;
  closure->n_guards = 0;
  closure->n_fnotifiers = 0;
  closure->n_inotifiers = 0;
  closure->in_inotify = FALSE;
  closure->floating = TRUE;
  closure->derivative_flag = 0;
  closure->in_marshal = FALSE;
  closure->is_invalid = FALSE;
  closure->marshal = NULL;
  closure->data = data;
  closure->notifiers = NULL;
  memset (G_STRUCT_MEMBER_P (closure, sizeof (*closure)), 0, sizeof_closure - sizeof (*closure));

  return closure;
}

static inline void
closure_invoke_notifiers (GClosure *closure,
			  guint     notify_type)
{
  GClosure bits, new;

  /* notifier layout:
   *     meta_marshal  n_guards    n_guards     n_fnotif.  n_inotifiers
   * ->[[meta_marshal][pre_guards][post_guards][fnotifiers][inotifiers]]
   *
   * CLOSURE_N_MFUNCS(cl)    = meta_marshal + n_guards + n_guards;
   * CLOSURE_N_NOTIFIERS(cl) = CLOSURE_N_MFUNCS(cl) + n_fnotifiers + n_inotifiers
   *
   * constrains/catches:
   * - closure->notifiers may be reloacted during callback
   * - closure->n_fnotifiers and closure->n_inotifiers may change during callback
   * - i.e. callbacks can be removed/added during invocation
   * - have to prepare for callback removal during invocation (->marshal & ->data)
   * - have to distinguish (->marshal & ->data) for INOTIFY/FNOTIFY (->in_inotify)
   * + closure->n_guards is const during PRE_NOTIFY & POST_NOTIFY
   * + closure->meta_marshal is const for all cases
   * + none of the callbacks can cause recursion
   * + closure->n_inotifiers is const 0 during FNOTIFY
   */
  switch (notify_type)
    {
      GClosureNotifyData *ndata;
      guint i, offs;
    case FNOTIFY:
      CLOSURE_READ_BITS (closure, &bits);
      while (bits.n_fnotifiers)
	{
	  register guint n = --bits.n_fnotifiers;

	  ndata = closure->notifiers + CLOSURE_N_MFUNCS (&bits) + n;
	  closure->marshal = (GClosureMarshal) ndata->notify;
	  closure->data = ndata->data;
	  ndata->notify (ndata->data, closure);
	}
      closure->marshal = NULL;
      closure->data = NULL;
      break;
    case INOTIFY:
      do {
        CLOSURE_READ_BITS2 (closure, &bits, &new);
        new.in_inotify = TRUE;
      }
      while (!CLOSURE_SWAP_BITS (closure, &bits,  &new));

      while (bits.n_inotifiers)
	{
          register guint n = --bits.n_inotifiers;

	  ndata = closure->notifiers + CLOSURE_N_MFUNCS (&bits) + bits.n_fnotifiers + n;
	  closure->marshal = (GClosureMarshal) ndata->notify;
	  closure->data = ndata->data;
	  ndata->notify (ndata->data, closure);
	}
      closure->marshal = NULL;
      closure->data = NULL;
      do {
        CLOSURE_READ_BITS2 (closure, &bits, &new);
        new.n_inotifiers = 0;
        new.in_inotify = FALSE;
      }
      while (!CLOSURE_SWAP_BITS (closure, &bits, &new));
      break;
    case PRE_NOTIFY:
      CLOSURE_READ_BITS (closure, &bits);
      i = bits.n_guards;
      offs = bits.meta_marshal;
      while (i--)
	{
	  ndata = closure->notifiers + offs + i;
	  ndata->notify (ndata->data, closure);
	}
      break;
    case POST_NOTIFY:
      CLOSURE_READ_BITS (closure, &bits);
      i = bits.n_guards;
      offs = bits.meta_marshal + i;
      while (i--)
	{
	  ndata = closure->notifiers + offs + i;
	  ndata->notify (ndata->data, closure);
	}
      break;
    }
}

void
g_closure_set_meta_marshal (GClosure       *closure,
			    gpointer        marshal_data,
			    GClosureMarshal meta_marshal)
{
  GClosureNotifyData *old_notifiers, *new_notifiers;
  guint n;
  GClosure old, new;

  g_return_if_fail (closure != NULL);
  g_return_if_fail (meta_marshal != NULL);

retry:
  CLOSURE_READ_BITS2 (closure, &old, &new);

  g_return_if_fail (old.is_invalid == FALSE);
  g_return_if_fail (old.in_marshal == FALSE);
  g_return_if_fail (old.meta_marshal == 0);

  n = CLOSURE_N_NOTIFIERS (&old);

  old_notifiers = closure->notifiers;
  new_notifiers = g_renew (GClosureNotifyData, NULL, n + 1);
  if (old_notifiers)
    {
      /* usually the meta marshal will be setup right after creation, so the
       * g_memmove() should be rare-case scenario
       */
      g_memmove (new_notifiers + 1, old_notifiers, n * sizeof (old_notifiers[0]));
    }
  new_notifiers[0].data = marshal_data;
  new_notifiers[0].notify = (GClosureNotify) meta_marshal;

  new.meta_marshal = 1;

  /* this cannot be made atomic, as soon as we switch on the meta_marshal
   * bit, another thread could use the notifier while we have not yet
   * copied it. the safest is to install the new_notifiers first and then
   * switch on the meta_marshal flag. */
  closure->notifiers = new_notifiers;

  if (!CLOSURE_SWAP_BITS (closure, &old, &new)) {
    g_free (new_notifiers);
    goto retry;
  }

  g_free (old_notifiers);
}

void
g_closure_add_marshal_guards (GClosure      *closure,
			      gpointer       pre_marshal_data,
			      GClosureNotify pre_marshal_notify,
			      gpointer       post_marshal_data,
			      GClosureNotify post_marshal_notify)
{
  guint i;
  GClosure old, new;
  GClosureNotifyData *old_notifiers, *new_notifiers;

  g_return_if_fail (closure != NULL);
  g_return_if_fail (pre_marshal_notify != NULL);
  g_return_if_fail (post_marshal_notify != NULL);

retry:
  CLOSURE_READ_BITS2 (closure,  &old,  &new);

  g_return_if_fail (old.is_invalid == FALSE);
  g_return_if_fail (old.in_marshal == FALSE);
  g_return_if_fail (old.n_guards < CLOSURE_MAX_N_GUARDS);

  old_notifiers = closure->notifiers;
  new_notifiers = g_renew (GClosureNotifyData, old_notifiers, CLOSURE_N_NOTIFIERS (&old) + 2);
  if (old.n_inotifiers)
    new_notifiers[(CLOSURE_N_MFUNCS (&old) +
			old.n_fnotifiers +
			old.n_inotifiers + 1)] = new_notifiers[(CLOSURE_N_MFUNCS (&old) +
									  old.n_fnotifiers + 0)];
  if (old.n_inotifiers > 1)
    new_notifiers[(CLOSURE_N_MFUNCS (&old) +
			old.n_fnotifiers +
			old.n_inotifiers)] = new_notifiers[(CLOSURE_N_MFUNCS (&old) +
								      old.n_fnotifiers + 1)];
  if (old.n_fnotifiers)
    new_notifiers[(CLOSURE_N_MFUNCS (&old) +
			old.n_fnotifiers + 1)] = new_notifiers[CLOSURE_N_MFUNCS (&old) + 0];
  if (old.n_fnotifiers > 1)
    new_notifiers[(CLOSURE_N_MFUNCS (&old) +
			old.n_fnotifiers)] = new_notifiers[CLOSURE_N_MFUNCS (&old) + 1];
  if (old.n_guards)
    new_notifiers[(old.meta_marshal +
			old.n_guards +
			old.n_guards + 1)] = new_notifiers[old.meta_marshal + old.n_guards];
  i = old.n_guards;

  new.n_guards = i+1;

  new_notifiers[old.meta_marshal + i].data = pre_marshal_data;
  new_notifiers[old.meta_marshal + i].notify = pre_marshal_notify;
  new_notifiers[old.meta_marshal + i + 1].data = post_marshal_data;
  new_notifiers[old.meta_marshal + i + 1].notify = post_marshal_notify;

  /* not really atomic */
  closure->notifiers = new_notifiers;

  if (!CLOSURE_SWAP_BITS (closure, &old, &new))
    goto retry;
}

void
g_closure_add_finalize_notifier (GClosure      *closure,
				 gpointer       notify_data,
				 GClosureNotify notify_func)
{
  guint i;
  GClosure old, new;
  GClosureNotifyData *old_notifiers, *new_notifiers;

  g_return_if_fail (closure != NULL);
  g_return_if_fail (notify_func != NULL);

retry:
  CLOSURE_READ_BITS2 (closure, &old, &new);

  g_return_if_fail (old.n_fnotifiers < CLOSURE_MAX_N_FNOTIFIERS);

  old_notifiers = closure->notifiers;
  new_notifiers = g_renew (GClosureNotifyData, old_notifiers, CLOSURE_N_NOTIFIERS (&old) + 1);
  if (old.n_inotifiers)
    new_notifiers[(CLOSURE_N_MFUNCS (&old) +
			old.n_fnotifiers +
			old.n_inotifiers)] = new_notifiers[(CLOSURE_N_MFUNCS (&old) +
								      old.n_fnotifiers + 0)];
  i = CLOSURE_N_MFUNCS (&old) + old.n_fnotifiers;
  new.n_fnotifiers++;

  new_notifiers[i].data = notify_data;
  new_notifiers[i].notify = notify_func;

  /* not really atomic */
  closure->notifiers = new_notifiers;

  while (!CLOSURE_SWAP_BITS (closure, &old, &new))
    goto retry;
}

void
g_closure_add_invalidate_notifier (GClosure      *closure,
				   gpointer       notify_data,
				   GClosureNotify notify_func)
{
  guint i;
  GClosure old, new;
  GClosureNotifyData *old_notifiers, *new_notifiers;

  g_return_if_fail (closure != NULL);
  g_return_if_fail (notify_func != NULL);

retry:
  CLOSURE_READ_BITS2 (closure, &old, &new);

  g_return_if_fail (old.is_invalid == FALSE);
  g_return_if_fail (old.n_inotifiers < CLOSURE_MAX_N_INOTIFIERS);

  old_notifiers = closure->notifiers;
  new_notifiers = g_renew (GClosureNotifyData, old_notifiers, CLOSURE_N_NOTIFIERS (&old) + 1);
  i = CLOSURE_N_MFUNCS (&old) + old.n_fnotifiers + old.n_inotifiers;
  new.n_inotifiers++;

  new_notifiers[i].data = notify_data;
  new_notifiers[i].notify = notify_func;

  /* not really atomic */
  closure->notifiers = new_notifiers;

  while (!CLOSURE_SWAP_BITS (closure, &old, &new))
    goto retry;
}

static inline gboolean
closure_try_remove_inotify (GClosure       *closure,
			    gpointer       notify_data,
			    GClosureNotify notify_func)
{
  GClosureNotifyData *ndata, *nlast;
  GClosure old, new;

retry:
  CLOSURE_READ_BITS2 (closure, &old, &new);

  nlast = closure->notifiers + CLOSURE_N_NOTIFIERS (&old) - 1;
  for (ndata = nlast + 1 - old.n_inotifiers; ndata <= nlast; ndata++)
    if (ndata->notify == notify_func && ndata->data == notify_data)
      {
	new.n_inotifiers -= 1;
	if (!CLOSURE_SWAP_BITS (closure, &old, &new))
          goto retry;
	
	if (ndata < nlast)
	  *ndata = *nlast;

	return TRUE;
      }
  return FALSE;
}

static inline gboolean
closure_try_remove_fnotify (GClosure       *closure,
			    gpointer       notify_data,
			    GClosureNotify notify_func)
{
  GClosureNotifyData *ndata, *nlast;
  GClosure old, new;

retry:
  CLOSURE_READ_BITS2 (closure, &old, &new);

  nlast = closure->notifiers + CLOSURE_N_NOTIFIERS (&old) - old.n_inotifiers - 1;
  for (ndata = nlast + 1 - old.n_fnotifiers; ndata <= nlast; ndata++)
    if (ndata->notify == notify_func && ndata->data == notify_data)
      {
	new.n_fnotifiers -= 1;
	if (!CLOSURE_SWAP_BITS (closure, &old, &new))
          goto retry;
	
	if (ndata < nlast)
	  *ndata = *nlast;

	if (new.n_inotifiers)
	  closure->notifiers[(CLOSURE_N_MFUNCS (&new) +
			      new.n_fnotifiers)] = closure->notifiers[(CLOSURE_N_MFUNCS (&new) +
									    new.n_fnotifiers +
									    new.n_inotifiers)];
	return TRUE;
      }
  return FALSE;
}

GClosure*
g_closure_ref (GClosure *closure)
{
  g_return_val_if_fail (closure != NULL, NULL);
  g_return_val_if_fail (closure->ref_count > 0, NULL);
  g_return_val_if_fail (closure->ref_count < CLOSURE_MAX_REF_COUNT, NULL);

  CLOSURE_REF (closure);

  return closure;
}

void
g_closure_invalidate (GClosure *closure)
{
  GClosure old, new;

  g_return_if_fail (closure != NULL);

retry:
  CLOSURE_READ_BITS2 (closure, &old, &new);

  if (!old.is_invalid)
    {
      new.ref_count++;
      new.is_invalid = TRUE;

      if (!CLOSURE_SWAP_BITS (closure, &old, &new))
	goto retry;

      closure_invoke_notifiers (closure, INOTIFY);
      g_closure_unref (closure);
    }
}

void
g_closure_unref (GClosure *closure)
{
  gboolean is_zero;

  g_return_if_fail (closure != NULL);
  g_return_if_fail (closure->ref_count > 0);

  CLOSURE_UNREF (closure, is_zero);

  if (G_UNLIKELY (is_zero))
    {
      closure_invoke_notifiers (closure, FNOTIFY);
      g_free (closure->notifiers);
      g_free (closure);
    }
}

void
g_closure_sink (GClosure *closure)
{
  GClosure old, new;

  g_return_if_fail (closure != NULL);
  g_return_if_fail (closure->ref_count > 0);

  /* floating is basically a kludge to avoid creating closures
   * with a ref_count of 0. so the intial ref_count a closure has
   * is unowned. with invoking g_closure_sink() code may
   * indicate that it takes over that intiial ref_count.
   */
retry:
  CLOSURE_READ_BITS2 (closure, &old, &new);

  if (old.floating)
    {
      new.floating = FALSE;

      if (!CLOSURE_SWAP_BITS (closure, &old, &new))
	goto retry;

      g_closure_unref (closure);
    }
}

void
g_closure_remove_invalidate_notifier (GClosure      *closure,
				      gpointer       notify_data,
				      GClosureNotify notify_func)
{
  GClosure bits;

  g_return_if_fail (closure != NULL);
  g_return_if_fail (notify_func != NULL);

  CLOSURE_READ_BITS (closure, &bits);

  if (bits.is_invalid && bits.in_inotify && /* account removal of notify_func() while its called */
      ((gpointer) closure->marshal) == ((gpointer) notify_func) && closure->data == notify_data)
    closure->marshal = NULL;
  else if (!closure_try_remove_inotify (closure, notify_data, notify_func))
    g_warning (G_STRLOC ": unable to remove uninstalled invalidation notifier: %p (%p)",
	       notify_func, notify_data);
}

void
g_closure_remove_finalize_notifier (GClosure      *closure,
				    gpointer       notify_data,
				    GClosureNotify notify_func)
{
  GClosure bits;
	
  g_return_if_fail (closure != NULL);
  g_return_if_fail (notify_func != NULL);

  CLOSURE_READ_BITS (closure, &bits);

  if (bits.is_invalid && !bits.in_inotify && /* account removal of notify_func() while its called */
      ((gpointer) closure->marshal) == ((gpointer) notify_func) && closure->data == notify_data)
    closure->marshal = NULL;
  else if (!closure_try_remove_fnotify (closure, notify_data, notify_func))
    g_warning (G_STRLOC ": unable to remove uninstalled finalization notifier: %p (%p)",
               notify_func, notify_data);
}

void
g_closure_invoke (GClosure       *closure,
		  GValue /*out*/ *return_value,
		  guint           n_param_values,
		  const GValue   *param_values,
		  gpointer        invocation_hint)
{
  GClosure old, new;

  g_return_if_fail (closure != NULL);

retry:
  CLOSURE_READ_BITS2 (closure, &old, &new);

  if (!old.is_invalid)
   {
      GClosureMarshal marshal;
      gpointer marshal_data;
      gboolean in_marshal = old.in_marshal;
      gboolean meta_marshal = old.meta_marshal;

      g_return_if_fail (closure->marshal || meta_marshal);

      new.ref_count++;
      new.in_marshal = TRUE;

      if (!CLOSURE_SWAP_BITS (closure, &old, &new))
	goto retry;

      if (meta_marshal)
	{
	  marshal_data = closure->notifiers[0].data;
	  marshal = (GClosureMarshal) closure->notifiers[0].notify;
	}
      else
	{
	  marshal_data = NULL;
	  marshal = closure->marshal;
	}
      if (!in_marshal)
	closure_invoke_notifiers (closure, PRE_NOTIFY);

      marshal (closure,
	       return_value,
	       n_param_values, param_values,
	       invocation_hint,
	       marshal_data);

      if (!in_marshal)
	closure_invoke_notifiers (closure, POST_NOTIFY);

      do {
        CLOSURE_READ_BITS2 (closure, &old, &new);
        new.in_marshal = in_marshal;
        new.ref_count--;
      }
      while (!CLOSURE_SWAP_BITS (closure, &old, &new));
    }
}

void
g_closure_set_marshal (GClosure       *closure,
		       GClosureMarshal marshal)
{
  g_return_if_fail (closure != NULL);
  g_return_if_fail (marshal != NULL);

  if (closure->marshal && closure->marshal != marshal)
    g_warning ("attempt to override closure->marshal (%p) with new marshal (%p)",
	       closure->marshal, marshal);
  else
    closure->marshal = marshal;
}

GClosure*
g_cclosure_new (GCallback      callback_func,
		gpointer       user_data,
		GClosureNotify destroy_data)
{
  GClosure *closure;
  
  g_return_val_if_fail (callback_func != NULL, NULL);
  
  closure = g_closure_new_simple (sizeof (GCClosure), user_data);
  if (destroy_data)
    g_closure_add_finalize_notifier (closure, user_data, destroy_data);
  ((GCClosure*) closure)->callback = (gpointer) callback_func;
  
  return closure;
}

GClosure*
g_cclosure_new_swap (GCallback      callback_func,
		     gpointer       user_data,
		     GClosureNotify destroy_data)
{
  GClosure *closure;
  
  g_return_val_if_fail (callback_func != NULL, NULL);
  
  closure = g_closure_new_simple (sizeof (GCClosure), user_data);
  if (destroy_data)
    g_closure_add_finalize_notifier (closure, user_data, destroy_data);
  ((GCClosure*) closure)->callback = (gpointer) callback_func;
  closure->derivative_flag = TRUE;
  
  return closure;
}

static void
g_type_class_meta_marshal (GClosure       *closure,
			   GValue /*out*/ *return_value,
			   guint           n_param_values,
			   const GValue   *param_values,
			   gpointer        invocation_hint,
			   gpointer        marshal_data)
{
  GTypeClass *class;
  gpointer callback;
  /* GType itype = (GType) closure->data; */
  guint offset = GPOINTER_TO_UINT (marshal_data);
  
  class = G_TYPE_INSTANCE_GET_CLASS (g_value_peek_pointer (param_values + 0), itype, GTypeClass);
  callback = G_STRUCT_MEMBER (gpointer, class, offset);
  if (callback)
    closure->marshal (closure,
		      return_value,
		      n_param_values, param_values,
		      invocation_hint,
		      callback);
}

static void
g_type_iface_meta_marshal (GClosure       *closure,
			   GValue /*out*/ *return_value,
			   guint           n_param_values,
			   const GValue   *param_values,
			   gpointer        invocation_hint,
			   gpointer        marshal_data)
{
  GTypeClass *class;
  gpointer callback;
  GType itype = (GType) closure->data;
  guint offset = GPOINTER_TO_UINT (marshal_data);
  
  class = G_TYPE_INSTANCE_GET_INTERFACE (g_value_peek_pointer (param_values + 0), itype, GTypeClass);
  callback = G_STRUCT_MEMBER (gpointer, class, offset);
  if (callback)
    closure->marshal (closure,
		      return_value,
		      n_param_values, param_values,
		      invocation_hint,
		      callback);
}

GClosure*
g_signal_type_cclosure_new (GType    itype,
			    guint    struct_offset)
{
  GClosure *closure;
  
  g_return_val_if_fail (G_TYPE_IS_CLASSED (itype) || G_TYPE_IS_INTERFACE (itype), NULL);
  g_return_val_if_fail (struct_offset >= sizeof (GTypeClass), NULL);
  
  closure = g_closure_new_simple (sizeof (GClosure), (gpointer) itype);
  if (G_TYPE_IS_INTERFACE (itype))
    g_closure_set_meta_marshal (closure, GUINT_TO_POINTER (struct_offset), g_type_iface_meta_marshal);
  else
    g_closure_set_meta_marshal (closure, GUINT_TO_POINTER (struct_offset), g_type_class_meta_marshal);
  
  return closure;
}

#define __G_CLOSURE_C__
#include "gobjectaliasdef.c"
