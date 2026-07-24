/* Unit test exposing a use-after-free of GMainContext's cached poll array.
 *
 * To compile on macOS:
 *
 *   meson setup _build-asan -Db_sanitize=address -Db_lundef=false --buildtype=debug
 *   meson compile -C _build-asan glib/tests/mainloop-poll-realloc
 *   ASAN_OPTIONS=abort_on_error=1:halt_on_error=1 _build-asan/glib/tests/mainloop-poll-realloc
 */

#include <glib.h>

#include <unistd.h>

#define N_OUTER_FDS 8
#define N_INNER_FDS 64

static gboolean
fd_source_dispatch (GSource     *source,
                    GSourceFunc  callback,
                    gpointer     user_data)
{
  return G_SOURCE_CONTINUE;
}

static GSourceFuncs fd_source_funcs = {
  NULL, NULL, fd_source_dispatch, NULL, NULL, NULL
};

static void
add_fd_source (GMainContext *context)
{
  gint fds[2];
  GSource *source;

  g_assert_cmpint (pipe (fds), ==, 0);
  source = g_source_new (&fd_source_funcs, sizeof (GSource));
  g_source_add_unix_fd (source, fds[0], G_IO_IN);
  g_source_attach (source, context);
  g_source_unref (source);
}

static GMainContext *test_context = NULL;
static gboolean recursed = FALSE;

static gint
recursing_poll_func (GPollFD *fds,
                     guint    nfds,
                     gint     timeout)
{
  if (!recursed)
    {
      recursed = TRUE;

      /* Add many more fd sources. */
      for (guint i = 0; i < N_INNER_FDS; i++)
        add_fd_source (test_context);

      /* Re-enter the same context. In the buggy implementation this frees the
       * poll array that the outer iteration is still using. */
      g_main_context_iteration (test_context, FALSE);
    }

  return g_poll (fds, nfds, timeout);
}

static void
test_poll_array_realloc_use_after_free (void)
{
  g_test_summary ("Recursing into a GMainContext from its poll function must "
                  "not free the poll array still in use by the outer iteration");

  test_context = g_main_context_new ();
  recursed = FALSE;

  g_main_context_set_poll_func (test_context, recursing_poll_func);

  for (guint i = 0; i < N_OUTER_FDS; i++)
    add_fd_source (test_context);

  g_main_context_iteration (test_context, FALSE);

  g_assert_true (recursed);

  /* Should have crashed at this point. */
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/mainloop/poll-array-realloc-use-after-free",
                   test_poll_array_realloc_use_after_free);

  return g_test_run ();
}
