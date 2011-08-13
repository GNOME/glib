#include <glib.h>

static gboolean
redisplay_clock (gpointer data)
{
  GSource *source;
  GDateTime *now, *expiry;

  now = g_date_time_new_now_local ();
  g_print ("%02d:%02d\n",
	   g_date_time_get_hour (now),
	   g_date_time_get_minute (now));

  expiry = g_date_time_add_seconds (now, 60 - g_date_time_get_second (now));
  source = g_date_time_source_new (expiry, TRUE);
  g_source_set_callback (source, redisplay_clock, NULL, NULL);
  g_source_attach (source, NULL);
  g_source_unref (source);

  g_date_time_unref (expiry);
  g_date_time_unref (now);

  return FALSE;
}

int
main (void)
{
  GMainLoop *loop;
  
  loop = g_main_loop_new (NULL, FALSE);

  redisplay_clock (NULL);

  g_main_loop_run (loop);
  
  return 0;
}
