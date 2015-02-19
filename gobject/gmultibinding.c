#include "config.h"

#include <string.h>

#include "gmultibinding.h"
#include "genums.h"
#include "gmarshal.h"
#include "gobject.h"
#include "gsignal.h"
#include "gparamspecs.h"
#include "gvaluetypes.h"

#include "glibintl.h"


#define G_MULTI_BINDING_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), G_TYPE_MULTI_BINDING, GMultiBindingClass))
#define G_IS_MULTI_BINDING_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), G_TYPE_MULTI_BINDING))
#define G_MULTI_BINDING_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), G_TYPE_MULTI_BINDING, GMultiBindingClass))

typedef struct _GMultiBindingClass      GMultiBindingClass;

struct _GMultiBinding
{
  GObject parent_instance;

  gint n_sources;
  gint n_targets;

  /* no reference is held on the objects, to avoid cycles */
  GObject **source;
  GObject **target;

  /* the property names are interned, so they should not be freed */
  GParamSpec **source_pspec;
  GParamSpec **target_pspec;

  GMultiBindingTransformFunc transform;
  gpointer transform_data;
  GDestroyNotify notify;

  guint *source_notify;

  /* a guard, to avoid loops */
  guint is_frozen : 1;
};

struct _GMultiBindingClass
{
  GObjectClass parent_class;
};

static GQuark quark_gbinding = 0;

G_DEFINE_TYPE (GMultiBinding, g_multi_binding, G_TYPE_OBJECT);

static inline void
add_binding_qdata (GObject       *gobject,
                   GMultiBinding *binding)
{
  GHashTable *bindings;

  bindings = g_object_get_qdata (gobject, quark_gbinding);
  if (bindings == NULL)
    {
      bindings = g_hash_table_new (NULL, NULL);

      g_object_set_qdata_full (gobject, quark_gbinding,
                               bindings,
                               (GDestroyNotify) g_hash_table_unref);
    }

  g_hash_table_add (bindings, binding);
}

static inline gboolean
has_binding_qdata (GObject       *object,
                   GMultiBinding *binding)
{
  GHashTable *bindings;

  bindings = g_object_get_qdata (object, quark_gbinding);
  if (bindings)
    return g_hash_table_contains (bindings, binding);

  return FALSE;
}

static inline void
remove_binding_qdata (GObject       *gobject,
                      GMultiBinding *binding)
{
  GHashTable *bindings;

  bindings = g_object_get_qdata (gobject, quark_gbinding);
  if (binding != NULL)
    g_hash_table_remove (bindings, binding);
}

/* the basic assumption is that if either the source or the target
 * goes away then the binding does not exist any more and it should
 * be reaped as well
 */
static void
weak_unbind (gpointer  user_data,
             GObject  *where_the_object_was)
{
  GMultiBinding *binding = user_data;
  gint i;

  /* if what went away was a source, unset it so that GBinding::finalize
   * does not try to access it; otherwise, disconnect everything and remove
   * the GBinding instance from the object's qdata
   */
  for (i = 0; i < binding->n_sources; i++)
    {
      if (binding->source[i] == where_the_object_was)
        binding->source[i] = NULL;
      else
        {
          if (binding->source_notify[i] != 0)
            g_signal_handler_disconnect (binding->source[i], binding->source_notify[i]);

          g_object_weak_unref (binding->source[i], weak_unbind, user_data);
          remove_binding_qdata (binding->source[i], binding);

          binding->source_notify[i] = 0;
          binding->source[i] = NULL;
        }
    }

  /* as above, but with the targets */
  for (i = 0; i < binding->n_targets; i++)
    {
      if (binding->target[i] == where_the_object_was)
        binding->target[i] = NULL;
      else
        {
          g_object_weak_unref (binding->target[i], weak_unbind, user_data);
          remove_binding_qdata (binding->target[i], binding);
          binding->target[i] = NULL;
        }
    }

  /* this will take care of the binding itself */
  g_object_unref (binding);
}

static void
on_source_notify (GObject       *gobject,
                  GParamSpec    *pspec,
                  GMultiBinding *binding)
{
  GValue *from_values;
  GValue *to_values;
  gboolean res;
  gint i;

  if (binding->is_frozen)
    return;

  from_values = g_new0 (GValue, binding->n_sources);
  for (i = 0; i < binding->n_sources; i++)
    {
      g_value_init (&from_values[i], G_PARAM_SPEC_VALUE_TYPE (binding->source_pspec[i]));
      g_object_get_property (binding->source[i], binding->source_pspec[i]->name, &from_values[i]);
    }

  to_values = g_new0 (GValue, binding->n_targets);
  for (i = 0; i < binding->n_targets; i++)
    {
      g_value_init (&to_values[i], G_PARAM_SPEC_VALUE_TYPE (binding->target_pspec[i]));
      g_object_get_property (binding->target[i], binding->target_pspec[i]->name, &to_values[i]);
    }

  res = binding->transform (binding, (const GValue *)from_values, to_values, binding->transform_data);

  if (res)
    {
      binding->is_frozen = TRUE;
      for (i = 0; i < binding->n_targets; i++)
        {
          g_param_value_validate (binding->target_pspec[i], &to_values[i]);
          g_object_set_property (binding->target[i], binding->target_pspec[i]->name, &to_values[i]);
        }
      binding->is_frozen = FALSE;
    }

  for (i = 0; i < binding->n_sources; i++)
    g_value_unset (&from_values[i]);
  g_free (from_values);

  for (i = 0; i < binding->n_targets; i++)
    g_value_unset (&to_values[i]);
  g_free (to_values);
}

static inline void
g_multi_binding_unbind_internal (GMultiBinding *binding,
                                 gboolean       unref_binding)
{
  gint i;

  /* dispose of the transformation data */
  if (binding->notify != NULL)
    {
      binding->notify (binding->transform_data);

      binding->transform_data = NULL;
      binding->notify = NULL;
    }

  for (i = 0; i < binding->n_sources; i++)
    {
      if (binding->source[i] != NULL)
        {
          if (binding->source_notify[i] != 0)
            g_signal_handler_disconnect (binding->source[i], binding->source_notify[i]);

          g_object_weak_unref (binding->source[i], weak_unbind, binding);
          remove_binding_qdata (binding->source[i], binding);

          binding->source_notify[i] = 0;
          binding->source[i] = NULL;
        }
    }

  for (i = 0; i < binding->n_targets; i++)
    {
      if (binding->target[i] != NULL)
        {
          g_object_weak_unref (binding->target[i], weak_unbind, binding);
          remove_binding_qdata (binding->target[i], binding);
          binding->target[i] = NULL;
        }
    }

  if (unref_binding)
    g_object_unref (binding);
}

static void
g_multi_binding_finalize (GObject *gobject)
{
  GMultiBinding *binding = G_MULTI_BINDING (gobject);

  g_multi_binding_unbind_internal (binding, FALSE);
  g_free (binding->source);
  g_free (binding->source_pspec);
  g_free (binding->source_notify);
  g_free (binding->target);
  g_free (binding->target_pspec);

  G_OBJECT_CLASS (g_multi_binding_parent_class)->finalize (gobject);
}

static void
g_multi_binding_class_init (GMultiBindingClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  quark_gbinding = g_quark_from_static_string ("g-multi-binding");

  gobject_class->finalize = g_multi_binding_finalize;
}

static void
g_multi_binding_init (GMultiBinding *binding)
{
}

gint
g_multi_binding_get_n_sources (GMultiBinding *binding)
{
  g_return_val_if_fail (G_IS_MULTI_BINDING (binding), 0);

  return binding->n_sources;
}

GObject *
g_multi_binding_get_source (GMultiBinding *binding,
                            gint           idx)
{
  g_return_val_if_fail (G_IS_MULTI_BINDING (binding), NULL);
  g_return_val_if_fail (0 <= idx && idx < binding->n_sources, NULL);

  return binding->source[idx];
}

const gchar *
g_multi_binding_get_source_property (GMultiBinding *binding,
                                     gint           idx)
{
  g_return_val_if_fail (G_IS_MULTI_BINDING (binding), NULL);
  g_return_val_if_fail (0 <= idx && idx < binding->n_sources, NULL);

  return binding->source_pspec[idx]->name;
}

gint
g_multi_binding_get_n_targets (GMultiBinding *binding)
{
  g_return_val_if_fail (G_IS_MULTI_BINDING (binding), 0);

  return binding->n_targets;
}

GObject *
g_multi_binding_get_target (GMultiBinding *binding,
                            gint           idx)
{
  g_return_val_if_fail (G_IS_MULTI_BINDING (binding), NULL);
  g_return_val_if_fail (0 <= idx && idx < binding->n_targets, NULL);

  return binding->target[idx];
}

const gchar *
g_multi_binding_get_target_property (GMultiBinding *binding,
                                     gint           idx)
{
  g_return_val_if_fail (G_IS_MULTI_BINDING (binding), NULL);
  g_return_val_if_fail (0 <= idx && idx < binding->n_targets, NULL);

  return binding->target_pspec[idx]->name;
}

void
g_multi_binding_unbind (GMultiBinding *binding)
{
  g_return_if_fail (G_IS_MULTI_BINDING (binding));

  g_multi_binding_unbind_internal (binding, TRUE);
}

GMultiBinding *
g_object_multi_bind_property_v (gint                        n_sources,
                                GObject                    *sources[],
                                const gchar                *source_properties[],
                                gint                        n_targets,
                                GObject                    *targets[],
                                const gchar                *target_properties[],
                                GMultiBindingTransformFunc  transform,
                                gpointer                    user_data,
                                GDestroyNotify              notify)
{
  GMultiBinding *binding;
  GParamSpec *pspec;
  gint i;
  gchar *signal;

  /* FIXME: don't look up pspecs twice */
  for (i = 0; i < n_sources; i++)
    {
      pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (sources[i]), source_properties[i]);
      if (pspec == NULL)
        {
          g_warning ("%s: The source object %d of type %s has no property called '%s'",
                     G_STRLOC,
                     i,
                     G_OBJECT_TYPE_NAME (sources[i]),
                     source_properties[i]);
          return NULL;
        }

      if (!(pspec->flags & G_PARAM_READABLE))
        {
          g_warning ("%s: The source object %d of type %s has no readable property called '%s'",
                     G_STRLOC,
                     i,
                     G_OBJECT_TYPE_NAME (sources[i]),
                     source_properties[i]);
          return NULL;
        }
    }

  for (i = 0; i < n_targets; i++)
    {
      pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (targets[i]), target_properties[i]);
      if (pspec == NULL)
        {
          g_warning ("%s: The target object %d of type %s has no property called '%s'",
                     G_STRLOC,
                     i,
                     G_OBJECT_TYPE_NAME (targets[i]),
                     target_properties[i]);
          return NULL;
        }

      if ((pspec->flags & G_PARAM_CONSTRUCT_ONLY) || !(pspec->flags & G_PARAM_WRITABLE))
        {
          g_warning ("%s: The target object %d of type %s has no writable property called '%s'",
                     G_STRLOC,
                     i,
                     G_OBJECT_TYPE_NAME (targets[i]),
                     target_properties[i]);
          return NULL;
        }
    }

  binding = g_object_new (G_TYPE_MULTI_BINDING, NULL);

  binding->transform = transform;
  binding->transform_data = user_data;
  binding->notify = notify;

  binding->n_sources = n_sources;

  binding->source = g_new (GObject *, n_sources);
  binding->source_pspec = g_new (GParamSpec *, n_sources);
  binding->source_notify = g_new (guint, n_sources);

  for (i = 0; i < n_sources; i++)
    {
      pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (sources[i]), source_properties[i]);
      binding->source[i] = sources[i];
      binding->source_pspec[i] = pspec;

      signal = g_strconcat ("notify::", source_properties[i], NULL);
      binding->source_notify[i] = g_signal_connect (binding->source[i], signal,
                                                    G_CALLBACK (on_source_notify), binding);
      g_free (signal);
      if (!has_binding_qdata (binding->source[i], binding))
        {
          g_object_weak_ref (binding->source[i], weak_unbind, binding);
          add_binding_qdata (binding->source[i], binding);
        }
    }

  binding->n_targets = n_targets;
  binding->target = g_new (GObject *, n_targets);
  binding->target_pspec = g_new (GParamSpec *, n_targets);

  for (i = 0; i < n_targets; i++)
    {
      pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (targets[i]), target_properties[i]);
      binding->target[i] = targets[i];
      binding->target_pspec[i] = pspec;

      if (!has_binding_qdata (binding->target[i], binding))
        {
          g_object_weak_ref (binding->target[i], weak_unbind, binding);
          add_binding_qdata (binding->target[i], binding);
        }
    }

  return binding;
}
