#ifndef _gmemorysettingsbackend_h_
#define _gmemorysettingsbackend_h_

#define G_SETTINGS_ENABLE_BACKEND
#include <gio/gsettingsbackend.h>

G_BEGIN_DECLS

#define G_TYPE_MEMORY_SETTINGS_BACKEND                      (g_memory_settings_backend_get_type ())
#define G_MEMORY_SETTINGS_BACKEND(inst)                     (G_TYPE_CHECK_INSTANCE_CAST ((inst),                     \
                                                             G_TYPE_MEMORY_SETTINGS_BACKEND,                         \
                                                             GMemorySettingsBackend))
#define G_MEMORY_SETTINGS_BACKEND_CLASS(class)              (G_TYPE_CHECK_CLASS_CAST ((class),                       \
                                                             G_TYPE_MEMORY_SETTINGS_BACKEND,                         \
                                                             GMemorySettingsBackendClass))
#define G_IS_MEMORY_SETTINGS_BACKEND(inst)                  (G_TYPE_CHECK_INSTANCE_TYPE ((inst),                     \
                                                             G_TYPE_MEMORY_SETTINGS_BACKEND))
#define G_IS_MEMORY_SETTINGS_BACKEND_CLASS(class)           (G_TYPE_CHECK_CLASS_TYPE ((class),                       \
                                                             G_TYPE_MEMORY_SETTINGS_BACKEND))
#define G_MEMORY_SETTINGS_BACKEND_GET_CLASS(inst)           (G_TYPE_INSTANCE_GET_CLASS ((inst),                      \
                                                             G_TYPE_MEMORY_SETTINGS_BACKEND,                         \
                                                             GMemorySettingsBackendClass))

#define G_MEMORY_SETTINGS_BACKEND_EXTENSION_POINT_NAME "gsettings-backend"

/**
 * GMemorySettingsBackend:
 *
 * A backend to GSettings that stores the settings in memory.
 **/
typedef struct _GMemorySettingsBackendPrivate               GMemorySettingsBackendPrivate;
typedef struct _GMemorySettingsBackendClass                 GMemorySettingsBackendClass;
typedef struct _GMemorySettingsBackend                      GMemorySettingsBackend;

struct _GMemorySettingsBackendClass
{
  GSettingsBackendClass parent_class;
};

struct _GMemorySettingsBackend
{
  GSettingsBackend parent_instance;

  /*< private >*/
  GMemorySettingsBackendPrivate *priv;
};

GType                           g_memory_settings_backend_get_type      (void);

G_END_DECLS

#endif /* _gmemorysettingsbackend_h_ */
