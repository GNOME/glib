/* GLIB - Library of useful routines for C programming
 * Copyright (C) 2000  Tor Lillqvist
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

/* A test program for the main loop and IO channel code.
 * Just run it.
 */

#include "config.h"

#include <glib.h>

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#ifdef G_OS_WIN32
  #include <io.h>
  #include <fcntl.h>
  #include <process.h>
#else
  #ifdef HAVE_UNISTD_H
    #include <unistd.h>
  #endif
#endif

static int nrunning;
static GMainLoop *main_loop;

#define BUFSIZE 5000		/* Larger than the circular buffer in
				 * giowin32.c on purpose.
				 */

static int nkiddies;

static struct {
  int fd;
  int seq;
} *seqtab;

static GIOError
read_all (int         fd,
	  GIOChannel *channel,
	  char       *buffer,
	  guint       nbytes,
	  guint      *bytes_read)
{
  guint left = nbytes;
  guint nb;
  GIOError error;
  char *bufp = buffer;

  /* g_io_channel_read() doesn't necessarily return all the
   * data we want at once.
   */
  *bytes_read = 0;
  while (left)
    {
      error = g_io_channel_read (channel, bufp, left, &nb);
      
      if (error != G_IO_ERROR_NONE)
	{
	  g_print ("gio-test: ...from %d: G_IO_ERROR_%s\n", fd,
		   (error == G_IO_ERROR_AGAIN ? "AGAIN" :
		    (error == G_IO_ERROR_INVAL ? "INVAL" :
		     (error == G_IO_ERROR_UNKNOWN ? "UNKNOWN" : "???"))));
	  if (error == G_IO_ERROR_AGAIN)
	    continue;
	  break;
	}
      if (nb == 0)
	return error;
      left -= nb;
      bufp += nb;
      *bytes_read += nb;
    }
  return error;
}


static gboolean
recv_message (GIOChannel  *channel,
	      GIOCondition cond,
	      gpointer    data)
{
  gint fd = g_io_channel_unix_get_fd (channel);

  g_print ("gio-test: ...from %d:%s%s%s%s\n", fd,
	   (cond & G_IO_ERR) ? " ERR" : "",
	   (cond & G_IO_HUP) ? " HUP" : "",
	   (cond & G_IO_IN)  ? " IN"  : "",
	   (cond & G_IO_PRI) ? " PRI" : "");

  if (cond & (G_IO_ERR | G_IO_HUP))
    {
      g_source_remove (*(guint *) data);
      nrunning--;
      if (nrunning == 0)
	g_main_quit (main_loop);
    }

  if (cond & G_IO_IN)
    {
      char buf[BUFSIZE];
      guint nbytes;
      guint nb;
      int i, j, seq;
      GIOError error;
      
      error = read_all (fd, channel, (gchar *) &seq, sizeof (seq), &nb);
      if (error == G_IO_ERROR_NONE)
	{
	  if (nb == 0)
	    {
	      g_print ("gio-test: ...from %d: EOF\n", fd);
	      return FALSE;
	    }
	  
	  g_assert (nb == sizeof (nbytes));

	  for (i = 0; i < nkiddies; i++)
	    if (seqtab[i].fd == fd)
	      {
		if (seq != seqtab[i].seq)
		  {
		    g_print ("gio-test: ...from &d: invalid sequence number %d, expected %d\n",
			     seq, seqtab[i].seq);
		    g_assert_not_reached ();
		  }
		seqtab[i].seq++;
		break;
	      }

	  error = read_all (fd, channel, (gchar *) &nbytes, sizeof (nbytes), &nb);
	}

      if (error != G_IO_ERROR_NONE)
	return FALSE;
      
      if (nb == 0)
	{
	  g_print ("gio-test: ...from %d: EOF\n", fd);
	  return FALSE;
	}
      
      g_assert (nb == sizeof (nbytes));

      if (nbytes >= BUFSIZE)
	{
	  g_print ("gio-test: ...from %d: nbytes = %d (%#x)!\n", fd, nbytes, nbytes);
	  g_assert_not_reached ();
	}
      g_assert (nbytes >= 0 && nbytes < BUFSIZE);
      
      g_print ("gio-test: ...from %d: %d bytes\n", fd, nbytes);
      
      if (nbytes > 0)
	{
	  error = read_all (fd, channel, buf, nbytes, &nb);

	  if (error != G_IO_ERROR_NONE)
	    return FALSE;

	  if (nb == 0)
	    {
	      g_print ("gio-test: ...from %d: EOF\n", fd);
	      return FALSE;
	    }
      
	  for (j = 0; j < nbytes; j++)
	    if (buf[j] != ' ' + ((nbytes + j) % 95))
	      {
		g_print ("gio-test: ...from %d: buf[%d] == '%c', should be '%c'\n",
			 fd, j, buf[j], 'a' + ((nbytes + j) % 32));
		g_assert_not_reached ();
	      }
	  g_print ("gio-test: ...from %d: OK\n", fd);
	}
    }
  return TRUE;
}

int
main (int    argc,
      char **argv)
{
  if (argc < 3)
    {
      /* Parent */
      
      GIOChannel *my_read_channel;
      gchar *cmdline;
      guint *id;
      int i;
#ifdef G_OS_WIN32
      GTimeVal start, end;
      int pollresult;
#endif

      nkiddies = (argc == 1 ? 1 : atoi(argv[1]));
      seqtab = g_malloc (nkiddies * 2 * sizeof (int));

      for (i = 0; i < nkiddies; i++)
	{
	  int pipe_to_sub[2], pipe_from_sub[2];
	  
	  if (pipe (pipe_to_sub) == -1 ||
	      pipe (pipe_from_sub) == -1)
	    perror ("pipe"), exit (1);
	  
	  
	  seqtab[i].fd = pipe_from_sub[0];
	  seqtab[i].seq = 0;

	  my_read_channel = g_io_channel_unix_new (pipe_from_sub[0]);
	  
	  id = g_new (guint, 1);
	  *id =
	    g_io_add_watch (my_read_channel,
			    G_IO_IN | G_IO_PRI | G_IO_ERR | G_IO_HUP,
			    recv_message,
			    id);
	  
	  cmdline = g_strdup_printf ("%s %d %d &", argv[0],
				     pipe_to_sub[0], pipe_from_sub[1]);
	  
	  nrunning++;
	  
#ifdef G_OS_WIN32
	  {
	    gchar *readfd = g_strdup_printf ("%d", pipe_to_sub[0]);
	    gchar *writefd = g_strdup_printf ("%d", pipe_from_sub[1]);
	    _spawnl (_P_NOWAIT, argv[0], argv[0], readfd, writefd, NULL);
	  }
#else
	  system (cmdline);
#endif
	  close (pipe_to_sub[0]);
	  close (pipe_from_sub [1]);

#ifdef G_OS_WIN32
	  g_get_current_time (&start);
	  pollresult = g_io_channel_win32_wait_for_condition (my_read_channel, G_IO_IN, 100);
	  g_get_current_time (&end);
	  if (end.tv_usec < start.tv_usec)
	    end.tv_sec--, end.tv_usec += 1000000;
	  g_print ("gio-test: had to wait %ld.%03ld s, result:%d\n",
		   end.tv_sec - start.tv_sec,
		   (end.tv_usec - start.tv_usec) / 1000,
		   pollresult);
#endif
	}
      
      main_loop = g_main_new (FALSE);
      
      g_main_run (main_loop);
    }
  else if (argc == 3)
    {
      /* Child */
      
      int readfd, writefd;
      int i, j;
      char buf[BUFSIZE];
      int buflen;
      GTimeVal tv;
  
      g_get_current_time (&tv);
      
      readfd = atoi (argv[1]);
      writefd = atoi (argv[2]);
      
      srand (tv.tv_sec ^ (tv.tv_usec / 1000) ^ readfd ^ (writefd << 4));
  
      for (i = 0; i < 20 + rand() % 20; i++)
	{
	  g_usleep (100 + (rand() % 10) * 5000);
	  buflen = rand() % BUFSIZE;
	  for (j = 0; j < buflen; j++)
	    buf[j] = ' ' + ((buflen + j) % 95);
	  g_print ("gio-test: child writing %d bytes to %d\n", buflen, writefd);
	  write (writefd, &i, sizeof (i));
	  write (writefd, &buflen, sizeof (buflen));
	  write (writefd, buf, buflen);
	}
      g_print ("gio-test: child exiting, closing %d\n", writefd);
      close (writefd);
    }
  else
    g_print ("Huh?\n");
  
  return 0;
}

