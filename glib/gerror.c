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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/types.h>

#include <time.h>
#include <unistd.h>
#include "glib.h"

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif /* HAVE_SYS_SELECT_H */

#ifdef STDC_HEADERS
#include <string.h> /* for bzero on BSD systems */
#endif

#define INTERACTIVE 0
#define STACK_TRACE 1


#ifndef NO_FD_SET
#  define SELECT_MASK fd_set
#else
#  ifndef _AIX
     typedef long fd_mask;
#  endif
#  if defined(_IBMR2)
#    define SELECT_MASK void
#  else
#    define SELECT_MASK int
#  endif
#endif


static int  do_query (char *prompt);
static void debug (const gchar *progname, int method);
static void stack_trace (char **);
static void stack_trace_sigchld (int);


static int stack_trace_done;

void
g_debug (const gchar *progname)
{
  char buf[32];

  fprintf (stdout, "[n]othing, [e]xit, [s]tack trace, [a]ttach to process: ");
  fflush (stdout);

  fgets (buf, 32, stdin);
  if (strcmp (buf, "n\n") == 0)
    return;
  else if (strcmp (buf, "s\n") == 0)
    debug (progname, STACK_TRACE);
  else if (strcmp (buf, "a\n") == 0)
    debug (progname, INTERACTIVE);
  else
    exit (0);
}

void
g_attach_process (const gchar *progname,
		  int          query)
{
  if (!query || do_query ("attach to process"))
    debug (progname, INTERACTIVE);
}

void
g_stack_trace (const gchar *progname,
	       int          query)
{
  if (!query || do_query ("print stack trace"))
    debug (progname, STACK_TRACE);
}

static int
do_query (char *prompt)
{
  char buf[32];

  fprintf (stdout, "%s (y/n) ", prompt);
  fflush (stdout);

  fgets (buf, 32, stdin);
  if ((strcmp (buf, "yes\n") == 0) ||
      (strcmp (buf, "y\n") == 0) ||
      (strcmp (buf, "YES\n") == 0) ||
      (strcmp (buf, "Y\n") == 0))
    return TRUE;

  return FALSE;
}

static void
debug (const char *progname,
       int   method)
{
  pid_t pid;
  char buf[16];
  char *args[4] = { "gdb", NULL, NULL, NULL };
  volatile int x;

  sprintf (buf, "%d", (int) getpid ());

  args[1] = (gchar*) progname;
  args[2] = buf;

  switch (method)
    {
    case INTERACTIVE:
      fprintf (stdout, "pid: %s\n", buf);
      break;
    case STACK_TRACE:
      pid = fork ();
      if (pid == 0)
	{
	  stack_trace (args);
	  _exit (0);
	}
      else if (pid == (pid_t) -1)
	{
	  perror ("could not fork");
	  return;
	}
      break;
    }

  x = 1;
  while (x)
    ;
}

static void
stack_trace (char **args)
{
  pid_t pid;
  int in_fd[2];
  int out_fd[2];
  SELECT_MASK fdset;
  SELECT_MASK readset;
  struct timeval tv;
  int sel, index, state;
  char buffer[256];
  char c;

  stack_trace_done = 0;
  signal (SIGCHLD, stack_trace_sigchld);

  if ((pipe (in_fd) == -1) || (pipe (out_fd) == -1))
    {
      perror ("could open pipe");
      _exit (0);
    }

  pid = fork ();
  if (pid == 0)
    {
      close (0); dup (in_fd[0]);   /* set the stdin to the in pipe */
      close (1); dup (out_fd[1]);  /* set the stdout to the out pipe */
      close (2); dup (out_fd[1]);  /* set the stderr to the out pipe */

      execvp (args[0], args);      /* exec gdb */
      perror ("exec failed");
      _exit (0);
    }
  else if (pid == (pid_t) -1)
    {
      perror ("could not fork");
      _exit (0);
    }

  FD_ZERO (&fdset);
  FD_SET (out_fd[0], &fdset);

  write (in_fd[1], "backtrace\n", 10);
  write (in_fd[1], "p x = 0\n", 8);
  write (in_fd[1], "quit\n", 5);

  index = 0;
  state = 0;

  while (1)
    {
      readset = fdset;
      tv.tv_sec = 1;
      tv.tv_usec = 0;

      sel = select (FD_SETSIZE, &readset, NULL, NULL, &tv);
      if (sel == -1)
        break;

      if ((sel > 0) && (FD_ISSET (out_fd[0], &readset)))
        {
          if (read (out_fd[0], &c, 1))
            {
              switch (state)
                {
                case 0:
                  if (c == '#')
                    {
                      state = 1;
                      index = 0;
                      buffer[index++] = c;
                    }
                  break;
                case 1:
                  buffer[index++] = c;
                  if ((c == '\n') || (c == '\r'))
                    {
                      buffer[index] = 0;
                      fprintf (stdout, "%s", buffer);
                      state = 0;
                      index = 0;
                    }
                  break;
                default:
                  break;
                }
            }
        }
      else if (stack_trace_done)
        break;
    }

  close (in_fd[0]);
  close (in_fd[1]);
  close (out_fd[0]);
  close (out_fd[1]);
  _exit (0);
}

static void
stack_trace_sigchld (int signum)
{
  stack_trace_done = 1;
}
