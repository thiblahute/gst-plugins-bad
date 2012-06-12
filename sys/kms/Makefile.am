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
	$(GST_BASE_LIBS) \
	-lgstvideo-$(GST_MAJORMINOR) \
	-lgstbasevideo-$(GST_MAJORMINOR) \
	-lgstdmabuf-$(GST_MAJORMINOR) \
	-lgstdrm-$(GST_MAJORMINOR) \
	$(GST_LIBS) \
	$(DRM_LIBS)

libgstkmssink_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstkmssink_la_LIBTOOLFLAGS = --tag=disable-static

noinst_HEADERS = gstkmssink.h gstdrmutils.h gstkmsbufferpriv.h
