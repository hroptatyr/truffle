# Help the Developers and yourself. Just use the C locale and settings
# for the compilation. They can still be overriden by make LANG=<whatever>
# but that is general a not very good idea
LANG=C
LC_ALL=C

EXTRA_DIST = $(TESTS)
EXTRA_DIST += $(TRUFTEST_LOG_COMPILER)
TESTS =

TESTS += toy1.truftest
EXTRA_DIST += toy1.schema toy1.series toy1.result toy1.-f.result

TESTS += toy2.truftest
EXTRA_DIST += toy2.schema toy2.series toy2.result toy2.-f.result

TESTS += toy3.truftest
EXTRA_DIST += toy3.schema toy3.series toy3.result toy3.-f.result

EXTRA_DIST += toy5.schema toy5.series toy4.--sparse.result toy4.--sparse.-f.result

TESTS += toy5.truftest
EXTRA_DIST += toy5.schema toy5.series toy5.result toy5.-f.result

TEST_EXTENSIONS = .truftest
TRUFTEST_LOG_COMPILER = $(srcdir)/check-truffle.sh
AM_TRUFTEST_LOG_FLAGS =
LOG_COMPILER = echo

