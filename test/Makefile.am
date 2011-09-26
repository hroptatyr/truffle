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

