### Makefile.am

AM_CPPFLAGS = -D_GNU_SOURCE -D_POSIX_C_SOURCE=201001L -D__USE_XOPEN2K8
AM_LDFLAGS =

bin_PROGRAMS =
noinst_PROGRAMS =
noinst_HEADERS =
BUILT_SOURCES =
CLEANFILES =
EXTRA_DIST = $(BUILT_SOURCES)

bin_PROGRAMS += truffle
truffle_SOURCES = truffle.c truffle.h truffle-clo.ggo
truffle_SOURCES += yd.h
truffle_SOURCES += schema.c schema.h
truffle_SOURCES += cut.c cut.h
truffle_SOURCES += dt-strpf.c dt-strpf.h
truffle_CPPFLAGS = $(AM_CPPFLAGS) -DSTANDALONE
truffle_LDFLAGS = $(AM_LDFLAGS)
truffle_LDADD = -lm
BUILT_SOURCES += truffle-clo.c truffle-clo.h

noinst_PROGRAMS += trod
trod_SOURCES = trod.c trod-clo.ggo
trod_CPPFLAGS = $(AM_CPPFLAGS) -DSTANDALONE
trod_LDFLAGS = $(AM_LDFLAGS)
trod_LDADD = -lm
BUILT_SOURCES += trod-clo.c trod-clo.h

## ggo rule
%.c %.h: %.ggo
	gengetopt -l -i $< -F $*

# 
# Help the developers get nice post-processed source files

## Create preprocessor output (debugging purposes only)
.c.i:
	$(COMPILE) -E -o $@ $<

## Create assembler output (debugging purposes only)
.c.s:
	$(COMPILE) -S -c $(AM_CFLAGS) $<
