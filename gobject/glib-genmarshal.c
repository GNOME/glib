/* GLIB-GenMarshal - Marshaller generator for GObject library
 * Copyright (C) 2000 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
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
 */
#include	<glib-object.h>

#include	<stdio.h>
#include	<fcntl.h>
#include	<string.h>
#include	<errno.h>
#include	<unistd.h>
#include	<sys/types.h>
#include	<sys/stat.h>


/* --- defines --- */
#define	PRG_NAME	"glib-genmarshal"
#define	PKG_NAME	"GLib"
#define PKG_HTTP_HOME	"http://www.gtk.org"


/* --- typedefs & structures --- */
typedef struct _Argument  Argument;
typedef struct _Signature Signature;
struct _Argument
{
  gchar       *pname;		/* parsed name */
  const gchar *sname;		/* signature name */
  const gchar *func;		/* functional extension */
  const gchar *cname;		/* C name */
};
struct _Signature
{
  gchar    *ploc;
  Argument *rarg;
  GList    *args;	/* of type Argument* */
};


/* --- prototypes --- */
static void	parse_args	(gint	   	*argc_p,
				 gchar	     ***argv_p);
static void	print_blurb	(FILE	       	*bout,
				 gboolean	 print_help);


/* --- variables --- */
static FILE          *fout = NULL;
static GScannerConfig scanner_config_template =
{
  (
   " \t\r"		/* "\n" is statement delimiter */
   )                    /* cset_skip_characters */,
  (
   G_CSET_a_2_z
   "_"
   G_CSET_A_2_Z
   )                    /* cset_identifier_first */,
  (
   G_CSET_a_2_z
   "_0123456789"
   G_CSET_A_2_Z
   )                    /* cset_identifier_nth */,
  ( "#\n" )             /* cpair_comment_single */,

  FALSE                 /* case_sensitive */,

  TRUE                  /* skip_comment_multi */,
  TRUE                  /* skip_comment_single */,
  TRUE                  /* scan_comment_multi */,
  TRUE                  /* scan_identifier */,
  FALSE                 /* scan_identifier_1char */,
  FALSE                 /* scan_identifier_NULL */,
  TRUE                  /* scan_symbols */,
  FALSE                 /* scan_binary */,
  TRUE                  /* scan_octal */,
  TRUE                  /* scan_float */,
  TRUE                  /* scan_hex */,
  FALSE                 /* scan_hex_dollar */,
  TRUE                  /* scan_string_sq */,
  TRUE                  /* scan_string_dq */,
  TRUE                  /* numbers_2_int */,
  FALSE                 /* int_2_float */,
  FALSE                 /* identifier_2_string */,
  TRUE                  /* char_2_token */,
  FALSE                 /* symbol_2_token */,
  FALSE                 /* scope_0_fallback */,
};
static gchar		*marshaller_prefix = "g_cclosure_marshal";
static GHashTable	*marshallers = NULL;
static gboolean		 gen_cheader = FALSE;
static gboolean		 gen_cbody = FALSE;
static gboolean		 skip_ploc = FALSE;


/* --- functions --- */
static gboolean
complete_arg (Argument *arg,
	      gboolean  is_return)
{
  static const Argument inout_arguments[] = {
    /* pname,		sname,		func,			cname		*/
    { "VOID",		"VOID",		NULL,			"void",		},
    { "BOOLEAN",	"BOOLEAN",	"boolean",		"gboolean",	},
    { "CHAR",		"CHAR",		"char",			"gchar",	},
    { "UCHAR",		"UCHAR",	"uchar",		"guchar",	},
    { "INT",		"INT",		"int",			"gint",		},
    { "UINT",		"UINT",		"uint",			"guint",	},
    { "LONG",		"LONG",		"long",			"glong",	},
    { "ULONG",		"ULONG",	"ulong",		"gulong",	},
    { "ENUM",		"ENUM",		"enum",			"gint",		},
    { "FLAGS",		"FLAGS",	"flags",		"guint",	},
    { "FLOAT",		"FLOAT",	"float",		"gfloat",	},
    { "DOUBLE",		"DOUBLE",	"double",		"gdouble",	},
    /* deprecated: */
    { "NONE",		"VOID",		NULL,			"void",		},
    { "BOOL",		"BOOLEAN",	"boolean",		"gboolean",	},
  };
  static const Argument in_arguments[] = {
    { "STRING",		"POINTER",	"as_pointer",		"gpointer",	},
    { "BOXED",		"POINTER",	"as_pointer",		"gpointer",	},
    { "POINTER",	"POINTER",	"as_pointer",		"gpointer",	},
    { "OBJECT",		"POINTER",	"as_pointer",		"gpointer",	},
  };
  static const Argument out_arguments[] = {
    { "STRING",		"STRING",	"string",		"gchar*",	},
    { "BOXED",		"BOXED",	"boxed",		"gpointer",	},
    { "POINTER",	"POINTER",	"pointer",		"gpointer",	},
    { "OBJECT",		"OBJECT",	"object",		"GObject*",	},
  };
  const guint n_inout_arguments = sizeof (inout_arguments) / sizeof (inout_arguments[0]);
  const guint n_out_arguments = sizeof (out_arguments) / sizeof (out_arguments[0]);
  const guint n_in_arguments = sizeof (in_arguments) / sizeof (in_arguments[0]);
  const Argument *arguments;
  guint i, n_arguments;

  g_return_val_if_fail (arg != NULL, FALSE);

  arguments = inout_arguments;
  n_arguments = n_inout_arguments;
  for (i = 0; i < n_arguments; i++)
    if (strcmp (arguments[i].pname, arg->pname) == 0)
      {
	arg->sname = arguments[i].sname;
	arg->func = arguments[i].func;
	arg->cname = arguments[i].cname;

	return TRUE;
      }
  arguments = is_return ? out_arguments : in_arguments;
  n_arguments = is_return ? n_out_arguments : n_in_arguments;
  for (i = 0; i < n_arguments; i++)
    if (strcmp (arguments[i].pname, arg->pname) == 0)
      {
	arg->sname = arguments[i].sname;
	arg->func = arguments[i].func;
	arg->cname = arguments[i].cname;

	return TRUE;
      }

  return FALSE;
}

static const gchar*
pad (const gchar *string)
{
#define PAD_LENGTH	12
  static gchar *buffer = NULL;
  gint i;

  g_return_val_if_fail (string != NULL, NULL);

  if (!buffer)
    buffer = g_new (gchar, PAD_LENGTH + 1);

  /* paranoid check */
  if (strlen (string) >= PAD_LENGTH)
    {
      g_free (buffer);
      buffer = g_strdup_printf ("%s ", string);
      g_warning ("overfull string (%u bytes) for padspace", strlen (string));

      return buffer;
    }

  for (i = 0; i < PAD_LENGTH; i++)
    {
      gboolean done = *string == 0;

      buffer[i] = done ? ' ' : *string++;
    }
  buffer[i] = 0;

  return buffer;
}

static const gchar*
indent (guint n_spaces)
{
  static gchar *buffer;
  static guint blength = 0;

  if (blength <= n_spaces)
    {
      blength = n_spaces + 1;
      g_free (buffer);
      buffer = g_new (gchar, blength);
    }
  memset (buffer, ' ', n_spaces);
  buffer[n_spaces] = 0;

  return buffer;
}

static void
generate_marshal (const gchar *signame,
		  Signature   *sig)
{
  guint ind, a;
  GList *node;

  if (g_hash_table_lookup (marshallers, signame))
    return;
  else
    {
      gchar *tmp = g_strdup (signame);

      g_hash_table_insert (marshallers, tmp, tmp);
    }
  
  if (gen_cheader)
    {
      ind = fprintf (fout, "extern void ");
      ind += fprintf (fout, "%s_%s (", marshaller_prefix, signame);
      fprintf (fout,   "GClosure     *closure,\n");
      fprintf (fout, "%sGValue       *return_value,\n", indent (ind));
      fprintf (fout, "%sguint         n_param_values,\n", indent (ind));
      fprintf (fout, "%sconst GValue *param_values,\n", indent (ind));
      fprintf (fout, "%sgpointer      invocation_hint,\n", indent (ind));
      fprintf (fout, "%sgpointer      marshal_data);\n", indent (ind));
    }
  if (gen_cbody)
    {
      /* cfile marhsal header */
      fprintf (fout, "void\n");
      ind = fprintf (fout, "%s_%s (", marshaller_prefix, signame);
      fprintf (fout,   "GClosure     *closure,\n");
      fprintf (fout, "%sGValue       *return_value,\n", indent (ind));
      fprintf (fout, "%sguint         n_param_values,\n", indent (ind));
      fprintf (fout, "%sconst GValue *param_values,\n", indent (ind));
      fprintf (fout, "%sgpointer      invocation_hint,\n", indent (ind));
      fprintf (fout, "%sgpointer      marshal_data)\n", indent (ind));
      fprintf (fout, "{\n");

      /* cfile GSignalFunc typedef */
      ind = fprintf (fout, "  typedef %s (*GSignalFunc_%s) (", sig->rarg->cname, signame);
      fprintf (fout, "%s data1,\n", pad ("gpointer"));
      for (a = 1, node = sig->args; node; node = node->next)
	{
	  Argument *arg = node->data;

	  if (arg->func)
	    fprintf (fout, "%s%s arg_%d,\n", indent (ind), pad (arg->cname), a++);
	}
      fprintf (fout, "%s%s data2);\n", indent (ind), pad ("gpointer"));

      /* cfile marshal variables */
      fprintf (fout, "  register GSignalFunc_%s callback;\n", signame);
      fprintf (fout, "  register GCClosure *cc = (GCClosure*) closure;\n");
      fprintf (fout, "  register gpointer data1, data2;\n");
      if (sig->rarg->func)
	fprintf (fout, "  %s v_return;\n", sig->rarg->cname);

      if (sig->args || sig->rarg->func)
	{
	  fprintf (fout, "\n");

	  if (sig->rarg->func)
	    fprintf (fout, "  g_return_if_fail (return_value != NULL);\n");
	  if (sig->args)
	    {
	      for (a = 0, node = sig->args; node; node = node->next)
		{
		  Argument *arg = node->data;

		  if (arg->func)
		    a++;
		}
	      fprintf (fout, "  g_return_if_fail (n_param_values >= %u);\n", 1 + a);
	    }
	}

      /* cfile marshal data1, data2 and callback setup */
      fprintf (fout, "\n");
      fprintf (fout, "  if (G_CCLOSURE_SWAP_DATA (closure))\n    {\n");
      fprintf (fout, "      data1 = closure->data;\n");
      fprintf (fout, "      data2 = g_value_get_as_pointer (param_values + 0);\n");
      fprintf (fout, "    }\n  else\n    {\n");
      fprintf (fout, "      data1 = g_value_get_as_pointer (param_values + 0);\n");
      fprintf (fout, "      data2 = closure->data;\n");
      fprintf (fout, "    }\n");
      fprintf (fout, "  callback = (GSignalFunc_%s) (marshal_data ? marshal_data : cc->callback);\n", signame);

      /* cfile marshal callback action */
      fprintf (fout, "\n");
      ind = fprintf (fout, " %s callback (", sig->rarg->func ? " v_return =" : "");
      fprintf (fout, "data1,\n");
      for (a = 1, node = sig->args; node; node = node->next)
	{
	  Argument *arg = node->data;

	  if (arg->func)
	    fprintf (fout, "%sg_value_get_%s (param_values + %d),\n", indent (ind), arg->func, a++);
	}
      fprintf (fout, "%sdata2);\n", indent (ind));

      /* cfile marshal return value storage */
      if (sig->rarg->func)
	{
	  fprintf (fout, "\n");
	  fprintf (fout, "  g_value_set_%s (return_value, v_return);\n", sig->rarg->func);
	}

      /* cfile marshal footer */
      fprintf (fout, "}\n");
    }
}

static void
process_signature (Signature *sig)
{
  gchar *pname, *sname;
  GList *node;

  /* lookup and complete info on arguments */
  if (!complete_arg (sig->rarg, TRUE))
    {
      g_warning ("unknown type: %s", sig->rarg->pname);
      return;
    }
  for (node = sig->args; node; node = node->next)
    {
      Argument *arg = node->data;

      if (!complete_arg (arg, FALSE))
	{
	  g_warning ("unknown type: %s", arg->pname);
	  return;
	}
    }

  /* construct requested marshaller name and technical marshaller name */
  pname = g_strconcat (sig->rarg->pname, "_", NULL);
  sname = g_strconcat (sig->rarg->sname, "_", NULL);
  for (node = sig->args; node; node = node->next)
    {
      Argument *arg = node->data;
      gchar *tmp;

      tmp = sname;
      sname = g_strconcat (tmp, "_", arg->sname, NULL);
      g_free (tmp);
      tmp = pname;
      pname = g_strconcat (tmp, "_", arg->pname, NULL);
      g_free (tmp);
    }

  /* introductionary comment */
  fprintf (fout, "\n/* %s", sig->rarg->pname);
  for (node = sig->args; node; node = node->next)
    {
      Argument *arg = node->data;

      fprintf (fout, "%c%s", node->prev ? ',' : ':', arg->pname);
    }
  if (!skip_ploc)
    fprintf (fout, " (%s)", sig->ploc);
  fprintf (fout, " */\n");

  /* generate signature marshaller */
  generate_marshal (sname, sig);

  /* put out marshaler alias if required */
  if (gen_cheader && !g_hash_table_lookup (marshallers, pname))
    {
      gchar *tmp = g_strdup (pname);

      fprintf (fout, "#define %s_%s\t%s_%s\n", marshaller_prefix, pname, marshaller_prefix, sname);

      g_hash_table_insert (marshallers, tmp, tmp);
    }

  g_free (pname);
  g_free (sname);
}

static Argument*
new_arg (const gchar *pname)
{
  Argument *arg = g_new0 (Argument, 1);

  arg->pname = g_strdup (pname);

  return arg;
}

static guint
parse_line (GScanner  *scanner,
	    Signature *sig)
{
  /* parse identifier for return value */
  if (g_scanner_get_next_token (scanner) != G_TOKEN_IDENTIFIER)
    return G_TOKEN_IDENTIFIER;
  sig->rarg = new_arg (scanner->value.v_identifier);

  /* keep a note on the location */
  sig->ploc = g_strdup_printf ("%s:%u", scanner->input_name, scanner->line);

  /* expect ':' */
  if (g_scanner_get_next_token (scanner) != ':')
    return ':';

  /* parse first argument */
  if (g_scanner_get_next_token (scanner) != G_TOKEN_IDENTIFIER)
    return G_TOKEN_IDENTIFIER;
  sig->args = g_list_append (sig->args, new_arg (scanner->value.v_identifier));

  /* parse rest of argument list */
  while (g_scanner_peek_next_token (scanner) == ',')
    {
      /* eat comma */
      g_scanner_get_next_token (scanner);

      /* parse arg identifier */
      if (g_scanner_get_next_token (scanner) != G_TOKEN_IDENTIFIER)
	return G_TOKEN_IDENTIFIER;
      sig->args = g_list_append (sig->args, new_arg (scanner->value.v_identifier));
    }
  
  /* expect end of line, done */
  if (g_scanner_get_next_token (scanner) != '\n')
    return '\n';
  
  /* success */
  return G_TOKEN_NONE;
}

static gboolean
string_key_destroy (gpointer key,
		    gpointer value,
		    gpointer user_data)
{
  g_free (key);

  return TRUE;
}

int
main (int   argc,
      char *argv[])
{
  GScanner *scanner;
  GSList *slist, *files = NULL;
  gint i;

  /* parse args and do fast exits */
  parse_args (&argc, &argv);

  /* list input files */
  for (i = 1; i < argc; i++)
    files = g_slist_prepend (files, argv[i]);
  if (files)
    files = g_slist_reverse (files);
  else
    files = g_slist_prepend (files, "/dev/stdin");

  /* setup auxillary structs */
  scanner = g_scanner_new (&scanner_config_template);
  fout = stdout;
  marshallers = g_hash_table_new (g_str_hash, g_str_equal);

  /* process input files */
  for (slist = files; slist; slist = slist->next)
    {
      gchar *file = slist->data;
      gint fd = open (file, O_RDONLY);

      if (fd < 0)
	{
	  g_warning ("failed to open \"%s\": %s", file, g_strerror (errno));
	  continue;
	}

      /* set file name for error reports */
      scanner->input_name = file;

      /* parse & process file */
      g_scanner_input_file (scanner, fd);
      
      /* scanning loop, we parse the input untill it's end is reached,
       * or our sub routine came across invalid syntax
       */
      do
	{
	  guint expected_token = G_TOKEN_NONE;

	  switch (g_scanner_peek_next_token (scanner))
	    {
	    case '\n':
	      /* eat newline and restart */
	      g_scanner_get_next_token (scanner);
	      continue;
	    case G_TOKEN_EOF:
	      /* done */
	      break;
	    default:
	      /* parse and process signatures */
	      {
		Signature signature = { NULL, NULL, NULL };
		GList *node;

		expected_token = parse_line (scanner, &signature);
		
		/* once we got a valid signature, process it */
		if (expected_token == G_TOKEN_NONE)
		  process_signature (&signature);
		
		/* clean up signature contents */
		g_free (signature.ploc);
		if (signature.rarg)
		  g_free (signature.rarg->pname);
		g_free (signature.rarg);
		for (node = signature.args; node; node = node->next)
		  {
		    Argument *arg = node->data;
		    
		    g_free (arg->pname);
		    g_free (arg);
		  }
		g_list_free (signature.args);
	      }
	      break;
	    }

	  /* bail out on errors */
	  if (expected_token != G_TOKEN_NONE)
	    {
	      g_scanner_unexp_token (scanner, expected_token, "type name", NULL, NULL, NULL, TRUE);
	      break;
	    }

	  g_scanner_peek_next_token (scanner);
	}
      while (scanner->next_token != G_TOKEN_EOF);

      close (fd);
    }

  /* clean up */
  g_slist_free (files);
  g_scanner_destroy (scanner);
  g_hash_table_foreach_remove (marshallers, string_key_destroy, NULL);
  g_hash_table_destroy (marshallers);

  return 0;
}

static void
parse_args (gint    *argc_p,
	    gchar ***argv_p)
{
  guint argc = *argc_p;
  gchar **argv = *argv_p;
  guint i, e;
  
  for (i = 1; i < argc; i++)
    {
      if (strcmp ("--header", argv[i]) == 0)
	{
	  gen_cheader = TRUE;
	  argv[i] = NULL;
	}
      else if (strcmp ("--body", argv[i]) == 0)
	{
	  gen_cbody = TRUE;
	  argv[i] = NULL;
	}
      else if (strcmp ("--skip-source", argv[i]) == 0)
	{
	  skip_ploc = TRUE;
	  argv[i] = NULL;
	}
      else if ((strcmp ("--prefix", argv[i]) == 0) ||
	       (strncmp ("--prefix=", argv[i], 9) == 0))
	{
          gchar *equal = argv[i] + 8;

	  if (*equal == '=')
	    marshaller_prefix = g_strdup (equal + 1);
	  else if (i + 1 < argc)
	    {
	      marshaller_prefix = g_strdup (argv[i + 1]);
	      argv[i] = NULL;
	      i += 1;
	    }
	  argv[i] = NULL;
	}
      else if (strcmp ("-h", argv[i]) == 0 ||
	  strcmp ("--help", argv[i]) == 0)
	{
	  print_blurb (stderr, TRUE);
	  argv[i] = NULL;
	  exit (0);
	}
      else if (strcmp ("-v", argv[i]) == 0 ||
	       strcmp ("--version", argv[i]) == 0)
	{
	  print_blurb (stderr, FALSE);
	  argv[i] = NULL;
	  exit (0);
	}
      else if (strcmp (argv[i], "--g-fatal-warnings") == 0)
	{
	  GLogLevelFlags fatal_mask;
	  
	  fatal_mask = g_log_set_always_fatal (G_LOG_FATAL_MASK);
	  fatal_mask |= G_LOG_LEVEL_WARNING | G_LOG_LEVEL_CRITICAL;
	  g_log_set_always_fatal (fatal_mask);
	  
	  argv[i] = NULL;
	}
    }
  
  e = 0;
  for (i = 1; i < argc; i++)
    {
      if (e)
	{
	  if (argv[i])
	    {
	      argv[e++] = argv[i];
	      argv[i] = NULL;
	    }
	}
      else if (!argv[i])
	e = i;
    }
  if (e)
    *argc_p = e;
}

static void
print_blurb (FILE    *bout,
	     gboolean print_help)
{
  if (!print_help)
    {
      fprintf (bout, "%s version ", PRG_NAME);
      fprintf (bout, "%u.%u.%u", GLIB_MAJOR_VERSION, GLIB_MINOR_VERSION, GLIB_MICRO_VERSION);
      fprintf (bout, "\n");
      fprintf (bout, "%s comes with ABSOLUTELY NO WARRANTY.\n", PRG_NAME);
      fprintf (bout, "You may redistribute copies of %s under the terms of\n", PRG_NAME);
      fprintf (bout, "the GNU General Public License which can be found in the\n");
      fprintf (bout, "%s source package. Sources, examples and contact\n", PKG_NAME);
      fprintf (bout, "information are available at %s\n", PKG_HTTP_HOME);
    }
  else
    {
      fprintf (bout, "Usage: %s [options] [files...]\n", PRG_NAME);
      fprintf (bout, "  --header                        generate C headers\n");
      fprintf (bout, "  --body                          generate C code\n");
      fprintf (bout, "  --prefix=string                 specify marshaller prefix\n");
      fprintf (bout, "  --skip-source                   skip source location comments\n");
      fprintf (bout, "  -h, --help                      show this help message\n");
      fprintf (bout, "  -v, --version                   print version informations\n");
      fprintf (bout, "  --g-fatal-warnings              make warnings fatal (abort)\n");
    }
}
