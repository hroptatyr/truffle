# Help the Developers and yourself. Just use the C locale and settings
# for the compilation. They can still be overriden by make LANG=<whatever>
# but that is general a not very good idea
LANG=C
LC_ALL=C

TESTS = toy1.truftest toy2.truftest toy3.truftest toy4.truftest toy5.truftest
TEST_EXTENSIONS = .truftest
TRUFTEST_LOG_COMPILER = $(srcdir)/check-truffle.sh
AM_TRUFTEST_LOG_FLAGS =
LOG_COMPILER = echo

