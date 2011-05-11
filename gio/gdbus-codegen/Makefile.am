
NULL =
bin_SCRIPTS =
CLEANFILES =
EXTRA_DIST =

codegendir = $(libdir)/gdbus-codegen
codegen_PYTHON =						\
	__init__.py						\
	codegen.py						\
	codegen_main.py						\
	codegen_docbook.py					\
	config.py						\
	dbustypes.py						\
	parser.py						\
	utils.py						\
	$(NULL)

bin_SCRIPTS += gdbus-codegen
CLEANFILES += gdbus-codegen
EXTRA_DIST += gdbus-codegen.in

gdbus-codegen: gdbus-codegen.in Makefile
	$(AM_V_GEN) sed -e 's,@libdir\@,$(libdir),' -e 's,@PYTHON\@,$(PYTHON),' $< > $@.tmp && mv $@.tmp $@
	@chmod a+x $@

clean-local:
	rm -f *~
