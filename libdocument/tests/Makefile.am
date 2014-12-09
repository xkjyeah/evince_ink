bin_PROGRAMS=test_quadtree

test_quadtree_SOURCES =			\
	test_quadtree.c \
	../ev-document.h \
	../ev-mapping-tree.h

test_quadtree_CPPFLAGS = \
	-DEVINCE_COMPILATION

test_quadtree_CFLAGS = \
	$(LIBDOCUMENT_CFLAGS)			\
	$(AM_CFLAGS) \
	-I$(top_srcdir)/libdocument

test_quadtree_LDADD =\
	$(LIBDOCUMENT_LIBS)	\
	$(ZLIB_LIBS)		\
	$(LIBM) \
	../libevdocument3.la
