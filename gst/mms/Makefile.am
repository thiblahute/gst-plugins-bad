# plugindir is set in configure

plugin_LTLIBRARIES = libgstmmssrc.la

libgstmmssrc_la_SOURCES = \
	gstmmssrc.c mms.c

noinst_HEADERS = \
	gstmmssrc.h mms.h

libgstmmssrc_la_CFLAGS = $(GST_CFLAGS)  $(GIO_CFLAGS)
libgstmmssrc_la_LIBADD = $(GST_LIBS) $(GST_BASE_LIBS)  $(GIO_LIBS)
libgstmmssrc_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS) $(GIO_LDFLAGS)
libgstmmssrc_la_LIBTOOLFLAGS = --tag=disable-static

Android.mk: Makefile.am $(BUILT_SOURCES)
	androgenizer \
	-:PROJECT libgstmms -:SHARED libgstmms \
	 -:TAGS eng debug \
         -:REL_TOP $(top_srcdir) -:ABS_TOP $(abs_top_srcdir) \
	 -:SOURCES $(libgstmms_la_SOURCES) \
	 -:CFLAGS $(DEFS) $(DEFAULT_INCLUDES) $(libgstmms_la_CFLAGS) \
	 -:LDFLAGS $(libgstmms_la_LDFLAGS) \
	           $(libgstmms_la_LIBADD) \
	           -ldl \
	 -:PASSTHROUGH LOCAL_ARM_MODE:=arm \
		       LOCAL_MODULE_PATH:='$$(TARGET_OUT)/lib/gstreamer-0.10' \
	> $@

