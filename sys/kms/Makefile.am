plugin_LTLIBRARIES = libgstkmssink.la

libgstkmssink_la_SOURCES = \
	gstkmssink.c \
	gstkmsbufferpriv.c \
	gstdrmutils.c

libgstkmssink_la_CFLAGS = \
	$(GST_PLUGINS_BAD_CFLAGS) \
	$(GST_PLUGINS_BASE_CFLAGS) \
	$(GST_BASE_CFLAGS) \
	$(GST_CFLAGS) \
	$(DRM_CFLAGS)

libgstkmssink_la_LIBADD = \
	$(GST_PLUGINS_BASE_LIBS) \
	$(GST_BASE_LIBS) \
	$(GST_LIBS) \
	$(DRM_LIBS) \
	-lgstvideo-$(GST_MAJORMINOR) \
	$(top_builddir)/gst-libs/gst/video/libgstbasevideo-$(GST_MAJORMINOR).la \
	-lgstdmabuf-$(GST_MAJORMINOR) \
	$(top_builddir)/gst-libs/gst/drm/libgstdrm-$(GST_MAJORMINOR).la

libgstkmssink_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstkmssink_la_LIBTOOLFLAGS = --tag=disable-static

noinst_HEADERS = gstkmssink.h gstdrmutils.h gstkmsbufferpriv.h
