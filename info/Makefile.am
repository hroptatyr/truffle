include $(top_builddir)/version.mk

# Help the Developers and yourself.  Just use the C locale and settings
# for the compilation. They can still be overriden by make LANG=<whatever>
# but that is general a not very good idea
LANG=C
LC_ALL=C

BUILT_SOURCES =
BUILT_MANS =
EXTRA_DIST = $(BUILT_SOURCES) $(BUILT_MANS)

man1_MANS =
man1_MANS += $(BUILT_MANS)

BUILT_MANS += truffle.man
BUILT_MANS += trod.man

EXTRA_DIST += author.h2m


## help2man helpers
%.man: $(top_srcdir)/src/%-clo.ggo $(top_srcdir)/configure $(top_builddir)/version.mk
	$(AM_V_GEN) $(HELP2MAN) -S "$(PACKAGE_STRING)" -p "($(PACKAGE))$*" \
		--no-info \
		-I "$(srcdir)/$*.name" \
		-I "$(srcdir)/author.h2m" \
		-o $@ $(top_builddir)/src/$*

## Makefile.am ends here
