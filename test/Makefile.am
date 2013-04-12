# Help the Developers and yourself. Just use the C locale and settings
# for the compilation. They can still be overriden by make LANG=<whatever>
# but that is general a not very good idea
LANG=C
LC_ALL=C

AM_CPPFLAGS = -D_GNU_SOURCE -D_XOPEN_SOURCE=600 -D_POSIX_C_SOURCE=201001L
AM_CPPFLAGS += -I$(abs_top_srcdir)/src
AM_LDFLAGS = -L$(abs_top_builddir)/src

EXTRA_DIST = $(TESTS)
EXTRA_DIST += $(BUILT_SOURCES)
check_PROGRAMS =
TESTS =
BUILT_SOURCES =

TEST_EXTENSIONS = .truftest
TRUFTEST_LOG_COMPILER = $(builddir)/truf-test
AM_TRUFTEST_LOG_FLAGS = --builddir $(top_builddir)/src --hash sha1sum
LOG_COMPILER = echo

check_PROGRAMS += truf-test
BUILT_SOURCES += truf-test-clo.c truf-test-clo.h
EXTRA_DIST += truf-test.sh

TESTS += toy1.1.truftest
TESTS += toy1.2.truftest
EXTRA_DIST += toy1.schema toy1.series

TESTS += toy2.1.truftest
TESTS += toy2.2.truftest
EXTRA_DIST += toy2.schema toy2.series

TESTS += toy3.1.truftest
TESTS += toy3.2.truftest
EXTRA_DIST += toy3.schema toy3.series

TESTS += toy4.1.truftest
TESTS += toy4.2.truftest
EXTRA_DIST += toy4.schema toy4.series

TESTS += toy5.1.truftest
TESTS += toy5.2.truftest
EXTRA_DIST += toy5.schema toy5.series

TESTS += toy6.1.truftest
EXTRA_DIST += toy6.schema

TESTS += toy7.1.truftest
EXTRA_DIST += toy7.schema

TESTS += schema_to_trod.1.deflt.truftest
TESTS += schema_to_trod.1.abs.truftest
TESTS += schema_to_trod.1.oco.truftest

TESTS += schema_to_trod.2.deflt.truftest
TESTS += schema_to_trod.2.abs.truftest
TESTS += schema_to_trod.2.oco.truftest


## ggo rule
%.c %.h: %.ggo
	$(AM_V_GEN) gengetopt -F $* -i $<

clean-local:
	-rm -rf *.tmpd

## Makefile.am ends here
