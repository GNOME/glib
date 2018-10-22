#include "mock-resolver.h"

struct _MockResolver
{
  GResolver parent_instance;
  guint ipv4_delay;
  guint ipv6_delay;
  GList *ipv4_results;
  GList *ipv6_results;
  GError *ipv4_error;
  GError *ipv6_error;
};

G_DEFINE_TYPE (MockResolver, mock_resolver, G_TYPE_RESOLVER)

MockResolver *
mock_resolver_new (void)
{
  return g_object_new (MOCK_TYPE_RESOLVER, NULL);
}

void
mock_resolver_set_ipv4_delay (MockResolver *self, guint delay)
{
  self->ipv4_delay = delay;
}

static gpointer
copy_object (gconstpointer obj, gpointer user_data)
{
  return g_object_ref (G_OBJECT (obj));
}

void
mock_resolver_set_ipv4_results (MockResolver *self, GList *results)
{
  if (self->ipv4_results)
    g_list_free_full (self->ipv4_results, g_object_unref);
  self->ipv4_results = g_list_copy_deep (results, copy_object, NULL);
}

void
mock_resolver_set_ipv4_error (MockResolver *self, GError *error)
{
  g_clear_error (&self->ipv4_error);
  if (error)
    self->ipv4_error = g_error_copy (error);
}

void
mock_resolver_set_ipv6_delay (MockResolver *self, guint delay)
{
  self->ipv6_delay = delay;
}

void
mock_resolver_set_ipv6_results (MockResolver *self, GList *results)
{
  if (self->ipv6_results)
    g_list_free_full (self->ipv6_results, g_object_unref);
  self->ipv6_results = g_list_copy_deep (results, copy_object, NULL);
}

void
mock_resolver_set_ipv6_error (MockResolver *self, GError *error)
{
  g_clear_error (&self->ipv6_error);
  if (error)
    self->ipv6_error = g_error_copy (error);
}

static void
do_lookup_by_name (GTask         *task,
                   gpointer       source_object,
                   gpointer       task_data,
                   GCancellable  *cancellable)
{
  MockResolver *self = source_object;
  GResolverNameLookupFlags flags = GPOINTER_TO_UINT(task_data);

  if (flags == G_RESOLVER_LOOKUP_NAME_FLAGS_IPV4)
    {
      g_usleep (self->ipv4_delay * 1000);
      if (self->ipv4_error)
        g_task_return_error (task, g_error_copy (self->ipv4_error));
      else
        g_task_return_pointer (task, g_list_copy_deep (self->ipv4_results, copy_object, NULL), NULL);
    }
  else if (flags == G_RESOLVER_LOOKUP_NAME_FLAGS_IPV6)
    {
      g_usleep (self->ipv6_delay * 1000);
      if (self->ipv6_error)
        g_task_return_error (task, g_error_copy (self->ipv6_error));
      else
        g_task_return_pointer (task, g_list_copy_deep (self->ipv6_results, copy_object, NULL), NULL);
    }
  else
    g_assert_not_reached ();
}

static void
lookup_by_name_with_flags_async (GResolver                *resolver,
                                 const gchar              *hostname,
                                 GResolverNameLookupFlags  flags,
                                 GCancellable             *cancellable,
                                 GAsyncReadyCallback       callback,
                                 gpointer                  user_data)
{
  GTask *task = g_task_new (resolver, cancellable, callback, user_data);
  g_task_set_task_data (task, GUINT_TO_POINTER(flags), NULL);
  g_task_run_in_thread (task, do_lookup_by_name);
  g_object_unref (task);
}

static GList *
lookup_by_name_with_flags_finish (GResolver     *resolver,
                                  GAsyncResult  *result,
                                  GError       **error)
{
  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
mock_resolver_finalize (GObject *object)
{
  MockResolver *self = (MockResolver*)object;

  g_clear_error (&self->ipv4_error);
  g_clear_error (&self->ipv6_error);
  if (self->ipv6_results)
    g_list_free_full (self->ipv6_results, g_object_unref);
  if (self->ipv4_results)
    g_list_free_full (self->ipv4_results, g_object_unref);

  G_OBJECT_CLASS (mock_resolver_parent_class)->finalize (object);
}

static void
mock_resolver_class_init (MockResolverClass *klass)
{
  GResolverClass *resolver_class = G_RESOLVER_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  resolver_class->lookup_by_name_with_flags_async  = lookup_by_name_with_flags_async;
  resolver_class->lookup_by_name_with_flags_finish = lookup_by_name_with_flags_finish;
  object_class->finalize = mock_resolver_finalize;
}

static void
mock_resolver_init (MockResolver *self)
{
}
