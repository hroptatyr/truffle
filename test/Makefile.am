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
TESTS += position_07.clit
TESTS += position_08.clit
EXTRA_DIST += print_05.trod

TESTS += filter_01.clit
TESTS += filter_02.clit
TESTS += filter_03.clit
TESTS += filter_04.clit
TESTS += filter_05.clit
TESTS += filter_06.clit
TESTS += filter_07.clit
TESTS += filter_08.clit
EXTRA_DIST += filt_01.tser
EXTRA_DIST += filt_01.trod

TESTS += glue_01.clit
TESTS += glue_02.clit
TESTS += glue_03.clit
TESTS += glue_04.clit
TESTS += glue_05.clit
TESTS += glue_06.clit
TESTS += glue_07.clit
TESTS += glue_08.clit
TESTS += glue_09.clit
TESTS += glue_10.clit
EXTRA_DIST += glue_01.tser
EXTRA_DIST += glue_01.trod
EXTRA_DIST += glue_02.tser
EXTRA_DIST += glue_02.trod

TESTS += filter_09.clit
TESTS += filter_10.clit
TESTS += filter_11.clit
TESTS += filter_12.clit
TESTS += glue_11.clit
TESTS += glue_12.clit
TESTS += glue_13.clit
TESTS += glue_14.clit
EXTRA_DIST += filt_02.tser
EXTRA_DIST += filt_02.trod
EXTRA_DIST += glue_03.tser
EXTRA_DIST += glue_02.trod

TESTS += roll_01.clit
TESTS += roll_02.clit
TESTS += roll_03.clit
TESTS += roll_04.clit
TESTS += roll_05.clit
TESTS += roll_06.clit
TESTS += roll_07.clit
TESTS += roll_08.clit

TESTS += roll_09.clit
TESTS += roll_10.clit
EXTRA_DIST += roll_01.tser
EXTRA_DIST += roll_01.trod
TESTS += roll_11.clit
TESTS += roll_12.clit
EXTRA_DIST += roll_02.tser
EXTRA_DIST += roll_02.trod

TESTS += roll_13.clit
TESTS += roll_14.clit
TESTS += roll_15.clit
TESTS += roll_16.clit
TESTS += roll_17.clit
TESTS += roll_18.clit
TESTS += roll_19.clit
TESTS += roll_20.clit

TESTS += string_symbols_01.clit
TESTS += string_symbols_02.clit
TESTS += string_symbols_03.clit
TESTS += string_symbols_04.clit
EXTRA_DIST += strsym_01.tser
EXTRA_DIST += strsym_01.trod


## our friendly helpers
check_PROGRAMS += clitoris
clitoris_CPPFLAGS = $(AM_CPPFLAGS)
clitoris_CPPFLAGS += -D_GNU_SOURCE
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
