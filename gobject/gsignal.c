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
 *
 * this code is based on the original GtkSignal implementation
 * for the Gtk+ library by Peter Mattis <petm@xcf.berkeley.edu>
 */

/*
 * MT safe
 */

#include <config.h>

#include        "gsignal.h"
#include        "gbsearcharray.h"
#include        "gvaluecollector.h"
#include	"gvaluetypes.h"
#include	"gboxed.h"
#include	<string.h> 
#include	<signal.h>


/* pre allocation configurations
 */
#define	MAX_STACK_VALUES	(16)
#define HANDLER_PRE_ALLOC       (48)

#define REPORT_BUG      "please report occurrence circumstances to gtk-devel-list@gnome.org"
#ifdef	G_ENABLE_DEBUG
#define IF_DEBUG(debug_type, cond)	if ((_g_type_debug_flags & G_TYPE_DEBUG_ ## debug_type) || cond)
static volatile gpointer g_trace_instance_signals = NULL;
static volatile gpointer g_trap_instance_signals = NULL;
#endif	/* G_ENABLE_DEBUG */


/* --- generic allocation --- */
/* we special case allocations generically by replacing
 * these functions with more speed/memory aware variants
 */
#ifndef	DISABLE_MEM_POOLS
static inline gpointer
g_generic_node_alloc (GTrashStack **trash_stack_p,
                      guint         sizeof_node,
                      guint         nodes_pre_alloc)
{
  gpointer node = g_trash_stack_pop (trash_stack_p);
  
  if (!node)
    {
      guint8 *block;
      
      nodes_pre_alloc = MAX (nodes_pre_alloc, 1);
      block = g_malloc (sizeof_node * nodes_pre_alloc);
      while (--nodes_pre_alloc)
        {
          g_trash_stack_push (trash_stack_p, block);
          block += sizeof_node;
        }
      node = block;
    }
  
  return node;
}
#define	g_generic_node_free(trash_stack_p, node) g_trash_stack_push (trash_stack_p, node)
#else	/* !DISABLE_MEM_POOLS */
#define	g_generic_node_alloc(t,sizeof_node,p)	 g_malloc (sizeof_node)
#define	g_generic_node_free(t,node)		 g_free (node)
#endif	/* !DISABLE_MEM_POOLS */


/* --- typedefs --- */
typedef struct _SignalNode   SignalNode;
typedef struct _SignalKey    SignalKey;
typedef struct _Emission     Emission;
typedef struct _Handler      Handler;
typedef struct _HandlerList  HandlerList;
typedef struct _HandlerMatch HandlerMatch;
typedef enum
{
  EMISSION_STOP,
  EMISSION_RUN,
  EMISSION_HOOK,
  EMISSION_RESTART
} EmissionState;


/* --- prototypes --- */
static inline guint		signal_id_lookup	(GQuark		  quark,
							 GType		  itype);
static	      void		signal_destroy_R	(SignalNode	 *signal_node);
static inline HandlerList*	handler_list_ensure	(guint		  signal_id,
							 gpointer	  instance);
static inline HandlerList*	handler_list_lookup	(guint		  signal_id,
							 gpointer	  instance);
static inline Handler*		handler_new		(gboolean	  after);
static	      void		handler_insert		(guint		  signal_id,
							 gpointer	  instance,
							 Handler	 *handler);
static	      Handler*		handler_lookup		(gpointer	  instance,
							 gulong		  handler_id,
							 guint		 *signal_id_p);
static inline HandlerMatch*	handler_match_prepend	(HandlerMatch	 *list,
							 Handler	 *handler,
							 guint		  signal_id);
static inline HandlerMatch*	handler_match_free1_R	(HandlerMatch	 *node,
							 gpointer	  instance);
static	      HandlerMatch*	handlers_find		(gpointer	  instance,
							 GSignalMatchType mask,
							 guint		  signal_id,
							 GQuark		  detail,
							 GClosure	 *closure,
							 gpointer	  func,
							 gpointer	  data,
							 gboolean	  one_and_only);
static inline void		handler_ref		(Handler	 *handler);
static inline void		handler_unref_R		(guint		  signal_id,
							 gpointer	  instance,
							 Handler	 *handler);
static gint			handler_lists_cmp	(gconstpointer	  node1,
							 gconstpointer	  node2);
static inline void		emission_push		(Emission	**emission_list_p,
							 Emission	 *emission);
static inline void		emission_pop		(Emission	**emission_list_p,
							 Emission	 *emission);
static inline Emission*		emission_find		(Emission	 *emission_list,
							 guint		  signal_id,
							 GQuark		  detail,
							 gpointer	  instance);
static gint			class_closures_cmp	(gconstpointer	  node1,
							 gconstpointer	  node2);
static gint			signal_key_cmp		(gconstpointer	  node1,
							 gconstpointer	  node2);
static	      gboolean		signal_emit_unlocked_R	(SignalNode	 *node,
							 GQuark		  detail,
							 gpointer	  instance,
							 GValue		 *return_value,
							 const GValue	 *instance_and_params);
static const gchar *            type_debug_name         (GType            type);


/* --- structures --- */
typedef struct
{
  GSignalAccumulator func;
  gpointer           data;
} SignalAccumulator;
typedef struct
{
  GHook hook;
  GQuark detail;
} SignalHook;
#define	SIGNAL_HOOK(hook)	((SignalHook*) (hook))

struct _SignalNode
{
  /* permanent portion */
  guint              signal_id;
  GType              itype;
  gchar             *name;
  guint              destroyed : 1;
  
  /* reinitializable portion */
  guint		     test_class_offset : 12;
  guint              flags : 8;
  guint              n_params : 8;
  GType		    *param_types; /* mangled with G_SIGNAL_TYPE_STATIC_SCOPE flag */
  GType		     return_type; /* mangled with G_SIGNAL_TYPE_STATIC_SCOPE flag */
  GBSearchArray     *class_closure_bsa;
  SignalAccumulator *accumulator;
  GSignalCMarshaller c_marshaller;
  GHookList         *emission_hooks;
};
#define	MAX_TEST_CLASS_OFFSET	(4096)	/* 2^12, 12 bits for test_class_offset */
#define	TEST_CLASS_MAGIC	(1)	/* indicates NULL class closure, candidate for NOP optimization */

struct _SignalKey
{
  GType  itype;
  GQuark quark;
  guint  signal_id;
};

struct _Emission
{
  Emission             *next;
  gpointer              instance;
  GSignalInvocationHint ihint;
  EmissionState         state;
  GType			chain_type;
};

struct _HandlerList
{
  guint    signal_id;
  Handler *handlers;
};
struct _Handler
{
  gulong        sequential_number;
  Handler      *next;
  Handler      *prev;
  GQuark	detail;
  guint         ref_count : 16;
#define HANDLER_MAX_REF_COUNT   (1 << 16)
  guint         block_count : 12;
#define HANDLER_MAX_BLOCK_COUNT (1 << 12)
  guint         after : 1;
  GClosure     *closure;
};
struct _HandlerMatch
{
  Handler      *handler;
  HandlerMatch *next;
  union {
    guint       signal_id;
    gpointer	dummy;
  } d;
};

typedef struct
{
  GType     instance_type; /* 0 for default closure */
  GClosure *closure;
} ClassClosure;


/* --- variables --- */
static GBSearchArray *g_signal_key_bsa = NULL;
static const GBSearchConfig g_signal_key_bconfig = {
  sizeof (SignalKey),
  signal_key_cmp,
  G_BSEARCH_ARRAY_ALIGN_POWER2,
};
static GBSearchConfig g_signal_hlbsa_bconfig = {
  sizeof (HandlerList),
  handler_lists_cmp,
  0,
};
static GBSearchConfig g_class_closure_bconfig = {
  sizeof (ClassClosure),
  class_closures_cmp,
  0,
};
static GHashTable    *g_handler_list_bsa_ht = NULL;
static Emission      *g_recursive_emissions = NULL;
static Emission      *g_restart_emissions = NULL;
#ifndef DISABLE_MEM_POOLS
static GTrashStack   *g_handler_ts = NULL;
#endif
static gulong         g_handler_sequential_number = 1;
G_LOCK_DEFINE_STATIC (g_signal_mutex);
#define	SIGNAL_LOCK()		G_LOCK (g_signal_mutex)
#define	SIGNAL_UNLOCK()		G_UNLOCK (g_signal_mutex)


/* --- signal nodes --- */
static guint          g_n_signal_nodes = 0;
static SignalNode   **g_signal_nodes = NULL;

static inline SignalNode*
LOOKUP_SIGNAL_NODE (register guint signal_id)
{
  if (signal_id < g_n_signal_nodes)
    return g_signal_nodes[signal_id];
  else
    return NULL;
}


/* --- functions --- */
static inline guint
signal_id_lookup (GQuark quark,
		  GType  itype)
{
  GType *ifaces, type = itype;
  SignalKey key;
  guint n_ifaces;

  key.quark = quark;

  /* try looking up signals for this type and its ancestors */
  do
    {
      SignalKey *signal_key;
      
      key.itype = type;
      signal_key = g_bsearch_array_lookup (g_signal_key_bsa, &g_signal_key_bconfig, &key);
      
      if (signal_key)
	return signal_key->signal_id;
      
      type = g_type_parent (type);
    }
  while (type);

  /* no luck, try interfaces it exports */
  ifaces = g_type_interfaces (itype, &n_ifaces);
  while (n_ifaces--)
    {
      SignalKey *signal_key;

      key.itype = ifaces[n_ifaces];
      signal_key = g_bsearch_array_lookup (g_signal_key_bsa, &g_signal_key_bconfig, &key);

      if (signal_key)
	{
	  g_free (ifaces);
	  return signal_key->signal_id;
	}
    }
  g_free (ifaces);
  
  return 0;
}

static gint
class_closures_cmp (gconstpointer node1,
		    gconstpointer node2)
{
  const ClassClosure *c1 = node1, *c2 = node2;
  
  return G_BSEARCH_ARRAY_CMP (c1->instance_type, c2->instance_type);
}

static gint
handler_lists_cmp (gconstpointer node1,
                   gconstpointer node2)
{
  const HandlerList *hlist1 = node1, *hlist2 = node2;
  
  return G_BSEARCH_ARRAY_CMP (hlist1->signal_id, hlist2->signal_id);
}

static inline HandlerList*
handler_list_ensure (guint    signal_id,
		     gpointer instance)
{
  GBSearchArray *hlbsa = g_hash_table_lookup (g_handler_list_bsa_ht, instance);
  HandlerList key;
  
  key.signal_id = signal_id;
  key.handlers = NULL;
  if (!hlbsa)
    {
      hlbsa = g_bsearch_array_create (&g_signal_hlbsa_bconfig);
      hlbsa = g_bsearch_array_insert (hlbsa, &g_signal_hlbsa_bconfig, &key);
      g_hash_table_insert (g_handler_list_bsa_ht, instance, hlbsa);
    }
  else
    {
      GBSearchArray *o = hlbsa;

      hlbsa = g_bsearch_array_insert (o, &g_signal_hlbsa_bconfig, &key);
      if (hlbsa != o)
	g_hash_table_insert (g_handler_list_bsa_ht, instance, hlbsa);
    }
  return g_bsearch_array_lookup (hlbsa, &g_signal_hlbsa_bconfig, &key);
}

static inline HandlerList*
handler_list_lookup (guint    signal_id,
		     gpointer instance)
{
  GBSearchArray *hlbsa = g_hash_table_lookup (g_handler_list_bsa_ht, instance);
  HandlerList key;
  
  key.signal_id = signal_id;
  
  return hlbsa ? g_bsearch_array_lookup (hlbsa, &g_signal_hlbsa_bconfig, &key) : NULL;
}

static Handler*
handler_lookup (gpointer instance,
		gulong   handler_id,
		guint   *signal_id_p)
{
  GBSearchArray *hlbsa = g_hash_table_lookup (g_handler_list_bsa_ht, instance);
  
  if (hlbsa)
    {
      guint i;
      
      for (i = 0; i < hlbsa->n_nodes; i++)
        {
          HandlerList *hlist = g_bsearch_array_get_nth (hlbsa, &g_signal_hlbsa_bconfig, i);
          Handler *handler;
          
          for (handler = hlist->handlers; handler; handler = handler->next)
            if (handler->sequential_number == handler_id)
              {
                if (signal_id_p)
                  *signal_id_p = hlist->signal_id;
		
                return handler;
              }
        }
    }
  
  return NULL;
}

static inline HandlerMatch*
handler_match_prepend (HandlerMatch *list,
		       Handler      *handler,
		       guint	     signal_id)
{
  HandlerMatch *node;
  
  /* yeah, we could use our own memchunk here, introducing yet more
   * rarely used cached nodes and extra allocation overhead.
   * instead, we use GList* nodes, since they are exactly the size
   * we need and are already cached. g_signal_init() asserts this.
   */
  node = (HandlerMatch*) g_list_alloc ();
  node->handler = handler;
  node->next = list;
  node->d.signal_id = signal_id;
  handler_ref (handler);
  
  return node;
}
static inline HandlerMatch*
handler_match_free1_R (HandlerMatch *node,
		       gpointer      instance)
{
  HandlerMatch *next = node->next;
  
  handler_unref_R (node->d.signal_id, instance, node->handler);
  g_list_free_1 ((GList*) node);
  
  return next;
}

static HandlerMatch*
handlers_find (gpointer         instance,
	       GSignalMatchType mask,
	       guint            signal_id,
	       GQuark           detail,
	       GClosure        *closure,
	       gpointer         func,
	       gpointer         data,
	       gboolean         one_and_only)
{
  HandlerMatch *mlist = NULL;
  
  if (mask & G_SIGNAL_MATCH_ID)
    {
      HandlerList *hlist = handler_list_lookup (signal_id, instance);
      Handler *handler;
      SignalNode *node = NULL;
      
      if (mask & G_SIGNAL_MATCH_FUNC)
	{
	  node = LOOKUP_SIGNAL_NODE (signal_id);
	  if (!node || !node->c_marshaller)
	    return NULL;
	}
      
      mask = ~mask;
      for (handler = hlist ? hlist->handlers : NULL; handler; handler = handler->next)
        if (handler->sequential_number &&
	    ((mask & G_SIGNAL_MATCH_DETAIL) || handler->detail == detail) &&
	    ((mask & G_SIGNAL_MATCH_CLOSURE) || handler->closure == closure) &&
            ((mask & G_SIGNAL_MATCH_DATA) || handler->closure->data == data) &&
	    ((mask & G_SIGNAL_MATCH_UNBLOCKED) || handler->block_count == 0) &&
	    ((mask & G_SIGNAL_MATCH_FUNC) || (handler->closure->marshal == node->c_marshaller &&
					      handler->closure->meta_marshal == 0 &&
					      ((GCClosure*) handler->closure)->callback == func)))
	  {
	    mlist = handler_match_prepend (mlist, handler, signal_id);
	    if (one_and_only)
	      return mlist;
	  }
    }
  else
    {
      GBSearchArray *hlbsa = g_hash_table_lookup (g_handler_list_bsa_ht, instance);
      
      mask = ~mask;
      if (hlbsa)
        {
          guint i;
          
          for (i = 0; i < hlbsa->n_nodes; i++)
            {
              HandlerList *hlist = g_bsearch_array_get_nth (hlbsa, &g_signal_hlbsa_bconfig, i);
	      SignalNode *node = NULL;
              Handler *handler;
              
	      if (!(mask & G_SIGNAL_MATCH_FUNC))
		{
		  node = LOOKUP_SIGNAL_NODE (hlist->signal_id);
		  if (!node->c_marshaller)
		    continue;
		}
	      
              for (handler = hlist->handlers; handler; handler = handler->next)
		if (handler->sequential_number &&
		    ((mask & G_SIGNAL_MATCH_DETAIL) || handler->detail == detail) &&
                    ((mask & G_SIGNAL_MATCH_CLOSURE) || handler->closure == closure) &&
                    ((mask & G_SIGNAL_MATCH_DATA) || handler->closure->data == data) &&
		    ((mask & G_SIGNAL_MATCH_UNBLOCKED) || handler->block_count == 0) &&
		    ((mask & G_SIGNAL_MATCH_FUNC) || (handler->closure->marshal == node->c_marshaller &&
						      handler->closure->meta_marshal == 0 &&
						      ((GCClosure*) handler->closure)->callback == func)))
		  {
		    mlist = handler_match_prepend (mlist, handler, hlist->signal_id);
		    if (one_and_only)
		      return mlist;
		  }
            }
        }
    }
  
  return mlist;
}

static inline Handler*
handler_new (gboolean after)
{
  Handler *handler = g_generic_node_alloc (&g_handler_ts,
                                           sizeof (Handler),
                                           HANDLER_PRE_ALLOC);
#ifndef G_DISABLE_CHECKS
  if (g_handler_sequential_number < 1)
    g_error (G_STRLOC ": handler id overflow, %s", REPORT_BUG);
#endif
  
  handler->sequential_number = g_handler_sequential_number++;
  handler->prev = NULL;
  handler->next = NULL;
  handler->detail = 0;
  handler->ref_count = 1;
  handler->block_count = 0;
  handler->after = after != FALSE;
  handler->closure = NULL;
  
  return handler;
}

static inline void
handler_ref (Handler *handler)
{
  g_return_if_fail (handler->ref_count > 0);
  
#ifndef G_DISABLE_CHECKS
  if (handler->ref_count >= HANDLER_MAX_REF_COUNT - 1)
    g_error (G_STRLOC ": handler ref_count overflow, %s", REPORT_BUG);
#endif
  
  handler->ref_count += 1;
}

static inline void
handler_unref_R (guint    signal_id,
		 gpointer instance,
		 Handler *handler)
{
  g_return_if_fail (handler->ref_count > 0);
  
  handler->ref_count -= 1;
  if (!handler->ref_count)
    {
      if (handler->next)
        handler->next->prev = handler->prev;
      if (handler->prev)	/* watch out for g_signal_handlers_destroy()! */
        handler->prev->next = handler->next;
      else
        {
          HandlerList *hlist = handler_list_lookup (signal_id, instance);
          
          hlist->handlers = handler->next;
        }
      SIGNAL_UNLOCK ();
      g_closure_unref (handler->closure);
      SIGNAL_LOCK ();
      g_generic_node_free (&g_handler_ts, handler);
    }
}

static void
handler_insert (guint    signal_id,
		gpointer instance,
		Handler  *handler)
{
  HandlerList *hlist;
  
  g_assert (handler->prev == NULL && handler->next == NULL); /* paranoid */
  
  hlist = handler_list_ensure (signal_id, instance);
  if (!hlist->handlers)
    hlist->handlers = handler;
  else if (hlist->handlers->after && !handler->after)
    {
      handler->next = hlist->handlers;
      hlist->handlers->prev = handler;
      hlist->handlers = handler;
    }
  else
    {
      Handler *tmp = hlist->handlers;
      
      if (handler->after)
        while (tmp->next)
          tmp = tmp->next;
      else
        while (tmp->next && !tmp->next->after)
          tmp = tmp->next;
      if (tmp->next)
        tmp->next->prev = handler;
      handler->next = tmp->next;
      handler->prev = tmp;
      tmp->next = handler;
    }
}

static inline void
emission_push (Emission **emission_list_p,
	       Emission  *emission)
{
  emission->next = *emission_list_p;
  *emission_list_p = emission;
}

static inline void
emission_pop (Emission **emission_list_p,
	      Emission  *emission)
{
  Emission *node, *last = NULL;

  for (node = *emission_list_p; node; last = node, node = last->next)
    if (node == emission)
      {
	if (last)
	  last->next = node->next;
	else
	  *emission_list_p = node->next;
	return;
      }
  g_assert_not_reached ();
}

static inline Emission*
emission_find (Emission *emission_list,
	       guint     signal_id,
	       GQuark    detail,
	       gpointer  instance)
{
  Emission *emission;
  
  for (emission = emission_list; emission; emission = emission->next)
    if (emission->instance == instance &&
	emission->ihint.signal_id == signal_id &&
	emission->ihint.detail == detail)
      return emission;
  return NULL;
}

static inline Emission*
emission_find_innermost (gpointer instance)
{
  Emission *emission, *s = NULL, *c = NULL;
  
  for (emission = g_restart_emissions; emission; emission = emission->next)
    if (emission->instance == instance)
      {
	s = emission;
	break;
      }
  for (emission = g_recursive_emissions; emission; emission = emission->next)
    if (emission->instance == instance)
      {
	c = emission;
	break;
      }
  if (!s)
    return c;
  else if (!c)
    return s;
  else
    return G_HAVE_GROWING_STACK ? MAX (c, s) : MIN (c, s);
}

static gint
signal_key_cmp (gconstpointer node1,
                gconstpointer node2)
{
  const SignalKey *key1 = node1, *key2 = node2;
  
  if (key1->itype == key2->itype)
    return G_BSEARCH_ARRAY_CMP (key1->quark, key2->quark);
  else
    return G_BSEARCH_ARRAY_CMP (key1->itype, key2->itype);
}

void
g_signal_init (void) /* sync with gtype.c */
{
  SIGNAL_LOCK ();
  if (!g_n_signal_nodes)
    {
      /* handler_id_node_prepend() requires this */
      g_assert (sizeof (GList) == sizeof (HandlerMatch));
      
      /* setup handler list binary searchable array hash table (in german, that'd be one word ;) */
      g_handler_list_bsa_ht = g_hash_table_new (g_direct_hash, NULL);
      g_signal_key_bsa = g_bsearch_array_create (&g_signal_key_bconfig);
      
      /* invalid (0) signal_id */
      g_n_signal_nodes = 1;
      g_signal_nodes = g_renew (SignalNode*, g_signal_nodes, g_n_signal_nodes);
      g_signal_nodes[0] = NULL;
    }
  SIGNAL_UNLOCK ();
}

void
_g_signals_destroy (GType itype)
{
  guint i;
  
  SIGNAL_LOCK ();
  for (i = 1; i < g_n_signal_nodes; i++)
    {
      SignalNode *node = g_signal_nodes[i];
      
      if (node->itype == itype)
        {
          if (node->destroyed)
            g_warning (G_STRLOC ": signal \"%s\" of type `%s' already destroyed",
                       node->name,
                       type_debug_name (node->itype));
          else
	    signal_destroy_R (node);
        }
    }
  SIGNAL_UNLOCK ();
}

void
g_signal_stop_emission (gpointer instance,
                        guint    signal_id,
			GQuark   detail)
{
  SignalNode *node;
  
  g_return_if_fail (G_TYPE_CHECK_INSTANCE (instance));
  g_return_if_fail (signal_id > 0);
  
  SIGNAL_LOCK ();
  node = LOOKUP_SIGNAL_NODE (signal_id);
  if (node && detail && !(node->flags & G_SIGNAL_DETAILED))
    {
      g_warning ("%s: signal id `%u' does not support detail (%u)", G_STRLOC, signal_id, detail);
      SIGNAL_UNLOCK ();
      return;
    }
  if (node && g_type_is_a (G_TYPE_FROM_INSTANCE (instance), node->itype))
    {
      Emission *emission_list = node->flags & G_SIGNAL_NO_RECURSE ? g_restart_emissions : g_recursive_emissions;
      Emission *emission = emission_find (emission_list, signal_id, detail, instance);
      
      if (emission)
        {
          if (emission->state == EMISSION_HOOK)
            g_warning (G_STRLOC ": emission of signal \"%s\" for instance `%p' cannot be stopped from emission hook",
                       node->name, instance);
          else if (emission->state == EMISSION_RUN)
            emission->state = EMISSION_STOP;
        }
      else
        g_warning (G_STRLOC ": no emission of signal \"%s\" to stop for instance `%p'",
                   node->name, instance);
    }
  else
    g_warning ("%s: signal id `%u' is invalid for instance `%p'", G_STRLOC, signal_id, instance);
  SIGNAL_UNLOCK ();
}

static void
signal_finalize_hook (GHookList *hook_list,
		      GHook     *hook)
{
  GDestroyNotify destroy = hook->destroy;

  if (destroy)
    {
      hook->destroy = NULL;
      SIGNAL_UNLOCK ();
      destroy (hook->data);
      SIGNAL_LOCK ();
    }
}

gulong
g_signal_add_emission_hook (guint               signal_id,
			    GQuark              detail,
			    GSignalEmissionHook hook_func,
			    gpointer            hook_data,
			    GDestroyNotify      data_destroy)
{
  static gulong seq_hook_id = 1;
  SignalNode *node;
  GHook *hook;
  SignalHook *signal_hook;

  g_return_val_if_fail (signal_id > 0, 0);
  g_return_val_if_fail (hook_func != NULL, 0);

  SIGNAL_LOCK ();
  node = LOOKUP_SIGNAL_NODE (signal_id);
  if (!node || node->destroyed || (node->flags & G_SIGNAL_NO_HOOKS))
    {
      g_warning ("%s: invalid signal id `%u'", G_STRLOC, signal_id);
      SIGNAL_UNLOCK ();
      return 0;
    }
  if (detail && !(node->flags & G_SIGNAL_DETAILED))
    {
      g_warning ("%s: signal id `%u' does not support detail (%u)", G_STRLOC, signal_id, detail);
      SIGNAL_UNLOCK ();
      return 0;
    }
  if (!node->emission_hooks)
    {
      node->emission_hooks = g_new (GHookList, 1);
      g_hook_list_init (node->emission_hooks, sizeof (SignalHook));
      node->emission_hooks->finalize_hook = signal_finalize_hook;
    }
  hook = g_hook_alloc (node->emission_hooks);
  hook->data = hook_data;
  hook->func = (gpointer) hook_func;
  hook->destroy = data_destroy;
  signal_hook = SIGNAL_HOOK (hook);
  signal_hook->detail = detail;
  node->emission_hooks->seq_id = seq_hook_id;
  g_hook_append (node->emission_hooks, hook);
  seq_hook_id = node->emission_hooks->seq_id;
  SIGNAL_UNLOCK ();

  return hook->hook_id;
}

void
g_signal_remove_emission_hook (guint  signal_id,
			       gulong hook_id)
{
  SignalNode *node;

  g_return_if_fail (signal_id > 0);
  g_return_if_fail (hook_id > 0);

  SIGNAL_LOCK ();
  node = LOOKUP_SIGNAL_NODE (signal_id);
  if (!node || node->destroyed)
    g_warning ("%s: invalid signal id `%u'", G_STRLOC, signal_id);
  else if (!node->emission_hooks || !g_hook_destroy (node->emission_hooks, hook_id))
    g_warning ("%s: signal \"%s\" had no hook (%lu) to remove", G_STRLOC, node->name, hook_id);
  SIGNAL_UNLOCK ();
}

static inline guint
signal_parse_name (const gchar *name,
		   GType        itype,
		   GQuark      *detail_p,
		   gboolean     force_quark)
{
  const gchar *colon = strchr (name, ':');
  guint signal_id;
  
  if (!colon)
    {
      signal_id = signal_id_lookup (g_quark_try_string (name), itype);
      if (signal_id && detail_p)
	*detail_p = 0;
    }
  else if (colon[1] == ':')
    {
      gchar buffer[32];
      guint l = colon - name;
      
      if (l < 32)
	{
	  memcpy (buffer, name, l);
	  buffer[l] = 0;
	  signal_id = signal_id_lookup (g_quark_try_string (buffer), itype);
	}
      else
	{
	  gchar *signal = g_new (gchar, l + 1);
	  
	  memcpy (signal, name, l);
	  signal[l] = 0;
	  signal_id = signal_id_lookup (g_quark_try_string (signal), itype);
	  g_free (signal);
	}
      
      if (signal_id && detail_p)
	*detail_p = colon[2] ? (force_quark ? g_quark_from_string : g_quark_try_string) (colon + 2) : 0;
    }
  else
    signal_id = 0;
  return signal_id;
}

gboolean
g_signal_parse_name (const gchar *detailed_signal,
		     GType        itype,
		     guint       *signal_id_p,
		     GQuark      *detail_p,
		     gboolean	  force_detail_quark)
{
  SignalNode *node;
  GQuark detail = 0;
  guint signal_id;
  
  g_return_val_if_fail (detailed_signal != NULL, FALSE);
  g_return_val_if_fail (G_TYPE_IS_INSTANTIATABLE (itype) || G_TYPE_IS_INTERFACE (itype), FALSE);
  
  SIGNAL_LOCK ();
  signal_id = signal_parse_name (detailed_signal, itype, &detail, force_detail_quark);
  SIGNAL_UNLOCK ();

  node = signal_id ? LOOKUP_SIGNAL_NODE (signal_id) : NULL;
  if (!node || node->destroyed ||
      (detail && !(node->flags & G_SIGNAL_DETAILED)))
    return FALSE;

  if (signal_id_p)
    *signal_id_p = signal_id;
  if (detail_p)
    *detail_p = detail;
  
  return TRUE;
}

void
g_signal_stop_emission_by_name (gpointer     instance,
				const gchar *detailed_signal)
{
  guint signal_id;
  GQuark detail = 0;
  GType itype;
  
  g_return_if_fail (G_TYPE_CHECK_INSTANCE (instance));
  g_return_if_fail (detailed_signal != NULL);
  
  SIGNAL_LOCK ();
  itype = G_TYPE_FROM_INSTANCE (instance);
  signal_id = signal_parse_name (detailed_signal, itype, &detail, TRUE);
  if (signal_id)
    {
      SignalNode *node = LOOKUP_SIGNAL_NODE (signal_id);
      
      if (detail && !(node->flags & G_SIGNAL_DETAILED))
	g_warning ("%s: signal `%s' does not support details", G_STRLOC, detailed_signal);
      else if (!g_type_is_a (itype, node->itype))
	g_warning ("%s: signal `%s' is invalid for instance `%p'", G_STRLOC, detailed_signal, instance);
      else
	{
	  Emission *emission_list = node->flags & G_SIGNAL_NO_RECURSE ? g_restart_emissions : g_recursive_emissions;
	  Emission *emission = emission_find (emission_list, signal_id, detail, instance);
	  
	  if (emission)
	    {
	      if (emission->state == EMISSION_HOOK)
		g_warning (G_STRLOC ": emission of signal \"%s\" for instance `%p' cannot be stopped from emission hook",
			   node->name, instance);
	      else if (emission->state == EMISSION_RUN)
		emission->state = EMISSION_STOP;
	    }
	  else
	    g_warning (G_STRLOC ": no emission of signal \"%s\" to stop for instance `%p'",
		       node->name, instance);
	}
    }
  else
    g_warning ("%s: signal `%s' is invalid for instance `%p'", G_STRLOC, detailed_signal, instance);
  SIGNAL_UNLOCK ();
}

guint
g_signal_lookup (const gchar *name,
                 GType        itype)
{
  guint signal_id;
  g_return_val_if_fail (name != NULL, 0);
  g_return_val_if_fail (G_TYPE_IS_INSTANTIATABLE (itype) || G_TYPE_IS_INTERFACE (itype), 0);
  
  SIGNAL_LOCK ();
  signal_id = signal_id_lookup (g_quark_try_string (name), itype);
  SIGNAL_UNLOCK ();
  if (!signal_id)
    {
      /* give elaborate warnings */
      if (!g_type_name (itype))
	g_warning (G_STRLOC ": unable to lookup signal \"%s\" for invalid type id `%lu'",
		   name, itype);
      else if (!G_TYPE_IS_INSTANTIATABLE (itype))
	g_warning (G_STRLOC ": unable to lookup signal \"%s\" for non instantiatable type `%s'",
		   name, g_type_name (itype));
      else if (!g_type_class_peek (itype))
	g_warning (G_STRLOC ": unable to lookup signal \"%s\" of unloaded type `%s'",
		   name, g_type_name (itype));
    }
  
  return signal_id;
}

guint*
g_signal_list_ids (GType  itype,
		   guint *n_ids)
{
  SignalKey *keys;
  GArray *result;
  guint n_nodes;
  guint i;
  
  g_return_val_if_fail (G_TYPE_IS_INSTANTIATABLE (itype) || G_TYPE_IS_INTERFACE (itype), NULL);
  g_return_val_if_fail (n_ids != NULL, NULL);
  
  SIGNAL_LOCK ();
  keys = g_bsearch_array_get_nth (g_signal_key_bsa, &g_signal_key_bconfig, 0);
  n_nodes = g_bsearch_array_get_n_nodes (g_signal_key_bsa);
  result = g_array_new (FALSE, FALSE, sizeof (guint));
  
  for (i = 0; i < n_nodes; i++)
    if (keys[i].itype == itype)
      {
	const gchar *name = g_quark_to_string (keys[i].quark);
	
	/* Signal names with "_" in them are aliases to the same
	 * name with "-" instead of "_".
	 */
	if (!strchr (name, '_'))
	  g_array_append_val (result, keys[i].signal_id);
      }
  *n_ids = result->len;
  SIGNAL_UNLOCK ();
  if (!n_nodes)
    {
      /* give elaborate warnings */
      if (!g_type_name (itype))
	g_warning (G_STRLOC ": unable to list signals for invalid type id `%lu'",
		   itype);
      else if (!G_TYPE_IS_INSTANTIATABLE (itype))
	g_warning (G_STRLOC ": unable to list signals of non instantiatable type `%s'",
		   g_type_name (itype));
      else if (!g_type_class_peek (itype))
	g_warning (G_STRLOC ": unable to list signals of unloaded type `%s'",
		   g_type_name (itype));
    }
  
  return (guint*) g_array_free (result, FALSE);
}

G_CONST_RETURN gchar*
g_signal_name (guint signal_id)
{
  SignalNode *node;
  gchar *name;
  
  SIGNAL_LOCK ();
  node = LOOKUP_SIGNAL_NODE (signal_id);
  name = node ? node->name : NULL;
  SIGNAL_UNLOCK ();
  
  return name;
}

void
g_signal_query (guint         signal_id,
		GSignalQuery *query)
{
  SignalNode *node;
  
  g_return_if_fail (query != NULL);
  
  SIGNAL_LOCK ();
  node = LOOKUP_SIGNAL_NODE (signal_id);
  if (!node || node->destroyed)
    query->signal_id = 0;
  else
    {
      query->signal_id = node->signal_id;
      query->signal_name = node->name;
      query->itype = node->itype;
      query->signal_flags = node->flags;
      query->return_type = node->return_type;
      query->n_params = node->n_params;
      query->param_types = node->param_types;
    }
  SIGNAL_UNLOCK ();
}

guint
g_signal_new (const gchar	 *signal_name,
	      GType		  itype,
	      GSignalFlags	  signal_flags,
	      guint               class_offset,
	      GSignalAccumulator  accumulator,
	      gpointer		  accu_data,
	      GSignalCMarshaller  c_marshaller,
	      GType		  return_type,
	      guint		  n_params,
	      ...)
{
  va_list args;
  guint signal_id;

  g_return_val_if_fail (signal_name != NULL, 0);
  
  va_start (args, n_params);

  signal_id = g_signal_new_valist (signal_name, itype, signal_flags,
                                   class_offset ? g_signal_type_cclosure_new (itype, class_offset) : NULL,
				   accumulator, accu_data, c_marshaller,
                                   return_type, n_params, args);

  va_end (args);

  /* optimize NOP emissions with NULL class handlers */
  if (signal_id && G_TYPE_IS_INSTANTIATABLE (itype) && return_type == G_TYPE_NONE &&
      class_offset && class_offset < MAX_TEST_CLASS_OFFSET)
    {
      SignalNode *node;

      SIGNAL_LOCK ();
      node = LOOKUP_SIGNAL_NODE (signal_id);
      node->test_class_offset = class_offset;
      SIGNAL_UNLOCK ();
    }
 
  return signal_id;
}

static inline ClassClosure*
signal_find_class_closure (SignalNode *node,
			   GType       itype)
{
  GBSearchArray *bsa = node->class_closure_bsa;
  ClassClosure *cc;

  if (bsa)
    {
      ClassClosure key;

      /* cc->instance_type is 0 for default closure */
      
      key.instance_type = itype;
      cc = g_bsearch_array_lookup (bsa, &g_class_closure_bconfig, &key);
      while (!cc && key.instance_type)
	{
	  key.instance_type = g_type_parent (key.instance_type);
	  cc = g_bsearch_array_lookup (bsa, &g_class_closure_bconfig, &key);
	}
    }
  else
    cc = NULL;
  return cc;
}

static inline GClosure*
signal_lookup_closure (SignalNode    *node,
		       GTypeInstance *instance)
{
  ClassClosure *cc;

  if (node->class_closure_bsa && g_bsearch_array_get_n_nodes (node->class_closure_bsa) == 1)
    cc = g_bsearch_array_get_nth (node->class_closure_bsa, &g_class_closure_bconfig, 0);
  else
    cc = signal_find_class_closure (node, G_TYPE_FROM_INSTANCE (instance));
  return cc ? cc->closure : NULL;
}

static void
signal_add_class_closure (SignalNode *node,
			  GType       itype,
			  GClosure   *closure)
{
  ClassClosure key;

  /* can't optimize NOP emissions with overridden class closures */
  node->test_class_offset = 0;

  if (!node->class_closure_bsa)
    node->class_closure_bsa = g_bsearch_array_create (&g_class_closure_bconfig);
  key.instance_type = itype;
  key.closure = g_closure_ref (closure);
  node->class_closure_bsa = g_bsearch_array_insert (node->class_closure_bsa,
						    &g_class_closure_bconfig,
						    &key);
  g_closure_sink (closure);
  if (node->c_marshaller && closure && G_CLOSURE_NEEDS_MARSHAL (closure))
    g_closure_set_marshal (closure, node->c_marshaller);
}

guint
g_signal_newv (const gchar       *signal_name,
               GType              itype,
               GSignalFlags       signal_flags,
               GClosure          *class_closure,
               GSignalAccumulator accumulator,
	       gpointer		  accu_data,
               GSignalCMarshaller c_marshaller,
               GType		  return_type,
               guint              n_params,
               GType		 *param_types)
{
  gchar *name;
  guint signal_id, i;
  SignalNode *node;
  
  g_return_val_if_fail (signal_name != NULL, 0);
  g_return_val_if_fail (G_TYPE_IS_INSTANTIATABLE (itype) || G_TYPE_IS_INTERFACE (itype), 0);
  if (n_params)
    g_return_val_if_fail (param_types != NULL, 0);
  g_return_val_if_fail ((return_type & G_SIGNAL_TYPE_STATIC_SCOPE) == 0, 0);
  if (return_type == (G_TYPE_NONE & ~G_SIGNAL_TYPE_STATIC_SCOPE))
    g_return_val_if_fail (accumulator == NULL, 0);
  if (!accumulator)
    g_return_val_if_fail (accu_data == NULL, 0);

  name = g_strdup (signal_name);
  g_strdelimit (name, G_STR_DELIMITERS ":^", '_');  /* FIXME do character checks like for types */
  
  SIGNAL_LOCK ();
  
  signal_id = signal_id_lookup (g_quark_try_string (name), itype);
  node = LOOKUP_SIGNAL_NODE (signal_id);
  if (node && !node->destroyed)
    {
      g_warning (G_STRLOC ": signal \"%s\" already exists in the `%s' %s",
                 name,
                 type_debug_name (node->itype),
                 G_TYPE_IS_INTERFACE (node->itype) ? "interface" : "class ancestry");
      g_free (name);
      SIGNAL_UNLOCK ();
      return 0;
    }
  if (node && node->itype != itype)
    {
      g_warning (G_STRLOC ": signal \"%s\" for type `%s' was previously created for type `%s'",
                 name,
                 type_debug_name (itype),
                 type_debug_name (node->itype));
      g_free (name);
      SIGNAL_UNLOCK ();
      return 0;
    }
  for (i = 0; i < n_params; i++)
    if (!G_TYPE_IS_VALUE (param_types[i] & ~G_SIGNAL_TYPE_STATIC_SCOPE))
      {
	g_warning (G_STRLOC ": parameter %d of type `%s' for signal \"%s::%s\" is not a value type",
		   i + 1, type_debug_name (param_types[i]), type_debug_name (itype), name);
	g_free (name);
	SIGNAL_UNLOCK ();
	return 0;
      }
  if (return_type != G_TYPE_NONE && !G_TYPE_IS_VALUE (return_type & ~G_SIGNAL_TYPE_STATIC_SCOPE))
    {
      g_warning (G_STRLOC ": return value of type `%s' for signal \"%s::%s\" is not a value type",
		 type_debug_name (return_type), type_debug_name (itype), name);
      g_free (name);
      SIGNAL_UNLOCK ();
      return 0;
    }
  if (return_type != G_TYPE_NONE &&
      (signal_flags & (G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST | G_SIGNAL_RUN_CLEANUP)) == G_SIGNAL_RUN_FIRST)
    {
      g_warning (G_STRLOC ": signal \"%s::%s\" has return type `%s' and is only G_SIGNAL_RUN_FIRST",
		 type_debug_name (itype), name, type_debug_name (return_type));
      g_free (name);
      SIGNAL_UNLOCK ();
      return 0;
    }
  
  /* setup permanent portion of signal node */
  if (!node)
    {
      SignalKey key;
      
      signal_id = g_n_signal_nodes++;
      node = g_new (SignalNode, 1);
      node->signal_id = signal_id;
      g_signal_nodes = g_renew (SignalNode*, g_signal_nodes, g_n_signal_nodes);
      g_signal_nodes[signal_id] = node;
      node->itype = itype;
      node->name = name;
      key.itype = itype;
      key.quark = g_quark_from_string (node->name);
      key.signal_id = signal_id;
      g_signal_key_bsa = g_bsearch_array_insert (g_signal_key_bsa, &g_signal_key_bconfig, &key);
      g_strdelimit (node->name, "_", '-');
      key.quark = g_quark_from_static_string (node->name);
      g_signal_key_bsa = g_bsearch_array_insert (g_signal_key_bsa, &g_signal_key_bconfig, &key);
    }
  node->destroyed = FALSE;
  node->test_class_offset = 0;

  /* setup reinitializable portion */
  node->flags = signal_flags & G_SIGNAL_FLAGS_MASK;
  node->n_params = n_params;
  node->param_types = g_memdup (param_types, sizeof (GType) * n_params);
  node->return_type = return_type;
  node->class_closure_bsa = NULL;
  if (accumulator)
    {
      node->accumulator = g_new (SignalAccumulator, 1);
      node->accumulator->func = accumulator;
      node->accumulator->data = accu_data;
    }
  else
    node->accumulator = NULL;
  node->c_marshaller = c_marshaller;
  node->emission_hooks = NULL;
  if (class_closure)
    signal_add_class_closure (node, 0, class_closure);
  else if (G_TYPE_IS_INSTANTIATABLE (itype) && return_type == G_TYPE_NONE)
    {
      /* optimize NOP emissions */
      node->test_class_offset = TEST_CLASS_MAGIC;
    }
  SIGNAL_UNLOCK ();

  return signal_id;
}

guint
g_signal_new_valist (const gchar       *signal_name,
                     GType              itype,
                     GSignalFlags       signal_flags,
                     GClosure          *class_closure,
                     GSignalAccumulator accumulator,
		     gpointer		accu_data,
                     GSignalCMarshaller c_marshaller,
                     GType              return_type,
                     guint              n_params,
                     va_list            args)
{
  GType *param_types;
  guint i;
  guint signal_id;

  if (n_params > 0)
    {
      param_types = g_new (GType, n_params);

      for (i = 0; i < n_params; i++)
	param_types[i] = va_arg (args, GType);
    }
  else
    param_types = NULL;

  signal_id = g_signal_newv (signal_name, itype, signal_flags,
			     class_closure, accumulator, accu_data, c_marshaller,
			     return_type, n_params, param_types);
  g_free (param_types);

  return signal_id;
}

static void
signal_destroy_R (SignalNode *signal_node)
{
  SignalNode node = *signal_node;

  signal_node->destroyed = TRUE;
  
  /* reentrancy caution, zero out real contents first */
  signal_node->test_class_offset = 0;
  signal_node->n_params = 0;
  signal_node->param_types = NULL;
  signal_node->return_type = 0;
  signal_node->class_closure_bsa = NULL;
  signal_node->accumulator = NULL;
  signal_node->c_marshaller = NULL;
  signal_node->emission_hooks = NULL;
  
#ifdef	G_ENABLE_DEBUG
  /* check current emissions */
  {
    Emission *emission;
    
    for (emission = (node.flags & G_SIGNAL_NO_RECURSE) ? g_restart_emissions : g_recursive_emissions;
         emission; emission = emission->next)
      if (emission->ihint.signal_id == node.signal_id)
        g_critical (G_STRLOC ": signal \"%s\" being destroyed is currently in emission (instance `%p')",
                    node.name, emission->instance);
  }
#endif
  
  /* free contents that need to
   */
  SIGNAL_UNLOCK ();
  g_free (node.param_types);
  if (node.class_closure_bsa)
    {
      guint i;

      for (i = 0; i < node.class_closure_bsa->n_nodes; i++)
	{
	  ClassClosure *cc = g_bsearch_array_get_nth (node.class_closure_bsa, &g_class_closure_bconfig, i);

	  g_closure_unref (cc->closure);
	}
      g_bsearch_array_free (node.class_closure_bsa, &g_class_closure_bconfig);
    }
  g_free (node.accumulator);
  if (node.emission_hooks)
    {
      g_hook_list_clear (node.emission_hooks);
      g_free (node.emission_hooks);
    }
  SIGNAL_LOCK ();
}

void
g_signal_override_class_closure (guint     signal_id,
				 GType     instance_type,
				 GClosure *class_closure)
{
  SignalNode *node;
  
  g_return_if_fail (signal_id > 0);
  g_return_if_fail (class_closure != NULL);
  
  SIGNAL_LOCK ();
  node = LOOKUP_SIGNAL_NODE (signal_id);
  if (!g_type_is_a (instance_type, node->itype))
    g_warning ("%s: type `%s' cannot be overridden for signal id `%u'", G_STRLOC, type_debug_name (instance_type), signal_id);
  else
    {
      ClassClosure *cc = signal_find_class_closure (node, instance_type);
      
      if (cc && cc->instance_type == instance_type)
	g_warning ("%s: type `%s' is already overridden for signal id `%u'", G_STRLOC, type_debug_name (instance_type), signal_id);
      else
	signal_add_class_closure (node, instance_type, class_closure);
    }
  SIGNAL_UNLOCK ();
}

void
g_signal_chain_from_overridden (const GValue *instance_and_params,
				GValue       *return_value)
{
  GType chain_type = 0, restore_type = 0;
  Emission *emission = NULL;
  GClosure *closure = NULL;
  guint n_params = 0;
  gpointer instance;
  
  g_return_if_fail (instance_and_params != NULL);
  instance = g_value_peek_pointer (instance_and_params);
  g_return_if_fail (G_TYPE_CHECK_INSTANCE (instance));
  
  SIGNAL_LOCK ();
  emission = emission_find_innermost (instance);
  if (emission)
    {
      SignalNode *node = LOOKUP_SIGNAL_NODE (emission->ihint.signal_id);
      
      g_assert (node != NULL);	/* paranoid */
      
      /* we should probably do the same parameter checks as g_signal_emit() here.
       */
      if (emission->chain_type != G_TYPE_NONE)
	{
	  ClassClosure *cc = signal_find_class_closure (node, emission->chain_type);
	  
	  g_assert (cc != NULL);	/* closure currently in call stack */

	  n_params = node->n_params;
	  restore_type = cc->instance_type;
	  cc = signal_find_class_closure (node, g_type_parent (cc->instance_type));
	  if (cc && cc->instance_type != restore_type)
	    {
	      closure = cc->closure;
	      chain_type = cc->instance_type;
	    }
	}
      else
	g_warning ("%s: signal id `%u' cannot be chained from current emission stage for instance `%p'", G_STRLOC, node->signal_id, instance);
    }
  else
    g_warning ("%s: no signal is currently being emitted for instance `%p'", G_STRLOC, instance);
  if (closure)
    {
      emission->chain_type = chain_type;
      SIGNAL_UNLOCK ();
      g_closure_invoke (closure,
			return_value,
			n_params + 1,
			instance_and_params,
			&emission->ihint);
      SIGNAL_LOCK ();
      emission->chain_type = restore_type;
    }
  SIGNAL_UNLOCK ();
}

GSignalInvocationHint*
g_signal_get_invocation_hint (gpointer instance)
{
  Emission *emission = NULL;
  
  g_return_val_if_fail (G_TYPE_CHECK_INSTANCE (instance), NULL);

  SIGNAL_LOCK ();
  emission = emission_find_innermost (instance);
  SIGNAL_UNLOCK ();
  
  return emission ? &emission->ihint : NULL;
}

gulong
g_signal_connect_closure_by_id (gpointer  instance,
				guint     signal_id,
				GQuark    detail,
				GClosure *closure,
				gboolean  after)
{
  SignalNode *node;
  gulong handler_seq_no = 0;
  
  g_return_val_if_fail (G_TYPE_CHECK_INSTANCE (instance), 0);
  g_return_val_if_fail (signal_id > 0, 0);
  g_return_val_if_fail (closure != NULL, 0);
  
  SIGNAL_LOCK ();
  node = LOOKUP_SIGNAL_NODE (signal_id);
  if (node)
    {
      if (detail && !(node->flags & G_SIGNAL_DETAILED))
	g_warning ("%s: signal id `%u' does not support detail (%u)", G_STRLOC, signal_id, detail);
      else if (!g_type_is_a (G_TYPE_FROM_INSTANCE (instance), node->itype))
	g_warning ("%s: signal id `%u' is invalid for instance `%p'", G_STRLOC, signal_id, instance);
      else
	{
	  Handler *handler = handler_new (after);
	  
	  handler_seq_no = handler->sequential_number;
	  handler->detail = detail;
	  handler->closure = g_closure_ref (closure);
	  g_closure_sink (closure);
	  handler_insert (signal_id, instance, handler);
	  if (node->c_marshaller && G_CLOSURE_NEEDS_MARSHAL (closure))
	    g_closure_set_marshal (closure, node->c_marshaller);
	}
    }
  else
    g_warning ("%s: signal id `%u' is invalid for instance `%p'", G_STRLOC, signal_id, instance);
  SIGNAL_UNLOCK ();
  
  return handler_seq_no;
}

gulong
g_signal_connect_closure (gpointer     instance,
			  const gchar *detailed_signal,
			  GClosure    *closure,
			  gboolean     after)
{
  guint signal_id;
  gulong handler_seq_no = 0;
  GQuark detail = 0;
  GType itype;

  g_return_val_if_fail (G_TYPE_CHECK_INSTANCE (instance), 0);
  g_return_val_if_fail (detailed_signal != NULL, 0);
  g_return_val_if_fail (closure != NULL, 0);

  SIGNAL_LOCK ();
  itype = G_TYPE_FROM_INSTANCE (instance);
  signal_id = signal_parse_name (detailed_signal, itype, &detail, TRUE);
  if (signal_id)
    {
      SignalNode *node = LOOKUP_SIGNAL_NODE (signal_id);

      if (detail && !(node->flags & G_SIGNAL_DETAILED))
	g_warning ("%s: signal `%s' does not support details", G_STRLOC, detailed_signal);
      else if (!g_type_is_a (itype, node->itype))
	g_warning ("%s: signal `%s' is invalid for instance `%p'", G_STRLOC, detailed_signal, instance);
      else
	{
	  Handler *handler = handler_new (after);

	  handler_seq_no = handler->sequential_number;
	  handler->detail = detail;
	  handler->closure = g_closure_ref (closure);
	  g_closure_sink (closure);
	  handler_insert (signal_id, instance, handler);
	  if (node->c_marshaller && G_CLOSURE_NEEDS_MARSHAL (handler->closure))
	    g_closure_set_marshal (handler->closure, node->c_marshaller);
	}
    }
  else
    g_warning ("%s: signal `%s' is invalid for instance `%p'", G_STRLOC, detailed_signal, instance);
  SIGNAL_UNLOCK ();

  return handler_seq_no;
}

gulong
g_signal_connect_data (gpointer       instance,
		       const gchar   *detailed_signal,
		       GCallback      c_handler,
		       gpointer       data,
		       GClosureNotify destroy_data,
		       GConnectFlags  connect_flags)
{
  guint signal_id;
  gulong handler_seq_no = 0;
  GQuark detail = 0;
  GType itype;
  gboolean swapped, after;
  
  g_return_val_if_fail (G_TYPE_CHECK_INSTANCE (instance), 0);
  g_return_val_if_fail (detailed_signal != NULL, 0);
  g_return_val_if_fail (c_handler != NULL, 0);

  swapped = (connect_flags & G_CONNECT_SWAPPED) != FALSE;
  after = (connect_flags & G_CONNECT_AFTER) != FALSE;

  SIGNAL_LOCK ();
  itype = G_TYPE_FROM_INSTANCE (instance);
  signal_id = signal_parse_name (detailed_signal, itype, &detail, TRUE);
  if (signal_id)
    {
      SignalNode *node = LOOKUP_SIGNAL_NODE (signal_id);

      if (detail && !(node->flags & G_SIGNAL_DETAILED))
	g_warning ("%s: signal `%s' does not support details", G_STRLOC, detailed_signal);
      else if (!g_type_is_a (itype, node->itype))
	g_warning ("%s: signal `%s' is invalid for instance `%p'", G_STRLOC, detailed_signal, instance);
      else
	{
	  Handler *handler = handler_new (after);

	  handler_seq_no = handler->sequential_number;
	  handler->detail = detail;
	  handler->closure = g_closure_ref ((swapped ? g_cclosure_new_swap : g_cclosure_new) (c_handler, data, destroy_data));
	  g_closure_sink (handler->closure);
	  handler_insert (signal_id, instance, handler);
	  if (node->c_marshaller && G_CLOSURE_NEEDS_MARSHAL (handler->closure))
	    g_closure_set_marshal (handler->closure, node->c_marshaller);
	}
    }
  else
    g_warning ("%s: signal `%s' is invalid for instance `%p'", G_STRLOC, detailed_signal, instance);
  SIGNAL_UNLOCK ();

  return handler_seq_no;
}

void
g_signal_handler_block (gpointer instance,
                        gulong   handler_id)
{
  Handler *handler;
  
  g_return_if_fail (G_TYPE_CHECK_INSTANCE (instance));
  g_return_if_fail (handler_id > 0);
  
  SIGNAL_LOCK ();
  handler = handler_lookup (instance, handler_id, NULL);
  if (handler)
    {
#ifndef G_DISABLE_CHECKS
      if (handler->block_count >= HANDLER_MAX_BLOCK_COUNT - 1)
        g_error (G_STRLOC ": handler block_count overflow, %s", REPORT_BUG);
#endif
      handler->block_count += 1;
    }
  else
    g_warning ("%s: instance `%p' has no handler with id `%lu'", G_STRLOC, instance, handler_id);
  SIGNAL_UNLOCK ();
}

void
g_signal_handler_unblock (gpointer instance,
                          gulong   handler_id)
{
  Handler *handler;
  
  g_return_if_fail (G_TYPE_CHECK_INSTANCE (instance));
  g_return_if_fail (handler_id > 0);
  
  SIGNAL_LOCK ();
  handler = handler_lookup (instance, handler_id, NULL);
  if (handler)
    {
      if (handler->block_count)
        handler->block_count -= 1;
      else
        g_warning (G_STRLOC ": handler `%lu' of instance `%p' is not blocked", handler_id, instance);
    }
  else
    g_warning ("%s: instance `%p' has no handler with id `%lu'", G_STRLOC, instance, handler_id);
  SIGNAL_UNLOCK ();
}

void
g_signal_handler_disconnect (gpointer instance,
                             gulong   handler_id)
{
  Handler *handler;
  guint signal_id;
  
  g_return_if_fail (G_TYPE_CHECK_INSTANCE (instance));
  g_return_if_fail (handler_id > 0);
  
  SIGNAL_LOCK ();
  handler = handler_lookup (instance, handler_id, &signal_id);
  if (handler)
    {
      handler->sequential_number = 0;
      handler->block_count = 1;
      handler_unref_R (signal_id, instance, handler);
    }
  else
    g_warning ("%s: instance `%p' has no handler with id `%lu'", G_STRLOC, instance, handler_id);
  SIGNAL_UNLOCK ();
}

gboolean
g_signal_handler_is_connected (gpointer instance,
			       gulong   handler_id)
{
  Handler *handler;
  gboolean connected;

  g_return_val_if_fail (G_TYPE_CHECK_INSTANCE (instance), FALSE);

  SIGNAL_LOCK ();
  handler = handler_lookup (instance, handler_id, NULL);
  connected = handler != NULL;
  SIGNAL_UNLOCK ();

  return connected;
}

void
g_signal_handlers_destroy (gpointer instance)
{
  GBSearchArray *hlbsa;
  
  g_return_if_fail (G_TYPE_CHECK_INSTANCE (instance));
  
  SIGNAL_LOCK ();
  hlbsa = g_hash_table_lookup (g_handler_list_bsa_ht, instance);
  if (hlbsa)
    {
      guint i;
      
      /* reentrancy caution, delete instance trace first */
      g_hash_table_remove (g_handler_list_bsa_ht, instance);
      
      for (i = 0; i < hlbsa->n_nodes; i++)
        {
          HandlerList *hlist = g_bsearch_array_get_nth (hlbsa, &g_signal_hlbsa_bconfig, i);
          Handler *handler = hlist->handlers;
	  
          while (handler)
            {
              Handler *tmp = handler;
	      
              handler = tmp->next;
              tmp->block_count = 1;
              /* cruel unlink, this works because _all_ handlers vanish */
              tmp->next = NULL;
              tmp->prev = tmp;
              if (tmp->sequential_number)
		{
		  tmp->sequential_number = 0;
		  handler_unref_R (0, NULL, tmp);
		}
            }
        }
      g_bsearch_array_free (hlbsa, &g_signal_hlbsa_bconfig);
    }
  SIGNAL_UNLOCK ();
}

gulong
g_signal_handler_find (gpointer         instance,
                       GSignalMatchType mask,
                       guint            signal_id,
		       GQuark		detail,
                       GClosure        *closure,
                       gpointer         func,
                       gpointer         data)
{
  gulong handler_seq_no = 0;
  
  g_return_val_if_fail (G_TYPE_CHECK_INSTANCE (instance), 0);
  g_return_val_if_fail ((mask & ~G_SIGNAL_MATCH_MASK) == 0, 0);
  
  if (mask & G_SIGNAL_MATCH_MASK)
    {
      HandlerMatch *mlist;
      
      SIGNAL_LOCK ();
      mlist = handlers_find (instance, mask, signal_id, detail, closure, func, data, TRUE);
      if (mlist)
	{
	  handler_seq_no = mlist->handler->sequential_number;
	  handler_match_free1_R (mlist, instance);
	}
      SIGNAL_UNLOCK ();
    }
  
  return handler_seq_no;
}

static guint
signal_handlers_foreach_matched_R (gpointer         instance,
				   GSignalMatchType mask,
				   guint            signal_id,
				   GQuark           detail,
				   GClosure        *closure,
				   gpointer         func,
				   gpointer         data,
				   void		  (*callback) (gpointer instance,
							       gulong   handler_seq_no))
{
  HandlerMatch *mlist;
  guint n_handlers = 0;
  
  mlist = handlers_find (instance, mask, signal_id, detail, closure, func, data, FALSE);
  while (mlist)
    {
      n_handlers++;
      if (mlist->handler->sequential_number)
	{
	  SIGNAL_UNLOCK ();
	  callback (instance, mlist->handler->sequential_number);
	  SIGNAL_LOCK ();
	}
      mlist = handler_match_free1_R (mlist, instance);
    }
  
  return n_handlers;
}

guint
g_signal_handlers_block_matched (gpointer         instance,
				 GSignalMatchType mask,
				 guint            signal_id,
				 GQuark           detail,
				 GClosure        *closure,
				 gpointer         func,
				 gpointer         data)
{
  guint n_handlers = 0;
  
  g_return_val_if_fail (G_TYPE_CHECK_INSTANCE (instance), 0);
  g_return_val_if_fail ((mask & ~G_SIGNAL_MATCH_MASK) == 0, 0);
  
  if (mask & (G_SIGNAL_MATCH_CLOSURE | G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA))
    {
      SIGNAL_LOCK ();
      n_handlers = signal_handlers_foreach_matched_R (instance, mask, signal_id, detail,
						      closure, func, data,
						      g_signal_handler_block);
      SIGNAL_UNLOCK ();
    }
  
  return n_handlers;
}

guint
g_signal_handlers_unblock_matched (gpointer         instance,
				   GSignalMatchType mask,
				   guint            signal_id,
				   GQuark           detail,
				   GClosure        *closure,
				   gpointer         func,
				   gpointer         data)
{
  guint n_handlers = 0;
  
  g_return_val_if_fail (G_TYPE_CHECK_INSTANCE (instance), 0);
  g_return_val_if_fail ((mask & ~G_SIGNAL_MATCH_MASK) == 0, 0);
  
  if (mask & (G_SIGNAL_MATCH_CLOSURE | G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA))
    {
      SIGNAL_LOCK ();
      n_handlers = signal_handlers_foreach_matched_R (instance, mask, signal_id, detail,
						      closure, func, data,
						      g_signal_handler_unblock);
      SIGNAL_UNLOCK ();
    }
  
  return n_handlers;
}

guint
g_signal_handlers_disconnect_matched (gpointer         instance,
				      GSignalMatchType mask,
				      guint            signal_id,
				      GQuark           detail,
				      GClosure        *closure,
				      gpointer         func,
				      gpointer         data)
{
  guint n_handlers = 0;
  
  g_return_val_if_fail (G_TYPE_CHECK_INSTANCE (instance), 0);
  g_return_val_if_fail ((mask & ~G_SIGNAL_MATCH_MASK) == 0, 0);
  
  if (mask & (G_SIGNAL_MATCH_CLOSURE | G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA))
    {
      SIGNAL_LOCK ();
      n_handlers = signal_handlers_foreach_matched_R (instance, mask, signal_id, detail,
						      closure, func, data,
						      g_signal_handler_disconnect);
      SIGNAL_UNLOCK ();
    }
  
  return n_handlers;
}

gboolean
g_signal_has_handler_pending (gpointer instance,
			      guint    signal_id,
			      GQuark   detail,
			      gboolean may_be_blocked)
{
  HandlerMatch *mlist;
  gboolean has_pending;
  
  g_return_val_if_fail (G_TYPE_CHECK_INSTANCE (instance), FALSE);
  g_return_val_if_fail (signal_id > 0, FALSE);
  
  SIGNAL_LOCK ();
  if (detail)
    {
      SignalNode *node = LOOKUP_SIGNAL_NODE (signal_id);
      
      if (!(node->flags & G_SIGNAL_DETAILED))
	{
	  g_warning ("%s: signal id `%u' does not support detail (%u)", G_STRLOC, signal_id, detail);
	  SIGNAL_UNLOCK ();
	  return FALSE;
	}
    }
  mlist = handlers_find (instance,
			 (G_SIGNAL_MATCH_ID | G_SIGNAL_MATCH_DETAIL | (may_be_blocked ? 0 : G_SIGNAL_MATCH_UNBLOCKED)),
			 signal_id, detail, NULL, NULL, NULL, TRUE);
  if (mlist)
    {
      has_pending = TRUE;
      handler_match_free1_R (mlist, instance);
    }
  else
    has_pending = FALSE;
  SIGNAL_UNLOCK ();
  
  return has_pending;
}

static inline gboolean
signal_check_skip_emission (SignalNode *node,
			    gpointer    instance,
			    GQuark      detail)
{
  HandlerList *hlist;

  /* are we able to check for NULL class handlers? */
  if (!node->test_class_offset)
    return FALSE;

  /* are there emission hooks pending? */
  if (node->emission_hooks && node->emission_hooks->hooks)
    return FALSE;

  /* is there a non-NULL class handler? */
  if (node->test_class_offset != TEST_CLASS_MAGIC)
    {
      GTypeClass *class = G_TYPE_INSTANCE_GET_CLASS (instance, G_TYPE_FROM_INSTANCE (instance), GTypeClass);

      if (G_STRUCT_MEMBER (gpointer, class, node->test_class_offset))
	return FALSE;
    }

  /* are signals being debugged? */
#ifdef  G_ENABLE_DEBUG
  IF_DEBUG (SIGNALS, g_trace_instance_signals || g_trap_instance_signals)
    return FALSE;
#endif /* G_ENABLE_DEBUG */

  /* is this a no-recurse signal already in emission? */
  if (node->flags & G_SIGNAL_NO_RECURSE &&
      emission_find (g_restart_emissions, node->signal_id, detail, instance))
    return FALSE;

  /* do we have pending handlers? */
  hlist = handler_list_lookup (node->signal_id, instance);
  if (hlist && hlist->handlers)
    return FALSE;

  /* none of the above, no emission required */
  return TRUE;
}

void
g_signal_emitv (const GValue *instance_and_params,
		guint         signal_id,
		GQuark	      detail,
		GValue       *return_value)
{
  const GValue *param_values;
  gpointer instance;
  SignalNode *node;
#ifdef G_ENABLE_DEBUG
  guint i;
#endif
  
  g_return_if_fail (instance_and_params != NULL);
  instance = g_value_peek_pointer (instance_and_params);
  g_return_if_fail (G_TYPE_CHECK_INSTANCE (instance));
  g_return_if_fail (signal_id > 0);

  param_values = instance_and_params + 1;
  
  SIGNAL_LOCK ();
  node = LOOKUP_SIGNAL_NODE (signal_id);
  if (!node || !g_type_is_a (G_TYPE_FROM_INSTANCE (instance), node->itype))
    {
      g_warning ("%s: signal id `%u' is invalid for instance `%p'", G_STRLOC, signal_id, instance);
      SIGNAL_UNLOCK ();
      return;
    }
#ifdef G_ENABLE_DEBUG
  if (detail && !(node->flags & G_SIGNAL_DETAILED))
    {
      g_warning ("%s: signal id `%u' does not support detail (%u)", G_STRLOC, signal_id, detail);
      SIGNAL_UNLOCK ();
      return;
    }
  for (i = 0; i < node->n_params; i++)
    if (!G_TYPE_CHECK_VALUE_TYPE (param_values + i, node->param_types[i] & ~G_SIGNAL_TYPE_STATIC_SCOPE))
      {
	g_critical ("%s: value for `%s' parameter %u for signal \"%s\" is of type `%s'",
		    G_STRLOC,
		    type_debug_name (node->param_types[i]),
		    i,
		    node->name,
		    G_VALUE_TYPE_NAME (param_values + i));
	SIGNAL_UNLOCK ();
	return;
      }
  if (node->return_type != G_TYPE_NONE)
    {
      if (!return_value)
	{
	  g_critical ("%s: return value `%s' for signal \"%s\" is (NULL)",
		      G_STRLOC,
		      type_debug_name (node->return_type),
		      node->name);
	  SIGNAL_UNLOCK ();
	  return;
	}
      else if (!node->accumulator && !G_TYPE_CHECK_VALUE_TYPE (return_value, node->return_type & ~G_SIGNAL_TYPE_STATIC_SCOPE))
	{
	  g_critical ("%s: return value `%s' for signal \"%s\" is of type `%s'",
		      G_STRLOC,
		      type_debug_name (node->return_type),
		      node->name,
		      G_VALUE_TYPE_NAME (return_value));
	  SIGNAL_UNLOCK ();
	  return;
	}
    }
  else
    return_value = NULL;
#endif	/* G_ENABLE_DEBUG */

  /* optimize NOP emissions */
  if (signal_check_skip_emission (node, instance, detail))
    {
      /* nothing to do to emit this signal */
      SIGNAL_UNLOCK ();
      /* g_printerr ("omitting emission of \"%s\"\n", node->name); */
      return;
    }

  SIGNAL_UNLOCK ();
  signal_emit_unlocked_R (node, detail, instance, return_value, instance_and_params);
}

void
g_signal_emit_valist (gpointer instance,
		      guint    signal_id,
		      GQuark   detail,
		      va_list  var_args)
{
  GValue *instance_and_params, stack_values[MAX_STACK_VALUES], *free_me = NULL;
  GType signal_return_type;
  GValue *param_values;
  SignalNode *node;
  guint i, n_params;
  
  g_return_if_fail (G_TYPE_CHECK_INSTANCE (instance));
  g_return_if_fail (signal_id > 0);

  SIGNAL_LOCK ();
  node = LOOKUP_SIGNAL_NODE (signal_id);
  if (!node || !g_type_is_a (G_TYPE_FROM_INSTANCE (instance), node->itype))
    {
      g_warning ("%s: signal id `%u' is invalid for instance `%p'", G_STRLOC, signal_id, instance);
      SIGNAL_UNLOCK ();
      return;
    }
#ifndef G_DISABLE_CHECKS
  if (detail && !(node->flags & G_SIGNAL_DETAILED))
    {
      g_warning ("%s: signal id `%u' does not support detail (%u)", G_STRLOC, signal_id, detail);
      SIGNAL_UNLOCK ();
      return;
    }
#endif  /* !G_DISABLE_CHECKS */

  /* optimize NOP emissions */
  if (signal_check_skip_emission (node, instance, detail))
    {
      /* nothing to do to emit this signal */
      SIGNAL_UNLOCK ();
      /* g_printerr ("omitting emission of \"%s\"\n", node->name); */
      return;
    }

  n_params = node->n_params;
  signal_return_type = node->return_type;
  if (node->n_params < MAX_STACK_VALUES)
    instance_and_params = stack_values;
  else
    {
      free_me = g_new (GValue, node->n_params + 1);
      instance_and_params = free_me;
    }
  param_values = instance_and_params + 1;
  for (i = 0; i < node->n_params; i++)
    {
      gchar *error;
      GType ptype = node->param_types[i] & ~G_SIGNAL_TYPE_STATIC_SCOPE;
      gboolean static_scope = node->param_types[i] & G_SIGNAL_TYPE_STATIC_SCOPE;
      
      param_values[i].g_type = 0;
      SIGNAL_UNLOCK ();
      g_value_init (param_values + i, ptype);
      G_VALUE_COLLECT (param_values + i,
		       var_args,
		       static_scope ? G_VALUE_NOCOPY_CONTENTS : 0,
		       &error);
      if (error)
	{
	  g_warning ("%s: %s", G_STRLOC, error);
	  g_free (error);

	  /* we purposely leak the value here, it might not be
	   * in a sane state if an error condition occoured
	   */
	  while (i--)
	    g_value_unset (param_values + i);

	  g_free (free_me);
	  return;
	}
      SIGNAL_LOCK ();
    }
  SIGNAL_UNLOCK ();
  instance_and_params->g_type = 0;
  g_value_init (instance_and_params, G_TYPE_FROM_INSTANCE (instance));
  g_value_set_instance (instance_and_params, instance);
  if (signal_return_type == G_TYPE_NONE)
    signal_emit_unlocked_R (node, detail, instance, NULL, instance_and_params);
  else
    {
      GValue return_value = { 0, };
      gchar *error = NULL;
      GType rtype = signal_return_type & ~G_SIGNAL_TYPE_STATIC_SCOPE;
      gboolean static_scope = signal_return_type & G_SIGNAL_TYPE_STATIC_SCOPE;
      
      g_value_init (&return_value, rtype);

      signal_emit_unlocked_R (node, detail, instance, &return_value, instance_and_params);

      G_VALUE_LCOPY (&return_value,
		     var_args,
		     static_scope ? G_VALUE_NOCOPY_CONTENTS : 0,
		     &error);
      if (!error)
	g_value_unset (&return_value);
      else
	{
	  g_warning ("%s: %s", G_STRLOC, error);
	  g_free (error);
	  
	  /* we purposely leak the value here, it might not be
	   * in a sane state if an error condition occured
	   */
	}
    }
  for (i = 0; i < n_params; i++)
    g_value_unset (param_values + i);
  g_value_unset (instance_and_params);
  if (free_me)
    g_free (free_me);
}

void
g_signal_emit (gpointer instance,
	       guint    signal_id,
	       GQuark   detail,
	       ...)
{
  va_list var_args;

  va_start (var_args, detail);
  g_signal_emit_valist (instance, signal_id, detail, var_args);
  va_end (var_args);
}

void
g_signal_emit_by_name (gpointer     instance,
		       const gchar *detailed_signal,
		       ...)
{
  GQuark detail = 0;
  guint signal_id;

  g_return_if_fail (G_TYPE_CHECK_INSTANCE (instance));
  g_return_if_fail (detailed_signal != NULL);

  SIGNAL_LOCK ();
  signal_id = signal_parse_name (detailed_signal, G_TYPE_FROM_INSTANCE (instance), &detail, TRUE);
  SIGNAL_UNLOCK ();

  if (signal_id)
    {
      va_list var_args;

      va_start (var_args, detailed_signal);
      g_signal_emit_valist (instance, signal_id, detail, var_args);
      va_end (var_args);
    }
  else
    g_warning ("%s: signal name `%s' is invalid for instance `%p'", G_STRLOC, detailed_signal, instance);
}

static inline gboolean
accumulate (GSignalInvocationHint *ihint,
	    GValue                *return_accu,
	    GValue	          *handler_return,
	    SignalAccumulator     *accumulator)
{
  gboolean continue_emission;

  if (!accumulator)
    return TRUE;

  continue_emission = accumulator->func (ihint, return_accu, handler_return, accumulator->data);
  g_value_reset (handler_return);

  return continue_emission;
}

static gboolean
signal_emit_unlocked_R (SignalNode   *node,
			GQuark	      detail,
			gpointer      instance,
			GValue	     *emission_return,
			const GValue *instance_and_params)
{
  SignalAccumulator *accumulator;
  Emission emission;
  GClosure *class_closure;
  HandlerList *hlist;
  Handler *handler_list = NULL;
  GValue *return_accu, accu = { 0, };
  guint signal_id;
  gulong max_sequential_handler_number;
  gboolean return_value_altered = FALSE;
  
#ifdef	G_ENABLE_DEBUG
  IF_DEBUG (SIGNALS, g_trace_instance_signals == instance || g_trap_instance_signals == instance)
    {
      g_message ("%s::%s(%u) emitted (instance=%p, signal-node=%p)",
		 g_type_name (G_TYPE_FROM_INSTANCE (instance)),
		 node->name, detail,
		 instance, node);
      if (g_trap_instance_signals == instance)
	G_BREAKPOINT ();
    }
#endif	/* G_ENABLE_DEBUG */
  
  SIGNAL_LOCK ();
  signal_id = node->signal_id;
  if (node->flags & G_SIGNAL_NO_RECURSE)
    {
      Emission *node = emission_find (g_restart_emissions, signal_id, detail, instance);
      
      if (node)
	{
	  node->state = EMISSION_RESTART;
	  SIGNAL_UNLOCK ();
	  return return_value_altered;
	}
    }
  accumulator = node->accumulator;
  if (accumulator)
    {
      SIGNAL_UNLOCK ();
      g_value_init (&accu, node->return_type & ~G_SIGNAL_TYPE_STATIC_SCOPE);
      return_accu = &accu;
      SIGNAL_LOCK ();
    }
  else
    return_accu = emission_return;
  emission.instance = instance;
  emission.ihint.signal_id = node->signal_id;
  emission.ihint.detail = detail;
  emission.ihint.run_type = 0;
  emission.state = 0;
  emission.chain_type = G_TYPE_NONE;
  emission_push ((node->flags & G_SIGNAL_NO_RECURSE) ? &g_restart_emissions : &g_recursive_emissions, &emission);
  class_closure = signal_lookup_closure (node, instance);
  
 EMIT_RESTART:
  
  if (handler_list)
    handler_unref_R (signal_id, instance, handler_list);
  max_sequential_handler_number = g_handler_sequential_number;
  hlist = handler_list_lookup (signal_id, instance);
  handler_list = hlist ? hlist->handlers : NULL;
  if (handler_list)
    handler_ref (handler_list);
  
  emission.ihint.run_type = G_SIGNAL_RUN_FIRST;
  
  if ((node->flags & G_SIGNAL_RUN_FIRST) && class_closure)
    {
      emission.state = EMISSION_RUN;

      emission.chain_type = G_TYPE_FROM_INSTANCE (instance);
      SIGNAL_UNLOCK ();
      g_closure_invoke (class_closure,
			return_accu,
			node->n_params + 1,
			instance_and_params,
			&emission.ihint);
      if (!accumulate (&emission.ihint, emission_return, &accu, accumulator) &&
	  emission.state == EMISSION_RUN)
	emission.state = EMISSION_STOP;
      SIGNAL_LOCK ();
      emission.chain_type = G_TYPE_NONE;
      return_value_altered = TRUE;
      
      if (emission.state == EMISSION_STOP)
	goto EMIT_CLEANUP;
      else if (emission.state == EMISSION_RESTART)
	goto EMIT_RESTART;
    }
  
  if (node->emission_hooks)
    {
      gboolean need_destroy, was_in_call, may_recurse = TRUE;
      GHook *hook;

      emission.state = EMISSION_HOOK;
      hook = g_hook_first_valid (node->emission_hooks, may_recurse);
      while (hook)
	{
	  SignalHook *signal_hook = SIGNAL_HOOK (hook);
	  
	  if (!signal_hook->detail || signal_hook->detail == detail)
	    {
	      GSignalEmissionHook hook_func = (GSignalEmissionHook) hook->func;
	      
	      was_in_call = G_HOOK_IN_CALL (hook);
	      hook->flags |= G_HOOK_FLAG_IN_CALL;
              SIGNAL_UNLOCK ();
	      need_destroy = !hook_func (&emission.ihint, node->n_params + 1, instance_and_params, hook->data);
	      SIGNAL_LOCK ();
	      if (!was_in_call)
		hook->flags &= ~G_HOOK_FLAG_IN_CALL;
	      if (need_destroy)
		g_hook_destroy_link (node->emission_hooks, hook);
	    }
	  hook = g_hook_next_valid (node->emission_hooks, hook, may_recurse);
	}
      
      if (emission.state == EMISSION_RESTART)
	goto EMIT_RESTART;
    }
  
  if (handler_list)
    {
      Handler *handler = handler_list;
      
      emission.state = EMISSION_RUN;
      handler_ref (handler);
      do
	{
	  Handler *tmp;
	  
	  if (handler->after)
	    {
	      handler_unref_R (signal_id, instance, handler_list);
	      handler_list = handler;
	      break;
	    }
	  else if (!handler->block_count && (!handler->detail || handler->detail == detail) &&
		   handler->sequential_number < max_sequential_handler_number)
	    {
	      SIGNAL_UNLOCK ();
	      g_closure_invoke (handler->closure,
				return_accu,
				node->n_params + 1,
				instance_and_params,
				&emission.ihint);
	      if (!accumulate (&emission.ihint, emission_return, &accu, accumulator) &&
		  emission.state == EMISSION_RUN)
		emission.state = EMISSION_STOP;
	      SIGNAL_LOCK ();
	      return_value_altered = TRUE;
	      
	      tmp = emission.state == EMISSION_RUN ? handler->next : NULL;
	    }
	  else
	    tmp = handler->next;
	  
	  if (tmp)
	    handler_ref (tmp);
	  handler_unref_R (signal_id, instance, handler_list);
	  handler_list = handler;
	  handler = tmp;
	}
      while (handler);
      
      if (emission.state == EMISSION_STOP)
	goto EMIT_CLEANUP;
      else if (emission.state == EMISSION_RESTART)
	goto EMIT_RESTART;
    }
  
  emission.ihint.run_type = G_SIGNAL_RUN_LAST;
  
  if ((node->flags & G_SIGNAL_RUN_LAST) && class_closure)
    {
      emission.state = EMISSION_RUN;
      
      emission.chain_type = G_TYPE_FROM_INSTANCE (instance);
      SIGNAL_UNLOCK ();
      g_closure_invoke (class_closure,
			return_accu,
			node->n_params + 1,
			instance_and_params,
			&emission.ihint);
      if (!accumulate (&emission.ihint, emission_return, &accu, accumulator) &&
	  emission.state == EMISSION_RUN)
	emission.state = EMISSION_STOP;
      SIGNAL_LOCK ();
      emission.chain_type = G_TYPE_NONE;
      return_value_altered = TRUE;
      
      if (emission.state == EMISSION_STOP)
	goto EMIT_CLEANUP;
      else if (emission.state == EMISSION_RESTART)
	goto EMIT_RESTART;
    }
  
  if (handler_list)
    {
      Handler *handler = handler_list;
      
      emission.state = EMISSION_RUN;
      handler_ref (handler);
      do
	{
	  Handler *tmp;
	  
	  if (handler->after && !handler->block_count && (!handler->detail || handler->detail == detail) &&
	      handler->sequential_number < max_sequential_handler_number)
	    {
	      SIGNAL_UNLOCK ();
	      g_closure_invoke (handler->closure,
				return_accu,
				node->n_params + 1,
				instance_and_params,
				&emission.ihint);
	      if (!accumulate (&emission.ihint, emission_return, &accu, accumulator) &&
		  emission.state == EMISSION_RUN)
		emission.state = EMISSION_STOP;
	      SIGNAL_LOCK ();
	      return_value_altered = TRUE;
	      
	      tmp = emission.state == EMISSION_RUN ? handler->next : NULL;
	    }
	  else
	    tmp = handler->next;
	  
	  if (tmp)
	    handler_ref (tmp);
	  handler_unref_R (signal_id, instance, handler);
	  handler = tmp;
	}
      while (handler);
      
      if (emission.state == EMISSION_STOP)
	goto EMIT_CLEANUP;
      else if (emission.state == EMISSION_RESTART)
	goto EMIT_RESTART;
    }
  
 EMIT_CLEANUP:
  
  emission.ihint.run_type = G_SIGNAL_RUN_CLEANUP;
  
  if ((node->flags & G_SIGNAL_RUN_CLEANUP) && class_closure)
    {
      gboolean need_unset = FALSE;
      
      emission.state = EMISSION_STOP;
      
      emission.chain_type = G_TYPE_FROM_INSTANCE (instance);
      SIGNAL_UNLOCK ();
      if (node->return_type != G_TYPE_NONE && !accumulator)
	{
	  g_value_init (&accu, node->return_type & ~G_SIGNAL_TYPE_STATIC_SCOPE);
	  need_unset = TRUE;
	}
      g_closure_invoke (class_closure,
			node->return_type != G_TYPE_NONE ? &accu : NULL,
			node->n_params + 1,
			instance_and_params,
			&emission.ihint);
      if (need_unset)
	g_value_unset (&accu);
      SIGNAL_LOCK ();
      emission.chain_type = G_TYPE_NONE;
      
      if (emission.state == EMISSION_RESTART)
	goto EMIT_RESTART;
    }
  
  if (handler_list)
    handler_unref_R (signal_id, instance, handler_list);
  
  emission_pop ((node->flags & G_SIGNAL_NO_RECURSE) ? &g_restart_emissions : &g_recursive_emissions, &emission);
  SIGNAL_UNLOCK ();
  if (accumulator)
    g_value_unset (&accu);
  
  return return_value_altered;
}

static const gchar*
type_debug_name (GType type)
{
  if (type)
    {
      const char *name = g_type_name (type & ~G_SIGNAL_TYPE_STATIC_SCOPE);
      return name ? name : "<unknown>";
    }
  else
    return "<invalid>";
}

gboolean
g_signal_accumulator_true_handled (GSignalInvocationHint *ihint,
				   GValue                *return_accu,
				   const GValue          *handler_return,
				   gpointer               dummy)
{
  gboolean continue_emission;
  gboolean signal_handled;
  
  signal_handled = g_value_get_boolean (handler_return);
  g_value_set_boolean (return_accu, signal_handled);
  continue_emission = !signal_handled;
  
  return continue_emission;
}

/* --- compile standard marshallers --- */
#include	"gobject.h"
#include	"genums.h"
#include        "gmarshal.c"
