/* GStreamer
 * Copyright (C) 2012 Collabora Ltd.
 *
 * Author: Thibault Saunier <thibault.saunier@collabora.com
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
 * Boston, MA 02110-1335, USA.
 */
/**
 * SECTION:element-gstmms_src
 *
 * The src element does FIXME stuff.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v mmssrc2 uri=mms://find.an.example ! decodebin ! audioresample ! audioconvert ! autoaudiosink
 * ]|
 * FIXME Describe what the pipeline does.
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <glib.h>
#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>
#include "gstmmssrc.h"

GST_DEBUG_CATEGORY_STATIC (gst_mms_src_debug_category);
#define GST_CAT_DEFAULT gst_mms_src_debug_category

/* prototypes */
static void gst_mms_src_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_mms_src_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_mms_src_dispose (GObject * object);
static void gst_mms_src_finalize (GObject * object);

/*static GstCaps *gst_mms_src_get_caps (GstBaseSrc * bsrc);*/
/*static gboolean gst_mms_src_set_caps (GstBaseSrc * bsrc, GstCaps * caps);*/
static gboolean gst_mms_src_newsegment (GstBaseSrc * bsrc);
static gboolean gst_mms_src_start (GstBaseSrc * bsrc);
static gboolean gst_mms_src_stop (GstBaseSrc * bsrc);
static void
gst_mms_src_get_times (GstBaseSrc * bsrc, GstBuffer * buffer,
    GstClockTime * start, GstClockTime * end);
static gboolean gst_mms_src_get_size (GstBaseSrc * bsrc, guint64 * size);
static gboolean gst_mms_src_is_seekable (GstBaseSrc * bsrc);
static gboolean gst_mms_src_unlock (GstBaseSrc * bsrc);
static gboolean gst_mms_src_event (GstBaseSrc * bsrc, GstEvent * event);
/*static gboolean gst_mms_src_do_seek (GstBaseSrc * bsrc, GstSegment * segment);*/
static gboolean gst_mms_src_query (GstBaseSrc * bsrc, GstQuery * query);
static gboolean gst_mms_src_check_get_range (GstBaseSrc * bsrc);
static void gst_mms_src_fixate (GstBaseSrc * bsrc, GstCaps * caps);
static gboolean gst_mms_src_unlock_stop (GstBaseSrc * bsrc);
static gboolean
gst_mms_src_prepare_seek_segment (GstBaseSrc * bsrc, GstEvent * seek,
    GstSegment * segment);
static GstFlowReturn gst_mms_src_create (GstPushSrc * bsrc, GstBuffer ** buf);

enum
{
  PROP_0,
  PROP_URI
};

/* pad templates */

static GstStaticPadTemplate gst_mms_src_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-ms-asf")
    );


/* GstURIHandler implementation
 * (Mostly copy/pasted from -bad/ext/libmms/gstmms.c) */
static GstURIType
gst_mms_uri_get_type (void)
{
  return GST_URI_SRC;
}

static gchar **
gst_mms_uri_get_protocols (void)
{
  static const gchar *protocols[] = { "mms", "mmsh", "mmst", "mmsu", NULL };

  return (gchar **) protocols;
}

static const gchar *
gst_mms_uri_get_uri (GstURIHandler * handler)
{
  GstMMSSrc *bsrc = GST_MMS_SRC (handler);

  return bsrc->uri;
}

static inline gboolean
gst_mms_uri_is_valid_uri (const gchar * uri)
{
  gchar *protocol;
  const gchar *colon, *tmp;
  gsize len;

  if (!uri || !gst_uri_is_valid (uri))
    return FALSE;

  protocol = gst_uri_get_protocol (uri);

  if ((g_strcmp0 (protocol, "mms") != 0) &&
      (g_strcmp0 (protocol, "mmsh") != 0) &&
      (g_strcmp0 (protocol, "mmst") != 0) &&
      (g_strcmp0 (protocol, "mmsu") != 0)) {
    g_free (protocol);
    return FALSE;
  }
  g_free (protocol);

  colon = strstr (uri, "://");
  if (!colon)
    return FALSE;

  tmp = colon + 3;
  len = strlen (tmp);
  if (len == 0)
    return FALSE;

  return TRUE;
}

static gboolean
gst_mms_uri_set_uri (GstURIHandler * handler, const gchar * uri)
{
  GstMMSSrc *bsrc = GST_MMS_SRC (handler);

  if (gst_mms_uri_is_valid_uri (uri) == FALSE) {
    GST_DEBUG_OBJECT (bsrc, "%s is not a valid URI", uri);

    return FALSE;
  }

  GST_DEBUG_OBJECT (bsrc, "Set uri: %s", uri);

  if (bsrc->uri)
    g_free (bsrc->uri);
  bsrc->uri = g_strdup (uri);

  return TRUE;
}

static void
gst_mms_uri_handler_init (gpointer g_iface, gpointer iface_data)
{
  GstURIHandlerInterface *iface = (GstURIHandlerInterface *) g_iface;

  iface->get_type = gst_mms_uri_get_type;
  iface->get_protocols = gst_mms_uri_get_protocols;
  iface->get_uri = gst_mms_uri_get_uri;
  iface->set_uri = gst_mms_uri_set_uri;
}

static void
gst_mms_urihandler_init (GType mms_type)
{
  static const GInterfaceInfo urihandler_info = {
    gst_mms_uri_handler_init,
    NULL,
    NULL
  };

  g_type_add_interface_static (mms_type, GST_TYPE_URI_HANDLER,
      &urihandler_info);
}

/* class initialization */
GST_BOILERPLATE_FULL (GstMMSSrc, gst_mms_src, GstPushSrc,
    GST_TYPE_PUSH_SRC, gst_mms_urihandler_init);

static void
gst_mms_src_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_add_static_pad_template (element_class,
      &gst_mms_src_src_template);

  gst_element_class_set_details_simple (element_class, "MMS source",
      "Source/Network", "Receive data as a client via the MMS protocol",
      "Thibault Saunier <thibault.saunier@collabora.com>");

  GST_DEBUG_CATEGORY_INIT (gst_mms_src_debug_category, "mmssrc2", 0,
      "debug category for src element");
}

static void
gst_mms_src_class_init (GstMMSSrcClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBaseSrcClass *base_src_class = GST_BASE_SRC_CLASS (klass);
  GstPushSrcClass *push_src_class = GST_PUSH_SRC_CLASS (klass);

  gobject_class->set_property = gst_mms_src_set_property;
  gobject_class->get_property = gst_mms_src_get_property;
  gobject_class->dispose = gst_mms_src_dispose;
  gobject_class->finalize = gst_mms_src_finalize;

  /*base_src_class->get_caps = GST_DEBUG_FUNCPTR (gst_mms_src_get_caps); */
  /*base_src_class->set_caps = GST_DEBUG_FUNCPTR (gst_mms_src_set_caps); */
  base_src_class->newsegment = GST_DEBUG_FUNCPTR (gst_mms_src_newsegment);
  base_src_class->start = GST_DEBUG_FUNCPTR (gst_mms_src_start);
  base_src_class->stop = GST_DEBUG_FUNCPTR (gst_mms_src_stop);
  base_src_class->get_times = GST_DEBUG_FUNCPTR (gst_mms_src_get_times);
  base_src_class->get_size = GST_DEBUG_FUNCPTR (gst_mms_src_get_size);
  base_src_class->is_seekable = GST_DEBUG_FUNCPTR (gst_mms_src_is_seekable);
  base_src_class->unlock = GST_DEBUG_FUNCPTR (gst_mms_src_unlock);
  base_src_class->event = GST_DEBUG_FUNCPTR (gst_mms_src_event);
  /*base_src_class->do_seek = GST_DEBUG_FUNCPTR (gst_mms_src_do_seek); */
  base_src_class->query = GST_DEBUG_FUNCPTR (gst_mms_src_query);
  base_src_class->check_get_range =
      GST_DEBUG_FUNCPTR (gst_mms_src_check_get_range);
  base_src_class->fixate = GST_DEBUG_FUNCPTR (gst_mms_src_fixate);
  base_src_class->unlock_stop = GST_DEBUG_FUNCPTR (gst_mms_src_unlock_stop);
  base_src_class->prepare_seek_segment =
      GST_DEBUG_FUNCPTR (gst_mms_src_prepare_seek_segment);

  push_src_class->create = GST_DEBUG_FUNCPTR (gst_mms_src_create);

  g_object_class_install_property (gobject_class, PROP_URI,
      g_param_spec_string ("uri", "uri",
          "Host URL to connect to. Accepted are mms://, mmsu://, mmst:// URL types",
          NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gst_mms_src_init (GstMMSSrc * src, GstMMSSrcClass * mms_src_class)
{
  GstBaseSrc *bsrc = GST_BASE_SRC (src);

  mms_session_init (&src->mms_session, GST_ELEMENT (src));
  src->srcpad = GST_BASE_SRC_PAD (bsrc);

  src->stream_lock = g_new (GStaticRecMutex, 1);
  g_static_rec_mutex_init (src->stream_lock);

  gst_base_src_set_format (bsrc, GST_FORMAT_TIME);
  gst_base_src_set_do_timestamp (bsrc, TRUE);

}

/* GObject virtual methods */
void
gst_mms_src_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstMMSSrc *src = GST_MMS_SRC (object);

  switch (property_id) {
    case PROP_URI:
      gst_mms_uri_set_uri (GST_URI_HANDLER (src), g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_mms_src_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstMMSSrc *src = GST_MMS_SRC (object);

  switch (property_id) {
    case PROP_URI:
      g_value_set_string (value, src->uri);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_mms_src_dispose (GObject * object)
{
  /* GstMMSSrc *src = GST_MMS_SRC (object); */

  /* clean up as possible.  may be called multiple times */

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

void
gst_mms_src_finalize (GObject * object)
{
  mms_session_clean (&GST_MMS_SRC (object)->mms_session);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}


/* GstBaseSrc virtual methods */
/*static GstCaps **/
/*gst_mms_src_get_caps (GstBaseSrc * bsrc)*/
/*{*/
/*GstMMSSrc *src = GST_MMS_SRC (bsrc);*/

/*GST_DEBUG_OBJECT (src, "get_caps");*/

/*return NULL;*/
/*}*/

/*static gboolean*/
/*gst_mms_src_set_caps (GstBaseSrc * bsrc, GstCaps * caps)*/
/*{*/
/*GstMMSSrc *src = GST_MMS_SRC (bsrc);*/

/*GST_DEBUG_OBJECT (src, "set_caps");*/

/*return TRUE;*/
/*}*/

static gboolean
gst_mms_src_newsegment (GstBaseSrc * bsrc)
{
  GstMMSSrc *src = GST_MMS_SRC (bsrc);

  GST_DEBUG_OBJECT (src, "newsegment");

  return TRUE;
}

static void
gst_mms_src_io_thread (GstMMSSrc * src)
{
  /* Run the context loop */
  g_main_context_iteration (src->context, FALSE);
}

static gboolean
gst_mms_src_start (GstBaseSrc * bsrc)
{
  gint res;
  GstMMSSrc *src = GST_MMS_SRC (bsrc);

  src->context = g_main_context_new ();
  src->loop = g_main_loop_new (src->context, TRUE);

  if (src->loop == NULL) {
    GST_ELEMENT_ERROR (src, LIBRARY, INIT, (NULL),
        ("Failed to start GMainLoop"));
    g_main_context_unref (src->context);
    return FALSE;
  }

  src->task = gst_task_create ((GstTaskFunction) gst_mms_src_io_thread, src);
  gst_task_set_lock (src->task, GST_MMS_SRC_GET_LOCK (src));

  /* We start the I/O task */
  gst_task_start (src->task);
  res = mms_session_connect (&src->mms_session, src->uri, src->context);

  if (res == -1) {
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ, ("No URI to open specified"),
        (NULL));

    return FALSE;
  }

  return res ? TRUE : FALSE;
}

static gboolean
gst_mms_src_stop (GstBaseSrc * bsrc)
{
  GstMMSSrc *src = GST_MMS_SRC (bsrc);

  GST_DEBUG_OBJECT (src, "stopping");

  if (src->task) {
    gst_task_stop (src->task);

    /* now wait for the task to finish */
    gst_task_join (src->task);

    gst_object_unref (GST_OBJECT (src->task));
    g_main_loop_quit (src->loop);
    g_main_context_unref (src->context);
    g_main_loop_unref (src->loop);

    src->loop = NULL;
    src->context = NULL;
    src->task = NULL;
  }

  /* We can now free ressources */
  mms_session_clean (&src->mms_session);
  GST_DEBUG_OBJECT (src, "stop");

  return TRUE;
}

static void
gst_mms_src_get_times (GstBaseSrc * bsrc, GstBuffer * buffer,
    GstClockTime * start, GstClockTime * end)
{
  GstMMSSrc *src = GST_MMS_SRC (bsrc);

  GST_DEBUG_OBJECT (src, "get_times");
}

static gboolean
gst_mms_src_get_size (GstBaseSrc * bsrc, guint64 * size)
{
  GstMMSSrc *src = GST_MMS_SRC (bsrc);

  GST_DEBUG_OBJECT (src, "get_size");

  /**size = 0;*/
  return TRUE;
}

static gboolean
gst_mms_src_is_seekable (GstBaseSrc * bsrc)
{
  GstMMSSrc *src = GST_MMS_SRC (bsrc);

  GST_DEBUG_OBJECT (src, "is_seekable");

  return mms_session_is_seekable (&src->mms_session);
}

static gboolean
gst_mms_src_unlock (GstBaseSrc * bsrc)
{
  GstMMSSrc *src = GST_MMS_SRC (bsrc);

  GST_DEBUG_OBJECT (src, "unlock");
  if (src->task) {
    gst_task_stop (src->task);
    gst_task_join (src->task);
    g_main_loop_quit (src->loop);
  }

  GST_MMS_SRC_STREAM_UNLOCK (src);

  return TRUE;
}

static gboolean
gst_mms_src_event (GstBaseSrc * bsrc, GstEvent * event)
{
  GstMMSSrc *src = GST_MMS_SRC (bsrc);

  GST_DEBUG_OBJECT (src, "event");

  return TRUE;
}

/*static gboolean*/
/*gst_mms_src_do_seek (GstBaseSrc * bsrc, GstSegment * segment)*/
/*{*/
/*GstMMSSrc *src = GST_MMS_SRC (bsrc);*/

/*GST_DEBUG_OBJECT (src, "do_seek");*/

/*return FALSE;*/
/*}*/

static gboolean
gst_mms_src_query (GstBaseSrc * bsrc, GstQuery * query)
{
  GstMMSSrc *src = GST_MMS_SRC (bsrc);

  GST_DEBUG_OBJECT (src, "query");

  return TRUE;
}

static gboolean
gst_mms_src_check_get_range (GstBaseSrc * bsrc)
{
  GstMMSSrc *src = GST_MMS_SRC (bsrc);

  GST_DEBUG_OBJECT (src, "get_range");

  return FALSE;
}

static void
gst_mms_src_fixate (GstBaseSrc * bsrc, GstCaps * caps)
{
  GstMMSSrc *src = GST_MMS_SRC (bsrc);

  GST_DEBUG_OBJECT (src, "fixate");
}

static gboolean
gst_mms_src_unlock_stop (GstBaseSrc * bsrc)
{
  GstMMSSrc *src = GST_MMS_SRC (bsrc);

  GST_DEBUG_OBJECT (src, "stop");

  return TRUE;
}

static gboolean
gst_mms_src_prepare_seek_segment (GstBaseSrc * bsrc, GstEvent * seek,
    GstSegment * segment)
{
  GstMMSSrc *src = GST_MMS_SRC (bsrc);

  GST_DEBUG_OBJECT (src, "seek_segment");

  return FALSE;
}

/* GstPushSrc virtual methods */
static GstFlowReturn
gst_mms_src_create (GstPushSrc * bsrc, GstBuffer ** buf)
{
  GError *err = NULL;
  GstMMSSrc *src = GST_MMS_SRC (bsrc);

  GST_LOG_OBJECT (src, "buffer wanted");

  if (G_UNLIKELY (src->mms_session.initialized == FALSE &&
          src->mms_session.flow == GST_FLOW_OK)) {

    GST_DEBUG ("But still connecting... waiting for the connection "
        "to the server to be done");
    /* Wait until connection is done */
    while ((src->mms_session.flow == GST_FLOW_OK &&
            G_UNLIKELY (src->mms_session.initialized == FALSE))) {
      g_cond_wait (src->mms_session.connected_cond,
          (GMutex *) GST_MMS_SRC_GET_LOCK (src));
    }

    if (src->mms_session.flow != GST_FLOW_OK) {
      GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ,
          ("Could not connect to streaming server."), (NULL));
      /*FIXME redirect to rtsp in this case */

      return GST_FLOW_ERROR;
    }

    GST_DEBUG ("Now connected");
  }


  mms_session_fill_buffer (&src->mms_session, buf, &err);

  /* Wait for the buffer to be filled */
  while (src->mms_session.filled != TRUE &&
      src->mms_session.flow == GST_FLOW_OK)
    g_cond_wait (src->mms_session.buf_ready,
        (GMutex *) GST_MMS_SRC_GET_LOCK (src));

  return src->mms_session.flow;
}

/* Plugin initialization */
static gboolean
plugin_init (GstPlugin * plugin)
{

  return gst_element_register (plugin, "mmssrc2", GST_RANK_PRIMARY,
      GST_TYPE_MMS_SRC);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "mms2",
    "Microsoft Multi Media Server streaming protocol support",
    plugin_init, VERSION, "LGPL", PACKAGE_NAME, GST_PACKAGE_ORIGIN)
