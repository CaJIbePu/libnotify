SUBDIRS = libnotify docs tools tests

ACLOCAL_AMFLAGS = -I m4

pcdata_DATA = libnotify.pc
pcdatadir = $(libdir)/pkgconfig

EXTRA_DIST = \
	AUTHORS NEWS INSTALL COPYING README \
	ChangeLog \
	autogen.sh \
	configure.ac \
	libnotify.pc.in \
	libnotify-uninstalled.pc.in \
	m4/introspection.m4

$(OBJECTS): libtool
libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status --recheck

doxygen:
	@echo "Running doxygen..."
	@doxygen Doxyfile

DISTCHECK_CONFIGURE_FLAGS = --enable-gtk-doc
