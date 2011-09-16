/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * gthread.c: solaris thread system implementation
 * Copyright 1998-2001 Sebastian Wilhelmi; University of Karlsruhe
 * Copyright 2001 Hans Breuer
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

/*
 * Modified by the GLib Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/.
 */

/* The GMutex and GCond implementations in this file are some of the
 * lowest-level code in GLib.  All other parts of GLib (messages,
 * memory, slices, etc) assume that they can freely use these facilities
 * without risking recursion.
 *
 * As such, these functions are NOT permitted to call any other part of
 * GLib.
 *
 * The thread manipulation functions (create, exit, join, etc.) have
 * more freedom -- they can do as they please.
 */

#include "config.h"

#include "gthread.h"

#define _WIN32_WINDOWS 0x0401 /* to get IsDebuggerPresent */
#include <windows.h>

#include <process.h>
#include <stdlib.h>
#include <stdio.h>

static void
g_thread_abort (gint         status,
                const gchar *function)
{
  fprintf (stderr, "GLib (gthread-win32.c): Unexpected error from C library during '%s': %s.  Aborting.\n",
           strerror (status), function);
  abort ();
}

/* Starting with Vista and Windows 2008, we have access to the
 * CONDITION_VARIABLE and SRWLock primatives on Windows, which are
 * pretty reasonable approximations of the primatives specified in
 * POSIX 2001 (pthread_cond_t and pthread_mutex_t respectively).
 *
 * Both of these types are structs containing a single pointer.  That
 * pointer is used as an atomic bitfield to support user-space mutexes
 * that only get the kernel involved in cases of contention (similar
 * to how futex()-based mutexes work on Linux).  The biggest advantage
 * of these new types is that they can be statically initialised to
 * zero.  This allows us to use them directly and still support:
 *
 *   GMutex mutex = G_MUTEX_INIT;
 *
 * and
 *
 *   GCond cond = G_COND_INIT;
 *
 * Unfortunately, Windows XP lacks these facilities and GLib still
 * needs to support Windows XP.  Our approach here is as follows:
 *
 *   - avoid depending on structure declarations at compile-time by
 *     declaring our own GMutex and GCond strutures to be
 *     ABI-compatible with SRWLock and CONDITION_VARIABLE and using
 *     those instead
 *
 *   - avoid a hard dependency on the symbols used to manipulate these
 *     structures by doing a dynamic lookup of those symbols at
 *     runtime
 *
 *   - if the symbols are not available, emulate them using other
 *     primatives
 *
 * Using this approach also allows us to easily build a GLib that lacks
 * support for Windows XP or to remove this code entirely when XP is no
 * longer supported (end of line is currently April 8, 2014).
 */
typedef struct
{
  void     (* CallThisOnThreadExit)        (void);              /* fake */

  void     (* InitializeSRWLock)           (gpointer lock);
  void     (* DeleteSRWLock)               (gpointer lock);     /* fake */
  void     (* AcquireSRWLockExclusive)     (gpointer lock);
  BOOLEAN  (* TryAcquireSRWLockExclusive)  (gpointer lock);
  void     (* ReleaseSRWLockExclusive)     (gpointer lock);

  void     (* InitializeConditionVariable) (gpointer cond);
  void     (* DeleteConditionVariable)     (gpointer cond);     /* fake */
  BOOL     (* SleepConditionVariableSRW)   (gpointer cond,
                                            gpointer lock,
                                            DWORD    timeout,
                                            ULONG    flags);
  void     (* WakeAllConditionVariable)    (gpointer cond);
  void     (* WakeConditionVariable)       (gpointer cond);
} GThreadImplVtable;

static GThreadImplVtable g_thread_impl_vtable;

/* {{{1 GMutex */
void
g_mutex_init (GMutex *mutex)
{
  g_thread_impl_vtable.InitializeSRWLock (mutex);
}

void
g_mutex_clear (GMutex *mutex)
{
  if (g_thread_impl_vtable.DeleteSRWLock != NULL)
    g_thread_impl_vtable.DeleteSRWLock (mutex);
}

void
g_mutex_lock (GMutex *mutex)
{
  /* temporary until we fix libglib */
  if (mutex == NULL)
    return;

  g_thread_impl_vtable.AcquireSRWLockExclusive (mutex);
}

gboolean
g_mutex_trylock (GMutex *mutex)
{
  /* temporary until we fix libglib */
  if (mutex == NULL)
    return TRUE;

  return g_thread_impl_vtable.TryAcquireSRWLockExclusive (mutex);
}

void
g_mutex_unlock (GMutex *mutex)
{
  /* temporary until we fix libglib */
  if (mutex == NULL)
    return;

  g_thread_impl_vtable.ReleaseSRWLockExclusive (mutex);
}

/* {{{1 GCond */
void
g_cond_init (GCond *cond)
{
  g_thread_impl_vtable.InitializeConditionVariable (cond);
}

void
g_cond_clear (GCond *cond)
{
  if (g_thread_impl_vtable.DeleteConditionVariable)
    g_thread_impl_vtable.DeleteConditionVariable (cond);
}

void
g_cond_signal (GCond *cond)
{
  /* temporary until we fix libglib */
  if (cond == NULL)
    return;

  g_thread_impl_vtable.WakeConditionVariable (cond);
}

void
g_cond_broadcast (GCond *cond)
{
  /* temporary until we fix libglib */
  if (cond == NULL)
    return;

  g_thread_impl_vtable.WakeAllConditionVariable (cond);
}

void
g_cond_wait (GCond  *cond,
             GMutex *entered_mutex)
{
  g_thread_impl_vtable.SleepConditionVariableSRW (cond, entered_mutex, INFINITE, 0);
}

gboolean
g_cond_timedwait (GCond  *cond,
                  GMutex *entered_mutex,
                  gint64  abs_time)
{
  gint64 span;
  FILETIME ft;
  gint64 now;

  GetSystemTimeAsFileTime (&ft);
  memmove (&now, &ft, sizeof (FILETIME));

  now -= G_GINT64_CONSTANT (116444736000000000);
  now /= 10;

  span = abs_time - now;

  if G_UNLIKELY (span < 0)
    span = 0;

  if G_UNLIKELY (span > G_GINT64_CONSTANT (1000) * G_MAXINT32)
    span = INFINITE;

  return g_thread_impl_vtable.SleepConditionVariableSRW (cond, entered_mutex, span / 1000, 0);
}

gboolean
g_cond_timed_wait (GCond    *cond,
                   GMutex   *entered_mutex,
                   GTimeVal *abs_time)
{
  if (abs_time)
    {
      gint64 micros;

      micros = abs_time->tv_sec;
      micros *= 1000000;
      micros += abs_time->tv_usec;

      return g_cond_timedwait (cond, entered_mutex, micros);
    }
  else
    {
      g_cond_wait (cond, entered_mutex);
      return TRUE;
    }
}

/* {{{1 new/free API */
GMutex *
g_mutex_new (void)
{
  GMutex *mutex;

  /* malloc() is temporary until all libglib users are ported away */
  mutex = malloc (sizeof (GMutex));
  if G_UNLIKELY (mutex == NULL)
    g_thread_abort (errno, "malloc");
  g_mutex_init (mutex);

  return mutex;
}

void
g_mutex_free (GMutex *mutex)
{
  g_mutex_clear (mutex);
  free (mutex);
}

GCond *
g_cond_new (void)
{
  GCond *cond;

  /* malloc() is temporary until all libglib users are ported away */
  cond = malloc (sizeof (GCond));
  if G_UNLIKELY (cond == NULL)
    g_thread_abort (errno, "malloc");
  g_cond_init (cond);

  return cond;
}

void
g_cond_free (GCond *cond)
{
  g_cond_clear (cond);
  free (cond);
}

/* {{{1 GPrivate */

#include "glib.h"
#include "gthreadprivate.h"

#define win32_check_for_error(what) G_STMT_START{			\
  if (!(what))								\
    g_error ("file %s: line %d (%s): error %s during %s",		\
	     __FILE__, __LINE__, G_STRFUNC,				\
	     g_win32_error_message (GetLastError ()), #what);		\
  }G_STMT_END

#define G_MUTEX_SIZE (sizeof (gpointer))

static DWORD g_thread_self_tls;
static DWORD g_private_tls;
static CRITICAL_SECTION g_thread_global_spinlock;

typedef BOOL (__stdcall *GTryEnterCriticalSectionFunc) (CRITICAL_SECTION *);

/* As noted in the docs, GPrivate is a limited resource, here we take
 * a rather low maximum to save memory, use GStaticPrivate instead. */
#define G_PRIVATE_MAX 100

static GDestroyNotify g_private_destructors[G_PRIVATE_MAX];

static guint g_private_next = 0;

typedef struct _GThreadData GThreadData;
struct _GThreadData
{
  GThreadFunc func;
  gpointer data;
  HANDLE thread;
  gboolean joinable;
};

static GPrivate *
g_private_new_win32_impl (GDestroyNotify destructor)
{
  GPrivate *result;
  EnterCriticalSection (&g_thread_global_spinlock);
  if (g_private_next >= G_PRIVATE_MAX)
    {
      char buf[100];
      sprintf (buf,
	       "Too many GPrivate allocated. Their number is limited to %d.",
	       G_PRIVATE_MAX);
      MessageBox (NULL, buf, NULL, MB_ICONERROR|MB_SETFOREGROUND);
      if (IsDebuggerPresent ())
	G_BREAKPOINT ();
      abort ();
    }
  g_private_destructors[g_private_next] = destructor;
  result = GUINT_TO_POINTER (g_private_next);
  g_private_next++;
  LeaveCriticalSection (&g_thread_global_spinlock);

  return result;
}

/* NOTE: the functions g_private_get and g_private_set may not use
   functions from gmem.c and gmessages.c */

static void
g_private_set_win32_impl (GPrivate * private_key, gpointer value)
{
  gpointer* array = TlsGetValue (g_private_tls);
  guint index = GPOINTER_TO_UINT (private_key);

  if (index >= G_PRIVATE_MAX)
      return;

  if (!array)
    {
      array = (gpointer*) calloc (G_PRIVATE_MAX, sizeof (gpointer));
      TlsSetValue (g_private_tls, array);
    }

  array[index] = value;
}

static gpointer
g_private_get_win32_impl (GPrivate * private_key)
{
  gpointer* array = TlsGetValue (g_private_tls);
  guint index = GPOINTER_TO_UINT (private_key);

  if (index >= G_PRIVATE_MAX || !array)
    return NULL;

  return array[index];
}

/* {{{1 GThread */

static void
g_thread_set_priority_win32_impl (gpointer thread, GThreadPriority priority)
{
  GThreadData *target = *(GThreadData **)thread;
  gint native_prio;

  switch (priority)
    {
    case G_THREAD_PRIORITY_LOW:
      native_prio = THREAD_PRIORITY_BELOW_NORMAL;
      break;

    case G_THREAD_PRIORITY_NORMAL:
      native_prio = THREAD_PRIORITY_NORMAL;
      break;

    case G_THREAD_PRIORITY_HIGH:
      native_prio = THREAD_PRIORITY_ABOVE_NORMAL;
      break;

    case G_THREAD_PRIORITY_URGENT:
      native_prio = THREAD_PRIORITY_HIGHEST;
      break;

    default:
      g_return_if_reached ();
    }

  win32_check_for_error (SetThreadPriority (target->thread, native_prio));
}

static void
g_thread_self_win32_impl (gpointer thread)
{
  GThreadData *self = TlsGetValue (g_thread_self_tls);

  if (!self)
    {
      /* This should only happen for the main thread! */
      HANDLE handle = GetCurrentThread ();
      HANDLE process = GetCurrentProcess ();
      self = g_new (GThreadData, 1);
      win32_check_for_error (DuplicateHandle (process, handle, process,
					      &self->thread, 0, FALSE,
					      DUPLICATE_SAME_ACCESS));
      win32_check_for_error (TlsSetValue (g_thread_self_tls, self));
      self->func = NULL;
      self->data = NULL;
      self->joinable = FALSE;
    }

  *(GThreadData **)thread = self;
}

static void
g_thread_exit_win32_impl (void)
{
  GThreadData *self = TlsGetValue (g_thread_self_tls);
  guint i, private_max;
  gpointer *array = TlsGetValue (g_private_tls);

  EnterCriticalSection (&g_thread_global_spinlock);
  private_max = g_private_next;
  LeaveCriticalSection (&g_thread_global_spinlock);

  if (array)
    {
      gboolean some_data_non_null;

      do {
	some_data_non_null = FALSE;
	for (i = 0; i < private_max; i++)
	  {
	    GDestroyNotify destructor = g_private_destructors[i];
	    GDestroyNotify data = array[i];

	    if (data)
	      some_data_non_null = TRUE;

	    array[i] = NULL;

	    if (destructor && data)
	      destructor (data);
	  }
      } while (some_data_non_null);

      free (array);

      win32_check_for_error (TlsSetValue (g_private_tls, NULL));
    }

  if (self)
    {
      if (!self->joinable)
	{
	  win32_check_for_error (CloseHandle (self->thread));
	  g_free (self);
	}
      win32_check_for_error (TlsSetValue (g_thread_self_tls, NULL));
    }

  if (g_thread_impl_vtable.CallThisOnThreadExit)
    g_thread_impl_vtable.CallThisOnThreadExit ();

  _endthreadex (0);
}

static guint __stdcall
g_thread_proxy (gpointer data)
{
  GThreadData *self = (GThreadData*) data;

  win32_check_for_error (TlsSetValue (g_thread_self_tls, self));

  self->func (self->data);

  g_thread_exit_win32_impl ();

  g_assert_not_reached ();

  return 0;
}

static void
g_thread_create_win32_impl (GThreadFunc func,
			    gpointer data,
			    gulong stack_size,
			    gboolean joinable,
			    gboolean bound,
			    GThreadPriority priority,
			    gpointer thread,
			    GError **error)
{
  guint ignore;
  GThreadData *retval;

  g_return_if_fail (func);
  g_return_if_fail (priority >= G_THREAD_PRIORITY_LOW);
  g_return_if_fail (priority <= G_THREAD_PRIORITY_URGENT);

  retval = g_new(GThreadData, 1);
  retval->func = func;
  retval->data = data;

  retval->joinable = joinable;

  retval->thread = (HANDLE) _beginthreadex (NULL, stack_size, g_thread_proxy,
					    retval, 0, &ignore);

  if (retval->thread == NULL)
    {
      gchar *win_error = g_win32_error_message (GetLastError ());
      g_set_error (error, G_THREAD_ERROR, G_THREAD_ERROR_AGAIN,
                   "Error creating thread: %s", win_error);
      g_free (retval);
      g_free (win_error);
      return;
    }

  *(GThreadData **)thread = retval;

  g_thread_set_priority_win32_impl (thread, priority);
}

static void
g_thread_yield_win32_impl (void)
{
  Sleep(0);
}

static void
g_thread_join_win32_impl (gpointer thread)
{
  GThreadData *target = *(GThreadData **)thread;

  g_return_if_fail (target->joinable);

  win32_check_for_error (WAIT_FAILED !=
			 WaitForSingleObject (target->thread, INFINITE));

  win32_check_for_error (CloseHandle (target->thread));
  g_free (target);
}

/* {{{1 SRWLock and CONDITION_VARIABLE emulation (for Windows XP) */

static DWORD            g_thread_xp_waiter_tls;
static CRITICAL_SECTION g_thread_xp_lock;

/* {{{2 GThreadWaiter utility class for CONDITION_VARIABLE emulation */
typedef struct _GThreadXpWaiter GThreadXpWaiter;
struct _GThreadXpWaiter
{
  HANDLE                    event;
  volatile GThreadXpWaiter *next;
};

static GThreadXpWaiter *
g_thread_xp_waiter_get (void)
{
  GThreadXpWaiter *waiter;

  waiter = TlsGetValue (g_thread_xp_waiter_tls);

  if G_UNLIKELY (waiter == NULL)
    {
      waiter = malloc (sizeof (GThreadXpWaiter));
      if (waiter == NULL)
        g_thread_abort (GetLastError (), "malloc");
      waiter->event = CreateEvent (0, FALSE, FALSE, NULL);
      if (waiter->event == NULL)
        g_thread_abort (GetLastError (), "CreateEvent");

      TlsSetValue (g_thread_xp_waiter_tls, waiter);
    }

  return waiter;
}

static void
g_thread_xp_CallThisOnThreadExit (void)
{
  GThreadXpWaiter *waiter;

  waiter = TlsGetValue (g_thread_xp_waiter_tls);

  if (waiter != NULL)
    {
      TlsSetValue (g_thread_xp_waiter_tls, NULL);
      CloseHandle (waiter->event);
      free (waiter);
    }
}

/* {{{2 SRWLock emulation */
typedef struct
{
  CRITICAL_SECTION critical_section;
} GThreadSRWLock;

static void
g_thread_xp_InitializeSRWLock (gpointer mutex)
{
  *(GThreadSRWLock * volatile *) mutex = NULL;
}

static void
g_thread_xp_DeleteSRWLock (gpointer mutex)
{
  GThreadSRWLock *lock = *(GThreadSRWLock * volatile *) mutex;

  if (lock)
    {
      DeleteCriticalSection (&lock->critical_section);
      free (lock);
    }
}

static GThreadSRWLock *
g_thread_xp_get_srwlock (GThreadSRWLock * volatile *lock)
{
  GThreadSRWLock *result;

  /* It looks like we're missing some barriers here, but this code only
   * ever runs on Windows XP, which in turn only ever runs on hardware
   * with a relatively rigid memory model.  The 'volatile' will take
   * care of the compiler.
   */
  result = *lock;

  if G_UNLIKELY (result == NULL)
    {
      EnterCriticalSection (&g_thread_xp_lock);

      result = malloc (sizeof (GThreadSRWLock));

      if (result == NULL)
        g_thread_abort (errno, "malloc");

      InitializeCriticalSection (&result->critical_section);
      *lock = result;

      LeaveCriticalSection (&g_thread_xp_lock);
    }

  return result;
}

static void
g_thread_xp_AcquireSRWLockExclusive (gpointer mutex)
{
  GThreadSRWLock *lock = g_thread_xp_get_srwlock (mutex);

  EnterCriticalSection (&lock->critical_section);
}

static BOOLEAN
g_thread_xp_TryAcquireSRWLockExclusive (gpointer mutex)
{
  GThreadSRWLock *lock = g_thread_xp_get_srwlock (mutex);

  return TryEnterCriticalSection (&lock->critical_section);
}

static void
g_thread_xp_ReleaseSRWLockExclusive (gpointer mutex)
{
  GThreadSRWLock *lock = *(GThreadSRWLock * volatile *) mutex;

  /* We need this until we fix some weird parts of GLib that try to
   * unlock freshly-allocated mutexes.
   */
  if (lock != NULL)
    LeaveCriticalSection (&lock->critical_section);
}

/* {{{2 CONDITION_VARIABLE emulation */
typedef struct
{
  volatile GThreadXpWaiter  *first;
  volatile GThreadXpWaiter **last_ptr;
} GThreadXpCONDITION_VARIABLE;

static void
g_thread_xp_InitializeConditionVariable (gpointer cond)
{
  *(GThreadXpCONDITION_VARIABLE * volatile *) cond = NULL;
}

static void
g_thread_xp_DeleteConditionVariable (gpointer cond)
{
  GThreadXpCONDITION_VARIABLE *cv = *(GThreadXpCONDITION_VARIABLE * volatile *) cond;

  if (cv)
    free (cv);
}

static GThreadXpCONDITION_VARIABLE *
g_thread_xp_get_condition_variable (GThreadXpCONDITION_VARIABLE * volatile *cond)
{
  GThreadXpCONDITION_VARIABLE *result;

  /* It looks like we're missing some barriers here, but this code only
   * ever runs on Windows XP, which in turn only ever runs on hardware
   * with a relatively rigid memory model.  The 'volatile' will take
   * care of the compiler.
   */
  result = *cond;

  if G_UNLIKELY (result == NULL)
    {
      result = malloc (sizeof (GThreadXpCONDITION_VARIABLE));

      if (result == NULL)
        g_thread_abort (errno, "malloc");

      result->first = NULL;
      result->last_ptr = &result->first;

      if (InterlockedCompareExchangePointer (cond, result, NULL) != NULL)
        {
          free (result);
          result = *cond;
        }
    }

  return result;
}

static BOOL
g_thread_xp_SleepConditionVariableSRW (gpointer cond,
                                       gpointer mutex,
                                       DWORD    timeout,
                                       ULONG    flags)
{
  GThreadXpCONDITION_VARIABLE *cv = g_thread_xp_get_condition_variable (cond);
  GThreadXpWaiter *waiter = g_thread_xp_waiter_get ();
  DWORD status;

  waiter->next = NULL;

  EnterCriticalSection (&g_thread_xp_lock);
  *cv->last_ptr = waiter;
  cv->last_ptr = &waiter->next;
  LeaveCriticalSection (&g_thread_xp_lock);

  g_mutex_unlock (mutex);
  status = WaitForSingleObject (waiter->event, timeout);

  if (status != WAIT_TIMEOUT && status != WAIT_OBJECT_0)
    g_thread_abort (GetLastError (), "WaitForSingleObject");

  g_mutex_lock (mutex);

  return status == WAIT_OBJECT_0;
}

static void
g_thread_xp_WakeConditionVariable (gpointer cond)
{
  GThreadXpCONDITION_VARIABLE *cv = g_thread_xp_get_condition_variable (cond);
  volatile GThreadXpWaiter *waiter;

  EnterCriticalSection (&g_thread_xp_lock);
  waiter = cv->first;
  if (waiter != NULL)
    {
      cv->first = waiter->next;
      if (cv->first == NULL)
        cv->last_ptr = &cv->first;
    }
  LeaveCriticalSection (&g_thread_xp_lock);

  if (waiter != NULL)
    SetEvent (waiter->event);
}

static void
g_thread_xp_WakeAllConditionVariable (gpointer cond)
{
  GThreadXpCONDITION_VARIABLE *cv = g_thread_xp_get_condition_variable (cond);
  volatile GThreadXpWaiter *waiter;

  EnterCriticalSection (&g_thread_xp_lock);
  waiter = cv->first;
  cv->first = NULL;
  cv->last_ptr = &cv->first;
  LeaveCriticalSection (&g_thread_xp_lock);

  while (waiter != NULL)
    {
      volatile GThreadXpWaiter *next;

      next = waiter->next;
      SetEvent (waiter->event);
      waiter = next;
    }
}

/* {{{2 XP Setup */
static void
g_thread_xp_init (void)
{
  static const GThreadImplVtable g_thread_xp_impl_vtable = {
    g_thread_xp_CallThisOnThreadExit,
    g_thread_xp_InitializeSRWLock,
    g_thread_xp_DeleteSRWLock,
    g_thread_xp_AcquireSRWLockExclusive,
    g_thread_xp_TryAcquireSRWLockExclusive,
    g_thread_xp_ReleaseSRWLockExclusive,
    g_thread_xp_InitializeConditionVariable,
    g_thread_xp_DeleteConditionVariable,
    g_thread_xp_SleepConditionVariableSRW,
    g_thread_xp_WakeAllConditionVariable,
    g_thread_xp_WakeConditionVariable
  };

  InitializeCriticalSection (&g_thread_xp_lock);
  g_thread_xp_waiter_tls = TlsAlloc ();

  g_thread_impl_vtable = g_thread_xp_impl_vtable;
}

/* {{{1 Epilogue */

GThreadFunctions g_thread_functions_for_glib_use =
{
  g_mutex_new,           /* mutex */
  g_mutex_lock,
  g_mutex_trylock,
  g_mutex_unlock,
  g_mutex_free,
  g_cond_new,            /* condition */
  g_cond_signal,
  g_cond_broadcast,
  g_cond_wait,
  g_cond_timed_wait,
  g_cond_free,
  g_private_new_win32_impl,         /* private thread data */
  g_private_get_win32_impl,
  g_private_set_win32_impl,
  g_thread_create_win32_impl,       /* thread */
  g_thread_yield_win32_impl,
  g_thread_join_win32_impl,
  g_thread_exit_win32_impl,
  g_thread_set_priority_win32_impl,
  g_thread_self_win32_impl,
  NULL                             /* no equal function necessary */
};

void
_g_thread_impl_init (void)
{
  static gboolean beenhere = FALSE;

  if (beenhere)
    return;

  beenhere = TRUE;

  printf ("thread init\n");
  win32_check_for_error (TLS_OUT_OF_INDEXES !=
			 (g_thread_self_tls = TlsAlloc ()));
  win32_check_for_error (TLS_OUT_OF_INDEXES !=
			 (g_private_tls = TlsAlloc ()));
  InitializeCriticalSection (&g_thread_global_spinlock);
}

static gboolean
g_thread_lookup_native_funcs (void)
{
  GThreadImplVtable native_vtable = { 0, };
  HMODULE kernel32;

  kernel32 = GetModuleHandle ("KERNEL32.DLL");

  if (kernel32 == NULL)
    return FALSE;

#define GET_FUNC(name) if ((native_vtable.name = (void *) GetProcAddress (kernel32, #name)) == NULL) return FALSE
  GET_FUNC(InitializeSRWLock);
  GET_FUNC(AcquireSRWLockExclusive);
  GET_FUNC(TryAcquireSRWLockExclusive);
  GET_FUNC(ReleaseSRWLockExclusive);

  GET_FUNC(InitializeConditionVariable);
  GET_FUNC(SleepConditionVariableSRW);
  GET_FUNC(WakeAllConditionVariable);
  GET_FUNC(WakeConditionVariable);
#undef GET_FUNC

  g_thread_impl_vtable = native_vtable;

  return TRUE;
}

G_GNUC_INTERNAL void
g_thread_DllMain (void)
{
  /* XXX This is broken right now for some unknown reason...

  if (g_thread_lookup_native_funcs ())
    fprintf (stderr, "(debug) GThread using native mode\n");
  else
*/
    {
      fprintf (stderr, "(debug) GThread using Windows XP mode\n");
      g_thread_xp_init ();
    }
}

/* vim:set foldmethod=marker: */

