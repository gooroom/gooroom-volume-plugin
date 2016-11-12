AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\" \
	-DDEFAULT_MIXER_COMMAND=\"$(DEFAULT_MIXER_COMMAND)\" \
	-DDATADIR=\""$(datadir)"\" \
	$(PLATFORM_CPPFLAGS)

plugindir = $(libdir)/xfce4/panel/plugins

plugin_LTLIBRARIES = \
	libvolume-plugin.la

BUILT_SOURCES = \
	volume-resources.c \
	volume-resources.h

libvolume_plugin_la_SOURCES = \
	$(BUILT_SOURCES) \
	pulseaudio-volume.c \
	pulseaudio-volume.h \
	pulseaudio-config.c \
	pulseaudio-config.h \
	pulseaudio-debug.c \
	pulseaudio-debug.h \
	volume-plugin.c \
	volume-plugin.h

resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/gresource.xml)
volume-resources.c: gresource.xml $(resource_files)
	$(AM_V_GEN) glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-source --c-name volume $<
volume-resources.h: gresource.xml $(resource_files)
	$(AM_V_GEN) glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-header --c-name volume $<

libvolume_plugin_la_CFLAGS = \
	$(CANBERRA_CFLAGS) \
	$(PULSEAUDIO_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(LIBXFCE4UI_CFLAGS) \
	$(LIBXFCE4PANEL_CFLAGS) \
	$(XFCONF_CFLAGS) \
	$(KEYBINDER_CFLAGS) \
	$(LIBNOTIFY_CFLAGS) \
	$(PLATFORM_CFLAGS)

libvolume_plugin_la_LDFLAGS = \
	-avoid-version \
	-module \
	-no-undefined \
	-export-symbols-regex '^xfce_panel_module_(preinit|init|construct)' \
	$(PLATFORM_LDFLAGS)

libvolume_plugin_la_LIBADD = \
	$(CANBERRA_LIBS) \
	$(PULSEAUDIO_LIBS) \
	$(GLIB_LIBS) \
	$(GTK_LIBS) \
	$(LIBXFCE4UTIL_LIBS) \
	$(LIBXFCE4UI_LIBS) \
	$(LIBXFCE4PANEL_LIBS) \
	$(XFCONF_LIBS) \
	$(KEYBINDER_LIBS) \
	$(LIBNOTIFY_LIBS) \
	$(LIBM)

#
# Desktop file
#
desktopdir = \
	$(datadir)/xfce4/panel/plugins

desktop_in_files = \
	volume-plugin.desktop.in.in

desktop_DATA = $(desktop_in_files:.desktop.in.in=.desktop)

@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = $(desktop_in_files)

DISTCLEANFILES = $(desktop_DATA)

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
