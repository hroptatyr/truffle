### Makefile.am

AM_CPPFLAGS = -D_GNU_SOURCE -D_POSIX_C_SOURCE=201001L -D__USE_XOPEN2K8
AM_LDFLAGS = -lm

bin_PROGRAMS =
noinst_PROGRAMS =
noinst_HEADERS =
BUILT_SOURCES =
CLEANFILES = $(BUILT_SOURCES)

## ggo rule
%.c: %.ggo
	gengetopt -l -i $< -F $*

# 
# Help the developers get nice post-processed source files

## Create preprocessor output (debugging purposes only)
.c.i:
	$(COMPILE) -E -o $@ $<

## Create assembler output (debugging purposes only)
.c.s:
	$(COMPILE) -S -c $(AM_CFLAGS) $<
