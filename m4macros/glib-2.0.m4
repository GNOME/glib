# Configure paths for GLIB
# Owen Taylor     1997-2001

# GLIB_CONFIG([MODULES, [MINIMUM-VERSION, [MAXIMUM-VERSION]]])

# Test for GLib, erroring out if it is not found. If gmodule, gobject,
# gthread, or gio is specified in MODULES, it will be passed to
# pkg-config. If MINIMUM-VERSION is specified, then it will require
# at least that version of GLib, and it will define
# GLIB_VERSION_MIN_REQUIRED so as to generate deprecation warnings
# for older functions. If MAXIMUM-VERSION is specified, it will
# define GLIB_VERSION_MAX_ALLOWED (but it does not actually error
# out if the found GLib version is larger than that).
#
# Defines GLIB_CFLAGS, GLIB_LIBS, GLIB_MKENUMS, GLIB_GENMARSHAL,
# GOBJECT_QUERY, GLIB_COMPILE_RESOURCES, and GLIB_COMPILE_SCHEMAS.
#
# Adds --disable-glibtest and --disable-schemas-compile configure
# flags.
AC_DEFUN([GLIB_CONFIG],[
  _GLIB_CONFIG_INTERNAL($2,,[
      if test "$GLIB_LIBS" = ""; then
          if test -n "$2"; then
              AC_MSG_ERROR(GLib $2 or later is required)
          else
              AC_MSG_ERROR(GLib is required)
          fi
      fi
  ],$1)

  _GLIB_CONFIG_SCHEMAS

  if test -n "$2"; then
      ver=`echo $2 | sed -e 's/\./_/' -e 's/\..*//'`
      GLIB_CFLAGS="$GLIB_CFLAGS -DGLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_$ver"
  fi
  if test -n "$3"; then
      ver=`echo $3 | sed -e 's/\./_/' -e 's/\..*//'`
      GLIB_CFLAGS="$GLIB_CFLAGS -DGLIB_VERSION_MAX_ALLOWED=GLIB_VERSION_$ver"
  fi
])

dnl AM_PATH_GLIB_2_0([MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND [, MODULES]]]])
AC_DEFUN([AM_PATH_GLIB_2_0], [
m4_warn([obsolete],[AM_PATH_GLIB_2_0 is deprecated; use GLIB_CONFIG])
_GLIB_CONFIG_INTERNAL($1,$2,$3,$4)
])

dnl _GLIB_CONFIG_INTERNAL: same args as AM_PATH_GLIB_2_0
AC_DEFUN([_GLIB_CONFIG_INTERNAL], [
  AC_ARG_ENABLE(glibtest, AS_HELP_STRING([--disable-glibtest],[do not try to compile and run a test GLIB program]),,enable_glibtest=yes)

  pkg_config_args=glib-2.0
  for module in . $4
  do
      case "$module" in
         gmodule) 
             pkg_config_args="$pkg_config_args gmodule-2.0"
         ;;
         gmodule-no-export) 
             pkg_config_args="$pkg_config_args gmodule-no-export-2.0"
         ;;
         gobject) 
             pkg_config_args="$pkg_config_args gobject-2.0"
         ;;
         gthread) 
             pkg_config_args="$pkg_config_args gthread-2.0"
         ;;
         gio*) 
             pkg_config_args="$pkg_config_args $module-2.0"
         ;;
      esac
  done

  PKG_PROG_PKG_CONFIG([0.16])

  no_glib=""

  if test "x$PKG_CONFIG" = x ; then
    no_glib=yes
    PKG_CONFIG=no
  fi

  min_glib_version=ifelse([$1], ,2.0.0,$1)
  AC_MSG_CHECKING(for GLib - version >= $min_glib_version)

  if test x$PKG_CONFIG != xno ; then
    ## don't try to run the test against uninstalled libtool libs
    if $PKG_CONFIG --uninstalled $pkg_config_args; then
	  echo "Will use uninstalled version of GLib found in PKG_CONFIG_PATH"
	  enable_glibtest=no
    fi

    if $PKG_CONFIG --atleast-version $min_glib_version $pkg_config_args; then
	  :
    else
	  no_glib=yes
    fi
  fi

  if test x"$no_glib" = x ; then
    if test "$cross_compiling" != yes; then
      GLIB_GENMARSHAL=`$PKG_CONFIG --variable=glib_genmarshal glib-2.0`
      GOBJECT_QUERY=`$PKG_CONFIG --variable=gobject_query glib-2.0`
      GLIB_MKENUMS=`$PKG_CONFIG --variable=glib_mkenums glib-2.0`
      GLIB_COMPILE_RESOURCES=`$PKG_CONFIG --variable=glib_compile_resources gio-2.0`
    else
      AC_PATH_PROG(GLIB_GENMARSHAL, glib-genmarshal)
      AC_PATH_PROG(GOBJECT_QUERY, gobject-query)
      AC_PATH_PROG(GLIB_MKENUMS, glib-mkenums)
      AC_PATH_PROG(GLIB_COMPILE_RESOURCES, glib-compile-resources)
    fi

    GLIB_CFLAGS=`$PKG_CONFIG --cflags $pkg_config_args`
    GLIB_LIBS=`$PKG_CONFIG --libs $pkg_config_args`
    glib_config_major_version=`$PKG_CONFIG --modversion glib-2.0 | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\1/'`
    glib_config_minor_version=`$PKG_CONFIG --modversion glib-2.0 | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\2/'`
    glib_config_micro_version=`$PKG_CONFIG --modversion glib-2.0 | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\3/'`
    if test "x$enable_glibtest" = "xyes" ; then
      ac_save_CFLAGS="$CFLAGS"
      ac_save_LIBS="$LIBS"
      CFLAGS="$CFLAGS $GLIB_CFLAGS"
      LIBS="$GLIB_LIBS $LIBS"
dnl
dnl Now check if the installed GLIB is sufficiently new. (Also sanity
dnl checks the results of pkg-config to some extent)
dnl
      rm -f conf.glibtest
      AC_TRY_RUN([
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>

int 
main ()
{
  unsigned int major, minor, micro;

  fclose (fopen ("conf.glibtest", "w"));

  if (sscanf("$min_glib_version", "%u.%u.%u", &major, &minor, &micro) != 3) {
     printf("%s, bad version string\n", "$min_glib_version");
     exit(1);
   }

  if ((glib_major_version != $glib_config_major_version) ||
      (glib_minor_version != $glib_config_minor_version) ||
      (glib_micro_version != $glib_config_micro_version))
    {
      printf("\n*** 'pkg-config --modversion glib-2.0' returned %d.%d.%d, but GLIB (%d.%d.%d)\n", 
             $glib_config_major_version, $glib_config_minor_version, $glib_config_micro_version,
             glib_major_version, glib_minor_version, glib_micro_version);
      printf ("*** was found! If pkg-config was correct, then it is best\n");
      printf ("*** to remove the old version of GLib. You may also be able to fix the error\n");
      printf("*** by modifying your LD_LIBRARY_PATH enviroment variable, or by editing\n");
      printf("*** /etc/ld.so.conf. Make sure you have run ldconfig if that is\n");
      printf("*** required on your system.\n");
      printf("*** If pkg-config was wrong, set the environment variable PKG_CONFIG_PATH\n");
      printf("*** to point to the correct configuration files\n");
    } 
  else if ((glib_major_version != GLIB_MAJOR_VERSION) ||
	   (glib_minor_version != GLIB_MINOR_VERSION) ||
           (glib_micro_version != GLIB_MICRO_VERSION))
    {
      printf("*** GLIB header files (version %d.%d.%d) do not match\n",
	     GLIB_MAJOR_VERSION, GLIB_MINOR_VERSION, GLIB_MICRO_VERSION);
      printf("*** library (version %d.%d.%d)\n",
	     glib_major_version, glib_minor_version, glib_micro_version);
    }
  else
    {
      if ((glib_major_version > major) ||
        ((glib_major_version == major) && (glib_minor_version > minor)) ||
        ((glib_major_version == major) && (glib_minor_version == minor) && (glib_micro_version >= micro)))
      {
        return 0;
       }
     else
      {
        printf("\n*** An old version of GLIB (%u.%u.%u) was found.\n",
               glib_major_version, glib_minor_version, glib_micro_version);
        printf("*** You need a version of GLIB newer than %u.%u.%u. The latest version of\n",
	       major, minor, micro);
        printf("*** GLIB is always available from ftp://ftp.gtk.org.\n");
        printf("***\n");
        printf("*** If you have already installed a sufficiently new version, this error\n");
        printf("*** probably means that the wrong copy of the pkg-config shell script is\n");
        printf("*** being found. The easiest way to fix this is to remove the old version\n");
        printf("*** of GLIB, but you can also set the PKG_CONFIG environment to point to the\n");
        printf("*** correct copy of pkg-config. (In this case, you will have to\n");
        printf("*** modify your LD_LIBRARY_PATH enviroment variable, or edit /etc/ld.so.conf\n");
        printf("*** so that the correct libraries are found at run-time))\n");
      }
    }
  return 1;
}
],, no_glib=yes,[echo $ac_n "cross compiling; assumed OK... $ac_c"])
       CFLAGS="$ac_save_CFLAGS"
       LIBS="$ac_save_LIBS"
     fi
  fi
  if test "x$no_glib" = x ; then
     AC_MSG_RESULT(yes (version $glib_config_major_version.$glib_config_minor_version.$glib_config_micro_version))
     ifelse([$2], , :, [$2])     
  else
     AC_MSG_RESULT(no)
     if test "$PKG_CONFIG" = "no" ; then
       echo "*** A new enough version of pkg-config was not found."
       echo "*** See http://www.freedesktop.org/software/pkgconfig/"
     else
       if test -f conf.glibtest ; then
        :
       else
          echo "*** Could not run GLIB test program, checking why..."
          ac_save_CFLAGS="$CFLAGS"
          ac_save_LIBS="$LIBS"
          CFLAGS="$CFLAGS $GLIB_CFLAGS"
          LIBS="$LIBS $GLIB_LIBS"
          AC_TRY_LINK([
#include <glib.h>
#include <stdio.h>
],      [ return ((glib_major_version) || (glib_minor_version) || (glib_micro_version)); ],
        [ echo "*** The test program compiled, but did not run. This usually means"
          echo "*** that the run-time linker is not finding GLIB or finding the wrong"
          echo "*** version of GLIB. If it is not finding GLIB, you'll need to set your"
          echo "*** LD_LIBRARY_PATH environment variable, or edit /etc/ld.so.conf to point"
          echo "*** to the installed location  Also, make sure you have run ldconfig if that"
          echo "*** is required on your system"
	  echo "***"
          echo "*** If you have an old version installed, it is best to remove it, although"
          echo "*** you may also be able to get things to work by modifying LD_LIBRARY_PATH" ],
        [ echo "*** The test program failed to compile or link. See the file config.log for the"
          echo "*** exact error that occured. This usually means GLIB is incorrectly installed."])
          CFLAGS="$ac_save_CFLAGS"
          LIBS="$ac_save_LIBS"
       fi
     fi
     GLIB_CFLAGS=""
     GLIB_LIBS=""
     GLIB_GENMARSHAL=""
     GOBJECT_QUERY=""
     GLIB_MKENUMS=""
     GLIB_COMPILE_RESOURCES=""
     ifelse([$3], , :, [$3])
  fi
  AC_SUBST(GLIB_CFLAGS)
  AC_SUBST(GLIB_LIBS)
  AC_SUBST(GLIB_GENMARSHAL)
  AC_SUBST(GOBJECT_QUERY)
  AC_SUBST(GLIB_MKENUMS)
  AC_SUBST(GLIB_COMPILE_RESOURCES)
  rm -f conf.glibtest
])

AC_DEFUN([_GLIB_CONFIG_SCHEMAS],
[
  AC_ARG_ENABLE(schemas-compile,
                AS_HELP_STRING([--disable-schemas-compile],
                               [Disable regeneration of gschemas.compiled on install]),
                [case ${enableval} in
                  yes) GSETTINGS_DISABLE_SCHEMAS_COMPILE=""  ;;
                  no)  GSETTINGS_DISABLE_SCHEMAS_COMPILE="1" ;;
                  *) AC_MSG_ERROR([bad value ${enableval} for --enable-schemas-compile]) ;;
                 esac])
  AC_SUBST([GSETTINGS_DISABLE_SCHEMAS_COMPILE])
  AC_SUBST(gsettingsschemadir, [${datadir}/glib-2.0/schemas])
  if test "$cross_compiling" != yes; then
    GLIB_COMPILE_SCHEMAS=`$PKG_CONFIG --variable glib_compile_schemas gio-2.0`
  else
    AC_PATH_PROG(GLIB_COMPILE_SCHEMAS, glib-compile-schemas)
  fi
  AC_SUBST(GLIB_COMPILE_SCHEMAS)
])
