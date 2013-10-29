### Makefile.am
include $(top_builddir)/version.mk

AM_CPPFLAGS = -D_GNU_SOURCE -D_POSIX_C_SOURCE=201001L -D__USE_XOPEN2K8
AM_LDFLAGS =

bin_PROGRAMS =
noinst_PROGRAMS =
noinst_HEADERS =
BUILT_SOURCES =
CLEANFILES =
EXTRA_DIST = $(BUILT_SOURCES)

noinst_LIBRARIES = libtruffle.a
libtruffle_a_SOURCES = yd.h
libtruffle_a_SOURCES += dt-strpf.c dt-strpf.h
libtruffle_a_SOURCES += gq.c gq.h
libtruffle_a_SOURCES += gbs.c gbs.h
libtruffle_a_SOURCES += schema.c schema.h
libtruffle_a_SOURCES += series.c series.h
libtruffle_a_SOURCES += trod.c trod.h
libtruffle_a_SOURCES += cut.c cut.h
libtruffle_a_SOURCES += mmy.c mmy.h
libtruffle_a_SOURCES += wheap.c wheap.h
libtruffle_a_SOURCES += instant.c instant.h
libtruffle_a_SOURCES += idate.c idate.h
libtruffle_a_SOURCES += daisy.c daisy.h
libtruffle_a_SOURCES += dt-strpf.c dt-strpf.h
nodist_libtruffle_a_SOURCES = version.c
noinst_HEADERS += version.h
CLEANFILES += version.c
EXTRA_DIST += version.c.in

bin_PROGRAMS += truffle
truffle_SOURCES = truffle.c truffle.h truffle-clo.ggo
truffle_CPPFLAGS = $(AM_CPPFLAGS) -DSTANDALONE
truffle_LDFLAGS = $(AM_LDFLAGS)
truffle_LDADD = libtruffle.a
truffle_LDADD += -lm
BUILT_SOURCES += truffle-clo.c truffle-clo.h

bin_PROGRAMS += trod
trod_SOURCES = trod.c trod-clo.ggo
trod_SOURCES += mmy.h
EXTRA_trod_SOURCES =
EXTRA_trod_SOURCES += daisy.c
trod_CPPFLAGS = $(AM_CPPFLAGS) -DSTANDALONE
trod_LDFLAGS = $(AM_LDFLAGS)
trod_LDADD = libtruffle.a
trod_LDADD += -lm
BUILT_SOURCES += trod-clo.c trod-clo.h

version.c: version.c.in $(top_builddir)/version.mk
	sed -e 's,[@]VERSION[@],$(VERSION),g' <$(srcdir)/$@.in >$@

## gengetopt rule with sed hack to avoid using VERSION and config.h
## depend on Makefile.am to survive a git pull
%.c %.h: %.ggo Makefile.am
	gengetopt -l -i $< -F $* \
	&& sed \
	  -e 's,config.h,version.h,g' \
	  -e 's,\(.*\)_PARSER_VERSION VERSION,\1_PARSER_VERSION $(PACKAGE)_version_string,g' \
	  $*.h >$*.h-t \
	&& mv $*.h-t $*.h

## for dist or dist check we need binaries, so help2man can work
dist-hook: $(bin_PROGRAMS)

# 
# Help the developers get nice post-processed source files

## Create preprocessor output (debugging purposes only)
.c.i:
	$(COMPILE) -E -o $@ $<

## Create assembler output (debugging purposes only)
.c.s:
	$(COMPILE) -S -c $(AM_CFLAGS) $<
