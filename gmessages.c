/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
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
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "glib.h"


/* --- structures --- */
typedef struct _GLogDomain	GLogDomain;
typedef struct _GLogHandler	GLogHandler;
struct _GLogDomain
{
  gchar		*log_domain;
  GLogLevelFlags fatal_mask;
  GLogHandler	*handlers;
  GLogDomain	*next;
};
struct _GLogHandler
{
  guint		 id;
  GLogLevelFlags log_level;
  GLogFunc	 log_func;
  gpointer	 data;
  GLogHandler	*next;
};


/* --- variables --- */
const gchar	     *g_log_domain_glib = "GLib";
static GLogDomain    *g_log_domains = NULL;
static GLogLevelFlags g_log_always_fatal = G_LOG_FATAL_MASK;
static GPrintFunc     glib_print_func = NULL;
static GPrintFunc     glib_printerr_func = NULL;
static GErrorFunc     glib_error_func = NULL;
static GWarningFunc   glib_warning_func = NULL;
static GPrintFunc     glib_message_func = NULL;


/* --- functions --- */
static inline GLogDomain*
g_log_find_domain (const gchar	  *log_domain)
{
  register GLogDomain *domain;

  domain = g_log_domains;
  while (domain)
    {
      if (strcmp (domain->log_domain, log_domain) == 0)
	return domain;
      domain = domain->next;
    }
  return NULL;
}

static inline GLogDomain*
g_log_domain_new (const gchar *log_domain)
{
  register GLogDomain *domain;

  domain = g_new (GLogDomain, 1);
  domain->log_domain = g_strdup (log_domain);
  domain->fatal_mask = G_LOG_FATAL_MASK;
  domain->handlers = NULL;
  domain->next = g_log_domains;
  g_log_domains = domain;
  
  return domain;
}

static inline void
g_log_domain_check_free (GLogDomain *domain)
{
  if (domain->fatal_mask == G_LOG_FATAL_MASK &&
      domain->handlers == NULL)
    {
      register GLogDomain *last, *work;
      
      last = NULL;
      work = g_log_domains;
      while (work)
	{
	  if (work == domain)
	    {
	      if (last)
		last->next = domain->next;
	      else
		g_log_domains = domain->next;
	      g_free (domain->log_domain);
	      g_free (domain);
	      break;
	    }
	  work = work->next;
	}
    }
}

static inline GLogFunc
g_log_domain_get_handler (GLogDomain	*domain,
			  GLogLevelFlags log_level,
			  gpointer	*data)
{
  if (domain && log_level)
    {
      register GLogHandler *handler;
      
      handler = domain->handlers;
      while (handler)
	{
	  if ((handler->log_level & log_level) == log_level)
	    {
	      *data = handler->data;
	      return handler->log_func;
	    }
	  handler = handler->next;
	}
    }
  return g_log_default_handler;
}

GLogLevelFlags
g_log_set_always_fatal (GLogLevelFlags fatal_mask)
{
  GLogLevelFlags old_mask;

  /* restrict the global mask to levels that are known to glib */
  fatal_mask &= (1 << G_LOG_LEVEL_USER_SHIFT) - 1;
  /* force errors to be fatal */
  fatal_mask |= G_LOG_LEVEL_ERROR;
  /* remove bogus flag */
  fatal_mask &= ~G_LOG_FLAG_FATAL;

  old_mask = g_log_always_fatal;
  g_log_always_fatal = fatal_mask;

  return old_mask;
}

GLogLevelFlags
g_log_set_fatal_mask (const gchar    *log_domain,
		      GLogLevelFlags  fatal_mask)
{
  GLogLevelFlags old_flags;
  register GLogDomain *domain;
  
  if (!log_domain)
    log_domain = "";
  
  /* force errors to be fatal */
  fatal_mask |= G_LOG_LEVEL_ERROR;
  /* remove bogus flag */
  fatal_mask &= ~G_LOG_FLAG_FATAL;
  
  domain = g_log_find_domain (log_domain);
  if (!domain)
    domain = g_log_domain_new (log_domain);
  old_flags = domain->fatal_mask;
  
  domain->fatal_mask = fatal_mask;
  g_log_domain_check_free (domain);
  
  return old_flags;
}

guint
g_log_set_handler (const gchar	  *log_domain,
		   GLogLevelFlags  log_levels,
		   GLogFunc	   log_func,
		   gpointer	   user_data)
{
  register GLogDomain *domain;
  register GLogHandler *handler;
  static guint handler_id = 0;
  
  g_return_val_if_fail ((log_levels & G_LOG_LEVEL_MASK) != 0, 0);
  g_return_val_if_fail (log_func != NULL, 0);
  
  if (!log_domain)
    log_domain = "";
  
  domain = g_log_find_domain (log_domain);
  if (!domain)
    domain = g_log_domain_new (log_domain);
  
  handler = g_new (GLogHandler, 1);
  handler->id = ++handler_id;
  handler->log_level = log_levels;
  handler->log_func = log_func;
  handler->data = user_data;
  handler->next = domain->handlers;
  domain->handlers = handler;
  
  return handler_id;
}

void
g_log_remove_handler (const gchar    *log_domain,
		      guint	      handler_id)
{
  register GLogDomain *domain;
  
  g_return_if_fail (handler_id > 0);
  
  if (!log_domain)
    log_domain = "";
  
  domain = g_log_find_domain (log_domain);
  if (domain)
    {
      register GLogHandler *work, *last;
      
      last = NULL;
      work = domain->handlers;
      while (work)
	{
	  if (work->id == handler_id)
	    {
	      if (last)
		last->next = work->next;
	      else
		domain->handlers = work->next;
	      g_free (work);
	      g_log_domain_check_free (domain);
	      return;
	    }
	  work = work->next;
	}
    }
  g_warning ("g_log_remove_handler(): could not find handler with id `%d' for domain \"%s\"",
	     handler_id,
	     log_domain);
}

void
g_logv (const gchar    *log_domain,
	GLogLevelFlags	log_level,
	const gchar    *format,
	va_list	        args1)
{
  va_list args2;
  gchar buffer[1025];
  register gint i;
  
  log_level &= G_LOG_LEVEL_MASK;
  if (!log_level)
    return;
  
  /* we use a stack buffer of fixed size, because we might get called
   * recursively.
   */
  G_VA_COPY (args2, args1);
  if (g_printf_string_upper_bound (format, args1) < 1024)
    vsprintf (buffer, format, args2);
  else
    {
      /* since we might be out of memory, we can't use g_vsnprintf(). */
#ifdef  HAVE_VSNPRINTF
      vsnprintf (buffer, 1024, format, args2);
#else	/* !HAVE_VSNPRINTF */
      /* we are out of luck here */
      strncpy (buffer, format, 1024);
#endif	/* !HAVE_VSNPRINTF */
      buffer[1024] = 0;
    }
  va_end (args2);
  
  for (i = g_bit_nth_msf (log_level, -1); i >= 0; i = g_bit_nth_msf (log_level, i))
    {
      register GLogLevelFlags test_level;
      
      test_level = 1 << i;
      if (log_level & test_level)
	{
	  static guint g_log_depth = 0;
	  GLogDomain *domain;
	  GLogFunc log_func;
	  gpointer data = NULL;
	  
	  domain = g_log_find_domain (log_domain ? log_domain : "");
	  
	  if (g_log_depth++)
	    test_level |= G_LOG_FLAG_RECURSION;
	  
	  if ((((domain ? domain->fatal_mask : G_LOG_FATAL_MASK) | g_log_always_fatal) &
	       test_level) != 0)
	    test_level |= G_LOG_FLAG_FATAL;
	  log_func = g_log_domain_get_handler (domain, test_level, &data);
	  log_func (log_domain, test_level, buffer, data);
	  
	  /* *domain can be cluttered now */
	  
	  if (test_level & G_LOG_FLAG_FATAL)
	    abort ();
	  
	  g_log_depth--;
	}
    }
}

void
g_log (const gchar    *log_domain,
       GLogLevelFlags  log_level,
       const gchar    *format,
       ...)
{
  va_list args;
  
  va_start (args, format);
  g_logv (log_domain, log_level, format, args);
  va_end (args);
}

void
g_log_default_handler (const gchar    *log_domain,
		       GLogLevelFlags  log_level,
		       const gchar    *message,
		       gpointer	       unused_data)
{
  gint fd;
  gboolean in_recursion;
  gboolean is_fatal;
  
  in_recursion = (log_level & G_LOG_FLAG_RECURSION) != 0;
  is_fatal = (log_level & G_LOG_FLAG_FATAL) != 0;
  log_level &= G_LOG_LEVEL_MASK;
  
  if (!message)
    message = "g_log_default_handler(): (NULL) message";
  
  fd = (log_level >= G_LOG_LEVEL_MESSAGE) ? 1 : 2;
  
  switch (log_level)
    {
    case G_LOG_LEVEL_ERROR:
      if (!log_domain && glib_error_func)
	{
	  /* compatibility code */
	  glib_error_func (message);
	  return;
	}
      /* use write(2) for output, in case we are out of memeory */
      if (log_domain)
	{
	  write (fd, "\n", 1);
	  write (fd, log_domain, strlen (log_domain));
	  write (fd, "-", 1);
	}
      else
	write (fd, "\n** ", 4);
      if (in_recursion)
	write (fd, "ERROR (recursed) **: ", 21);
      else
	write (fd, "ERROR **: ", 10);
      write (fd, message, strlen(message));
      if (is_fatal)
	write (fd, "\naborting...\n", 13);
      else
	write (fd, "\n", 1);
      break;
    case G_LOG_LEVEL_CRITICAL:
      if (log_domain)
	{
	  write (fd, "\n", 1);
	  write (fd, log_domain, strlen (log_domain));
	  write (fd, "-", 1);
	}
      else
	write (fd, "\n** ", 4);
      if (in_recursion)
	write (fd, "CRITICAL (recursed) **: ", 24);
      else
	write (fd, "CRITICAL **: ", 13);
      write (fd, message, strlen(message));
      if (is_fatal)
	write (fd, "\naborting...\n", 13);
      else
	write (fd, "\n", 1);
      break;
    case G_LOG_LEVEL_WARNING:
      if (!log_domain && glib_warning_func)
	{
	  /* compatibility code */
	  glib_warning_func (message);
	  return;
	}
      if (log_domain)
	{
	  write (fd, "\n", 1);
	  write (fd, log_domain, strlen (log_domain));
	  write (fd, "-", 1);
	}
      else
	write (fd, "\n** ", 4);
      if (in_recursion)
	write (fd, "WARNING (recursed) **: ", 23);
      else
	write (fd, "WARNING **: ", 12);
      write (fd, message, strlen(message));
      if (is_fatal)
	write (fd, "\naborting...\n", 13);
      else
	write (fd, "\n", 1);
      break;
    case G_LOG_LEVEL_MESSAGE:
      if (!log_domain && glib_message_func)
	{
	  /* compatibility code */
	  glib_message_func (message);
	  return;
	}
      if (log_domain)
	{
	  write (fd, log_domain, strlen (log_domain));
	  write (fd, "-", 1);
	}
      if (in_recursion)
	write (fd, "Message (recursed): ", 20);
      else
	write (fd, "Message: ", 9);
      write (fd, message, strlen(message));
      if (is_fatal)
	write (fd, "\naborting...\n", 13);
      else
	write (fd, "\n", 1);
      break;
    case G_LOG_LEVEL_INFO:
      if (log_domain)
	{
	  write (fd, log_domain, strlen (log_domain));
	  write (fd, "-", 1);
	}
      if (in_recursion)
	write (fd, "INFO (recursed): ", 17);
      else
	write (fd, "INFO: ", 6);
      write (fd, message, strlen(message));
      if (is_fatal)
	write (fd, "\naborting...\n", 13);
      else
	write (fd, "\n", 1);
      break;
    case G_LOG_LEVEL_DEBUG:
      if (log_domain)
	{
	  write (fd, log_domain, strlen (log_domain));
	  write (fd, "-", 1);
	}
      if (in_recursion)
	write (fd, "DEBUG (recursed): ", 18);
      else
	write (fd, "DEBUG: ", 7);
      write (fd, message, strlen(message));
      if (is_fatal)
	write (fd, "\naborting...\n", 13);
      else
	write (fd, "\n", 1);
      break;
    default:
      /* we are used for a log level that is not defined by GLib itself,
       * try to make the best out of it.
       */
      if (log_domain)
	{
	  write (fd, log_domain, strlen (log_domain));
	  if (in_recursion)
	    write (fd, "-LOG (recursed:", 15);
	  else
	    write (fd, "-LOG (", 6);
	}
      else if (in_recursion)
	write (fd, "LOG (recursed:", 14);
      else
	write (fd, "LOG (", 5);
      if (log_level)
	{
	  gchar string[] = "0x00): ";
	  gchar *p = string + 2;
	  guint i;
	  
	  i = g_bit_nth_msf (log_level, -1);
	  *p = i >> 4;
	  p++;
	  *p = '0' + (i & 0xf);
	  if (*p > '9')
	    *p += 'A' - '9' - 1;
	  
	  write (fd, string, 7);
	}
      else
	write (fd, "): ", 3);
      write (fd, message, strlen(message));
      if (is_fatal)
	write (fd, "\naborting...\n", 13);
      else
	write (fd, "\n", 1);
      break;
    }
}

GPrintFunc
g_set_print_handler (GPrintFunc func)
{
  GPrintFunc old_print_func;
  
  old_print_func = glib_print_func;
  glib_print_func = func;
  
  return old_print_func;
}

void
g_print (const gchar *format,
	 ...)
{
  va_list args;
  gchar *string;
  
  g_return_if_fail (format != NULL);
  
  va_start (args, format);
  string = g_strdup_vprintf (format, args);
  va_end (args);
  
  if (glib_print_func)
    glib_print_func (string);
  else
    {
      fputs (string, stdout);
      fflush (stdout);
    }
  g_free (string);
}

GPrintFunc
g_set_printerr_handler (GPrintFunc func)
{
  GPrintFunc old_printerr_func;
  
  old_printerr_func = glib_printerr_func;
  glib_printerr_func = func;
  
  return old_printerr_func;
}

void
g_printerr (const gchar *format,
	    ...)
{
  va_list args;
  gchar *string;
  
  g_return_if_fail (format != NULL);
  
  va_start (args, format);
  string = g_strdup_vprintf (format, args);
  va_end (args);
  
  if (glib_printerr_func)
    glib_printerr_func (string);
  else
    {
      fputs (string, stderr);
      fflush (stderr);
    }
  g_free (string);
}

/* compatibility code */
GErrorFunc
g_set_error_handler (GErrorFunc func)
{
  GErrorFunc old_error_func;
  
  old_error_func = glib_error_func;
  glib_error_func = func;
  
  return old_error_func;
}

/* compatibility code */
GWarningFunc
g_set_warning_handler (GWarningFunc func)
{
  GWarningFunc old_warning_func;
  
  old_warning_func = glib_warning_func;
  glib_warning_func = func;
  
  return old_warning_func;
}

/* compatibility code */
GPrintFunc
g_set_message_handler (GPrintFunc func)
{
  GPrintFunc old_message_func;
  
  old_message_func = glib_message_func;
  glib_message_func = func;
  
  return old_message_func;
}
