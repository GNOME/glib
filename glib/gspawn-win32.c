/* gspawn-win32.c - Process launching on Win32
 *
 *  Copyright 2000 Red Hat, Inc.
 *  Copyright 2003 Tor Lillqvist
 *
 * GLib is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * GLib is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GLib; see the file COPYING.LIB.  If not, write
 * to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Implementation details on Win32.
 *
 * - There is no way to set the no-inherit flag for
 *   a "file descriptor" in the MS C runtime. The flag is there,
 *   and the dospawn() function uses it, but unfortunately
 *   this flag can only be set when opening the file.
 * - As there is no fork(), we cannot reliably change directory
 *   before starting the child process. (There might be several threads
 *   running, and the current directory is common for all threads.)
 *
 * Thus, we must in most cases use a helper program to handle closing
 * of (inherited) file descriptors and changing of directory. The
 * helper process is also needed if the standard input, standard
 * output, or standard error of the process to be run are supposed to
 * be redirected somewhere.
 *
 * The structure of the source code in this file is a mess, I know.
 */

/* FIXME: Actually implement G_SPAWN_FILE_AND_ARGV_ZERO */

/* Define this to get some logging all the time */
/* #define G_SPAWN_WIN32_DEBUG */

#include <config.h>

#include "glib.h"
#include "gprintfint.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <windows.h>
#include <errno.h>
#include <fcntl.h>
#include <io.h>
#include <process.h>
#include <direct.h>

#include "glibintl.h"

#ifdef G_SPAWN_WIN32_DEBUG
  static int debug = 1;
  #define SETUP_DEBUG() /* empty */

#else
  static int debug = -1;
  #define SETUP_DEBUG()					\
    G_STMT_START					\
      {							\
	if (debug == -1)				\
	  {						\
	    if (getenv ("G_SPAWN_WIN32_DEBUG") != NULL)	\
	      debug = 1;				\
	    else					\
	      debug = 0;				\
	  }						\
      }							\
    G_STMT_END
#endif

enum
{
  CHILD_NO_ERROR,
  CHILD_CHDIR_FAILED,
  CHILD_SPAWN_FAILED,
};

enum {
  ARG_CHILD_ERR_REPORT = 1,
  ARG_STDIN,
  ARG_STDOUT,
  ARG_STDERR,
  ARG_WORKING_DIRECTORY,
  ARG_CLOSE_DESCRIPTORS,
  ARG_USE_PATH,
  ARG_WAIT,
  ARG_PROGRAM,
  ARG_COUNT = ARG_PROGRAM
};

/* Return codes from do_spawn() */
#define DO_SPAWN_ERROR_HELPER -1
#define DO_SPAWN_OK_NO_HELPER -2
#define DO_SPAWN_ERROR_NO_HELPER -3

static gint
protect_argv (gchar  **argv,
	      gchar ***new_argv)
{
  gint i;
  gint argc = 0;
  
  while (argv[argc])
    ++argc;
  *new_argv = g_new (gchar *, argc+1);

  /* Quote each argv element if necessary, so that it will get
   * reconstructed correctly in the C runtime startup code.  Note that
   * the unquoting algorithm in the C runtime is really weird, and
   * rather different than what Unix shells do. See stdargv.c in the C
   * runtime sources (in the Platform SDK, in src/crt).
   *
   * Note that an new_argv[0] constructed by this function should
   * *not* be passed as the filename argument to a spawn* or exec*
   * family function. That argument should be the real file name
   * without any quoting.
   */
  for (i = 0; i < argc; i++)
    {
      gchar *p = argv[i];
      gchar *q;
      gint len = 0;
      gboolean need_dblquotes = FALSE;
      while (*p)
	{
	  if (*p == ' ' || *p == '\t')
	    need_dblquotes = TRUE;
	  else if (*p == '"')
	    len++;
	  else if (*p == '\\')
	    {
	      gchar *pp = p;
	      while (*pp && *pp == '\\')
		pp++;
	      if (*pp == '"')
		len++;
	    }
	  len++;
	  p++;
	}

      q = (*new_argv)[i] = g_malloc (len + need_dblquotes*2 + 1);
      p = argv[i];

      if (need_dblquotes)
	*q++ = '"';

      while (*p)
	{
	  if (*p == '"')
	    *q++ = '\\';
	  else if (*p == '\\')
	    {
	      gchar *pp = p;
	      while (*pp && *pp == '\\')
		pp++;
	      if (*pp == '"')
		*q++ = '\\';
	    }
	  *q++ = *p;
	  p++;
	}

      if (need_dblquotes)
	*q++ = '"';
      *q++ = '\0';
      /* printf ("argv[%d]:%s, need_dblquotes:%s len:%d => %s\n", i, argv[i], need_dblquotes?"TRUE":"FALSE", len, (*new_argv)[i]); */
    }
  (*new_argv)[argc] = NULL;

  return argc;
}

#ifndef GSPAWN_HELPER

static gboolean make_pipe            (gint                  p[2],
                                      GError              **error);
static gboolean do_spawn_with_pipes  (gboolean              dont_wait,
				      gboolean		    dont_return_handle,
				      const gchar          *working_directory,
                                      gchar               **argv,
                                      gchar               **envp,
                                      gboolean              close_descriptors,
                                      gboolean              search_path,
                                      gboolean              stdout_to_null,
                                      gboolean              stderr_to_null,
                                      gboolean              child_inherits_stdin,
				      gboolean		    file_and_argv_zero,
                                      GSpawnChildSetupFunc  child_setup,
                                      gpointer              user_data,
                                      GPid                 *child_handle,
                                      gint                 *standard_input,
                                      gint                 *standard_output,
                                      gint                 *standard_error,
				      gint                 *exit_status,
                                      GError              **error);

GQuark
g_spawn_error_quark (void)
{
  static GQuark quark = 0;
  if (quark == 0)
    quark = g_quark_from_static_string ("g-exec-error-quark");
  return quark;
}

gboolean
g_spawn_async (const gchar          *working_directory,
               gchar               **argv,
               gchar               **envp,
               GSpawnFlags           flags,
               GSpawnChildSetupFunc  child_setup,
               gpointer              user_data,
               GPid                 *child_handle,
               GError              **error)
{
  g_return_val_if_fail (argv != NULL, FALSE);
  
  return g_spawn_async_with_pipes (working_directory,
                                   argv, envp,
                                   flags,
                                   child_setup,
                                   user_data,
                                   child_handle,
                                   NULL, NULL, NULL,
                                   error);
}

/* Avoids a danger in threaded situations (calling close()
 * on a file descriptor twice, and another thread has
 * re-opened it since the first close)
 */
static gint
close_and_invalidate (gint *fd)
{
  gint ret;

  if (*fd < 0)
    return -1;
  else
    {
      ret = close (*fd);
      *fd = -1;
    }

  return ret;
}

typedef enum
{
  READ_FAILED = 0, /* FALSE */
  READ_OK,
  READ_EOF
} ReadResult;

static ReadResult
read_data (GString     *str,
           GIOChannel  *iochannel,
           GError     **error)
{
  GIOStatus giostatus;
  gssize bytes;
  gchar buf[4096];

 again:
  
  giostatus = g_io_channel_read_chars (iochannel, buf, sizeof (buf), &bytes, NULL);

  if (bytes == 0)
    return READ_EOF;
  else if (bytes > 0)
    {
      g_string_append_len (str, buf, bytes);
      return READ_OK;
    }
  else if (giostatus == G_IO_STATUS_AGAIN)
    goto again;
  else if (giostatus == G_IO_STATUS_ERROR)
    {
      g_set_error (error,
                   G_SPAWN_ERROR,
                   G_SPAWN_ERROR_READ,
                   _("Failed to read data from child process"));
      
      return READ_FAILED;
    }
  else
    return READ_OK;
}

gboolean
g_spawn_sync (const gchar          *working_directory,
              gchar               **argv,
              gchar               **envp,
              GSpawnFlags           flags,
              GSpawnChildSetupFunc  child_setup,
              gpointer              user_data,
              gchar               **standard_output,
              gchar               **standard_error,
              gint                 *exit_status,
              GError              **error)     
{
  gint outpipe = -1;
  gint errpipe = -1;
  GPid pid;
  GIOChannel *outchannel = NULL;
  GIOChannel *errchannel = NULL;
  GPollFD outfd, errfd;
  GPollFD fds[2];
  gint nfds;
  gint outindex = -1;
  gint errindex = -1;
  gint ret;
  GString *outstr = NULL;
  GString *errstr = NULL;
  gboolean failed;
  gint status;
  
  g_return_val_if_fail (argv != NULL, FALSE);
  g_return_val_if_fail (!(flags & G_SPAWN_DO_NOT_REAP_CHILD), FALSE);
  g_return_val_if_fail (standard_output == NULL ||
                        !(flags & G_SPAWN_STDOUT_TO_DEV_NULL), FALSE);
  g_return_val_if_fail (standard_error == NULL ||
                        !(flags & G_SPAWN_STDERR_TO_DEV_NULL), FALSE);
  
  /* Just to ensure segfaults if callers try to use
   * these when an error is reported.
   */
  if (standard_output)
    *standard_output = NULL;

  if (standard_error)
    *standard_error = NULL;
  
  if (!do_spawn_with_pipes (FALSE,
			    TRUE,
			    working_directory,
			    argv,
			    envp,
			    !(flags & G_SPAWN_LEAVE_DESCRIPTORS_OPEN),
			    (flags & G_SPAWN_SEARCH_PATH) != 0,
			    (flags & G_SPAWN_STDOUT_TO_DEV_NULL) != 0,
			    (flags & G_SPAWN_STDERR_TO_DEV_NULL) != 0,
			    (flags & G_SPAWN_CHILD_INHERITS_STDIN) != 0,
			    (flags & G_SPAWN_FILE_AND_ARGV_ZERO) != 0,
			    child_setup,
			    user_data,
			    &pid,
			    NULL,
			    standard_output ? &outpipe : NULL,
			    standard_error ? &errpipe : NULL,
			    &status,
			    error))
    return FALSE;

  /* Read data from child. */
  
  failed = FALSE;

  if (outpipe >= 0)
    {
      outstr = g_string_new (NULL);
      outchannel = g_io_channel_win32_new_fd (outpipe);
      g_io_channel_set_encoding (outchannel, NULL, NULL);
      g_io_channel_win32_make_pollfd (outchannel,
				      G_IO_IN | G_IO_ERR | G_IO_HUP,
				      &outfd);
    }
      
  if (errpipe >= 0)
    {
      errstr = g_string_new (NULL);
      errchannel = g_io_channel_win32_new_fd (errpipe);
      g_io_channel_set_encoding (errchannel, NULL, NULL);
      g_io_channel_win32_make_pollfd (errchannel,
				      G_IO_IN | G_IO_ERR | G_IO_HUP,
				      &errfd);
    }

  /* Read data until we get EOF on both pipes. */
  while (!failed &&
         (outpipe >= 0 ||
          errpipe >= 0))
    {
      nfds = 0;
      if (outpipe >= 0)
	{
	  fds[nfds] = outfd;
	  outindex = nfds;
	  nfds++;
	}
      if (errpipe >= 0)
	{
	  fds[nfds] = errfd;
	  errindex = nfds;
	  nfds++;
	}

      if (debug)
	g_print ("%s:g_spawn_sync: calling g_io_channel_win32_poll, nfds=%d\n",
		 __FILE__, nfds);

      ret = g_io_channel_win32_poll (fds, nfds, -1);

      if (ret < 0)
        {
          failed = TRUE;

          g_set_error (error,
                       G_SPAWN_ERROR,
                       G_SPAWN_ERROR_READ,
                       _("Unexpected error in g_io_channel_win32_poll() reading data from a child process"));
              
          break;
        }

      if (outpipe >= 0 && (fds[outindex].revents & G_IO_IN))
        {
          switch (read_data (outstr, outchannel, error))
            {
            case READ_FAILED:
	      if (debug)
		g_print ("g_spawn_sync: outchannel: READ_FAILED\n");
              failed = TRUE;
              break;
            case READ_EOF:
	      if (debug)
		g_print ("g_spawn_sync: outchannel: READ_EOF\n");
              g_io_channel_unref (outchannel);
	      outchannel = NULL;
              close_and_invalidate (&outpipe);
              break;
            default:
	      if (debug)
		g_print ("g_spawn_sync: outchannel: OK\n");
              break;
            }

          if (failed)
            break;
        }

      if (errpipe >= 0 && (fds[errindex].revents & G_IO_IN))
        {
          switch (read_data (errstr, errchannel, error))
            {
            case READ_FAILED:
	      if (debug)
		g_print ("g_spawn_sync: errchannel: READ_FAILED\n");
              failed = TRUE;
              break;
            case READ_EOF:
	      if (debug)
		g_print ("g_spawn_sync: errchannel: READ_EOF\n");
	      g_io_channel_unref (errchannel);
	      errchannel = NULL;
              close_and_invalidate (&errpipe);
              break;
            default:
	      if (debug)
		g_print ("g_spawn_sync: errchannel: OK\n");
              break;
            }

          if (failed)
            break;
        }
    }

  /* These should only be open still if we had an error.  */
  
  if (outchannel != NULL)
    g_io_channel_unref (outchannel);
  if (errchannel != NULL)
    g_io_channel_unref (errchannel);
  if (outpipe >= 0)
    close_and_invalidate (&outpipe);
  if (errpipe >= 0)
    close_and_invalidate (&errpipe);
  
  g_spawn_close_pid(pid);

  if (failed)
    {
      if (outstr)
        g_string_free (outstr, TRUE);
      if (errstr)
        g_string_free (errstr, TRUE);

      return FALSE;
    }
  else
    {
      if (exit_status)
        *exit_status = status;
      
      if (standard_output)        
        *standard_output = g_string_free (outstr, FALSE);

      if (standard_error)
        *standard_error = g_string_free (errstr, FALSE);

      return TRUE;
    }
}

gboolean
g_spawn_async_with_pipes (const gchar          *working_directory,
                          gchar               **argv,
                          gchar               **envp,
                          GSpawnFlags           flags,
                          GSpawnChildSetupFunc  child_setup,
                          gpointer              user_data,
                          GPid                 *child_handle,
                          gint                 *standard_input,
                          gint                 *standard_output,
                          gint                 *standard_error,
                          GError              **error)
{
  g_return_val_if_fail (argv != NULL, FALSE);
  g_return_val_if_fail (standard_output == NULL ||
                        !(flags & G_SPAWN_STDOUT_TO_DEV_NULL), FALSE);
  g_return_val_if_fail (standard_error == NULL ||
                        !(flags & G_SPAWN_STDERR_TO_DEV_NULL), FALSE);
  /* can't inherit stdin if we have an input pipe. */
  g_return_val_if_fail (standard_input == NULL ||
                        !(flags & G_SPAWN_CHILD_INHERITS_STDIN), FALSE);
  
  return do_spawn_with_pipes (TRUE,
			      !(flags & G_SPAWN_DO_NOT_REAP_CHILD),
			      working_directory,
			      argv,
			      envp,
			      !(flags & G_SPAWN_LEAVE_DESCRIPTORS_OPEN),
			      (flags & G_SPAWN_SEARCH_PATH) != 0,
			      (flags & G_SPAWN_STDOUT_TO_DEV_NULL) != 0,
			      (flags & G_SPAWN_STDERR_TO_DEV_NULL) != 0,
			      (flags & G_SPAWN_CHILD_INHERITS_STDIN) != 0,
			      (flags & G_SPAWN_FILE_AND_ARGV_ZERO) != 0,
			      child_setup,
			      user_data,
			      child_handle,
			      standard_input,
			      standard_output,
			      standard_error,
			      NULL,
			      error);
}

gboolean
g_spawn_command_line_sync (const gchar  *command_line,
                           gchar       **standard_output,
                           gchar       **standard_error,
                           gint         *exit_status,
                           GError      **error)
{
  gboolean retval;
  gchar **argv = 0;

  g_return_val_if_fail (command_line != NULL, FALSE);
  
  if (!g_shell_parse_argv (command_line,
                           NULL, &argv,
                           error))
    return FALSE;
  
  retval = g_spawn_sync (NULL,
                         argv,
                         NULL,
                         G_SPAWN_SEARCH_PATH,
                         NULL,
                         NULL,
                         standard_output,
                         standard_error,
                         exit_status,
                         error);
  g_strfreev (argv);

  return retval;
}

gboolean
g_spawn_command_line_async (const gchar *command_line,
                            GError     **error)
{
  gboolean retval;
  gchar **argv = 0;

  g_return_val_if_fail (command_line != NULL, FALSE);

  if (!g_shell_parse_argv (command_line,
                           NULL, &argv,
                           error))
    return FALSE;
  
  retval = g_spawn_async (NULL,
                          argv,
                          NULL,
                          G_SPAWN_SEARCH_PATH,
                          NULL,
                          NULL,
                          NULL,
                          error);
  g_strfreev (argv);

  return retval;
}

/* This stinks, code reorg needed. Presumably do_spawn() should be
 * inserted into its only caller, do_spawn_with_pipes(), and then code
 * snippets from that function should be split out to separate
 * functions if necessary.
 */

static gint shortcut_spawn_retval;

static gint
do_spawn (gboolean              dont_wait,
	  gint                  child_err_report_fd,
	  gint                  stdin_fd,
	  gint                  stdout_fd,
	  gint                  stderr_fd,
	  const gchar          *working_directory,
	  gchar               **argv,
	  gchar               **envp,
	  gboolean              close_descriptors,
	  gboolean              search_path,
	  gboolean              stdout_to_null,
	  gboolean              stderr_to_null,
	  gboolean              child_inherits_stdin,
	  gboolean		file_and_argv_zero,
	  GSpawnChildSetupFunc  child_setup,
	  gpointer              user_data)
{
  gchar **protected_argv;
  gchar **new_argv;
  gchar args[ARG_COUNT][10];
  gint i;
  int rc;
  int argc;

  SETUP_DEBUG();

  argc = protect_argv (argv, &protected_argv);

  if (stdin_fd == -1 && stdout_fd == -1 && stderr_fd == -1 &&
      (working_directory == NULL || !*working_directory) &&
      !close_descriptors &&
      !stdout_to_null && !stderr_to_null &&
      child_inherits_stdin)
    {
      /* We can do without the helper process */
      int mode = dont_wait ? P_NOWAIT : P_WAIT;

      if (debug)
	g_print ("doing without gspawn-win32-helper\n");

      if (search_path)
	rc = spawnvp (mode, argv[0], protected_argv);
      else
	rc = spawnv (mode, argv[0], protected_argv);

      for (i = 0; i < argc; i++)
	g_free (protected_argv[i]);
      g_free (protected_argv);
      
      if (rc == -1)
	{
	  return DO_SPAWN_ERROR_NO_HELPER;
	}
      else
	{
	  shortcut_spawn_retval = rc;
	  return DO_SPAWN_OK_NO_HELPER;
	}
    }

  new_argv = g_new (gchar *, argc + 1 + ARG_COUNT);

  new_argv[0] = "gspawn-win32-helper";
  _g_sprintf (args[ARG_CHILD_ERR_REPORT], "%d", child_err_report_fd);
  new_argv[ARG_CHILD_ERR_REPORT] = args[ARG_CHILD_ERR_REPORT];

  if (stdin_fd >= 0)
    {
      _g_sprintf (args[ARG_STDIN], "%d", stdin_fd);
      new_argv[ARG_STDIN] = args[ARG_STDIN];
    }
  else if (child_inherits_stdin)
    {
      /* Let stdin be alone */
      new_argv[ARG_STDIN] = "-";
    }
  else
    {
      /* Keep process from blocking on a read of stdin */
      new_argv[ARG_STDIN] = "z";
    }

  if (stdout_fd >= 0)
    {
      _g_sprintf (args[ARG_STDOUT], "%d", stdout_fd);
      new_argv[ARG_STDOUT] = args[ARG_STDOUT];
    }
  else if (stdout_to_null)
    {
      new_argv[ARG_STDOUT] = "z";
    }
  else
    {
      new_argv[ARG_STDOUT] = "-";
    }

  if (stderr_fd >= 0)
    {
      _g_sprintf (args[ARG_STDERR], "%d", stderr_fd);
      new_argv[ARG_STDERR] = args[ARG_STDERR];
    }
  else if (stderr_to_null)
    {
      new_argv[ARG_STDERR] = "z";
    }
  else
    {
      new_argv[ARG_STDERR] = "-";
    }

  if (working_directory && *working_directory)
    /* The g_strdup() to lose the constness */
    new_argv[ARG_WORKING_DIRECTORY] = g_strdup (working_directory);
  else
    new_argv[ARG_WORKING_DIRECTORY] = g_strdup ("-");

  if (close_descriptors)
    new_argv[ARG_CLOSE_DESCRIPTORS] = "y";
  else
    new_argv[ARG_CLOSE_DESCRIPTORS] = "-";

  if (search_path)
    new_argv[ARG_USE_PATH] = "y";
  else
    new_argv[ARG_USE_PATH] = "-";

  if (dont_wait)
    new_argv[ARG_WAIT] = "-";
  else
    new_argv[ARG_WAIT] = "w";

  for (i = 0; i <= argc; i++)
    new_argv[ARG_PROGRAM + i] = protected_argv[i];

  /* Call user function just before we execute the helper program,
   * which executes the program. Dunno what's the usefulness of this.
   * A child setup function used on Unix probably isn't of much use
   * as such on Win32, anyhow
   */
  if (child_setup)
    {
      (* child_setup) (user_data);
    }

  if (debug)
    {
      g_print ("calling gspawn-win32-helper with argv:\n");
      for (i = 0; i < argc + 1 + ARG_COUNT; i++)
	g_print ("argv[%d]: %s\n", i, (new_argv[i] ? new_argv[i] : "NULL"));
    }
  
  if (envp != NULL)
    /* Let's hope envp hasn't mucked with PATH so that
     * gspawn-win32-helper.exe isn't found.
     */
    rc = spawnvpe (P_NOWAIT, "gspawn-win32-helper", new_argv, envp);
  else
    rc = spawnvp (P_NOWAIT, "gspawn-win32-helper", new_argv);

  /* Close the child_err_report_fd and the other process's ends of the
   * pipes in this process, otherwise the reader will never get
   * EOF.
   */
  close (child_err_report_fd);
  if (stdin_fd >= 0)
    close (stdin_fd);
  if (stdout_fd >= 0)
    close (stdout_fd);
  if (stderr_fd >= 0)
    close (stderr_fd);

  for (i = 0; i < argc; i++)
    g_free (protected_argv[i]);
  g_free (protected_argv);

  g_free (new_argv[ARG_WORKING_DIRECTORY]);
  g_free (new_argv);

  return rc;
}

static gboolean
read_ints (int      fd,
           gint*    buf,
           gint     n_ints_in_buf,
           gint    *n_ints_read,
           GError **error)
{
  gint bytes = 0;
  
  while (bytes < sizeof(gint)*n_ints_in_buf)
    {
      gint chunk;

      if (debug)
	g_print ("%s:read_ints: trying to read %d bytes from pipe...\n",
		 __FILE__,
		 sizeof(gint)*n_ints_in_buf - bytes);

      chunk = read (fd, ((gchar*)buf) + bytes,
		    sizeof(gint)*n_ints_in_buf - bytes);

      if (debug)
	g_print ("... got %d bytes\n", chunk);
          
      if (chunk < 0)
        {
          /* Some weird shit happened, bail out */
              
          g_set_error (error,
                       G_SPAWN_ERROR,
                       G_SPAWN_ERROR_FAILED,
                       _("Failed to read from child pipe (%s)"),
                       g_strerror (errno));

          return FALSE;
        }
      else if (chunk == 0)
        break; /* EOF */
      else
	bytes += chunk;
    }

  *n_ints_read = bytes/sizeof(gint);

  return TRUE;
}

static gboolean
do_spawn_with_pipes (gboolean              dont_wait,
		     gboolean		   dont_return_handle,
		     const gchar          *working_directory,
		     gchar               **argv,
		     gchar               **envp,
		     gboolean              close_descriptors,
		     gboolean              search_path,
		     gboolean              stdout_to_null,
		     gboolean              stderr_to_null,
		     gboolean              child_inherits_stdin,
		     gboolean		   file_and_argv_zero,
		     GSpawnChildSetupFunc  child_setup,
		     gpointer              user_data,
		     GPid                 *child_handle,
		     gint                 *standard_input,
		     gint                 *standard_output,
		     gint                 *standard_error,
		     gint                 *exit_status,
		     GError              **error)     
{
  gint stdin_pipe[2] = { -1, -1 };
  gint stdout_pipe[2] = { -1, -1 };
  gint stderr_pipe[2] = { -1, -1 };
  gint child_err_report_pipe[2] = { -1, -1 };
  gint helper = -1;
  gint buf[2];
  gint n_ints = 0;
  
  if (!make_pipe (child_err_report_pipe, error))
    return FALSE;

  if (standard_input && !make_pipe (stdin_pipe, error))
    goto cleanup_and_fail;
  
  if (standard_output && !make_pipe (stdout_pipe, error))
    goto cleanup_and_fail;

  if (standard_error && !make_pipe (stderr_pipe, error))
    goto cleanup_and_fail;

  helper = do_spawn (dont_wait,
		     child_err_report_pipe[1],
		     stdin_pipe[0],
		     stdout_pipe[1],
		     stderr_pipe[1],
		     working_directory,
		     argv,
		     envp,
		     close_descriptors,
		     search_path,
		     stdout_to_null,
		     stderr_to_null,
		     child_inherits_stdin,
		     file_and_argv_zero,
		     child_setup,
		     user_data);
      
  /* Check if gspawn-win32-helper couldn't be run */
  if (helper == DO_SPAWN_ERROR_HELPER)
    {
      g_set_error (error,
		   G_SPAWN_ERROR,
		   G_SPAWN_ERROR_FAILED,
		   _("Failed to execute helper program"));
      goto cleanup_and_fail;
    }

  else if (helper == DO_SPAWN_OK_NO_HELPER)
    {
      if (child_handle && dont_wait && !dont_return_handle)
	*child_handle = shortcut_spawn_retval;
      else if (!dont_wait && exit_status)
	*exit_status = shortcut_spawn_retval;

      close_and_invalidate (&child_err_report_pipe[0]);
      close_and_invalidate (&child_err_report_pipe[1]);

      return TRUE;
    }
  else if (helper == DO_SPAWN_ERROR_NO_HELPER)
    {
      g_set_error (error,
		   G_SPAWN_ERROR,
		   G_SPAWN_ERROR_FAILED,
		   _("Failed to execute child process (%s)"),
		   g_strerror (errno));
      helper = -1;
      goto cleanup_and_fail;
    }

  if (!read_ints (child_err_report_pipe[0],
		  buf, 2, &n_ints,
		  error) ||
	   n_ints != 2)
    goto cleanup_and_fail;
        
  /* Error code from gspawn-win32-helper. */
  switch (buf[0])
    {
    case CHILD_NO_ERROR:
      if (child_handle && dont_wait && !dont_return_handle)
	{
	  /* helper is our HANDLE for gspawn-win32-helper. It has
	   * told us the HANDLE of its child. Duplicate that into
	   * a HANDLE valid in this process.
	   */
	  if (!DuplicateHandle ((HANDLE) helper, (HANDLE) buf[1],
				GetCurrentProcess (), (LPHANDLE) child_handle,
				0, TRUE, DUPLICATE_SAME_ACCESS))
	    *child_handle = 0;
	}
      else if (child_handle)
	*child_handle = 0;
      break;
      
    case CHILD_CHDIR_FAILED:
      g_set_error (error,
		   G_SPAWN_ERROR,
		   G_SPAWN_ERROR_CHDIR,
		   _("Failed to change to directory '%s' (%s)"),
		   working_directory,
		   g_strerror (buf[1]));
      goto cleanup_and_fail;
      
    case CHILD_SPAWN_FAILED:
      g_set_error (error,
		   G_SPAWN_ERROR,
		   G_SPAWN_ERROR_FAILED,
		   _("Failed to execute child process (%s)"),
		   g_strerror (buf[1]));
      goto cleanup_and_fail;
    }

  /* Success against all odds! return the information */
      
  if (standard_input)
    *standard_input = stdin_pipe[1];
  if (standard_output)
    *standard_output = stdout_pipe[0];
  if (standard_error)
    *standard_error = stderr_pipe[0];
  if (exit_status)
    *exit_status = buf[1];
  CloseHandle ((HANDLE) helper);
  
  close_and_invalidate (&child_err_report_pipe[0]);

  return TRUE;

 cleanup_and_fail:
  if (helper != -1)
    CloseHandle ((HANDLE) helper);
  close_and_invalidate (&child_err_report_pipe[0]);
  close_and_invalidate (&child_err_report_pipe[1]);
  close_and_invalidate (&stdin_pipe[0]);
  close_and_invalidate (&stdin_pipe[1]);
  close_and_invalidate (&stdout_pipe[0]);
  close_and_invalidate (&stdout_pipe[1]);
  close_and_invalidate (&stderr_pipe[0]);
  close_and_invalidate (&stderr_pipe[1]);

  return FALSE;
}

static gboolean
make_pipe (gint     p[2],
           GError **error)
{
  if (pipe (p) < 0)
    {
      g_set_error (error,
                   G_SPAWN_ERROR,
                   G_SPAWN_ERROR_FAILED,
                   _("Failed to create pipe for communicating with child process (%s)"),
                   g_strerror (errno));
      return FALSE;
    }
  else
    return TRUE;
}
#endif /* !GSPAWN_HELPER */

void
g_spawn_close_pid (GPid pid)
{
    CloseHandle (pid);
}
