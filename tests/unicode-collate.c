#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  const char *key;
  const char *str;
} Line;
  

int 
compare_collate (const void *a, const void *b)
{
  const Line *line_a = a;
  const Line *line_b = b;

  return g_utf8_collate (line_a->str, line_b->str);
}

int 
compare_key (const void *a, const void *b)
{
  const Line *line_a = a;
  const Line *line_b = b;

  return strcmp (line_a->key, line_b->key);
}

int main (int argc, char **argv)
{
  GIOChannel *in;
  GError *error = NULL;
  GArray *line_array = g_array_new (FALSE, FALSE, sizeof(Line));
  guint i;

  if (argc != 1 && argc != 2)
    {
      fprintf (stderr, "Usage: unicode-collate [FILE]\n");
      return 1;
    }

  if (argc == 2)
    {
      in = g_io_channel_new_file (argv[1], "r", &error);
      if (!in)
	{
	  fprintf (stderr, "Cannot open %s: %s\n", argv[1], error->message);
	  return 1;
	}
    }
  else
    {
      in = g_io_channel_unix_new (fileno (stdin));
    }

  while (TRUE)
    {
      gsize term_pos;
      gchar *str;
      Line line;

      if (g_io_channel_read_line (in, &str, NULL, &term_pos, &error) != G_IO_STATUS_NORMAL)
	break;

      str[term_pos] = '\0';

      line.key = g_utf8_collate_key (str, -1);
      line.str = str;

      g_array_append_val (line_array, line);
    }

  if (error)
    {
      fprintf (stderr, "Error reading test file, %s\n", error->message);
      return 1;
    }

  printf ("== g_utf8_collate ==\n");

  qsort (line_array->data, line_array->len, sizeof (Line), compare_collate);
  for (i = 0; i < line_array->len; i++)
    printf ("%s\n", g_array_index (line_array, Line, i).str);

  printf ("== g_utf8_collate_key ==\n");

  qsort (line_array->data, line_array->len, sizeof (Line), compare_key);
  for (i = 0; i < line_array->len; i++)
    printf ("%s\n", g_array_index (line_array, Line, i).str);

  g_io_channel_close (in);

  return 0;
}
