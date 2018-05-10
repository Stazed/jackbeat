gladedir = $(pkgdatadir)/glade

glade_DATA = jackbeat.ui

EXTRA_DIST = jackbeat.ui jackbeat.glade

%.ui: %.glade
	gtk-builder-convert $< - | grep -v requires-version > $@

