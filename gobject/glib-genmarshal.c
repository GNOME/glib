/* GLIB-GenMarshal - Marshaller generator for GObject library
 * Copyright (C) 2000-2001 Red Hat, Inc.
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
#include	"config.h"


/* ok, this is a bit hackish, have to provide gruntime log domain as
 * we don't link against -lgruntime
 */
char *g_log_domain_gruntime = "GLib-Genmarshal";

#include	<glib.h>

#include	<stdio.h>
#include	<stdlib.h>
#include	<fcntl.h>
#include	<string.h>
#include	<errno.h>
#ifdef HAVE_UNISTD_H
#include	<unistd.h>
#endif
#include	<sys/types.h>
#include	<sys/stat.h>

#ifdef G_OS_WIN32
#include <io.h>
#endif

/* --- defines --- */
#define	PRG_NAME	"glib-genmarshal"
#define	PKG_NAME	"GLib"
#define PKG_HTTP_HOME	"http://www.gtk.org"


/* --- typedefs & structures --- */
typedef struct
{
  gchar	      *keyword;		/* marhaller list keyword [MY_STRING] */
  const gchar *sig_name;	/* signature name [STRING] */
  const gchar *ctype;		/* C type name [gchar*] */
  const gchar *getter;		/* value getter function [g_value_get_string] */
} InArgument;
typedef struct
{
  gchar	      *keyword;		/* marhaller list keyword [MY_STRING] */
  const gchar *sig_name;	/* signature name [STRING] */
  const gchar *ctype;		/* C type name [gchar*] */
  const gchar *setter;		/* value setter function [g_value_set_string] */
  const gchar *release;		/* value release function [g_free] */
  const gchar *release_check;   /* checks if release function is safe to call */
} OutArgument;
typedef struct
{
  gchar       *ploc;
  OutArgument *rarg;
  GList       *args;	/* of type InArgument* */
} Signature;


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
static gchar		*std_marshaller_prefix = "g_cclosure_marshal";
static gchar		*marshaller_prefix = "g_cclosure_user_marshal";
static GHashTable	*marshallers = NULL;
static gboolean		 gen_cheader = FALSE;
static gboolean		 gen_cbody = FALSE;
static gboolean		 skip_ploc = FALSE;
static gboolean		 std_includes = TRUE;


/* --- functions --- */
static gboolean
complete_in_arg (InArgument *iarg)
{
  static const InArgument args[] = {
    /* keyword		sig_name	ctype		getter			*/
    { "VOID",		"VOID",		"void",		NULL,			},
    { "BOOLEAN",	"BOOLEAN",	"gboolean",	"g_value_get_boolean",	},
    { "CHAR",		"CHAR",		"gchar",	"g_value_get_char",	},
    { "UCHAR",		"UCHAR",	"guchar",	"g_value_get_uchar",	},
    { "INT",		"INT",		"gint",		"g_value_get_int",	},
    { "UINT",		"UINT",		"guint",	"g_value_get_uint",	},
    { "LONG",		"LONG",		"glong",	"g_value_get_long",	},
    { "ULONG",		"ULONG",	"gulong",	"g_value_get_ulong",	},
    { "INT64",		"INT64",	"gint64",       "g_value_get_int64",	},
    { "UINT64",		"UINT64",	"guint64",	"g_value_get_uint64",	},
    { "ENUM",		"ENUM",		"gint",		"g_value_get_enum",	},
    { "FLAGS",		"FLAGS",	"guint",	"g_value_get_flags",	},
    { "FLOAT",		"FLOAT",	"gfloat",	"g_value_get_float",	},
    { "DOUBLE",		"DOUBLE",	"gdouble",	"g_value_get_double",	},
    { "STRING",		"STRING",	"gpointer",	"(char*) g_value_get_string",	},
    { "PARAM",		"PARAM",	"gpointer",	"g_value_get_param",	},
    { "BOXED",		"BOXED",	"gpointer",	"g_value_get_boxed",	},
    { "POINTER",	"POINTER",	"gpointer",	"g_value_get_pointer",	},
    { "OBJECT",		"OBJECT",	"gpointer",	"g_value_get_object",	},
    /* deprecated: */
    { "NONE",		"VOID",		"void",		NULL,			},
    { "BOOL",		"BOOLEAN",	"gboolean",	"g_value_get_boolean",	},
  };
  const guint n_args = sizeof (args) / sizeof (args[0]);
  guint i;

  g_return_val_if_fail (iarg != NULL, FALSE);

  for (i = 0; i < n_args; i++)
    if (strcmp (args[i].keyword, iarg->keyword) == 0)
      {
	iarg->sig_name = args[i].sig_name;
	iarg->ctype = args[i].ctype;
	iarg->getter = args[i].getter;

	return TRUE;
      }
  return FALSE;
}

static gboolean
complete_out_arg (OutArgument *oarg)
{
  static const OutArgument args[] = {
    /* keyword		sig_name	ctype		setter			release                 release_check */
    { "VOID",		"VOID",		"void",		NULL,			NULL,			NULL          },
    { "BOOLEAN",	"BOOLEAN",	"gboolean",	"g_value_set_boolean",	NULL,			NULL          },
    { "CHAR",		"CHAR",		"gchar",	"g_value_set_char",	NULL,			NULL          },
    { "UCHAR",		"UCHAR",	"guchar",	"g_value_set_uchar",	NULL,			NULL          },
    { "INT",		"INT",		"gint",		"g_value_set_int",	NULL,			NULL          },
    { "UINT",		"UINT",		"guint",	"g_value_set_uint",	NULL,			NULL          },
    { "LONG",		"LONG",		"glong",	"g_value_set_long",	NULL,			NULL          },
    { "ULONG",		"ULONG",	"gulong",	"g_value_set_ulong",	NULL,			NULL          },
    { "INT64",		"INT64",	"gint64",	"g_value_set_int64",	NULL,			NULL          },
    { "UINT64",		"UINT64",	"guint64",	"g_value_set_uint64",	NULL,			NULL          },
    { "ENUM",		"ENUM",		"gint",		"g_value_set_enum",	NULL,			NULL          },
    { "FLAGS",		"FLAGS",	"guint",	"g_value_set_flags",	NULL,			NULL          },
    { "FLOAT",		"FLOAT",	"gfloat",	"g_value_set_float",	NULL,			NULL          },
    { "DOUBLE",		"DOUBLE",	"gdouble",	"g_value_set_double",	NULL,			NULL          },
    { "STRING",		"STRING",	"gchar*",	"g_value_set_string_take_ownership", NULL,	NULL          },
    { "PARAM",		"PARAM",	"GParamSpec*",	"g_value_set_param",	"g_param_spec_unref",	NULL          },
    { "BOXED",		"BOXED",	"gpointer",	"g_value_set_boxed_take_ownership", NULL,	NULL          },
    { "POINTER",	"POINTER",	"gpointer",	"g_value_set_pointer",	NULL,			NULL          },
    { "OBJECT",		"OBJECT",	"GObject*",	"g_value_set_object",	"g_object_unref",	"NULL !="     },
    /* deprecated: */
    { "NONE",		"VOID",		"void",		NULL,			NULL,			NULL          },
    { "BOOL",		"BOOLEAN",	"gboolean",	"g_value_set_boolean",	NULL,			NULL          }
  };
  const guint n_args = sizeof (args) / sizeof (args[0]);
  guint i;

  g_return_val_if_fail (oarg != NULL, FALSE);

  for (i = 0; i < n_args; i++)
    if (strcmp (args[i].keyword, oarg->keyword) == 0)
      {
	oarg->sig_name = args[i].sig_name;
	oarg->ctype = args[i].ctype;
	oarg->setter = args[i].setter;
	oarg->release = args[i].release;
	oarg->release_check = args[i].release_check;

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
      g_warning ("overfull string (%u bytes) for padspace", (guint) strlen (string));

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
  static gchar *buffer = NULL;
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
  gchar *tmp = g_strconcat (marshaller_prefix, "_", signame, NULL);
  gboolean have_std_marshaller = FALSE;

  /* here we have to make sure a marshaller named <marshaller_prefix>_<signame>
   * exists. we might have put it out already, can revert to a standard marshaller
   * provided by glib, or need to generate one.
   */

  if (g_hash_table_lookup (marshallers, tmp))
    {
      /* done, marshaller already generated */
      g_free (tmp);
      return;
    }
  else
    {
      /* need to alias/generate marshaller, register name */
      g_hash_table_insert (marshallers, tmp, tmp);
    }

  /* can we revert to a standard marshaller? */
  if (std_includes)
    {
      tmp = g_strconcat (std_marshaller_prefix, "_", signame, NULL);
      have_std_marshaller = g_hash_table_lookup (marshallers, tmp) != NULL;
      g_free (tmp);
    }

  if (gen_cheader && have_std_marshaller)
    {
      fprintf (fout, "#define %s_%s\t%s_%s\n", marshaller_prefix, signame, std_marshaller_prefix, signame);
    }
  if (gen_cheader && !have_std_marshaller)
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
  if (gen_cbody && !have_std_marshaller)
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

      /* cfile GMarshalFunc typedef */
      ind = fprintf (fout, "  typedef %s (*GMarshalFunc_%s) (", sig->rarg->ctype, signame);
      fprintf (fout, "%s data1,\n", pad ("gpointer"));
      for (a = 1, node = sig->args; node; node = node->next)
	{
	  InArgument *iarg = node->data;

	  if (iarg->getter)
	    fprintf (fout, "%s%s arg_%d,\n", indent (ind), pad (iarg->ctype), a++);
	}
      fprintf (fout, "%s%s data2);\n", indent (ind), pad ("gpointer"));

      /* cfile marshal variables */
      fprintf (fout, "  register GMarshalFunc_%s callback;\n", signame);
      fprintf (fout, "  register GCClosure *cc = (GCClosure*) closure;\n");
      fprintf (fout, "  register gpointer data1, data2;\n");
      if (sig->rarg->setter)
	fprintf (fout, "  %s v_return;\n", sig->rarg->ctype);

      if (sig->args || sig->rarg->setter)
	{
	  fprintf (fout, "\n");

	  if (sig->rarg->setter)
	    fprintf (fout, "  g_return_if_fail (return_value != NULL);\n");
	  if (sig->args)
	    {
	      for (a = 0, node = sig->args; node; node = node->next)
		{
		  InArgument *iarg = node->data;

		  if (iarg->getter)
		    a++;
		}
	      fprintf (fout, "  g_return_if_fail (n_param_values == %u);\n", 1 + a);
	    }
	}

      /* cfile marshal data1, data2 and callback setup */
      fprintf (fout, "\n");
      fprintf (fout, "  if (G_CCLOSURE_SWAP_DATA (closure))\n    {\n");
      fprintf (fout, "      data1 = closure->data;\n");
      fprintf (fout, "      data2 = g_value_peek_pointer (param_values + 0);\n");
      fprintf (fout, "    }\n  else\n    {\n");
      fprintf (fout, "      data1 = g_value_peek_pointer (param_values + 0);\n");
      fprintf (fout, "      data2 = closure->data;\n");
      fprintf (fout, "    }\n");
      fprintf (fout, "  callback = (GMarshalFunc_%s) (marshal_data ? marshal_data : cc->callback);\n", signame);

      /* cfile marshal callback action */
      fprintf (fout, "\n");
      ind = fprintf (fout, " %s callback (", sig->rarg->setter ? " v_return =" : "");
      fprintf (fout, "data1,\n");
      for (a = 1, node = sig->args; node; node = node->next)
	{
	  InArgument *iarg = node->data;

	  if (iarg->getter)
	    fprintf (fout, "%s%s (param_values + %d),\n", indent (ind), iarg->getter, a++);
	}
      fprintf (fout, "%sdata2);\n", indent (ind));

      /* cfile marshal return value storage */
      if (sig->rarg->setter)
	{
	  fprintf (fout, "\n");
	  fprintf (fout, "  %s (return_value, v_return);\n", sig->rarg->setter);
	  if (sig->rarg->release)
	    {
	      if (sig->rarg->release_check)
		{
		  fprintf (fout, "  if (%s (v_return))\n", sig->rarg->release_check);
		  fprintf (fout, "    %s (v_return);\n", sig->rarg->release);
			   
		}
	      else
		{
		  fprintf (fout, "  %s (v_return);\n", sig->rarg->release);
		}
	    }
	}

      /* cfile marshal footer */
      fprintf (fout, "}\n");
    }
}

static void
process_signature (Signature *sig)
{
  gchar *pname, *sname, *tmp;
  GList *node;

  /* lookup and complete info on arguments */
  if (!complete_out_arg (sig->rarg))
    {
      g_warning ("unknown type: %s", sig->rarg->keyword);
      return;
    }
  for (node = sig->args; node; node = node->next)
    {
      InArgument *iarg = node->data;

      if (!complete_in_arg (iarg))
	{
	  g_warning ("unknown type: %s", iarg->keyword);
	  return;
	}
    }

  /* construct requested marshaller name and technical marshaller name */
  pname = g_strconcat (sig->rarg->keyword, "_", NULL);
  sname = g_strconcat (sig->rarg->sig_name, "_", NULL);
  for (node = sig->args; node; node = node->next)
    {
      InArgument *iarg = node->data;
      gchar *tmp;

      tmp = sname;
      sname = g_strconcat (tmp, "_", iarg->sig_name, NULL);
      g_free (tmp);
      tmp = pname;
      pname = g_strconcat (tmp, "_", iarg->keyword, NULL);
      g_free (tmp);
    }

  /* introductionary comment */
  fprintf (fout, "\n/* %s", sig->rarg->keyword);
  for (node = sig->args; node; node = node->next)
    {
      InArgument *iarg = node->data;

      fprintf (fout, "%c%s", node->prev ? ',' : ':', iarg->keyword);
    }
  if (!skip_ploc)
    fprintf (fout, " (%s)", sig->ploc);
  fprintf (fout, " */\n");

  /* ensure technical marshaller exists (<marshaller_prefix>_<sname>) */
  generate_marshal (sname, sig);

  /* put out marshaller alias for requested name if required (<marshaller_prefix>_<pname>) */
  tmp = g_strconcat (marshaller_prefix, "_", pname, NULL);
  if (gen_cheader && !g_hash_table_lookup (marshallers, tmp))
    {
      fprintf (fout, "#define %s_%s\t%s_%s\n", marshaller_prefix, pname, marshaller_prefix, sname);

      g_hash_table_insert (marshallers, tmp, tmp);
    }
  else
    g_free (tmp);

  g_free (pname);
  g_free (sname);
}

static InArgument*
new_in_arg (const gchar *pname)
{
  InArgument *iarg = g_new0 (InArgument, 1);

  iarg->keyword = g_strdup (pname);

  return iarg;
}

static OutArgument*
new_out_arg (const gchar *pname)
{
  OutArgument *oarg = g_new0 (OutArgument, 1);

  oarg->keyword = g_strdup (pname);

  return oarg;
}

static guint
parse_line (GScanner  *scanner,
	    Signature *sig)
{
  /* parse identifier for return value */
  if (g_scanner_get_next_token (scanner) != G_TOKEN_IDENTIFIER)
    return G_TOKEN_IDENTIFIER;
  sig->rarg = new_out_arg (scanner->value.v_identifier);

  /* keep a note on the location */
  sig->ploc = g_strdup_printf ("%s:%u", scanner->input_name, scanner->line);

  /* expect ':' */
  if (g_scanner_get_next_token (scanner) != ':')
    return ':';

  /* parse first argument */
  if (g_scanner_get_next_token (scanner) != G_TOKEN_IDENTIFIER)
    return G_TOKEN_IDENTIFIER;
  sig->args = g_list_append (sig->args, new_in_arg (scanner->value.v_identifier));

  /* parse rest of argument list */
  while (g_scanner_peek_next_token (scanner) == ',')
    {
      /* eat comma */
      g_scanner_get_next_token (scanner);

      /* parse arg identifier */
      if (g_scanner_get_next_token (scanner) != G_TOKEN_IDENTIFIER)
	return G_TOKEN_IDENTIFIER;
      sig->args = g_list_append (sig->args, new_in_arg (scanner->value.v_identifier));
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
  const gchar *gruntime_marshallers[] = {
#include	"gmarshal.strings"
  };
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

  /* add GRuntime standard marshallers */
  if (std_includes)
    for (i = 0; i < sizeof (gruntime_marshallers) / sizeof (gruntime_marshallers[0]); i++)
      {
	gchar *tmp = g_strdup (gruntime_marshallers[i]);
	
	g_hash_table_insert (marshallers, tmp, tmp);
      }

  /* put out initial heading */
  fprintf (fout, "\n");
  if (gen_cheader)
    {
      if (std_includes)
	fprintf (fout, "#include\t<gobject/gmarshal.h>\n\n");
      fprintf (fout, "G_BEGIN_DECLS\n");
    }

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
		  g_free (signature.rarg->keyword);
		g_free (signature.rarg);
		for (node = signature.args; node; node = node->next)
		  {
		    InArgument *iarg = node->data;
		    
		    g_free (iarg->keyword);
		    g_free (iarg);
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

  /* put out trailer */
  if (gen_cheader)
    {
      fprintf (fout, "\nG_END_DECLS\n");
    }
  fprintf (fout, "\n");

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
      else if (strcmp ("--nostdinc", argv[i]) == 0)
	{
	  std_includes = FALSE;
	  argv[i] = NULL;
	}
      else if (strcmp ("--stdinc", argv[i]) == 0)
	{
	  std_includes = TRUE;
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
      fprintf (bout, "  --header                   generate C headers\n");
      fprintf (bout, "  --body                     generate C code\n");
      fprintf (bout, "  --prefix=string            specify marshaller prefix\n");
      fprintf (bout, "  --skip-source              skip source location comments\n");
      fprintf (bout, "  --stdinc, --nostdinc       include/use GRuntime standard marshallers\n");
      fprintf (bout, "  -h, --help                 show this help message\n");
      fprintf (bout, "  -v, --version              print version informations\n");
      fprintf (bout, "  --g-fatal-warnings         make warnings fatal (abort)\n");
    }
}
