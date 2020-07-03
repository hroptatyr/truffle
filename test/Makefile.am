include $(top_builddir)/version.mk

# Help the Developers and yourself. Just use the C locale and settings
# for the compilation. They can still be overriden by make LANG=<whatever>
# but that is general a not very good idea
LANG=C
LC_ALL=C

AM_CPPFLAGS = -D_POSIX_C_SOURCE=200112L -D_XOPEN_SOURCE=600 -D_BSD_SOURCE
AM_CPPFLAGS += -I$(abs_top_srcdir)/src
AM_LDFLAGS = -L$(abs_top_builddir)/src

EXTRA_DIST = $(TESTS)
EXTRA_DIST += $(BUILT_SOURCES)
check_PROGRAMS =
TESTS =
TEST_EXTENSIONS =
BUILT_SOURCES =

LOG_COMPILER = echo

## summon our helper
include clitoris.am
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
TESTS += print_11.clit
TESTS += print_12.clit
TESTS += print_13.clit
TESTS += print_14.clit
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
TESTS += roll_21.clit
TESTS += roll_22.clit
TESTS += roll_23.clit
TESTS += roll_24.clit
EXTRA_DIST += roll_25.tser
EXTRA_DIST += roll_25.trod

TESTS += string_symbols_01.clit
TESTS += string_symbols_02.clit
TESTS += string_symbols_03.clit
TESTS += string_symbols_04.clit
EXTRA_DIST += strsym_01.tser
EXTRA_DIST += strsym_01.trod

TESTS += migrate_01.clit
TESTS += migrate_02.clit
EXTRA_DIST += old_01.truf
EXTRA_DIST += old_02.truf

TESTS += expcon_01.clit
TESTS += expcon_02.clit
TESTS += expcon_03.clit
TESTS += expcon_04.clit
TESTS += expcon_05.clit


clean-local:
	-rm -rf *.tmpd

## Makefile.am ends here
