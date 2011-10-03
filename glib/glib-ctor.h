#ifndef __GLIB_CTOR_H__

#if defined (__GNUC__) && !defined (_WIN32)
#define GLIB_CTOR(func) \
  __attribute__((constructor)) static void func (void)
#define GLIB_ENSURE_CTOR(func) G_STMT_START { } G_STMT_END
#else
/* could be vastly improved... */
#define GLIB_CTOR(func) \
  static GMutex   g__##func##_mutex;                  \
  static gboolean g__##func##_initialised;            \
  static void func (void)
#define GLIB_ENSURE_CTOR(func) \
  G_STMT_START {                                      \
    g_mutex_lock (&g__##func##_mutex);                \
    if (!g__##func##_initialised)                     \
      {                                               \
        g__##func##_initialised = TRUE;               \
        func ();                                      \
      }                                               \
    g_mutex_unlock (&g__##func##_mutex);              \
  } G_STMT_END
#endif

#endif /* __GLIB_CTOR_H__ */
