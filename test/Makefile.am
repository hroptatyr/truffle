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

LOG_COMPILER = echo

TEST_EXTENSIONS += .clit
TESTS_ENVIRONMENT = root=$(top_srcdir)
CLIT_LOG_COMPILER = $(builddir)/clitoris
AM_CLIT_LOG_FLAGS = -v --builddir $(top_builddir)/src

TESTS += old_01.clit
TESTS += old_02.clit
EXTRA_DIST += toy1.schema toy1.series

TESTS += old_03.clit
TESTS += old_04.clit
EXTRA_DIST += toy2.schema toy2.series

TESTS += old_05.clit
TESTS += old_06.clit
EXTRA_DIST += toy3.schema toy3.series

TESTS += old_07.clit
TESTS += old_08.clit
EXTRA_DIST += toy4.schema toy4.series

TESTS += old_09.clit
TESTS += old_10.clit
EXTRA_DIST += toy5.schema toy5.series

TESTS += old_11.clit
TESTS += old_12.clit
EXTRA_DIST += toy6.schema
EXTRA_DIST += toy7.schema

TESTS += old_13.clit
TESTS += old_14.clit
EXTRA_DIST += toy8.schema

TESTS += schema_to_trod.1.deflt.clit
TESTS += schema_to_trod.1.abs.clit
TESTS += schema_to_trod.1.oco.clit

TESTS += schema_to_trod.2.deflt.clit
TESTS += schema_to_trod.2.abs.clit
TESTS += schema_to_trod.2.oco.clit

TESTS += schema_to_trod.3.deflt.clit
TESTS += schema_to_trod.3.abs.clit
TESTS += schema_to_trod.3.oco.clit

TESTS += schema_to_trod.4.deflt.clit
EXTRA_DIST += toy4.trod

TESTS += schema_to_trod.5.deflt.clit
EXTRA_DIST += toy5.trod

TESTS += schema_to_trod.6.deflt.clit
TESTS += schema_to_trod.6.abs.clit

TESTS += schema_to_trod.7.deflt.clit
TESTS += schema_to_trod.7.abs.clit

TESTS += trod_to_trod.5.deflt.clit
TESTS += trod_to_trod.5.abs.clit
TESTS += trod_to_trod.5.oco.clit
EXTRA_DIST += toy5.trod

TESTS += toy1.1.mmy.clit
TESTS += toy1.2.mmy.clit
EXTRA_DIST += toy1.mmy.series

TESTS += trod.1.clit
TESTS += trod.1.f.clit
EXTRA_DIST += toy1.trod

TESTS += trod.2.clit
TESTS += trod.2.f.clit
EXTRA_DIST += toy2.trod

TESTS += trod.2.abs.clit
TESTS += trod.2.abs.f.clit
EXTRA_DIST += toy2.abs.trod

TESTS += trod.2.oco.clit
TESTS += trod.2.oco.f.clit
EXTRA_DIST += toy2.oco.trod

TESTS += truffle_schema_contracts.1.deflt.clit
TESTS += truffle_schema_contracts.1.abs.clit
TESTS += truffle_schema_contracts.1.oco.clit

TESTS += truffle_trod_contracts.1.deflt.clit
TESTS += truffle_trod_contracts.1.abs.clit
TESTS += truffle_trod_contracts.1.oco.clit

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

TESTS += position_01.clit
TESTS += position_02.clit
TESTS += position_03.clit
TESTS += position_04.clit
TESTS += position_05.clit
TESTS += position_06.clit
EXTRA_DIST += print_05.trod

TESTS += filter_01.clit
TESTS += filter_02.clit
EXTRA_DIST += filt_01.tser
EXTRA_DIST += filt_01.trod


## our friendly helpers
check_PROGRAMS += clitoris
clitoris_CPPFLAGS = $(AM_CPPFLAGS)
clitoris_CPPFLAGS += -D_GNU_SOURCE
clitoris_LDFLAGS = $(AM_LDFLAGS) -static
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
