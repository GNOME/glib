## Process this file with automake to produce Makefile.in

info_TEXINFOS = glib.texi

man_MANS = glib-config.1

EXTRA_DIST = glib-config.1	\
	     glib.html 		\
	     glib_toc.html

glib.html:
	texi2html glib.texi

glib_toc.html:
	texi2html glib.texi

files:
	@files=`ls $(DISTFILES) 2> /dev/null `; for p in $$files; do \
	  echo $$p; \
	done
