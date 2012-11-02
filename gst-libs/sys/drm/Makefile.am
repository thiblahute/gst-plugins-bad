
lib_LTLIBRARIES = libgstdrm-@GST_API_VERSION@.la

CLEANFILES = $(BUILT_SOURCES)

libgstdrm_@GST_API_VERSION@_la_SOURCES = \
	gstdrmbufferpool.c gstdrmmeta.c

libgstdrm_@GST_API_VERSION@includedir = \
	$(includedir)/gstreamer-@GST_API_VERSION@/sys/drm

libgstdrm_@GST_API_VERSION@include_HEADERS = \
	gstdrmbufferpool.h gstdrmmeta.h

libgstdrm_@GST_API_VERSION@_la_CFLAGS = \
	$(DRM_CFLAGS) \
	$(GST_PLUGINS_BAD_CFLAGS) \
	$(GST_PLUGINS_BASE_CFLAGS) \
	-DGST_USE_UNSTABLE_API \
	$(GST_CFLAGS)

libgstdrm_@GST_API_VERSION@_la_LIBADD = \
	$(DRM_LIBS) \
	$(top_builddir)/gst-libs/sys/dma/libgstdmabuf-$(GST_API_VERSION).la \
	$(GST_PLUGINS_BASE_LIBS) \
	$(GST_BASE_LIBS) \
	$(GST_LIBS)

libgstdrm_@GST_API_VERSION@_la_LDFLAGS = \
	$(DRM_LDFLAGS) \
	$(GST_LIB_LDFLAGS) \
	$(GST_ALL_LDFLAGS) \
	$(GST_LT_LDFLAGS)

