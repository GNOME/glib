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
#if !defined (__GLIB_GOBJECT_H_INSIDE__) && !defined (GOBJECT_COMPILATION)
#error "Only <glib-object.h> can be included directly."
#endif

#ifndef __G_TYPE_H__
#define __G_TYPE_H__

#include        <glib.h>

G_BEGIN_DECLS

/* Basic Type Macros
 */
#define G_TYPE_FUNDAMENTAL(type)	(g_type_fundamental (type))
#define	G_TYPE_FUNDAMENTAL_MAX		(255 << G_TYPE_FUNDAMENTAL_SHIFT)

/* Constant fundamental types,
 * introduced by g_type_init().
 */
#define G_TYPE_INVALID			G_TYPE_MAKE_FUNDAMENTAL (0)
#define G_TYPE_NONE			G_TYPE_MAKE_FUNDAMENTAL (1)
#define G_TYPE_INTERFACE		G_TYPE_MAKE_FUNDAMENTAL (2)
#define G_TYPE_CHAR			G_TYPE_MAKE_FUNDAMENTAL (3)
#define G_TYPE_UCHAR			G_TYPE_MAKE_FUNDAMENTAL (4)
#define G_TYPE_BOOLEAN			G_TYPE_MAKE_FUNDAMENTAL (5)
#define G_TYPE_INT			G_TYPE_MAKE_FUNDAMENTAL (6)
#define G_TYPE_UINT			G_TYPE_MAKE_FUNDAMENTAL (7)
#define G_TYPE_LONG			G_TYPE_MAKE_FUNDAMENTAL (8)
#define G_TYPE_ULONG			G_TYPE_MAKE_FUNDAMENTAL (9)
#define G_TYPE_INT64			G_TYPE_MAKE_FUNDAMENTAL (10)
#define G_TYPE_UINT64			G_TYPE_MAKE_FUNDAMENTAL (11)
#define G_TYPE_ENUM			G_TYPE_MAKE_FUNDAMENTAL (12)
#define G_TYPE_FLAGS			G_TYPE_MAKE_FUNDAMENTAL (13)
#define G_TYPE_FLOAT			G_TYPE_MAKE_FUNDAMENTAL (14)
#define G_TYPE_DOUBLE			G_TYPE_MAKE_FUNDAMENTAL (15)
#define G_TYPE_STRING			G_TYPE_MAKE_FUNDAMENTAL (16)
#define G_TYPE_POINTER			G_TYPE_MAKE_FUNDAMENTAL (17)
#define G_TYPE_BOXED			G_TYPE_MAKE_FUNDAMENTAL (18)
#define G_TYPE_PARAM			G_TYPE_MAKE_FUNDAMENTAL (19)
#define G_TYPE_OBJECT			G_TYPE_MAKE_FUNDAMENTAL (20)


/* Reserved fundamental type numbers to create new fundamental
 * type IDs with G_TYPE_MAKE_FUNDAMENTAL().
 * Send email to gtk-devel-list@redhat.com for reservations.
 */
#define	G_TYPE_FUNDAMENTAL_SHIFT	(2)
#define	G_TYPE_MAKE_FUNDAMENTAL(x)	((GType) ((x) << G_TYPE_FUNDAMENTAL_SHIFT))
#define G_TYPE_RESERVED_GLIB_FIRST	(21)
#define G_TYPE_RESERVED_GLIB_LAST	(31)
#define G_TYPE_RESERVED_BSE_FIRST	(32)
#define G_TYPE_RESERVED_BSE_LAST	(48)
#define G_TYPE_RESERVED_USER_FIRST	(49)


/* Type Checking Macros
 */
#define G_TYPE_IS_FUNDAMENTAL(type)             ((type) <= G_TYPE_FUNDAMENTAL_MAX)
#define G_TYPE_IS_DERIVED(type)                 ((type) > G_TYPE_FUNDAMENTAL_MAX)
#define G_TYPE_IS_INTERFACE(type)               (G_TYPE_FUNDAMENTAL (type) == G_TYPE_INTERFACE)
#define G_TYPE_IS_CLASSED(type)                 (g_type_test_flags ((type), G_TYPE_FLAG_CLASSED))
#define G_TYPE_IS_INSTANTIATABLE(type)          (g_type_test_flags ((type), G_TYPE_FLAG_INSTANTIATABLE))
#define G_TYPE_IS_DERIVABLE(type)               (g_type_test_flags ((type), G_TYPE_FLAG_DERIVABLE))
#define G_TYPE_IS_DEEP_DERIVABLE(type)          (g_type_test_flags ((type), G_TYPE_FLAG_DEEP_DERIVABLE))
#define G_TYPE_IS_ABSTRACT(type)                (g_type_test_flags ((type), G_TYPE_FLAG_ABSTRACT))
#define G_TYPE_IS_VALUE_ABSTRACT(type)          (g_type_test_flags ((type), G_TYPE_FLAG_VALUE_ABSTRACT))
#define G_TYPE_IS_VALUE_TYPE(type)              (g_type_check_is_value_type (type))
#define G_TYPE_HAS_VALUE_TABLE(type)            (g_type_value_table_peek (type) != NULL)


/* Typedefs
 */
#if	GLIB_SIZEOF_LONG == GLIB_SIZEOF_SIZE_T
typedef gulong                          GType;
#else	/* hm, shouldn't happen? */
typedef gsize                           GType;
#endif
typedef struct _GValue                  GValue;
typedef union  _GTypeCValue             GTypeCValue;
typedef struct _GTypePlugin             GTypePlugin;
typedef struct _GTypeClass              GTypeClass;
typedef struct _GTypeInterface          GTypeInterface;
typedef struct _GTypeInstance           GTypeInstance;
typedef struct _GTypeInfo               GTypeInfo;
typedef struct _GTypeFundamentalInfo    GTypeFundamentalInfo;
typedef struct _GInterfaceInfo          GInterfaceInfo;
typedef struct _GTypeValueTable         GTypeValueTable;
typedef struct _GTypeQuery		GTypeQuery;


/* Basic Type Structures
 */
struct _GTypeClass
{
  /*< private >*/
  GType g_type;
};
struct _GTypeInstance
{
  /*< private >*/
  GTypeClass *g_class;
};
struct _GTypeInterface
{
  /*< private >*/
  GType g_type;         /* iface type */
  GType g_instance_type;
};
struct _GTypeQuery
{
  GType		type;
  const gchar  *type_name;
  guint		class_size;
  guint		instance_size;
};


/* Casts, checks and accessors for structured types
 * usage of these macros is reserved to type implementations only
 */
/*< protected >*/
#define G_TYPE_CHECK_INSTANCE(instance)				(_G_TYPE_CHI ((GTypeInstance*) (instance)))
#define G_TYPE_CHECK_INSTANCE_CAST(instance, g_type, c_type)    (_G_TYPE_CIC ((instance), (g_type), c_type))
#define G_TYPE_CHECK_INSTANCE_TYPE(instance, g_type)            (_G_TYPE_CIT ((instance), (g_type)))
#define G_TYPE_INSTANCE_GET_CLASS(instance, g_type, c_type)     (_G_TYPE_IGC ((instance), (g_type), c_type))
#define G_TYPE_INSTANCE_GET_INTERFACE(instance, g_type, c_type) (_G_TYPE_IGI ((instance), (g_type), c_type))
#define G_TYPE_CHECK_CLASS_CAST(g_class, g_type, c_type)        (_G_TYPE_CCC ((g_class), (g_type), c_type))
#define G_TYPE_CHECK_CLASS_TYPE(g_class, g_type)                (_G_TYPE_CCT ((g_class), (g_type)))
#define G_TYPE_CHECK_VALUE(value)				(_G_TYPE_CHV ((value)))
#define G_TYPE_CHECK_VALUE_TYPE(value, g_type)			(_G_TYPE_CVH ((value), (g_type)))
#define G_TYPE_FROM_INSTANCE(instance)                          (G_TYPE_FROM_CLASS (((GTypeInstance*) (instance))->g_class))
#define G_TYPE_FROM_CLASS(g_class)                              (((GTypeClass*) (g_class))->g_type)
#define G_TYPE_FROM_INTERFACE(g_iface)                          (((GTypeInterface*) (g_iface))->g_type)

#define G_TYPE_INSTANCE_GET_PRIVATE(instance, g_type, c_type)   ((c_type*) g_type_instance_get_private ((GTypeInstance*) (instance), (g_type)))


/* debug flags for g_type_init_with_debug_flags() */
typedef enum	/*< skip >*/
{
  G_TYPE_DEBUG_NONE	= 0,
  G_TYPE_DEBUG_OBJECTS	= 1 << 0,
  G_TYPE_DEBUG_SIGNALS	= 1 << 1,
  G_TYPE_DEBUG_MASK	= 0x03
} GTypeDebugFlags;


/* --- prototypes --- */
void                  g_type_init                    (void);
void                  g_type_init_with_debug_flags   (GTypeDebugFlags  debug_flags);
G_CONST_RETURN gchar* g_type_name                    (GType            type);
GQuark                g_type_qname                   (GType            type);
GType                 g_type_from_name               (const gchar     *name);
GType                 g_type_parent                  (GType            type);
guint                 g_type_depth                   (GType            type);
GType                 g_type_next_base               (GType            leaf_type,
						      GType            root_type);
gboolean              g_type_is_a                    (GType            type,
						      GType            is_a_type);
gpointer              g_type_class_ref               (GType            type);
gpointer              g_type_class_peek              (GType            type);
gpointer              g_type_class_peek_static       (GType            type);
void                  g_type_class_unref             (gpointer         g_class);
gpointer              g_type_class_peek_parent       (gpointer         g_class);
gpointer              g_type_interface_peek          (gpointer         instance_class,
						      GType            iface_type);
gpointer              g_type_interface_peek_parent   (gpointer         g_iface);

gpointer              g_type_default_interface_ref   (GType            g_type);
gpointer              g_type_default_interface_peek  (GType            g_type);
void                  g_type_default_interface_unref (gpointer         g_iface);

/* g_free() the returned arrays */
GType*                g_type_children                (GType            type,
						      guint           *n_children);
GType*                g_type_interfaces              (GType            type,
						      guint           *n_interfaces);

/* per-type _static_ data */
void                  g_type_set_qdata               (GType            type,
						      GQuark           quark,
						      gpointer         data);
gpointer              g_type_get_qdata               (GType            type,
						      GQuark           quark);
void		      g_type_query		     (GType	       type,
						      GTypeQuery      *query);


/* --- type registration --- */
typedef void   (*GBaseInitFunc)              (gpointer         g_class);
typedef void   (*GBaseFinalizeFunc)          (gpointer         g_class);
typedef void   (*GClassInitFunc)             (gpointer         g_class,
					      gpointer         class_data);
typedef void   (*GClassFinalizeFunc)         (gpointer         g_class,
					      gpointer         class_data);
typedef void   (*GInstanceInitFunc)          (GTypeInstance   *instance,
					      gpointer         g_class);
typedef void   (*GInterfaceInitFunc)         (gpointer         g_iface,
					      gpointer         iface_data);
typedef void   (*GInterfaceFinalizeFunc)     (gpointer         g_iface,
					      gpointer         iface_data);
typedef gboolean (*GTypeClassCacheFunc)	     (gpointer	       cache_data,
					      GTypeClass      *g_class);
typedef void     (*GTypeInterfaceCheckFunc)  (gpointer	       func_data,
					      gpointer         g_iface);
typedef enum    /*< skip >*/
{
  G_TYPE_FLAG_CLASSED           = (1 << 0),
  G_TYPE_FLAG_INSTANTIATABLE    = (1 << 1),
  G_TYPE_FLAG_DERIVABLE         = (1 << 2),
  G_TYPE_FLAG_DEEP_DERIVABLE    = (1 << 3)
} GTypeFundamentalFlags;
typedef enum    /*< skip >*/
{
  G_TYPE_FLAG_ABSTRACT		= (1 << 4),
  G_TYPE_FLAG_VALUE_ABSTRACT	= (1 << 5)
} GTypeFlags;
struct _GTypeInfo
{
  /* interface types, classed types, instantiated types */
  guint16                class_size;
  
  GBaseInitFunc          base_init;
  GBaseFinalizeFunc      base_finalize;
  
  /* interface types, classed types, instantiated types */
  GClassInitFunc         class_init;
  GClassFinalizeFunc     class_finalize;
  gconstpointer          class_data;
  
  /* instantiated types */
  guint16                instance_size;
  guint16                n_preallocs;
  GInstanceInitFunc      instance_init;
  
  /* value handling */
  const GTypeValueTable	*value_table;
};
struct _GTypeFundamentalInfo
{
  GTypeFundamentalFlags  type_flags;
};
struct _GInterfaceInfo
{
  GInterfaceInitFunc     interface_init;
  GInterfaceFinalizeFunc interface_finalize;
  gpointer               interface_data;
};
struct _GTypeValueTable
{
  void     (*value_init)         (GValue       *value);
  void     (*value_free)         (GValue       *value);
  void     (*value_copy)         (const GValue *src_value,
				  GValue       *dest_value);
  /* varargs functionality (optional) */
  gpointer (*value_peek_pointer) (const GValue *value);
  gchar	    *collect_format;
  gchar*   (*collect_value)      (GValue       *value,
				  guint         n_collect_values,
				  GTypeCValue  *collect_values,
				  guint		collect_flags);
  gchar	    *lcopy_format;
  gchar*   (*lcopy_value)        (const GValue *value,
				  guint         n_collect_values,
				  GTypeCValue  *collect_values,
				  guint		collect_flags);
};
GType g_type_register_static		(GType			     parent_type,
					 const gchar		    *type_name,
					 const GTypeInfo	    *info,
					 GTypeFlags		     flags);
GType g_type_register_dynamic		(GType			     parent_type,
					 const gchar		    *type_name,
					 GTypePlugin		    *plugin,
					 GTypeFlags		     flags);
GType g_type_register_fundamental	(GType			     type_id,
					 const gchar		    *type_name,
					 const GTypeInfo	    *info,
					 const GTypeFundamentalInfo *finfo,
					 GTypeFlags		     flags);
void  g_type_add_interface_static	(GType			     instance_type,
					 GType			     interface_type,
					 const GInterfaceInfo	    *info);
void  g_type_add_interface_dynamic	(GType			     instance_type,
					 GType			     interface_type,
					 GTypePlugin		    *plugin);
void  g_type_interface_add_prerequisite (GType			     interface_type,
					 GType			     prerequisite_type);
GType *g_type_interface_prerequisites   (GType                       interface_type,
					 guint                       *n_prerequisites);

void     g_type_class_add_private    (gpointer       g_class,
				      gsize          private_size);
gpointer g_type_instance_get_private (GTypeInstance *instance,
				      GType          private_type);

/* --- protected (for fundamental type implementations) --- */
GTypePlugin*	 g_type_get_plugin		(GType		     type);
GTypePlugin*	 g_type_interface_get_plugin	(GType		     instance_type,
						 GType               interface_type);
GType		 g_type_fundamental_next	(void);
GType		 g_type_fundamental		(GType		     type_id);
GTypeInstance*   g_type_create_instance         (GType               type);
void             g_type_free_instance           (GTypeInstance      *instance);

void		 g_type_add_class_cache_func    (gpointer	     cache_data,
						 GTypeClassCacheFunc cache_func);
void		 g_type_remove_class_cache_func (gpointer	     cache_data,
						 GTypeClassCacheFunc cache_func);
void             g_type_class_unref_uncached    (gpointer            g_class);

void             g_type_add_interface_check     (gpointer	         check_data,
						 GTypeInterfaceCheckFunc check_func);
void             g_type_remove_interface_check  (gpointer	         check_data,
						 GTypeInterfaceCheckFunc chec_func);

GTypeValueTable* g_type_value_table_peek        (GType		     type);


/*< private >*/
gboolean	 g_type_check_instance          (GTypeInstance      *instance);
GTypeInstance*   g_type_check_instance_cast     (GTypeInstance      *instance,
						 GType               iface_type);
gboolean         g_type_check_instance_is_a	(GTypeInstance      *instance,
						 GType               iface_type);
GTypeClass*      g_type_check_class_cast        (GTypeClass         *g_class,
						 GType               is_a_type);
gboolean         g_type_check_class_is_a        (GTypeClass         *g_class,
						 GType               is_a_type);
gboolean	 g_type_check_is_value_type     (GType		     type);
gboolean	 g_type_check_value             (GValue		    *value);
gboolean	 g_type_check_value_holds	(GValue		    *value,
						 GType		     type);
gboolean         g_type_test_flags              (GType               type,
						 guint               flags);


/* --- debugging functions --- */
G_CONST_RETURN gchar* g_type_name_from_instance	(GTypeInstance	*instance);
G_CONST_RETURN gchar* g_type_name_from_class	(GTypeClass	*g_class);


/* --- implementation bits --- */
#ifndef G_DISABLE_CAST_CHECKS
#  define _G_TYPE_CIC(ip, gt, ct) \
    ((ct*) g_type_check_instance_cast ((GTypeInstance*) ip, gt))
#  define _G_TYPE_CCC(cp, gt, ct) \
    ((ct*) g_type_check_class_cast ((GTypeClass*) cp, gt))
#else /* G_DISABLE_CAST_CHECKS */
#  define _G_TYPE_CIC(ip, gt, ct)       ((ct*) ip)
#  define _G_TYPE_CCC(cp, gt, ct)       ((ct*) cp)
#endif /* G_DISABLE_CAST_CHECKS */
#define _G_TYPE_CHI(ip)			(g_type_check_instance ((GTypeInstance*) ip))
#define _G_TYPE_CVH(vl, gt)             (g_type_check_value_holds ((GValue*) vl, gt))
#define _G_TYPE_CHV(vl)			(g_type_check_value ((GValue*) vl))
#define _G_TYPE_IGC(ip, gt, ct)         ((ct*) (((GTypeInstance*) ip)->g_class))
#define _G_TYPE_IGI(ip, gt, ct)         ((ct*) g_type_interface_peek (((GTypeInstance*) ip)->g_class, gt))
#ifdef	__GNUC__
#  define _G_TYPE_CIT(ip, gt)             (G_GNUC_EXTENSION ({ \
  GTypeInstance *__inst = (GTypeInstance*) ip; GType __t = gt; gboolean __r; \
  if (__inst && __inst->g_class && __inst->g_class->g_type == __t) \
    __r = TRUE; \
  else \
    __r = g_type_check_instance_is_a (__inst, __t); \
  __r; \
}))
#  define _G_TYPE_CCT(cp, gt)             (G_GNUC_EXTENSION ({ \
  GTypeClass *__class = (GTypeClass*) cp; GType __t = gt; gboolean __r; \
  if (__class && __class->g_type == __t) \
    __r = TRUE; \
  else \
    __r = g_type_check_class_is_a (__class, __t); \
  __r; \
}))
#else  /* !__GNUC__ */
#  define _G_TYPE_CIT(ip, gt)             (g_type_check_instance_is_a ((GTypeInstance*) ip, gt))
#  define _G_TYPE_CCT(cp, gt)             (g_type_check_class_is_a ((GTypeClass*) cp, gt))
#endif /* !__GNUC__ */
#define	G_TYPE_FLAG_RESERVED_ID_BIT	((GType) (1 << 0))
extern GTypeDebugFlags			_g_type_debug_flags;

G_END_DECLS

#endif /* __G_TYPE_H__ */
