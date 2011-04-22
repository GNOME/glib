#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <glib/gstdio.h>
#include <glib-object.h>

#define TEST_TYPE_FILE          (test_file_get_type ())
#define TEST_FILE(obj)          (G_TYPE_CHECK_INSTANCE_CAST ((obj), TEST_TYPE_FILE, TestFile))
#define TEST_IS_FILE(obj)       (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TEST_TYPE_FILE))

typedef struct _TestFile        TestFile;
typedef struct _TestFilePrivate TestFilePrivate;
typedef struct _TestFileClass   TestFileClass;

struct _TestFile
{
  GObject parent_instance;

  TestFilePrivate *priv;
};

struct _TestFileClass
{
  GObjectClass parent_class;
};

struct _TestFilePrivate
{
  gchar *path;

  gint64 size;
};

GType test_file_get_type (void); /* for -Wmissing-prototypes */

G_DEFINE_TYPE (TestFile, test_file, G_TYPE_OBJECT)

enum { PROP_0, PROP_PATH, PROP_SIZE, LAST_PROP };

static GParamSpec *test_file_properties[LAST_PROP] = { 0, };

/* for -Wmissing-prototypes */
G_DECLARE_PROPERTY_GET_SET (TestFile, test_file, const gchar *, path)

G_DEFINE_PROPERTY_GET (TestFile, test_file, const gchar *, path)

void
test_file_set_path (TestFile    *self,
                    const gchar *value)
{
  GStatBuf s_buf;

  g_return_if_fail (TEST_IS_FILE (self));
  g_return_if_fail (value != NULL && *value != '\0');

  if (g_strcmp0 (value, self->priv->path) == 0)
    return;

  if (g_stat (value, &s_buf) == -1)
    {
      int saved_errno = errno;

      g_warning ("Unable to access the path: %s", g_strerror (saved_errno));

      return;
    }

  self->priv->size = (gint64) s_buf.st_size;

  g_free (self->priv->path);
  self->priv->path = g_strdup (value);

  g_object_notify_by_pspec (G_OBJECT (self), test_file_properties[PROP_SIZE]);
}

/* for -Wmissing-prototypes */
G_DECLARE_PROPERTY_GET (TestFile, test_file, gint64, size)

G_DEFINE_PROPERTY_GET (TestFile, test_file, gint64, size)

static void
test_file_finalize (GObject *gobject)
{
  TestFilePrivate *priv = TEST_FILE (gobject)->priv;

  g_free (priv->path);

  G_OBJECT_CLASS (test_file_parent_class)->finalize (gobject);
}

static void
test_file_class_init (TestFileClass *klass)
{
  G_OBJECT_CLASS (klass)->finalize = test_file_finalize;

  g_type_class_add_private (klass, sizeof (TestFilePrivate));

  test_file_properties[PROP_PATH] =
    g_string_property_new ("path",
                           G_PROPERTY_READWRITE | G_PROPERTY_COPY_SET,
                           G_STRUCT_OFFSET (TestFilePrivate, path),
                           (GPropertyStringSet) test_file_set_path,
                           NULL);

  test_file_properties[PROP_SIZE] =
    g_string_property_new ("size", G_PROPERTY_READABLE,
                           G_STRUCT_OFFSET (TestFilePrivate, size),
                           NULL, NULL);

  g_object_class_install_properties (G_OBJECT_CLASS (klass),
                                     G_N_ELEMENTS (test_file_properties),
                                     test_file_properties);
}

static void
test_file_init (TestFile *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            TEST_TYPE_FILE,
                                            TestFilePrivate);
}

int
main (int   argc,
      char *argv[])
{
  TestFile *f;
  int i;

  f = g_object_new (TEST_TYPE_FILE, NULL);

  for (i = 1; i < argc; i++)
    {
      test_file_set_path (f, argv[i]);

      g_print ("File: %s, size: %" G_GINT64_FORMAT "\n",
               test_file_get_path (f),
               test_file_get_size (f));
    }

  g_object_unref (f);

  return EXIT_SUCCESS;
}
