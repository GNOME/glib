# Macro to add for using GNU gettext.
# Ulrich Drepper <drepper@cygnus.com>, 1995.
#
# Modified to never use included libintl. 
# Owen Taylor <otaylor@redhat.com>, 12/15/1998
#
#
# This file can be copied and used freely without restrictions.  It can
# be used in projects which are not available under the GNU Public License
# but which still want to provide support for the GNU gettext functionality.
# Please note that the actual code is *not* freely available.
#
#
# If you make changes to this file, you MUST update the copy in
# acinclude.m4. [ aclocal dies on duplicate macros, so if
# we run 'aclocal -I macros/' then we'll run into problems
# once we've installed glib-gettext.m4 :-( ]
#

# serial 5

AC_DEFUN(AM_GLIB_WITH_NLS,
  dnl NLS is obligatory
  [USE_NLS=yes
    AC_SUBST(USE_NLS)

    dnl Figure out what method
    nls_cv_force_use_gnu_gettext="no"

    nls_cv_use_gnu_gettext="$nls_cv_force_use_gnu_gettext"
    if test "$nls_cv_force_use_gnu_gettext" != "yes"; then
      dnl User does not insist on using GNU NLS library.  Figure out what
      dnl to use.  If gettext or catgets are available (in this order) we
      dnl use this.  Else we have to fall back to GNU NLS library.
      dnl catgets is only used if permitted by option --with-catgets.
      nls_cv_header_intl=
      nls_cv_header_libgt=
      CATOBJEXT=NONE

      AC_CHECK_HEADER(libintl.h,
        [AC_CACHE_CHECK([for dgettext in libc], gt_cv_func_dgettext_libc,
	  [AC_TRY_LINK([#include <libintl.h>], [return (int) dgettext ("","")],
	    gt_cv_func_dgettext_libc=yes, gt_cv_func_dgettext_libc=no)])

	  if test "$gt_cv_func_dgettext_libc" != "yes"; then
	    AC_CHECK_LIB(intl, bindtextdomain,
	      [AC_CACHE_CHECK([for dgettext in libintl],
	        gt_cv_func_dgettext_libintl,
	        [AC_CHECK_LIB(intl, dgettext,
		  gt_cv_func_dgettext_libintl=yes,
		  gt_cv_func_dgettext_libintl=no)],
	        gt_cv_func_dgettext_libintl=no)])
	  fi

          if test "$gt_cv_func_dgettext_libintl" = "yes"; then
	    LIBS="$LIBS -lintl";
          fi

	  if test "$gt_cv_func_dgettext_libc" = "yes" \
	    || test "$gt_cv_func_dgettext_libintl" = "yes"; then
	    AC_DEFINE(HAVE_GETTEXT,1,
              [Define if the GNU gettext() function is already present or preinstalled.])
	    AM_PATH_PROG_WITH_TEST(MSGFMT, msgfmt,
 	      [test -z "`$ac_dir/$ac_word -h 2>&1 | grep 'dv '`"], no)dnl
	    if test "$MSGFMT" != "no"; then
	      AC_CHECK_FUNCS(dcgettext)
	      AC_PATH_PROG(GMSGFMT, gmsgfmt, $MSGFMT)
	      AM_PATH_PROG_WITH_TEST(XGETTEXT, xgettext,
	        [test -z "`$ac_dir/$ac_word -h 2>&1 | grep '(HELP)'`"], :)
	      AC_TRY_LINK(, [extern int _nl_msg_cat_cntr;
		 	     return _nl_msg_cat_cntr],
	        [CATOBJEXT=.gmo
	         DATADIRNAME=share],
	        [CATOBJEXT=.mo
	         DATADIRNAME=lib])
	      INSTOBJEXT=.mo
	    fi
	  fi

	  # Added by Martin Baulig 12/15/98 for libc5 systems
	  if test "$gt_cv_func_dgettext_libc" != "yes" \
	    && test "$gt_cv_func_dgettext_libintl" = "yes"; then
	    INTLLIBS=-lintl
	    LIBS=`echo $LIBS | sed -e 's/-lintl//'`
	  fi
      ])

      if test "$CATOBJEXT" = "NONE"; then
        dnl Neither gettext nor catgets in included in the C library.
        dnl Fall back on GNU gettext library.
        nls_cv_use_gnu_gettext=yes
      fi
    fi

    if test "$nls_cv_use_gnu_gettext" != "yes"; then
      AC_DEFINE(ENABLE_NLS, 1,
        [always defined to indicate that i18n is enabled])
    else
      dnl Unset this variable since we use the non-zero value as a flag.
      CATOBJEXT=
    fi

    dnl Test whether we really found GNU xgettext.
    if test "$XGETTEXT" != ":"; then
      dnl If it is no GNU xgettext we define it as : so that the
      dnl Makefiles still can work.
      if $XGETTEXT --omit-header /dev/null 2> /dev/null; then
        : ;
      else
        AC_MSG_RESULT(
	  [found xgettext program is not GNU xgettext; ignore it])
        XGETTEXT=":"
      fi
    fi

    # We need to process the po/ directory.
    POSUB=po

    AC_OUTPUT_COMMANDS(
      [case "$CONFIG_FILES" in *po/Makefile.in*)
        sed -e "/POTFILES =/r po/POTFILES" po/Makefile.in > po/Makefile
      esac])

    dnl These rules are solely for the distribution goal.  While doing this
    dnl we only have to keep exactly one list of the available catalogs
    dnl in configure.in.
    for lang in $ALL_LINGUAS; do
      GMOFILES="$GMOFILES $lang.gmo"
      POFILES="$POFILES $lang.po"
    done

    dnl Make all variables we use known to autoconf.
    AC_SUBST(CATALOGS)
    AC_SUBST(CATOBJEXT)
    AC_SUBST(DATADIRNAME)
    AC_SUBST(GMOFILES)
    AC_SUBST(INSTOBJEXT)
    AC_SUBST(INTLDEPS)
    AC_SUBST(INTLLIBS)
    AC_SUBST(INTLOBJS)
    AC_SUBST(POFILES)
    AC_SUBST(POSUB)
  ])

AC_DEFUN(AM_GLIB_GNU_GETTEXT,
  [AC_REQUIRE([AC_PROG_MAKE_SET])dnl
   AC_REQUIRE([AC_PROG_CC])dnl
   AC_REQUIRE([AC_PROG_RANLIB])dnl
   AC_REQUIRE([AC_HEADER_STDC])dnl
   AC_REQUIRE([AC_C_CONST])dnl
   AC_REQUIRE([AC_C_INLINE])dnl
   AC_REQUIRE([AC_TYPE_OFF_T])dnl
   AC_REQUIRE([AC_TYPE_SIZE_T])dnl
   AC_REQUIRE([AC_FUNC_ALLOCA])dnl
   AC_REQUIRE([AC_FUNC_MMAP])dnl

   AC_CHECK_HEADERS([argz.h limits.h locale.h nl_types.h malloc.h string.h \
unistd.h sys/param.h])
   AC_CHECK_FUNCS([getcwd munmap putenv setenv setlocale strchr strcasecmp \
strdup __argz_count __argz_stringify __argz_next])

   AM_LC_MESSAGES
   AM_GLIB_WITH_NLS

   if test "x$CATOBJEXT" != "x"; then
     if test "x$ALL_LINGUAS" = "x"; then
       LINGUAS=
     else
       AC_MSG_CHECKING(for catalogs to be installed)
       NEW_LINGUAS=
       for lang in ${LINGUAS=$ALL_LINGUAS}; do
         case "$ALL_LINGUAS" in
          *$lang*) NEW_LINGUAS="$NEW_LINGUAS $lang" ;;
         esac
       done
       LINGUAS=$NEW_LINGUAS
       AC_MSG_RESULT($LINGUAS)
     fi

     dnl Construct list of names of catalog files to be constructed.
     if test -n "$LINGUAS"; then
       for lang in $LINGUAS; do CATALOGS="$CATALOGS $lang$CATOBJEXT"; done
     fi
   fi

   dnl Determine which catalog format we have (if any is needed)
   dnl For now we know about two different formats:
   dnl   Linux libc-5 and the normal X/Open format
   test -d po || mkdir po
   if test "$CATOBJEXT" = ".cat"; then
     AC_CHECK_HEADER(linux/version.h, msgformat=linux, msgformat=xopen)

     dnl Transform the SED scripts while copying because some dumb SEDs
     dnl cannot handle comments.
     sed -e '/^#/d' $srcdir/po/$msgformat-msg.sed > po/po2msg.sed
   fi

   dnl If the AC_CONFIG_AUX_DIR macro for autoconf is used we possibly
   dnl find the mkinstalldirs script in another subdir but ($top_srcdir).
   dnl Try to locate is.
   MKINSTALLDIRS=
   if test -n "$ac_aux_dir"; then
     MKINSTALLDIRS="$ac_aux_dir/mkinstalldirs"
   fi
   if test -z "$MKINSTALLDIRS"; then
     MKINSTALLDIRS="\$(top_srcdir)/mkinstalldirs"
   fi
   AC_SUBST(MKINSTALLDIRS)

   dnl Generate list of files to be processed by xgettext which will
   dnl be included in po/Makefile.
   test -d po || mkdir po
   if test "x$srcdir" != "x."; then
     if test "x`echo $srcdir | sed 's@/.*@@'`" = "x"; then
       posrcprefix="$srcdir/"
     else
       posrcprefix="../$srcdir/"
     fi
   else
     posrcprefix="../"
   fi
   rm -f po/POTFILES
   sed -e "/^#/d" -e "/^\$/d" -e "s,.*,	$posrcprefix& \\\\," -e "\$s/\(.*\) \\\\/\1/" \
	< $srcdir/po/POTFILES.in > po/POTFILES
  ])

