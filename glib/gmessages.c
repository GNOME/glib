/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GLib Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/. 
 */

/* 
 * MT safe
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "glib.h"
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <signal.h>
#include <locale.h>
#include <errno.h>
#include "gdebug.h"

#ifdef G_OS_WIN32
typedef FILE* GFileDescriptor;
#else
typedef gint GFileDescriptor;
#endif

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


/* --- prototypes --- */
#ifndef HAVE_C99_VSNPRINTF
static gsize printf_string_upper_bound (const gchar *format,
					gboolean     may_warn,
					va_list      args);
#endif /* !HAVE_C99_VSNPRINTF */


/* --- variables --- */
static GMutex        *g_messages_lock = NULL;
static GLogDomain    *g_log_domains = NULL;
static GLogLevelFlags g_log_always_fatal = G_LOG_FATAL_MASK;
static GPrintFunc     glib_print_func = NULL;
static GPrintFunc     glib_printerr_func = NULL;
static GPrivate	     *g_log_depth = NULL;
static GLogLevelFlags g_log_msg_prefix = G_LOG_LEVEL_ERROR | G_LOG_LEVEL_WARNING | G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_DEBUG;


/* --- functions --- */
#ifdef G_OS_WIN32
#  define STRICT
#  include <windows.h>
#  undef STRICT
#  include <process.h>          /* For _getpid() */
static gboolean alloc_console_called = FALSE;
static gboolean win32_keep_fatal_message = FALSE;

/* This default message will usually be overwritten. */
/* Yes, a fixed size buffer is bad. So sue me. But g_error is never
 * with huge strings, is it?
 */
static gchar  fatal_msg_buf[1000] = "Unspecified fatal error encountered, aborting.";
static gchar *fatal_msg_ptr = fatal_msg_buf;

/* Just use stdio. If we're out of memory, we're hosed anyway. */
#undef write
static inline int
dowrite (GFileDescriptor fd,
	 const void  *buf,
	 unsigned int len)
{
  if (win32_keep_fatal_message)
    {
      memcpy (fatal_msg_ptr, buf, len);
      fatal_msg_ptr += len;
      *fatal_msg_ptr = 0;
      return len;
    }

  fwrite (buf, len, 1, fd);
  fflush (fd);

  return len;
}
#define write(fd, buf, len) dowrite(fd, buf, len)

static void
ensure_stdout_valid (void)
{
  HANDLE handle;

  if (win32_keep_fatal_message)
    return;

  if (!alloc_console_called)
    {
      handle = GetStdHandle (STD_OUTPUT_HANDLE);
  
      if (handle == INVALID_HANDLE_VALUE)
	{
	  AllocConsole ();
	  alloc_console_called = TRUE;
	  freopen ("CONOUT$", "w", stdout);
	}
    }
}
#else
#define ensure_stdout_valid()	/* Define as empty */
#endif

static void
write_string (GFileDescriptor fd,
	      const gchar    *string)
{
  write (fd, string, strlen (string));
}

static void
g_messages_prefixed_init (void)
{
  static gboolean initialized = FALSE;

  if (!initialized)
    {
      const gchar *val;

      initialized = TRUE;
      val = g_getenv ("G_MESSAGES_PREFIXED");
      
      if (val)
	{
	  static const GDebugKey keys[] = {
	    { "error", G_LOG_LEVEL_ERROR },
	    { "critical", G_LOG_LEVEL_CRITICAL },
	    { "warning", G_LOG_LEVEL_WARNING },
	    { "message", G_LOG_LEVEL_MESSAGE },
	    { "info", G_LOG_LEVEL_INFO },
	    { "debug", G_LOG_LEVEL_DEBUG }
	  };
	  
	  g_log_msg_prefix = g_parse_debug_string (val, keys, G_N_ELEMENTS (keys));
	}
    }
}

static GLogDomain*
g_log_find_domain_L (const gchar *log_domain)
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

static GLogDomain*
g_log_domain_new_L (const gchar *log_domain)
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

static void
g_log_domain_check_free_L (GLogDomain *domain)
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
	  last = work;
	  work = last->next;
	}  
    }
}

static GLogFunc
g_log_domain_get_handler_L (GLogDomain	*domain,
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

  /* restrict the global mask to levels that are known to glib
   * since this setting applies to all domains
   */
  fatal_mask &= (1 << G_LOG_LEVEL_USER_SHIFT) - 1;
  /* force errors to be fatal */
  fatal_mask |= G_LOG_LEVEL_ERROR;
  /* remove bogus flag */
  fatal_mask &= ~G_LOG_FLAG_FATAL;

  g_mutex_lock (g_messages_lock);
  old_mask = g_log_always_fatal;
  g_log_always_fatal = fatal_mask;
  g_mutex_unlock (g_messages_lock);

  return old_mask;
}

GLogLevelFlags
g_log_set_fatal_mask (const gchar   *log_domain,
		      GLogLevelFlags fatal_mask)
{
  GLogLevelFlags old_flags;
  register GLogDomain *domain;
  
  if (!log_domain)
    log_domain = "";
  
  /* force errors to be fatal */
  fatal_mask |= G_LOG_LEVEL_ERROR;
  /* remove bogus flag */
  fatal_mask &= ~G_LOG_FLAG_FATAL;
  
  g_mutex_lock (g_messages_lock);

  domain = g_log_find_domain_L (log_domain);
  if (!domain)
    domain = g_log_domain_new_L (log_domain);
  old_flags = domain->fatal_mask;
  
  domain->fatal_mask = fatal_mask;
  g_log_domain_check_free_L (domain);

  g_mutex_unlock (g_messages_lock);

  return old_flags;
}

guint
g_log_set_handler (const gchar	 *log_domain,
		   GLogLevelFlags log_levels,
		   GLogFunc	  log_func,
		   gpointer	  user_data)
{
  static guint handler_id = 0;
  GLogDomain *domain;
  GLogHandler *handler;
  
  g_return_val_if_fail ((log_levels & G_LOG_LEVEL_MASK) != 0, 0);
  g_return_val_if_fail (log_func != NULL, 0);
  
  if (!log_domain)
    log_domain = "";

  handler = g_new (GLogHandler, 1);

  g_mutex_lock (g_messages_lock);

  domain = g_log_find_domain_L (log_domain);
  if (!domain)
    domain = g_log_domain_new_L (log_domain);
  
  handler->id = ++handler_id;
  handler->log_level = log_levels;
  handler->log_func = log_func;
  handler->data = user_data;
  handler->next = domain->handlers;
  domain->handlers = handler;

  g_mutex_unlock (g_messages_lock);
  
  return handler_id;
}

void
g_log_remove_handler (const gchar *log_domain,
		      guint	   handler_id)
{
  register GLogDomain *domain;
  
  g_return_if_fail (handler_id > 0);
  
  if (!log_domain)
    log_domain = "";
  
  g_mutex_lock (g_messages_lock);
  domain = g_log_find_domain_L (log_domain);
  if (domain)
    {
      GLogHandler *work, *last;
      
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
	      g_log_domain_check_free_L (domain); 
	      g_mutex_unlock (g_messages_lock);
	      g_free (work);
	      return;
	    }
	  last = work;
	  work = last->next;
	}
    } 
  g_mutex_unlock (g_messages_lock);
  g_warning ("%s: could not find handler with id `%d' for domain \"%s\"",
	     G_STRLOC, handler_id, log_domain);
}

void
g_logv (const gchar   *log_domain,
	GLogLevelFlags log_level,
	const gchar   *format,
	va_list	       args1)
{
  gchar buffer[1025];
  gboolean was_fatal = (log_level & G_LOG_FLAG_FATAL) != 0;
  gboolean was_recursion = (log_level & G_LOG_FLAG_RECURSION) != 0;
  gint i;

#ifndef  HAVE_VSNPRINTF
  va_list args2;
#endif	/* HAVE_VSNPRINTF */
  
  log_level &= G_LOG_LEVEL_MASK;
  if (!log_level)
    return;
  
  /* we use a stack buffer of fixed size, because we might get called
   * recursively.
   */
#ifdef  HAVE_VSNPRINTF
  vsnprintf (buffer, 1024, format, args1);
#else	/* !HAVE_VSNPRINTF */
  G_VA_COPY (args2, args1);
  if (printf_string_upper_bound (format, FALSE, args1) < 1024)
    vsprintf (buffer, format, args2);
  else
    {
      /* since we might be out of memory, we can't use g_vsnprintf(). */
      /* we are out of luck here */
      strncpy (buffer, format, 1024);
      buffer[1024] = 0;
    }
  va_end (args2);
#endif	/* !HAVE_VSNPRINTF */
  
  for (i = g_bit_nth_msf (log_level, -1); i >= 0; i = g_bit_nth_msf (log_level, i))
    {
      register GLogLevelFlags test_level;
      
      test_level = 1 << i;
      if (log_level & test_level)
	{
	  guint depth = GPOINTER_TO_UINT (g_private_get (g_log_depth));
	  GLogDomain *domain;
	  GLogFunc log_func;
	  guint domain_fatal_mask;
	  gpointer data = NULL;

	  if (was_fatal)
	    test_level |= G_LOG_FLAG_FATAL;
	  if (was_recursion)
	    test_level |= G_LOG_FLAG_RECURSION;

	  /* check recursion and lookup handler */
	  g_mutex_lock (g_messages_lock);
	  domain = g_log_find_domain_L (log_domain ? log_domain : "");
	  if (depth)
	    test_level |= G_LOG_FLAG_RECURSION;
	  depth++;
	  domain_fatal_mask = domain ? domain->fatal_mask : G_LOG_FATAL_MASK;
	  if ((domain_fatal_mask | g_log_always_fatal) & test_level)
	    test_level |= G_LOG_FLAG_FATAL;
	  if (test_level & G_LOG_FLAG_RECURSION)
	    log_func = _g_log_fallback_handler;
	  else
	    log_func = g_log_domain_get_handler_L (domain, test_level, &data);
	  domain = NULL;
	  g_mutex_unlock (g_messages_lock);

	  g_private_set (g_log_depth, GUINT_TO_POINTER (depth));

	  /* had to defer debug initialization until we can keep track of recursion */
	  if (!(test_level & G_LOG_FLAG_RECURSION) && !_g_debug_initialized)
	    {
	      guint orig_test_level = test_level;

	      _g_debug_init ();
	      if ((domain_fatal_mask | g_log_always_fatal) & test_level)
		test_level |= G_LOG_FLAG_FATAL;
	      if (test_level != orig_test_level)
		{
		  /* need a relookup, not nice, but not too bad either */
		  g_mutex_lock (g_messages_lock);
		  domain = g_log_find_domain_L (log_domain ? log_domain : "");
		  log_func = g_log_domain_get_handler_L (domain, test_level, &data);
		  domain = NULL;
		  g_mutex_unlock (g_messages_lock);
		}
	    }

	  log_func (log_domain, test_level, buffer, data);

	  if (test_level & G_LOG_FLAG_FATAL)
	    {
#ifdef G_OS_WIN32
	      MessageBox (NULL, fatal_msg_buf, NULL, MB_OK);
#endif
#if defined (G_ENABLE_DEBUG) && (defined (SIGTRAP) || defined (G_OS_WIN32))
	      if (!(test_level & G_LOG_FLAG_RECURSION))
		G_BREAKPOINT ();
	      else
		abort ();
#else /* !G_ENABLE_DEBUG || !(SIGTRAP || G_OS_WIN32) */
	      abort ();
#endif /* !G_ENABLE_DEBUG || !(SIGTRAP || G_OS_WIN32) */
	    }
	  
	  depth--;
	  g_private_set (g_log_depth, GUINT_TO_POINTER (depth));
	}
    }
}

void
g_log (const gchar   *log_domain,
       GLogLevelFlags log_level,
       const gchar   *format,
       ...)
{
  va_list args;
  
  va_start (args, format);
  g_logv (log_domain, log_level, format, args);
  va_end (args);
}

static gchar*
strdup_convert (const gchar *string,
		const gchar *charset)
{
  if (!g_utf8_validate (string, -1, NULL))
    return g_strconcat ("[Invalid UTF-8] ", string, NULL);
  else
    return g_convert_with_fallback (string, -1, charset, "UTF-8", "?", NULL, NULL, NULL);
}

/* For a radix of 8 we need at most 3 output bytes for 1 input
 * byte. Additionally we might need up to 2 output bytes for the
 * readix prefix and 1 byte for the trailing NULL.
 */
#define FORMAT_UNSIGNED_BUFSIZE ((GLIB_SIZEOF_LONG * 3) + 3)

static void
format_unsigned (gchar  *buf,
		 gulong  num,
		 guint   radix)
{
  gulong tmp;
  gchar c;
  gint i, n;

  /* we may not call _any_ GLib functions here (or macros like g_return_if_fail()) */

  if (radix != 8 && radix != 10 && radix != 16)
    {
      *buf = '\000';
      return;
    }
  
  if (!num)
    {
      *buf++ = '0';
      *buf = '\000';
      return;
    } 
  
  if (radix == 16)
    {
      *buf++ = '0';
      *buf++ = 'x';
    }
  else if (radix == 8)
    {
      *buf++ = '0';
    }
	
  n = 0;
  tmp = num;
  while (tmp)
    {
      tmp /= radix;
      n++;
    }

  i = n;

  /* Again we can't use g_assert; actually this check should _never_ fail. */
  if (n > FORMAT_UNSIGNED_BUFSIZE - 3)
    {
      *buf = '\000';
      return;
    }

  while (num)
    {
      i--;
      c = (num % radix);
      if (c < 10)
	buf[i] = c + '0';
      else
	buf[i] = c + 'a' - 10;
      num /= radix;
    }
  
  buf[n] = '\000';
}

/* string size big enough to hold level prefix */
#define	STRING_BUFFER_SIZE	(FORMAT_UNSIGNED_BUFSIZE + 32)

#define	ALERT_LEVELS		(G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_WARNING)

static GFileDescriptor
mklevel_prefix (gchar level_prefix[STRING_BUFFER_SIZE],
		guint log_level)
{
  gboolean to_stdout = TRUE;

  /* we may not call _any_ GLib functions here */

  switch (log_level & G_LOG_LEVEL_MASK)
    {
    case G_LOG_LEVEL_ERROR:
      strcpy (level_prefix, "ERROR");
      to_stdout = FALSE;
      break;
    case G_LOG_LEVEL_CRITICAL:
      strcpy (level_prefix, "CRITICAL");
      to_stdout = FALSE;
      break;
    case G_LOG_LEVEL_WARNING:
      strcpy (level_prefix, "WARNING");
      to_stdout = FALSE;
      break;
    case G_LOG_LEVEL_MESSAGE:
      strcpy (level_prefix, "Message");
      to_stdout = FALSE;
      break;
    case G_LOG_LEVEL_INFO:
      strcpy (level_prefix, "INFO");
      break;
    case G_LOG_LEVEL_DEBUG:
      strcpy (level_prefix, "DEBUG");
      break;
    default:
      if (log_level)
	{
	  strcpy (level_prefix, "LOG-");
	  format_unsigned (level_prefix + 4, log_level & G_LOG_LEVEL_MASK, 16);
	}
      else
	strcpy (level_prefix, "LOG");
      break;
    }
  if (log_level & G_LOG_FLAG_RECURSION)
    strcat (level_prefix, " (recursed)");
  if (log_level & ALERT_LEVELS)
    strcat (level_prefix, " **");

  ensure_stdout_valid ();
#ifdef G_OS_WIN32
  win32_keep_fatal_message = (log_level & G_LOG_FLAG_FATAL) != 0;
  /* Use just stdout as stderr is hard to get redirected from the DOS prompt. */
  return stdout;
#else
  return to_stdout ? 1 : 2;
#endif
}

void
_g_log_fallback_handler (const gchar   *log_domain,
			 GLogLevelFlags log_level,
			 const gchar   *message,
			 gpointer       unused_data)
{
  gchar level_prefix[STRING_BUFFER_SIZE], pid_string[FORMAT_UNSIGNED_BUFSIZE];
  gboolean is_fatal = (log_level & G_LOG_FLAG_FATAL) != 0;
  GFileDescriptor fd;

  /* we can not call _any_ GLib functions in this fallback handler,
   * which is why we skip UTF-8 conversion, etc.
   * since we either recursed or ran out of memory, we're in a pretty
   * pathologic situation anyways, what we can do is giving the
   * the process ID unconditionally however.
   */

  fd = mklevel_prefix (level_prefix, log_level);
  if (!message)
    message = "(NULL) message";

  format_unsigned (pid_string, getpid (), 10);

  if (log_domain)
    write_string (fd, "\n");
  else
    write_string (fd, "\n** ");
  write_string (fd, "(process:");
  write_string (fd, pid_string);
  write_string (fd, "): ");
  if (log_domain)
    {
      write_string (fd, log_domain);
      write_string (fd, "-");
    }
  write_string (fd, level_prefix);
  write_string (fd, ": ");
  write_string (fd, message);
  if (is_fatal)
    write_string (fd, "\naborting...\n");
  else
    write_string (fd, "\n");
}

void
g_log_default_handler (const gchar   *log_domain,
		       GLogLevelFlags log_level,
		       const gchar   *message,
		       gpointer	      unused_data)
{
  gboolean is_fatal = (log_level & G_LOG_FLAG_FATAL) != 0;
  gchar level_prefix[STRING_BUFFER_SIZE], *string;
  GString *gstring;
  GFileDescriptor fd;

  /* we can be called externally with recursion for whatever reason */
  if (log_level & G_LOG_FLAG_RECURSION)
    {
      _g_log_fallback_handler (log_domain, log_level, message, unused_data);
      return;
    }

  g_messages_prefixed_init ();

  fd = mklevel_prefix (level_prefix, log_level);

  gstring = g_string_new ("");
  if (log_level & ALERT_LEVELS)
    g_string_append (gstring, "\n");
  if (!log_domain)
    g_string_append (gstring, "** ");

  if ((g_log_msg_prefix & log_level) == log_level)
    {
      const gchar *prg_name = g_get_prgname ();
      
      if (!prg_name)
	g_string_append_printf (gstring, "(process:%lu): ", (gulong)getpid ());
      else
	g_string_append_printf (gstring, "(%s:%lu): ", prg_name, (gulong)getpid ());
    }

  if (log_domain)
    {
      g_string_append (gstring, log_domain);
      g_string_append_c (gstring, '-');
    }
  g_string_append (gstring, level_prefix);

  g_string_append (gstring, ": ");
  if (!message)
    g_string_append (gstring, "(NULL) message");
  else
    {
      const gchar *charset;

      if (g_get_charset (&charset))
	g_string_append (gstring, message);	/* charset is UTF-8 already */
      else
	{
	  string = strdup_convert (message, charset);
	  g_string_append (gstring, string);
	  g_free (string);
	}
    }
  if (is_fatal)
    g_string_append (gstring, "\naborting...\n");
  else
    g_string_append (gstring, "\n");

  string = g_string_free (gstring, FALSE);

  write_string (fd, string);
  g_free (string);
}

GPrintFunc
g_set_print_handler (GPrintFunc func)
{
  GPrintFunc old_print_func;
  
  g_mutex_lock (g_messages_lock);
  old_print_func = glib_print_func;
  glib_print_func = func;
  g_mutex_unlock (g_messages_lock);
  
  return old_print_func;
}

void
g_print (const gchar *format,
	 ...)
{
  va_list args;
  gchar *string;
  GPrintFunc local_glib_print_func;
  
  g_return_if_fail (format != NULL);
  
  va_start (args, format);
  string = g_strdup_vprintf (format, args);
  va_end (args);
  
  g_mutex_lock (g_messages_lock);
  local_glib_print_func = glib_print_func;
  g_mutex_unlock (g_messages_lock);
  
  if (local_glib_print_func)
    local_glib_print_func (string);
  else
    {
      const gchar *charset;

      ensure_stdout_valid ();
      if (g_get_charset (&charset))
	fputs (string, stdout); /* charset is UTF-8 already */
      else
	{
	  gchar *lstring = strdup_convert (string, charset);

	  fputs (lstring, stdout);
	  g_free (lstring);
	}
      fflush (stdout);
    }
  g_free (string);
}

GPrintFunc
g_set_printerr_handler (GPrintFunc func)
{
  GPrintFunc old_printerr_func;
  
  g_mutex_lock (g_messages_lock);
  old_printerr_func = glib_printerr_func;
  glib_printerr_func = func;
  g_mutex_unlock (g_messages_lock);
  
  return old_printerr_func;
}

void
g_printerr (const gchar *format,
	    ...)
{
  va_list args;
  gchar *string;
  GPrintFunc local_glib_printerr_func;
  
  g_return_if_fail (format != NULL);
  
  va_start (args, format);
  string = g_strdup_vprintf (format, args);
  va_end (args);
  
  g_mutex_lock (g_messages_lock);
  local_glib_printerr_func = glib_printerr_func;
  g_mutex_unlock (g_messages_lock);
  
  if (local_glib_printerr_func)
    local_glib_printerr_func (string);
  else
    {
      const gchar *charset;

      if (g_get_charset (&charset))
	fputs (string, stderr); /* charset is UTF-8 already */
      else
	{
	  gchar *lstring = strdup_convert (string, charset);

	  fputs (lstring, stderr);
	  g_free (lstring);
	}
      fflush (stderr);
    }
  g_free (string);
}

#ifndef MB_LEN_MAX
#  define MB_LEN_MAX 8
#endif

#ifndef HAVE_C99_VSNPRINTF

typedef struct
{
  guint min_width;
  guint precision;
  gboolean alternate_format, zero_padding, adjust_left, locale_grouping;
  gboolean add_space, add_sign, possible_sign, seen_precision;
  gboolean mod_half, mod_long, mod_extra_long;
} PrintfArgSpec;

static gsize
printf_string_upper_bound (const gchar *format,
			   gboolean     may_warn,
			   va_list      args)
{
  static const gboolean honour_longs = SIZEOF_LONG > 4 || SIZEOF_VOID_P > 4;
  gsize len = 1;
  
  if (!format)
    return len;
  
  while (*format)
    {
      register gchar c = *format++;
      
      if (c != '%')
	len += 1;
      else /* (c == '%') */
	{
	  PrintfArgSpec spec = { 0, };
	  gboolean seen_l = FALSE, conv_done = FALSE;
	  gsize conv_len = 0;
	  const gchar *spec_start = format;
	  
	  do
	    {
	      c = *format++;
	      switch (c)
		{
		  GDoubleIEEE754 u_double;
		  guint v_uint;
		  gint v_int;
		  const gchar *v_string;
		  
		  /* beware of positional parameters
		   */
		case '$':
		  if (may_warn)
		    g_warning (G_GNUC_PRETTY_FUNCTION
			       "(): unable to handle positional parameters (%%n$)");
		  len += 1024; /* try adding some safety padding */
		  conv_done = TRUE;
		  break;
		  
		  /* parse flags
		   */
		case '#':
		  spec.alternate_format = TRUE;
		  break;
		case '0':
		  spec.zero_padding = TRUE;
		  break;
		case '-':
		  spec.adjust_left = TRUE;
		  break;
		case ' ':
		  spec.add_space = TRUE;
		  break;
		case '+':
		  spec.add_sign = TRUE;
		  break;
		case '\'':
		  spec.locale_grouping = TRUE;
		  break;
		  
		  /* parse output size specifications
		   */
		case '.':
		  spec.seen_precision = TRUE;
		  break;
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
		  v_uint = c - '0';
		  c = *format;
		  while (c >= '0' && c <= '9')
		    {
		      format++;
		      v_uint = v_uint * 10 + c - '0';
		      c = *format;
		    }
		  if (spec.seen_precision)
		    spec.precision = MAX (spec.precision, v_uint);
		  else
		    spec.min_width = MAX (spec.min_width, v_uint);
		  break;
		case '*':
		  v_int = va_arg (args, int);
		  if (spec.seen_precision)
		    {
		      /* forget about negative precision */
		      if (v_int >= 0)
			spec.precision = MAX (spec.precision, v_int);
		    }
		  else
		    {
		      if (v_int < 0)
			{
			  v_int = - v_int;
			  spec.adjust_left = TRUE;
			}
		      spec.min_width = MAX (spec.min_width, v_int);
		    }
		  break;
		  
		  /* parse type modifiers
		   */
		case 'h':
		  spec.mod_half = TRUE;
		  break;
		case 'l':
		  if (!seen_l)
		    {
		      spec.mod_long = TRUE;
		      seen_l = TRUE;
		      break;
		    }
		  /* else, fall through */
		case 'L':
		case 'q':
		  spec.mod_long = TRUE;
		  spec.mod_extra_long = TRUE;
		  break;
		case 'z':
		case 'Z':
#if GLIB_SIZEOF_SIZE_T > 4
		  spec.mod_long = TRUE;
		  spec.mod_extra_long = TRUE;
#endif /* GLIB_SIZEOF_SIZE_T > 4 */
		  break;
		case 't':
#if GLIB_SIZEOF_PTRDIFF_T > 4
		  spec.mod_long = TRUE;
		  spec.mod_extra_long = TRUE;
#endif /* GLIB_SIZEOF_PTRDIFF_T > 4 */
		  break;
		case 'j':
#if GLIB_SIZEOF_INTMAX_T > 4
		  spec.mod_long = TRUE;
		  spec.mod_extra_long = TRUE;
#endif /* GLIB_SIZEOF_INTMAX_T > 4 */
		  break;
		  
		  /* parse output conversions
		   */
		case '%':
		  conv_len += 1;
		  break;
		case 'O':
		case 'D':
		case 'I':
		case 'U':
		  /* some C libraries feature long variants for these as well? */
		  spec.mod_long = TRUE;
		  /* fall through */
		case 'o':
		  conv_len += 2;
		  /* fall through */
		case 'd':
		case 'i':
		  conv_len += 1; /* sign */
		  /* fall through */
		case 'u':
		  conv_len += 4;
		  /* fall through */
		case 'x':
		case 'X':
		  spec.possible_sign = TRUE;
		  conv_len += 10;
		  if (spec.mod_long && honour_longs)
		    conv_len *= 2;
		  if (spec.mod_extra_long)
		    conv_len *= 2;
		  if (spec.mod_extra_long)
		    {
		      (void) va_arg (args, gint64);
		    }
		  else if (spec.mod_long)
		    (void) va_arg (args, long);
		  else
		    (void) va_arg (args, int);
		  break;
		case 'A':
		case 'a':
		  /*          0x */
		  conv_len += 2;
		  /* fall through */
		case 'g':
		case 'G':
		case 'e':
		case 'E':
		case 'f':
		  spec.possible_sign = TRUE;
		  /*          n   .   dddddddddddddddddddddddd   E   +-  eeee */
		  conv_len += 1 + 1 + MAX (24, spec.precision) + 1 + 1 + 4;
                  if (may_warn && spec.mod_extra_long)
		    g_warning (G_GNUC_PRETTY_FUNCTION
			       "(): unable to handle long double, collecting double only");
#ifdef HAVE_LONG_DOUBLE
#error need to implement special handling for long double
#endif
		  u_double.v_double = va_arg (args, double);
		  /* %f can expand up to all significant digits before '.' (308) */
		  if (c == 'f' &&
		      u_double.mpn.biased_exponent > 0 && u_double.mpn.biased_exponent < 2047)
		    {
		      gint exp = u_double.mpn.biased_exponent;
		      
		      exp -= G_IEEE754_DOUBLE_BIAS;
		      exp = exp * G_LOG_2_BASE_10 + 1;
		      conv_len += ABS (exp);	/* exp can be <0 */
		    }
		  /* some printf() implementations require extra padding for rounding */
		  conv_len += 2;
		  /* we can't really handle locale specific grouping here */
		  if (spec.locale_grouping)
		    conv_len *= 2;
		  break;
		case 'C':
		  spec.mod_long = TRUE;
                  /* fall through */
		case 'c':
		  conv_len += spec.mod_long ? MB_LEN_MAX : 1;
		  (void) va_arg (args, int);
		  break;
		case 'S':
		  spec.mod_long = TRUE;
		  /* fall through */
		case 's':
		  v_string = va_arg (args, char*);
		  if (!v_string)
		    conv_len += 8; /* hold "(null)" */
		  else if (spec.seen_precision)
		    conv_len += spec.precision;
		  else
		    conv_len += strlen (v_string);
		  conv_done = TRUE;
		  if (spec.mod_long)
		    {
		      if (may_warn)
			g_warning (G_GNUC_PRETTY_FUNCTION
				   "(): unable to handle wide char strings");
		      len += 1024; /* try adding some safety padding */
		    }
		  break;
		case 'P': /* do we actually need this? */
		  /* fall through */
		case 'p':
		  spec.alternate_format = TRUE;
		  conv_len += 10;
		  if (honour_longs)
		    conv_len *= 2;
		  /* fall through */
		case 'n':
		  conv_done = TRUE;
		  (void) va_arg (args, void*);
		  break;
		case 'm':
		  /* there's not much we can do to be clever */
		  v_string = g_strerror (errno);
		  v_uint = v_string ? strlen (v_string) : 0;
		  conv_len += MAX (256, v_uint);
		  break;
		  
		  /* handle invalid cases
		   */
		case '\000':
		  /* no conversion specification, bad bad */
		  conv_len += format - spec_start;
		  break;
		default:
		  if (may_warn)
		    g_warning (G_GNUC_PRETTY_FUNCTION
			       "(): unable to handle `%c' while parsing format",
			       c);
		  break;
		}
	      conv_done |= conv_len > 0;
	    }
	  while (!conv_done);
	  /* handle width specifications */
	  conv_len = MAX (conv_len, MAX (spec.precision, spec.min_width));
	  /* handle flags */
	  conv_len += spec.alternate_format ? 2 : 0;
	  conv_len += (spec.add_space || spec.add_sign || spec.possible_sign);
	  /* finally done */
	  len += conv_len;
	} /* else (c == '%') */
    } /* while (*format) */
  
  return len;
}

#endif /* !HAVE_C99_VSNPRINTF */


gsize
g_printf_string_upper_bound (const gchar *format,
			     va_list      args)
{
#if HAVE_C99_VSNPRINTF
  gchar c;
  return vsnprintf (&c, 1, format, args) + 1;
#else
  return printf_string_upper_bound (format, TRUE, args);
#endif
}

void
g_messages_init (void)
{
  g_messages_lock = g_mutex_new ();
  g_log_depth = g_private_new (NULL);
  g_messages_prefixed_init ();
  _g_debug_init ();
}

gboolean _g_debug_initialized = FALSE;
guint _g_debug_flags = 0;

void
_g_debug_init (void) 
{
  const gchar *val;
  
  _g_debug_initialized = TRUE;
  
  val = g_getenv ("G_DEBUG");
  if (val != NULL)
    {
      static const GDebugKey keys[] = {
	{"fatal_warnings", G_DEBUG_FATAL_WARNINGS}
      };
      
      _g_debug_flags = g_parse_debug_string (val, keys, G_N_ELEMENTS (keys));
    }
  
  if (_g_debug_flags & G_DEBUG_FATAL_WARNINGS) 
    {
      GLogLevelFlags fatal_mask;
      
      fatal_mask = g_log_set_always_fatal (G_LOG_FATAL_MASK);
      fatal_mask |= G_LOG_LEVEL_WARNING | G_LOG_LEVEL_CRITICAL;
      g_log_set_always_fatal (fatal_mask);
    }
}
