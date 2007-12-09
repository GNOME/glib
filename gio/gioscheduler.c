/* GIO - GLib Input, Output and Streaming Library
 * 
 * Copyright (C) 2006-2007 Red Hat, Inc.
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
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 */

#include <config.h>

#include "gioscheduler.h"

#include "gioalias.h"

/**
 * SECTION:gioscheduler
 * @short_description: I/O Scheduler
 * 
 * Schedules asynchronous I/O operations. #GIOScheduler integrates into the main 
 * event loop (#GMainLoop) and may use threads if they are available.
 * 
 * <para id="io-priority"><indexterm><primary>I/O priority</primary></indexterm>
 * Each I/O operation has a priority, and the scheduler uses the priorities
 * to determine the order in which operations are executed. They are 
 * <emphasis>not</emphasis> used to determine system-wide I/O scheduling.
 * Priorities are integers, with lower numbers indicating higher priority. 
 * It is recommended to choose priorities between %G_PRIORITY_LOW and 
 * %G_PRIORITY_HIGH, with %G_PRIORITY_DEFAULT as a default.
 * </para>
 **/

struct _GIOJob {
  GSList *active_link;
  GIOJobFunc job_func;
  GIODataFunc cancel_func; /* Runs under job map lock */
  gpointer data;
  GDestroyNotify destroy_notify;

  gint io_priority;
  GCancellable *cancellable;

  guint idle_tag;
};

G_LOCK_DEFINE_STATIC(active_jobs);
static GSList *active_jobs = NULL;

static GThreadPool *job_thread_pool = NULL;

static void io_job_thread (gpointer data,
			   gpointer user_data);

static void
g_io_job_free (GIOJob *job)
{
  if (job->cancellable)
    g_object_unref (job->cancellable);
  g_free (job);
}

static gint
g_io_job_compare (gconstpointer a,
		  gconstpointer b,
		  gpointer      user_data)
{
  const GIOJob *aa = a;
  const GIOJob *bb = b;

  /* Cancelled jobs are set prio == -1, so that
     they are executed as quickly as possible */
  
  /* Lower value => higher priority */
  if (aa->io_priority < bb->io_priority)
    return -1;
  if (aa->io_priority == bb->io_priority)
    return 0;
  return 1;
}

static gpointer
init_scheduler (gpointer arg)
{
  if (job_thread_pool == NULL)
    {
      /* TODO: thread_pool_new can fail */
      job_thread_pool = g_thread_pool_new (io_job_thread,
					   NULL,
					   10,
					   FALSE,
					   NULL);
      if (job_thread_pool != NULL)
	{
	  g_thread_pool_set_sort_function (job_thread_pool,
					   g_io_job_compare,
					   NULL);
	  /* Its kinda weird that this is a global setting
	   * instead of per threadpool. However, we really
	   * want to cache some threads, but not keep around
	   * those threads forever. */
	  g_thread_pool_set_max_idle_time (15 * 1000);
	  g_thread_pool_set_max_unused_threads (2);
	}
    }
  return NULL;
}

static void
remove_active_job (GIOJob *job)
{
  GIOJob *other_job;
  GSList *l;
  gboolean resort_jobs;
  
  G_LOCK (active_jobs);
  active_jobs = g_slist_delete_link (active_jobs, job->active_link);
  
  resort_jobs = FALSE;
  for (l = active_jobs; l != NULL; l = l->next)
    {
      other_job = l->data;
      if (other_job->io_priority >= 0 &&
	  g_cancellable_is_cancelled (other_job->cancellable))
	{
	  other_job->io_priority = -1;
	  resort_jobs = TRUE;
	}
    }
  G_UNLOCK (active_jobs);
  
  if (resort_jobs &&
      job_thread_pool != NULL)
    g_thread_pool_set_sort_function (job_thread_pool,
				     g_io_job_compare,
				     NULL);

}

static void
io_job_thread (gpointer data,
	       gpointer user_data)
{
  GIOJob *job = data;

  if (job->cancellable)
    g_push_current_cancellable (job->cancellable);
  job->job_func (job, job->cancellable, job->data);
  if (job->cancellable)
    g_pop_current_cancellable (job->cancellable);

  if (job->destroy_notify)
    job->destroy_notify (job->data);

  remove_active_job (job);
  g_io_job_free (job);

}

static gboolean
run_job_at_idle (gpointer data)
{
  GIOJob *job = data;

  if (job->cancellable)
    g_push_current_cancellable (job->cancellable);
  
  job->job_func (job, job->cancellable, job->data);
  
  if (job->cancellable)
    g_pop_current_cancellable (job->cancellable);

  if (job->destroy_notify)
    job->destroy_notify (job->data);

  remove_active_job (job);
  g_io_job_free (job);

  return FALSE;
}

/**
 * g_schedule_io_job:
 * @job_func: a #GIOJobFunc.
 * @user_data: a #gpointer.
 * @notify: a #GDestroyNotify.
 * @io_priority: the <link linkend="gioscheduler">I/O priority</link> 
 * of the request.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 *
 * Schedules the I/O Job.
 * 
 **/
void
g_schedule_io_job (GIOJobFunc      job_func,
		   gpointer        user_data,
		   GDestroyNotify  notify,
		   gint            io_priority,
		   GCancellable   *cancellable)
{
  static GOnce once_init = G_ONCE_INIT;
  GIOJob *job;

  g_return_if_fail (job_func != NULL);

  job = g_new0 (GIOJob, 1);
  job->job_func = job_func;
  job->data = user_data;
  job->destroy_notify = notify;
  job->io_priority = io_priority;
    
  if (cancellable)
    job->cancellable = g_object_ref (cancellable);

  G_LOCK (active_jobs);
  active_jobs = g_slist_prepend (active_jobs, job);
  job->active_link = active_jobs;
  G_UNLOCK (active_jobs);

  if (g_thread_supported())
    {
      g_once (&once_init, init_scheduler, NULL);
      g_thread_pool_push (job_thread_pool, job, NULL);
    }
  else
    {
      /* Threads not available, instead do the i/o sync inside a
       * low prio idle handler
       */
      job->idle_tag = g_idle_add_full (G_PRIORITY_DEFAULT_IDLE + 1 + io_priority / 10,
				       run_job_at_idle,
				       job, NULL);
    }
}

/**
 * g_cancel_all_io_jobs:
 * 
 * Cancels all cancellable I/O Jobs. 
 **/
void
g_cancel_all_io_jobs (void)
{
  GSList *cancellable_list, *l;
  
  G_LOCK (active_jobs);
  cancellable_list = NULL;
  for (l = active_jobs; l != NULL; l = l->next)
    {
      GIOJob *job = l->data;
      if (job->cancellable)
	cancellable_list = g_slist_prepend (cancellable_list,
					    g_object_ref (job->cancellable));
    }
  G_UNLOCK (active_jobs);

  for (l = cancellable_list; l != NULL; l = l->next)
    {
      GCancellable *c = l->data;
      g_cancellable_cancel (c);
      g_object_unref (c);
    }
  g_slist_free (cancellable_list);
}

typedef struct {
  GIODataFunc func;
  gpointer    data;
  GDestroyNotify notify;

  GMutex *ack_lock;
  GCond *ack_condition;
} MainLoopProxy;

static gboolean
mainloop_proxy_func (gpointer data)
{
  MainLoopProxy *proxy = data;

  proxy->func (proxy->data);

  if (proxy->ack_lock)
    {
      g_mutex_lock (proxy->ack_lock);
      g_cond_signal (proxy->ack_condition);
      g_mutex_unlock (proxy->ack_lock);
    }
  
  return FALSE;
}

static void
mainloop_proxy_free (MainLoopProxy *proxy)
{
  if (proxy->ack_lock)
    {
      g_mutex_free (proxy->ack_lock);
      g_cond_free (proxy->ack_condition);
    }
  
  g_free (proxy);
}

static void
mainloop_proxy_notify (gpointer data)
{
  MainLoopProxy *proxy = data;

  if (proxy->notify)
    proxy->notify (proxy->data);

  /* If nonblocking we free here, otherwise we free in io thread */
  if (proxy->ack_lock == NULL)
    mainloop_proxy_free (proxy);
}

/**
 * g_io_job_send_to_mainloop:
 * @job: a #GIOJob.
 * @func: a #GIODataFunc.
 * @user_data: a #gpointer.
 * @notify: a #GDestroyNotify.
 * @block: boolean flag indicating whether or not this job should block.
 * 
 * Sends an I/O job to the application's main loop for processing.
 **/
void
g_io_job_send_to_mainloop (GIOJob         *job,
			   GIODataFunc     func,
			   gpointer        user_data,
			   GDestroyNotify  notify,
			   gboolean        block)
{
  GSource *source;
  MainLoopProxy *proxy;
  guint id;

  g_return_if_fail (job != NULL);
  g_return_if_fail (func != NULL);

  if (job->idle_tag)
    {
      /* We just immediately re-enter in the case of idles (non-threads)
       * Anything else would just deadlock. If you can't handle this, enable threads.
       */
      func (user_data); 
      return;
    }
  
  proxy = g_new0 (MainLoopProxy, 1);
  proxy->func = func;
  proxy->data = user_data;
  proxy->notify = notify;
  
  if (block)
    {
      proxy->ack_lock = g_mutex_new ();
      proxy->ack_condition = g_cond_new ();
    }
  
  source = g_idle_source_new ();
  g_source_set_priority (source, G_PRIORITY_DEFAULT);

  g_source_set_callback (source, mainloop_proxy_func, proxy, mainloop_proxy_notify);

  if (block)
    g_mutex_lock (proxy->ack_lock);
		  
  id = g_source_attach (source, NULL);
  g_source_unref (source);

  if (block)
    {
      g_cond_wait (proxy->ack_condition, proxy->ack_lock);
      g_mutex_unlock (proxy->ack_lock);
      
      /* destroy notify didn't free proxy */
      mainloop_proxy_free (proxy);
    }
}

#define __G_IO_SCHEDULER_C__
#include "gioaliasdef.c"
