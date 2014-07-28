/*
 * Copyright © 2014 Canonical Limited
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the licence, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#include <gio/gio.h>
#include <gio/gunixinputstream.h>

#include <stdlib.h>

static void try_to_read (GMarkupReader *reader);

static gboolean sync_mode;

static void
print_token (GMarkupReader *reader)
{
  if (g_markup_reader_is_eof (reader))
    {
      g_print ("eof\n");
      exit (0);
    }

  else if (g_markup_reader_is_start_element (reader, NULL))
    g_print ("start %s\n", g_markup_reader_get_element_name (reader));

  else if (g_markup_reader_is_end_element (reader))
    g_print ("end %s\n", g_markup_reader_get_element_name (reader));

  else if (g_markup_reader_is_text (reader))
    g_print ("text\n");
}

static void
advance_complete (GObject      *source,
                  GAsyncResult *result,
                  gpointer      user_data)
{
  GMarkupReader *reader = G_MARKUP_READER (source);
  GError *error = NULL;

  if (!g_markup_reader_advance_finish (reader, result, &error))
    {
      g_printerr ("\nerror advancing to next token: %s\n", error->message);
      g_error_free (error);
      exit (1);
    }

  print_token (reader);

  try_to_read (reader);
}

static void
try_to_read (GMarkupReader *reader)
{
  GError *error = NULL;

  while (g_markup_reader_advance_nonblocking (reader, NULL, &error))
    print_token (reader);

  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_WOULD_BLOCK))
    {
      g_printerr ("\nwould block -- doing async read\n");
      g_error_free (error);

      g_markup_reader_advance_async (reader, NULL, advance_complete, NULL);
    }
  else
    {
      g_printerr ("\nerror advancing to next token: %s\n", error->message);
      g_error_free (error);
      exit (1);
    }
}

static void
read_sync (GMarkupReader *reader)
{
  GError *error = NULL;

  g_printerr ("stream cannot poll -- doing sync reads\n");

  while (g_markup_reader_advance (reader, NULL, &error))
    print_token (reader);

  g_printerr ("\nerror advancing to next token: %s\n", error->message);
  g_error_free (error);
  exit (1);
}

static void
got_stream (GInputStream *stream)
{
  GMarkupReader *reader;

  g_printerr ("got stream\n");

  reader = g_markup_reader_new (stream, 0);

  if (sync_mode)
    read_sync (reader);
  else
    try_to_read (reader);
}

static void
read_async_complete (GObject      *source,
                     GAsyncResult *result,
                     gpointer      user_data)
{
  GFileInputStream *stream;
  GError *error = NULL;

  stream = g_file_read_finish (G_FILE (source), result, &error);

  if (stream == NULL)
    {
      g_printerr ("failed to open stream: %s\n", error->message);
      g_error_free (error);
      exit (1);
    }

  got_stream (G_INPUT_STREAM (stream));

  g_object_unref (stream);
}

int
main (int     argc,
      gchar **argv)
{
  if (argc != 2)
    {
      g_printerr ("requires one argument: a file or uri\n");
      return 1;
    }

  if (g_str_equal (argv[1], "-"))
    {
      GInputStream *stream;

      stream = g_unix_input_stream_new (0, FALSE);
      got_stream (stream);
      g_object_unref (stream);
    }
  else
    {
      GFile *file;

      file = g_file_new_for_commandline_arg (argv[1]);
      g_file_read_async (file, G_PRIORITY_DEFAULT, NULL, read_async_complete, NULL);
      g_object_unref (file);
    }

  while (TRUE)
    g_main_context_iteration (NULL, TRUE);
}
