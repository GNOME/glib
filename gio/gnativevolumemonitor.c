#include "config.h"

#include <string.h>

#include <glib.h>
#include "gnativevolumemonitor.h"

#include "gioalias.h"

G_DEFINE_ABSTRACT_TYPE (GNativeVolumeMonitor, g_native_volume_monitor, G_TYPE_VOLUME_MONITOR);

static void
g_native_volume_monitor_finalize (GObject *object)
{
  G_OBJECT_CLASS (g_native_volume_monitor_parent_class)->finalize (object);
}


static void
g_native_volume_monitor_class_init (GNativeVolumeMonitorClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  
  gobject_class->finalize = g_native_volume_monitor_finalize;
}


static void
g_native_volume_monitor_init (GNativeVolumeMonitor *native_monitor)
{
}

#define __G_NATIVE_VOLUME_MONITOR_C__
#include "gioaliasdef.c"
