plugin_LTLIBRARIES = libgstpvr.la

libgstpvr_la_SOURCES = \
	gstpvr.c \
	gstpvrvideosink.c \
	gstpvrbufferpriv.c

libgstpvr_la_CFLAGS = $(GST_PLUGINS_BAD_CFLAGS) $(GST_PLUGINS_BASE_CFLAGS) $(GST_CFLAGS) $(PVR_CFLAGS) $(DRM_CFLAGS)

libgstpvr_la_LIBADD = $(GST_LIBS) $(GST_BASE_LIBS) \
	$(GST_PLUGINS_BASE_LIBS) $(X11_LIBS) -lgstvideo-$(GST_MAJORMINOR) \
	-lgstinterfaces-$(GST_MAJORMINOR) -lgstdmabuf-$(GST_MAJORMINOR) -lpvr2d -lpvrws_OMAPDRI2 \
	$(top_builddir)/gst-libs/gst/drm/libgstdrm-$(GST_MAJORMINOR).la \
	$(PVR_LIBS) $(DRM_LIBS) \
	$(LIBM)

libgstpvr_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstpvr_la_LIBTOOLFLAGS = --tag=disable-static

noinst_HEADERS = \
	gstpvr.h \
	gstpvrvideosink.h
	gstpvrbufferpriv.h
