/* GObject - GLib Type, Object, Parameter and Signal Library
 * Copyright (C) 1998, 1999, 2000 Tim Janik and Red Hat, Inc.
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

#include	<string.h>

#define FIXME_DISABLE_PREALLOCATIONS

/* NOTE: some functions (some internal variants and exported ones)
 * invalidate data portions of the TypeNodes. if external functions/callbacks
 * are called, pointers to memory maintained by TypeNodes have to be looked up
 * again. this affects most of the struct TypeNode fields, e.g. ->children or
 * ->iface_entries (not ->supers[] as of recently), as all those memory portions can
 * get realloc()ed during callback invocation.
 *
 * TODO:
 * - g_type_from_name() should do an ordered array lookup after fetching the
 *   the quark, instead of a second hashtable lookup.
 *
 * FIXME:
 * - force interface initialization for already existing classes
 */

#define G_TYPE_FLAG_MASK	(G_TYPE_FLAG_CLASSED | \
				 G_TYPE_FLAG_INSTANTIATABLE | \
				 G_TYPE_FLAG_DERIVABLE | \
				 G_TYPE_FLAG_DEEP_DERIVABLE)
#define	g_type_plugin_ref(p)				((p)->vtable->plugin_ref (p))
#define g_type_plugin_unref(p)				((p)->vtable->plugin_unref (p))
#define	g_type_plugin_complete_type_info(p,t,i,v)	((p)->vtable->complete_type_info ((p), (t), (i), (v)))
#define	g_type_plugin_complete_interface_info(p,f,t,i)	((p)->vtable->complete_interface_info ((p), (f), (t), (i)))

typedef struct _TypeNode        TypeNode;
typedef struct _CommonData      CommonData;
typedef struct _IFaceData       IFaceData;
typedef struct _ClassData       ClassData;
typedef struct _InstanceData    InstanceData;
typedef union  _TypeData        TypeData;
typedef struct _IFaceEntry      IFaceEntry;
typedef struct _IFaceHolder	IFaceHolder;


/* --- prototypes --- */
static inline GTypeFundamentalInfo*	type_node_fundamental_info	(TypeNode		*node);
static	      void			type_data_make			(TypeNode        	*node,
									 const GTypeInfo	*info,
									 const GTypeValueTable	*value_table);
static inline void			type_data_ref			(TypeNode		*node);
static inline void			type_data_unref			(TypeNode		*node,
									 gboolean                uncached);
static	      void			type_data_last_unref		(GType			 type,
									 gboolean                uncached);


/* --- structures --- */
struct _TypeNode
{
  GTypePlugin *plugin;
  guint        n_children : 12;
  guint        n_supers : 8;
  guint        n_ifaces : 9;
  guint        is_classed : 1;
  guint        is_instantiatable : 1;
  guint        is_iface : 1;
  GType       *children;
  TypeData    *data;
  GQuark       qname;
  GData       *static_gdata;
  union {
    IFaceEntry  *iface_entries;
    IFaceHolder *iface_conformants;
  } private;
  GType        supers[1]; /* flexible array */
};
#define SIZEOF_BASE_TYPE_NODE()	(G_STRUCT_OFFSET (TypeNode, supers))
#define MAX_N_SUPERS    (255)
#define MAX_N_CHILDREN  (4095)
#define MAX_N_IFACES    (511)

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
static guint           n_class_cache_funcs = 0;
static ClassCacheFunc *class_cache_funcs = NULL;


/* --- externs --- */
const char *g_log_domain_gobject = "GLib-Object";
GOBJECT_VAR GType _g_type_fundamental_last = 0;


/* --- type nodes --- */
static GHashTable       *g_type_nodes_ht = NULL;
static GType            *g_branch_seqnos = NULL;
static TypeNode       ***g_type_nodes = NULL;

static inline TypeNode*
LOOKUP_TYPE_NODE (register GType utype)
{
  register GType ftype = G_TYPE_FUNDAMENTAL (utype);
  register GType b_seqno = G_TYPE_BRANCH_SEQNO (utype);

  if (ftype < G_TYPE_FUNDAMENTAL_LAST && b_seqno < g_branch_seqnos[ftype])
    return g_type_nodes[ftype][b_seqno];
  else
    return NULL;
}
#define NODE_TYPE(node)         (node->supers[0])
#define NODE_PARENT_TYPE(node)  (node->supers[1])
#define NODE_NAME(node)         (g_quark_to_string (node->qname))

static TypeNode*
type_node_any_new (TypeNode    *pnode,
		   GType        ftype,
		   const gchar *name,
		   GTypePlugin *plugin,
		   GTypeFlags   type_flags)
{
  guint branch_last, n_supers = pnode ? pnode->n_supers + 1 : 0;
  GType type;
  TypeNode *node;
  guint i, node_size = 0;

  branch_last = g_branch_seqnos[ftype]++;
  type = G_TYPE_DERIVE_ID (ftype, branch_last);
  if (!branch_last || g_bit_storage (branch_last - 1) < g_bit_storage (g_branch_seqnos[ftype] - 1))
    g_type_nodes[ftype] = g_renew (TypeNode*, g_type_nodes[ftype], 1 << g_bit_storage (g_branch_seqnos[ftype] - 1));

  if (!pnode)
    node_size += sizeof (GTypeFundamentalInfo);	 /* fundamental type info */
  node_size += SIZEOF_BASE_TYPE_NODE ();	 /* TypeNode structure */
  node_size += (sizeof (GType) * (1 + n_supers + 1)); /* self + ancestors + 0 for ->supers[] */
  node = g_malloc0 (node_size);
  if (!pnode)					 /* fundamental type */
    node = G_STRUCT_MEMBER_P (node, sizeof (GTypeFundamentalInfo));
  g_type_nodes[ftype][branch_last] = node;

  node->n_supers = n_supers;
  if (!pnode)
    {
      node->supers[0] = type;
      node->supers[1] = 0;

      node->is_classed = (type_flags & G_TYPE_FLAG_CLASSED) != 0;
      node->is_instantiatable = (type_flags & G_TYPE_FLAG_INSTANTIATABLE) != 0;
      node->is_iface = G_TYPE_IS_INTERFACE (type);

      node->n_ifaces = 0;
      if (node->is_iface)
	node->private.iface_conformants = NULL;
      else
	node->private.iface_entries = NULL;
    }
  else
    {
      node->supers[0] = type;
      memcpy (node->supers + 1, pnode->supers, sizeof (GType) * (1 + pnode->n_supers + 1));

      node->is_classed = pnode->is_classed;
      node->is_instantiatable = pnode->is_instantiatable;
      node->is_iface = pnode->is_iface;

      if (node->is_iface)
	{
	  node->n_ifaces = 0;
	  node->private.iface_conformants = NULL;
	}
      else
	{
	  node->n_ifaces = pnode->n_ifaces;
	  node->private.iface_entries = g_memdup (pnode->private.iface_entries,
						  sizeof (pnode->private.iface_entries[0]) * node->n_ifaces);
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
  node->static_gdata = NULL;

  g_hash_table_insert (g_type_nodes_ht,
		       GUINT_TO_POINTER (node->qname),
		       GUINT_TO_POINTER (type));

  return node;
}

static inline GTypeFundamentalInfo*
type_node_fundamental_info (TypeNode *node)
{
  GType ftype = G_TYPE_FUNDAMENTAL (NODE_TYPE (node));
  
  if (ftype != NODE_TYPE (node))
    node = LOOKUP_TYPE_NODE (ftype);
  
  return node ? G_STRUCT_MEMBER_P (node, - sizeof (GTypeFundamentalInfo)) : NULL;
}

static TypeNode*
type_node_fundamental_new (GType        ftype,
			   const gchar *name,
			   GTypeFlags   type_flags)
{
  GTypeFundamentalInfo *finfo;
  TypeNode *node;
  guint i, flast = G_TYPE_FUNDAMENTAL_LAST;
  
  g_assert (ftype == G_TYPE_FUNDAMENTAL (ftype));
  
  type_flags &= G_TYPE_FLAG_MASK;

  _g_type_fundamental_last = MAX (_g_type_fundamental_last, ftype + 1);
  if (G_TYPE_FUNDAMENTAL_LAST > flast)
    {
      g_type_nodes = g_renew (TypeNode**, g_type_nodes, G_TYPE_FUNDAMENTAL_LAST);
      g_branch_seqnos = g_renew (GType, g_branch_seqnos, G_TYPE_FUNDAMENTAL_LAST);
      for (i = flast; i < G_TYPE_FUNDAMENTAL_LAST; i++)
	{
	  g_type_nodes[i] = NULL;
	  g_branch_seqnos[i] = 0;
	}
    }
  g_assert (g_branch_seqnos[ftype] == 0);

  node = type_node_any_new (NULL, ftype, name, NULL, type_flags);
  finfo = type_node_fundamental_info (node);
  finfo->type_flags = type_flags;

  return node;
}

static TypeNode*
type_node_new (TypeNode    *pnode,
	       const gchar *name,
	       GTypePlugin *plugin)

{
  g_assert (pnode);
  g_assert (pnode->n_supers < MAX_N_SUPERS);
  g_assert (pnode->n_children < MAX_N_CHILDREN);

  return type_node_any_new (pnode, G_TYPE_FUNDAMENTAL (NODE_TYPE (pnode)), name, plugin, 0);
}

static inline IFaceEntry*
type_lookup_iface_entry (TypeNode *node,
			 TypeNode *iface)
{
  if (iface->is_iface && node->n_ifaces)
    {
      IFaceEntry *ifaces = node->private.iface_entries - 1;
      guint n_ifaces = node->n_ifaces;
      GType iface_type = NODE_TYPE (iface);

      do		/* FIXME: should optimize iface lookups for <= 4 */
	{
	  guint i;
	  IFaceEntry *check;

	  i = (n_ifaces + 1) / 2;
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

static inline gchar*
type_descriptive_name (GType type)
{
  if (type)
    {
      gchar *name = g_type_name (type);

      return name ? name : "<unknown>";
    }
  else
    return "<invalid>";
}


/* --- type consistency checks --- */
static gboolean
check_plugin (GTypePlugin *plugin,
	      gboolean     need_complete_type_info,
	      gboolean     need_complete_interface_info,
	      const gchar *type_name)
{
  if (!plugin)
    {
      g_warning ("plugin handle for type `%s' is NULL",
		 type_name);
      return FALSE;
    }
  if (!plugin->vtable)
    {
      g_warning ("plugin for type `%s' has no function table",
		 type_name);
      return FALSE;
    }
  if (!plugin->vtable->plugin_ref)
    {
      g_warning ("plugin for type `%s' has no plugin_ref() implementation",
		 type_name);
      return FALSE;
    }
  if (!plugin->vtable->plugin_unref)
    {
      g_warning ("plugin for type `%s' has no plugin_unref() implementation",
		 type_name);
      return FALSE;
    }
  if (need_complete_type_info && !plugin->vtable->complete_type_info)
    {
      g_warning ("plugin for type `%s' has no complete_type_info() implementation",
		 type_name);
      return FALSE;
    }
  if (need_complete_interface_info && !plugin->vtable->complete_interface_info)
    {
      g_warning ("plugin for type `%s' has no complete_interface_info() implementation",
		 type_name);
      return FALSE;
    }
  return TRUE;
}

static gboolean
check_type_name (const gchar *type_name)
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
check_derivation (GType        parent_type,
		  const gchar *type_name)
{
  TypeNode *pnode = LOOKUP_TYPE_NODE (parent_type);
  GTypeFundamentalInfo* finfo = type_node_fundamental_info (pnode);
  
  if (!pnode)
    {
      g_warning ("cannot derive type `%s' from invalid parent type `%s'",
		 type_name,
		 type_descriptive_name (parent_type));
      return FALSE;
    }
  /* ensure flat derivability */
  if (!(finfo->type_flags & G_TYPE_FLAG_DERIVABLE))
    {
      g_warning ("cannot derive `%s' from non-derivable parent type `%s'",
		 type_name,
		 NODE_NAME (pnode));
      return FALSE;
    }
  /* ensure deep derivability */
  if (parent_type != G_TYPE_FUNDAMENTAL (parent_type) &&
      !(finfo->type_flags & G_TYPE_FLAG_DEEP_DERIVABLE))
    {
      g_warning ("cannot derive `%s' from non-fundamental parent type `%s'",
		 type_name,
		 NODE_NAME (pnode));
      return FALSE;
    }
  
  return TRUE;
}

static gboolean
check_value_table (const gchar           *type_name,
		   const GTypeValueTable *value_table)
{
  if (!value_table)
    return FALSE;
  else if (value_table->value_init == NULL)
    {
      if (value_table->value_free || value_table->value_copy ||
	  value_table->collect_type || value_table->collect_value ||
	  value_table->lcopy_type || value_table->lcopy_value)
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
      if ((value_table->collect_type || value_table->collect_value) &&
	  (!value_table->collect_type || !value_table->collect_value))
	{
	  g_warning ("one of `collect_type' and `collect_value()' is unspecified for type `%s'",
		     type_name);
	  return FALSE;
	}
      if ((value_table->lcopy_type || value_table->lcopy_value) &&
	  (!value_table->lcopy_type || !value_table->lcopy_value))
	{
	  g_warning ("one of `lcopy_type' and `lcopy_value()' is unspecified for type `%s'",
		     type_name);
	  return FALSE;
	}
    }

  return TRUE;
}

static gboolean
check_type_info (TypeNode        *pnode,
		 GType            ftype,
		 const gchar     *type_name,
		 const GTypeInfo *info)
{
  GTypeFundamentalInfo *finfo = type_node_fundamental_info (LOOKUP_TYPE_NODE (ftype));
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
find_conforming_type (TypeNode *pnode,
		      TypeNode *iface)
{
  TypeNode *node = NULL;
  guint i;

  if (type_lookup_iface_entry (pnode, iface))
    return pnode;

  for (i = 0; i < pnode->n_children && !node; i++)
    node = find_conforming_type (LOOKUP_TYPE_NODE (pnode->children[i]), iface);

  return node;
}

static gboolean
check_add_interface (GType instance_type,
		     GType iface_type)
{
  TypeNode *node = LOOKUP_TYPE_NODE (instance_type);
  TypeNode *iface = LOOKUP_TYPE_NODE (iface_type);
  TypeNode *tnode;

  if (!node || !node->is_instantiatable)
    {
      g_warning ("cannot add interfaces to invalid (non-instantiatable) type `%s'",
		 type_descriptive_name (instance_type));
      return FALSE;
    }
  if (!iface || !iface->is_iface)
    {
      g_warning ("cannot add invalid (non-interface) type `%s' to type `%s'",
		 type_descriptive_name (iface_type),
		 NODE_NAME (node));
      return FALSE;
    }
  tnode = LOOKUP_TYPE_NODE (NODE_PARENT_TYPE (iface));
  if (NODE_PARENT_TYPE (tnode) && !type_lookup_iface_entry (node, tnode))
    {
      g_warning ("cannot add sub-interface `%s' to type `%s' which does not conform to super-interface `%s'",
		 NODE_NAME (iface),
		 NODE_NAME (node),
		 NODE_NAME (tnode));
      return FALSE;
    }
  tnode = find_conforming_type (node, iface);
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
check_interface_info (TypeNode             *iface,
		      GType                 instance_type,
		      const GInterfaceInfo *info)
{
  if ((info->interface_finalize || info->interface_data) && !info->interface_init)
    {
      g_warning ("interface type `%s' for type `%s' comes without initializer",
		 NODE_NAME (iface),
		 type_descriptive_name (instance_type));
      return FALSE;
    }

  return TRUE;
}


/* --- type info (type node data) --- */
static void
type_data_make (TypeNode              *node,
		const GTypeInfo       *info,
		const GTypeValueTable *value_table)
{
  TypeData *data;
  GTypeValueTable *vtable = NULL;
  guint vtable_size = 0;
  
  g_assert (node->data == NULL && info != NULL);
  
  if (!value_table)
    {
      TypeNode *pnode = LOOKUP_TYPE_NODE (NODE_PARENT_TYPE (node));
      
      if (pnode)
	vtable = pnode->data->common.value_table;
      else
	{
	  static const GTypeValueTable zero_vtable = { NULL, };
	  
	  value_table = &zero_vtable;
	}
    }
  if (value_table)
    vtable_size = sizeof (GTypeValueTable);
  
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
      data->instance.n_preallocs = MIN (info->n_preallocs, 1024);
#ifdef FIXME_DISABLE_PREALLOCATIONS
      data->instance.n_preallocs = 0;
#endif
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
  else if (node->is_iface)
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
    *vtable = *value_table;
  node->data->common.value_table = vtable;

  g_assert (node->data->common.value_table != NULL); // FIXME: paranoid
}

static inline void
type_data_ref (TypeNode *node)
{
  if (!node->data)
    {
      TypeNode *pnode = LOOKUP_TYPE_NODE (NODE_PARENT_TYPE (node));
      GTypeInfo tmp_info;
      GTypeValueTable tmp_value_table;
      
      g_assert (node->plugin != NULL);
      
      if (pnode)
	type_data_ref (pnode);
      
      memset (&tmp_info, 0, sizeof (tmp_info));
      memset (&tmp_value_table, 0, sizeof (tmp_value_table));
      g_type_plugin_ref (node->plugin);
      g_type_plugin_complete_type_info (node->plugin, NODE_TYPE (node), &tmp_info, &tmp_value_table);
      check_type_info (pnode, G_TYPE_FUNDAMENTAL (NODE_TYPE (node)), NODE_NAME (node), &tmp_info);
      type_data_make (node, &tmp_info,
		      check_value_table (NODE_NAME (node), &tmp_value_table) ? &tmp_value_table : NULL);
    }
  else
    {
      g_assert (node->data->common.ref_count > 0);

      node->data->common.ref_count += 1;
    }
}

static inline void
type_data_unref (TypeNode *node,
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

      type_data_last_unref (NODE_TYPE (node), uncached);
    }
}

static void
type_node_add_iface_entry (TypeNode *node,
			   GType     iface_type)
{
  IFaceEntry *entries;
  guint i;

  g_assert (node->is_instantiatable && node->n_ifaces < MAX_N_IFACES);

  node->n_ifaces++;
  node->private.iface_entries = g_renew (IFaceEntry, node->private.iface_entries, node->n_ifaces);
  entries = node->private.iface_entries;
  for (i = 0; i < node->n_ifaces - 1; i++)
    if (entries[i].iface_type > iface_type)
      break;
  g_memmove (entries + i + 1, entries + i, sizeof (entries[0]) * (node->n_ifaces - i - 1));
  entries[i].iface_type = iface_type;
  entries[i].vtable = NULL;

  for (i = 0; i < node->n_children; i++)
    type_node_add_iface_entry (LOOKUP_TYPE_NODE (node->children[i]), iface_type);
}

static void
type_add_interface (TypeNode       *node,
		    TypeNode       *iface,
		    GInterfaceInfo *info,
		    GTypePlugin    *plugin)
{
  IFaceHolder *iholder = g_new0 (IFaceHolder, 1);
  
  /* we must not call any functions of GInterfaceInfo from within here, since
   * we got most probably called from _within_ a type registration function
   */
  g_assert (node->is_instantiatable && iface->is_iface && ((info && !plugin) || (!info && plugin)));
  
  iholder->next = iface->private.iface_conformants;
  iface->private.iface_conformants = iholder;
  iholder->instance_type = NODE_TYPE (node);
  iholder->info = info ? g_memdup (info, sizeof (*info)) : NULL;
  iholder->plugin = plugin;

  type_node_add_iface_entry (node, NODE_TYPE (iface));
}

static IFaceHolder*
type_iface_retrive_holder_info (TypeNode *iface,
				GType     instance_type)
{
  IFaceHolder *iholder = iface->private.iface_conformants;

  g_assert (iface->is_iface);

  while (iholder->instance_type != instance_type)
    iholder = iholder->next;

  if (!iholder->info)
    {
      GInterfaceInfo tmp_info;
      
      g_assert (iholder->plugin != NULL);
      
      type_data_ref (iface);

      memset (&tmp_info, 0, sizeof (tmp_info));
      g_type_plugin_ref (iholder->plugin);
      g_type_plugin_complete_interface_info (iholder->plugin, NODE_TYPE (iface), instance_type, &tmp_info);
      check_interface_info (iface, instance_type, &tmp_info);
      iholder->info = g_memdup (&tmp_info, sizeof (tmp_info));
    }
  
  return iholder;
}

static void
type_iface_blow_holder_info (TypeNode *iface,
			     GType     instance_type)
{
  IFaceHolder *iholder = iface->private.iface_conformants;

  g_assert (iface->is_iface);

  while (iholder->instance_type != instance_type)
    iholder = iholder->next;

  if (iholder->info && iholder->plugin)
    {
      g_free (iholder->info);
      iholder->info = NULL;
      g_type_plugin_unref (iholder->plugin);

      type_data_unref (iface, FALSE);
    }
}


/* --- type structure creation/destruction --- */
GTypeInstance*
g_type_create_instance (GType type)
{
  TypeNode *node = LOOKUP_TYPE_NODE (type);
  GTypeInstance *instance;
  GTypeClass *class;
  guint i;
  
  if (!node || !node->is_instantiatable)
    {
      g_warning ("cannot create new instance of invalid (non-instantiatable) type `%s'",
		 type_descriptive_name (type));
      return NULL;
    }
  
  class = g_type_class_ref (type);
  
  if (node->data->instance.n_preallocs)
    {
      if (!node->data->instance.mem_chunk)
	node->data->instance.mem_chunk = g_mem_chunk_new (NODE_NAME (node),
							  node->data->instance.instance_size,
							  (node->data->instance.instance_size *
							   node->data->instance.n_preallocs),
							  G_ALLOC_AND_FREE);
      instance = g_chunk_new0 (GTypeInstance, node->data->instance.mem_chunk);
    }
  else
    instance = g_malloc0 (node->data->instance.instance_size);
  
  for (i = node->n_supers; i > 0; i--)
    {
      TypeNode *pnode = LOOKUP_TYPE_NODE (node->supers[i]);
      
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

  class = instance->g_class;
  node = LOOKUP_TYPE_NODE (class->g_type);
  if (!node || !node->is_instantiatable || !node->data || node->data->class.class != (gpointer) class)
    {
      g_warning ("cannot free instance of invalid (non-instantiatable) type `%s'",
		 type_descriptive_name (class->g_type));
      return;
    }

  instance->g_class = NULL;
  if (node->data->instance.n_preallocs)
    g_chunk_free (instance, node->data->instance.mem_chunk);
  else
    g_free (instance);

  g_type_class_unref (class);
}

static void
type_propagate_iface_vtable (TypeNode       *pnode,
			     TypeNode       *iface,
			     GTypeInterface *vtable)
{
  IFaceEntry *entry = type_lookup_iface_entry (pnode, iface);
  guint i;

  entry->vtable = vtable;
  for (i = 0; i < pnode->n_children; i++)
    {
      TypeNode *node = LOOKUP_TYPE_NODE (pnode->children[i]);

      type_propagate_iface_vtable (node, iface, vtable);
    }
}

static void
type_iface_vtable_init (TypeNode       *iface,
			TypeNode       *node)
{
  IFaceEntry *entry = type_lookup_iface_entry (node, iface);
  IFaceHolder *iholder = type_iface_retrive_holder_info (iface, NODE_TYPE (node));
  GTypeInterface *vtable;
  
  g_assert (iface->data && entry && entry->vtable == NULL && iholder && iholder->info);

  vtable = g_malloc0 (iface->data->iface.vtable_size);
  type_propagate_iface_vtable (node, iface, vtable);
  vtable->g_type = NODE_TYPE (iface);
  vtable->g_instance_type = NODE_TYPE (node);

  if (iface->data->iface.vtable_init_base)
    iface->data->iface.vtable_init_base (vtable);
  if (iholder->info->interface_init)
    iholder->info->interface_init (vtable, iholder->info->interface_data);
}

static void
type_iface_vtable_finalize (TypeNode       *iface,
			    TypeNode       *node,
			    GTypeInterface *vtable)
{
  IFaceEntry *entry = type_lookup_iface_entry (node, iface);
  IFaceHolder *iholder = iface->private.iface_conformants;

  g_assert (entry && entry->vtable == vtable);

  while (iholder->instance_type != NODE_TYPE (node))
    iholder = iholder->next;
  g_assert (iholder && iholder->info);

  type_propagate_iface_vtable (node, iface, NULL);
  if (iholder->info->interface_finalize)
    iholder->info->interface_finalize (vtable, iholder->info->interface_data);
  if (iface->data->iface.vtable_finalize_base)
    iface->data->iface.vtable_finalize_base (vtable);
  
  vtable->g_type = 0;
  vtable->g_instance_type = 0;
  g_free (vtable);

  type_iface_blow_holder_info (iface, NODE_TYPE (node));
}

static void
type_class_init (TypeNode   *node,
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
      TypeNode *pnode = LOOKUP_TYPE_NODE (pclass->g_type);
      
      memcpy (class, pclass, pnode->data->class.class_size);
    }
  
  class->g_type = NODE_TYPE (node);
  
  /* stack all base class initialization functions, so we
   * call them in ascending order.
   */
  for (bnode = node; bnode; bnode = LOOKUP_TYPE_NODE (NODE_PARENT_TYPE (bnode)))
    if (bnode->data->class.class_init_base)
      init_slist = g_slist_prepend (init_slist, bnode->data->class.class_init_base);
  for (slist = init_slist; slist; slist = slist->next)
    {
      GBaseInitFunc class_init_base = slist->data;
      
      class_init_base (class);
    }
  g_slist_free (init_slist);
  
  if (node->data->class.class_init)
    node->data->class.class_init (class, (gpointer) node->data->class.class_data);

  /* ok, we got the class done, now initialize all interfaces */
  for (entry = NULL, i = 0; i < node->n_ifaces; i++)
    if (!node->private.iface_entries[i].vtable)
      entry = node->private.iface_entries + i;
  while (entry)
    {
      type_iface_vtable_init (LOOKUP_TYPE_NODE (entry->iface_type), node);
      
      for (entry = NULL, i = 0; i < node->n_ifaces; i++)
	if (!node->private.iface_entries[i].vtable)
	  entry = node->private.iface_entries + i;
    }
}

static void
type_data_finalize_class_ifaces (TypeNode *node)
{
  IFaceEntry *entry;
  guint i;
  
  g_assert (node->is_instantiatable && node->data && node->data->class.class && node->data->common.ref_count == 0);
  
  g_message ("finalizing interfaces for %sClass `%s'",
	     type_descriptive_name (G_TYPE_FUNDAMENTAL (NODE_TYPE (node))),
	     type_descriptive_name (NODE_TYPE (node)));
  
  for (entry = NULL, i = 0; i < node->n_ifaces; i++)
    if (node->private.iface_entries[i].vtable &&
	node->private.iface_entries[i].vtable->g_instance_type == NODE_TYPE (node))
      entry = node->private.iface_entries + i;
  while (entry)
    {
      type_iface_vtable_finalize (LOOKUP_TYPE_NODE (entry->iface_type), node, entry->vtable);
      
      for (entry = NULL, i = 0; i < node->n_ifaces; i++)
	if (node->private.iface_entries[i].vtable &&
	    node->private.iface_entries[i].vtable->g_instance_type == NODE_TYPE (node))
	  entry = node->private.iface_entries + i;
    }
}

static void
type_data_finalize_class (TypeNode  *node,
			  ClassData *cdata)
{
  GTypeClass *class = cdata->class;
  TypeNode *bnode;
  
  g_assert (cdata->class && cdata->common.ref_count == 0);
  
  g_message ("finalizing %sClass `%s'",
	     type_descriptive_name (G_TYPE_FUNDAMENTAL (NODE_TYPE (node))),
	     type_descriptive_name (NODE_TYPE (node)));

  if (cdata->class_finalize)
    cdata->class_finalize (class, (gpointer) cdata->class_data);
  
  /* call all base class destruction functions in descending order
   */
  if (cdata->class_finalize_base)
    cdata->class_finalize_base (class);
  for (bnode = LOOKUP_TYPE_NODE (NODE_PARENT_TYPE (node)); bnode; bnode = LOOKUP_TYPE_NODE (NODE_PARENT_TYPE (bnode)))
    if (bnode->data->class.class_finalize_base)
      bnode->data->class.class_finalize_base (class);
  
  class->g_type = 0;
  g_free (cdata->class);
}

static void
type_data_last_unref (GType    type,
		      gboolean uncached)
{
  TypeNode *node = LOOKUP_TYPE_NODE (type);

  g_return_if_fail (node != NULL && node->plugin != NULL);
  
  if (!node->data || node->data->common.ref_count == 0)
    {
      g_warning ("cannot drop last reference to unreferenced type `%s'",
		 type_descriptive_name (type));
      return;
    }

  if (node->is_classed && node->data && node->data->class.class)
    {
      guint i;

      for (i = 0; i < n_class_cache_funcs; i++)
	if (class_cache_funcs[i].cache_func (class_cache_funcs[i].cache_data, node->data->class.class))
	  break;
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
	  if (node->n_ifaces)
	    type_data_finalize_class_ifaces (node);
	  node->data = NULL;
	  type_data_finalize_class (node, &tdata->class);
	}
      else
	node->data = NULL;

      g_free (tdata);
      
      if (ptype)
	type_data_unref (LOOKUP_TYPE_NODE (ptype), FALSE);
      g_type_plugin_unref (node->plugin);
    }
}

void
g_type_add_class_cache_func (gpointer            cache_data,
			     GTypeClassCacheFunc cache_func)
{
  guint i;

  g_return_if_fail (cache_func != NULL);

  i = n_class_cache_funcs++;
  class_cache_funcs = g_renew (ClassCacheFunc, class_cache_funcs, n_class_cache_funcs);
  class_cache_funcs[i].cache_data = cache_data;
  class_cache_funcs[i].cache_func = cache_func;
}

void
g_type_remove_class_cache_func (gpointer            cache_data,
				GTypeClassCacheFunc cache_func)
{
  guint i;

  g_return_if_fail (cache_func != NULL);

  for (i = 0; i < n_class_cache_funcs; i++)
    if (class_cache_funcs[i].cache_data == cache_data &&
	class_cache_funcs[i].cache_func == cache_func)
      {
	n_class_cache_funcs--;
	g_memmove (class_cache_funcs + i,
		   class_cache_funcs + i + 1,
		   sizeof (class_cache_funcs[0]) * (n_class_cache_funcs - i));
	class_cache_funcs = g_renew (ClassCacheFunc, class_cache_funcs, n_class_cache_funcs);

	return;
      }

  g_warning (G_STRLOC ": cannot remove unregistered class cache func %p with data %p",
	     cache_func, cache_data);
}


/* --- type registration --- */
GType
g_type_register_fundamental (GType                       type_id,
			     const gchar                *type_name,
			     const GTypeInfo            *info,
			     const GTypeFundamentalInfo *finfo)
{
  GTypeFundamentalInfo *node_finfo;
  TypeNode *node;

  g_return_val_if_fail (type_id > 0, 0);
  g_return_val_if_fail (type_name != NULL, 0);
  g_return_val_if_fail (info != NULL, 0);
  g_return_val_if_fail (finfo != NULL, 0);

  if (!check_type_name (type_name))
    return 0;
  if (G_TYPE_FUNDAMENTAL (type_id) != type_id)
    {
      g_warning ("cannot register fundamental type `%s' with non-fundamental id (%u)",
		 type_name,
		 type_id);
      return 0;
    }
  if (LOOKUP_TYPE_NODE (type_id))
    {
      g_warning ("cannot register existing fundamental type `%s' (as `%s')",
		 type_descriptive_name (type_id),
		 type_name);
      return 0;
    }
  if ((finfo->type_flags & G_TYPE_FLAG_INSTANTIATABLE) &&
      !(finfo->type_flags & G_TYPE_FLAG_CLASSED))
    {
      g_warning ("cannot register instantiatable fundamental type `%s' as non-classed",
		 type_name);
      return 0;
    }

  node = type_node_fundamental_new (type_id, type_name, finfo->type_flags);
  node_finfo = type_node_fundamental_info (node);

  if (!check_type_info (NULL, G_TYPE_FUNDAMENTAL (NODE_TYPE (node)), type_name, info))
    return NODE_TYPE (node);
  type_data_make (node, info,
		  check_value_table (type_name, info->value_table) ? info->value_table : NULL);

  return NODE_TYPE (node);
}

GType
g_type_register_static (GType            parent_type,
			const gchar     *type_name,
			const GTypeInfo *info)
{
  TypeNode *pnode, *node;
  GType type;
  
  g_return_val_if_fail (parent_type > 0, 0);
  g_return_val_if_fail (type_name != NULL, 0);
  g_return_val_if_fail (info != NULL, 0);
  
  if (!check_type_name (type_name))
    return 0;
  if (!check_derivation (parent_type, type_name))
    return 0;

  pnode = LOOKUP_TYPE_NODE (parent_type);
  type_data_ref (pnode);

  if (!check_type_info (pnode, G_TYPE_FUNDAMENTAL (parent_type), type_name, info))
    return 0;
  if (info->class_finalize)
    {
      g_warning ("class destructor specified for static type `%s'",
		 type_name);
      return 0;
    }

  node = type_node_new (pnode, type_name, NULL);
  type = NODE_TYPE (node);
  type_data_make (node, info,
		  check_value_table (type_name, info->value_table) ? info->value_table : NULL);

  return type;
}

GType
g_type_register_dynamic (GType        parent_type,
			 const gchar *type_name,
			 GTypePlugin *plugin)
{
  TypeNode *pnode, *node;
  GType type;

  g_return_val_if_fail (parent_type > 0, 0);
  g_return_val_if_fail (type_name != NULL, 0);
  g_return_val_if_fail (plugin != NULL, 0);

  if (!check_type_name (type_name))
    return 0;
  if (!check_derivation (parent_type, type_name))
    return 0;
  if (!check_plugin (plugin, TRUE, FALSE, type_name))
    return 0;
  pnode = LOOKUP_TYPE_NODE (parent_type);

  node = type_node_new (pnode, type_name, plugin);
  type = NODE_TYPE (node);

  return type;
}

void
g_type_add_interface_static (GType           instance_type,
			     GType           interface_type,
			     GInterfaceInfo *info)
{
  TypeNode *node;
  TypeNode *iface;
  
  g_return_if_fail (G_TYPE_IS_INSTANTIATABLE (instance_type));
  g_return_if_fail (g_type_parent (interface_type) == G_TYPE_INTERFACE);

  if (!check_add_interface (instance_type, interface_type))
    return;
  node = LOOKUP_TYPE_NODE (instance_type);
  iface = LOOKUP_TYPE_NODE (interface_type);
  if (!check_interface_info (iface, NODE_TYPE (node), info))
    return;
  type_add_interface (node, iface, info, NULL);
}

void
g_type_add_interface_dynamic (GType        instance_type,
			      GType        interface_type,
			      GTypePlugin *plugin)
{
  TypeNode *node;
  TypeNode *iface;
  
  g_return_if_fail (G_TYPE_IS_INSTANTIATABLE (instance_type));
  g_return_if_fail (g_type_parent (interface_type) == G_TYPE_INTERFACE);

  if (!check_add_interface (instance_type, interface_type))
    return;
  node = LOOKUP_TYPE_NODE (instance_type);
  iface = LOOKUP_TYPE_NODE (interface_type);
  if (!check_plugin (plugin, FALSE, TRUE, NODE_NAME (node)))
    return;
  type_add_interface (node, iface, NULL, plugin);
}


/* --- public API functions --- */
gpointer
g_type_class_ref (GType type)
{
  TypeNode *node = LOOKUP_TYPE_NODE (type);

  /* optimize for common code path
   */
  if (node && node->is_classed && node->data &&
      node->data->class.class && node->data->common.ref_count > 0)
    {
      type_data_ref (node);

      return node->data->class.class;
    }

  if (!node || !node->is_classed ||
      (node->data && node->data->common.ref_count < 1))
    {
      g_warning ("cannot retrive class for invalid (unclassed) type `%s'",
		 type_descriptive_name (type));
      return NULL;
    }

  type_data_ref (node);

  if (!node->data->class.class)
    {
      GType ptype = NODE_PARENT_TYPE (node);
      GTypeClass *pclass = ptype ? g_type_class_ref (ptype) : NULL;

      type_class_init (node, pclass);
    }

  return node->data->class.class;
}

void
g_type_class_unref (gpointer g_class)
{
  TypeNode *node;
  GTypeClass *class = g_class;

  g_return_if_fail (g_class != NULL);

  node = LOOKUP_TYPE_NODE (class->g_type);
  if (node && node->is_classed && node->data &&
      node->data->class.class == class && node->data->common.ref_count > 0)
    type_data_unref (node, FALSE);
  else
    g_warning ("cannot unreference class of invalid (unclassed) type `%s'",
	       type_descriptive_name (class->g_type));
}

void
g_type_class_unref_uncached (gpointer g_class)
{
  TypeNode *node;
  GTypeClass *class = g_class;

  g_return_if_fail (g_class != NULL);

  node = LOOKUP_TYPE_NODE (class->g_type);
  if (node && node->is_classed && node->data &&
      node->data->class.class == class && node->data->common.ref_count > 0)
    type_data_unref (node, TRUE);
  else
    g_warning ("cannot unreference class of invalid (unclassed) type `%s'",
	       type_descriptive_name (class->g_type));
}

gpointer
g_type_class_peek (GType type)
{
  TypeNode *node = LOOKUP_TYPE_NODE (type);

  if (node && node->is_classed && node->data && node->data->class.class) /* common.ref_count _may_ be 0 */
    return node->data->class.class;
  else
    return NULL;
}

gpointer
g_type_class_peek_parent (gpointer g_class)
{
  TypeNode *node;

  g_return_val_if_fail (g_class != NULL, NULL);

  node = LOOKUP_TYPE_NODE (G_TYPE_FROM_CLASS (g_class));
  if (node && node->is_classed && node->data && NODE_PARENT_TYPE (node))
    {
      node = LOOKUP_TYPE_NODE (NODE_PARENT_TYPE (node));

      return node->data->class.class;
    }

  return NULL;
}

gpointer
g_type_interface_peek (gpointer instance_class,
		       GType    iface_type)
{
  TypeNode *node;
  TypeNode *iface;
  GTypeClass *class = instance_class;

  g_return_val_if_fail (instance_class != NULL, NULL);

  node = LOOKUP_TYPE_NODE (class->g_type);
  iface = LOOKUP_TYPE_NODE (iface_type);
  if (node && node->is_instantiatable && iface)
    {
      IFaceEntry *entry = type_lookup_iface_entry (node, iface);

      if (entry && entry->vtable)
	return entry->vtable;
    }

  return NULL;
}

GTypeValueTable*
g_type_value_table_peek (GType type)
{
  TypeNode *node = LOOKUP_TYPE_NODE (type);

  if (node && node->data && node->data->common.ref_count > 0)
    return node->data->common.value_table->value_init ? node->data->common.value_table : NULL;
  else
    return NULL;
}

gchar*
g_type_name (GType type)
{
  TypeNode *node = LOOKUP_TYPE_NODE (type);
  
  return node ? NODE_NAME (node) : NULL;
}

GQuark
g_type_qname (GType type)
{
  TypeNode *node = LOOKUP_TYPE_NODE (type);

  return node ? node->qname : 0;
}

GType
g_type_from_name (const gchar *name)
{
  GQuark quark;
  
  g_return_val_if_fail (name != NULL, 0);
  
  quark = g_quark_try_string (name);
  if (quark)
    {
      GType type = GPOINTER_TO_UINT (g_hash_table_lookup (g_type_nodes_ht, GUINT_TO_POINTER (quark)));
      
      if (type)
	return type;
    }
  
  return 0;
}

GType
g_type_parent (GType type)
{
  TypeNode *node = LOOKUP_TYPE_NODE (type);

  return node ? NODE_PARENT_TYPE (node) : 0;
}

GType
g_type_next_base (GType type,
		  GType base_type)
{
  TypeNode *node = LOOKUP_TYPE_NODE (type);
  
  if (node)
    {
      TypeNode *base_node = LOOKUP_TYPE_NODE (base_type);
      
      if (base_node && base_node->n_supers < node->n_supers)
	{
	  guint n = node->n_supers - base_node->n_supers;
	  
	  if (node->supers[n] == base_type)
	    return node->supers[n - 1];
	}
    }
  
  return 0;
}

gboolean
g_type_is_a (GType type,
	     GType is_a_type)
{
  if (type != is_a_type)
    {
      TypeNode *node = LOOKUP_TYPE_NODE (type);

      if (node)
	{
	  TypeNode *a_node = LOOKUP_TYPE_NODE (is_a_type);

	  if (a_node && a_node->n_supers <= node->n_supers)
	    return node->supers[node->n_supers - a_node->n_supers] == is_a_type;
	}
    }
  else
    return LOOKUP_TYPE_NODE (type) != NULL;

  return FALSE;
}

gboolean
g_type_conforms_to (GType type,
		    GType iface_type)
{
  if (type != iface_type)
    {
      TypeNode *node = LOOKUP_TYPE_NODE (type);

      if (node)
	{
	  TypeNode *iface_node = LOOKUP_TYPE_NODE (iface_type);

	  if (iface_node)
	    {
	      if (iface_node->is_iface && node->is_instantiatable)
		return type_lookup_iface_entry (node, iface_node) != NULL;
	      else if (iface_node->n_supers <= node->n_supers)
		return node->supers[node->n_supers - iface_node->n_supers] == iface_type;
	    }
	}
    }
  else
    {
      TypeNode *node = LOOKUP_TYPE_NODE (type);

      return node && (node->is_iface || node->is_instantiatable);
    }

  return FALSE;
}

guint
g_type_fundamental_branch_last (GType type)
{
  GType ftype = G_TYPE_FUNDAMENTAL (type);

  return ftype < G_TYPE_FUNDAMENTAL_LAST ? g_branch_seqnos[ftype] : 0;
}

GType* /* free result */
g_type_children (GType  type,
		 guint *n_children)
{
  TypeNode *node = LOOKUP_TYPE_NODE (type);
  
  if (node)
    {
      GType *children = g_new (GType, node->n_children + 1);
      
      memcpy (children, node->children, sizeof (GType) * node->n_children);
      children[node->n_children] = 0;
      
      if (n_children)
	*n_children = node->n_children;
      
      return children;
    }
  else
    {
      if (n_children)
	*n_children = 0;
      
      return NULL;
    }
}

GType* /* free result */
g_type_interfaces (GType  type,
		   guint *n_interfaces)
{
  TypeNode *node = LOOKUP_TYPE_NODE (type);

  if (node && node->is_instantiatable)
    {
      GType *ifaces = g_new (GType, node->n_ifaces + 1);
      guint i;

      for (i = 0; i < node->n_ifaces; i++)
	ifaces[i] = node->private.iface_entries[i].iface_type;
      ifaces[i] = 0;

      if (n_interfaces)
	*n_interfaces = node->n_ifaces;

      return ifaces;
    }
  else
    {
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

gpointer
g_type_get_qdata (GType  type,
		  GQuark quark)
{
  TypeNode *node = LOOKUP_TYPE_NODE (type);
  GData *gdata;
  
  g_return_val_if_fail (node != NULL, NULL);

  gdata = node->static_gdata;
  if (quark && gdata && gdata->n_qdatas)
    {
      QData *qdatas = gdata->qdatas - 1;
      guint n_qdatas = gdata->n_qdatas;

      do                /* FIXME: should optimize qdata lookups for <= 4 */
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

void
g_type_set_qdata (GType    type,
		  GQuark   quark,
		  gpointer data)
{
  TypeNode *node = LOOKUP_TYPE_NODE (type);
  GData *gdata;
  QData *qdata;
  guint i;

  g_return_if_fail (node != NULL);
  g_return_if_fail (quark != 0);

  /* setup qdata list if necessary */
  if (!node->static_gdata)
    node->static_gdata = g_new0 (GData, 1);
  gdata = node->static_gdata;

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


/* --- implementation details --- */
gboolean
g_type_check_flags (GType      type,
		    GTypeFlags flags)
{
  TypeNode *node = LOOKUP_TYPE_NODE (type);
  
  flags &= G_TYPE_FLAG_MASK;
  if (node)
    {
      GTypeFundamentalInfo *finfo = type_node_fundamental_info (node);
      
      return (finfo->type_flags & flags) != 0;
    }
  
  return FALSE;
}

gboolean
g_type_is_dynamic (GType      type,
		   GTypeFlags flags)
{
  TypeNode *node = LOOKUP_TYPE_NODE (type);

  return node && node->plugin;
}

gboolean
g_type_instance_conforms_to (GTypeInstance *type_instance,
			     GType          iface_type)
{
  return (type_instance && type_instance->g_class &&
	  g_type_conforms_to (type_instance->g_class->g_type, iface_type));
}

gboolean
g_type_class_is_a (GTypeClass *type_class,
		   GType       is_a_type)
{
  return (type_class && g_type_is_a (type_class->g_type, is_a_type));
}

GTypeInstance*
g_type_check_instance_cast (GTypeInstance *type_instance,
			    GType          iface_type)
{
  if (!type_instance)
    {
      g_warning ("invalid cast from (NULL) pointer to `%s'",
		 type_descriptive_name (iface_type));
      return type_instance;
    }
  if (!type_instance->g_class)
    {
      g_warning ("invalid unclassed pointer in cast to `%s'",
		 type_descriptive_name (iface_type));
      return type_instance;
    }
  if (!G_TYPE_IS_CLASSED (type_instance->g_class->g_type))
    {
      g_warning ("invalid unclassed type `%s' in cast to `%s'",
		 type_descriptive_name (type_instance->g_class->g_type),
		 type_descriptive_name (iface_type));
      return type_instance;
    }
  if (!g_type_conforms_to (type_instance->g_class->g_type, iface_type))
    {
      g_warning ("invalid cast from `%s' to `%s'",
		 type_descriptive_name (type_instance->g_class->g_type),
		 type_descriptive_name (iface_type));
      return type_instance;
    }

  return type_instance;
}

GTypeClass*
g_type_check_class_cast (GTypeClass *type_class,
			 GType       is_a_type)
{
  if (!type_class)
    {
      g_warning ("invalid class cast from (NULL) pointer to `%s'",
		 type_descriptive_name (is_a_type));
      return type_class;
    }
  if (!G_TYPE_IS_CLASSED (type_class->g_type))
    {
      g_warning ("invalid unclassed type `%s' in class cast to `%s'",
		 type_descriptive_name (type_class->g_type),
		 type_descriptive_name (is_a_type));
      return type_class;
    }
  if (!g_type_is_a (type_class->g_type, is_a_type))
    {
      g_warning ("invalid class cast from `%s' to `%s'",
		 type_descriptive_name (type_class->g_type),
		 type_descriptive_name (is_a_type));
      return type_class;
    }

  return type_class;
}


/* --- foreign prototypes --- */
extern void	g_value_types_init	(void); /* sync with gvaluetypes.c */
extern void	g_enum_types_init	(void);	/* sync with genums.c */
extern void     g_param_type_init       (void);	/* sync with gparam.c */
extern void     g_object_type_init      (void);	/* sync with gobject.c */
extern void	g_param_spec_types_init	(void);	/* sync with gparamspecs.c */


/* --- initialization --- */
void
g_type_init (void)
{
  static TypeNode *type0_node = NULL;
  GTypeInfo info;
  TypeNode *node;
  GType type;

  if (G_TYPE_FUNDAMENTAL_LAST)
    return;

  /* type qname hash table */
  g_type_nodes_ht = g_hash_table_new (g_direct_hash, g_direct_equal);

  /* invalid type G_TYPE_INVALID (0)
   */
  _g_type_fundamental_last = 1;
  g_type_nodes = g_renew (TypeNode**, g_type_nodes, G_TYPE_FUNDAMENTAL_LAST);
  g_type_nodes[0] = &type0_node;
  g_branch_seqnos = g_renew (GType, g_branch_seqnos, G_TYPE_FUNDAMENTAL_LAST);
  g_branch_seqnos[0] = 1;

  /* void type G_TYPE_NONE
   */
  node = type_node_fundamental_new (G_TYPE_NONE, "void", 0);
  type = NODE_TYPE (node);
  g_assert (type == G_TYPE_NONE);

  /* interface fundamental type G_TYPE_INTERFACE (!classed)
   */
  memset (&info, 0, sizeof (info));
  node = type_node_fundamental_new (G_TYPE_INTERFACE, "GInterface", G_TYPE_FLAG_DERIVABLE);
  type = NODE_TYPE (node);
  type_data_make (node, &info, NULL); // FIXME
  g_assert (type == G_TYPE_INTERFACE);

  /* G_TYPE_* value types
   */
  g_value_types_init ();
  
  /* G_TYPE_ENUM & G_TYPE_FLAGS
   */
  g_enum_types_init ();
  
  /* G_TYPE_PARAM
   */
  g_param_type_init ();

  /* G_TYPE_OBJECT
   */
  g_object_type_init ();

  /* G_TYPE_PARAM_* pspec types
   */
  g_param_spec_types_init ();
}
