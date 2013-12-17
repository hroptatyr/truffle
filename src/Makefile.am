### Makefile.am
include $(top_builddir)/version.mk

AM_CPPFLAGS = -D_GNU_SOURCE -D_POSIX_C_SOURCE=201001L -D_XOPEN_SOURCE=700
AM_CPPFLAGS += -D__STDC_WANT_DEC_FP__
AM_LDFLAGS =

bin_PROGRAMS =
noinst_PROGRAMS =
noinst_HEADERS =
BUILT_SOURCES =
CLEANFILES =
EXTRA_DIST = $(BUILT_SOURCES)

noinst_LIBRARIES = libtruffle.a
libtruffle_a_SOURCES = truffle.h
libtruffle_a_SOURCES += truf-dfp754.c truf-dfp754.h
libtruffle_a_SOURCES += dt-strpf.c dt-strpf.h
libtruffle_a_SOURCES += str.c str.h
libtruffle_a_SOURCES += mmy.c mmy.h
libtruffle_a_SOURCES += sym.c sym.h
libtruffle_a_SOURCES += trod.c trod.h
libtruffle_a_SOURCES += wheap.c wheap.h
libtruffle_a_SOURCES += step.c step.h
libtruffle_a_SOURCES += rpaf.c rpaf.h
libtruffle_a_SOURCES += instant.c instant.h
libtruffle_a_SOURCES += yd.h
libtruffle_a_SOURCES += idate.c idate.h
libtruffle_a_SOURCES += daisy.c daisy.h
libtruffle_a_SOURCES += dt-strpf.c dt-strpf.h
libtruffle_a_SOURCES += nifty.h
libtruffle_a_SOURCES += coru.h
## auxiliary giblets from the old truffle
libtruffle_a_SOURCES += schema.c schema.h
libtruffle_a_SOURCES += cut.c cut.h
nodist_libtruffle_a_SOURCES = version.c
noinst_HEADERS += version.h
CLEANFILES += version.c
EXTRA_DIST += version.c.in

if USE_ASM_CORUS
noinst_LIBRARIES += libcoru.a
libcoru_a_SOURCES =
EXTRA_libcoru_a_SOURCES =
libcoru_a_SOURCES += coru.h
libcoru_a_SOURCES += coru/cocore.c coru/cocore.h
libcoru_a_SOURCES += coru/switch.c coru/switch.h
libcoru_a_SOURCES += coru/platform.h
libcoru_a_SOURCES += coru/valgrind.h
libcoru_a_SOURCES += coru/memcheck.h
EXTRA_libcoru_a_SOURCES += coru/switch-arm.c
EXTRA_libcoru_a_SOURCES += coru/switch-ppc_osx.c
EXTRA_libcoru_a_SOURCES += coru/switch-x86.c
EXTRA_libcoru_a_SOURCES += coru/switch-x86_64.c
endif  USE_ASM_CORUS


## truffle umbrella command kit
bin_PROGRAMS += truffle
truffle_SOURCES = truffle.c truffle.yuck
truffle_LDADD = libtruffle.a
if USE_ASM_CORUS
truffle_LDADD += libcoru.a
endif  USE_ASM_CORUS
truffle_LDADD += -lm
BUILT_SOURCES += truffle.yucc


SUFFIXES = .yuck .yucc

version.c: version.c.in $(top_builddir)/version.mk
	sed -e 's,[@]VERSION[@],$(VERSION),g' <$(srcdir)/$@.in >$@

.yuck.yucc:
	$(AM_V_GEN) PATH=$(top_builddir)/build-aux:"$${PATH}" \
		yuck$(EXEEXT) gen --version=$(VERSION) $< > $@ || \
		{ rm -f -- $@; false; }

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
