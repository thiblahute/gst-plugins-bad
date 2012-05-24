plugin_LTLIBRARIES = libgstkmssink.la

libgstkmssink_la_SOURCES =  gstkmssink.c gstdrmutils.c \
			    gstducatibufferpool.c \
			    gstducatibuffer.c \
			    gstducatidrmbuffer.c \
			    gstducatikmsbuffer.c
libgstkmssink_la_CFLAGS = $(GST_PLUGINS_BAD_CFLAGS) $(GST_PLUGINS_BASE_CFLAGS) $(GST_BASE_CFLAGS) $(GST_CFLAGS) \
			  $(DRM_CFLAGS) $(LIBDCE_CFLAGS)
libgstkmssink_la_LIBADD = \
	$(GST_BASE_LIBS) -lgstvideo-$(GST_MAJORMINOR) -lgstbasevideo-$(GST_MAJORMINOR) \
	$(GST_LIBS) \
	$(LIBDCE_LIBS) \
	$(DRM_LIBS)

libgstkmssink_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstkmssink_la_LIBTOOLFLAGS = --tag=disable-static

noinst_HEADERS = gstkmssink.h gstdrmutils.h
