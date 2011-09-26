# Help the Developers and yourself. Just use the C locale and settings
# for the compilation. They can still be overriden by make LANG=<whatever>
# but that is general a not very good idea
LANG=C
LC_ALL=C

EXTRA_DIST = $(TESTS)
EXTRA_DIST += $(TRUFTEST_LOG_COMPILER)
TESTS =

TEST_EXTENSIONS = .truftest
TRUFTEST_LOG_COMPILER = $(srcdir)/truf-test.sh
AM_TRUFTEST_LOG_FLAGS = --builddir $(top_builddir)/src --hash sha1sum
LOG_COMPILER = echo

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
