notifyincdir = $(includedir)/libnotify

MARSHAL_PREFIX = notify_marshal
MARSHAL_FILE = notify-marshal
ENUM_FILE = notify-enum-types

lib_LTLIBRARIES = libnotify.la

notify_headers = \
	notify.h \
	notification.h

notifyinc_HEADERS = \
	$(notify_headers) \
	$(ENUM_FILE).h

noinst_HEADERS = \
	notify-marshal.h \
	internal.h

BUILT_SOURCES = \
	$(ENUM_FILE).h \
	$(ENUM_FILE).c \
	notify-marshal.c \
	notify-marshal.h

libnotify_la_SOURCES = \
	notify.c \
	notification.c \
	notify-enum-types.c \
	notify-marshal.c

libnotify_la_LIBADD = \
	$(PACKAGE_LIBS)

libnotify_la_LDFLAGS = \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)

notify-marshal.h: notify-marshal.list
	$(GLIB_GENMARSHAL) --prefix=$(MARSHAL_PREFIX) notify-marshal.list \
		--header > notify-marshal.h

notify-marshal.c: notify-marshal.list
	$(GLIB_GENMARSHAL) --prefix=$(MARSHAL_PREFIX) notify-marshal.list \
		--body > notify-marshal.c

$(ENUM_FILE).h: s-enum-types-h
	@true
s-enum-types-h: $(notify_headers) Makefile
	( cd $(srcdir) && glib-mkenums \
			--fhead "#ifndef _NOTIFY_ENUM_TYPES_H_\n#define _NOTIFY_ENUM_TYPES_H_\n\n#include <glib-object.h>\n\nG_BEGIN_DECLS\n" \
			--fprod "/* enumerations from \"@filename@\" */\n" \
			--vhead "GType @enum_name@_get_type(void);\n#define NOTIFY_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n"	\
			--ftail "G_END_DECLS\n\n#endif /* _NOTIFY_ENUM_TYPES_H_ */" \
		$(notify_headers) ) > tmp-$(ENUM_FILE).h \
	&& (cmp -s tmp-$(ENUM_FILE).h $(ENUM_FILE).h || cp tmp-$(ENUM_FILE).h $(ENUM_FILE).h ) \
	&& rm -f tmp-$(ENUM_FILE).h	\
	&& echo timestamp > $(@F)

$(ENUM_FILE).c: s-enum-types-c
	@true
s-enum-types-c: $(notify_headers) Makefile
	( cd $(srcdir) && glib-mkenums \
			--fhead "#include <libnotify/notify.h>" \
			--fprod "\n/* enumerations from \"@filename@\" */" \
			--vhead "GType\n@enum_name@_get_type (void)\n{\n  static GType etype = 0;\n  if (etype == 0) {\n    static const G@Type@Value values[] = {" 	\
			--vprod "      { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
			--vtail "      { 0, NULL, NULL }\n    };\n    etype = g_@type@_register_static (\"@EnumName@\", values);\n  }\n  return etype;\n}\n" \
		$(notify_headers) ) > tmp-$(ENUM_FILE).c \
	&& (cmp -s tmp-$(ENUM_FILE).c $(ENUM_FILE).c || cp tmp-$(ENUM_FILE).c $(ENUM_FILE).c ) \
	&& rm -f tmp-$(ENUM_FILE).c \
	&& echo timestamp > $(@F)

EXTRA_DIST = \
	notify-marshal.list \
	$(ENUM_FILE).h

CLEANFILES = \
	notify-marshal.c \
	notify-marshal.h \
	$(ENUM_FILE).c \
	$(ENUM_FILE).h \
	s-enum-types-c \
	s-enum-types-h

INCLUDES = \
	-I$(top_srcdir) \
	-DG_LOG_DOMAIN=\"libnotify\" \
	$(PACKAGE_CFLAGS)

if HAVE_INTROSPECTION

BUILT_GIRSOURCES =
Notify-0.4.gir: $(INTROSPECTION_SCANNER) libnotify.la
	$(INTROSPECTION_SCANNER) -v --namespace Notify --nsversion=0.4 \
	    $(INCLUDES) \
	    --include=Gtk-2.0 \
	    --library=notify \
	    --libtool="$(LIBTOOL)" \
	    --pkg gtk+-2.0 \
	    --output $@ \
	    notify.c notification.c
BUILT_GIRSOURCES += Notify-0.4.gir

# INTROSPECTION_GIRDIR/INTROSPECTION_TYPELIBDIR aren't the right place to install
# thing - we need to install inside our prefix.
girdir = $(datadir)/gir-1.0
gir_DATA = $(BUILT_GIRSOURCES)

typelibsdir = $(libdir)/girepository-1.0/
typelibs_DATA = $(BUILT_GIRSOURCES:.gir=.typelib)

%.typelib: %.gir $(INTROSPECTION_COMPILER)
	$(DEBUG) $(INTROSPECTION_COMPILER) --includedir=$(srcdir) --includedir=. $(INTROSPECTION_COMPILER_OPTS) $< -o $(builddir)/$(@F)

CLEANFILES += $(BUILT_GIRSOURCES) $(typelibs_DATA)

endif

