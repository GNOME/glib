/*
 * Copyright © 2020 Endless Mobile, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Philip Withnall <withnall@endlessm.com>
 */

/*
 * SECTION:trace
 * @Title: Performance tracing
 * @Short_description: Functions for measuring and tracing performance
 *
 * The performance tracing functions allow for the performance of code using
 * GLib to be measured by passing metrics from the current process to an
 * external measurement process such as `sysprof-cli` or `sysprofd`.
 *
 * They are designed to execute quickly, especially in the common case where no
 * measurement process is connected. They are guaranteed to not block the caller
 * and are guaranteed to have zero runtime cost if tracing support is disabled
 * at configure time.
 *
 * Tracing information can be provided as ‘marks’ with a start time and
 * duration; or as marks with a start time and no duration. Marks with a
 * duration are intended to show the execution time of a piece of code. Marks
 * with no duration are intended to show an instantaneous performance problem,
 * such as an unexpectedly large allocation, or that a slow path has been taken
 * in some code.
 *
 * |[<!-- language="C" -->
 * gint64 begin_time_nsec G_GNUC_UNUSED;
 *
 * begin_time_nsec = G_TRACE_CURRENT_TIME;
 *
 * // some code which might take a while
 *
 * g_trace_mark (begin_time_nsec, G_TRACE_CURRENT_TIME - begin_time_nsec,
 *               "GLib", "GSource.dispatch",
 *               "%s ⇒ %s", g_source_get_name (source), need_destroy ? "destroy" : "keep");
 * ]|
 *
 * The tracing API is currently internal to GLib.
 *
 * Since: 2.66
 */

#include "config.h"

#include "gtrace-private.h"

#ifdef __linux__
#include <sched.h>
#endif

#include <sys/types.h>
#include <unistd.h>


#ifdef HAVE_SYSPROF
G_LOCK_DEFINE_STATIC (writer_lock);
static gpointer dummy_writer;
#endif  /* HAVE_SYSPROF */

/* FIXME: Eventually it would likely give better performance to use
 * #SysprofCollector for this, with one memory-mapped ring buffer per thread.
 * However, #SysprofCollector currently depends on GIO, and rewriting it to drop
 * that dependency is too much work for an initial version of this support in
 * GLib.
 *
 * So instead this version uses #SysprofCaptureWriter, which writes captured
 * frames into an FD (typically backed by a memfd or socket). This is not thread
 * safe, and the FD cannot be written to by multiple threads, so one
 * #SysprofCaptureWriter instance has to be shared between all threads. This
 * also means that the application (nor any other loaded libraries) can create
 * a #SysprofCaptureWriter without overwriting frames from GLib (since each
 * newly created #SysprofCaptureWriter sends a fresh file header down the FD,
 * which resets the capture). So our use of #SysprofCaptureWriter is not
 * suitable in the long run.
 *
 * However, the GLib-internal API is designed so that it could be ported to
 * #SysprofCollector in future.
 */
#ifdef HAVE_SYSPROF
static SysprofCaptureWriter *
g_trace_collector_get (void)
{
  static SysprofCaptureWriter *singleton_writer;

  if (g_once_init_enter (&singleton_writer))
    {
      SysprofCaptureWriter *writer;

      writer = sysprof_capture_writer_new_from_env (0);
      g_unsetenv ("SYSPROF_TRACE_FD");

      /* NULL means ‘not initialised’, so we need a different dummy value for
       * ‘initialisation failed’, and would rather avoid a second atomic variable */
      if (writer == NULL)
        writer = (SysprofCaptureWriter *) &dummy_writer;

      g_once_init_leave (&singleton_writer, g_steal_pointer (&writer));
    }

  return singleton_writer;
}
#endif  /* HAVE_SYSPROF */

/*
 * g_trace_mark:
 * @begin_time_nsec: start time of the mark, as returned by %G_TRACE_CURRENT_TIME
 * @duration_nsec: duration of the mark, in nanoseconds
 * @group: name of the group for categorising this mark
 * @name: name of the mark
 * @message_format: format for the detailed message for the mark, in `printf()` format
 * @...: arguments to substitute into @message_format; none of these should have
 *    side effects
 *
 * Add a mark to the trace, starting at @begin_time_nsec and having length
 * @duration_nsec (which may be zero). The @group should typically be `GLib`,
 * and the @name should concisely describe the call site.
 *
 * All of the arguments to this function must not have side effects, as the
 * entire function call may be dropped if sysprof support is not available.
 *
 * Since: 2.66
 */
void
(g_trace_mark) (gint64       begin_time_nsec,
                gint64       duration_nsec,
                const gchar *group,
                const gchar *name,
                const gchar *message_format,
                ...)
{
#ifdef HAVE_SYSPROF
  SysprofCaptureWriter *writer = g_trace_collector_get ();
  gint cpu;
  pid_t pid;
  gchar message[512];  /* arbitrary; file an issue if you want this to be bigger */
  va_list args;

  /* Is tracing disabled? */
  if (G_LIKELY (writer == (SysprofCaptureWriter *) &dummy_writer))
    return;

#ifdef __linux__
  cpu = sched_getcpu ();
#else
  cpu = -1;
#endif
  pid = getpid ();

  /* Ignore truncation. This is only a developer tool. */
  va_start (args, message_format);
  g_vsnprintf (message, sizeof (message), message_format, args);
  va_end (args);

  G_LOCK (writer_lock);
  sysprof_capture_writer_add_mark (writer, begin_time_nsec, cpu, pid,
                                   duration_nsec, group, name, message);
  G_UNLOCK (writer_lock);
#endif  /* HAVE_SYSPROF */
}
