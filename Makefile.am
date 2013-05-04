include $(top_builddir)/version.mk

LANG = C
LC_ALL = C

ACLOCAL_AMFLAGS = -I m4

SUBDIRS =
SUBDIRS += src
SUBDIRS += info
SUBDIRS += test

EXTRA_DIST =
DISTCLEANFILES =

EXTRA_DIST += git-version-gen
EXTRA_DIST += m4/.dir

DISTCLEANFILES += version.mk

dist-hook:
	echo $(VERSION) > $(distdir)/version \
	&& touch -r $(top_builddir)/version.mk $(distdir)/version
