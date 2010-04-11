#include "gmemorysettingsbackend.h"

G_DEFINE_TYPE_WITH_CODE (GMemorySettingsBackend,
                         g_memory_settings_backend,
                         G_TYPE_SETTINGS_BACKEND,
                         g_io_extension_point_implement (G_SETTINGS_BACKEND_EXTENSION_POINT_NAME,
                                                         g_define_type_id, "memory", 0))

struct _GMemorySettingsBackendPrivate
{
  GHashTable *table;
};

static GVariant *
g_memory_settings_backend_read (GSettingsBackend   *backend,
                                const gchar        *key,
                                const GVariantType *expected_type)
{
  GVariant *value;

  GMemorySettingsBackend *memory = G_MEMORY_SETTINGS_BACKEND (backend);

  value = g_hash_table_lookup (memory->priv->table, key);

  if (value != NULL)
    g_variant_ref (value);

  return value;
}

static gboolean
g_memory_settings_backend_write_one (gpointer key,
                                     gpointer value,
                                     gpointer data)
{
  GMemorySettingsBackend *memory = ((gpointer *) data)[0];
  const gchar *prefix = ((gpointer *) data)[1];

  g_hash_table_insert (memory->priv->table,
                       g_strjoin ("", prefix, key, NULL),
                       g_variant_ref (value));

  return FALSE;
}

static void
g_memory_settings_backend_write (GSettingsBackend *backend,
                                 const gchar      *prefix,
                                 GTree            *tree,
                                 gpointer          origin_tag)
{
  gpointer write_info[] = { backend, (gpointer) prefix };
  g_tree_foreach (tree, g_memory_settings_backend_write_one, write_info);
  g_settings_backend_changed_tree (backend, prefix, tree, origin_tag);
}

static gboolean
g_memory_settings_backend_get_writable (GSettingsBackend *backend,
                                        const gchar      *name)
{
  return TRUE;
}

static void
g_memory_settings_backend_finalize (GObject *object)
{
  GMemorySettingsBackend *memory = G_MEMORY_SETTINGS_BACKEND (object);

  g_hash_table_unref (memory->priv->table);

  G_OBJECT_CLASS (g_memory_settings_backend_parent_class)
    ->finalize (object);
}

static void
g_memory_settings_backend_init (GMemorySettingsBackend *memory)
{
  memory->priv = G_TYPE_INSTANCE_GET_PRIVATE (memory,
                                              G_TYPE_MEMORY_SETTINGS_BACKEND,
                                              GMemorySettingsBackendPrivate);
  memory->priv->table =
    g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                           (GDestroyNotify) g_variant_unref);
}

static void
g_memory_settings_backend_class_init (GMemorySettingsBackendClass *class)
{
  GSettingsBackendClass *backend_class = G_SETTINGS_BACKEND_CLASS (class);
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  backend_class->read = g_memory_settings_backend_read;
  backend_class->write = g_memory_settings_backend_write;
  backend_class->get_writable = g_memory_settings_backend_get_writable;
  object_class->finalize = g_memory_settings_backend_finalize;

  g_type_class_add_private (class, sizeof (GMemorySettingsBackendPrivate));
}
