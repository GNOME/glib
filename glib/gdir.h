#ifndef __G_DIR_H__
#define __G_DIR_H__

#include <glib/gerror.h>

G_BEGIN_DECLS

typedef struct _GDir GDir;

GDir    *               g_dir_open      (const gchar  *path,
					 guint         flags,
					 GError      **error);
G_CONST_RETURN gchar   *g_dir_read_name (GDir         *dir);
void                    g_dir_rewind    (GDir         *dir);
void                    g_dir_close     (GDir         *dir);

G_END_DECLS

#endif /* __G_DIR_H__ */
