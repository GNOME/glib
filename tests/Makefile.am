
INCLUDES = -I$(top_srcdir)

EXTRA_DIST = \
	makefile.msc \
	makefile.msc.in

TESTS = \
	array-test	\
	dirname-test	\
	hash-test	\
	list-test	\
	node-test	\
	relation-test	\
	slist-test	\
	string-test	\
	strfunc-test	\
	tree-test	\
	type-test

noinst_PROGRAMS = $(TESTS)

array_test_LDADD = $(top_builddir)/libglib.la
dirname_test_LDADD = $(top_builddir)/libglib.la
hash_test_LDADD = $(top_builddir)/libglib.la
list_test_LDADD = $(top_builddir)/libglib.la
node_test_LDADD = $(top_builddir)/libglib.la
relation_test_LDADD = $(top_builddir)/libglib.la
slist_test_LDADD = $(top_builddir)/libglib.la
string_test_LDADD = $(top_builddir)/libglib.la
strfunc_test_LDADD = $(top_builddir)/libglib.la
tree_test_LDADD = $(top_builddir)/libglib.la
type_test_LDADD = $(top_builddir)/libglib.la

makefile.msc: $(top_builddir)/config.status $(top_srcdir)/tests/makefile.msc.in
	cd $(top_builddir) && CONFIG_FILES=tests/$@ CONFIG_HEADERS= $(SHELL) ./config.status
