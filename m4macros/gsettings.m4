dnl GLIB_GSETTINGS
dnl Defines GSETTINGS_SCHEMAS_INSTALL which controls whether
dnl the schema should be compiled
dnl

AC_DEFUN([GLIB_GSETTINGS],
[
  AC_ARG_ENABLE(schemas-install,
	AC_HELP_STRING([--disable-schemas-install],
		       [Disable installation of GSettings schemas]),
     [case ${enableval} in
       yes|no) ;;
       *) AC_MSG_ERROR([bad value ${enableval} for --enable-schemas-install]) ;;
      esac])
  AM_CONDITIONAL([GSETTINGS_SCHEMAS_INSTALL], [test "$enable_schemas_install" != no])

  PKG_PROG_PKG_CONFIG([0.16])

  AC_SUBST(gsettingsschemadir, [${datadir}/glib-2.0/schemas])
  AC_SUBST(GLIB_COMPILE_SCHEMAS, `$PKG_CONFIG --variable glib_compile_schemas gio-2.0`)
  if test "x$GLIB_COMPILE_SCHEMAS" = "x"; then
    AC_MSG_ERROR([glib-compile-schemas not found.])
  fi

  GSETTINGS_CHECK_RULE='
.PHONY : check-gsettings-schema

gschema_xml_files := $(wildcard *.gschema.xml)
check-gsettings-schema: gsettings_schema_validate_stamp
MOSTLYCLEANFILES += gsettings_schema_validate_stamp
gsettings_schema_validate_stamp: $(gschema_xml_files)
	$(GLIB_COMPILE_SCHEMAS) --dry-run --schema-files $?
	touch [$]@

all: check-gsettings-schema
'

  _GSETTINGS_SUBST(GSETTINGS_CHECK_RULE)
])

dnl _GSETTINGS_SUBST(VARIABLE)
dnl Abstract macro to do either _AM_SUBST_NOTMAKE or AC_SUBST
AC_DEFUN([_GSETTINGS_SUBST],
[
AC_SUBST([$1])
m4_ifdef([_AM_SUBST_NOTMAKE], [_AM_SUBST_NOTMAKE([$1])])
]
)
