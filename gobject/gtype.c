/* GObject - GLib Type, Object, Parameter and Signal Library
 * Copyright (C) 1998-1999, 2000-2001 Tim Janik and Red Hat, Inc.
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
#include	"gtype.h"

/*
 * MT safe
 */

#include	"gtypeplugin.h"
#include	"gvaluecollector.h"
#include	<string.h>


/* NOTE: some functions (some internal variants and exported ones)
 * invalidate data portions of the TypeNodes. if external functions/callbacks
 * are called, pointers to memory maintained by TypeNodes have to be looked up
 * again. this affects most of the struct TypeNode fields, e.g. ->children or
 * CLASSED_NODE_IFACES_ENTRIES() respectively IFACE_NODE_PREREQUISITES() (but
 * not ->supers[]), as all those memory portions can get realloc()ed during
 * callback invocation.
 *
 * TODO:
 * - g_type_from_name() should do an ordered array lookup after fetching the
 *   the quark, instead of a second hashtable lookup.
 * - speedup checks for virtual types, steal a bit somewhere
 *
 * FIXME:
 * - force interface initialization for already existing classes
 *
 * LOCKING:
 * lock handling issues when calling static functions are indicated by
 * uppercase letter postfixes, all static functions have to have
 * one of the below postfixes:
 * - _I:	[Indifferent about locking]
 *   function doesn't care about locks at all
 * - _U:	[Unlocked invocation]
 *   no read or write lock has to be held across function invocation
 *   (locks may be acquired and released during invocation though)
 * - _L:	[Locked invocation]
 *   a write lock or more than 0 read locks have to be held across
 *   function invocation
 * - _W:	[Write-locked invocation]
 *   a write lock has to be held across function invokation
 * - _Wm:	[Write-locked invocation, mutatable]
 *   like _W, but the write lock might be released and reacquired
 *   during invocation, watch your pointers
 */

static GStaticRWLock            type_rw_lock = G_STATIC_RW_LOCK_INIT;
#define G_READ_LOCK(rw_lock)    g_static_rw_lock_reader_lock (rw_lock)
#define G_READ_UNLOCK(rw_lock)  g_static_rw_lock_reader_unlock (rw_lock)
#define G_WRITE_LOCK(rw_lock)   g_static_rw_lock_writer_lock (rw_lock)
#define G_WRITE_UNLOCK(rw_lock) g_static_rw_lock_writer_unlock (rw_lock)
#define	INVALID_RECURSION(func, arg, type_name) G_STMT_START{ \
    static const gchar *_action = " invalidly modified type "; \
    gpointer _arg = (gpointer) (arg); const gchar *_tname = (type_name), *_fname = (func); \
    if (_arg) \
      g_error ("%s(%p)%s`%s'", _fname, _arg, _action, _tname); \
    else \
      g_error ("%s()%s`%s'", _fname, _action, _tname); \
}G_STMT_END
#define	g_return_val_if_uninitialized(condition, init_function, return_value) G_STMT_START{	\
  if (!(condition))										\
    {												\
      g_log (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL,						\
	     "%s: initialization assertion failed, use %s() prior to this function",		\
	     G_STRLOC, G_STRINGIFY (init_function));						\
      return (return_value);									\
    }												\
}G_STMT_END

#ifdef  G_ENABLE_DEBUG
#define DEBUG_CODE(debug_type, code_block)  G_STMT_START {    \
    if (_g_type_debug_flags & G_TYPE_DEBUG_ ## debug_type) \
      { code_block; }                                     \
} G_STMT_END
#else /* !G_ENABLE_DEBUG */
#define DEBUG_CODE(debug_type, code_block)  /* code_block */
#endif  /* G_ENABLE_DEBUG */

#define TYPE_FUNDAMENTAL_FLAG_MASK (G_TYPE_FLAG_CLASSED | \
				    G_TYPE_FLAG_INSTANTIATABLE | \
				    G_TYPE_FLAG_DERIVABLE | \
				    G_TYPE_FLAG_DEEP_DERIVABLE)
#define	TYPE_FLAG_MASK		   (G_TYPE_FLAG_ABSTRACT | G_TYPE_FLAG_VALUE_ABSTRACT)
#define	SIZEOF_FUNDAMENTAL_INFO	   ((gssize) MAX (MAX (sizeof (GTypeFundamentalInfo), \
						       sizeof (gpointer)), \
                                                  sizeof (glong)))


/* --- typedefs --- */
typedef struct _TypeNode        TypeNode;
typedef struct _CommonData      CommonData;
typedef struct _IFaceData       IFaceData;
typedef struct _ClassData       ClassData;
typedef struct _InstanceData    InstanceData;
typedef union  _TypeData        TypeData;
typedef struct _IFaceEntry      IFaceEntry;
typedef struct _IFaceHolder	IFaceHolder;


/* --- prototypes --- */
static inline GTypeFundamentalInfo*	type_node_fundamental_info_L	(TypeNode		*node);
static	      void			type_add_flags_W		(TypeNode		*node,
									 GTypeFlags		 flags);
static	      void			type_data_make_W		(TypeNode		*node,
									 const GTypeInfo	*info,
									 const GTypeValueTable	*value_table);
static inline void			type_data_ref_Wm		(TypeNode		*node);
static inline void			type_data_unref_Wm		(TypeNode		*node,
									 gboolean		 uncached);
static	      void			type_data_last_unref_Wm		(GType			 type,
									 gboolean		 uncached);
static inline gpointer			type_get_qdata_L		(TypeNode		*node,
									 GQuark			 quark);
static inline void			type_set_qdata_W		(TypeNode		*node,
									 GQuark			 quark,
									 gpointer		 data);


/* --- structures --- */
struct _GValue	/* kludge, keep in sync with gvalue.h */
{
  GType g_type;
};
struct _TypeNode
{
  GTypePlugin *plugin;
  guint        n_children : 12;
  guint        n_supers : 8;
  guint        _prot_n_ifaces_prerequisites : 9;
  guint        is_classed : 1;
  guint        is_instantiatable : 1;
  guint        free_flag : 1;
  GType       *children;
  TypeData    *data;
  GQuark       qname;
  GData       *global_gdata;
  union {
    IFaceEntry  *iface_entries;		/* for !iface types */
    GType       *prerequisistes;
  } _prot;
  GType        supers[1]; /* flexible array */
};
#define SIZEOF_BASE_TYPE_NODE()			(G_STRUCT_OFFSET (TypeNode, supers))
#define MAX_N_SUPERS				(255)
#define MAX_N_CHILDREN				(4095)
#define MAX_N_IFACES				(511)
#define	MAX_N_PREREQUISITES			(MAX_N_IFACES)
#define NODE_TYPE(node)				(node->supers[0])
#define NODE_PARENT_TYPE(node)			(node->supers[1])
#define NODE_NAME(node)				(g_quark_to_string (node->qname))
#define	NODE_IS_IFACE(node)			(G_TYPE_IS_INTERFACE (NODE_TYPE (node)))
#define	CLASSED_NODE_N_IFACES(node)		((node)->_prot_n_ifaces_prerequisites)
#define	CLASSED_NODE_IFACES_ENTRIES(node)	((node)->_prot.iface_entries)
#define	IFACE_NODE_N_PREREQUISITES(node)	((node)->_prot_n_ifaces_prerequisites)
#define	IFACE_NODE_PREREQUISITES(node)		((node)->_prot.prerequisistes)
#define	iface_node_get_holders_L(node)		((IFaceHolder*) type_get_qdata_L ((node), static_quark_iface_holder))
#define	iface_node_set_holders_W(node, holders)	(type_set_qdata_W ((node), static_quark_iface_holder, (holders)))
#define	iface_node_get_dependants_array_L(n)	((GType*) type_get_qdata_L ((n), static_quark_dependants_array))
#define	iface_node_set_dependants_array_W(n,d)	(type_set_qdata_W ((n), static_quark_dependants_array, (d)))

struct _IFaceHolder
{
  GType           instance_type;
  GInterfaceInfo *info;
  GTypePlugin    *plugin;
  IFaceHolder    *next;
};
struct _IFaceEntry
{
  GType           iface_type;
  GTypeInterface *vtable;
};
struct _CommonData
{
  guint             ref_count;
  GTypeValueTable  *value_table;
};
struct _IFaceData
{
  CommonData         common;
  guint16            vtable_size;
  GBaseInitFunc      vtable_init_base;
  GBaseFinalizeFunc  vtable_finalize_base;
};
struct _ClassData
{
  CommonData         common;
  guint16            class_size;
  GBaseInitFunc      class_init_base;
  GBaseFinalizeFunc  class_finalize_base;
  GClassInitFunc     class_init;
  GClassFinalizeFunc class_finalize;
  gconstpointer      class_data;
  gpointer           class;
};
struct _InstanceData
{
  CommonData         common;
  guint16            class_size;
  GBaseInitFunc      class_init_base;
  GBaseFinalizeFunc  class_finalize_base;
  GClassInitFunc     class_init;
  GClassFinalizeFunc class_finalize;
  gconstpointer      class_data;
  gpointer           class;
  guint16            instance_size;
  guint16            n_preallocs;
  GInstanceInitFunc  instance_init;
  GMemChunk        *mem_chunk;
};
union _TypeData
{
  CommonData         common;
  IFaceData          iface;
  ClassData          class;
  InstanceData       instance;
};
typedef struct {
  gpointer            cache_data;
  GTypeClassCacheFunc cache_func;
} ClassCacheFunc;


/* --- variables --- */
static guint           static_n_class_cache_funcs = 0;
static ClassCacheFunc *static_class_cache_funcs = NULL;
static GType           static_last_fundamental_id = 0;
static GQuark          static_quark_type_flags = 0;
static GQuark          static_quark_iface_holder = 0;
static GQuark          static_quark_dependants_array = 0;
GTypeDebugFlags	       _g_type_debug_flags = 0;


/* --- externs --- */
const char  *g_log_domain_gruntime = "GRuntime";


/* --- type nodes --- */
static GHashTable       *static_type_nodes_ht = NULL;
static GType            *static_branch_seqnos = NULL;
static TypeNode       ***static_type_nodes = NULL;

static inline TypeNode*
lookup_type_node_L (register GType utype)
{
  register GType ftype = G_TYPE_FUNDAMENTAL (utype);
  register GType b_seqno = G_TYPE_BRANCH_SEQNO (utype);
  
  if (ftype < static_last_fundamental_id && b_seqno < static_branch_seqnos[ftype])
    return static_type_nodes[ftype][b_seqno];
  else
    return NULL;
}

static TypeNode*
type_node_any_new_W (TypeNode             *pnode,
		     GType                 ftype,
		     const gchar          *name,
		     GTypePlugin          *plugin,
		     GTypeFundamentalFlags type_flags)
{
  guint branch_last, n_supers;
  GType type;
  TypeNode *node;
  guint i, node_size = 0;
  
  n_supers = pnode ? pnode->n_supers + 1 : 0;
  branch_last = static_branch_seqnos[ftype]++;
  type = G_TYPE_DERIVE_ID (ftype, branch_last);
  g_assert ((type & G_TYPE_FLAG_RESERVED_ID_BIT) == 0);
  if (!branch_last || g_bit_storage (branch_last - 1) < g_bit_storage (static_branch_seqnos[ftype] - 1))
    static_type_nodes[ftype] = g_renew (TypeNode*, static_type_nodes[ftype], 1 << g_bit_storage (static_branch_seqnos[ftype] - 1));
  
  if (!pnode)
    node_size += SIZEOF_FUNDAMENTAL_INFO;	      /* fundamental type info */
  node_size += SIZEOF_BASE_TYPE_NODE ();	      /* TypeNode structure */
  node_size += (sizeof (GType) * (1 + n_supers + 1)); /* self + ancestors + 0 for ->supers[] */
  node = g_malloc0 (node_size);
  if (!pnode)					      /* offset fundamental types */
    node = G_STRUCT_MEMBER_P (node, SIZEOF_FUNDAMENTAL_INFO);
  static_type_nodes[ftype][branch_last] = node;
  
  node->n_supers = n_supers;
  if (!pnode)
    {
      node->supers[0] = type;
      node->supers[1] = 0;
      
      node->is_classed = (type_flags & G_TYPE_FLAG_CLASSED) != 0;
      node->is_instantiatable = (type_flags & G_TYPE_FLAG_INSTANTIATABLE) != 0;
      
      if (NODE_IS_IFACE (node))
	{
          IFACE_NODE_N_PREREQUISITES (node) = 0;
	  IFACE_NODE_PREREQUISITES (node) = NULL;
	}
      else
	{
	  CLASSED_NODE_N_IFACES (node) = 0;
	  CLASSED_NODE_IFACES_ENTRIES (node) = NULL;
	}
    }
  else
    {
      node->supers[0] = type;
      memcpy (node->supers + 1, pnode->supers, sizeof (GType) * (1 + pnode->n_supers + 1));
      
      node->is_classed = pnode->is_classed;
      node->is_instantiatable = pnode->is_instantiatable;
      
      if (NODE_IS_IFACE (node))
	{
	  IFACE_NODE_N_PREREQUISITES (node) = 0;
	  IFACE_NODE_PREREQUISITES (node) = NULL;
	}
      else
	{
	  CLASSED_NODE_N_IFACES (node) = CLASSED_NODE_N_IFACES (pnode);
	  CLASSED_NODE_IFACES_ENTRIES (node) = g_memdup (CLASSED_NODE_IFACES_ENTRIES (pnode),
							 sizeof (CLASSED_NODE_IFACES_ENTRIES (pnode)[0]) *
							 CLASSED_NODE_N_IFACES (node));
	}
      
      i = pnode->n_children++;
      pnode->children = g_renew (GType, pnode->children, pnode->n_children);
      pnode->children[i] = type;
    }
  
  node->plugin = plugin;
  node->n_children = 0;
  node->children = NULL;
  node->data = NULL;
  node->qname = g_quark_from_string (name);
  node->global_gdata = NULL;
  
  g_hash_table_insert (static_type_nodes_ht,
		       GUINT_TO_POINTER (node->qname),
		       GUINT_TO_POINTER (type));
  return node;
}

static inline GTypeFundamentalInfo*
type_node_fundamental_info_L (TypeNode *node)
{
  GType ftype = G_TYPE_FUNDAMENTAL (NODE_TYPE (node));
  
  if (ftype != NODE_TYPE (node))
    node = lookup_type_node_L (ftype);
  
  return node ? G_STRUCT_MEMBER_P (node, - SIZEOF_FUNDAMENTAL_INFO) : NULL;
}

static TypeNode*
type_node_fundamental_new_W (GType                 ftype,
			     const gchar          *name,
			     GTypeFundamentalFlags type_flags)
{
  GTypeFundamentalInfo *finfo;
  TypeNode *node;
  guint i, flast;
  
  flast = static_last_fundamental_id;
  
  g_assert (ftype == G_TYPE_FUNDAMENTAL (ftype));
  
  type_flags &= TYPE_FUNDAMENTAL_FLAG_MASK;
  
  static_last_fundamental_id = MAX (static_last_fundamental_id, ftype + 1);
  if (static_last_fundamental_id > flast)
    {
      static_type_nodes = g_renew (TypeNode**, static_type_nodes, static_last_fundamental_id);
      static_branch_seqnos = g_renew (GType, static_branch_seqnos, static_last_fundamental_id);
      for (i = flast; i < static_last_fundamental_id; i++)
	{
	  static_type_nodes[i] = NULL;
	  static_branch_seqnos[i] = 0;
	}
    }
  g_assert (static_branch_seqnos[ftype] == 0);
  
  node = type_node_any_new_W (NULL, ftype, name, NULL, type_flags);
  
  finfo = type_node_fundamental_info_L (node);
  finfo->type_flags = type_flags;
  
  return node;
}

static TypeNode*
type_node_new_W (TypeNode    *pnode,
		 const gchar *name,
		 GTypePlugin *plugin)
     
{
  g_assert (pnode);
  g_assert (pnode->n_supers < MAX_N_SUPERS);
  g_assert (pnode->n_children < MAX_N_CHILDREN);
  
  return type_node_any_new_W (pnode, G_TYPE_FUNDAMENTAL (NODE_TYPE (pnode)), name, plugin, 0);
}

static inline IFaceEntry*
type_lookup_iface_entry_L (TypeNode *node,
			   TypeNode *iface_node)
{
  if (NODE_IS_IFACE (iface_node) && CLASSED_NODE_N_IFACES (node))
    {
      IFaceEntry *ifaces = CLASSED_NODE_IFACES_ENTRIES (node) - 1;
      guint n_ifaces = CLASSED_NODE_N_IFACES (node);
      GType iface_type = NODE_TYPE (iface_node);
      
      do
	{
	  guint i;
	  IFaceEntry *check;
	  
	  i = (n_ifaces + 1) >> 1;
	  check = ifaces + i;
	  if (iface_type == check->iface_type)
	    return check;
	  else if (iface_type > check->iface_type)
	    {
	      n_ifaces -= i;
	      ifaces = check;
	    }
	  else /* if (iface_type < check->iface_type) */
	    n_ifaces = i - 1;
	}
      while (n_ifaces);
    }
  
  return NULL;
}

static inline gboolean
type_lookup_prerequisite (TypeNode *iface,
			  GType     prerequisite_type)
{
  if (NODE_IS_IFACE (iface) && IFACE_NODE_N_PREREQUISITES (iface))
    {
      GType *prerequisites = IFACE_NODE_PREREQUISITES (iface) - 1;
      guint n_prerequisites = IFACE_NODE_N_PREREQUISITES (iface);

      do
	{
	  guint i;
	  GType *check;
	  
	  i = (n_prerequisites + 1) >> 1;
	  check = prerequisites + i;
	  if (prerequisite_type == *check)
	    return TRUE;
	  else if (prerequisite_type > *check)
	    {
	      n_prerequisites -= i;
	      prerequisites = check;
	    }
	  else /* if (prerequisite_type < *check) */
	    n_prerequisites = i - 1;
	}
      while (n_prerequisites);
    }
  return FALSE;
}

static inline gchar*
type_descriptive_name_L (GType type)
{
  if (type)
    {
      TypeNode *node = lookup_type_node_L (type);
      
      return node ? NODE_NAME (node) : "<unknown>";
    }
  else
    return "<invalid>";
}

static inline gchar*
type_descriptive_name_U (GType type)
{
  const gchar *name;
  
  G_READ_LOCK (&type_rw_lock);
  name = type_descriptive_name_L (type);
  G_READ_UNLOCK (&type_rw_lock);
  
  return (gchar *)name;
}


/* --- type consistency checks --- */
static gboolean
check_plugin_U (GTypePlugin *plugin,
		gboolean     need_complete_type_info,
		gboolean     need_complete_interface_info,
		const gchar *type_name)
{
  /* G_IS_TYPE_PLUGIN() and G_TYPE_PLUGIN_GET_CLASS() are external calls: _U 
   */
  if (!plugin)
    {
      g_warning ("plugin handle for type `%s' is NULL",
		 type_name);
      return FALSE;
    }
  if (!G_IS_TYPE_PLUGIN (plugin))
    {
      g_warning ("plugin pointer (%p) for type `%s' is invalid",
		 plugin, type_name);
      return FALSE;
    }
  if (need_complete_type_info && !G_TYPE_PLUGIN_GET_CLASS (plugin)->complete_type_info)
    {
      g_warning ("plugin for type `%s' has no complete_type_info() implementation",
		 type_name);
      return FALSE;
    }
  if (need_complete_interface_info && !G_TYPE_PLUGIN_GET_CLASS (plugin)->complete_interface_info)
    {
      g_warning ("plugin for type `%s' has no complete_interface_info() implementation",
		 type_name);
      return FALSE;
    }
  return TRUE;
}

static gboolean
check_type_name_U (const gchar *type_name)
{
  static const gchar *extra_chars = "-_+";
  const gchar *p = type_name;
  gboolean name_valid;
  
  if (!type_name[0] || !type_name[1] || !type_name[2])
    {
      g_warning ("type name `%s' is too short", type_name);
      return FALSE;
    }
  /* check the first letter */
  name_valid = (p[0] >= 'A' && p[0] <= 'Z') || (p[0] >= 'a' && p[0] <= 'z') || p[0] == '_';
  for (p = type_name + 1; *p; p++)
    name_valid &= ((p[0] >= 'A' && p[0] <= 'Z') ||
		   (p[0] >= 'a' && p[0] <= 'z') ||
		   (p[0] >= '0' && p[0] <= '9') ||
		   strchr (extra_chars, p[0]));
  if (!name_valid)
    {
      g_warning ("type name `%s' contains invalid characters", type_name);
      return FALSE;
    }
  if (g_type_from_name (type_name))
    {
      g_warning ("cannot register existing type `%s'", type_name);
      return FALSE;
    }
  
  return TRUE;
}

static gboolean
check_derivation_U (GType        parent_type,
		    const gchar *type_name)
{
  TypeNode *pnode;
  GTypeFundamentalInfo* finfo;
  
  G_READ_LOCK (&type_rw_lock);
  pnode = lookup_type_node_L (parent_type);
  if (!pnode)
    {
      G_READ_UNLOCK (&type_rw_lock);
      g_warning ("cannot derive type `%s' from invalid parent type `%s'",
		 type_name,
		 type_descriptive_name_U (parent_type));
      return FALSE;
    }
  finfo = type_node_fundamental_info_L (pnode);
  /* ensure flat derivability */
  if (!(finfo->type_flags & G_TYPE_FLAG_DERIVABLE))
    {
      G_READ_UNLOCK (&type_rw_lock);
      g_warning ("cannot derive `%s' from non-derivable parent type `%s'",
		 type_name,
		 NODE_NAME (pnode));
      return FALSE;
    }
  /* ensure deep derivability */
  if (parent_type != G_TYPE_FUNDAMENTAL (parent_type) &&
      !(finfo->type_flags & G_TYPE_FLAG_DEEP_DERIVABLE))
    {
      G_READ_UNLOCK (&type_rw_lock);
      g_warning ("cannot derive `%s' from non-fundamental parent type `%s'",
		 type_name,
		 NODE_NAME (pnode));
      return FALSE;
    }
  G_READ_UNLOCK (&type_rw_lock);
  
  return TRUE;
}

static gboolean
check_collect_format_I (const gchar *collect_format)
{
  const gchar *p = collect_format;
  gchar valid_format[] = { G_VALUE_COLLECT_INT, G_VALUE_COLLECT_LONG,
			   G_VALUE_COLLECT_DOUBLE, G_VALUE_COLLECT_POINTER,
			   0 };

  while (*p)
    if (!strchr (valid_format, *p++))
      return FALSE;
  return p - collect_format <= G_VALUE_COLLECT_FORMAT_MAX_LENGTH;
}

static gboolean
check_value_table_I (const gchar           *type_name,
		     const GTypeValueTable *value_table)
{
  if (!value_table)
    return FALSE;
  else if (value_table->value_init == NULL)
    {
      if (value_table->value_free || value_table->value_copy ||
	  value_table->value_peek_pointer ||
	  value_table->collect_format || value_table->collect_value ||
	  value_table->lcopy_format || value_table->lcopy_value)
	g_warning ("cannot handle uninitializable values of type `%s'",
		   type_name);
      return FALSE;
    }
  else /* value_table->value_init != NULL */
    {
      if (!value_table->value_free)
	{
	  /* +++ optional +++
	   * g_warning ("missing `value_free()' for type `%s'", type_name);
	   * return FALSE;
	   */
	}
      if (!value_table->value_copy)
	{
	  g_warning ("missing `value_copy()' for type `%s'", type_name);
	  return FALSE;
	}
      if ((value_table->collect_format || value_table->collect_value) &&
	  (!value_table->collect_format || !value_table->collect_value))
	{
	  g_warning ("one of `collect_format' and `collect_value()' is unspecified for type `%s'",
		     type_name);
	  return FALSE;
	}
      if (value_table->collect_format && !check_collect_format_I (value_table->collect_format))
	{
	  g_warning ("the `%s' specification for type `%s' is too long or invalid",
		     "collect_format",
		     type_name);
	  return FALSE;
	}
      if ((value_table->lcopy_format || value_table->lcopy_value) &&
	  (!value_table->lcopy_format || !value_table->lcopy_value))
	{
	  g_warning ("one of `lcopy_format' and `lcopy_value()' is unspecified for type `%s'",
		     type_name);
	  return FALSE;
	}
      if (value_table->lcopy_format && !check_collect_format_I (value_table->lcopy_format))
	{
	  g_warning ("the `%s' specification for type `%s' is too long or invalid",
		     "lcopy_format",
		     type_name);
	  return FALSE;
	}
    }
  return TRUE;
}

static gboolean
check_type_info_L (TypeNode        *pnode,
		   GType            ftype,
		   const gchar     *type_name,
		   const GTypeInfo *info)
{
  GTypeFundamentalInfo *finfo = type_node_fundamental_info_L (lookup_type_node_L (ftype));
  gboolean is_interface = G_TYPE_IS_INTERFACE (ftype);
  
  /* check instance members */
  if (!(finfo->type_flags & G_TYPE_FLAG_INSTANTIATABLE) &&
      (info->instance_size || info->n_preallocs || info->instance_init))
    {
      if (pnode)
	g_warning ("cannot instantiate `%s', derived from non-instantiatable parent type `%s'",
		   type_name,
		   NODE_NAME (pnode));
      else
	g_warning ("cannot instantiate `%s' as non-instantiatable fundamental",
		   type_name);
      return FALSE;
    }
  /* check class & interface members */
  if (!(finfo->type_flags & G_TYPE_FLAG_CLASSED) &&
      (info->class_init || info->class_finalize || info->class_data ||
       (!is_interface && (info->class_size || info->base_init || info->base_finalize))))
    {
      if (pnode)
	g_warning ("cannot create class for `%s', derived from non-classed parent type `%s'",
		   type_name,
                   NODE_NAME (pnode));
      else
	g_warning ("cannot create class for `%s' as non-classed fundamental",
		   type_name);
      return FALSE;
    }
  /* check interface size */
  if (is_interface && info->class_size < sizeof (GTypeInterface))
    {
      g_warning ("specified interface size for type `%s' is smaller than `GTypeInterface' size",
		 type_name);
      return FALSE;
    }
  /* check class size */
  if (finfo->type_flags & G_TYPE_FLAG_CLASSED)
    {
      if (info->class_size < sizeof (GTypeClass))
	{
	  g_warning ("specified class size for type `%s' is smaller than `GTypeClass' size",
		     type_name);
	  return FALSE;
	}
      if (pnode && info->class_size < pnode->data->class.class_size)
	{
	  g_warning ("specified class size for type `%s' is smaller "
		     "than the parent type's `%s' class size",
		     type_name,
		     NODE_NAME (pnode));
	  return FALSE;
	}
    }
  /* check instance size */
  if (finfo->type_flags & G_TYPE_FLAG_INSTANTIATABLE)
    {
      if (info->instance_size < sizeof (GTypeInstance))
	{
	  g_warning ("specified instance size for type `%s' is smaller than `GTypeInstance' size",
		     type_name);
	  return FALSE;
	}
      if (pnode && info->instance_size < pnode->data->instance.instance_size)
	{
	  g_warning ("specified instance size for type `%s' is smaller "
		     "than the parent type's `%s' instance size",
		     type_name,
		     NODE_NAME (pnode));
	  return FALSE;
	}
    }
  
  return TRUE;
}

static TypeNode*
find_conforming_type_L (TypeNode *pnode,
			TypeNode *iface)
{
  TypeNode *node = NULL;
  guint i;
  
  if (type_lookup_iface_entry_L (pnode, iface))
    return pnode;
  
  for (i = 0; i < pnode->n_children && !node; i++)
    node = find_conforming_type_L (lookup_type_node_L (pnode->children[i]), iface);
  
  return node;
}

static gboolean
check_add_interface_L (GType instance_type,
		       GType iface_type)
{
  TypeNode *node = lookup_type_node_L (instance_type);
  TypeNode *iface = lookup_type_node_L (iface_type);
  TypeNode *tnode;
  
  if (!node || !node->is_instantiatable)
    {
      g_warning ("cannot add interfaces to invalid (non-instantiatable) type `%s'",
		 type_descriptive_name_L (instance_type));
      return FALSE;
    }
  if (!iface || !NODE_IS_IFACE (iface))
    {
      g_warning ("cannot add invalid (non-interface) type `%s' to type `%s'",
		 type_descriptive_name_L (iface_type),
		 NODE_NAME (node));
      return FALSE;
    }
  tnode = lookup_type_node_L (NODE_PARENT_TYPE (iface));
  if (NODE_PARENT_TYPE (tnode) && !type_lookup_iface_entry_L (node, tnode))
    {
      g_warning ("cannot add sub-interface `%s' to type `%s' which does not conform to super-interface `%s'",
		 NODE_NAME (iface),
		 NODE_NAME (node),
		 NODE_NAME (tnode));
      return FALSE;
    }
  tnode = find_conforming_type_L (node, iface);
  if (tnode)
    {
      g_warning ("cannot add interface type `%s' to type `%s', since type `%s' already conforms to interface",
		 NODE_NAME (iface),
		 NODE_NAME (node),
		 NODE_NAME (tnode));
      return FALSE;
    }
  return TRUE;
}

static gboolean
check_interface_info_L (TypeNode             *iface,
			GType                 instance_type,
			const GInterfaceInfo *info)
{
  if ((info->interface_finalize || info->interface_data) && !info->interface_init)
    {
      g_warning ("interface type `%s' for type `%s' comes without initializer",
		 NODE_NAME (iface),
		 type_descriptive_name_L (instance_type));
      return FALSE;
    }
  
  return TRUE;
}

/* --- type info (type node data) --- */
static void
type_data_make_W (TypeNode              *node,
		  const GTypeInfo       *info,
		  const GTypeValueTable *value_table)
{
  TypeData *data;
  GTypeValueTable *vtable = NULL;
  guint vtable_size = 0;
  
  g_assert (node->data == NULL && info != NULL);
  
  if (!value_table)
    {
      TypeNode *pnode = lookup_type_node_L (NODE_PARENT_TYPE (node));
      
      if (pnode)
	vtable = pnode->data->common.value_table;
      else
	{
	  static const GTypeValueTable zero_vtable = { NULL, };
	  
	  value_table = &zero_vtable;
	}
    }
  if (value_table)
    {
      /* need to setup vtable_size since we have to allocate it with data in one chunk */
      vtable_size = sizeof (GTypeValueTable);
      if (value_table->collect_format)
	vtable_size += strlen (value_table->collect_format);
      if (value_table->lcopy_format)
	vtable_size += strlen (value_table->lcopy_format);
      vtable_size += 2;
    }
  
  if (node->is_instantiatable) /* carefull, is_instantiatable is also is_classed */
    {
      data = g_malloc0 (sizeof (InstanceData) + vtable_size);
      if (vtable_size)
	vtable = G_STRUCT_MEMBER_P (data, sizeof (InstanceData));
      data->instance.class_size = info->class_size;
      data->instance.class_init_base = info->base_init;
      data->instance.class_finalize_base = info->base_finalize;
      data->instance.class_init = info->class_init;
      data->instance.class_finalize = info->class_finalize;
      data->instance.class_data = info->class_data;
      data->instance.class = NULL;
      data->instance.instance_size = info->instance_size;
#ifdef	DISABLE_MEM_POOLS
      data->instance.n_preallocs = 0;
#else	/* !DISABLE_MEM_POOLS */
      data->instance.n_preallocs = MIN (info->n_preallocs, 1024);
#endif	/* !DISABLE_MEM_POOLS */
      data->instance.instance_init = info->instance_init;
      data->instance.mem_chunk = NULL;
    }
  else if (node->is_classed) /* only classed */
    {
      data = g_malloc0 (sizeof (ClassData) + vtable_size);
      if (vtable_size)
	vtable = G_STRUCT_MEMBER_P (data, sizeof (ClassData));
      data->class.class_size = info->class_size;
      data->class.class_init_base = info->base_init;
      data->class.class_finalize_base = info->base_finalize;
      data->class.class_init = info->class_init;
      data->class.class_finalize = info->class_finalize;
      data->class.class_data = info->class_data;
      data->class.class = NULL;
    }
  else if (NODE_IS_IFACE (node))
    {
      data = g_malloc0 (sizeof (IFaceData) + vtable_size);
      if (vtable_size)
	vtable = G_STRUCT_MEMBER_P (data, sizeof (IFaceData));
      data->iface.vtable_size = info->class_size;
      data->iface.vtable_init_base = info->base_init;
      data->iface.vtable_finalize_base = info->base_finalize;
    }
  else
    {
      data = g_malloc0 (sizeof (CommonData) + vtable_size);
      if (vtable_size)
	vtable = G_STRUCT_MEMBER_P (data, sizeof (CommonData));
    }
  
  node->data = data;
  node->data->common.ref_count = 1;
  
  if (vtable_size)
    {
      gchar *p;

      /* we allocate the vtable and its strings together with the type data, so
       * children can take over their parent's vtable pointer, and we don't
       * need to worry freeing it or not when the child data is destroyed
       */
      *vtable = *value_table;
      p = G_STRUCT_MEMBER_P (vtable, sizeof (*vtable));
      p[0] = 0;
      vtable->collect_format = p;
      if (value_table->collect_format)
	{
	  strcat (p, value_table->collect_format);
	  p += strlen (value_table->collect_format);
	}
      p++;
      p[0] = 0;
      vtable->lcopy_format = p;
      if (value_table->lcopy_format)
	strcat  (p, value_table->lcopy_format);
    }
  node->data->common.value_table = vtable;
  
  g_assert (node->data->common.value_table != NULL); /* paranoid */
}

static inline void
type_data_ref_Wm (TypeNode *node)
{
  if (!node->data)
    {
      TypeNode *pnode = lookup_type_node_L (NODE_PARENT_TYPE (node));
      GTypeInfo tmp_info;
      GTypeValueTable tmp_value_table;
      
      g_assert (node->plugin != NULL);
      
      if (pnode)
	{
	  type_data_ref_Wm (pnode);
	  if (node->data)
	    INVALID_RECURSION ("g_type_plugin_*", node->plugin, NODE_NAME (node));
	}
      
      memset (&tmp_info, 0, sizeof (tmp_info));
      memset (&tmp_value_table, 0, sizeof (tmp_value_table));
      
      G_WRITE_UNLOCK (&type_rw_lock);
      g_type_plugin_use (node->plugin);
      g_type_plugin_complete_type_info (node->plugin, NODE_TYPE (node), &tmp_info, &tmp_value_table);
      G_WRITE_LOCK (&type_rw_lock);
      if (node->data)
	INVALID_RECURSION ("g_type_plugin_*", node->plugin, NODE_NAME (node));
      
      check_type_info_L (pnode, G_TYPE_FUNDAMENTAL (NODE_TYPE (node)), NODE_NAME (node), &tmp_info);
      type_data_make_W (node, &tmp_info,
			check_value_table_I (NODE_NAME (node),
					     &tmp_value_table) ? &tmp_value_table : NULL);
    }
  else
    {
      g_assert (node->data->common.ref_count > 0);
      
      node->data->common.ref_count += 1;
    }
}

static inline void
type_data_unref_Wm (TypeNode *node,
		    gboolean  uncached)
{
  g_assert (node->data && node->data->common.ref_count);
  
  if (node->data->common.ref_count > 1)
    node->data->common.ref_count -= 1;
  else
    {
      if (!node->plugin)
	{
	  g_warning ("static type `%s' unreferenced too often",
		     NODE_NAME (node));
	  return;
	}
      
      type_data_last_unref_Wm (NODE_TYPE (node), uncached);
    }
}

static void
type_node_add_iface_entry_W (TypeNode *node,
			     GType     iface_type)
{
  IFaceEntry *entries;
  guint i;
  
  g_assert (node->is_instantiatable && CLASSED_NODE_N_IFACES (node) < MAX_N_IFACES);
  
  entries = CLASSED_NODE_IFACES_ENTRIES (node);
  for (i = 0; i < CLASSED_NODE_N_IFACES (node); i++)
    if (entries[i].iface_type == iface_type)
      {
	g_warning ("failed to add interface, type `%s' already conforms to interface type `%s'",
		   type_descriptive_name_L (NODE_TYPE (node)),
		   type_descriptive_name_L (iface_type));
	return;
      }
    else if (entries[i].iface_type > iface_type)
      break;
  CLASSED_NODE_N_IFACES (node) += 1;
  CLASSED_NODE_IFACES_ENTRIES (node) = g_renew (IFaceEntry,
						CLASSED_NODE_IFACES_ENTRIES (node),
						CLASSED_NODE_N_IFACES (node));
  entries = CLASSED_NODE_IFACES_ENTRIES (node);
  g_memmove (entries + i + 1, entries + i, sizeof (entries[0]) * (CLASSED_NODE_N_IFACES (node) - i - 1));
  entries[i].iface_type = iface_type;
  entries[i].vtable = NULL;
  
  for (i = 0; i < node->n_children; i++)
    type_node_add_iface_entry_W (lookup_type_node_L (node->children[i]), iface_type);
}

static void
type_add_interface_W (TypeNode             *node,
		      TypeNode             *iface,
		      const GInterfaceInfo *info,
		      GTypePlugin          *plugin)
{
  IFaceHolder *iholder = g_new0 (IFaceHolder, 1);
  
  /* we must not call any functions of GInterfaceInfo from within here, since
   * we got most probably called from _within_ a type registration function
   */
  g_assert (node->is_instantiatable && NODE_IS_IFACE (iface) && ((info && !plugin) || (!info && plugin)));
  
  iholder->next = iface_node_get_holders_L (iface);
  iface_node_set_holders_W (iface, iholder);
  iholder->instance_type = NODE_TYPE (node);
  iholder->info = info ? g_memdup (info, sizeof (*info)) : NULL;
  iholder->plugin = plugin;
  
  type_node_add_iface_entry_W (node, NODE_TYPE (iface));
}

static void
type_iface_add_prerequisite_W (TypeNode *iface,
			       TypeNode *prerequisite_node)
{
  GType prerequisite_type = NODE_TYPE (prerequisite_node);
  GType *prerequisites, *dependants;
  guint n_dependants, i;

  g_assert (NODE_IS_IFACE (iface) &&
	    IFACE_NODE_N_PREREQUISITES (iface) < MAX_N_PREREQUISITES &&
	    (prerequisite_node->is_instantiatable || NODE_IS_IFACE (prerequisite_node)));

  prerequisites = IFACE_NODE_PREREQUISITES (iface);
  for (i = 0; i < IFACE_NODE_N_PREREQUISITES (iface); i++)
    if (prerequisites[i] == prerequisite_type)
      return;			/* we already have that prerequisiste */
    else if (prerequisites[i] > prerequisite_type)
      break;
  IFACE_NODE_N_PREREQUISITES (iface) += 1;
  IFACE_NODE_PREREQUISITES (iface) = g_renew (GType,
					      IFACE_NODE_PREREQUISITES (iface),
					      IFACE_NODE_N_PREREQUISITES (iface));
  prerequisites = IFACE_NODE_PREREQUISITES (iface);
  g_memmove (prerequisites + i + 1, prerequisites + i,
	     sizeof (prerequisites[0]) * (IFACE_NODE_N_PREREQUISITES (iface) - i - 1));
  prerequisites[i] = prerequisite_type;

  /* we want to get notified when prerequisites get added to prerequisite_node */
  if (NODE_IS_IFACE (prerequisite_node))
    {
      dependants = iface_node_get_dependants_array_L (prerequisite_node);
      n_dependants = dependants ? dependants[0] : 0;
      n_dependants += 1;
      dependants = g_renew (GType, dependants, n_dependants + 1);
      dependants[n_dependants] = NODE_TYPE (iface);
      dependants[0] = n_dependants;
      iface_node_set_dependants_array_W (prerequisite_node, dependants);
    }

  /* we need to notify all dependants */
  dependants = iface_node_get_dependants_array_L (iface);
  n_dependants = dependants ? dependants[0] : 0;
  for (i = 1; i <= n_dependants; i++)
    type_iface_add_prerequisite_W (lookup_type_node_L (dependants[i]), prerequisite_node);
}

void
g_type_interface_add_prerequisite (GType interface_type,
				   GType prerequisite_type)
{
  TypeNode *iface, *prerequisite_node;
  IFaceHolder *holders;

  g_return_if_fail (G_TYPE_IS_INTERFACE (interface_type));
  g_return_if_fail (!g_type_is_a (interface_type, prerequisite_type));
  g_return_if_fail (!g_type_is_a (prerequisite_type, interface_type));

  G_WRITE_LOCK (&type_rw_lock);
  iface = lookup_type_node_L (interface_type);
  prerequisite_node = lookup_type_node_L (prerequisite_type);
  if (!iface || !prerequisite_node || !NODE_IS_IFACE (iface))
    {
      g_warning ("interface type `%s' or prerequisite type `%s' invalid",
		 type_descriptive_name_L (interface_type),
		 type_descriptive_name_L (prerequisite_type));
      G_WRITE_UNLOCK (&type_rw_lock);
      return;
    }
  holders = iface_node_get_holders_L (iface);
  if (holders)
    {
      g_warning ("unable to add prerequisite `%s' to interface `%s' which is already in use for `%s'",
		 type_descriptive_name_L (prerequisite_type),
		 type_descriptive_name_L (interface_type),
		 type_descriptive_name_L (holders->instance_type));
      G_WRITE_UNLOCK (&type_rw_lock);
      return;
    }
  if (prerequisite_node->is_instantiatable)
    {
      guint i;
      
      for (i = 0; i < IFACE_NODE_N_PREREQUISITES (iface); i++)
	{
	  TypeNode *prnode = lookup_type_node_L (IFACE_NODE_PREREQUISITES (iface)[i]);

	  if (prnode->is_instantiatable)
	    {
	      g_warning ("adding prerequisite `%s' to interface `%s' conflicts with existing prerequisite `%s'",
			 type_descriptive_name_L (prerequisite_type),
			 type_descriptive_name_L (interface_type),
			 type_descriptive_name_L (NODE_TYPE (prnode)));
	      G_WRITE_UNLOCK (&type_rw_lock);
	      return;
	    }
	}

      for (i = 0; i < prerequisite_node->n_supers + 1; i++)
	type_iface_add_prerequisite_W (iface, lookup_type_node_L (prerequisite_node->supers[i]));
    }
  else if (NODE_IS_IFACE (prerequisite_node))
    {
      GType *dependants;
      guint n_dependants, i;

      dependants = iface_node_get_dependants_array_L (prerequisite_node);
      n_dependants = dependants ? dependants[0] : 0;
      for (i = 1; i <= n_dependants; i++)
	type_iface_add_prerequisite_W (iface, lookup_type_node_L (dependants[i]));
      type_iface_add_prerequisite_W (iface, prerequisite_node);
    }
  else
    g_warning ("prerequisite `%s' for interface `%s' is not instantiatable or interface",
	       type_descriptive_name_L (prerequisite_type),
	       type_descriptive_name_L (interface_type));
  G_WRITE_UNLOCK (&type_rw_lock);
}

static IFaceHolder*
type_iface_retrive_holder_info_Wm (TypeNode *iface,
				   GType     instance_type)
{
  IFaceHolder *iholder = iface_node_get_holders_L (iface);
  
  g_assert (NODE_IS_IFACE (iface));
  
  while (iholder->instance_type != instance_type)
    iholder = iholder->next;
  
  if (!iholder->info)
    {
      GInterfaceInfo tmp_info;
      
      g_assert (iholder->plugin != NULL);
      
      type_data_ref_Wm (iface);
      if (iholder->info)
	INVALID_RECURSION ("g_type_plugin_*", iface->plugin, NODE_NAME (iface));
      
      memset (&tmp_info, 0, sizeof (tmp_info));
      
      G_WRITE_UNLOCK (&type_rw_lock);
      g_type_plugin_use (iholder->plugin);
      g_type_plugin_complete_interface_info (iholder->plugin, instance_type, NODE_TYPE (iface), &tmp_info);
      G_WRITE_LOCK (&type_rw_lock);
      if (iholder->info)
        INVALID_RECURSION ("g_type_plugin_*", iholder->plugin, NODE_NAME (iface));
      
      check_interface_info_L (iface, instance_type, &tmp_info);
      iholder->info = g_memdup (&tmp_info, sizeof (tmp_info));
    }
  
  return iholder;
}

static void
type_iface_blow_holder_info_Wm (TypeNode *iface,
				GType     instance_type)
{
  IFaceHolder *iholder = iface_node_get_holders_L (iface);
  
  g_assert (NODE_IS_IFACE (iface));
  
  while (iholder->instance_type != instance_type)
    iholder = iholder->next;
  
  if (iholder->info && iholder->plugin)
    {
      g_free (iholder->info);
      iholder->info = NULL;
      
      G_WRITE_UNLOCK (&type_rw_lock);
      g_type_plugin_unuse (iholder->plugin);
      G_WRITE_LOCK (&type_rw_lock);
      
      type_data_unref_Wm (iface, FALSE);
    }
}


/* --- type structure creation/destruction --- */
GTypeInstance*
g_type_create_instance (GType type)
{
  TypeNode *node;
  GTypeInstance *instance;
  GTypeClass *class;
  guint i;
  
  G_READ_LOCK (&type_rw_lock);
  node = lookup_type_node_L (type);
  G_READ_UNLOCK (&type_rw_lock);
  if (!node || !node->is_instantiatable)
    {
      g_warning ("cannot create new instance of invalid (non-instantiatable) type `%s'",
		 type_descriptive_name_U (type));
      return NULL;
    }
  /* G_TYPE_IS_ABSTRACT() is an external call: _U */
  if (G_TYPE_IS_ABSTRACT (type))
    {
      g_warning ("cannot create instance of abstract (non-instantiatable) type `%s'",
		 type_descriptive_name_U (type));
      return NULL;
    }
  
  class = g_type_class_ref (type);
  
  if (node->data->instance.n_preallocs)
    {
      G_WRITE_LOCK (&type_rw_lock);
      if (!node->data->instance.mem_chunk)
	node->data->instance.mem_chunk = g_mem_chunk_new (NODE_NAME (node),
							  node->data->instance.instance_size,
							  (node->data->instance.instance_size *
							   node->data->instance.n_preallocs),
							  G_ALLOC_AND_FREE);
      instance = g_chunk_new0 (GTypeInstance, node->data->instance.mem_chunk);
      G_WRITE_UNLOCK (&type_rw_lock);
    }
  else
    instance = g_malloc0 (node->data->instance.instance_size);	/* fine without read lock */
  for (i = node->n_supers; i > 0; i--)
    {
      TypeNode *pnode;
      
      G_READ_LOCK (&type_rw_lock);
      pnode = lookup_type_node_L (node->supers[i]);
      G_READ_UNLOCK (&type_rw_lock);
      
      if (pnode->data->instance.instance_init)
	{
	  instance->g_class = pnode->data->instance.class;
	  pnode->data->instance.instance_init (instance, class);
	}
    }
  instance->g_class = class;
  
  if (node->data->instance.instance_init)
    node->data->instance.instance_init (instance, class);
  
  return instance;
}

void
g_type_free_instance (GTypeInstance *instance)
{
  TypeNode *node;
  GTypeClass *class;
  
  g_return_if_fail (instance != NULL && instance->g_class != NULL);
  
  G_READ_LOCK (&type_rw_lock);
  class = instance->g_class;
  node = lookup_type_node_L (class->g_type);
  if (!node || !node->is_instantiatable || !node->data || node->data->class.class != (gpointer) class)
    {
      g_warning ("cannot free instance of invalid (non-instantiatable) type `%s'",
		 type_descriptive_name_L (class->g_type));
      G_READ_UNLOCK (&type_rw_lock);
      return;
    }
  G_READ_UNLOCK (&type_rw_lock);
  /* G_TYPE_IS_ABSTRACT() is an external call: _U */
  if (G_TYPE_IS_ABSTRACT (NODE_TYPE (node)))
    {
      g_warning ("cannot free instance of abstract (non-instantiatable) type `%s'",
		 NODE_NAME (node));
      return;
    }
  
  instance->g_class = NULL;
  memset (instance, 0xaa, node->data->instance.instance_size);	/* FIXME: debugging hack */
  if (node->data->instance.n_preallocs)
    {
      G_WRITE_LOCK (&type_rw_lock);
      g_chunk_free (instance, node->data->instance.mem_chunk);
      G_WRITE_UNLOCK (&type_rw_lock);
    }
  else
    g_free (instance);
  
  g_type_class_unref (class);
}

static void
type_propagate_iface_vtable_W (TypeNode       *pnode,
			       TypeNode       *iface,
			       GTypeInterface *vtable)
{
  IFaceEntry *entry = type_lookup_iface_entry_L (pnode, iface);
  guint i;
  
  entry->vtable = vtable;
  for (i = 0; i < pnode->n_children; i++)
    {
      TypeNode *node = lookup_type_node_L (pnode->children[i]);
      
      type_propagate_iface_vtable_W (node, iface, vtable);
    }
}

static void
type_iface_vtable_init_Wm (TypeNode *iface,
			   TypeNode *node)
{
  IFaceEntry *entry = type_lookup_iface_entry_L (node, iface);
  IFaceHolder *iholder = type_iface_retrive_holder_info_Wm (iface, NODE_TYPE (node));
  GTypeInterface *vtable;
  
  g_assert (iface->data && entry && entry->vtable == NULL && iholder && iholder->info);
  
  vtable = g_malloc0 (iface->data->iface.vtable_size);
  type_propagate_iface_vtable_W (node, iface, vtable);
  vtable->g_type = NODE_TYPE (iface);
  vtable->g_instance_type = NODE_TYPE (node);
  
  if (iface->data->iface.vtable_init_base || iholder->info->interface_init)
    {
      G_WRITE_UNLOCK (&type_rw_lock);
      if (iface->data->iface.vtable_init_base)
	iface->data->iface.vtable_init_base (vtable);
      if (iholder->info->interface_init)
	iholder->info->interface_init (vtable, iholder->info->interface_data);
      G_WRITE_LOCK (&type_rw_lock);
    }
}

static void
type_iface_vtable_finalize_Wm (TypeNode       *iface,
			       TypeNode       *node,
			       GTypeInterface *vtable)
{
  IFaceEntry *entry = type_lookup_iface_entry_L (node, iface);
  IFaceHolder *iholder = iface_node_get_holders_L (iface);
  
  g_assert (entry && entry->vtable == vtable);
  
  while (iholder->instance_type != NODE_TYPE (node))
    iholder = iholder->next;
  g_assert (iholder && iholder->info);
  
  type_propagate_iface_vtable_W (node, iface, NULL);
  if (iholder->info->interface_finalize || iface->data->iface.vtable_finalize_base)
    {
      G_WRITE_UNLOCK (&type_rw_lock);
      if (iholder->info->interface_finalize)
	iholder->info->interface_finalize (vtable, iholder->info->interface_data);
      if (iface->data->iface.vtable_finalize_base)
	iface->data->iface.vtable_finalize_base (vtable);
      G_WRITE_LOCK (&type_rw_lock);
    }
  vtable->g_type = 0;
  vtable->g_instance_type = 0;
  g_free (vtable);
  
  type_iface_blow_holder_info_Wm (iface, NODE_TYPE (node));
}

static void
type_class_init_Wm (TypeNode   *node,
		    GTypeClass *pclass)
{
  GSList *slist, *init_slist = NULL;
  GTypeClass *class;
  IFaceEntry *entry;
  TypeNode *bnode;
  guint i;
  
  g_assert (node->is_classed && node->data &&
	    node->data->class.class_size &&
	    !node->data->class.class);
  
  class = g_malloc0 (node->data->class.class_size);
  node->data->class.class = class;
  
  if (pclass)
    {
      TypeNode *pnode = lookup_type_node_L (pclass->g_type);
      
      memcpy (class, pclass, pnode->data->class.class_size);
    }
  class->g_type = NODE_TYPE (node);
  
  G_WRITE_UNLOCK (&type_rw_lock);
  
  /* stack all base class initialization functions, so we
   * call them in ascending order.
   */
  G_READ_LOCK (&type_rw_lock);
  for (bnode = node; bnode; bnode = lookup_type_node_L (NODE_PARENT_TYPE (bnode)))
    if (bnode->data->class.class_init_base)
      init_slist = g_slist_prepend (init_slist, (gpointer) bnode->data->class.class_init_base);
  G_READ_UNLOCK (&type_rw_lock);
  for (slist = init_slist; slist; slist = slist->next)
    {
      GBaseInitFunc class_init_base = (GBaseInitFunc) slist->data;
      
      class_init_base (class);
    }
  g_slist_free (init_slist);
  
  if (node->data->class.class_init)
    node->data->class.class_init (class, (gpointer) node->data->class.class_data);
  
  G_WRITE_LOCK (&type_rw_lock);
  
  /* ok, we got the class done, now initialize all interfaces */
  for (entry = NULL, i = 0; i < CLASSED_NODE_N_IFACES (node); i++)
    if (!CLASSED_NODE_IFACES_ENTRIES (node)[i].vtable)
      entry = CLASSED_NODE_IFACES_ENTRIES (node) + i;
  while (entry)
    {
      type_iface_vtable_init_Wm (lookup_type_node_L (entry->iface_type), node);
      
      for (entry = NULL, i = 0; i < CLASSED_NODE_N_IFACES (node); i++)
	if (!CLASSED_NODE_IFACES_ENTRIES (node)[i].vtable)
	  entry = CLASSED_NODE_IFACES_ENTRIES (node) + i;
    }
}

static void
type_data_finalize_class_ifaces_Wm (TypeNode *node)
{
  IFaceEntry *entry;
  guint i;
  
  g_assert (node->is_instantiatable && node->data && node->data->class.class && node->data->common.ref_count == 0);
  
  g_message ("finalizing interfaces for %sClass `%s'",
	     type_descriptive_name_L (G_TYPE_FUNDAMENTAL (NODE_TYPE (node))),
	     type_descriptive_name_L (NODE_TYPE (node)));
  
  for (entry = NULL, i = 0; i < CLASSED_NODE_N_IFACES (node); i++)
    if (CLASSED_NODE_IFACES_ENTRIES (node)[i].vtable &&
	CLASSED_NODE_IFACES_ENTRIES (node)[i].vtable->g_instance_type == NODE_TYPE (node))
      entry = CLASSED_NODE_IFACES_ENTRIES (node) + i;
  while (entry)
    {
      type_iface_vtable_finalize_Wm (lookup_type_node_L (entry->iface_type), node, entry->vtable);
      
      for (entry = NULL, i = 0; i < CLASSED_NODE_N_IFACES (node); i++)
	if (CLASSED_NODE_IFACES_ENTRIES (node)[i].vtable &&
	    CLASSED_NODE_IFACES_ENTRIES (node)[i].vtable->g_instance_type == NODE_TYPE (node))
	  entry = CLASSED_NODE_IFACES_ENTRIES (node) + i;
    }
}

static void
type_data_finalize_class_U (TypeNode  *node,
			    ClassData *cdata)
{
  GTypeClass *class = cdata->class;
  TypeNode *bnode;
  
  g_assert (cdata->class && cdata->common.ref_count == 0);
  
  if (cdata->class_finalize)
    cdata->class_finalize (class, (gpointer) cdata->class_data);
  
  /* call all base class destruction functions in descending order
   */
  if (cdata->class_finalize_base)
    cdata->class_finalize_base (class);
  G_READ_LOCK (&type_rw_lock);
  for (bnode = lookup_type_node_L (NODE_PARENT_TYPE (node)); bnode; bnode = lookup_type_node_L (NODE_PARENT_TYPE (bnode)))
    if (bnode->data->class.class_finalize_base)
      {
	G_READ_UNLOCK (&type_rw_lock);
	bnode->data->class.class_finalize_base (class);
	G_READ_LOCK (&type_rw_lock);
      }
  G_READ_UNLOCK (&type_rw_lock);
  
  class->g_type = 0;
  g_free (cdata->class);
}

static void
type_data_last_unref_Wm (GType    type,
			 gboolean uncached)
{
  TypeNode *node = lookup_type_node_L (type);
  
  g_return_if_fail (node != NULL && node->plugin != NULL);
  
  if (!node->data || node->data->common.ref_count == 0)
    {
      g_warning ("cannot drop last reference to unreferenced type `%s'",
		 type_descriptive_name_U (type));
      return;
    }
  
  if (node->is_classed && node->data && node->data->class.class && static_n_class_cache_funcs)
    {
      guint i;
      
      G_WRITE_UNLOCK (&type_rw_lock);
      G_READ_LOCK (&type_rw_lock);
      for (i = 0; i < static_n_class_cache_funcs; i++)
	{
	  GTypeClassCacheFunc cache_func = static_class_cache_funcs[i].cache_func;
	  gpointer cache_data = static_class_cache_funcs[i].cache_data;
	  gboolean need_break;
	  
	  G_READ_UNLOCK (&type_rw_lock);
	  need_break = cache_func (cache_data, node->data->class.class);
	  G_READ_LOCK (&type_rw_lock);
	  if (!node->data || node->data->common.ref_count == 0)
	    INVALID_RECURSION ("GType class cache function ", cache_func, NODE_NAME (node));
	  if (need_break)
	    break;
	}
      G_READ_UNLOCK (&type_rw_lock);
      G_WRITE_LOCK (&type_rw_lock);
    }
  
  if (node->data->common.ref_count > 1)	/* may have been re-referenced meanwhile */
    node->data->common.ref_count -= 1;
  else
    {
      GType ptype = NODE_PARENT_TYPE (node);
      TypeData *tdata;
      
      node->data->common.ref_count = 0;
      
      if (node->is_instantiatable && node->data->instance.mem_chunk)
	{
	  g_mem_chunk_destroy (node->data->instance.mem_chunk);
	  node->data->instance.mem_chunk = NULL;
	}
      
      tdata = node->data;
      if (node->is_classed && tdata->class.class)
	{
	  if (CLASSED_NODE_N_IFACES (node))
	    type_data_finalize_class_ifaces_Wm (node);
	  node->data = NULL;
	  G_WRITE_UNLOCK (&type_rw_lock);
	  type_data_finalize_class_U (node, &tdata->class);
	  G_WRITE_LOCK (&type_rw_lock);
	}
      else
	node->data = NULL;

      /* freeing tdata->common.value_table and its contents is taking care of
       * by allocating it in one chunk with tdata
       */
      g_free (tdata);
      
      if (ptype)
	type_data_unref_Wm (lookup_type_node_L (ptype), FALSE);
      G_WRITE_UNLOCK (&type_rw_lock);
      g_type_plugin_unuse (node->plugin);
      G_WRITE_LOCK (&type_rw_lock);
    }
}

void
g_type_add_class_cache_func (gpointer            cache_data,
			     GTypeClassCacheFunc cache_func)
{
  guint i;
  
  g_return_if_fail (cache_func != NULL);
  
  G_WRITE_LOCK (&type_rw_lock);
  i = static_n_class_cache_funcs++;
  static_class_cache_funcs = g_renew (ClassCacheFunc, static_class_cache_funcs, static_n_class_cache_funcs);
  static_class_cache_funcs[i].cache_data = cache_data;
  static_class_cache_funcs[i].cache_func = cache_func;
  G_WRITE_UNLOCK (&type_rw_lock);
}

void
g_type_remove_class_cache_func (gpointer            cache_data,
				GTypeClassCacheFunc cache_func)
{
  gboolean found_it = FALSE;
  guint i;
  
  g_return_if_fail (cache_func != NULL);
  
  G_WRITE_LOCK (&type_rw_lock);
  for (i = 0; i < static_n_class_cache_funcs; i++)
    if (static_class_cache_funcs[i].cache_data == cache_data &&
	static_class_cache_funcs[i].cache_func == cache_func)
      {
	static_n_class_cache_funcs--;
	g_memmove (static_class_cache_funcs + i,
		   static_class_cache_funcs + i + 1,
		   sizeof (static_class_cache_funcs[0]) * (static_n_class_cache_funcs - i));
	static_class_cache_funcs = g_renew (ClassCacheFunc, static_class_cache_funcs, static_n_class_cache_funcs);
	found_it = TRUE;
	break;
      }
  G_WRITE_UNLOCK (&type_rw_lock);
  
  if (!found_it)
    g_warning (G_STRLOC ": cannot remove unregistered class cache func %p with data %p",
	       cache_func, cache_data);
}


/* --- type registration --- */
GType
g_type_register_fundamental (GType                       type_id,
			     const gchar                *type_name,
			     const GTypeInfo            *info,
			     const GTypeFundamentalInfo *finfo,
			     GTypeFlags			 flags)
{
  GTypeFundamentalInfo *node_finfo;
  TypeNode *node;
  
  g_return_val_if_uninitialized (static_last_fundamental_id, g_type_init, 0);
  g_return_val_if_fail (type_id > 0, 0);
  g_return_val_if_fail (type_name != NULL, 0);
  g_return_val_if_fail (info != NULL, 0);
  g_return_val_if_fail (finfo != NULL, 0);
  
  if (!check_type_name_U (type_name))
    return 0;
  if (G_TYPE_FUNDAMENTAL (type_id) != type_id)
    {
      g_warning ("cannot register fundamental type `%s' with non-fundamental id (%u)",
		 type_name,
		 type_id);
      return 0;
    }
  if ((finfo->type_flags & G_TYPE_FLAG_INSTANTIATABLE) &&
      !(finfo->type_flags & G_TYPE_FLAG_CLASSED))
    {
      g_warning ("cannot register instantiatable fundamental type `%s' as non-classed",
		 type_name);
      return 0;
    }
  G_WRITE_LOCK (&type_rw_lock);
  if (lookup_type_node_L (type_id))
    {
      G_WRITE_UNLOCK (&type_rw_lock);
      g_warning ("cannot register existing fundamental type `%s' (as `%s')",
		 type_descriptive_name_U (type_id),
		 type_name);
      return 0;
    }
  
  node = type_node_fundamental_new_W (type_id, type_name, finfo->type_flags);
  node_finfo = type_node_fundamental_info_L (node);
  type_add_flags_W (node, flags);
  
  if (check_type_info_L (NULL, G_TYPE_FUNDAMENTAL (NODE_TYPE (node)), type_name, info))
    type_data_make_W (node, info,
		      check_value_table_I (type_name, info->value_table) ? info->value_table : NULL);
  G_WRITE_UNLOCK (&type_rw_lock);
  
  return NODE_TYPE (node);
}

GType
g_type_register_static (GType            parent_type,
			const gchar     *type_name,
			const GTypeInfo *info,
			GTypeFlags	 flags)
{
  TypeNode *pnode, *node;
  GType type = 0;
  
  g_return_val_if_uninitialized (static_last_fundamental_id, g_type_init, 0);
  g_return_val_if_fail (parent_type > 0, 0);
  g_return_val_if_fail (type_name != NULL, 0);
  g_return_val_if_fail (info != NULL, 0);
  
  if (!check_type_name_U (type_name) ||
      !check_derivation_U (parent_type, type_name))
    return 0;
  if (info->class_finalize)
    {
      g_warning ("class finalizer specified for static type `%s'",
		 type_name);
      return 0;
    }
  
  G_WRITE_LOCK (&type_rw_lock);
  pnode = lookup_type_node_L (parent_type);
  type_data_ref_Wm (pnode);
  if (check_type_info_L (pnode, G_TYPE_FUNDAMENTAL (parent_type), type_name, info))
    {
      node = type_node_new_W (pnode, type_name, NULL);
      type_add_flags_W (node, flags);
      type = NODE_TYPE (node);
      type_data_make_W (node, info,
			check_value_table_I (type_name, info->value_table) ? info->value_table : NULL);
    }
  G_WRITE_UNLOCK (&type_rw_lock);
  
  return type;
}

GType
g_type_register_dynamic (GType        parent_type,
			 const gchar *type_name,
			 GTypePlugin *plugin,
			 GTypeFlags   flags)
{
  TypeNode *pnode, *node;
  GType type;
  
  g_return_val_if_uninitialized (static_last_fundamental_id, g_type_init, 0);
  g_return_val_if_fail (parent_type > 0, 0);
  g_return_val_if_fail (type_name != NULL, 0);
  g_return_val_if_fail (plugin != NULL, 0);
  
  if (!check_type_name_U (type_name) ||
      !check_derivation_U (parent_type, type_name) ||
      !check_plugin_U (plugin, TRUE, FALSE, type_name))
    return 0;
  
  G_WRITE_LOCK (&type_rw_lock);
  pnode = lookup_type_node_L (parent_type);
  node = type_node_new_W (pnode, type_name, plugin);
  type_add_flags_W (node, flags);
  type = NODE_TYPE (node);
  G_WRITE_UNLOCK (&type_rw_lock);
  
  return type;
}

void
g_type_add_interface_static (GType                 instance_type,
			     GType                 interface_type,
			     const GInterfaceInfo *info)
{
  /* G_TYPE_IS_INSTANTIATABLE() is an external call: _U */
  g_return_if_fail (G_TYPE_IS_INSTANTIATABLE (instance_type));
  g_return_if_fail (g_type_parent (interface_type) == G_TYPE_INTERFACE);
  
  G_WRITE_LOCK (&type_rw_lock);
  if (check_add_interface_L (instance_type, interface_type))
    {
      TypeNode *node = lookup_type_node_L (instance_type);
      TypeNode *iface = lookup_type_node_L (interface_type);
      
      if (check_interface_info_L (iface, NODE_TYPE (node), info))
	type_add_interface_W (node, iface, info, NULL);
    }
  G_WRITE_UNLOCK (&type_rw_lock);
}

void
g_type_add_interface_dynamic (GType        instance_type,
			      GType        interface_type,
			      GTypePlugin *plugin)
{
  TypeNode *node;
  
  /* G_TYPE_IS_INSTANTIATABLE() is an external call: _U */
  g_return_if_fail (G_TYPE_IS_INSTANTIATABLE (instance_type));
  g_return_if_fail (g_type_parent (interface_type) == G_TYPE_INTERFACE);
  
  G_READ_LOCK (&type_rw_lock);
  node = lookup_type_node_L (instance_type);
  G_READ_UNLOCK (&type_rw_lock);
  if (!check_plugin_U (plugin, FALSE, TRUE, NODE_NAME (node)))
    return;
  
  G_WRITE_LOCK (&type_rw_lock);
  if (check_add_interface_L (instance_type, interface_type))
    {
      TypeNode *iface = lookup_type_node_L (interface_type);
      
      type_add_interface_W (node, iface, NULL, plugin);
    }
  G_WRITE_UNLOCK (&type_rw_lock);
}


/* --- public API functions --- */
gpointer
g_type_class_ref (GType type)
{
  TypeNode *node;
  
  /* optimize for common code path
   */
  G_WRITE_LOCK (&type_rw_lock);
  node = lookup_type_node_L (type);
  if (node && node->is_classed && node->data &&
      node->data->class.class && node->data->common.ref_count > 0)
    {
      type_data_ref_Wm (node);
      G_WRITE_UNLOCK (&type_rw_lock);
      
      return node->data->class.class;
    }
  
  if (!node || !node->is_classed ||
      (node->data && node->data->common.ref_count < 1))
    {
      G_WRITE_UNLOCK (&type_rw_lock);
      g_warning ("cannot retrive class for invalid (unclassed) type `%s'",
		 type_descriptive_name_U (type));
      return NULL;
    }
  
  type_data_ref_Wm (node);
  
  if (!node->data->class.class)
    {
      GType ptype = NODE_PARENT_TYPE (node);
      GTypeClass *pclass = NULL;
      
      if (ptype)
	{
	  G_WRITE_UNLOCK (&type_rw_lock);
	  pclass = g_type_class_ref (ptype);
	  if (node->data->class.class)
            INVALID_RECURSION ("g_type_plugin_*", node->plugin, NODE_NAME (node));
	  G_WRITE_LOCK (&type_rw_lock);
	}
      
      type_class_init_Wm (node, pclass);
    }
  G_WRITE_UNLOCK (&type_rw_lock);
  
  return node->data->class.class;
}

void
g_type_class_unref (gpointer g_class)
{
  TypeNode *node;
  GTypeClass *class = g_class;
  
  g_return_if_fail (g_class != NULL);
  
  G_WRITE_LOCK (&type_rw_lock);
  node = lookup_type_node_L (class->g_type);
  if (node && node->is_classed && node->data &&
      node->data->class.class == class && node->data->common.ref_count > 0)
    type_data_unref_Wm (node, FALSE);
  else
    g_warning ("cannot unreference class of invalid (unclassed) type `%s'",
	       type_descriptive_name_L (class->g_type));
  G_WRITE_UNLOCK (&type_rw_lock);
}

void
g_type_class_unref_uncached (gpointer g_class)
{
  TypeNode *node;
  GTypeClass *class = g_class;
  
  g_return_if_fail (g_class != NULL);
  
  G_WRITE_LOCK (&type_rw_lock);
  node = lookup_type_node_L (class->g_type);
  if (node && node->is_classed && node->data &&
      node->data->class.class == class && node->data->common.ref_count > 0)
    type_data_unref_Wm (node, TRUE);
  else
    g_warning ("cannot unreference class of invalid (unclassed) type `%s'",
	       type_descriptive_name_L (class->g_type));
  G_WRITE_UNLOCK (&type_rw_lock);
}

gpointer
g_type_class_peek (GType type)
{
  TypeNode *node;
  gpointer class;
  
  G_READ_LOCK (&type_rw_lock);
  node = lookup_type_node_L (type);
  if (node && node->is_classed && node->data && node->data->class.class) /* common.ref_count _may_ be 0 */
    class = node->data->class.class;
  else
    class = NULL;
  G_READ_UNLOCK (&type_rw_lock);
  
  return class;
}

gpointer
g_type_class_peek_parent (gpointer g_class)
{
  TypeNode *node;
  gpointer class;
  
  g_return_val_if_fail (g_class != NULL, NULL);
  
  G_READ_LOCK (&type_rw_lock);
  node = lookup_type_node_L (G_TYPE_FROM_CLASS (g_class));
  if (node && node->is_classed && node->data && NODE_PARENT_TYPE (node))
    {
      node = lookup_type_node_L (NODE_PARENT_TYPE (node));
      class = node->data->class.class;
    }
  else
    class = NULL;
  G_READ_UNLOCK (&type_rw_lock);
  
  return class;
}

gpointer
g_type_interface_peek (gpointer instance_class,
		       GType    iface_type)
{
  TypeNode *node;
  TypeNode *iface;
  gpointer vtable = NULL;
  GTypeClass *class = instance_class;
  
  g_return_val_if_fail (instance_class != NULL, NULL);
  
  G_READ_LOCK (&type_rw_lock);
  node = lookup_type_node_L (class->g_type);
  iface = lookup_type_node_L (iface_type);
  if (node && node->is_instantiatable && iface)
    {
      IFaceEntry *entry = type_lookup_iface_entry_L (node, iface);
      
      if (entry && entry->vtable)
	vtable = entry->vtable;
    }
  G_READ_UNLOCK (&type_rw_lock);
  
  return vtable;
}

G_CONST_RETURN gchar*
g_type_name (GType type)
{
  TypeNode *node;
  
  g_return_val_if_uninitialized (static_last_fundamental_id, g_type_init, NULL);

  G_READ_LOCK (&type_rw_lock);
  node = lookup_type_node_L (type);
  G_READ_UNLOCK (&type_rw_lock);
  
  return node ? NODE_NAME (node) : NULL;
}

GQuark
g_type_qname (GType type)
{
  TypeNode *node;
  
  G_READ_LOCK (&type_rw_lock);
  node = lookup_type_node_L (type);
  G_READ_UNLOCK (&type_rw_lock);
  
  return node ? node->qname : 0;
}

GType
g_type_from_name (const gchar *name)
{
  GType type = 0;
  GQuark quark;
  
  g_return_val_if_fail (name != NULL, 0);
  
  quark = g_quark_try_string (name);
  if (quark)
    {
      G_READ_LOCK (&type_rw_lock);
      type = GPOINTER_TO_UINT (g_hash_table_lookup (static_type_nodes_ht, GUINT_TO_POINTER (quark)));
      G_READ_UNLOCK (&type_rw_lock);
    }
  
  return type;
}

GType
g_type_parent (GType type)
{
  TypeNode *node;
  
  G_READ_LOCK (&type_rw_lock);
  node = lookup_type_node_L (type);
  G_READ_UNLOCK (&type_rw_lock);
  
  return node ? NODE_PARENT_TYPE (node) : 0;
}

guint
g_type_depth (GType type)
{
  TypeNode *node;
  
  G_READ_LOCK (&type_rw_lock);
  node = lookup_type_node_L (type);
  G_READ_UNLOCK (&type_rw_lock);
  
  return node ? node->n_supers + 1 : 0;
}

GType
g_type_next_base (GType type,
		  GType base_type)
{
  GType atype = 0;
  TypeNode *node;
  
  G_READ_LOCK (&type_rw_lock);
  node = lookup_type_node_L (type);
  if (node)
    {
      TypeNode *base_node = lookup_type_node_L (base_type);
      
      if (base_node && base_node->n_supers < node->n_supers)
	{
	  guint n = node->n_supers - base_node->n_supers;
	  
	  if (node->supers[n] == base_type)
	    atype = node->supers[n - 1];
	}
    }
  G_READ_UNLOCK (&type_rw_lock);
  
  return atype;
}

static inline gboolean
type_node_is_a_L (TypeNode *node,
		  TypeNode *iface_node,
		  /*        support_inheritance */
		  gboolean  support_interfaces,
		  gboolean  support_prerequisites)
{
  if (support_interfaces &&
      node->is_instantiatable && NODE_IS_IFACE (iface_node) &&
      type_lookup_iface_entry_L (node, iface_node) != NULL)
    return TRUE;
  else if (/* support_inheritance && */
	   iface_node->n_supers <= node->n_supers &&
	   node->supers[node->n_supers - iface_node->n_supers] == NODE_TYPE (iface_node))
    return TRUE;
  else if (support_prerequisites &&
	   NODE_IS_IFACE (node) &&
	   type_lookup_prerequisite (node, NODE_TYPE (iface_node)))
    return TRUE;
  else
    return FALSE;
}

gboolean
g_type_is_a (GType type,
	     GType iface_type)
{
  TypeNode *node, *iface_node;
  gboolean is_a;

  G_READ_LOCK (&type_rw_lock);
  node = lookup_type_node_L (type);
  iface_node = lookup_type_node_L (iface_type);
  is_a = node && iface_node && type_node_is_a_L (node, iface_node, TRUE, TRUE);
  G_READ_UNLOCK (&type_rw_lock);

  return is_a;
}

guint
g_type_fundamental_branch_last (GType type)
{
  GType ftype = G_TYPE_FUNDAMENTAL (type);
  guint last_type;
  
  G_READ_LOCK (&type_rw_lock);
  last_type = ftype < static_last_fundamental_id ? static_branch_seqnos[ftype] : 0;
  G_READ_UNLOCK (&type_rw_lock);
  
  return last_type;
}

GType* /* free result */
g_type_children (GType  type,
		 guint *n_children)
{
  TypeNode *node;
  
  G_READ_LOCK (&type_rw_lock);
  node = lookup_type_node_L (type);
  if (node)
    {
      GType *children = g_new (GType, node->n_children + 1);
      
      memcpy (children, node->children, sizeof (GType) * node->n_children);
      children[node->n_children] = 0;
      
      if (n_children)
	*n_children = node->n_children;
      G_READ_UNLOCK (&type_rw_lock);
      
      return children;
    }
  else
    {
      G_READ_UNLOCK (&type_rw_lock);
      if (n_children)
	*n_children = 0;
      
      return NULL;
    }
}

GType* /* free result */
g_type_interfaces (GType  type,
		   guint *n_interfaces)
{
  TypeNode *node;
  
  G_READ_LOCK (&type_rw_lock);
  node = lookup_type_node_L (type);
  if (node && node->is_instantiatable)
    {
      GType *ifaces = g_new (GType, CLASSED_NODE_N_IFACES (node) + 1);
      guint i;
      
      for (i = 0; i < CLASSED_NODE_N_IFACES (node); i++)
	ifaces[i] = CLASSED_NODE_IFACES_ENTRIES (node)[i].iface_type;
      ifaces[i] = 0;
      
      if (n_interfaces)
	*n_interfaces = CLASSED_NODE_N_IFACES (node);
      G_READ_UNLOCK (&type_rw_lock);
      
      return ifaces;
    }
  else
    {
      G_READ_UNLOCK (&type_rw_lock);
      if (n_interfaces)
	*n_interfaces = 0;
      
      return NULL;
    }
}

typedef struct _QData QData;
struct _GData
{
  guint  n_qdatas;
  QData *qdatas;
};
struct _QData
{
  GQuark   quark;
  gpointer data;
};

static inline gpointer
type_get_qdata_L (TypeNode *node,
		  GQuark    quark)
{
  GData *gdata = node->global_gdata;
  
  if (quark && gdata && gdata->n_qdatas)
    {
      QData *qdatas = gdata->qdatas - 1;
      guint n_qdatas = gdata->n_qdatas;
      
      do
	{
	  guint i;
	  QData *check;
	  
	  i = (n_qdatas + 1) / 2;
	  check = qdatas + i;
	  if (quark == check->quark)
	    return check->data;
	  else if (quark > check->quark)
	    {
	      n_qdatas -= i;
	      qdatas = check;
	    }
	  else /* if (quark < check->quark) */
	    n_qdatas = i - 1;
	}
      while (n_qdatas);
    }
  return NULL;
}

gpointer
g_type_get_qdata (GType  type,
		  GQuark quark)
{
  TypeNode *node;
  gpointer data;
  
  G_READ_LOCK (&type_rw_lock);
  node = lookup_type_node_L (type);
  if (node)
    {
      data = type_get_qdata_L (node, quark);
      G_READ_UNLOCK (&type_rw_lock);
    }
  else
    {
      G_READ_UNLOCK (&type_rw_lock);
      g_return_val_if_fail (node != NULL, NULL);
      data = NULL;
    }
  return data;
}

static inline void
type_set_qdata_W (TypeNode *node,
		  GQuark    quark,
		  gpointer  data)
{
  GData *gdata;
  QData *qdata;
  guint i;
  
  /* setup qdata list if necessary */
  if (!node->global_gdata)
    node->global_gdata = g_new0 (GData, 1);
  gdata = node->global_gdata;
  
  /* try resetting old data */
  qdata = gdata->qdatas;
  for (i = 0; i < gdata->n_qdatas; i++)
    if (qdata[i].quark == quark)
      {
	qdata[i].data = data;
	return;
      }
  
  /* add new entry */
  gdata->n_qdatas++;
  gdata->qdatas = g_renew (QData, gdata->qdatas, gdata->n_qdatas);
  qdata = gdata->qdatas;
  for (i = 0; i < gdata->n_qdatas - 1; i++)
    if (qdata[i].quark > quark)
      break;
  g_memmove (qdata + i + 1, qdata + i, sizeof (qdata[0]) * (gdata->n_qdatas - i - 1));
  qdata[i].quark = quark;
  qdata[i].data = data;
}

void
g_type_set_qdata (GType    type,
		  GQuark   quark,
		  gpointer data)
{
  TypeNode *node;
  
  g_return_if_fail (quark != 0);
  
  G_WRITE_LOCK (&type_rw_lock);
  node = lookup_type_node_L (type);
  if (node)
    {
      type_set_qdata_W (node, quark, data);
      G_WRITE_UNLOCK (&type_rw_lock);
    }
  else
    {
      G_WRITE_UNLOCK (&type_rw_lock);
      g_return_if_fail (node != NULL);
    }
}

static void
type_add_flags_W (TypeNode  *node,
		  GTypeFlags flags)
{
  guint dflags;
  
  g_return_if_fail ((flags & ~TYPE_FLAG_MASK) == 0);
  g_return_if_fail (node != NULL);
  
  if ((flags & TYPE_FLAG_MASK) && node->is_classed && node->data && node->data->class.class)
    g_warning ("tagging type `%s' as abstract after class initialization", NODE_NAME (node));
  dflags = GPOINTER_TO_UINT (type_get_qdata_L (node, static_quark_type_flags));
  dflags |= flags;
  type_set_qdata_W (node, static_quark_type_flags, GUINT_TO_POINTER (dflags));
}

void
g_type_query (GType       type,
	      GTypeQuery *query)
{
  TypeNode *node;

  g_return_if_fail (query != NULL);
  
  G_READ_LOCK (&type_rw_lock);
  node = lookup_type_node_L (type);
  if (node && node->is_classed && !node->plugin && node->data)
    {
      /* type is classed and static, probably even instantiatable */

      query->type = NODE_TYPE (node);
      query->type_name = NODE_NAME (node);
      query->class_size = node->data->class.class_size;
      query->instance_size = node->is_instantiatable ? node->data->instance.instance_size : 0;
    }
  else
    {
      /* node is not static and classed, won't allow query */
      query->type = 0;
    }
  G_READ_UNLOCK (&type_rw_lock);
}


/* --- implementation details --- */
gboolean
g_type_check_flags (GType type,
		    guint flags)
{
  TypeNode *node;
  gboolean result = FALSE;
  
  G_READ_LOCK (&type_rw_lock);
  node = lookup_type_node_L (type);
  if (node)
    {
      guint fflags = flags & TYPE_FUNDAMENTAL_FLAG_MASK;
      guint tflags = flags & TYPE_FLAG_MASK;
      
      if (fflags)
	{
	  GTypeFundamentalInfo *finfo = type_node_fundamental_info_L (node);
	  
	  fflags = (finfo->type_flags & fflags) == fflags;
	}
      else
	fflags = TRUE;
      
      if (tflags)
	tflags = (tflags & GPOINTER_TO_UINT (type_get_qdata_L (node, static_quark_type_flags))) == tflags;
      else
	tflags = TRUE;
      
      result = tflags && fflags;
    }
  G_READ_UNLOCK (&type_rw_lock);
  
  return result;
}

GTypePlugin*
g_type_get_plugin (GType type)
{
  TypeNode *node;
  
  G_READ_LOCK (&type_rw_lock);
  node = lookup_type_node_L (type);
  G_READ_UNLOCK (&type_rw_lock);
  
  return node ? node->plugin : NULL;
}

GTypePlugin*
g_type_interface_get_plugin (GType instance_type,
			     GType interface_type)
{
  TypeNode *node;
  TypeNode *iface;
  
  g_return_val_if_fail (G_TYPE_IS_INTERFACE (interface_type), NULL);
  
  G_READ_LOCK (&type_rw_lock);
  node = lookup_type_node_L (instance_type);  
  iface = lookup_type_node_L (interface_type);
  if (node && iface)
    {
      IFaceHolder *iholder = iface_node_get_holders_L (iface);
      
      while (iholder && iholder->instance_type != instance_type)
	iholder = iholder->next;
      G_READ_UNLOCK (&type_rw_lock);
      
      if (iholder)
	return iholder->plugin;
    }
  else
    G_READ_UNLOCK (&type_rw_lock);
  
  g_return_val_if_fail (node == NULL, NULL);
  g_return_val_if_fail (iface == NULL, NULL);
  
  g_warning (G_STRLOC ": attempt to look up plugin for invalid instance/interface type pair.");
  
  return NULL;
}

GType
g_type_fundamental_last (void)
{
  GType type;
  
  G_READ_LOCK (&type_rw_lock);
  type = static_last_fundamental_id;
  G_READ_UNLOCK (&type_rw_lock);
  
  return type;
}

gboolean
g_type_instance_is_a (GTypeInstance *type_instance,
		      GType          iface_type)
{
  TypeNode *node, *iface;
  gboolean check;

  if (!type_instance || !type_instance->g_class)
    return FALSE;

  G_READ_LOCK (&type_rw_lock);
  node = lookup_type_node_L (type_instance->g_class->g_type);
  iface = lookup_type_node_L (iface_type);
  check = node && node->is_instantiatable && iface && type_node_is_a_L (node, iface, TRUE, FALSE);
  G_READ_UNLOCK (&type_rw_lock);

  return check;
}

gboolean
g_type_class_is_a (GTypeClass *type_class,
		   GType       is_a_type)
{
  TypeNode *node, *iface;
  gboolean check;

  if (!type_class)
    return FALSE;

  G_READ_LOCK (&type_rw_lock);
  node = lookup_type_node_L (type_class->g_type);
  iface = lookup_type_node_L (is_a_type);
  check = node && node->is_classed && iface && type_node_is_a_L (node, iface, FALSE, FALSE);
  G_READ_UNLOCK (&type_rw_lock);

  return check;
}

GTypeInstance*
g_type_check_instance_cast (GTypeInstance *type_instance,
			    GType          iface_type)
{
  if (type_instance)
    {
      if (type_instance->g_class)
	{
	  TypeNode *node, *iface;
	  gboolean is_instantiatable, check;

	  G_READ_LOCK (&type_rw_lock);
	  node = lookup_type_node_L (type_instance->g_class->g_type);
	  is_instantiatable = node && node->is_instantiatable;
	  iface = lookup_type_node_L (iface_type);
	  check = is_instantiatable && iface && type_node_is_a_L (node, iface, TRUE, FALSE);
	  G_READ_UNLOCK (&type_rw_lock);
	  if (check)
	    return type_instance;

	  if (is_instantiatable)
	    g_warning ("invalid cast from `%s' to `%s'",
		       type_descriptive_name_U (type_instance->g_class->g_type),
		       type_descriptive_name_U (iface_type));
	  else
	    g_warning ("invalid uninstantiatable type `%s' in cast to `%s'",
		       type_descriptive_name_U (type_instance->g_class->g_type),
		       type_descriptive_name_U (iface_type));
	}
      else
	g_warning ("invalid unclassed pointer in cast to `%s'",
		   type_descriptive_name_U (iface_type));
    }
  else
    g_warning ("invalid cast from (NULL) pointer to `%s'",
	       type_descriptive_name_U (iface_type));
  
  return type_instance;
}

GTypeClass*
g_type_check_class_cast (GTypeClass *type_class,
			 GType       is_a_type)
{
  if (type_class)
    {
      TypeNode *node, *iface;
      gboolean is_classed, check;
      
      G_READ_LOCK (&type_rw_lock);
      node = lookup_type_node_L (type_class->g_type);
      is_classed = node && node->is_classed;
      iface = lookup_type_node_L (is_a_type);
      check = is_classed && iface && type_node_is_a_L (node, iface, FALSE, FALSE);
      G_READ_UNLOCK (&type_rw_lock);
      if (check)
	return type_class;

      if (is_classed)
	g_warning ("invalid class cast from `%s' to `%s'",
		   type_descriptive_name_U (type_class->g_type),
		   type_descriptive_name_U (is_a_type));
      else
	g_warning ("invalid unclassed type `%s' in class cast to `%s'",
		   type_descriptive_name_U (type_class->g_type),
		   type_descriptive_name_U (is_a_type));
    }
  else
    g_warning ("invalid class cast from (NULL) pointer to `%s'",
	       type_descriptive_name_U (is_a_type));
  return type_class;
}

gboolean
g_type_check_instance (GTypeInstance *type_instance)
{
  /* this function is just here to make the signal system
   * conveniently elaborated on instance checks
   */
  if (type_instance)
    {
      if (type_instance->g_class)
	{
	  TypeNode *node;
	  gboolean is_instantiatable;
	  
	  G_READ_LOCK (&type_rw_lock);
	  node = lookup_type_node_L (type_instance->g_class->g_type);
	  is_instantiatable = node && node->is_instantiatable;
	  G_READ_UNLOCK (&type_rw_lock);
	  if (is_instantiatable)
	    return TRUE;

	  g_warning ("instance of invalid non-instantiatable type `%s'",
		     type_descriptive_name_U (type_instance->g_class->g_type));
	}
      else
	g_warning ("instance with invalid (NULL) class pointer");
    }
  else
    g_warning ("invalid (NULL) pointer instance");

  return FALSE;
}

static inline gboolean
type_check_is_value_type_U (GType type)
{
  GTypeFlags tflags = G_TYPE_FLAG_VALUE_ABSTRACT;
  TypeNode *node;

  G_READ_LOCK (&type_rw_lock);
 restart_check:
  node = lookup_type_node_L (type);
  if (node)
    {
      if (node->data && node->data->common.ref_count > 0 &&
	  node->data->common.value_table->value_init)
	tflags = GPOINTER_TO_UINT (type_get_qdata_L (node, static_quark_type_flags));
      else if (NODE_IS_IFACE (node))
	{
	  guint i;

	  for (i = 0; i < IFACE_NODE_N_PREREQUISITES (node); i++)
	    {
	      GType prtype = IFACE_NODE_PREREQUISITES (node)[i];
	      TypeNode *prnode = lookup_type_node_L (prtype);

	      if (prnode->is_instantiatable)
		{
		  type = prtype;
		  goto restart_check;
		}
	    }
	}
    }
  G_READ_UNLOCK (&type_rw_lock);

  return !(tflags & G_TYPE_FLAG_VALUE_ABSTRACT);
}

gboolean
g_type_check_is_value_type (GType type)
{
  return type_check_is_value_type_U (type);
}

gboolean
g_type_check_value (GValue *value)
{
  return value && type_check_is_value_type_U (value->g_type);
}

gboolean
g_type_check_value_holds (GValue *value,
			  GType   type)
{
  return value && type_check_is_value_type_U (value->g_type) && g_type_is_a (value->g_type, type);
}

GTypeValueTable*
g_type_value_table_peek (GType type)
{
  TypeNode *node;
  GTypeValueTable *vtable;
  
  G_READ_LOCK (&type_rw_lock);
 restart_table_peek:
  node = lookup_type_node_L (type);
  if (!node)
    {
      g_warning ("type id `%u' is invalid", type);
      G_READ_UNLOCK (&type_rw_lock);
      return NULL;
    }
  if (!node->data || node->data->common.ref_count < 1)
    {
      g_warning ("can't peek value table for type `%s' which is not currently referenced",
		 type_descriptive_name_L (type));
      G_READ_UNLOCK (&type_rw_lock);
      return NULL;
    }
  if (node->data->common.value_table->value_init)
    vtable = node->data->common.value_table;
  else if (NODE_IS_IFACE (node))
    {
      guint i;

      for (i = 0; i < IFACE_NODE_N_PREREQUISITES (node); i++)
	{
	  GType prtype = IFACE_NODE_PREREQUISITES (node)[i];
	  TypeNode *prnode = lookup_type_node_L (prtype);

	  if (prnode->is_instantiatable)
	    {
	      type = prtype;
	      goto restart_table_peek;
	    }
	}
      vtable = NULL;
    }
  else
    vtable = NULL;
  G_READ_UNLOCK (&type_rw_lock);
  
  return vtable;
}


/* --- foreign prototypes --- */
extern void	g_value_types_init	(void); /* sync with gvaluetypes.c */
extern void	g_enum_types_init	(void);	/* sync with genums.c */
extern void     g_param_type_init       (void);	/* sync with gparam.c */
extern void     g_boxed_type_init       (void);	/* sync with gboxed.c */
extern void     g_object_type_init      (void);	/* sync with gobject.c */
extern void	g_param_spec_types_init	(void);	/* sync with gparamspecs.c */
extern void	g_value_transforms_init	(void); /* sync with gvaluetransform.c */
extern void	g_signal_init		(void);	/* sync with gsignal.c */


/* --- initialization --- */
void
g_type_init (GTypeDebugFlags debug_flags)
{
  G_LOCK_DEFINE_STATIC (type_init_lock);
  static TypeNode *type0_node = NULL;
  const gchar *env_string;
  GTypeInfo info;
  TypeNode *node;
  GType type;
  
  G_LOCK (type_init_lock);
  
  G_WRITE_LOCK (&type_rw_lock);
  
  if (static_last_fundamental_id)
    {
      G_WRITE_UNLOCK (&type_rw_lock);
      G_UNLOCK (type_init_lock);
      return;
    }

  /* setup GRuntime wide debugging flags */
  _g_type_debug_flags = debug_flags & G_TYPE_DEBUG_MASK;
  env_string = g_getenv ("GRUNTIME_DEBUG");
  if (env_string != NULL)
    {
      static GDebugKey debug_keys[] = {
	{ "objects", G_TYPE_DEBUG_OBJECTS },
	{ "signals", G_TYPE_DEBUG_SIGNALS },
      };

      _g_type_debug_flags |= g_parse_debug_string (env_string,
						   debug_keys,
						   sizeof (debug_keys) / sizeof (debug_keys[0]));
      env_string = NULL;
    }
  
  /* quarks */
  static_quark_type_flags = g_quark_from_static_string ("-g-type-private--GTypeFlags");
  static_quark_iface_holder = g_quark_from_static_string ("-g-type-private--IFaceHolder");
  static_quark_dependants_array = g_quark_from_static_string ("-g-type-private--dependants-array");
  
  /* type qname hash table */
  static_type_nodes_ht = g_hash_table_new (g_direct_hash, g_direct_equal);
  
  /* invalid type G_TYPE_INVALID (0)
   */
  static_last_fundamental_id = 1;
  static_type_nodes = g_renew (TypeNode**, static_type_nodes, static_last_fundamental_id);
  static_type_nodes[0] = &type0_node;
  static_branch_seqnos = g_renew (GType, static_branch_seqnos, static_last_fundamental_id);
  static_branch_seqnos[0] = 1;
  
  /* void type G_TYPE_NONE
   */
  node = type_node_fundamental_new_W (G_TYPE_NONE, "void", 0);
  type = NODE_TYPE (node);
  g_assert (type == G_TYPE_NONE);
  
  /* interface fundamental type G_TYPE_INTERFACE (!classed)
   */
  memset (&info, 0, sizeof (info));
  node = type_node_fundamental_new_W (G_TYPE_INTERFACE, "GInterface", G_TYPE_FLAG_DERIVABLE);
  type = NODE_TYPE (node);
  type_data_make_W (node, &info, NULL);
  g_assert (type == G_TYPE_INTERFACE);
  
  G_WRITE_UNLOCK (&type_rw_lock);
  
  /* G_TYPE_TYPE_PLUGIN
   */
  g_type_plugin_get_type ();
  
  /* G_TYPE_* value types
   */
  g_value_types_init ();
  
  /* G_TYPE_ENUM & G_TYPE_FLAGS
   */
  g_enum_types_init ();
  
  /* G_TYPE_BOXED
   */
  g_boxed_type_init ();
  
  /* G_TYPE_PARAM
   */
  g_param_type_init ();
  
  /* G_TYPE_OBJECT
   */
  g_object_type_init ();
  
  /* G_TYPE_PARAM_* pspec types
   */
  g_param_spec_types_init ();
  
  /* Value Transformations
   */
  g_value_transforms_init ();

  /* Signal system
   */
  g_signal_init ();
  
  G_UNLOCK (type_init_lock);
}
