/* GObject - GLib Type, Object, Parameter and Signal Library
 * Copyright (C) 2000 Red Hat, Inc.
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
#include        "gsignal.h"

#include        "gbsearcharray.h"


/* pre allocation configurations
 */
#define BSA_PRE_ALLOC           (20)
#define HANDLER_PRE_ALLOC       (48)
#define EMISSION_PRE_ALLOC      (16)

#define TIGHT_MEMORY    (1)

#define REPORT_BUG      "please report occourance circumstances to gtk-devel-list@gnome.org"


/* --- generic allocation --- */
/* we can special case allocations generically by replacing
 * these functions with more speed/memory aware variants
 */
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
static inline void
g_generic_node_free (GTrashStack **trash_stack_p,
                     gpointer      node)
{
  g_trash_stack_push (trash_stack_p, node);
}


/* --- typedefs --- */
typedef struct _SignalNode  SignalNode;
typedef struct _SignalKey   SignalKey;
typedef struct _Emission    Emission;
typedef struct _Handler     Handler;
typedef struct _HandlerList HandlerList;
typedef enum
{
  EMISSION_STOP,
  EMISSION_RUN,
  EMISSION_HOOK,
  EMISSION_RESTART
} EmissionState;


/* --- prototypes --- */
static inline guint             signal_id_lookup      (GQuark           quark,
						       GType            itype);
static        void              signal_destroy_R      (SignalNode      *signal_node);
static inline HandlerList*      handler_list_ensure   (guint            signal_id,
						       gpointer         instance);
static inline HandlerList*      handler_list_lookup   (guint            signal_id,
						       gpointer         instance);
static inline Handler*          handler_new           (gboolean         after);
static        void              handler_insert        (guint            signal_id,
						       gpointer         instance,
						       Handler         *handler);
static        Handler*          handler_lookup        (gpointer         instance,
						       guint            handler_id,
						       guint           *signal_id_p);
static        Handler*          handler_find          (gpointer         instance,
						       GSignalMatchType mask,
						       guint            signal_id,
						       GClosure        *closure,
						       gpointer         func,
						       gpointer         data);
static inline void              handler_ref           (Handler         *handler);
static inline void              handler_unref_R       (guint            signal_id,
						       gpointer         instance,
						       Handler         *handler);
static inline void              emission_push         (Emission       **emission_list_p,
						       guint            signal_id,
						       gpointer         instance,
						       EmissionState   *state_p);
static inline void              emission_pop          (Emission       **emission_list_p);
static inline Emission*         emission_find         (Emission        *emission_list,
						       guint            signal_id,
						       gpointer         instance);
static	      void		signal_emit_R	      (SignalNode      *node,
						       gpointer		instance,
						       GValue	       *return_value,
						       const GValue    *instance_and_params);


/* --- structures --- */
struct _SignalNode
{
  /* permanent portion */
  guint              signal_id;
  GType              itype;
  gchar             *name;
  guint              destroyed : 1;
  
  /* reinitializable portion */
  guint              flags : 8;
  guint              n_params : 8;
  GType		    *param_types;
  GType		     return_type;
  GClosure          *class_closure;
  GSignalAccumulator accumulator;
  GSignalCMarshaller c_marshaller;
  GHookList         *emission_hooks;
};

struct _SignalKey
{
  GType  itype;
  GQuark quark;
  guint  signal_id;
};

struct _Emission
{
  Emission      *next;
  guint          signal_id;
  gpointer       instance;
  EmissionState *state_p;
};

struct _HandlerList
{
  guint    signal_id;
  Handler *handlers;
};

struct _Handler
{
  guint         id;
  Handler      *next;
  Handler      *prev;
  guint         ref_count : 16;
#define HANDLER_MAX_REF_COUNT   (1 << 16)
  guint         block_count : 12;
#define HANDLER_MAX_BLOCK_COUNT (1 << 12)
  guint         after : 1;
  GClosure     *closure;
};


/* --- variables --- */
static GBSearchArray  g_signal_key_bsa = { NULL, 0, 0, 0, NULL };
static GHashTable    *g_handler_list_bsa_ht = NULL;
static Emission      *g_recursive_emissions = NULL;
static Emission      *g_restart_emissions = NULL;
static GTrashStack   *g_bsa_ts = NULL;
static GTrashStack   *g_handler_ts = NULL;
static GTrashStack   *g_emission_ts = NULL;
G_LOCK_DEFINE_STATIC (g_signal_mutex);


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
  SignalKey key, *signal_key;
  
  key.itype = itype;
  key.quark = quark;
  
  signal_key = g_bsearch_array_lookup (&g_signal_key_bsa, &key);
  
  return signal_key ? signal_key->signal_id : 0;
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
  
  if (!hlbsa)
    {
      hlbsa = g_generic_node_alloc (&g_bsa_ts,
                                    sizeof (GBSearchArray),
                                    BSA_PRE_ALLOC);
      hlbsa->cmp_func = handler_lists_cmp;
      hlbsa->sizeof_node = sizeof (HandlerList);
      hlbsa->flags = G_BSEARCH_DEFER_SHRINK;
      hlbsa->n_nodes = 0;
      hlbsa->nodes = NULL;
      g_hash_table_insert (g_handler_list_bsa_ht, instance, hlbsa);
    }
  key.signal_id = signal_id;
  key.handlers = NULL;
  
  return g_bsearch_array_insert (hlbsa, &key, FALSE);
}

static inline HandlerList*
handler_list_lookup (guint    signal_id,
		     gpointer instance)
{
  GBSearchArray *hlbsa = g_hash_table_lookup (g_handler_list_bsa_ht, instance);
  HandlerList key;
  
  key.signal_id = signal_id;
  
  return hlbsa ? g_bsearch_array_lookup (hlbsa, &key) : NULL;
}

static Handler*
handler_lookup (gpointer instance,
		guint    handler_id,
		guint   *signal_id_p)
{
  GBSearchArray *hlbsa = g_hash_table_lookup (g_handler_list_bsa_ht, instance);
  
  if (hlbsa)
    {
      guint i;
      
      for (i = 0; i < hlbsa->n_nodes; i++)
        {
          HandlerList *hlist = g_bsearch_array_get_nth (hlbsa, i);
          Handler *handler;
          
          for (handler = hlist->handlers; handler; handler = handler->next)
            if (handler->id == handler_id)
              {
                if (signal_id_p)
                  *signal_id_p = hlist->signal_id;
		
                return handler;
              }
        }
    }
  
  return NULL;
}

static Handler*
handler_find (gpointer         instance,
	      GSignalMatchType mask,
	      guint            signal_id,
	      GClosure        *closure,
	      gpointer         func,
	      gpointer         data)
{
  if (mask & G_SIGNAL_MATCH_ID)
    {
      HandlerList *hlist = handler_list_lookup (signal_id, instance);
      Handler *handler;
      SignalNode *node;
      
      if (mask & G_SIGNAL_MATCH_FUNC)
	{
	  node = LOOKUP_SIGNAL_NODE (signal_id);
	  if (!node || !node->c_marshaller)
	    return NULL;
	}
	  
      mask = ~mask;
      for (handler = hlist ? hlist->handlers : NULL; handler; handler = handler->next)
        if (((mask & G_SIGNAL_MATCH_CLOSURE) || handler->closure == closure) &&
	    ((mask & G_SIGNAL_MATCH_UNBLOCKED) || handler->block_count == 0) &&
            ((mask & G_SIGNAL_MATCH_DATA) || handler->closure->data == data) &&
	    ((mask & G_SIGNAL_MATCH_FUNC) || (handler->closure->marshal == node->c_marshaller &&
					      handler->closure->meta_marshal == 0 &&
					      ((GCClosure*) handler->closure)->callback == func)))
          return handler;
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
              HandlerList *hlist = g_bsearch_array_get_nth (hlbsa, i);
	      SignalNode *node;
              Handler *handler;
              
	      if (!(mask & G_SIGNAL_MATCH_FUNC))
		{
		  node = LOOKUP_SIGNAL_NODE (hlist->signal_id);
		  if (!node->c_marshaller)
		    continue;
		}

              for (handler = hlist->handlers; handler; handler = handler->next)
                if (((mask & G_SIGNAL_MATCH_CLOSURE) || handler->closure == closure) &&
		    ((mask & G_SIGNAL_MATCH_UNBLOCKED) || handler->block_count == 0) &&
                    ((mask & G_SIGNAL_MATCH_DATA) || handler->closure->data == data) &&
		    ((mask & G_SIGNAL_MATCH_FUNC) || (handler->closure->marshal == node->c_marshaller &&
						      handler->closure->meta_marshal == 0 &&
						      ((GCClosure*) handler->closure)->callback == func)))
                  return handler;
            }
        }
    }
  
  return NULL;
}

static inline Handler*
handler_new (gboolean after)
{
  static guint handler_id = 1;
  Handler *handler = g_generic_node_alloc (&g_handler_ts,
                                           sizeof (Handler),
                                           HANDLER_PRE_ALLOC);
#ifndef G_DISABLE_CHECKS
  if (handler_id == 0)
    g_error (G_STRLOC ": handler id overflow, %s", REPORT_BUG);
#endif
  
  handler->id = handler_id++;
  handler->prev = NULL;
  handler->next = NULL;
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
      G_UNLOCK (g_signal_mutex);
      g_closure_unref (handler->closure);
      G_LOCK (g_signal_mutex);
      g_generic_node_free (&g_handler_ts, handler);
    }
}

static void
handler_insert (guint    signal_id,
		gpointer instance,
		Handler  *handler)
{
  HandlerList *hlist;
  
  g_assert (handler->prev == NULL && handler->next == NULL); // FIXME: paranoid
  
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
emission_push (Emission     **emission_list_p,
	       guint          signal_id,
	       gpointer       instance,
	       EmissionState *state_p)
{
  Emission *emission = g_generic_node_alloc (&g_emission_ts,
                                             sizeof (Emission),
                                             EMISSION_PRE_ALLOC);
  emission->next = *emission_list_p;
  emission->signal_id = signal_id;
  emission->instance = instance;
  emission->state_p = state_p;
  *emission_list_p = emission;
}

static inline void
emission_pop (Emission **emission_list_p)
{
  Emission *emission = *emission_list_p;
  
  *emission_list_p = emission->next;
  g_generic_node_free (&g_emission_ts, emission);
}

static inline Emission*
emission_find (Emission *emission_list,
	       guint     signal_id,
	       gpointer  instance)
{
  Emission *emission;
  
  for (emission = emission_list; emission; emission = emission->next)
    if (emission->instance == instance && emission->signal_id == signal_id)
      return emission;
  return NULL;
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
  G_LOCK (g_signal_mutex);
  if (!g_n_signal_nodes)
    {
      /* setup signal key array */
      g_signal_key_bsa.cmp_func = signal_key_cmp;
      g_signal_key_bsa.sizeof_node = sizeof (SignalKey);
      g_signal_key_bsa.flags = 0; /* alloc-only */
      
      /* setup handler list binary searchable array hash table (in german, that'd be one word ;) */
      g_handler_list_bsa_ht = g_hash_table_new (g_direct_hash, NULL);
      
      /* invalid (0) signal_id */
      g_n_signal_nodes = 1;
      g_signal_nodes = g_renew (SignalNode*, g_signal_nodes, g_n_signal_nodes);
      g_signal_nodes[0] = NULL;
    }
  G_UNLOCK (g_signal_mutex);
}

void
g_signals_destroy (GType itype)
{
  guint i;
  gboolean found_one = FALSE;
  
  G_LOCK (g_signal_mutex);
  for (i = 0; i < g_n_signal_nodes; i++)
    {
      SignalNode *node = g_signal_nodes[i];
      
      if (node->itype == itype)
        {
          if (node->destroyed)
            g_warning (G_STRLOC ": signal \"%s\" of type `%s' already destroyed",
                       node->name,
                       g_type_name (node->itype));
          else
            {
              found_one = TRUE;
              signal_destroy_R (node);
            }
        }
    }
  if (!found_one)
    g_warning (G_STRLOC ": type `%s' has no signals that could be destroyed",
               g_type_name (itype));
  G_UNLOCK (g_signal_mutex);
}

void
g_signal_stop_emission (gpointer instance,
                        guint    signal_id)
{
  SignalNode *node;
  
  g_return_if_fail (G_TYPE_CHECK_INSTANCE (instance));
  g_return_if_fail (signal_id > 0);
  
  G_LOCK (g_signal_mutex);
  node = LOOKUP_SIGNAL_NODE (signal_id);
  if (node && g_type_conforms_to (G_TYPE_FROM_INSTANCE (instance), node->itype))
    {
      Emission *emission_list = node->flags & G_SIGNAL_NO_RECURSE ? g_restart_emissions : g_recursive_emissions;
      Emission *emission = emission_find (emission_list, signal_id, instance);
      
      if (emission)
        {
          if (*emission->state_p == EMISSION_HOOK)
            g_warning (G_STRLOC ": emission of signal \"%s\" for instance `%p' cannot be stopped from emission hook",
                       node->name, instance);
          else if (*emission->state_p == EMISSION_RUN)
            *emission->state_p = EMISSION_STOP;
        }
      else
        g_warning (G_STRLOC ": no emission of signal \"%s\" to stop for instance `%p'",
                   node->name, instance);
    }
  else
    g_warning ("%s: signal id `%u' is invalid for instance `%p'", G_STRLOC, signal_id, instance);
  G_UNLOCK (g_signal_mutex);
}

guint
g_signal_lookup (const gchar *name,
                 GType        itype)
{
  GQuark quark;

  g_return_val_if_fail (name != NULL, 0);
  g_return_val_if_fail (G_TYPE_IS_INSTANTIATABLE (itype) || G_TYPE_IS_INTERFACE (itype), 0);
  
  G_LOCK (g_signal_mutex);
  quark = g_quark_try_string (name);
  if (quark)
    do
      {
	guint signal_id = signal_id_lookup (quark, itype);

	if (signal_id)
	  return signal_id;

	itype = g_type_parent (itype);
      }
    while (itype);
  G_UNLOCK (g_signal_mutex);
  
  return 0;
}

gchar*
g_signal_name (guint signal_id)
{
  SignalNode *node;
  gchar *name;

  G_LOCK (g_signal_mutex);
  node = LOOKUP_SIGNAL_NODE (signal_id);
  name = node ? node->name : NULL;
  G_UNLOCK (g_signal_mutex);
  
  return name;
}

void
g_signal_query (guint         signal_id,
		GSignalQuery *query)
{
  SignalNode *node;

  g_return_if_fail (query != NULL);

  G_LOCK (g_signal_mutex);
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
  G_UNLOCK (g_signal_mutex);
}

guint
g_signal_newv (const gchar       *signal_name,
               GType              itype,
               GSignalType        signal_flags,
               GClosure          *class_closure,
               GSignalAccumulator accumulator,
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
  if (return_type != G_TYPE_NONE)
    g_return_val_if_fail (accumulator == NULL, 0);
  
  name = g_strdup (signal_name);
  g_strdelimit (name, G_STR_DELIMITERS ":^", '_');  // FIXME do character checks like for types
  
  G_LOCK (g_signal_mutex);
  
  signal_id = g_signal_lookup (name, itype);
  node = LOOKUP_SIGNAL_NODE (signal_id);
  if (node && !node->destroyed)
    {
      g_warning (G_STRLOC ": signal \"%s\" already exists in the `%s' %s",
                 name,
                 g_type_name (node->itype),
                 G_TYPE_IS_INTERFACE (node->itype) ? "interface" : "class ancestry");
      g_free (name);
      G_UNLOCK (g_signal_mutex);
      return 0;
    }
  if (node && node->itype != itype)
    {
      g_warning (G_STRLOC ": signal \"%s\" for type `%s' was previously created for type `%s'",
                 name,
                 g_type_name (itype),
                 g_type_name (node->itype));
      g_free (name);
      G_UNLOCK (g_signal_mutex);
      return 0;
    }
  for (i = 0; i < n_params; i++)
    if (!G_TYPE_IS_VALUE (param_types[i]) ||
	param_types[i] == G_TYPE_ENUM || param_types[i] == G_TYPE_FLAGS) /* FIXME: kludge */
      {
	g_warning (G_STRLOC ": parameter %d of type `%s' for signal \"%s::%s\" is not a value type",
		   i + 1, g_type_name (param_types[i]), g_type_name (itype), name);
	g_free (name);
	G_UNLOCK (g_signal_mutex);
	return 0;
      }
  if (return_type != G_TYPE_NONE && !G_TYPE_IS_VALUE (return_type))
    {
      g_warning (G_STRLOC ": return value of type `%s' for signal \"%s::%s\" is not a value type",
		 g_type_name (param_types[i]), g_type_name (itype), name);
      g_free (name);
      G_UNLOCK (g_signal_mutex);
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
      g_bsearch_array_insert (&g_signal_key_bsa, &key, FALSE);
      g_strdelimit (node->name, "_", '-');
      key.quark = g_quark_from_static_string (node->name);
      g_bsearch_array_insert (&g_signal_key_bsa, &key, FALSE);
    }
  node->destroyed = FALSE;
  
  /* setup reinitializable portion */
  node->flags = signal_flags & (G_SIGNAL_RUN_FIRST |
                                G_SIGNAL_RUN_LAST |
				G_SIGNAL_RUN_CLEANUP |
                                G_SIGNAL_NO_RECURSE |
                                G_SIGNAL_ACTION |
                                G_SIGNAL_NO_HOOKS);
  node->n_params = n_params;
  node->param_types = g_memdup (param_types, sizeof (GType) * n_params);
  node->return_type = return_type;
  node->class_closure = class_closure ? g_closure_ref (class_closure) : NULL;
  node->accumulator = accumulator;
  node->c_marshaller = c_marshaller;
  node->emission_hooks = NULL;
  if (node->c_marshaller && class_closure && G_CLOSURE_NEEDS_MARSHAL (class_closure))
    g_closure_set_marshal (class_closure, node->c_marshaller);
  
  G_UNLOCK (g_signal_mutex);
  return signal_id;
}

static void
signal_destroy_R (SignalNode *signal_node)
{
  SignalNode node = *signal_node;
  
  signal_node->destroyed = TRUE;
  
  /* reentrancy caution, zero out real contents first */
  signal_node->n_params = 0;
  signal_node->param_types = NULL;
  signal_node->return_type = 0;
  signal_node->class_closure = NULL;
  signal_node->accumulator = NULL;
  signal_node->c_marshaller = NULL;
  signal_node->emission_hooks = NULL;
  
#ifndef G_DISABLE_CHECKS
  /* check current emissions */
  {
    Emission *emission;
    
    for (emission = (node.flags & G_SIGNAL_NO_RECURSE) ? g_restart_emissions : g_recursive_emissions;
         emission; emission = emission->next)
      if (emission->signal_id == node.signal_id)
        g_critical (G_STRLOC ": signal \"%s\" being destroyed is currently in emission (instance `%p')",
                    node.name, emission->instance);
  }
#endif
  
  /* free contents that need to
   */
  G_UNLOCK (g_signal_mutex);
  g_free (node.param_types);
  g_closure_unref (node.class_closure);
  if (node.emission_hooks)
    {
      g_hook_list_clear (node.emission_hooks);
      g_free (node.emission_hooks);
    }
  G_LOCK (g_signal_mutex);
}

guint
g_signal_connect_closure (gpointer  instance,
			  guint     signal_id,
			  GClosure *closure,
			  gboolean  after)
{
  SignalNode *node;
  guint handler_id = 0;
  
  g_return_val_if_fail (G_TYPE_CHECK_INSTANCE (instance), 0);
  g_return_val_if_fail (signal_id > 0, 0);
  g_return_val_if_fail (closure != NULL, 0);
  
  G_LOCK (g_signal_mutex);
  node = LOOKUP_SIGNAL_NODE (signal_id);
  if (node && g_type_conforms_to (G_TYPE_FROM_INSTANCE (instance), node->itype))
    {
      Handler *handler = handler_new (after);
      
      handler_id = handler->id;
      handler->closure = g_closure_ref (closure);
      handler_insert (signal_id, instance, handler);
      if (node->c_marshaller && G_CLOSURE_NEEDS_MARSHAL (closure))
	g_closure_set_marshal (closure, node->c_marshaller);
    }
  else
    g_warning ("%s: signal id `%u' is invalid for instance `%p'", G_STRLOC, signal_id, instance);
  G_UNLOCK (g_signal_mutex);
  
  return handler_id;
}

void
g_signal_handler_disconnect (gpointer instance,
                             guint    handler_id)
{
  Handler *handler;
  guint signal_id;
  
  g_return_if_fail (G_TYPE_CHECK_INSTANCE (instance));
  g_return_if_fail (handler_id > 0);
  
  G_LOCK (g_signal_mutex);
  handler = handler_lookup (instance, handler_id, &signal_id);
  if (handler)
    {
      handler->id = 0;
      handler->block_count = 1;
      handler_unref_R (signal_id, instance, handler);
    }
  else
    g_warning ("%s: instance `%p' has no handler with id `%u'", G_STRLOC, instance, handler_id);
  G_UNLOCK (g_signal_mutex);
}

void
g_signal_handlers_destroy (gpointer instance)
{
  GBSearchArray *hlbsa;
  
  g_return_if_fail (G_TYPE_CHECK_INSTANCE (instance));
  
  G_LOCK (g_signal_mutex);
  hlbsa = g_hash_table_lookup (g_handler_list_bsa_ht, instance);
  if (hlbsa)
    {
      guint i;
      
      /* reentrancy caution, delete instance trace first */
      g_hash_table_remove (g_handler_list_bsa_ht, instance);

      for (i = 0; i < hlbsa->n_nodes; i++)
        {
          HandlerList *hlist = g_bsearch_array_get_nth (hlbsa, i);
          Handler *handler = hlist->handlers;
	  
          while (handler)
            {
              Handler *tmp = handler;
	      
              handler = tmp->next;
              tmp->block_count = 1;
              /* cruel unlink, this works because _all_ handlers vanish */
              tmp->next = NULL;
              tmp->prev = tmp;
              if (tmp->id)
		{
		  tmp->id = 0;
		  handler_unref_R (0, NULL, tmp);
		}
            }
        }
      g_free (hlbsa->nodes);
      g_generic_node_free (&g_bsa_ts, hlbsa);
    }
  G_UNLOCK (g_signal_mutex);
}

void
g_signal_handler_block (gpointer instance,
                        guint    handler_id)
{
  Handler *handler;
  
  g_return_if_fail (G_TYPE_CHECK_INSTANCE (instance));
  g_return_if_fail (handler_id > 0);
  
  G_LOCK (g_signal_mutex);
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
    g_warning ("%s: instance `%p' has no handler with id `%u'", G_STRLOC, instance, handler_id);
  G_UNLOCK (g_signal_mutex);
}

void
g_signal_handler_unblock (gpointer instance,
                          guint    handler_id)
{
  Handler *handler;
  
  g_return_if_fail (G_TYPE_CHECK_INSTANCE (instance));
  g_return_if_fail (handler_id > 0);
  
  G_LOCK (g_signal_mutex);
  handler = handler_lookup (instance, handler_id, NULL);
  if (handler)
    {
      if (handler->block_count)
        handler->block_count -= 1;
      else
        g_warning (G_STRLOC ": handler `%u' of instance `%p' is not blocked", handler_id, instance);
    }
  else
    g_warning ("%s: instance `%p' has no handler with id `%u'", G_STRLOC, instance, handler_id);
  G_UNLOCK (g_signal_mutex);
}

guint
g_signal_handler_find (gpointer         instance,
                       GSignalMatchType mask,
                       guint            signal_id,
                       GClosure        *closure,
                       gpointer         func,
                       gpointer         data)
{
  Handler *handler = NULL;
  guint handler_id;
  
  g_return_val_if_fail (G_TYPE_CHECK_INSTANCE (instance), 0);
  
  G_LOCK (g_signal_mutex);
  handler = handler_find (instance, mask, signal_id, closure, func, data);
  handler_id = handler ? handler->id : 0;
  G_UNLOCK (g_signal_mutex);
  
  return handler_id;
}

gboolean
g_signal_handler_pending (gpointer instance,
                          guint    signal_id,
                          gboolean may_be_blocked)
{
  Handler *handler = NULL;
  
  g_return_val_if_fail (G_TYPE_CHECK_INSTANCE (instance), FALSE);
  g_return_val_if_fail (signal_id > 0, FALSE);
  
  G_LOCK (g_signal_mutex);
  handler = handler_find (instance, G_SIGNAL_MATCH_ID, signal_id, NULL, NULL, NULL);
  if (!may_be_blocked)
    for (; handler; handler = handler->next)
      if (!handler->block_count)
        break;
  G_UNLOCK (g_signal_mutex);
  
  return handler != NULL;
}

void
g_signal_emitv (const GValue *instance_and_params,
		guint         signal_id,
		GValue       *return_value)
{
  SignalNode *node;
  gpointer instance;
  const GValue *param_values;
  guint i;
  
  g_return_if_fail (instance_and_params != NULL);
  instance = g_value_get_as_pointer (instance_and_params);
  g_return_if_fail (G_TYPE_CHECK_INSTANCE (instance));
  g_return_if_fail (signal_id > 0);

  param_values = instance_and_params + 1;
  
  G_LOCK (g_signal_mutex);
  node = LOOKUP_SIGNAL_NODE (signal_id);
#ifndef G_DISABLE_CHECKS
  if (!node || !g_type_conforms_to (G_TYPE_FROM_INSTANCE (instance), node->itype))
    g_warning ("%s: signal id `%u' is invalid for instance `%p'", G_STRLOC, signal_id, instance);
  for (i = 0; i < node->n_params; i++)
    if (!G_VALUE_HOLDS (param_values + i, node->param_types[i]))
      {
	g_critical (G_STRLOC ": value for `%s' parameter %u for signal \"%s\" is of type `%s'",
		    g_type_name (node->param_types[i]),
		    i,
		    node->name,
		    G_VALUE_TYPE_NAME (param_values + i));
	G_UNLOCK (g_signal_mutex);
	return;
      }
  if (node->return_type != G_TYPE_NONE)
    {
      if (!return_value)
	{
	  g_critical (G_STRLOC ": return value `%s' for signal \"%s\" is (NULL)",
		      g_type_name (node->return_type),
		      node->name);
	  G_UNLOCK (g_signal_mutex);
	  return;
	}
      else if (!node->accumulator && !G_VALUE_HOLDS (return_value, node->return_type))
	{
	  g_critical (G_STRLOC ": return value `%s' for signal \"%s\" is of type `%s'",
		      g_type_name (node->return_type),
		      node->name,
		      G_VALUE_TYPE_NAME (return_value));
	  G_UNLOCK (g_signal_mutex);
	  return;
	}
    }
  else
    return_value = NULL;
#endif	/* !G_DISABLE_CHECKS */
  
  signal_emit_R (node, instance, return_value, instance_and_params);
  
  G_UNLOCK (g_signal_mutex);
}

static void
signal_emit_R (SignalNode   *node,
	       gpointer      instance,
	       GValue	    *return_value,
	       const GValue *instance_and_params)
{
  EmissionState emission_state = 0;
  GSignalAccumulator accumulator;
  GClosure *class_closure;
  HandlerList *hlist;
  Handler *handlers;
  GValue accu;
  gboolean accu_used = FALSE;
  guint signal_id = node->signal_id;
  
  if (node->flags & G_SIGNAL_NO_RECURSE)
    {
      Emission *emission = emission_find (g_restart_emissions, signal_id, instance);
      
      if (emission)
	{
	  *emission->state_p = EMISSION_RESTART;
	  return;
	}
    }
  accumulator = node->accumulator;
  if (accumulator)
    {
      G_UNLOCK (g_signal_mutex);
      g_value_init (&accu, node->return_type);
      G_LOCK (g_signal_mutex);
    }
  emission_push ((node->flags & G_SIGNAL_NO_RECURSE) ? &g_restart_emissions : &g_recursive_emissions,
		 signal_id, instance, &emission_state);
  class_closure = node->class_closure;
  hlist = handler_list_lookup (signal_id, instance);
  handlers = hlist ? hlist->handlers : NULL;
  if (handlers)
    handler_ref (handlers);
  
 EMIT_RESTART:
  
  if ((node->flags & G_SIGNAL_RUN_FIRST) && class_closure)
    {
      emission_state = EMISSION_RUN;
      
      G_UNLOCK (g_signal_mutex);
      if (accumulator)
	{
	  if (accu_used)
	    g_value_reset (&accu);
	  g_closure_invoke (class_closure,
			    (signal_id << 8) | G_SIGNAL_RUN_FIRST,
			    &accu,
			    node->n_params + 1,
			    instance_and_params);
	  if (!accumulator (signal_id, return_value, &accu) &&
	      emission_state == EMISSION_RUN)
	    emission_state = EMISSION_STOP;
	  accu_used = TRUE;
	}
      else
	g_closure_invoke (class_closure,
			  (signal_id << 8) | G_SIGNAL_RUN_FIRST,
			  return_value,
			  node->n_params + 1,
			  instance_and_params);
      G_LOCK (g_signal_mutex);
      
      if (emission_state == EMISSION_STOP)
	goto EMIT_CLEANUP;
      else if (emission_state == EMISSION_RESTART)
	goto EMIT_RESTART;
    }
  
  if (node->emission_hooks)
    {
      emission_state = EMISSION_HOOK;
      
      G_UNLOCK (g_signal_mutex);
      g_print ("emission_hooks()\n");
      G_LOCK (g_signal_mutex);
      
      if (emission_state == EMISSION_RESTART)
	goto EMIT_RESTART;
    }
  
  if (handlers)
    {
      Handler *handler = handlers;
      
      emission_state = EMISSION_RUN;
      
      handler_ref (handler);
      do
	{
	  Handler *tmp;
	  
	  if (!handler->after && !handler->block_count)
	    {
	      G_UNLOCK (g_signal_mutex);
	      if (accumulator)
		{
		  if (accu_used)
		    g_value_reset (&accu);
		  g_closure_invoke (handler->closure,
				    (signal_id << 8) | G_SIGNAL_RUN_FIRST,
				    &accu,
				    node->n_params + 1,
				    instance_and_params);
		  if (!accumulator (signal_id, return_value, &accu) &&
		      emission_state == EMISSION_RUN)
		    emission_state = EMISSION_STOP;
		  accu_used = TRUE;
		}
	      else
		g_closure_invoke (handler->closure,
				  (signal_id << 8) | G_SIGNAL_RUN_FIRST,
				  return_value,
				  node->n_params + 1,
				  instance_and_params);
	      G_LOCK (g_signal_mutex);
	      
	      tmp = emission_state == EMISSION_RUN ? handler->next : NULL;
	    }
	  else
	    tmp = handler->next;
	  
	  if (tmp)
	    handler_ref (tmp);
	  handler_unref_R (signal_id, instance, handler);
	  handler = tmp;
	}
      while (handler);
      
      if (emission_state == EMISSION_STOP)
	goto EMIT_CLEANUP;
      else if (emission_state == EMISSION_RESTART)
	goto EMIT_RESTART;
    }
  
  if ((node->flags & G_SIGNAL_RUN_LAST) && class_closure)
    {
      emission_state = EMISSION_RUN;
      
      G_UNLOCK (g_signal_mutex);
      if (accumulator)
	{
	  if (accu_used)
	    g_value_reset (&accu);
	  g_closure_invoke (class_closure,
			    (signal_id << 8) | G_SIGNAL_RUN_LAST,
			    &accu,
			    node->n_params + 1,
			    instance_and_params);
          if (!accumulator (signal_id, return_value, &accu) &&
	      emission_state == EMISSION_RUN)
	    emission_state = EMISSION_STOP;
	  accu_used = TRUE;
	}
      else
	g_closure_invoke (class_closure,
			  (signal_id << 8) | G_SIGNAL_RUN_LAST,
			  return_value,
			  node->n_params + 1,
			  instance_and_params);
      G_LOCK (g_signal_mutex);
      
      if (emission_state == EMISSION_STOP)
	goto EMIT_CLEANUP;
      else if (emission_state == EMISSION_RESTART)
	goto EMIT_RESTART;
    }
  
  if (handlers)
    {
      Handler *handler = handlers;
      
      emission_state = EMISSION_RUN;
      
      handler_ref (handler);
      do
	{
	  Handler *tmp;
	  
	  if (handler->after && !handler->block_count)
	    {
	      G_UNLOCK (g_signal_mutex);
              if (accumulator)
		{
		  if (accu_used)
		    g_value_reset (&accu);
		  g_closure_invoke (handler->closure,
				    (signal_id << 8) | G_SIGNAL_RUN_LAST,
				    &accu,
				    node->n_params + 1,
				    instance_and_params);
		  if (!accumulator (signal_id, return_value, &accu) &&
		      emission_state == EMISSION_RUN)
		    emission_state = EMISSION_STOP;
		  accu_used = TRUE;
		}
	      else
		g_closure_invoke (handler->closure,
				  (signal_id << 8) | G_SIGNAL_RUN_LAST,
				  return_value,
				  node->n_params + 1,
				  instance_and_params);
	      G_LOCK (g_signal_mutex);
	      
	      tmp = emission_state == EMISSION_RUN ? handler->next : NULL;
	    }
	  else
	    tmp = handler->next;
	  
	  if (tmp)
	    handler_ref (tmp);
	  handler_unref_R (signal_id, instance, handler);
	  handler = tmp;
	}
      while (handler);
      
      if (emission_state == EMISSION_STOP)
	goto EMIT_CLEANUP;
      else if (emission_state == EMISSION_RESTART)
	goto EMIT_RESTART;
    }
  
 EMIT_CLEANUP:
  
  if ((node->flags & G_SIGNAL_RUN_CLEANUP) && class_closure)
    {
      emission_state = EMISSION_STOP;
      
      G_UNLOCK (g_signal_mutex);
      if (node->return_type != G_TYPE_NONE)
	{
	  if (!accumulator)
	    g_value_init (&accu, node->return_type);
	  else if (accu_used)
	    g_value_reset (&accu);
	  accu_used = TRUE;
	}
      g_closure_invoke (class_closure,
			(signal_id << 8) | G_SIGNAL_RUN_CLEANUP,
			node->return_type != G_TYPE_NONE ? &accu : NULL,
			node->n_params + 1,
			instance_and_params);
      if (node->return_type != G_TYPE_NONE && !accumulator)
	g_value_unset (&accu);
      G_LOCK (g_signal_mutex);

      if (emission_state == EMISSION_RESTART)
	goto EMIT_RESTART;
    }
  
  if (handlers)
    handler_unref_R (signal_id, instance, handlers);
  
  emission_pop ((node->flags & G_SIGNAL_NO_RECURSE) ? &g_restart_emissions : &g_recursive_emissions);
  if (accumulator)
    {
      G_UNLOCK (g_signal_mutex);
      g_value_unset (&accu);
      G_LOCK (g_signal_mutex);
    }
}
