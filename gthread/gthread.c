#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>

static const char *g_log_domain_gthread = "GThread";
static gboolean thread_system_already_initialized = FALSE;

#include G_THREAD_SOURCE

gboolean
g_thread_try_init(GThreadFunctions* init)
{
  if (thread_system_already_initialized)
    return FALSE;
    
  thread_system_already_initialized = TRUE;

  if (init == NULL)
    {
      g_thread_use_default_impl = TRUE;
      init = &g_thread_functions_for_glib_use_default;
    }

  g_thread_functions_for_glib_use = *init;

  g_thread_supported = 
    init->mutex_new &&  
    init->mutex_lock && 
    init->mutex_try_lock && 
    init->mutex_unlock && 
    init->mutex_free && 
    init->cond_new && 
    init->cond_signal && 
    init->cond_broadcast && 
    init->cond_wait && 
    init->cond_timed_wait &&
    init->cond_free &&
    init->private_new &&
    init->private_get &&
    init->private_get;

  /* if somebody is calling g_thread_init(), it means that he at least wants
     to have mutex support, so check this */

  if (!g_thread_supported)
    g_error( "Mutex functions missing." );

  return TRUE;
}

void
g_thread_init(GThreadFunctions* init)
{  
  /* Make sure, this function is only called once. */
  if (!g_thread_try_init (init))
    g_error( "the glib thread system may only be initialized once." );
}

