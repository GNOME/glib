dnl @synopsis AC_FUNC_VSNPRINTF_C99
dnl
dnl Check whether there is a vsnprintf() function with C99 semantics installed.
dnl
AC_DEFUN([AC_FUNC_VSNPRINTF_C99],
[AC_CACHE_CHECK(for C99 vsnprintf,
  ac_cv_func_vsnprintf_c99,
[AC_TRY_RUN(
[#include <stdio.h>
#include <stdarg.h>

int
doit(char * s, ...)
{
  char buffer[32];
  va_list args;
  int r;

  va_start(args, s);
  r = vsnprintf(buffer, 5, s, args);
  va_end(args);

  if (r != 7)
    exit(1);

  exit(0);
}

int
main(void)
{
  doit("1234567");
  exit(1);
}], ac_cv_func_vsnprintf_c99=yes, ac_cv_func_vsnprintf_c99=no, ac_cv_func_vsnprintf_c99=no)])
dnl Note that the default is to be pessimistic in the case of cross compilation.
dnl If you know that the target has a C99 vsnprintf(), you can get around this
dnl by setting ac_func_vsnprintf_c99 to yes, as described in the Autoconf manual.
if test $ac_cv_func_vsnprintf_c99 = yes; then
  AC_DEFINE(HAVE_C99_VSNPRINTF, 1,
            [Define if you have a version of the vsnprintf function
             with semantics as specified by the ISO C99 standard.])
fi
])# AC_FUNC_VSNPRINTF_C99


dnl @synopsis AC_FUNC_PRINTF_UNIX98
dnl
dnl Check whether the printf() family supports Unix98 %n$ positional parameters 
dnl
AC_DEFUN([AC_FUNC_PRINTF_UNIX98],
[AC_CACHE_CHECK(whether printf supports positional parameters,
  ac_cv_func_printf_unix98,
[AC_TRY_RUN(
[#include <stdio.h>

int
main (void)
{
  char buffer[128];

  sprintf (buffer, "%2\$d %3\$d %1\$d", 1, 2, 3);
  if (strcmp ("2 3 1", buffer) == 0)
    exit (0);
  exit (1);
}], ac_cv_func_printf_unix98=yes, ac_cv_func_printf_unix98=no, ac_cv_func_printf_unix98=no)])
dnl Note that the default is to be pessimistic in the case of cross compilation.
dnl If you know that the target printf() supports positional parameters, you can get around 
dnl this by setting ac_func_printf_unix98 to yes, as described in the Autoconf manual.
if test $ac_cv_func_printf_unix98 = yes; then
  AC_DEFINE(HAVE_UNIX98_PRINTF, 1,
            [Define if your printf function family supports positional parameters
             as specified by Unix98.])
fi
])# AC_FUNC_PRINTF_UNIX98




