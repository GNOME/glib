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

#define _GNU_SOURCE		/* For stpcpy */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <ctype.h>		/* For tolower() */
#if !defined (HAVE_STRSIGNAL) || !defined(NO_SYS_SIGLIST_DECL)
#include <signal.h>
#endif
#include "glib.h"

#ifdef G_OS_WIN32
#include <windows.h>
#endif

/* do not include <unistd.h> in this place since it
 * inteferes with g_strsignal() on some OSes
 */

gchar*
g_strdup (const gchar *str)
{
  gchar *new_str;

  if (str)
    {
      new_str = g_new (char, strlen (str) + 1);
      strcpy (new_str, str);
    }
  else
    new_str = NULL;

  return new_str;
}

gpointer
g_memdup (gconstpointer mem,
	  guint         byte_size)
{
  gpointer new_mem;

  if (mem)
    {
      new_mem = g_malloc (byte_size);
      memcpy (new_mem, mem, byte_size);
    }
  else
    new_mem = NULL;

  return new_mem;
}

gchar*
g_strndup (const gchar *str,
	   guint        n)
{
  gchar *new_str;

  if (str)
    {
      new_str = g_new (gchar, n + 1);
      strncpy (new_str, str, n);
      new_str[n] = '\0';
    }
  else
    new_str = NULL;

  return new_str;
}

gchar*
g_strnfill (guint length,
	    gchar fill_char)
{
  register gchar *str, *s, *end;

  str = g_new (gchar, length + 1);
  s = str;
  end = str + length;
  while (s < end)
    *(s++) = fill_char;
  *s = 0;

  return str;
}

/**
 * g_stpcpy:
 * @dest: destination buffer
 * @src: source string
 * 
 * Copies a nul-terminated string into the dest buffer, include the
 * trailing nul, and return a pointer to the trailing nul byte.
 * This is useful for concatenating multiple strings together
 * without having to repeatedly scan for the end.
 * 
 * Return value: a pointer to trailing nul byte.
 **/
gchar *
g_stpcpy (gchar       *dest,
          const gchar *src)
{
#ifdef HAVE_STPCPY
  g_return_val_if_fail (dest != NULL, NULL);
  g_return_val_if_fail (src != NULL, NULL);
  return stpcpy (dest, src);
#else
  register gchar *d = dest;
  register const gchar *s = src;

  g_return_val_if_fail (dest != NULL, NULL);
  g_return_val_if_fail (src != NULL, NULL);
  do
    *d++ = *s;
  while (*s++ != '\0');

  return d - 1;
#endif
}

gchar*
g_strdup_vprintf (const gchar *format,
		  va_list      args1)
{
  gchar *buffer;
  va_list args2;

  G_VA_COPY (args2, args1);

  buffer = g_new (gchar, g_printf_string_upper_bound (format, args1));

  vsprintf (buffer, format, args2);
  va_end (args2);

  return buffer;
}

gchar*
g_strdup_printf (const gchar *format,
		 ...)
{
  gchar *buffer;
  va_list args;

  va_start (args, format);
  buffer = g_strdup_vprintf (format, args);
  va_end (args);

  return buffer;
}

gchar*
g_strconcat (const gchar *string1, ...)
{
  guint	  l;
  va_list args;
  gchar	  *s;
  gchar	  *concat;
  gchar   *ptr;

  g_return_val_if_fail (string1 != NULL, NULL);

  l = 1 + strlen (string1);
  va_start (args, string1);
  s = va_arg (args, gchar*);
  while (s)
    {
      l += strlen (s);
      s = va_arg (args, gchar*);
    }
  va_end (args);

  concat = g_new (gchar, l);
  ptr = concat;

  ptr = g_stpcpy (ptr, string1);
  va_start (args, string1);
  s = va_arg (args, gchar*);
  while (s)
    {
      strcat (concat, s);
      s = va_arg (args, gchar*);
    }
  va_end (args);

  return concat;
}

gdouble
g_strtod (const gchar *nptr,
	  gchar **endptr)
{
  gchar *fail_pos_1;
  gchar *fail_pos_2;
  gdouble val_1;
  gdouble val_2 = 0;

  g_return_val_if_fail (nptr != NULL, 0);

  fail_pos_1 = NULL;
  fail_pos_2 = NULL;

  val_1 = strtod (nptr, &fail_pos_1);

  if (fail_pos_1 && fail_pos_1[0] != 0)
    {
      gchar *old_locale;

      old_locale = g_strdup (setlocale (LC_NUMERIC, NULL));
      setlocale (LC_NUMERIC, "C");
      val_2 = strtod (nptr, &fail_pos_2);
      setlocale (LC_NUMERIC, old_locale);
      g_free (old_locale);
    }

  if (!fail_pos_1 || fail_pos_1[0] == 0 || fail_pos_1 >= fail_pos_2)
    {
      if (endptr)
	*endptr = fail_pos_1;
      return val_1;
    }
  else
    {
      if (endptr)
	*endptr = fail_pos_2;
      return val_2;
    }
}

G_CONST_RETURN gchar*
g_strerror (gint errnum)
{
  static GStaticPrivate msg_private = G_STATIC_PRIVATE_INIT;
  char *msg;

#ifdef HAVE_STRERROR
  return strerror (errnum);
#elif NO_SYS_ERRLIST
  switch (errnum)
    {
#ifdef E2BIG
    case E2BIG: return "argument list too long";
#endif
#ifdef EACCES
    case EACCES: return "permission denied";
#endif
#ifdef EADDRINUSE
    case EADDRINUSE: return "address already in use";
#endif
#ifdef EADDRNOTAVAIL
    case EADDRNOTAVAIL: return "can't assign requested address";
#endif
#ifdef EADV
    case EADV: return "advertise error";
#endif
#ifdef EAFNOSUPPORT
    case EAFNOSUPPORT: return "address family not supported by protocol family";
#endif
#ifdef EAGAIN
    case EAGAIN: return "try again";
#endif
#ifdef EALIGN
    case EALIGN: return "EALIGN";
#endif
#ifdef EALREADY
    case EALREADY: return "operation already in progress";
#endif
#ifdef EBADE
    case EBADE: return "bad exchange descriptor";
#endif
#ifdef EBADF
    case EBADF: return "bad file number";
#endif
#ifdef EBADFD
    case EBADFD: return "file descriptor in bad state";
#endif
#ifdef EBADMSG
    case EBADMSG: return "not a data message";
#endif
#ifdef EBADR
    case EBADR: return "bad request descriptor";
#endif
#ifdef EBADRPC
    case EBADRPC: return "RPC structure is bad";
#endif
#ifdef EBADRQC
    case EBADRQC: return "bad request code";
#endif
#ifdef EBADSLT
    case EBADSLT: return "invalid slot";
#endif
#ifdef EBFONT
    case EBFONT: return "bad font file format";
#endif
#ifdef EBUSY
    case EBUSY: return "mount device busy";
#endif
#ifdef ECHILD
    case ECHILD: return "no children";
#endif
#ifdef ECHRNG
    case ECHRNG: return "channel number out of range";
#endif
#ifdef ECOMM
    case ECOMM: return "communication error on send";
#endif
#ifdef ECONNABORTED
    case ECONNABORTED: return "software caused connection abort";
#endif
#ifdef ECONNREFUSED
    case ECONNREFUSED: return "connection refused";
#endif
#ifdef ECONNRESET
    case ECONNRESET: return "connection reset by peer";
#endif
#if defined(EDEADLK) && (!defined(EWOULDBLOCK) || (EDEADLK != EWOULDBLOCK))
    case EDEADLK: return "resource deadlock avoided";
#endif
#ifdef EDEADLOCK
    case EDEADLOCK: return "resource deadlock avoided";
#endif
#ifdef EDESTADDRREQ
    case EDESTADDRREQ: return "destination address required";
#endif
#ifdef EDIRTY
    case EDIRTY: return "mounting a dirty fs w/o force";
#endif
#ifdef EDOM
    case EDOM: return "math argument out of range";
#endif
#ifdef EDOTDOT
    case EDOTDOT: return "cross mount point";
#endif
#ifdef EDQUOT
    case EDQUOT: return "disk quota exceeded";
#endif
#ifdef EDUPPKG
    case EDUPPKG: return "duplicate package name";
#endif
#ifdef EEXIST
    case EEXIST: return "file already exists";
#endif
#ifdef EFAULT
    case EFAULT: return "bad address in system call argument";
#endif
#ifdef EFBIG
    case EFBIG: return "file too large";
#endif
#ifdef EHOSTDOWN
    case EHOSTDOWN: return "host is down";
#endif
#ifdef EHOSTUNREACH
    case EHOSTUNREACH: return "host is unreachable";
#endif
#ifdef EIDRM
    case EIDRM: return "identifier removed";
#endif
#ifdef EINIT
    case EINIT: return "initialization error";
#endif
#ifdef EINPROGRESS
    case EINPROGRESS: return "operation now in progress";
#endif
#ifdef EINTR
    case EINTR: return "interrupted system call";
#endif
#ifdef EINVAL
    case EINVAL: return "invalid argument";
#endif
#ifdef EIO
    case EIO: return "I/O error";
#endif
#ifdef EISCONN
    case EISCONN: return "socket is already connected";
#endif
#ifdef EISDIR
    case EISDIR: return "illegal operation on a directory";
#endif
#ifdef EISNAME
    case EISNAM: return "is a name file";
#endif
#ifdef ELBIN
    case ELBIN: return "ELBIN";
#endif
#ifdef EL2HLT
    case EL2HLT: return "level 2 halted";
#endif
#ifdef EL2NSYNC
    case EL2NSYNC: return "level 2 not synchronized";
#endif
#ifdef EL3HLT
    case EL3HLT: return "level 3 halted";
#endif
#ifdef EL3RST
    case EL3RST: return "level 3 reset";
#endif
#ifdef ELIBACC
    case ELIBACC: return "can not access a needed shared library";
#endif
#ifdef ELIBBAD
    case ELIBBAD: return "accessing a corrupted shared library";
#endif
#ifdef ELIBEXEC
    case ELIBEXEC: return "can not exec a shared library directly";
#endif
#ifdef ELIBMAX
    case ELIBMAX: return "attempting to link in more shared libraries than system limit";
#endif
#ifdef ELIBSCN
    case ELIBSCN: return ".lib section in a.out corrupted";
#endif
#ifdef ELNRNG
    case ELNRNG: return "link number out of range";
#endif
#ifdef ELOOP
    case ELOOP: return "too many levels of symbolic links";
#endif
#ifdef EMFILE
    case EMFILE: return "too many open files";
#endif
#ifdef EMLINK
    case EMLINK: return "too many links";
#endif
#ifdef EMSGSIZE
    case EMSGSIZE: return "message too long";
#endif
#ifdef EMULTIHOP
    case EMULTIHOP: return "multihop attempted";
#endif
#ifdef ENAMETOOLONG
    case ENAMETOOLONG: return "file name too long";
#endif
#ifdef ENAVAIL
    case ENAVAIL: return "not available";
#endif
#ifdef ENET
    case ENET: return "ENET";
#endif
#ifdef ENETDOWN
    case ENETDOWN: return "network is down";
#endif
#ifdef ENETRESET
    case ENETRESET: return "network dropped connection on reset";
#endif
#ifdef ENETUNREACH
    case ENETUNREACH: return "network is unreachable";
#endif
#ifdef ENFILE
    case ENFILE: return "file table overflow";
#endif
#ifdef ENOANO
    case ENOANO: return "anode table overflow";
#endif
#if defined(ENOBUFS) && (!defined(ENOSR) || (ENOBUFS != ENOSR))
    case ENOBUFS: return "no buffer space available";
#endif
#ifdef ENOCSI
    case ENOCSI: return "no CSI structure available";
#endif
#ifdef ENODATA
    case ENODATA: return "no data available";
#endif
#ifdef ENODEV
    case ENODEV: return "no such device";
#endif
#ifdef ENOENT
    case ENOENT: return "no such file or directory";
#endif
#ifdef ENOEXEC
    case ENOEXEC: return "exec format error";
#endif
#ifdef ENOLCK
    case ENOLCK: return "no locks available";
#endif
#ifdef ENOLINK
    case ENOLINK: return "link has be severed";
#endif
#ifdef ENOMEM
    case ENOMEM: return "not enough memory";
#endif
#ifdef ENOMSG
    case ENOMSG: return "no message of desired type";
#endif
#ifdef ENONET
    case ENONET: return "machine is not on the network";
#endif
#ifdef ENOPKG
    case ENOPKG: return "package not installed";
#endif
#ifdef ENOPROTOOPT
    case ENOPROTOOPT: return "bad proocol option";
#endif
#ifdef ENOSPC
    case ENOSPC: return "no space left on device";
#endif
#ifdef ENOSR
    case ENOSR: return "out of stream resources";
#endif
#ifdef ENOSTR
    case ENOSTR: return "not a stream device";
#endif
#ifdef ENOSYM
    case ENOSYM: return "unresolved symbol name";
#endif
#ifdef ENOSYS
    case ENOSYS: return "function not implemented";
#endif
#ifdef ENOTBLK
    case ENOTBLK: return "block device required";
#endif
#ifdef ENOTCONN
    case ENOTCONN: return "socket is not connected";
#endif
#ifdef ENOTDIR
    case ENOTDIR: return "not a directory";
#endif
#ifdef ENOTEMPTY
    case ENOTEMPTY: return "directory not empty";
#endif
#ifdef ENOTNAM
    case ENOTNAM: return "not a name file";
#endif
#ifdef ENOTSOCK
    case ENOTSOCK: return "socket operation on non-socket";
#endif
#ifdef ENOTTY
    case ENOTTY: return "inappropriate device for ioctl";
#endif
#ifdef ENOTUNIQ
    case ENOTUNIQ: return "name not unique on network";
#endif
#ifdef ENXIO
    case ENXIO: return "no such device or address";
#endif
#ifdef EOPNOTSUPP
    case EOPNOTSUPP: return "operation not supported on socket";
#endif
#ifdef EPERM
    case EPERM: return "not owner";
#endif
#ifdef EPFNOSUPPORT
    case EPFNOSUPPORT: return "protocol family not supported";
#endif
#ifdef EPIPE
    case EPIPE: return "broken pipe";
#endif
#ifdef EPROCLIM
    case EPROCLIM: return "too many processes";
#endif
#ifdef EPROCUNAVAIL
    case EPROCUNAVAIL: return "bad procedure for program";
#endif
#ifdef EPROGMISMATCH
    case EPROGMISMATCH: return "program version wrong";
#endif
#ifdef EPROGUNAVAIL
    case EPROGUNAVAIL: return "RPC program not available";
#endif
#ifdef EPROTO
    case EPROTO: return "protocol error";
#endif
#ifdef EPROTONOSUPPORT
    case EPROTONOSUPPORT: return "protocol not suppored";
#endif
#ifdef EPROTOTYPE
    case EPROTOTYPE: return "protocol wrong type for socket";
#endif
#ifdef ERANGE
    case ERANGE: return "math result unrepresentable";
#endif
#if defined(EREFUSED) && (!defined(ECONNREFUSED) || (EREFUSED != ECONNREFUSED))
    case EREFUSED: return "EREFUSED";
#endif
#ifdef EREMCHG
    case EREMCHG: return "remote address changed";
#endif
#ifdef EREMDEV
    case EREMDEV: return "remote device";
#endif
#ifdef EREMOTE
    case EREMOTE: return "pathname hit remote file system";
#endif
#ifdef EREMOTEIO
    case EREMOTEIO: return "remote i/o error";
#endif
#ifdef EREMOTERELEASE
    case EREMOTERELEASE: return "EREMOTERELEASE";
#endif
#ifdef EROFS
    case EROFS: return "read-only file system";
#endif
#ifdef ERPCMISMATCH
    case ERPCMISMATCH: return "RPC version is wrong";
#endif
#ifdef ERREMOTE
    case ERREMOTE: return "object is remote";
#endif
#ifdef ESHUTDOWN
    case ESHUTDOWN: return "can't send afer socket shutdown";
#endif
#ifdef ESOCKTNOSUPPORT
    case ESOCKTNOSUPPORT: return "socket type not supported";
#endif
#ifdef ESPIPE
    case ESPIPE: return "invalid seek";
#endif
#ifdef ESRCH
    case ESRCH: return "no such process";
#endif
#ifdef ESRMNT
    case ESRMNT: return "srmount error";
#endif
#ifdef ESTALE
    case ESTALE: return "stale remote file handle";
#endif
#ifdef ESUCCESS
    case ESUCCESS: return "Error 0";
#endif
#ifdef ETIME
    case ETIME: return "timer expired";
#endif
#ifdef ETIMEDOUT
    case ETIMEDOUT: return "connection timed out";
#endif
#ifdef ETOOMANYREFS
    case ETOOMANYREFS: return "too many references: can't splice";
#endif
#ifdef ETXTBSY
    case ETXTBSY: return "text file or pseudo-device busy";
#endif
#ifdef EUCLEAN
    case EUCLEAN: return "structure needs cleaning";
#endif
#ifdef EUNATCH
    case EUNATCH: return "protocol driver not attached";
#endif
#ifdef EUSERS
    case EUSERS: return "too many users";
#endif
#ifdef EVERSION
    case EVERSION: return "version mismatch";
#endif
#if defined(EWOULDBLOCK) && (!defined(EAGAIN) || (EWOULDBLOCK != EAGAIN))
    case EWOULDBLOCK: return "operation would block";
#endif
#ifdef EXDEV
    case EXDEV: return "cross-domain link";
#endif
#ifdef EXFULL
    case EXFULL: return "message tables full";
#endif
    }
#else /* NO_SYS_ERRLIST */
  extern int sys_nerr;
  extern char *sys_errlist[];

  if ((errnum > 0) && (errnum <= sys_nerr))
    return sys_errlist [errnum];
#endif /* NO_SYS_ERRLIST */

  msg = g_static_private_get (&msg_private);
  if (!msg)
    {
      msg = g_new (gchar, 64);
      g_static_private_set (&msg_private, msg, g_free);
    }

  sprintf (msg, "unknown error (%d)", errnum);

  return msg;
}

G_CONST_RETURN gchar*
g_strsignal (gint signum)
{
  static GStaticPrivate msg_private = G_STATIC_PRIVATE_INIT;
  char *msg;

#ifdef HAVE_STRSIGNAL
#if defined(G_OS_BEOS) || defined(G_WITH_CYGWIN)
extern const char *strsignal(int);
#else
  /* this is declared differently (const) in string.h on BeOS */
  extern char *strsignal (int sig);
#endif /* !G_OS_BEOS && !G_WITH_CYGWIN */
  return strsignal (signum);
#elif NO_SYS_SIGLIST
  switch (signum)
    {
#ifdef SIGHUP
    case SIGHUP: return "Hangup";
#endif
#ifdef SIGINT
    case SIGINT: return "Interrupt";
#endif
#ifdef SIGQUIT
    case SIGQUIT: return "Quit";
#endif
#ifdef SIGILL
    case SIGILL: return "Illegal instruction";
#endif
#ifdef SIGTRAP
    case SIGTRAP: return "Trace/breakpoint trap";
#endif
#ifdef SIGABRT
    case SIGABRT: return "IOT trap/Abort";
#endif
#ifdef SIGBUS
    case SIGBUS: return "Bus error";
#endif
#ifdef SIGFPE
    case SIGFPE: return "Floating point exception";
#endif
#ifdef SIGKILL
    case SIGKILL: return "Killed";
#endif
#ifdef SIGUSR1
    case SIGUSR1: return "User defined signal 1";
#endif
#ifdef SIGSEGV
    case SIGSEGV: return "Segmentation fault";
#endif
#ifdef SIGUSR2
    case SIGUSR2: return "User defined signal 2";
#endif
#ifdef SIGPIPE
    case SIGPIPE: return "Broken pipe";
#endif
#ifdef SIGALRM
    case SIGALRM: return "Alarm clock";
#endif
#ifdef SIGTERM
    case SIGTERM: return "Terminated";
#endif
#ifdef SIGSTKFLT
    case SIGSTKFLT: return "Stack fault";
#endif
#ifdef SIGCHLD
    case SIGCHLD: return "Child exited";
#endif
#ifdef SIGCONT
    case SIGCONT: return "Continued";
#endif
#ifdef SIGSTOP
    case SIGSTOP: return "Stopped (signal)";
#endif
#ifdef SIGTSTP
    case SIGTSTP: return "Stopped";
#endif
#ifdef SIGTTIN
    case SIGTTIN: return "Stopped (tty input)";
#endif
#ifdef SIGTTOU
    case SIGTTOU: return "Stopped (tty output)";
#endif
#ifdef SIGURG
    case SIGURG: return "Urgent condition";
#endif
#ifdef SIGXCPU
    case SIGXCPU: return "CPU time limit exceeded";
#endif
#ifdef SIGXFSZ
    case SIGXFSZ: return "File size limit exceeded";
#endif
#ifdef SIGVTALRM
    case SIGVTALRM: return "Virtual time alarm";
#endif
#ifdef SIGPROF
    case SIGPROF: return "Profile signal";
#endif
#ifdef SIGWINCH
    case SIGWINCH: return "Window size changed";
#endif
#ifdef SIGIO
    case SIGIO: return "Possible I/O";
#endif
#ifdef SIGPWR
    case SIGPWR: return "Power failure";
#endif
#ifdef SIGUNUSED
    case SIGUNUSED: return "Unused signal";
#endif
    }
#else /* NO_SYS_SIGLIST */

#ifdef NO_SYS_SIGLIST_DECL
  extern char *sys_siglist[];	/*(see Tue Jan 19 00:44:24 1999 in changelog)*/
#endif

  return (char*) /* this function should return const --josh */ sys_siglist [signum];
#endif /* NO_SYS_SIGLIST */

  msg = g_static_private_get (&msg_private);
  if (!msg)
    {
      msg = g_new (gchar, 64);
      g_static_private_set (&msg_private, msg, g_free);
    }

  sprintf (msg, "unknown signal (%d)", signum);
  
  return msg;
}

/* Functions g_strlcpy and g_strlcat were originally developed by
 * Todd C. Miller <Todd.Miller@courtesan.com> to simplify writing secure code.
 * See ftp://ftp.openbsd.org/pub/OpenBSD/src/lib/libc/string/strlcpy.3
 * for more information.
 */

#ifdef HAVE_STRLCPY
/* Use the native ones, if available; they might be implemented in assembly */
gsize
g_strlcpy (gchar       *dest,
	   const gchar *src,
	   gsize        dest_size)
{
  g_return_val_if_fail (dest != NULL, 0);
  g_return_val_if_fail (src  != NULL, 0);
  
  return strlcpy (dest, src, dest_size);
}

gsize
g_strlcat (gchar       *dest,
	   const gchar *src,
	   gsize        dest_size)
{
  g_return_val_if_fail (dest != NULL, 0);
  g_return_val_if_fail (src  != NULL, 0);
  
  return strlcat (dest, src, dest_size);
}

#else /* ! HAVE_STRLCPY */
/* g_strlcpy
 *
 * Copy string src to buffer dest (of buffer size dest_size).  At most
 * dest_size-1 characters will be copied.  Always NUL terminates
 * (unless dest_size == 0).  This function does NOT allocate memory.
 * Unlike strncpy, this function doesn't pad dest (so it's often faster).
 * Returns size of attempted result, strlen(src),
 * so if retval >= dest_size, truncation occurred.
 */
gsize
g_strlcpy (gchar       *dest,
           const gchar *src,
           gsize        dest_size)
{
  register gchar *d = dest;
  register const gchar *s = src;
  register gsize n = dest_size;
  
  g_return_val_if_fail (dest != NULL, 0);
  g_return_val_if_fail (src  != NULL, 0);
  
  /* Copy as many bytes as will fit */
  if (n != 0 && --n != 0)
    do
      {
	register gchar c = *s++;
	
	*d++ = c;
	if (c == 0)
	  break;
      }
    while (--n != 0);
  
  /* If not enough room in dest, add NUL and traverse rest of src */
  if (n == 0)
    {
      if (dest_size != 0)
	*d = 0;
      while (*s++)
	;
    }
  
  return s - src - 1;  /* count does not include NUL */
}

/* g_strlcat
 *
 * Appends string src to buffer dest (of buffer size dest_size).
 * At most dest_size-1 characters will be copied.
 * Unlike strncat, dest_size is the full size of dest, not the space left over.
 * This function does NOT allocate memory.
 * This always NUL terminates (unless siz == 0 or there were no NUL characters
 * in the dest_size characters of dest to start with).
 * Returns size of attempted result, which is
 * MIN (dest_size, strlen (original dest)) + strlen (src),
 * so if retval >= dest_size, truncation occurred.
 */
gsize
g_strlcat (gchar       *dest,
           const gchar *src,
           gsize        dest_size)
{
  register gchar *d = dest;
  register const gchar *s = src;
  register gsize bytes_left = dest_size;
  gsize dlength;  /* Logically, MIN (strlen (d), dest_size) */
  
  g_return_val_if_fail (dest != NULL, 0);
  g_return_val_if_fail (src  != NULL, 0);
  
  /* Find the end of dst and adjust bytes left but don't go past end */
  while (*d != 0 && bytes_left-- != 0)
    d++;
  dlength = d - dest;
  bytes_left = dest_size - dlength;
  
  if (bytes_left == 0)
    return dlength + strlen (s);
  
  while (*s != 0)
    {
      if (bytes_left != 1)
	{
	  *d++ = *s;
	  bytes_left--;
	}
      s++;
    }
  *d = 0;
  
  return dlength + (s - src);  /* count does not include NUL */
}
#endif /* ! HAVE_STRLCPY */

gchar*
g_strdown (gchar *string)
{
  register guchar *s;
  
  g_return_val_if_fail (string != NULL, NULL);
  
  s = (guchar *) string;
  
  while (*s)
    {
      if (isupper (*s))
	*s = tolower (*s);
      s++;
    }
  
  return (gchar *) string;
}

gchar*
g_strup (gchar *string)
{
  register guchar *s;

  g_return_val_if_fail (string != NULL, NULL);

  s = (guchar *) string;

  while (*s)
    {
      if (islower (*s))
	*s = toupper (*s);
      s++;
    }

  return (gchar *) string;
}

gchar*
g_strreverse (gchar *string)
{
  g_return_val_if_fail (string != NULL, NULL);

  if (*string)
    {
      register gchar *h, *t;

      h = string;
      t = string + strlen (string) - 1;

      while (h < t)
	{
	  register gchar c;

	  c = *h;
	  *h = *t;
	  h++;
	  *t = c;
	  t--;
	}
    }

  return string;
}

gint
g_strcasecmp (const gchar *s1,
	      const gchar *s2)
{
#ifdef HAVE_STRCASECMP
  g_return_val_if_fail (s1 != NULL, 0);
  g_return_val_if_fail (s2 != NULL, 0);

  return strcasecmp (s1, s2);
#else
  gint c1, c2;

  g_return_val_if_fail (s1 != NULL, 0);
  g_return_val_if_fail (s2 != NULL, 0);

  while (*s1 && *s2)
    {
      /* According to A. Cox, some platforms have islower's that
       * don't work right on non-uppercase
       */
      c1 = isupper ((guchar)*s1) ? tolower ((guchar)*s1) : *s1;
      c2 = isupper ((guchar)*s2) ? tolower ((guchar)*s2) : *s2;
      if (c1 != c2)
	return (c1 - c2);
      s1++; s2++;
    }

  return (((gint)(guchar) *s1) - ((gint)(guchar) *s2));
#endif
}

gint
g_strncasecmp (const gchar *s1,
	       const gchar *s2,
	       guint n)
{
#ifdef HAVE_STRNCASECMP
  return strncasecmp (s1, s2, n);
#else
  gint c1, c2;

  g_return_val_if_fail (s1 != NULL, 0);
  g_return_val_if_fail (s2 != NULL, 0);

  while (n && *s1 && *s2)
    {
      n -= 1;
      /* According to A. Cox, some platforms have islower's that
       * don't work right on non-uppercase
       */
      c1 = isupper ((guchar)*s1) ? tolower ((guchar)*s1) : *s1;
      c2 = isupper ((guchar)*s2) ? tolower ((guchar)*s2) : *s2;
      if (c1 != c2)
	return (c1 - c2);
      s1++; s2++;
    }

  if (n)
    return (((gint) (guchar) *s1) - ((gint) (guchar) *s2));
  else
    return 0;
#endif
}

gchar*
g_strdelimit (gchar	  *string,
	      const gchar *delimiters,
	      gchar	   new_delim)
{
  register gchar *c;

  g_return_val_if_fail (string != NULL, NULL);

  if (!delimiters)
    delimiters = G_STR_DELIMITERS;

  for (c = string; *c; c++)
    {
      if (strchr (delimiters, *c))
	*c = new_delim;
    }

  return string;
}

gchar*
g_strcanon (gchar       *string,
	    const gchar *valid_chars,
	    gchar        substitutor)
{
  register gchar *c;

  g_return_val_if_fail (string != NULL, NULL);
  g_return_val_if_fail (valid_chars != NULL, NULL);

  for (c = string; *c; c++)
    {
      if (!strchr (valid_chars, *c))
	*c = substitutor;
    }

  return string;
}

gchar*
g_strcompress (const gchar *source)
{
  const gchar *p = source, *octal;
  gchar *dest = g_malloc (strlen (source) + 1);
  gchar *q = dest;
  
  while (*p)
    {
      if (*p == '\\')
	{
	  p++;
	  switch (*p)
	    {
	    case '0':  case '1':  case '2':  case '3':  case '4':
	    case '5':  case '6':  case '7':
	      *q = 0;
	      octal = p;
	      while ((p < octal + 3) && (*p >= '0') && (*p <= '7'))
		{
		  *q = (*q * 8) + (*p - '0');
		  p++;
		}
	      q++;
	      p--;
	      break;
	    case 'b':
	      *q++ = '\b';
	      break;
	    case 'f':
	      *q++ = '\f';
	      break;
	    case 'n':
	      *q++ = '\n';
	      break;
	    case 'r':
	      *q++ = '\r';
	      break;
	    case 't':
	      *q++ = '\t';
	      break;
	    default:		/* Also handles \" and \\ */
	      *q++ = *p;
	      break;
	    }
	}
      else
	*q++ = *p;
      p++;
    }
  *q = 0;
  
  return dest;
}

gchar *
g_strescape (const gchar *source,
	     const gchar *exceptions)
{
  const guchar *p;
  gchar *dest;
  gchar *q;
  guchar excmap[256];
  
  g_return_val_if_fail (source != NULL, NULL);

  p = (guchar *) source;
  /* Each source byte needs maximally four destination chars (\777) */
  q = dest = g_malloc (strlen (source) * 4 + 1);

  memset (excmap, 0, 256);
  if (exceptions)
    {
      guchar *e = (guchar *) exceptions;

      while (*e)
	{
	  excmap[*e] = 1;
	  e++;
	}
    }

  while (*p)
    {
      if (excmap[*p])
	*q++ = *p;
      else
	{
	  switch (*p)
	    {
	    case '\b':
	      *q++ = '\\';
	      *q++ = 'b';
	      break;
	    case '\f':
	      *q++ = '\\';
	      *q++ = 'f';
	      break;
	    case '\n':
	      *q++ = '\\';
	      *q++ = 'n';
	      break;
	    case '\r':
	      *q++ = '\\';
	      *q++ = 'r';
	      break;
	    case '\t':
	      *q++ = '\\';
	      *q++ = 't';
	      break;
	    case '\\':
	      *q++ = '\\';
	      *q++ = '\\';
	      break;
	    case '"':
	      *q++ = '\\';
	      *q++ = '"';
	      break;
	    default:
	      if ((*p < ' ') || (*p >= 0177))
		{
		  *q++ = '\\';
		  *q++ = '0' + (((*p) >> 6) & 07);
		  *q++ = '0' + (((*p) >> 3) & 07);
		  *q++ = '0' + ((*p) & 07);
		}
	      else
		*q++ = *p;
	      break;
	    }
	}
      p++;
    }
  *q = 0;
  return dest;
}

gchar*
g_strchug (gchar *string)
{
  guchar *start;

  g_return_val_if_fail (string != NULL, NULL);

  for (start = (guchar*) string; *start && isspace (*start); start++)
    ;

  g_memmove (string, start, strlen ((gchar *) start) + 1);

  return string;
}

gchar*
g_strchomp (gchar *string)
{
  gchar *s;

  g_return_val_if_fail (string != NULL, NULL);

  if (!*string)
    return string;

  for (s = string + strlen (string) - 1; s >= string && isspace ((guchar)*s); 
       s--)
    *s = '\0';

  return string;
}

gchar**
g_strsplit (const gchar *string,
	    const gchar *delimiter,
	    gint         max_tokens)
{
  GSList *string_list = NULL, *slist;
  gchar **str_array, *s;
  guint n = 1;

  g_return_val_if_fail (string != NULL, NULL);
  g_return_val_if_fail (delimiter != NULL, NULL);

  if (max_tokens < 1)
    max_tokens = G_MAXINT;

  s = strstr (string, delimiter);
  if (s)
    {
      guint delimiter_len = strlen (delimiter);

      do
	{
	  guint len;
	  gchar *new_string;

	  len = s - string;
	  new_string = g_new (gchar, len + 1);
	  strncpy (new_string, string, len);
	  new_string[len] = 0;
	  string_list = g_slist_prepend (string_list, new_string);
	  n++;
	  string = s + delimiter_len;
	  s = strstr (string, delimiter);
	}
      while (--max_tokens && s);
    }
  string_list = g_slist_prepend (string_list, g_strdup (string));

  str_array = g_new (gchar*, n + 1);

  str_array[n--] = NULL;
  for (slist = string_list; slist; slist = slist->next)
    str_array[n--] = slist->data;

  g_slist_free (string_list);

  return str_array;
}

void
g_strfreev (gchar **str_array)
{
  if (str_array)
    {
      int i;

      for(i = 0; str_array[i] != NULL; i++)
	g_free(str_array[i]);

      g_free (str_array);
    }
}

/**
 * g_strdupv:
 * @str_array: %NULL-terminated array of strings
 * 
 * Copies %NULL-terminated array of strings. The copy is a deep copy;
 * the new array should be freed by first freeing each string, then
 * the array itself. g_strfreev() does this for you. If called
 * on a %NULL value, g_strdupv() simply returns %NULL.
 * 
 * Return value: a new %NULL-terminated array of strings
 **/
gchar**
g_strdupv (gchar **str_array)
{
  if (str_array)
    {
      gint i;
      gchar **retval;

      i = 0;
      while (str_array[i])
        ++i;
          
      retval = g_new (gchar*, i + 1);

      i = 0;
      while (str_array[i])
        {
          retval[i] = g_strdup (str_array[i]);
          ++i;
        }
      retval[i] = NULL;

      return retval;
    }
  else
    return NULL;
}

gchar*
g_strjoinv (const gchar  *separator,
	    gchar       **str_array)
{
  gchar *string;
  gchar *ptr;

  g_return_val_if_fail (str_array != NULL, NULL);

  if (separator == NULL)
    separator = "";

  if (*str_array)
    {
      guint i, len;
      guint separator_len;

      separator_len = strlen (separator);
      /* First part, getting length */
      len = 1 + strlen (str_array[0]);
      for (i = 1; str_array[i] != NULL; i++)
        len += strlen (str_array[i]);
      len += separator_len * (i - 1);

      /* Second part, building string */
      string = g_new (gchar, len);
      ptr = g_stpcpy (string, *str_array);
      for (i = 1; str_array[i] != NULL; i++)
	{
          ptr = g_stpcpy (ptr, separator);
          ptr = g_stpcpy (ptr, str_array[i]);
	}
      }
  else
    string = g_strdup ("");

  return string;
}

gchar*
g_strjoin (const gchar  *separator,
	   ...)
{
  gchar *string, *s;
  va_list args;
  guint len;
  guint separator_len;
  gchar *ptr;

  if (separator == NULL)
    separator = "";

  separator_len = strlen (separator);

  va_start (args, separator);

  s = va_arg (args, gchar*);

  if (s)
    {
      /* First part, getting length */
      len = 1 + strlen (s);

      s = va_arg (args, gchar*);
      while (s)
	{
	  len += separator_len + strlen (s);
	  s = va_arg (args, gchar*);
	}
      va_end (args);

      /* Second part, building string */
      string = g_new (gchar, len);

      va_start (args, separator);

      s = va_arg (args, gchar*);
      ptr = g_stpcpy (string, s);

      s = va_arg (args, gchar*);
      while (s)
	{
	  ptr = g_stpcpy (ptr, separator);
          ptr = g_stpcpy (ptr, s);
	  s = va_arg (args, gchar*);
	}
    }
  else
    string = g_strdup ("");

  va_end (args);

  return string;
}


/**
 * g_strstr_len:
 * @haystack: a string
 * @haystack_len: The maximum length of haystack
 * @needle: The string to search for.
 *
 * Searches the string haystack for the first occurrence
 * of the string needle, limiting the length of the search
 * to haystack_len. 
 *
 * Return value: A pointer to the found occurrence, or
 * NULL if not found.
 **/
gchar *
g_strstr_len (const gchar *haystack,
	      gint         haystack_len,
	      const gchar *needle)
{
  int i;

  g_return_val_if_fail (haystack != NULL, NULL);
  g_return_val_if_fail (needle != NULL, NULL);
  
  if (haystack_len < 0)
    return strstr (haystack, needle);
  else
    {
      const char *p = haystack;
      int needle_len = strlen (needle);
      const char *end = haystack + haystack_len - needle_len;
      
      if (needle_len == 0)
	return (char *)haystack;

      while (*p && p <= end)
	{
	  for (i = 0; i < needle_len; i++)
	    if (p[i] != needle[i])
	      goto next;
	  
	  return (char *)p;
	  
	next:
	  p++;
	}
    }
  
  return NULL;
}

/**
 * g_strrstr_len:
 * @haystack: a nul-terminated string
 * @needle: The nul-terminated string to search for.
 *
 * Searches the string haystack for the last occurrence
 * of the string needle.
 *
 * Return value: A pointer to the found occurrence, or
 * NULL if not found.
 **/
gchar *
g_strrstr (const gchar *haystack,
	   const gchar *needle)
{
  int i;
  int needle_len = strlen (needle);
  int haystack_len = strlen (haystack);
  const char *p = haystack + haystack_len - needle_len;
      
  g_return_val_if_fail (haystack != NULL, NULL);
  g_return_val_if_fail (needle != NULL, NULL);
  
  if (needle_len == 0)
    return (char *)p;
  
  while (p >= haystack)
    {
      for (i = 0; i < needle_len; i++)
	if (p[i] != needle[i])
	  goto next;
      
      return (char *)p;
      
    next:
      p--;
    }
  
  return NULL;
}

/**
 * g_strrstr_len:
 * @haystack: a nul-terminated string
 * @haystack_len: The maximum length of haystack
 * @needle: The nul-terminated string to search for.
 *
 * Searches the string haystack for the last occurrence
 * of the string needle, limiting the length of the search
 * to haystack_len. 
 *
 * Return value: A pointer to the found occurrence, or
 * NULL if not found.
 **/
gchar *
g_strrstr_len (const gchar *haystack,
	       gint         haystack_len,
	       const gchar *needle)
{
  int i;
      
  g_return_val_if_fail (haystack != NULL, NULL);
  g_return_val_if_fail (needle != NULL, NULL);
  
  if (haystack_len < 0)
    return g_strrstr (haystack, needle);
  else
    {
      int needle_len = strlen (needle);
      const char *haystack_max = haystack + haystack_len;
      const char *p = haystack;

      while (p < haystack_max && *p)
	p++;

      p -= needle_len;

      while (p >= haystack)
	{
	  for (i = 0; i < needle_len; i++)
	    if (p[i] != needle[i])
	      goto next;
	  
	  return (char *)p;
	  
	next:
	  p--;
	}
    }

  return NULL;
}


