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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include	"gobject.h"


#include	"gvalue.h"
#include	"gvaluecollector.h"


#define	DEBUG_OBJECTS


/* --- macros --- */
#define PARAM_SPEC_PARAM_ID(pspec)	(GPOINTER_TO_UINT (g_param_spec_get_qdata ((pspec), quark_param_id)))


/* --- prototypes --- */
extern void	g_object_type_init			(void);
static void	g_object_base_class_init		(GObjectClass	*class);
static void	g_object_base_class_finalize		(GObjectClass	*class);
static void	g_object_do_class_init			(GObjectClass	*class);
static void	g_object_do_init			(GObject	*object);
static void	g_object_do_queue_param_changed 	(GObject        *object,
							 GParamSpec     *pspec);
static void	g_object_do_dispatch_param_changed	(GObject        *object,
							 GParamSpec     *pspec);
static void	g_object_last_unref			(GObject	*object);
static void	g_object_do_shutdown			(GObject	*object);
static void	g_object_do_finalize			(GObject	*object);


/* --- variables --- */
static GQuark		 quark_param_id = 0;
static GQuark		 quark_param_changed_queue = 0;
static GHashTable	*param_spec_hash_table = NULL;


/* --- functions --- */
#ifdef	DEBUG_OBJECTS
static guint		  debug_objects_count = 0;
static GHashTable	 *debug_objects_ht = NULL;
static void
debug_objects_foreach (gpointer key,
		       gpointer value,
		       gpointer user_data)
{
  GObject *object = value;

  g_message ("[%p] stale %s\tref_count=%u",
	     object,
	     G_OBJECT_TYPE_NAME (object),
	     object->ref_count);
}
static void
debug_objects_atexit (void)
{
  if (debug_objects_ht)
    {
      g_message ("stale GObjects: %u", debug_objects_count);
      g_hash_table_foreach (debug_objects_ht, debug_objects_foreach, NULL);
    }
}
#endif	DEBUG_OBJECTS

void
g_object_type_init (void)	/* sync with glib-gtype.c */
{
  static gboolean initialized = FALSE;
  static const GTypeFundamentalInfo finfo = {
    G_TYPE_FLAG_CLASSED | G_TYPE_FLAG_INSTANTIATABLE | G_TYPE_FLAG_DERIVABLE | G_TYPE_FLAG_DEEP_DERIVABLE,
    0       /* n_collect_bytes */,
    NULL    /* GTypeParamCollector */,
  };
  static GTypeInfo info = {
    sizeof (GObjectClass),
    (GBaseInitFunc) g_object_base_class_init,
    (GBaseFinalizeFunc) g_object_base_class_finalize,
    (GClassInitFunc) g_object_do_class_init,
    NULL	/* class_destroy */,
    NULL	/* class_data */,
    sizeof (GObject),
    0		/* n_preallocs */,
    (GInstanceInitFunc) g_object_do_init,
  };
  GType type;
  
  g_return_if_fail (initialized == FALSE);
  initialized = TRUE;
  
  /* G_TYPE_OBJECT
   */
  type = g_type_register_fundamental (G_TYPE_OBJECT, "GObject", &finfo, &info);
  g_assert (type == G_TYPE_OBJECT);

#ifdef  DEBUG_OBJECTS
  g_atexit (debug_objects_atexit);
#endif  DEBUG_OBJECTS
}

static void
g_object_base_class_init (GObjectClass *class)
{
  /* reset instance specific fields and methods that don't get inherited */
  class->n_param_specs = 0;
  class->param_specs = NULL;
  class->get_param = NULL;
  class->set_param = NULL;
}

static void
g_object_base_class_finalize (GObjectClass *class)
{
  guint i;

  g_message ("finallizing base class of %s", G_OBJECT_CLASS_NAME (class));
  
  for (i = 0; i < class->n_param_specs; i++)
    {
      GParamSpec *pspec = class->param_specs[i];
      
      g_param_spec_hash_table_remove (param_spec_hash_table, pspec);
      g_param_spec_set_qdata (pspec, quark_param_id, NULL);
      g_param_spec_unref (pspec);
    }
  class->n_param_specs = 0;
  g_free (class->param_specs);
  class->param_specs = NULL;
}

static void
g_object_do_class_init (GObjectClass *class)
{
  quark_param_id = g_quark_from_static_string ("glib-object-param-id");
  quark_param_changed_queue = g_quark_from_static_string ("glib-object-param-changed-queue");
  param_spec_hash_table = g_param_spec_hash_table_new ();
  
  class->queue_param_changed = g_object_do_queue_param_changed;
  class->dispatch_param_changed = g_object_do_dispatch_param_changed;
  class->shutdown = g_object_do_shutdown;
  class->finalize = g_object_do_finalize;
}

void
g_object_class_install_param (GObjectClass *class,
			      guint         param_id,
			      GParamSpec   *pspec /* 1 ref_count taken over */)
{
  guint i;

  g_return_if_fail (G_IS_OBJECT_CLASS (class));
  g_return_if_fail (G_IS_PARAM_SPEC (pspec));
  if (pspec->flags & G_PARAM_WRITABLE)
    g_return_if_fail (class->set_param != NULL);
  if (pspec->flags & G_PARAM_READABLE)
    g_return_if_fail (class->get_param != NULL);
  g_return_if_fail (param_id > 0);
  g_return_if_fail (PARAM_SPEC_PARAM_ID (pspec) == 0);	/* paranoid */

  /* expensive paranoia checks ;( */
  for (i = 0; i < class->n_param_specs; i++)
    if (PARAM_SPEC_PARAM_ID (class->param_specs[i]) == param_id)
      {
	g_warning (G_STRLOC ": class `%s' already contains a parameter `%s' with id %u, "
		   "cannot install parameter `%s'",
		   G_OBJECT_CLASS_NAME (class),
		   class->param_specs[i]->name,
		   param_id,
		   pspec->name);
	return;
      }
  if (g_object_class_find_param_spec (class, pspec->name))
    {
      g_warning (G_STRLOC ": class `%s' already contains a parameter named `%s'",
		 G_OBJECT_CLASS_NAME (class),
		 pspec->name);
      return;
    }

  g_param_spec_set_qdata (pspec, quark_param_id, GUINT_TO_POINTER (param_id));
  g_param_spec_hash_table_insert (param_spec_hash_table, pspec, G_OBJECT_CLASS_TYPE (class));
  i = class->n_param_specs++;
  class->param_specs = g_renew (GParamSpec*, class->param_specs, class->n_param_specs);
  class->param_specs[i] = pspec;
}

GParamSpec*
g_object_class_find_param_spec (GObjectClass *class,
				const gchar  *param_name)
{
  g_return_val_if_fail (G_IS_OBJECT_CLASS (class), NULL);
  g_return_val_if_fail (param_name != NULL, NULL);

  return g_param_spec_hash_table_lookup (param_spec_hash_table,
					 param_name,
					 G_OBJECT_CLASS_TYPE (class),
					 TRUE, NULL);
}

static void
g_object_do_init (GObject *object)
{
  object->ref_count = 1;
  object->qdata = NULL;

#ifdef	DEBUG_OBJECTS
  if (!debug_objects_ht)
    debug_objects_ht = g_hash_table_new (g_direct_hash, NULL);
  debug_objects_count++;
  g_hash_table_insert (debug_objects_ht, object, object);
#endif	DEBUG_OBJECTS
}

static void
g_object_last_unref (GObject *object)
{
  g_return_if_fail (object->ref_count > 0);

  if (object->ref_count == 1)	/* may have been re-referenced meanwhile */
    G_OBJECT_GET_CLASS (object)->shutdown (object);

  object->ref_count -= 1;

  if (object->ref_count == 0)	/* may have been re-referenced meanwhile */
    G_OBJECT_GET_CLASS (object)->finalize (object);
}

static void
g_object_do_shutdown (GObject *object)
{
  /* this function needs to be always present for unconditional
   * chaining, we also might add some code here later.
   * beware though, subclasses may invoke shutdown() arbitrarily.
   */
}

static void
g_object_do_finalize (GObject *object)
{
  g_datalist_clear (&object->qdata);
  
#ifdef	DEBUG_OBJECTS
  g_assert (g_hash_table_lookup (debug_objects_ht, object) == object);
  
  g_hash_table_remove (debug_objects_ht, object);
  debug_objects_count--;
#endif	DEBUG_OBJECTS
  
  g_type_free_instance ((GTypeInstance*) object);
}

gpointer
g_object_new (GType        object_type,
	      const gchar *first_param_name,
	      ...)
{
  GObject *object;
  va_list var_args;

  g_return_val_if_fail (G_TYPE_IS_OBJECT (object_type), NULL);

  va_start (var_args, first_param_name);
  object = g_object_new_valist (object_type, first_param_name, var_args);
  va_end (var_args);

  return object;
}

gpointer
g_object_new_valist (GType        object_type,
		     const gchar *first_param_name,
		     va_list      var_args)
{
  GObject *object;

  g_return_val_if_fail (G_TYPE_IS_OBJECT (object_type), NULL);

  object = (GObject*) g_type_create_instance (object_type);
  if (first_param_name)
    g_object_set_valist (object, first_param_name, var_args);
  
  return object;
}

static void
g_object_do_dispatch_param_changed (GObject    *object,
				    GParamSpec *pspec)
{
  g_message ("NOTIFICATION: parameter `%s' changed on object `%s'",
	     pspec->name,
	     G_OBJECT_TYPE_NAME (object));
}

static gboolean
notify_param_changed_handler (gpointer data)
{
  GObject *object;
  GObjectClass *class;
  GSList *slist;

  /* FIXME: need GDK_THREADS lock */

  object = G_OBJECT (data);
  class = G_OBJECT_GET_CLASS (object);
  slist = g_datalist_id_get_data (&object->qdata, quark_param_changed_queue);

  /* a reference count is still being held */

  for (; slist; slist = slist->next)
    if (slist->data)
      {
	GParamSpec *pspec = slist->data;
	
	slist->data = NULL;
	class->dispatch_param_changed (object, pspec);
      }
  
  g_datalist_id_set_data (&object->qdata, quark_param_changed_queue, NULL);
  
  return FALSE;
}

static void
g_object_do_queue_param_changed (GObject    *object,
				 GParamSpec *pspec)
{
  GSList *slist, *last = NULL;
  
  /* if this is a recursive call on this object (i.e. pspecs are queued
   * for notification, while param_changed notification is currently in
   * progress), we simply add them to the queue that is currently being
   * dispatched. otherwise, we later dispatch parameter changed notification
   * asyncronously from an idle handler untill the queue is completely empty.
   */
  
  slist = g_datalist_id_get_data (&object->qdata, quark_param_changed_queue);
  for (; slist; last = slist, slist = last->next)
    if (slist->data == pspec)
      return;
  
  if (!last)
    {
      g_object_ref (object);
      g_idle_add_full (G_NOTIFY_PRIORITY,
		       notify_param_changed_handler,
		       object,
		       (GDestroyNotify) g_object_unref);
      g_object_set_qdata_full (object,
			       quark_param_changed_queue,
			       g_slist_prepend (NULL, pspec),
			       (GDestroyNotify) g_slist_free);
    }
  else
    last->next = g_slist_prepend (NULL, pspec);
}

static inline void
object_get_param (GObject     *object,
		  GValue      *value,
		  GParamSpec  *pspec,
		  const gchar *trailer)
{
  GObjectClass *class;

  g_return_if_fail (g_type_is_a (G_OBJECT_TYPE (object), pspec->owner_type));	/* paranoid */

  class = g_type_class_peek (pspec->owner_type);

  class->get_param (object, PARAM_SPEC_PARAM_ID (pspec), value, pspec, trailer);
}

static inline void
object_set_param (GObject     *object,
		  GValue      *value,
		  GParamSpec  *pspec,
		  const gchar *trailer)
{
  GObjectClass *class;

  g_return_if_fail (g_type_is_a (G_OBJECT_TYPE (object), pspec->owner_type));	/* paranoid */

  class = g_type_class_peek (pspec->owner_type);

  class->set_param (object, PARAM_SPEC_PARAM_ID (pspec), value, pspec, trailer);

  class->queue_param_changed (object, pspec);
}

void
g_object_set_valist (GObject     *object,
		     const gchar *first_param_name,
		     va_list      var_args)
{
  const gchar *name;

  g_return_if_fail (G_IS_OBJECT (object));

  g_object_ref (object);

  name = first_param_name;

  while (name)
    {
      const gchar *trailer = NULL;
      GValue value = { 0, };
      GParamSpec *pspec;
      gchar *error = NULL;

      pspec = g_param_spec_hash_table_lookup (param_spec_hash_table,
					      name,
					      G_OBJECT_TYPE (object),
					      TRUE,
					      &trailer);
      if (!pspec)
	{
	  g_warning ("%s: object class `%s' has no parameter named `%s'",
		     G_STRLOC,
		     G_OBJECT_TYPE_NAME (object),
		     name);
	  break;
	}
      if (!(pspec->flags & G_PARAM_WRITABLE))
	{
	  g_warning ("%s: parameter `%s' of object class `%s' is not writable",
		     G_STRLOC,
		     pspec->name,
		     G_OBJECT_TYPE_NAME (object));
	  break;
	}

      g_value_init (&value, G_PARAM_SPEC_TYPE (pspec));

      G_PARAM_COLLECT_VALUE (&value, pspec, var_args, &error);
      if (error)
	{
	  g_warning ("%s: %s", G_STRLOC, error);
	  g_free (error);
	  
	  /* we purposely leak the value here, it might not be
	   * in a sane state if an error condition occoured
	   */
	  break;
	}

      object_set_param (object, &value, pspec, trailer);
      
      g_value_unset (&value);

      name = va_arg (var_args, gchar*);
    }

  g_object_unref (object);
}

void
g_object_get_valist (GObject     *object,
		     const gchar *first_param_name,
		     va_list      var_args)
{
  const gchar *name;

  g_return_if_fail (G_IS_OBJECT (object));

  g_object_ref (object);

  name = first_param_name;

  while (name)
    {
      const gchar *trailer = NULL;
      GValue value = { 0, };
      GParamSpec *pspec;
      gchar *error = NULL;

      pspec = g_param_spec_hash_table_lookup (param_spec_hash_table,
					      name,
					      G_OBJECT_TYPE (object),
					      TRUE,
					      &trailer);
      if (!pspec)
	{
	  g_warning ("%s: object class `%s' has no parameter named `%s'",
		     G_STRLOC,
		     G_OBJECT_TYPE_NAME (object),
		     name);
	  break;
	}
      if (!(pspec->flags & G_PARAM_READABLE))
	{
	  g_warning ("%s: parameter `%s' of object class `%s' is not readable",
		     G_STRLOC,
		     pspec->name,
		     G_OBJECT_TYPE_NAME (object));
	  break;
	}

      g_value_init (&value, G_PARAM_SPEC_TYPE (pspec));

      object_get_param (object, &value, pspec, trailer);

      G_PARAM_LCOPY_VALUE (&value, pspec, var_args, &error);
      if (error)
	{
	  g_warning ("%s: %s", G_STRLOC, error);
	  g_free (error);
	  
	  /* we purposely leak the value here, it might not be
	   * in a sane state if an error condition occoured
	   */
	  break;
	}
      
      g_value_unset (&value);

      name = va_arg (var_args, gchar*);
    }

  g_object_unref (object);
}

void
g_object_set (GObject     *object,
	      const gchar *first_param_name,
	      ...)
{
  va_list var_args;

  g_return_if_fail (G_IS_OBJECT (object));

  va_start (var_args, first_param_name);
  g_object_set_valist (object, first_param_name, var_args);
  va_end (var_args);
}

void
g_object_get (GObject     *object,
	      const gchar *first_param_name,
	      ...)
{
  va_list var_args;

  g_return_if_fail (G_IS_OBJECT (object));

  va_start (var_args, first_param_name);
  g_object_get_valist (object, first_param_name, var_args);
  va_end (var_args);
}

void
g_object_set_param (GObject      *object,
		    const gchar  *param_name,
		    const GValue *value)
{
  GParamSpec *pspec;
  const gchar *trailer;

  g_return_if_fail (G_IS_OBJECT (object));
  g_return_if_fail (param_name != NULL);
  g_return_if_fail (G_IS_VALUE (value));

  g_object_ref (object);

  pspec = g_param_spec_hash_table_lookup (param_spec_hash_table,
					  param_name,
					  G_OBJECT_TYPE (object),
					  TRUE,
					  &trailer);
  if (!pspec)
    g_warning ("%s: object class `%s' has no parameter named `%s'",
	       G_STRLOC,
	       G_OBJECT_TYPE_NAME (object),
	       param_name);
  else
    {
      GValue tmp_value = { 0, };

      /* provide a copy to work from and convert if necessary */
      g_value_init (&tmp_value, G_PARAM_SPEC_TYPE (pspec));

      if (!g_value_convert (value, &tmp_value) ||
	  g_value_validate (&tmp_value, pspec))
	g_warning ("%s: can't convert `%s' value to parameter `%s' of type `%s'",
		   G_STRLOC,
		   G_VALUE_TYPE_NAME (value),
		   pspec->name,
		   G_PARAM_SPEC_TYPE_NAME (pspec));
      else
	object_set_param (object, &tmp_value, pspec, trailer);
      
      g_value_unset (&tmp_value);
    }
  
  g_object_unref (object);
}

void
g_object_get_param (GObject     *object,
		    const gchar *param_name,
		    GValue      *value)
{
  GParamSpec *pspec;
  const gchar *trailer;
  
  g_return_if_fail (G_IS_OBJECT (object));
  g_return_if_fail (param_name != NULL);
  g_return_if_fail (G_IS_VALUE (value));
  
  g_object_ref (object);
  
  pspec = g_param_spec_hash_table_lookup (param_spec_hash_table,
					  param_name,
					  G_OBJECT_TYPE (object),
					  TRUE,
					  &trailer);
  if (!pspec)
    g_warning ("%s: object class `%s' has no parameter named `%s'",
	       G_STRLOC,
	       G_OBJECT_TYPE_NAME (object),
	       param_name);
  else
    {
      GValue tmp_value = { 0, };
      
      /* provide a copy to work from and later convert if necessary, so
       * _get_param() implementations need *not* care about freeing values
       * that might be already set in the parameter to get.
       * (though, at this point, GValue should exclusively be modified
       * through the accessor functions anyways)
       */
      g_value_init (&tmp_value, G_PARAM_SPEC_TYPE (pspec));
      
      if (!g_value_types_exchangable (G_VALUE_TYPE (value), G_PARAM_SPEC_TYPE (pspec)))
	g_warning ("%s: can't retrive parameter `%s' of type `%s' as value of type `%s'",
		   G_STRLOC,
		   pspec->name,
		   G_PARAM_SPEC_TYPE_NAME (pspec),
		   G_VALUE_TYPE_NAME (value));
      else
	{
	  object_get_param (object, &tmp_value, pspec, trailer);
	  g_value_convert (&tmp_value, value);
	  /* g_value_validate (value, pspec); */
	}
      
      g_value_unset (&tmp_value);
    }
  
  g_object_unref (object);
}

void
g_object_queue_param_changed (GObject     *object,
			      const gchar *param_name)
{
  GParamSpec *pspec;
  
  g_return_if_fail (G_IS_OBJECT (object));
  g_return_if_fail (param_name != NULL);

  pspec = g_param_spec_hash_table_lookup (param_spec_hash_table,
					  param_name,
					  G_OBJECT_TYPE (object),
					  TRUE, NULL);
  if (!pspec)
    g_warning ("%s: object class `%s' has no parameter named `%s'",
	       G_STRLOC,
	       G_OBJECT_TYPE_NAME (object),
	       param_name);
  else
    G_OBJECT_GET_CLASS (object)->queue_param_changed (object, pspec);
}

GObject*
g_object_ref (GObject *object)
{
  g_return_val_if_fail (G_IS_OBJECT (object), NULL);
  g_return_val_if_fail (object->ref_count > 0, NULL);

  object->ref_count += 1;

  return object;
}

void
g_object_unref (GObject *object)
{
  g_return_if_fail (G_IS_OBJECT (object));
  g_return_if_fail (object->ref_count > 0);

  if (object->ref_count > 1)
    object->ref_count -= 1;
  else
    g_object_last_unref (object);
}

gpointer
g_object_get_qdata (GObject *object,
		    GQuark   quark)
{
  g_return_val_if_fail (G_IS_OBJECT (object), NULL);

  return quark ? g_datalist_id_get_data (&object->qdata, quark) : NULL;
}

void
g_object_set_qdata (GObject *object,
		    GQuark   quark,
		    gpointer data)
{
  g_return_if_fail (G_IS_OBJECT (object));
  g_return_if_fail (quark > 0);
  
  g_datalist_id_set_data (&object->qdata, quark, data);
}

void
g_object_set_qdata_full (GObject       *object,
			 GQuark         quark,
			 gpointer       data,
			 GDestroyNotify destroy)
{
  g_return_if_fail (G_IS_OBJECT (object));
  g_return_if_fail (quark > 0);
  
  g_datalist_id_set_data_full (&object->qdata, quark, data, data ? destroy : NULL);
}

gpointer
g_object_steal_qdata (GObject *object,
		      GQuark   quark)
{
  g_return_val_if_fail (G_IS_OBJECT (object), NULL);
  g_return_val_if_fail (quark > 0, NULL);
  
  return g_datalist_id_remove_no_notify (&object->qdata, quark);
}
