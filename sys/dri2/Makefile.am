plugin_LTLIBRARIES = libgstdri2.la

libgstdri2_la_SOURCES = \
	gstdri2.c \
	gstdri2videosink.c \
	gstdri2bufferpool.c \
	gstdri2util.c

libgstdri2_la_CFLAGS = $(GST_PLUGINS_BAD_CFLAGS) $(GST_PLUGINS_BASE_CFLAGS) \
		$(GST_CFLAGS) $(DRI2_CFLAGS) $(GST_VIDEO)

libgstdri2_la_LIBADD = $(GST_LIBS) $(GST_BASE_LIBS) \
	$(GST_PLUGINS_BASE_LIBS) $(GST_PLUGINS_BAD_LIBS) $(X11_LIBS) $(DRI2_LIBS) \
	$(LIBM) -lgstvideo-$(GST_API_VERSION) \
	$(top_builddir)/gst-libs/sys/dma/libgstdmabuf-$(GST_API_VERSION).la \
	$(top_builddir)/gst-libs/sys/drm/libgstdrm-$(GST_API_VERSION).la

libgstdri2_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstdri2_la_LIBTOOLFLAGS = --tag=disable-static

noinst_HEADERS = \
	gstdri2videosink.h \
	gstdri2bufferpool.h \
	gstdri2util.h
