/* GStreamer
 * Copyright (C) 2012 Texas Instruments
 * Copyright (C) 2012 Collabora Ltd
 *
 * Authors:
 *  Alessandro Decina <alessandro.decina@collabora.co.uk>
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors:
 *  Alessandro Decina <alessandro.decina@collabora.co.uk>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/video/videocontext.h>
#include "gstkmssink.h"
#include "gstkmsbufferpriv.h"

#include <omap_drm.h>
#include <omap_drmif.h>
#include <xf86drmMode.h>

GST_DEBUG_CATEGORY_STATIC (gst_debug_kms_sink);
#define GST_CAT_DEFAULT gst_debug_kms_sink

static void
gst_kms_sink_video_context_interface_init (GstVideoContextInterface * iface);

G_DEFINE_TYPE_EXTENDED (GstKMSSink, gst_kms_sink, GST_TYPE_VIDEO_SINK, 0,
    G_IMPLEMENT_INTERFACE (GST_TYPE_VIDEO_CONTEXT,
        gst_kms_sink_video_context_interface_init));

static void gst_kms_sink_reset (GstKMSSink * sink);

static GstStaticPadTemplate gst_kms_sink_template_factory =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_YUV ("NV12"))
    );

enum
{
  PROP_0,
  PROP_PIXEL_ASPECT_RATIO,
  PROP_FORCE_ASPECT_RATIO,
  PROP_SCALE,
  PROP_CONNECTOR,
};

static gboolean
gst_kms_sink_calculate_aspect_ratio (GstKMSSink * sink, gint width,
    gint height, gint video_par_n, gint video_par_d)
{
  guint calculated_par_n;
  guint calculated_par_d;

  if (!gst_video_calculate_display_ratio (&calculated_par_n, &calculated_par_d,
          width, height, video_par_n, video_par_d, 1, 1)) {
    GST_ELEMENT_ERROR (sink, CORE, NEGOTIATION, (NULL),
        ("Error calculating the output display ratio of the video."));
    return FALSE;
  }
  GST_DEBUG_OBJECT (sink,
      "video width/height: %dx%d, calculated display ratio: %d/%d",
      width, height, calculated_par_n, calculated_par_d);

  /* now find a width x height that respects this display ratio.
   * prefer those that have one of w/h the same as the incoming video
   * using wd / hd = calculated_pad_n / calculated_par_d */

  /* start with same height, because of interlaced video */
  /* check hd / calculated_par_d is an integer scale factor, and scale wd with the PAR */
  if (height % calculated_par_d == 0) {
    GST_DEBUG_OBJECT (sink, "keeping video height");
    GST_VIDEO_SINK_WIDTH (sink) = (guint)
        gst_util_uint64_scale_int (height, calculated_par_n, calculated_par_d);
    GST_VIDEO_SINK_HEIGHT (sink) = height;
  } else if (width % calculated_par_n == 0) {
    GST_DEBUG_OBJECT (sink, "keeping video width");
    GST_VIDEO_SINK_WIDTH (sink) = width;
    GST_VIDEO_SINK_HEIGHT (sink) = (guint)
        gst_util_uint64_scale_int (width, calculated_par_d, calculated_par_n);
  } else {
    GST_DEBUG_OBJECT (sink, "approximating while keeping video height");
    GST_VIDEO_SINK_WIDTH (sink) = (guint)
        gst_util_uint64_scale_int (height, calculated_par_n, calculated_par_d);
    GST_VIDEO_SINK_HEIGHT (sink) = height;
  }
  GST_DEBUG_OBJECT (sink, "scaling to %dx%d",
      GST_VIDEO_SINK_WIDTH (sink), GST_VIDEO_SINK_HEIGHT (sink));

  return TRUE;
}

static gboolean
gst_kms_sink_setcaps (GstBaseSink * bsink, GstCaps * caps)
{
  GstKMSSink *sink;
  gboolean ret = TRUE;
  gint width, height;
  gint fps_n, fps_d;
  gint par_n, par_d;
  GstVideoFormat format;

  sink = GST_KMS_SINK (bsink);

  ret = gst_video_format_parse_caps (caps, &format, &width, &height);
  ret &= gst_video_parse_caps_framerate (caps, &fps_n, &fps_d);
  ret &= gst_video_parse_caps_pixel_aspect_ratio (caps, &par_n, &par_d);
  if (!ret)
    return FALSE;

  if (width <= 0 || height <= 0) {
    GST_ELEMENT_ERROR (sink, CORE, NEGOTIATION, (NULL),
        ("Invalid image size."));
    return FALSE;
  }

  sink->format = format;
  sink->par_n = par_n;
  sink->par_d = par_d;
  sink->src_rect.x = sink->src_rect.y = 0;
  sink->src_rect.w = width;
  sink->src_rect.h = height;
  sink->input_width = width;
  sink->input_height = height;

  if (!sink->pool || !gst_drm_buffer_pool_check_caps (sink->pool, caps)) {
    int size;

    if (sink->pool) {
      gst_drm_buffer_pool_destroy (sink->pool);
      sink->pool = NULL;
    }

    size = gst_video_format_get_size (format, width, height);
    sink->pool = gst_drm_buffer_pool_new (GST_ELEMENT (sink),
        sink->fd, caps, size);
  }

  sink->conn.crtc = -1;
  sink->plane = NULL;

  return TRUE;
}

static void
gst_kms_sink_get_times (GstBaseSink * bsink, GstBuffer * buf,
    GstClockTime * start, GstClockTime * end)
{
  GstKMSSink *sink;

  sink = GST_KMS_SINK (bsink);

  if (GST_BUFFER_TIMESTAMP_IS_VALID (buf)) {
    *start = GST_BUFFER_TIMESTAMP (buf);
    if (GST_BUFFER_DURATION_IS_VALID (buf)) {
      *end = *start + GST_BUFFER_DURATION (buf);
    } else {
      if (sink->fps_n > 0) {
        *end = *start +
            gst_util_uint64_scale_int (GST_SECOND, sink->fps_d, sink->fps_n);
      }
    }
  }
}

static gboolean
gst_kms_sink_query (GstBaseSink * basesink, GstQuery * query)
{
  gboolean res = FALSE;
  GstKMSSink *sink = GST_KMS_SINK (basesink);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CUSTOM:
    {
      const gchar **types;
      gint i;
      GstStructure *structure;

      structure = gst_query_get_structure (query);
      if (strcmp (gst_structure_get_name (structure), "prepare-video-context"))
        break;

      types = gst_video_context_query_get_supported_types (query);
      for (i = 0; types[i]; i++) {
        if (strcmp (types[i], "drm-fd"))
          continue;

        if (sink->fd != -1) {
          gst_structure_set (structure,
              "video-context-type", G_TYPE_STRING, types[i],
              "video-context", G_TYPE_INT, sink->fd, NULL);

          res = TRUE;
        }

        break;
      }

      break;
    }
    default:
      break;
  }

  return res;
}

static GstFlowReturn
gst_kms_sink_show_frame (GstVideoSink * vsink, GstBuffer * inbuf)
{
  GstKMSSink *sink = GST_KMS_SINK (vsink);
  GstBuffer *buf = NULL;
  GstKMSBufferPriv *priv;
  GstFlowReturn flow_ret = GST_FLOW_OK;
  int ret;

  GST_INFO_OBJECT (sink, "enter");

  if (sink->conn.crtc == -1) {
    GstVideoRectangle dest = { 0 };

    if (!gst_drm_connector_find_mode_and_plane (sink->fd,
            sink->src_rect.w, sink->src_rect.h,
            sink->resources, sink->plane_resources, &sink->conn, &sink->plane))
      goto connector_not_found;

    dest.w = sink->conn.mode->hdisplay;
    dest.h = sink->conn.mode->vdisplay;
    gst_video_sink_center_rect (sink->src_rect, dest, &sink->dst_rect,
        sink->scale);
  }

  priv = gst_kms_buffer_priv (sink, inbuf);
  if (priv) {
    buf = gst_buffer_ref (inbuf);
  } else {
    GST_LOG_OBJECT (sink, "not a KMS buffer, slow-path!");
    buf = gst_drm_buffer_pool_get (sink->pool, FALSE);
    if (buf) {
      GST_BUFFER_TIMESTAMP (buf) = GST_BUFFER_TIMESTAMP (inbuf);
      GST_BUFFER_DURATION (buf) = GST_BUFFER_DURATION (inbuf);
      memcpy (GST_BUFFER_DATA (buf),
          GST_BUFFER_DATA (inbuf), GST_BUFFER_SIZE (inbuf));
      priv = gst_kms_buffer_priv (sink, buf);
    }
    if (!priv)
      goto add_fb2_failed;
  }

  ret = drmModeSetPlane (sink->fd, sink->plane->plane_id,
      sink->conn.crtc, priv->fb_id, 0,
      /* make video fullscreen: */
      sink->dst_rect.x, sink->dst_rect.y, sink->dst_rect.w, sink->dst_rect.h,
      /* source/cropping coordinates are given in Q16 */
      sink->src_rect.x << 16, sink->src_rect.y << 16,
      sink->src_rect.w << 16, sink->src_rect.h << 16);
  if (ret)
    goto set_plane_failed;

out:
  GST_INFO_OBJECT (sink, "exit");
  /* TODO: we probably want to unref after displaying the *next* frame */
  gst_buffer_unref (buf);
  return flow_ret;

add_fb2_failed:
  GST_ELEMENT_ERROR (sink, RESOURCE, FAILED,
      (NULL), ("drmModeAddFB2 failed: %s (%d)", strerror (errno), errno));
  flow_ret = GST_FLOW_ERROR;
  goto out;

set_plane_failed:
  GST_ELEMENT_ERROR (sink, RESOURCE, FAILED,
      (NULL), ("drmModeSetPlane failed: %s (%d)", strerror (errno), errno));
  flow_ret = GST_FLOW_ERROR;
  goto out;

connector_not_found:
  GST_ELEMENT_ERROR (sink, RESOURCE, NOT_FOUND,
      (NULL), ("connector not found", strerror (errno), errno));
  goto out;
}


static gboolean
gst_kms_sink_event (GstBaseSink * bsink, GstEvent * event)
{
  GstKMSSink *sink = GST_KMS_SINK (bsink);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CROP:{
      gint left, top, width, height;
      GstStructure *structure;
      GstMessage *message;
      GstVideoRectangle *c = &sink->src_rect;

      gst_event_parse_crop (event, &top, &left, &width, &height);

      c->y = top;
      c->x = left;

      if (width == -1)
        width = GST_VIDEO_SINK_WIDTH (sink);
      if (height == -1)
        height = GST_VIDEO_SINK_HEIGHT (sink);

      c->w = width;
      c->h = height;

      structure = gst_structure_new ("video-size-src_rect", "width", G_TYPE_INT,
          width, "height", G_TYPE_INT, height, NULL);
      message = gst_message_new_application (GST_OBJECT (sink), structure);
      gst_bus_post (gst_element_get_bus (GST_ELEMENT (sink)), message);

      if (!gst_kms_sink_calculate_aspect_ratio (sink, width, height,
              sink->par_n, sink->par_d))
        return FALSE;

      break;
    }
    default:
      break;
  }
  if (GST_BASE_SINK_CLASS (gst_kms_sink_parent_class)->event)
    return GST_BASE_SINK_CLASS (gst_kms_sink_parent_class)->event (bsink,
        event);
  else
    return TRUE;
}

static void
gst_kms_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstKMSSink *sink;

  g_return_if_fail (GST_IS_KMS_SINK (object));

  sink = GST_KMS_SINK (object);

  switch (prop_id) {
    case PROP_FORCE_ASPECT_RATIO:
      sink->keep_aspect = g_value_get_boolean (value);
      break;
    case PROP_SCALE:
      sink->scale = g_value_get_boolean (value);
      break;
    case PROP_CONNECTOR:
      sink->conn.id = g_value_get_int (value);
      break;
    case PROP_PIXEL_ASPECT_RATIO:
    {
      GValue *tmp;

      tmp = g_new0 (GValue, 1);
      g_value_init (tmp, GST_TYPE_FRACTION);

      if (!g_value_transform (value, tmp)) {
        GST_WARNING_OBJECT (sink, "Could not transform string to aspect ratio");
        g_free (tmp);
      } else {
        sink->par_n = gst_value_get_fraction_numerator (tmp);
        sink->par_d = gst_value_get_fraction_denominator (tmp);
        GST_DEBUG_OBJECT (sink, "set PAR to %d/%d", sink->par_n, sink->par_d);
      }
    }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_kms_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstKMSSink *sink;

  g_return_if_fail (GST_IS_KMS_SINK (object));

  sink = GST_KMS_SINK (object);

  switch (prop_id) {
    case PROP_FORCE_ASPECT_RATIO:
      g_value_set_boolean (value, sink->keep_aspect);
      break;
    case PROP_SCALE:
      g_value_set_boolean (value, sink->scale);
      break;
    case PROP_CONNECTOR:
      g_value_set_int (value, sink->conn.id);
      break;
    case PROP_PIXEL_ASPECT_RATIO:
    {
      char *v = g_strdup_printf ("%d/%d", sink->par_n, sink->par_d);
      g_value_take_string (value, v);
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_kms_sink_reset (GstKMSSink * sink)
{
  memset (&sink->conn, 0, sizeof (struct connector));

  if (sink->pool) {
    gst_drm_buffer_pool_destroy (sink->pool);
    sink->pool = NULL;
  }

  if (sink->plane) {
    drmModeFreePlane (sink->plane);
    sink->plane = NULL;
  }

  if (sink->plane_resources) {
    drmModeFreePlaneResources (sink->plane_resources);
    sink->plane_resources = NULL;
  }

  if (sink->resources) {
    drmModeFreeResources (sink->resources);
    sink->resources = NULL;
  }

  if (sink->dev) {
    omap_device_del (sink->dev);
    sink->dev = NULL;
  }

  if (sink->fd != -1) {
    close (sink->fd);
    sink->fd = -1;
  }

  sink->par_n = sink->par_d = 1;
  sink->src_rect.x = 0;
  sink->src_rect.y = 0;
  sink->src_rect.w = 0;
  sink->src_rect.h = 0;
  sink->input_width = 0;
  sink->input_height = 0;
  sink->format = GST_VIDEO_FORMAT_UNKNOWN;

  memset (&sink->src_rect, 0, sizeof (GstVideoRectangle));
  memset (&sink->dst_rect, 0, sizeof (GstVideoRectangle));
}

static gboolean
gst_kms_sink_start (GstBaseSink * bsink)
{
  GstKMSSink *sink;
  const gchar *context_types[] = { "drm-fd", NULL };

  sink = GST_KMS_SINK (bsink);

  gst_video_context_prepare (GST_VIDEO_CONTEXT (sink), context_types);
  if (sink->fd == -1) {
    sink->fd = drmOpen ("omapdrm", NULL);
    if (sink->fd < 0)
      goto open_failed;
  }

  sink->dev = omap_device_new (sink->fd);
  if (sink->dev == NULL)
    goto device_failed;

  sink->resources = drmModeGetResources (sink->fd);
  if (sink->resources == NULL)
    goto resources_failed;

  sink->plane_resources = drmModeGetPlaneResources (sink->fd);
  if (sink->plane_resources == NULL)
    goto plane_resources_failed;

  return TRUE;

fail:
  gst_kms_sink_reset (sink);
  return FALSE;

open_failed:
  GST_ELEMENT_ERROR (sink, RESOURCE, FAILED,
      (NULL), ("drmOpen failed: %s (%d)", strerror (errno), errno));
  goto fail;

device_failed:
  GST_ELEMENT_ERROR (sink, RESOURCE, FAILED,
      (NULL), ("omap_device_new failed"));
  goto fail;

resources_failed:
  GST_ELEMENT_ERROR (sink, RESOURCE, FAILED,
      (NULL), ("drmModeGetResources failed: %s (%d)", strerror (errno), errno));
  goto fail;

plane_resources_failed:
  GST_ELEMENT_ERROR (sink, RESOURCE, FAILED,
      (NULL), ("drmModeGetPlaneResources failed: %s (%d)",
          strerror (errno), errno));
  goto fail;
}

static gboolean
gst_kms_sink_stop (GstBaseSink * bsink)
{
  GstKMSSink *sink;

  sink = GST_KMS_SINK (bsink);
  gst_kms_sink_reset (sink);

  return TRUE;
}

static GstFlowReturn
gst_kms_sink_buffer_alloc (GstBaseSink * bsink, guint64 offset, guint size,
    GstCaps * caps, GstBuffer ** buf)
{
  GstKMSSink *sink;
  GstFlowReturn ret = GST_FLOW_OK;

  sink = GST_KMS_SINK (bsink);

  GST_DEBUG_OBJECT (sink, "begin");

  if (G_UNLIKELY (!caps)) {
    GST_WARNING_OBJECT (sink, "have no caps, doing fallback allocation");
    *buf = NULL;
    ret = GST_FLOW_OK;
    goto beach;
  }

  GST_LOG_OBJECT (sink,
      "a buffer of %d bytes was requested with caps %" GST_PTR_FORMAT
      " and offset %" G_GUINT64_FORMAT, size, caps, offset);

  /* initialize the buffer pool if not initialized yet */
  if (G_UNLIKELY (!sink->pool ||
      gst_drm_buffer_pool_size (sink->pool) != size)) {
    GstVideoFormat format;
    gint width, height;

    if (sink->pool) {
      GST_INFO_OBJECT (sink, "in buffer alloc, pool->size != size");
      gst_drm_buffer_pool_destroy (sink->pool);
      sink->pool = NULL;
    }

    gst_video_format_parse_caps (caps, &format, &width, &height);
    size = gst_video_format_get_size (format, width, height);
    sink->pool = gst_drm_buffer_pool_new (GST_ELEMENT (sink),
        sink->fd, caps, size);
  }
  *buf = GST_BUFFER_CAST (gst_drm_buffer_pool_get (sink->pool, FALSE));

beach:
  return ret;
}

static void
gst_kms_sink_set_video_context (GstVideoContext * context,
    const gchar * type, const GValue * value)
{
  GstKMSSink *sink = GST_KMS_SINK (context);

  sink->fd = g_value_get_int (value);
}

static void
gst_kms_sink_video_context_interface_init (GstVideoContextInterface * iface)
{
  iface->set_context = gst_kms_sink_set_video_context;
}

static void
gst_kms_sink_finalize (GObject * object)
{
  GstKMSSink *sink;

  sink = GST_KMS_SINK (object);
  gst_kms_sink_reset (sink);

  G_OBJECT_CLASS (gst_kms_sink_parent_class)->finalize (object);
}

static void
gst_kms_sink_init (GstKMSSink * sink)
{
  sink->fd = -1;
  gst_kms_sink_reset (sink);
}

static void
gst_kms_sink_class_init (GstKMSSinkClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseSinkClass *gstbasesink_class;
  GstVideoSinkClass *videosink_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstbasesink_class = (GstBaseSinkClass *) klass;
  videosink_class = (GstVideoSinkClass *) klass;

  gobject_class->finalize = gst_kms_sink_finalize;
  gobject_class->set_property = gst_kms_sink_set_property;
  gobject_class->get_property = gst_kms_sink_get_property;

  g_object_class_install_property (gobject_class, PROP_FORCE_ASPECT_RATIO,
      g_param_spec_boolean ("force-aspect-ratio", "Force aspect ratio",
          "When enabled, reverse caps negotiation (scaling) will respect "
          "original aspect ratio", FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_PIXEL_ASPECT_RATIO,
      g_param_spec_string ("pixel-aspect-ratio", "Pixel Aspect Ratio",
          "The pixel aspect ratio of the device", "1/1",
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_SCALE,
      g_param_spec_boolean ("scale", "Scale",
          "When true, scale to render fullscreen", FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_CONNECTOR,
      g_param_spec_int ("connector", "Connector",
          "DRM connector id", 1, G_MAXINT32, 7,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT));

  gst_element_class_set_details_simple (gstelement_class,
      "Video sink", "Sink/Video",
      "A video sink using the linux kernel mode setting API",
      "Alessandro Decina <alessandro.d@gmail.com>");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_kms_sink_template_factory));

  gstbasesink_class->set_caps = GST_DEBUG_FUNCPTR (gst_kms_sink_setcaps);
  gstbasesink_class->get_times = GST_DEBUG_FUNCPTR (gst_kms_sink_get_times);
  gstbasesink_class->event = GST_DEBUG_FUNCPTR (gst_kms_sink_event);
  gstbasesink_class->query = GST_DEBUG_FUNCPTR (gst_kms_sink_query);
  gstbasesink_class->start = GST_DEBUG_FUNCPTR (gst_kms_sink_start);
  gstbasesink_class->stop = GST_DEBUG_FUNCPTR (gst_kms_sink_stop);
  gstbasesink_class->buffer_alloc =
      GST_DEBUG_FUNCPTR (gst_kms_sink_buffer_alloc);
  /* disable preroll as it's called before GST_CROP_EVENT has been received, so
   * we end up configuring the wrong mode... (based on padded caps)
   */
  gstbasesink_class->preroll = NULL;
  videosink_class->show_frame = GST_DEBUG_FUNCPTR (gst_kms_sink_show_frame);
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  if (!gst_element_register (plugin, "kmssink",
          GST_RANK_SECONDARY, GST_TYPE_KMS_SINK))
    return FALSE;

  GST_DEBUG_CATEGORY_INIT (gst_debug_kms_sink, "kmssink", 0, "kmssink element");

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "kms",
    "KMS video output element",
    plugin_init, VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
