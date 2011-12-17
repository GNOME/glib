#include "giotypes.h"

typedef struct _GApplicationImpl GApplicationImpl;

typedef struct
{
  gchar *name;

  GVariantType *parameter_type;
  gboolean      enabled;
  GVariant     *state;
} RemoteActionInfo;

G_GNUC_INTERNAL
void                    g_application_impl_destroy                      (GApplicationImpl   *impl);

G_GNUC_INTERNAL
GApplicationImpl *      g_application_impl_register                     (GApplication        *application,
                                                                         const gchar         *appid,
                                                                         GApplicationFlags    flags,
                                                                         GActionGroup        *exported_actions,
                                                                         GRemoteActionGroup **remote_actions,
                                                                         GCancellable        *cancellable,
                                                                         GError             **error);

G_GNUC_INTERNAL
void                    g_application_impl_activate                     (GApplicationImpl   *impl,
                                                                         GVariant           *platform_data);

G_GNUC_INTERNAL
void                    g_application_impl_open                         (GApplicationImpl   *impl,
                                                                         GFile             **files,
                                                                         gint                n_files,
                                                                         const gchar        *hint,
                                                                         GVariant           *platform_data);

G_GNUC_INTERNAL
int                     g_application_impl_command_line                 (GApplicationImpl   *impl,
                                                                         gchar             **arguments,
                                                                         GVariant           *platform_data);

G_GNUC_INTERNAL
void                    g_application_impl_flush                        (GApplicationImpl   *impl);
