/* goption.c - Option parser
 *
 *  Copyright (C) 1999, 2003 Red Hat Software
 *  Copyright (C) 2004       Anders Carlsson <andersca@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "goption.h"

#include "glib.h"
#include "gi18n.h"

#include <string.h>
#include <stdlib.h>
#include <errno.h>

#define TRANSLATE(group, str) (((group)->translate_func ? (* (group)->translate_func) ((str), (group)->translate_data) : (str)))

typedef struct {
  GOptionArg arg_type;
  gpointer arg_data;  
  union {
    gboolean bool;
    gint integer;
    gchar *str;
    gchar **array;
  } prev;
  union {
    gchar *str;
    struct {
      gint len;
      gchar **data;
    } array;
  } allocated;
} Change;

typedef struct
{
  gchar **ptr;
  gchar *value;
} PendingNull;

struct _GOptionContext
{
  GList *groups;

  gchar *parameter_string;

  gboolean help_enabled;
  gboolean ignore_unknown;
  
  GOptionGroup *main_group;

  /* We keep a list of change so we can revert them */
  GList *changes;
  
  /* We also keep track of all argv elements that should be NULLed or
   * modified.
   */
  GList *pending_nulls;
};

struct _GOptionGroup
{
  gchar *name;
  gchar *description;
  gchar *help_description;

  GDestroyNotify  destroy_notify;
  gpointer        user_data;

  GTranslateFunc  translate_func;
  GDestroyNotify  translate_notify;
  gpointer	  translate_data;

  GOptionEntry *entries;
  gint         n_entries;

  GOptionParseFunc pre_parse_func;
  GOptionParseFunc post_parse_func;
  GOptionErrorFunc error_func;
};

static void free_changes_list (GOptionContext *context,
			       gboolean        revert);
static void free_pending_nulls (GOptionContext *context,
				gboolean        perform_nulls);

GQuark
g_option_context_error_quark (void)
{
  static GQuark q = 0;
  
  if (q == 0)
    q = g_quark_from_static_string ("g-option-context-error-quark");

  return q;
}

GOptionContext *
g_option_context_new (const gchar *parameter_string)

{
  GOptionContext *context;

  context = g_new0 (GOptionContext, 1);

  context->parameter_string = g_strdup (parameter_string);
  context->help_enabled = TRUE;

  return context;
}

void
g_option_context_free (GOptionContext *context)
{
  g_return_if_fail (context != NULL);

  g_list_foreach (context->groups, (GFunc)g_option_group_free, NULL);
  g_list_free (context->groups);

  g_option_group_free (context->main_group);

  free_changes_list (context, FALSE);
  free_pending_nulls (context, FALSE);
  
  g_free (context->parameter_string);
  
  g_free (context);
}

void
g_option_context_set_help_enabled (GOptionContext *context,
				   gboolean        help_enabled)

{
  g_return_if_fail (context != NULL);

  context->help_enabled = help_enabled;
}

gboolean
g_option_context_get_help_enabled (GOptionContext *context)
{
  g_return_val_if_fail (context != NULL, FALSE);

  return context->help_enabled;
}

void
g_option_context_set_ignore_unknown_options (GOptionContext *context,
					     gboolean	     ignore_unknown)
{
  g_return_if_fail (context != NULL);

  context->ignore_unknown = ignore_unknown;
}

gboolean
g_option_context_get_ignore_unknown_options (GOptionContext *context)
{
  g_return_val_if_fail (context != NULL, FALSE);

  return context->ignore_unknown;
}

void
g_option_context_add_group (GOptionContext *context,
			    GOptionGroup   *group)
{
  g_return_if_fail (context != NULL);
  g_return_if_fail (group != NULL);
  g_return_if_fail (group->name != NULL);
  g_return_if_fail (group->description != NULL);
  g_return_if_fail (group->help_description != NULL);

  context->groups = g_list_prepend (context->groups, group);
}

void
g_option_context_set_main_group (GOptionContext *context,
				 GOptionGroup   *group)
{
  g_return_if_fail (context != NULL);
  g_return_if_fail (group != NULL);

  context->main_group = group;
}

GOptionGroup *
g_option_context_get_main_group (GOptionContext *context)
{
  g_return_val_if_fail (context != NULL, NULL);

  return context->main_group;
}

void
g_option_context_add_main_entries (GOptionContext      *context,
				   const GOptionEntry  *entries,
				   const gchar         *translation_domain)
{
  g_return_if_fail (entries != NULL);

  if (!context->main_group)
    context->main_group = g_option_group_new (NULL, NULL, NULL, NULL, NULL);
  
  g_option_group_add_entries (context->main_group, entries);
  g_option_group_set_translation_domain (context->main_group, translation_domain);
}

static void
print_entry (GOptionGroup       *group,
	     gint                max_length,
	     const GOptionEntry *entry)
{
  GString *str;

  if (entry->flags & G_OPTION_FLAG_HIDDEN)
    return;
	  
  str = g_string_new (NULL);
  
  if (entry->short_name)
    g_string_append_printf (str, "  -%c, --%s", entry->short_name, entry->long_name);
  else
    g_string_append_printf (str, "  --%s", entry->long_name);
  
  if (entry->arg_description)
    g_string_append_printf (str, "=%s", TRANSLATE (group, entry->arg_description));
  
  g_print ("%-*s %s\n", max_length + 4, str->str,
	   entry->description ? TRANSLATE (group, entry->description) : "");
  g_string_free (str, TRUE);  
}

static void
print_help (GOptionContext *context,
	    gboolean        main_help,
	    GOptionGroup   *group)
{
  GList *list;
  gint max_length, len;
  gint i;

  g_print ("Usage:\n");
  g_print ("  %s [OPTION...] %s\n\n", g_get_prgname (),
	   context->parameter_string ? context->parameter_string : "");

  list = context->groups;

  max_length = g_utf8_strlen ("--help, -?", -1);

  if (list)
    {
      len = g_utf8_strlen ("--help-all", -1);
      max_length = MAX (max_length, len);
    }

  while (list != NULL)
    {
      GOptionGroup *group = list->data;
      
      /* First, we check the --help-<groupname> options */
      len = g_utf8_strlen ("--help-", -1) + g_utf8_strlen (group->name, -1);
      max_length = MAX (max_length, len);

      /* Then we go through the entries */
      for (i = 0; i < group->n_entries; i++)
	{
	  len = g_utf8_strlen (group->entries[i].long_name, -1);

	  if (group->entries[i].short_name)
	    len += 4;

	  if (group->entries[i].arg != G_OPTION_ARG_NONE &&
	      group->entries[i].arg_description)
	    len += 1 + g_utf8_strlen (TRANSLATE (group, group->entries[i].arg_description), -1);

	  max_length = MAX (max_length, len);
	}
      
      list = list->next;
    }

  /* Add a bit of padding */
  max_length += 4;
  
  list = context->groups;

  g_print ("Help Options:\n");
  g_print ("  --%-*s %s\n", max_length, "help", "Show help options");

  /* We only want --help-all when there are groups */
  if (list)
    g_print ("  --%-*s %s\n", max_length, "help-all", "Show all help options");

  while (list)
    {
      GOptionGroup *group = list->data;

      g_print ("  --help-%-*s %s\n", max_length - 5, group->name, TRANSLATE (group, group->help_description));
      
      list = list->next;
    }

  g_print ("\n");

  if (group)
    {
      /* Print a certain group */
      
      g_print ("%s\n", TRANSLATE (group, group->description));
      for (i = 0; i < group->n_entries; i++)
	print_entry (group, max_length, &group->entries[i]);
      g_print ("\n");
    }
  else if (!main_help)
    {
      /* Print all groups */

      list = context->groups;

      while (list)
	{
	  GOptionGroup *group = list->data;

	  g_print ("%s\n", group->description);

	  for (i = 0; i < group->n_entries; i++)
	    if (!(group->entries[i].flags & G_OPTION_FLAG_IN_MAIN))
	      print_entry (group, max_length, &group->entries[i]);
	  
	  g_print ("\n");
	  list = list->next;
	}
    }
  
  /* Print application options if --help or --help-all has been specified */
  if (main_help || !group)
    {
      list = context->groups;

      g_print ("Application Options\n");

      for (i = 0; i < context->main_group->n_entries; i++) 
	print_entry (context->main_group, max_length, &context->main_group->entries[i]);

      while (list != NULL)
	{
	  GOptionGroup *group = list->data;

	  /* Print main entries from other groups */
	  for (i = 0; i < group->n_entries; i++)
	    if (group->entries[i].flags & G_OPTION_FLAG_IN_MAIN)
	      print_entry (group, max_length, &group->entries[i]);
	  
	  list = list->next;
	}

      g_print ("\n");
    }

  
  exit (0);
}

static gboolean
parse_int (const gchar *arg_name,
	   const gchar *arg,
	   gint        *result,
	   GError     **error)
{
  gchar *end;
  glong tmp = strtol (arg, &end, 0);

  errno = 0;
  
  if (*arg == '\0' || *end != '\0')
    {
      g_set_error (error,
		   G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
		   "Cannot parse integer value '%s' for --%s",
		   arg, arg_name);
      return FALSE;
    }

  *result = tmp;
  if (*result != tmp || errno == ERANGE)
    {
      g_set_error (error,
		   G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
		   "Integer value '%s' for %s out of range",
		   arg, arg_name);
      return FALSE;
    }

  return TRUE;
}

static Change *
get_change (GOptionContext *context,
	    GOptionArg      arg_type,
	    gpointer        arg_data)
{
  GList *list;
  Change *change = NULL;
  
  for (list = context->changes; list != NULL; list = list->next)
    {
      change = list->data;

      if (change->arg_data == arg_data)
	break;
    }

  if (!change)
    {
      change = g_new0 (Change, 1);
      change->arg_type = arg_type;
      change->arg_data = arg_data;

      context->changes = g_list_prepend (context->changes, change);
    }
  
  return change;
}

static void
add_pending_null (GOptionContext *context,
		  gchar         **ptr,
		  gchar          *value)
{
  PendingNull *n;

  n = g_new0 (PendingNull, 1);
  n->ptr = ptr;
  n->value = value;

  context->pending_nulls = g_list_prepend (context->pending_nulls, n);
}
		  
static gboolean
parse_arg (GOptionContext *context,
	   GOptionGroup   *group,
	   GOptionEntry   *entry,
	   const gchar    *value,
	   const gchar    *option_name,
	   GError        **error)
     
{
  Change *change;
  
  switch (entry->arg)
    {
    case G_OPTION_ARG_NONE:
      {
	change = get_change (context, G_OPTION_ARG_NONE,
			     entry->arg_data);

	*(gboolean *)entry->arg_data = TRUE;
 	break;
      }	     
    case G_OPTION_ARG_STRING:
      {
	gchar *data;

	data = g_locale_to_utf8 (value, -1, NULL, NULL, error);

	if (!data)
	  return FALSE;

	change = get_change (context, G_OPTION_ARG_STRING,
			     entry->arg_data);
	g_free (change->allocated.str);
	
	change->prev.str = *(gchar **)entry->arg_data;
	change->allocated.str = data;
	
	*(gchar **)entry->arg_data = data;
	break;
      }
    case G_OPTION_ARG_STRING_ARRAY:
      {
	gchar *data;
	
	data = g_locale_to_utf8 (value, -1, NULL, NULL, error);

	if (!data)
	  return FALSE;

	change = get_change (context, G_OPTION_ARG_STRING_ARRAY,
			     entry->arg_data);

	if (change->allocated.array.len == 0)
	  {
	    change->prev.array = entry->arg_data;
	    change->allocated.array.data = g_new (gchar *, 2);
	  }
	else
	  change->allocated.array.data =
	    g_renew (gchar *, change->allocated.array.data,
		     change->allocated.array.len + 2);

	change->allocated.array.data[change->allocated.array.len] = data;
	change->allocated.array.data[change->allocated.array.len + 1] = NULL;

	change->allocated.array.len ++;

	*(gchar ***)entry->arg_data = change->allocated.array.data;

	break;
      }
      
    case G_OPTION_ARG_FILENAME:
      {
	gchar *data;

	data = g_strdup (value);

	change = get_change (context, G_OPTION_ARG_FILENAME,
			     entry->arg_data);
	g_free (change->allocated.str);
	
	change->prev.str = *(gchar **)entry->arg_data;
	change->allocated.str = data;

	break;
      }

    case G_OPTION_ARG_FILENAME_ARRAY:
      {
	gchar *data;
	
	data = g_strdup (value);

	change = get_change (context, G_OPTION_ARG_STRING_ARRAY,
			     entry->arg_data);

	if (change->allocated.array.len == 0)
	  {
	    change->prev.array = entry->arg_data;
	    change->allocated.array.data = g_new (gchar *, 2);
	  }
	else
	  change->allocated.array.data =
	    g_renew (gchar *, change->allocated.array.data,
		     change->allocated.array.len + 2);

	change->allocated.array.data[change->allocated.array.len] = data;
	change->allocated.array.data[change->allocated.array.len + 1] = NULL;

	change->allocated.array.len ++;

	*(gchar ***)entry->arg_data = change->allocated.array.data;

	break;
      }
      
    case G_OPTION_ARG_INT:
      {
	gint data;

	if (!parse_int (option_name, value,
 			&data,
			error))
	  return FALSE;

	change = get_change (context, G_OPTION_ARG_INT,
			     entry->arg_data);
	change->prev.integer = *(gint *)entry->arg_data;
	*(gint *)entry->arg_data = data;
      }
      break;
    case G_OPTION_ARG_CALLBACK:
      {
	gchar *tmp;
	gboolean retval;
	
	tmp = g_locale_to_utf8 (value, -1, NULL, NULL, error);

	if (!value)
	  return FALSE;

	retval = (* (GOptionArgFunc) entry->arg_data) (option_name, tmp, group->user_data, error);
	
	g_free (tmp);
	
	return retval;
	
	break;
      }
    default:
      g_assert_not_reached ();
    }

  return TRUE;
}

static gboolean
parse_short_option (GOptionContext *context,
		    GOptionGroup   *group,
		    gint            index,
		    gint           *new_index,
		    gchar           arg,
		    gint           *argc,
		    gchar        ***argv,
		    GError        **error,
		    gboolean       *parsed)
{
  gint j;
    
  for (j = 0; j < group->n_entries; j++)
    {
      if (arg == group->entries[j].short_name)
	{
	  if (group->entries[j].arg == G_OPTION_ARG_NONE)
	    {
	      parse_arg (context, group, &group->entries[j],
			 NULL, NULL, error);
	      *parsed = TRUE;
	    }
	  else
	    {
	      gchar *value = NULL;
	      gchar *option_name;
	      
	      if (*new_index > index)
		{
		  g_warning ("FIXME: figure out the correct error here");

		  return FALSE;
		}
	      
	      if (index < *argc - 1)
		{
		  value = (*argv)[index + 1];
		  add_pending_null (context, &((*argv)[index + 1]), NULL);
		  *new_index = index + 1;
		}
	      else
		value = "";


	      option_name = g_strdup_printf ("-%c", group->entries[j].short_name);
	      
	      if (!parse_arg (context, group, &group->entries[j], value, option_name, error))
		{
		  g_free (option_name);
		  return FALSE;
		}

	      g_free (option_name);
	      *parsed = TRUE;
	    }
	}
    }

  return TRUE;
}

static gboolean
parse_long_option (GOptionContext *context,
		   GOptionGroup   *group,
		   gint           *index,
		   gchar          *arg,
		   gint           *argc,
		   gchar        ***argv,
		   GError        **error,
		   gboolean       *parsed)
{
  gint j;

  for (j = 0; j < group->n_entries; j++)
    {
      if (*index >= *argc)
	return TRUE;

      if (group->entries[j].arg == G_OPTION_ARG_NONE &&
	  strcmp (arg, group->entries[j].long_name) == 0)
	{
	  parse_arg (context, group, &group->entries[j],
		     NULL, NULL, error);
	  
	  add_pending_null (context, &((*argv)[*index]), NULL);
	}
      else
	{
	  gint len = strlen (group->entries[j].long_name);
	  
	  if (strncmp (arg, group->entries[j].long_name, len) == 0 &&
	      (arg[len] == '=' || arg[len] == 0))
	    {
	      gchar *value = NULL;
	      gchar *option_name;

	      add_pending_null (context, &((*argv)[*index + 1]), NULL);
	      
	      if (arg[len] == '=')
		value = arg + len + 1;
	      else if (*index < *argc - 1)
		{
		  value = (*argv)[*index + 1];
		  add_pending_null (context, &((*argv)[*index + 1]), NULL);
		  (*index)++;
		}
	      else
		value = "";

	      option_name = g_strconcat ("--", group->entries[j].long_name, NULL);
	      
	      if (!parse_arg (context, group, &group->entries[j], value, option_name, error))
		{
		  g_free (option_name);
		  return FALSE;
		}

	      g_free (option_name);
	      *parsed = TRUE;
	    }
	}
    }
  
  return TRUE;
}

static void
free_changes_list (GOptionContext *context,
		   gboolean        revert)
{
  GList *list;

  for (list = context->changes; list != NULL; list = list->next)
    {
      Change *change = list->data;

      if (revert)
	{
	  /* Free any allocated data */
	  g_free (change->allocated.str);
	  g_strfreev (change->allocated.array.data);
	  
	  switch (change->arg_type)
	    {
	    case G_OPTION_ARG_NONE:
	      *(gboolean *)change->arg_data = change->prev.bool;
	      break;
	    case G_OPTION_ARG_INT:
	      *(gint *)change->arg_data = change->prev.integer;
	      break;
	    case G_OPTION_ARG_STRING:
	    case G_OPTION_ARG_FILENAME:
	      *(gchar **)change->arg_data = change->prev.str;
	      break;
	    case G_OPTION_ARG_STRING_ARRAY:
	    case G_OPTION_ARG_FILENAME_ARRAY:
	      *(gchar ***)change->arg_data = change->prev.array;
	    default:
	      g_assert_not_reached ();
	    }
	}
      
      g_free (change);
    }

  g_list_free (context->changes);
  context->changes = NULL;
}

static void
free_pending_nulls (GOptionContext *context,
		    gboolean        perform_nulls)
{
  GList *list;

  for (list = context->pending_nulls; list != NULL; list = list->next)
    {
      PendingNull *n = list->data;

      if (perform_nulls)
	{
	  if (n->value)
	    {
	      /* Copy back the short options */
	      *(n->ptr)[0] = '-';	      
	      strcpy (*n->ptr + 1, n->value);
	    }
	  else
	    *n->ptr = NULL;
	}
      
      g_free (n->value);
      g_free (n);
    }

  g_list_free (context->pending_nulls);
  context->pending_nulls = NULL;
}

gboolean
g_option_context_parse (GOptionContext   *context,
			gint             *argc,
			gchar          ***argv,
			GError          **error)
{
  gint i, j, k;
  GList *list;

  /* Call pre-parse hooks */
  list = context->groups;
  while (list)
    {
      GOptionGroup *group = list->data;
      
      if (group->pre_parse_func)
	{
	  if (!(* group->pre_parse_func) (context, group,
					  group->user_data, error))
	    goto fail;
	}
      
      list = list->next;
    }

  if (context->main_group->pre_parse_func)
    {
      if (!(* context->main_group->pre_parse_func) (context, context->main_group,
						    context->main_group->user_data, error))
	goto fail;
    }

  if (argc && argv)
    {
      for (i = 1; i < *argc; i++)
	{
	  gchar *arg;
	  gboolean parsed = FALSE;

	  if ((*argv)[i][0] == '-')
	    {
	      if ((*argv)[i][1] == '-')
		{
		  /* -- option */

		  arg = (*argv)[i] + 2;

		  /* '--' terminates list of arguments */
		  if (*arg == 0)
		    {
		      add_pending_null (context, &((*argv)[i]), NULL);
		      break;
		    }

		  /* Handle help options */
		  if (context->help_enabled)
		    {
		      if (strcmp (arg, "help") == 0)
			print_help (context, TRUE, NULL);
		      else if (strcmp (arg, "help-all") == 0)
			print_help (context, FALSE, NULL);		      
		      else if (strncmp (arg, "help-", 5) == 0)
			{
			  GList *list;
			  
			  list = context->groups;
			  
			  while (list)
			    {
			      GOptionGroup *group = list->data;
			      
			      if (strcmp (arg + 5, group->name) == 0)
				print_help (context, FALSE, group);		      			      
			      
			      list = list->next;
			    }
			}
		    }

		  if (!parse_long_option (context, context->main_group, &i, arg,
					  argc, argv, error, &parsed))
		    goto fail;

		  if (parsed)
		    continue;
		  
		  /* Try the groups */
		  list = context->groups;
		  while (list)
		    {
		      GOptionGroup *group = list->data;
		      
		      if (!parse_long_option (context, group, &i, arg,
					      argc, argv, error, &parsed))
			goto fail;
		      
		      if (parsed)
			break;
		      
		      list = list->next;
		    }

		  if (context->ignore_unknown)
		    continue;
		}
	      else
		{
		  gint new_i, j;
		  gboolean *nulled_out = NULL;
		  
		  arg = (*argv)[i] + 1;

		  new_i = i;

		  if (context->ignore_unknown)
		    nulled_out = g_new0 (gboolean, strlen (arg));
		  
		  for (j = 0; j < strlen (arg); j++)
		    {
		      parsed = FALSE;
		      
		      if (!parse_short_option (context, context->main_group,
					       i, &new_i, arg[j],
					       argc, argv, error, &parsed))
			{

			  g_free (nulled_out);
			  goto fail;
			}

		      if (!parsed)
			{
			  /* Try the groups */
			  list = context->groups;
			  while (list)
			    {
			      GOptionGroup *group = list->data;
			      
			      if (!parse_short_option (context, group, i, &new_i, arg[j],
						       argc, argv, error, &parsed))
				goto fail;
			      
			      if (parsed)
				break;
			  
			      list = list->next;
			    }
			}

		      if (context->ignore_unknown)
			{
			  if (parsed)
			    nulled_out[j] = TRUE;
			  else
			    continue;
			}

		      if (!parsed)
			break;
		    }

		  if (context->ignore_unknown)
		    {
		      gchar *new_arg = NULL; 
		      int arg_index = 0;
		      
		      for (j = 0; j < strlen (arg); j++)
			{
			  if (!nulled_out[j])
			    {
			      if (!new_arg)
				new_arg = g_malloc (strlen (arg));
			      new_arg[arg_index++] = arg[j];
			    }
			}
		      if (new_arg)
			new_arg[arg_index] = '\0';
		      
		      add_pending_null (context, &((*argv)[i]), new_arg);
		    }
		  else if (parsed)
		    {
		      add_pending_null (context, &((*argv)[i]), NULL);
		      i = new_i;
		    }
		}
	      
	      if (!parsed && !context->ignore_unknown)
		{
		  g_set_error (error,
			       G_OPTION_ERROR, G_OPTION_ERROR_UNKNOWN_OPTION,
			       _("Unknown option %s"), (*argv)[i]);
		  goto fail;
		}
	    }
	}

      /* Call post-parse hooks */
      list = context->groups;
      while (list)
	{
	  GOptionGroup *group = list->data;

	  if (group->post_parse_func)
	    {
	      if (!(* group->post_parse_func) (context, group,
					       group->user_data, error))
		goto fail;
	    }
	  
	  list = list->next;
	}

      if (context->main_group->post_parse_func)
	{
	  if (!(* context->main_group->post_parse_func) (context, context->main_group,
							 context->main_group->user_data, error))
	    goto fail;
	}

      free_pending_nulls (context, TRUE);
      
      for (i = 1; i < *argc; i++)
	{
	  for (k = i; k < *argc; k++)
	    if ((*argv)[k] != NULL)
	      break;
	  
	  if (k > i)
	    {
	      k -= i;
	      for (j = i + k; j < *argc; j++)
		(*argv)[j-k] = (*argv)[j];
	      *argc -= k;
	    }
	}
      
    }

  return TRUE;

 fail:
  
  /* Call error hooks */
  list = context->groups;
  while (list)
    {
      GOptionGroup *group = list->data;
      
      if (group->error_func)
	(* group->post_parse_func) (context, group,
				    group->user_data, error);
      
      list = list->next;
    }

  if (context->main_group->error_func)
    (* context->main_group->error_func) (context, context->main_group,
					 context->main_group->user_data, error);
  
  free_changes_list (context, TRUE);
  free_pending_nulls (context, FALSE);

  return FALSE;
}
				   
     
GOptionGroup *
g_option_group_new (const gchar    *name,
		    const gchar    *description,
		    const gchar    *help_description,
		    gpointer        user_data,
		    GDestroyNotify  destroy)

{
  GOptionGroup *group;

  group = g_new0 (GOptionGroup, 1);
  group->name = g_strdup (name);
  group->description = g_strdup (description);
  group->help_description = g_strdup (help_description);
  group->user_data = user_data;
  group->destroy_notify = destroy;
  
  return group;
}

void
g_option_group_free (GOptionGroup *group)
{
  g_return_if_fail (group != NULL);

  g_free (group->name);
  g_free (group->description);
  g_free (group->help_description);

  g_free (group->entries);
  
  if (group->destroy_notify)
    (* group->destroy_notify) (group->user_data);

  if (group->translate_notify)
    (* group->translate_notify) (group->translate_data);
  
  g_free (group);
}


void
g_option_group_add_entries (GOptionGroup       *group,
			    const GOptionEntry *entries)
{
  gint n_entries;
  gint i;
  
  g_return_if_fail (entries != NULL);

  for (n_entries = 0; entries[n_entries].long_name != NULL; n_entries++);

  group->entries = g_renew (GOptionEntry, group->entries, group->n_entries + n_entries);

  memcpy (group->entries + group->n_entries, entries, sizeof (GOptionEntry) * n_entries);

  group->n_entries += n_entries;
}

void
g_option_group_set_parse_hooks (GOptionGroup     *group,
				GOptionParseFunc  pre_parse_func,
				GOptionParseFunc  post_parse_func)
{
  g_return_if_fail (group != NULL);

  group->pre_parse_func = pre_parse_func;
  group->post_parse_func = post_parse_func;  
}

void
g_option_group_set_error_hook (GOptionGroup     *group,
			       GOptionErrorFunc	 error_func)
{
  g_return_if_fail (group != NULL);

  group->error_func = error_func;  
}


void
g_option_group_set_translate_func (GOptionGroup   *group,
				   GTranslateFunc  func,
				   gpointer        data,
				   GDestroyNotify  notify)
{
  g_return_if_fail (group != NULL);
  
  if (group->translate_notify)
    group->translate_notify (group->translate_data);
      
  group->translate_func = func;
  group->translate_data = data;
  group->translate_notify = notify;
}

static gchar *
dgettext_swapped (const gchar *msgid, 
		  const gchar *domainname)
{
  return dgettext (domainname, msgid);
}

void
g_option_group_set_translation_domain (GOptionGroup *group,
				       const gchar  *domain)
{
  g_return_if_fail (group != NULL);

  g_option_group_set_translate_func (group, 
				     (GTranslateFunc)dgettext_swapped,
				     g_strdup (domain),
				     g_free);
} 

