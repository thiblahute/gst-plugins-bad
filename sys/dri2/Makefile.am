plugin_LTLIBRARIES = libgstdri2.la

libgstdri2_la_SOURCES = \
	gstdri2.c \
	gstdri2videosink.c \
	gstdri2bufferpool.c \
	gstdri2util.c

libgstdri2_la_CFLAGS = $(GST_PLUGINS_BAD_CFLAGS) $(GST_PLUGINS_BASE_CFLAGS) $(GST_CFLAGS) $(DRI2_CFLAGS)

libgstdri2_la_LIBADD = $(GST_LIBS) $(GST_BASE_LIBS) \
	$(GST_PLUGINS_BASE_LIBS) $(GST_PLUGINS_BAD_LIBS) $(X11_LIBS) -lgstvideo-$(GST_MAJORMINOR) \
	-lgstinterfaces-$(GST_MAJORMINOR) -lgstdmabuf-$(GST_MAJORMINOR) \
	$(top_builddir)/gst-libs/gst/drm/libgstdrm-$(GST_MAJORMINOR).la \
	$(DRI2_LIBS) \
	$(LIBM)

libgstdri2_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstdri2_la_LIBTOOLFLAGS = --tag=disable-static

noinst_HEADERS = \
	gstdri2videosink.h \
	gstdri2bufferpool.h \
	gstdri2util.h
