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

# Checks the location of the XML Catalog
# Usage:
#   JH_PATH_XML_CATALOG
# Defines XMLCATALOG and XML_CATALOG_FILE substitutions
AC_DEFUN([JH_PATH_XML_CATALOG],
[
  # check for the presence of the XML catalog
  AC_ARG_WITH([xml-catalog],
              AC_HELP_STRING([--with-xml-catalog=CATALOG],
                             [path to xml catalog to use]),,
              [with_xml_catalog=/etc/xml/catalog])
  XML_CATALOG_FILE="$with_xml_catalog"
  AC_MSG_CHECKING([for XML catalog ($XML_CATALOG_FILE)])
  if test -f "$XML_CATALOG_FILE"; then
    AC_MSG_RESULT([found])
  else
    AC_MSG_RESULT([not found])
    AC_MSG_ERROR([XML catalog not found])
  fi
  AC_SUBST([XML_CATALOG_FILE])

  # check for the xmlcatalog program
  AC_PATH_PROG(XMLCATALOG, xmlcatalog, no)
  if test "x$XMLCATALOG" = xno; then
    AC_MSG_ERROR([could not find xmlcatalog program])
  fi
])

# Checks if a particular URI appears in the XML catalog
# Usage:
#   JH_CHECK_XML_CATALOG(URI, [FRIENDLY-NAME], [ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND])
AC_DEFUN([JH_CHECK_XML_CATALOG],
[
  AC_REQUIRE([JH_PATH_XML_CATALOG])dnl
  AC_MSG_CHECKING([for ifelse([$2],,[$1],[$2]) in XML catalog])
  if AC_RUN_LOG([$XMLCATALOG --noout "$XML_CATALOG_FILE" "$1" >&2]); then
    AC_MSG_RESULT([found])
    ifelse([$3],,,[$3
])dnl
  else
    AC_MSG_RESULT([not found])
    ifelse([$4],,
       [AC_MSG_ERROR([could not find ifelse([$2],,[$1],[$2]) in XML catalog])],
       [$4])
  fi
])



