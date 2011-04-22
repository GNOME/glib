#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <glib/gstdio.h>
#include <glib-object.h>

/* Test::File {{{ */

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
  gchar *extension;

  gint64 size;
};

GType test_file_get_type (void); /* for -Wmissing-prototypes */

G_DEFINE_TYPE (TestFile, test_file, G_TYPE_OBJECT)

enum { PROP_FILE_0, PROP_PATH, PROP_SIZE, PROP_EXTENSION, LAST_FILE_PROP };

static GParamSpec *test_file_properties[LAST_FILE_PROP] = { NULL, };

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

  self->priv->extension = strrchr (self->priv->path, '.');

  if (self->priv->extension != NULL &&
      strlen (self->priv->extension) != 0)
    {
      self->priv->extension += 1;
    }
  else
    self->priv->extension = NULL;

  g_object_notify_by_pspec (G_OBJECT (self), test_file_properties[PROP_PATH]);
  g_object_notify_by_pspec (G_OBJECT (self), test_file_properties[PROP_SIZE]);
  g_object_notify_by_pspec (G_OBJECT (self), test_file_properties[PROP_EXTENSION]);
}

G_DECLARE_PROPERTY_GET (TestFile, test_file, const gchar *, extension)
G_DEFINE_PROPERTY_GET (TestFile, test_file, const gchar *, extension)

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
    g_string_property_new ("path", G_PROPERTY_READWRITE,
                           G_STRUCT_OFFSET (TestFilePrivate, path),
                           (GPropertyStringSet) test_file_set_path,
                           NULL);

  test_file_properties[PROP_EXTENSION] =
    g_string_property_new ("extension", G_PROPERTY_READABLE,
                           G_STRUCT_OFFSET (TestFilePrivate, extension),
                           NULL, NULL);

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

/* }}} */

/* Test::File::Mp3 {{{ */

#define TEST_TYPE_FILE_MP3      (test_file_mp3_get_type ())
#define TEST_FILE_MP3(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), TEST_TYPE_FILE_MP3, TestFileMp3))
#define TEST_IS_FILE_MP3(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TEST_TYPE_FILE_MP3))

typedef struct _TestFileMp3             TestFileMp3;
typedef struct _TestFileMp3Private      TestFileMp3Private;
typedef struct _TestFileMp3Class        TestFileMp3Class;

struct _TestFileMp3
{
  TestFile parent_instance;

  TestFileMp3Private *priv;
};

struct _TestFileMp3Class
{
  TestFileClass parent_class;
};

struct _TestFileMp3Private
{
  gchar *artist;
  gchar *title;
  gchar *album;

  gint64 duration;
};

GType test_file_mp3_get_type (void); /* for -Wmissing-prototypes */

G_DEFINE_TYPE (TestFileMp3, test_file_mp3, TEST_TYPE_FILE)

enum { PROP_MP3_0, PROP_ARTIST, PROP_TITLE, PROP_ALBUM, PROP_DURATION, LAST_MP3_PROP };

static GParamSpec *test_file_mp3_properties[LAST_MP3_PROP] = { NULL, };

G_DECLARE_PROPERTY_GET_SET (TestFileMp3, test_file_mp3, const gchar *, artist) /* for -Wmissing-prototypes */
G_DEFINE_PROPERTY_GET_SET (TestFileMp3, test_file_mp3, const gchar *, artist)

G_DECLARE_PROPERTY_GET_SET (TestFileMp3, test_file_mp3, const gchar *, title) /* for -Wmissing-prototypes */
G_DEFINE_PROPERTY_GET_SET (TestFileMp3, test_file_mp3, const gchar *, title)

G_DECLARE_PROPERTY_GET_SET (TestFileMp3, test_file_mp3, const gchar *, album) /* for -Wmissing-prototypes */
G_DEFINE_PROPERTY_GET_SET (TestFileMp3, test_file_mp3, const gchar *, album)

G_DECLARE_PROPERTY_GET_SET (TestFileMp3, test_file_mp3, gint64, duration) /* for -Wmissing-prototypes */
G_DEFINE_PROPERTY_GET_SET (TestFileMp3, test_file_mp3, gint64, duration)

static void
test_file_mp3_play (TestFileMp3 *file)
{
  g_return_if_fail (TEST_IS_FILE_MP3 (file));

  g_print ("Playing...\n");
}

static void
test_file_mp3_finalize (GObject *gobject)
{
  TestFileMp3Private *priv = TEST_FILE_MP3 (gobject)->priv;

  g_free (priv->artist);
  g_free (priv->album);
  g_free (priv->title);

  G_OBJECT_CLASS (test_file_mp3_parent_class)->finalize (gobject);
}

static void
test_file_mp3_class_init (TestFileMp3Class *klass)
{
  G_OBJECT_CLASS (klass)->finalize = test_file_mp3_finalize;

  g_type_class_add_private (klass, sizeof (TestFileMp3Private));

  test_file_mp3_properties[PROP_ALBUM] =
    g_string_property_new ("album",
                           G_PROPERTY_READWRITE | G_PROPERTY_COPY_SET,
                           G_STRUCT_OFFSET (TestFileMp3Private, album),
                           NULL, NULL);
  g_property_set_default (G_PROPERTY (test_file_mp3_properties[PROP_ALBUM]),
                          klass,
                          "Unknown Album");

  test_file_mp3_properties[PROP_ARTIST] =
    g_string_property_new ("artist",
                           G_PROPERTY_READWRITE | G_PROPERTY_COPY_SET,
                           G_STRUCT_OFFSET (TestFileMp3Private, artist),
                           NULL, NULL);
  g_property_set_default (G_PROPERTY (test_file_mp3_properties[PROP_ARTIST]),
                          klass,
                          "Unknown Author");

  test_file_mp3_properties[PROP_TITLE] =
    g_string_property_new ("title",
                           G_PROPERTY_READWRITE | G_PROPERTY_COPY_SET,
                           G_STRUCT_OFFSET (TestFileMp3Private, title),
                           NULL, NULL);
  g_property_set_default (G_PROPERTY (test_file_mp3_properties[PROP_TITLE]),
                          klass,
                          "Unknown Track");

  test_file_mp3_properties[PROP_DURATION] =
    g_int64_property_new ("duration", G_PROPERTY_READABLE,
                          G_STRUCT_OFFSET (TestFileMp3Private, duration),
                          NULL, NULL);

  g_object_class_install_properties (G_OBJECT_CLASS (klass),
                                     G_N_ELEMENTS (test_file_mp3_properties),
                                     test_file_mp3_properties);
}

static void
test_file_mp3_init (TestFileMp3 *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            TEST_TYPE_FILE_MP3,
                                            TestFileMp3Private);

  g_property_get_default (G_PROPERTY (test_file_mp3_properties[PROP_ARTIST]),
                          self,
                          &(self->priv->artist));
  g_property_get_default (G_PROPERTY (test_file_mp3_properties[PROP_ALBUM]),
                          self,
                          &(self->priv->album));
  g_property_get_default (G_PROPERTY (test_file_mp3_properties[PROP_TITLE]),
                          self,
                          &(self->priv->title));
}

/* }}} */

/* main {{{ */
int
main (int   argc,
      char *argv[])
{
  TestFile *f;
  int i;

  f = g_object_new (TEST_TYPE_FILE_MP3, NULL);

  for (i = 1; i < argc; i++)
    {
      test_file_set_path (f, argv[i]);

      if (g_strcmp0 (test_file_get_extension (f), "mp3") != 0)
        continue;

      g_print ("File: %s, size: %" G_GINT64_FORMAT "\n",
               test_file_get_path (f),
               test_file_get_size (f));
      g_print ("  Track: %s - %s\n",
               test_file_mp3_get_artist (TEST_FILE_MP3 (f)),
               test_file_mp3_get_title (TEST_FILE_MP3 (f)));

      test_file_mp3_play (TEST_FILE_MP3 (f));
    }

  g_object_unref (f);

  return EXIT_SUCCESS;
}
/* }}} */
