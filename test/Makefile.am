include $(top_builddir)/version.mk

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
TEST_EXTENSIONS =
BUILT_SOURCES =

TEST_EXTENSIONS += .truftest
TRUFTEST_LOG_COMPILER = $(builddir)/truf-test
AM_TRUFTEST_LOG_FLAGS = --builddir $(top_builddir)/src --hash sha1sum
LOG_COMPILER = echo

TEST_EXTENSIONS += .clit
TESTS_ENVIRONMENT = root=$(top_srcdir)
CLIT_LOG_COMPILER = $(builddir)/clitoris
AM_CLIT_LOG_FLAGS = -v --builddir $(top_builddir)/src

check_PROGRAMS += truf-test
truf_test_LDADD = ../src/libtruffle.a
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

TESTS += toy8.1.truftest
TESTS += toy8.2.truftest
EXTRA_DIST += toy8.schema

TESTS += schema_to_trod.1.deflt.truftest
TESTS += schema_to_trod.1.abs.truftest
TESTS += schema_to_trod.1.oco.truftest

TESTS += schema_to_trod.2.deflt.truftest
TESTS += schema_to_trod.2.abs.truftest
TESTS += schema_to_trod.2.oco.truftest

TESTS += schema_to_trod.3.deflt.truftest
TESTS += schema_to_trod.3.abs.truftest
TESTS += schema_to_trod.3.oco.truftest

TESTS += schema_to_trod.4.deflt.truftest
EXTRA_DIST += toy4.trod

TESTS += schema_to_trod.5.deflt.truftest
EXTRA_DIST += toy5.trod

TESTS += schema_to_trod.6.deflt.truftest
TESTS += schema_to_trod.6.abs.truftest

TESTS += schema_to_trod.7.deflt.truftest
TESTS += schema_to_trod.7.abs.truftest

TESTS += trod_to_trod.5.deflt.truftest
TESTS += trod_to_trod.5.abs.truftest
TESTS += trod_to_trod.5.oco.truftest
EXTRA_DIST += toy5.trod

TESTS += toy1.1.mmy.truftest
TESTS += toy1.2.mmy.truftest
EXTRA_DIST += toy1.mmy.series

TESTS += trod.1.truftest
TESTS += trod.1.f.truftest
EXTRA_DIST += toy1.trod

TESTS += trod.2.truftest
TESTS += trod.2.f.truftest
EXTRA_DIST += toy2.trod

TESTS += trod.2.abs.truftest
TESTS += trod.2.abs.f.truftest
EXTRA_DIST += toy2.abs.trod

TESTS += trod.2.oco.truftest
TESTS += trod.2.oco.f.truftest
EXTRA_DIST += toy2.oco.trod

TESTS += truffle_schema_contracts.1.deflt.truftest
TESTS += truffle_schema_contracts.1.abs.truftest
TESTS += truffle_schema_contracts.1.oco.truftest

TESTS += truffle_trod_contracts.1.deflt.truftest
TESTS += truffle_trod_contracts.1.abs.truftest
TESTS += truffle_trod_contracts.1.oco.truftest

TESTS += print_01.clit
TESTS += print_02.clit
TESTS += print_03.clit
TESTS += print_04.clit
TESTS += print_05.clit
TESTS += print_06.clit
TESTS += print_07.clit
TESTS += print_08.clit
TESTS += print_09.clit
TESTS += print_10.clit
EXTRA_DIST += print_01.trod
EXTRA_DIST += print_02.trod
EXTRA_DIST += print_03.trod
EXTRA_DIST += print_04.trod

## our friendly helpers
check_PROGRAMS += clitoris
clitoris_CPPFLAGS = $(AM_CPPFLAGS)
clitoris_CPPFLAGS += -D_GNU_SOURCE
clitoris_LDFLAGS = $(AM_LDFLAGS) -static
clitoris_LDADD = -lutil
BUILT_SOURCES += clitoris.x clitoris.xh


## ggo rule
%.x %.xh: %.ggo
	$(AM_V_GEN) gengetopt -F $* -c x -H xh -i $<
	@sed -e 's,_PARSER_VERSION VERSION,_PARSER_VERSION "$(VERSION)",g' \
	     $*.xh > $*.xh-t \
	&& mv $*.xh-t $*.xh

clean-local:
	-rm -rf *.tmpd

## Makefile.am ends here
