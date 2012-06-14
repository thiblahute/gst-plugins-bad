
lib_LTLIBRARIES = libgstdrm-@GST_MAJORMINOR@.la

CLEANFILES = $(BUILT_SOURCES)

libgstdrm_@GST_MAJORMINOR@_la_SOURCES = \
	gstdrmbufferpool.c

libgstdrm_@GST_MAJORMINOR@includedir = \
	$(includedir)/gstreamer-@GST_MAJORMINOR@/gst/drm

libgstdrm_@GST_MAJORMINOR@include_HEADERS = \
	gstdrmbufferpool.h

libgstdrm_@GST_MAJORMINOR@_la_CFLAGS = \
	$(DRM_CFLAGS) \
	$(GST_PLUGINS_BAD_CFLAGS) \
	$(GST_PLUGINS_BASE_CFLAGS) \
	-DGST_USE_UNSTABLE_API \
	$(GST_CFLAGS)

libgstdrm_@GST_MAJORMINOR@_la_LIBADD = \
	$(DRM_LIBS) \
	-lgstdmabuf-$(GST_MAJORMINOR) \
	$(GST_PLUGINS_BASE_LIBS) \
	$(GST_BASE_LIBS) \
	$(GST_LIBS) \
	-lgstdmabuf-$(GST_MAJORMINOR)

libgstdrm_@GST_MAJORMINOR@_la_LDFLAGS = \
	$(DRM_LDFLAGS) \
	$(GST_LIB_LDFLAGS) \
	$(GST_ALL_LDFLAGS) \
	$(GST_LT_LDFLAGS)

