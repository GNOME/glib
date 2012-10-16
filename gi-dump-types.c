/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
#include "gdump.c"
#include <gio/gunixoutputstream.h>

int
main (int    argc,
      char **argv)
{
  int i;
  GOutputStream *stdout;
  GModule *self;

  stdout = g_unix_output_stream_new (1, FALSE);

  self = g_module_open (NULL, 0);

  for (i = 1; i < argc; i++)
    {
      GError *error = NULL;
      GType type;

      type = invoke_get_type (self, argv[i], &error);
      if (!type)
	{
	  g_printerr ("%s\n", error->message);
	  g_clear_error (&error);
	}
      else
	dump_type (type, argv[i], stdout);
    }

  return 0;
}
