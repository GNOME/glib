#ifndef __GLIBINTL_H__
#define __GLIBINTL_H__

#include "config.h"

#ifdef ENABLE_NLS

extern int _glib_gettext_initialized;

char *_glib_gettext_init (const char *str);

#include<libintl.h>
#define _(String)                         \
   (_glib_gettext_initialized ?            \
      dgettext(GETTEXT_PACKAGE,String) :  \
      _glib_gettext_init(String))

#ifdef gettext_noop
#define N_(String) gettext_noop(String)
#else
#define N_(String) (String)
#endif
#else /* NLS is disabled */
#define _(String) (String)
#define N_(String) (String)
#define textdomain(String) (String)
#define gettext(String) (String)
#define dgettext(Domain,String) (String)
#define dcgettext(Domain,String,Type) (String)
#define bindtextdomain(Domain,Directory) (Domain) 
#endif

#endif /* __GLIBINTL_H__ */
