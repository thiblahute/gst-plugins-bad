/*
 * GStreamer
 *
 * Copyright (C) 2012 Texas Instruments
 * Copyright (C) 2012 Collabora Ltd
 *
 * Authors:
 *  Alessandro Decina <alessandro.decina@collabora.co.uk>
 *  Rob Clark <rob.clark@linaro.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* for XkbKeycodeToKeysym */
#include <X11/XKBlib.h>

#include <gst/video/gstvideosink.h>
#include <gst/video/videooverlay.h>
#include <gst/video/navigation.h>
#include <gst/gstinfo.h>

/* Metas */
#include <sys/drm/gstdrmmeta.h>
#include <sys/dma/gstdmabufmeta.h>


#include "gstdri2videosink.h"
#include "gstdri2bufferpool.h"

static void gst_dri2videosink_reset (GstDRI2VideoSink * self);
static void gst_dri2videosink_expose (GstVideoOverlay * overlay);
static void gst_dri2videosink_set_event_handling (GstVideoOverlay * overlay,
    gboolean handle_events);

/* TODO we can get supported color formats from xserver */
static GstStaticPadTemplate gst_dri2videosink_sink_template_factory =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw, "
        "format = (string){NV12, I420, YUY2, UYVY}, "
        "width = [1, 2048], "
        "height = [1, 2048], " "framerate = " GST_VIDEO_FPS_RANGE)
    );

enum
{
  PROP_0,
  PROP_FORCE_ASPECT_RATIO,
  PROP_WINDOW_WIDTH,
  PROP_WINDOW_HEIGHT
};

/* ============================================================= */
/*                                                               */
/*                       Public Methods                          */
/*                                                               */
/* ============================================================= */

/* =========================================== */
/*                                             */
/*          Object typing & Creation           */
/*                                             */
/* =========================================== */
static void gst_dri2videosink_navigation_init (GstNavigationInterface * iface);
static void gst_dri2videosink_video_overlay_init (GstVideoOverlayInterface *
    iface);

#define gst_dri2videosink_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstDRI2VideoSink, gst_dri2videosink,
    GST_TYPE_VIDEO_SINK, G_IMPLEMENT_INTERFACE (GST_TYPE_NAVIGATION,
        gst_dri2videosink_navigation_init);
    G_IMPLEMENT_INTERFACE (GST_TYPE_VIDEO_OVERLAY,
        gst_dri2videosink_video_overlay_init));

/* ============================================================= */
/*                                                               */
/*                       Private Methods                         */
/*                                                               */
/* ============================================================= */
/* call w/ x_lock held */
static gboolean
gst_dri2videosink_xwindow_update_geometry (GstDRI2VideoSink * self)
{
  GST_DRI2CONTEXT_LOCK_X (self->dcontext);
  /* Update the window geometry */
  if (G_UNLIKELY (self->xwindow == NULL))
    goto beach;

  if (gst_dri2window_update_geometry (self->xwindow)) {
    if (!self->have_render_rect) {
      self->render_rect.x = self->render_rect.y = 0;
      self->render_rect.w = self->xwindow->width;
      self->render_rect.h = self->xwindow->height;
    }
    GST_DRI2CONTEXT_UNLOCK_X (self->dcontext);

    gst_pad_push_event (GST_BASE_SINK_PAD (self), gst_event_new_reconfigure ());

    return TRUE;
  }

beach:
  GST_DRI2CONTEXT_UNLOCK_X (self->dcontext);
  return FALSE;
}

/* This function handles XEvents that might be in the queue. It generates
   GstEvent that will be sent upstream in the pipeline to handle interactivity
   and navigation. It will also listen for configure events on the window to
   trigger caps renegotiation so on the fly software scaling can work. */
static void
gst_dri2videosink_handle_xevents (GstDRI2VideoSink * self)
{
  Display *dpy = self->dcontext->x_display;
  Window win = self->xwindow->window;
  XEvent e;
  gboolean exposed = FALSE;
  gboolean configured = FALSE;
  guint pointer_x = 0, pointer_y = 0;
  gboolean pointer_moved = FALSE;

  GST_DRI2VIDEOSINK_LOCK_FLOW (self);
  GST_DRI2CONTEXT_LOCK_X (self->dcontext);

  /* First get all pointer motion events, only the last position is
   * interesting so throw out the earlier ones:
   */
  while (XCheckWindowEvent (dpy, win, PointerMotionMask, &e)) {
    switch (e.type) {
      case MotionNotify:
        pointer_x = e.xmotion.x;
        pointer_y = e.xmotion.y;
        pointer_moved = TRUE;
        break;
      default:
        break;
    }
  }

  if (pointer_moved) {
    GST_DEBUG_OBJECT (self,
        "pointer moved over window at %d,%d", pointer_x, pointer_y);
    GST_DRI2CONTEXT_UNLOCK_X (self->dcontext);
    gst_navigation_send_mouse_event (GST_NAVIGATION (self),
        "mouse-move", 0, e.xbutton.x, e.xbutton.y);
    GST_DRI2CONTEXT_LOCK_X (self->dcontext);
  }

  /* Then handle all the other events: */
  while (XCheckWindowEvent (self->dcontext->x_display,
          self->xwindow->window,
          ExposureMask | StructureNotifyMask |
          KeyPressMask | KeyReleaseMask |
          ButtonPressMask | ButtonReleaseMask, &e)) {
    KeySym keysym;
    const char *key_str = NULL;

    GST_DRI2CONTEXT_UNLOCK_X (self->dcontext);

    switch (e.type) {
      case Expose:
        exposed = TRUE;
        self->exposed = TRUE;
        break;
      case MapNotify:
        exposed = TRUE;
        self->exposed = TRUE;
        break;
      case UnmapNotify:
        self->exposed = FALSE;
        break;
      case ConfigureNotify:
        gst_dri2videosink_xwindow_update_geometry (self);
        configured = TRUE;
        break;
      case ButtonPress:
        GST_DEBUG_OBJECT (self,
            "button %d pressed over window at %d,%d",
            e.xbutton.button, e.xbutton.x, e.xbutton.y);
        gst_navigation_send_mouse_event (GST_NAVIGATION (self),
            "mouse-button-press", e.xbutton.button, e.xbutton.x, e.xbutton.y);
        break;
      case ButtonRelease:
        GST_DEBUG_OBJECT (self,
            "button %d released over window at %d,%d", e.xbutton.button,
            e.xbutton.x, e.xbutton.y);
        gst_navigation_send_mouse_event (GST_NAVIGATION (self),
            "mouse-button-release", e.xbutton.button, e.xbutton.x, e.xbutton.y);
        break;
      case KeyPress:
      case KeyRelease:
        GST_DRI2CONTEXT_LOCK_X (self->dcontext);
        keysym = XkbKeycodeToKeysym (dpy, e.xkey.keycode, 0, 0);
        if (keysym != NoSymbol) {
          key_str = XKeysymToString (keysym);
        } else {
          key_str = "unknown";
        }
        GST_DRI2CONTEXT_UNLOCK_X (self->dcontext);
        GST_DEBUG_OBJECT (self,
            "key %d pressed over window at %d,%d (%s)",
            e.xkey.keycode, e.xkey.x, e.xkey.y, key_str);
        gst_navigation_send_key_event (GST_NAVIGATION (self),
            e.type == KeyPress ? "key-press" : "key-release", key_str);
        break;
      default:
        GST_DEBUG_OBJECT (self, "unhandled X event (%d)", e.type);
        break;
    }

    GST_DRI2CONTEXT_LOCK_X (self->dcontext);
  }

  if (exposed || configured) {
    GST_DRI2CONTEXT_UNLOCK_X (self->dcontext);
    GST_DRI2VIDEOSINK_UNLOCK_FLOW (self);

    gst_dri2videosink_expose (GST_VIDEO_OVERLAY (self));

    GST_DRI2VIDEOSINK_LOCK_FLOW (self);
    GST_DRI2CONTEXT_LOCK_X (self->dcontext);
  }

  /* Handle Display events */
  while (XPending (self->dcontext->x_display)) {
    XNextEvent (self->dcontext->x_display, &e);

    switch (e.type) {
      case ClientMessage:{
        Atom wm_delete;

        wm_delete = XInternAtom (self->dcontext->x_display,
            "WM_DELETE_WINDOW", True);
        if (wm_delete != None && wm_delete == (Atom) e.xclient.data.l[0]) {
          /* Handle window deletion by posting an error on the bus */
          GST_ELEMENT_ERROR (self, RESOURCE, NOT_FOUND,
              ("Output window was closed"), (NULL));

          GST_DRI2CONTEXT_UNLOCK_X (self->dcontext);
          gst_dri2window_delete (self->xwindow);
          self->xwindow = NULL;
          GST_DRI2CONTEXT_LOCK_X (self->dcontext);
        }
        break;
      }
      default:
        break;
    }
  }

  GST_DRI2CONTEXT_UNLOCK_X (self->dcontext);
  GST_DRI2VIDEOSINK_UNLOCK_FLOW (self);
}

static gpointer
gst_dri2videosink_event_thread (GstDRI2VideoSink * self)
{
  GST_OBJECT_LOCK (self);
  while (self->running) {
    GST_OBJECT_UNLOCK (self);

    if (self->xwindow) {
      gst_dri2videosink_handle_xevents (self);
    }
    g_usleep (G_USEC_PER_SEC / 20);

    GST_OBJECT_LOCK (self);
  }
  GST_OBJECT_UNLOCK (self);

  return NULL;
}

static void
gst_dri2videosink_manage_event_thread (GstDRI2VideoSink * self)
{
  GThread *thread = NULL;

  /* don't start the thread too early */
  if (self->dcontext == NULL) {
    return;
  }

  GST_OBJECT_LOCK (self);
  if (!self->event_thread) {
    /* Setup our event listening thread */
    GST_DEBUG_OBJECT (self, "run xevent thread");
    self->running = TRUE;
    self->event_thread = g_thread_create (
        (GThreadFunc) gst_dri2videosink_event_thread, self, TRUE, NULL);
  }
  GST_OBJECT_UNLOCK (self);

  /* Wait for our event thread to finish */
  if (thread)
    g_thread_join (thread);
}

static void
gst_dri2videosink_xwindow_set_title (GstDRI2VideoSink * self,
    GstDRI2Window * xwindow, const gchar * media_title)
{
  if (media_title) {
    g_free (self->media_title);
    self->media_title = g_strdup (media_title);
  }
  if (xwindow) {
    /* we have a window */
    if (xwindow->internal) {
      XTextProperty xproperty;
      const gchar *app_name;
      const gchar *title = NULL;
      gchar *title_mem = NULL;

      /* set application name as a title */
      app_name = g_get_application_name ();

      if (app_name && self->media_title) {
        title = title_mem = g_strconcat (self->media_title, " : ",
            app_name, NULL);
      } else if (app_name) {
        title = app_name;
      } else if (self->media_title) {
        title = self->media_title;
      }

      if (title) {
        if ((XStringListToTextProperty (((char **) &title), 1,
                    &xproperty)) != 0) {
          XSetWMName (self->dcontext->x_display, xwindow->window, &xproperty);
          XFree (xproperty.value);
        }

        g_free (title_mem);
      }
    }
  }
}

static GstDRI2Window *
gst_dri2videosink_create_window (GstDRI2VideoSink * self, gint width,
    gint height)
{
  GstDRI2Window *xwindow;

  GST_DEBUG_OBJECT (self, "begin");

  xwindow = gst_dri2window_new (self->dcontext, width, height);

  GST_DRI2CONTEXT_LOCK_X (self->dcontext);
  gst_dri2videosink_xwindow_set_title (self, xwindow, NULL);
  GST_DRI2CONTEXT_UNLOCK_X (self->dcontext);
  gst_dri2videosink_xwindow_update_geometry (self);

  self->exposed = TRUE;

  GST_DEBUG_OBJECT (self, "end");

  return xwindow;
}

static inline GstDRI2Window *
gst_dri2videosink_get_window (GstDRI2VideoSink * self)
{
  if (!self->xwindow) {
    self->xwindow = gst_dri2videosink_create_window (self,
        GST_VIDEO_SINK_WIDTH (self), GST_VIDEO_SINK_HEIGHT (self));
  }
  return self->xwindow;
}

/* Element stuff */

static inline gboolean
gst_dri2videosink_configure_overlay (GstDRI2VideoSink * self, gint width,
    gint height, gint video_par_n, gint video_par_d, gint display_par_n,
    gint display_par_d)
{
  guint calculated_par_n;
  guint calculated_par_d;

  if (!gst_video_calculate_display_ratio (&calculated_par_n, &calculated_par_d,
          width, height, video_par_n, video_par_d, display_par_n,
          display_par_d)) {
    GST_ELEMENT_ERROR (self, CORE, NEGOTIATION, (NULL),
        ("Error calculating the output display ratio of the video."));
    return FALSE;
  }

  GST_DEBUG_OBJECT (self,
      "video width/height: %dx%d, calculated display ratio: %d/%d",
      width, height, calculated_par_n, calculated_par_d);

  /* now find a width x height that respects this display ratio.
   * prefer those that have one of w/h the same as the incoming video
   * using wd / hd = calculated_pad_n / calculated_par_d */

  /* start with same height, because of interlaced video */
  /* check hd / calculated_par_d is an integer scale factor, and scale wd with the PAR */
  if (height % calculated_par_d == 0) {
    GST_DEBUG_OBJECT (self, "keeping video height");
    GST_VIDEO_SINK_WIDTH (self) = (guint)
        gst_util_uint64_scale_int (height, calculated_par_n, calculated_par_d);
    GST_VIDEO_SINK_HEIGHT (self) = height;
  } else if (width % calculated_par_n == 0) {
    GST_DEBUG_OBJECT (self, "keeping video width");
    GST_VIDEO_SINK_WIDTH (self) = width;
    GST_VIDEO_SINK_HEIGHT (self) = (guint)
        gst_util_uint64_scale_int (width, calculated_par_d, calculated_par_n);
  } else {
    GST_DEBUG_OBJECT (self, "approximating while keeping video height");
    GST_VIDEO_SINK_WIDTH (self) = (guint)
        gst_util_uint64_scale_int (height, calculated_par_n, calculated_par_d);
    GST_VIDEO_SINK_HEIGHT (self) = height;
  }
  GST_DEBUG_OBJECT (self, "scaling to %dx%d",
      GST_VIDEO_SINK_WIDTH (self), GST_VIDEO_SINK_HEIGHT (self));

  return TRUE;
}

static gboolean
gst_dri2videosink_setcaps (GstBaseSink * bsink, GstCaps * caps)
{
  GstVideoInfo info;

  GstDRI2VideoSink *self = GST_DRI2VIDEOSINK (bsink);

  GST_DEBUG_OBJECT (self, "In setcaps. Current caps %"
      GST_PTR_FORMAT ", setting caps %" GST_PTR_FORMAT,
      self->current_caps, caps);

  if (self->current_caps) {
    if (gst_caps_is_equal (self->current_caps, caps)) {
      GST_DEBUG_OBJECT (self, "caps are equal!");
      return TRUE;
    }

    GST_DEBUG_OBJECT (self, "caps are different");
  }

  if (!gst_video_info_from_caps (&info, caps))
    goto invalid_format;

  self->video_width = info.width;
  self->video_height = info.height;

  /* get video's pixel-aspect-ratio */
  self->video_par_n = info.par_n;
  self->video_par_d = info.par_d;

  /* get display's pixel-aspect-ratio */
  if (self->display_par) {
    self->display_par_n = gst_value_get_fraction_numerator (self->display_par);
    self->display_par_d =
        gst_value_get_fraction_denominator (self->display_par);
  } else {
    self->display_par_n = 1;
    self->display_par_d = 1;
  }

  if (!gst_dri2videosink_configure_overlay (self, info.width, info.height,
          self->video_par_n, self->video_par_d,
          self->display_par_n, self->display_par_d))
    return FALSE;

  /* Notify application to set xwindow id now */
  GST_DRI2VIDEOSINK_LOCK_FLOW (self);
  if (!self->xwindow) {
    GST_DRI2VIDEOSINK_UNLOCK_FLOW (self);
    gst_video_overlay_prepare_window_handle (GST_VIDEO_OVERLAY (self));
  } else {
    GST_DRI2VIDEOSINK_UNLOCK_FLOW (self);
  }

  /* Creating our window and our image with the display size in pixels */
  if (GST_VIDEO_SINK_WIDTH (bsink) <= 0 || GST_VIDEO_SINK_HEIGHT (bsink) <= 0)
    goto no_display_size;

  GST_DRI2VIDEOSINK_LOCK_FLOW (self);

  if (!gst_dri2window_create_pool (gst_dri2videosink_get_window (self), &info,
          caps))
    goto config_failed;

  GST_DRI2VIDEOSINK_UNLOCK_FLOW (self);

  gst_dri2videosink_set_event_handling (GST_VIDEO_OVERLAY (self), TRUE);

  gst_caps_replace (&self->current_caps, caps);

  return TRUE;

  /* ERRORS */
invalid_format:
  {
    GST_DEBUG_OBJECT (self,
        "Could not locate image format from caps %" GST_PTR_FORMAT, caps);
    return FALSE;
  }
no_display_size:
  {
    GST_ELEMENT_ERROR (self, CORE, NEGOTIATION, (NULL),
        ("Error calculating the output display ratio of the video."));
    return FALSE;
  }
config_failed:
  {
    GST_ERROR_OBJECT (self, "failed to set config.");
    GST_DRI2VIDEOSINK_UNLOCK_FLOW (self);
    return FALSE;
  }
}

static GstCaps *
gst_dri2videosink_getcaps (GstBaseSink * bsink, GstCaps * filter)
{
  GstCaps *caps;
  GstDRI2VideoSink *dri2sink = GST_DRI2VIDEOSINK (bsink);

  caps = gst_pad_get_pad_template_caps (GST_VIDEO_SINK_PAD (dri2sink));
  if (filter) {
    GstCaps *intersection;

    intersection = gst_caps_intersect_full (filter, caps,
        GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (caps);
    caps = intersection;
  }

  return caps;
}

static GstStateChangeReturn
gst_dri2videosink_change_state (GstElement * element, GstStateChange transition)
{
  GstDRI2VideoSink *self;
  GstDRI2Context *dcontext;
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;

  self = GST_DRI2VIDEOSINK (element);

  GST_DEBUG_OBJECT (self, "%s -> %s",
      gst_element_state_get_name (GST_STATE_TRANSITION_CURRENT (transition)),
      gst_element_state_get_name (GST_STATE_TRANSITION_NEXT (transition)));

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      if (self->dcontext == NULL) {
        dcontext = gst_dri2context_new (GST_ELEMENT (self));
        if (dcontext == NULL)
          return GST_STATE_CHANGE_FAILURE;
        GST_OBJECT_LOCK (self);
        self->dcontext = dcontext;
        GST_OBJECT_UNLOCK (self);
      }

      /* update object's pixel-aspect-ratio with calculated one */
      if (!self->display_par) {
        self->display_par = g_new0 (GValue, 1);
        gst_value_init_and_copy (self->display_par, self->dcontext->par);
        GST_DEBUG_OBJECT (self, "set calculated PAR on object's PAR");
      }

      gst_dri2videosink_manage_event_thread (self);
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      if (self->xwindow)
        gst_dri2window_set_pool_valid (self->xwindow, TRUE);
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      if (self->xwindow)
        gst_dri2window_set_pool_valid (self->xwindow, FALSE);
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      GST_VIDEO_SINK_WIDTH (self) = 0;
      GST_VIDEO_SINK_HEIGHT (self) = 0;
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      gst_dri2videosink_reset (self);
      break;
    default:
      break;
  }

  return ret;
}

static void
gst_dri2videosink_get_times (GstBaseSink * bsink, GstBuffer * buf,
    GstClockTime * start, GstClockTime * end)
{
  GstDRI2VideoSink *self;

  self = GST_DRI2VIDEOSINK (bsink);

  if (GST_BUFFER_TIMESTAMP_IS_VALID (buf)) {
    *start = GST_BUFFER_TIMESTAMP (buf);
    if (GST_BUFFER_DURATION_IS_VALID (buf)) {
      *end = *start + GST_BUFFER_DURATION (buf);
    } else {
      if (self->info.fps_n > 0) {
        *end = *start + gst_util_uint64_scale_int (GST_SECOND,
            self->info.fps_d, self->info.fps_n);
      }
    }
  }
}

static GstFlowReturn
gst_dri2videosink_show_frame (GstVideoSink * vsink, GstBuffer * buf)
{
  GstFlowReturn ret;
  GstDRI2Window *xwindow;
  GstDRI2VideoSink *self = GST_DRI2VIDEOSINK (vsink);

  g_return_val_if_fail (buf != NULL, GST_FLOW_ERROR);

  if (!self->exposed)
    return GST_FLOW_OK;

  GST_LOG_OBJECT (self, "render buffer: %p (%" GST_TIME_FORMAT ")",
      buf, GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buf)));

#if 0

  /* TODO Check that */
  if (!GST_IS_DRI2_BUFFER (buf)) {
    /* special case check for sub-buffers:  In certain cases, places like
     * GstBaseTransform, which might check that the buffer is writable
     * before copying metadata, timestamp, and such, will find that the
     * buffer has more than one reference to it.  In these cases, they
     * will create a sub-buffer with an offset=0 and length equal to the
     * original buffer size.
     *
     * This could happen in two scenarios: (1) a tee in the pipeline, and
     * (2) because the refcount is incremented in gst_mini_object_free()
     * before the finalize function is called, and decremented after it
     * returns..  but returning this buffer to the buffer pool in the
     * finalize function, could wake up a thread blocked in _buffer_alloc()
     * which could run and get a buffer w/ refcnt==2 before the thread
     * originally unref'ing the buffer returns from finalize function and
     * decrements the refcnt back to 1!
     */
    if (buf->parent &&
        (GST_BUFFER_DATA (buf) == GST_BUFFER_DATA (buf->parent)) &&
        (GST_BUFFER_SIZE (buf) == GST_BUFFER_SIZE (buf->parent))) {
      GST_DEBUG_OBJECT (self, "I have a sub-buffer!");
      return gst_dri2videosink_show_frame (vsink, buf->parent);
    }
  }
#endif

  GST_DRI2VIDEOSINK_LOCK_FLOW (self);

  xwindow = gst_dri2videosink_get_window (self);

  if (!xwindow) {
    GST_ERROR_OBJECT (self, "no drawable!");
    ret = GST_FLOW_ERROR;
    goto beach;
  }

  buf = gst_dri2window_buffer_prepare (xwindow, buf);
  if (buf == NULL) {
    ret = GST_FLOW_ERROR;

    goto beach;
  }
  ret = gst_dri2window_buffer_show (xwindow, buf);

  if (ret == GST_FLOW_OK) {
    gst_buffer_replace (&self->last_buf, self->display_buf);
    gst_buffer_replace (&self->display_buf, buf);
  }

  gst_buffer_unref (buf);

beach:
  GST_DRI2VIDEOSINK_UNLOCK_FLOW (self);

  return ret;
}


static gboolean
_propose_alloc (GstBaseSink * bsink, GstQuery * query)
{
  gsize size;
  GstCaps *caps;
  gboolean need_pool;
  GstBufferPool *pool;
  GstStructure *config;

  GstDRI2VideoSink *self = GST_DRI2VIDEOSINK (bsink);
  GstDRI2Window *xwindow = self->xwindow;

  gst_query_parse_allocation (query, &caps, &need_pool);

  if (caps == NULL)
    goto no_caps;

  GST_DRI2WINDOW_LOCK_POOL (xwindow);
  if ((pool = xwindow->buffer_pool))
    gst_object_ref (pool);
  GST_DRI2WINDOW_UNLOCK_POOL (xwindow);

  if (pool != NULL) {
    GstCaps *pcaps;

    /* we had a pool, check caps */
    GST_DEBUG_OBJECT (self, "check existing pool caps");
    config = gst_buffer_pool_get_config (pool);
    gst_buffer_pool_config_get_params (config, &pcaps, &size, NULL, NULL);

    if (!gst_caps_is_equal (caps, pcaps)) {
      GST_DEBUG_OBJECT (self, "pool has different caps");
      /* different caps, we can't use this pool */
      gst_object_unref (pool);
      pool = NULL;
    }
    gst_structure_free (config);
  }

  if (pool == NULL && need_pool) {
    GstVideoInfo info;

    if (!gst_video_info_from_caps (&info, caps))
      goto invalid_caps;

    GST_DEBUG_OBJECT (self, "create new pool");
    pool = gst_dri2_buffer_pool_new (xwindow, self->dcontext->drm_fd);

    /* the normal size of a frame */
    /* FIXME DRI2 on OMAP has a 32 quantization step for strides...
     * check if it is the right place?
     * FIXME: This looks suboptimal also.
     */
    gst_video_info_set_format (&info, info.finfo->format,
        GET_COMPATIBLE_STRIDE (info.finfo->format, info.width), info.height);

    config = gst_buffer_pool_get_config (pool);
    gst_buffer_pool_config_set_params (config, caps, info.size, 0, 0);
    if (!gst_buffer_pool_set_config (pool, config))
      goto config_failed;
  }

  if (pool) {
    GST_DEBUG_OBJECT (self, "We got a pool %" GST_PTR_FORMAT, pool);
    /* FIXME Why do we need at least 2 buffers? */
    gst_query_add_allocation_pool (query, pool, size, 2, 0);
    gst_object_unref (pool);
  }

  /* we also support various metadata */
  gst_query_add_allocation_meta (query, GST_DRM_META_API_TYPE, NULL);
  gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL);
  gst_query_add_allocation_meta (query, GST_DMA_BUF_META_API_TYPE, NULL);
  gst_query_add_allocation_meta (query, GST_VIDEO_CROP_META_API_TYPE, NULL);

  return TRUE;

no_caps:
  {
    GST_DEBUG_OBJECT (bsink, "no caps specified");
    return FALSE;
  }
invalid_caps:
  {
    GST_DEBUG_OBJECT (bsink, "invalid caps specified");
    return FALSE;
  }
config_failed:
  {
    GST_DEBUG_OBJECT (bsink, "failed setting config");
    gst_object_unref (pool);
    return FALSE;
  }
}

/* Interfaces stuff */

/*
 * GstVideoOverlay Interface:
 */

static void
gst_dri2videosink_set_window_handle (GstVideoOverlay * overlay, guintptr id)
{
  XID xwindow_id = id;
  GstDRI2VideoSink *self = GST_DRI2VIDEOSINK (overlay);

  g_return_if_fail (GST_IS_DRI2VIDEOSINK (self));
  GST_DRI2VIDEOSINK_LOCK_FLOW (self);

  /* If we already use that window return */
  if (self->xwindow && (xwindow_id == self->xwindow->window)) {
    GST_DRI2VIDEOSINK_UNLOCK_FLOW (self);
    return;
  }

  /* If the element has not initialized the X11 context try to do so */
  if (!(self->dcontext ||
          (self->dcontext = gst_dri2context_new (GST_ELEMENT (self))))) {
    GST_DRI2VIDEOSINK_UNLOCK_FLOW (self);
    /* we have thrown a GST_ELEMENT_ERROR now */
    return;
  }

  /* If a window is there already we destroy it */
  if (self->xwindow) {
    gst_dri2window_delete (self->xwindow);
    self->xwindow = NULL;
  }

  /* If the xid is 0 we will create an internal one in buffer_alloc */
  if (xwindow_id != 0) {
    self->xwindow = gst_dri2window_new_from_handle (self->dcontext, xwindow_id);
    gst_dri2videosink_xwindow_update_geometry (self);
  }

  GST_DRI2VIDEOSINK_UNLOCK_FLOW (self);

  gst_dri2videosink_set_event_handling (overlay, TRUE);
}

static void
gst_dri2videosink_expose (GstVideoOverlay * overlay)
{
  GstDRI2VideoSink *self = GST_DRI2VIDEOSINK (overlay);

  if (self->display_buf) {
    gst_dri2videosink_show_frame (GST_VIDEO_SINK (self), self->display_buf);
  }
}

static void
gst_dri2videosink_set_event_handling (GstVideoOverlay * overlay,
    gboolean handle_events)
{
  GstDRI2VideoSink *self = GST_DRI2VIDEOSINK (overlay);
  long event_mask;

  GST_DRI2VIDEOSINK_LOCK_FLOW (self);
  if (G_UNLIKELY (!self->xwindow)) {
    GST_DRI2VIDEOSINK_UNLOCK_FLOW (self);
    return;
  }

  GST_DRI2CONTEXT_LOCK_X (self->dcontext);
  event_mask = ExposureMask | StructureNotifyMask |
      PointerMotionMask | KeyPressMask | KeyReleaseMask;

  if (self->xwindow->internal) {
    event_mask |= ButtonPressMask | ButtonReleaseMask;
  }

  XSelectInput (self->dcontext->x_display, self->xwindow->window, event_mask);
  GST_DRI2CONTEXT_UNLOCK_X (self->dcontext);
  GST_DRI2VIDEOSINK_UNLOCK_FLOW (self);
}

static void
gst_dri2videosink_set_render_rectangle (GstVideoOverlay * overlay, gint x,
    gint y, gint width, gint height)
{
  GstDRI2VideoSink *self = GST_DRI2VIDEOSINK (overlay);

  /* FIXME: how about some locking? */
  if (width >= 0 && height >= 0) {
    self->render_rect.x = x;
    self->render_rect.y = y;
    self->render_rect.w = width;
    self->render_rect.h = height;
    self->have_render_rect = TRUE;
  } else {
    self->render_rect.x = 0;
    self->render_rect.y = 0;
    self->render_rect.w = self->xwindow->width;
    self->render_rect.h = self->xwindow->height;
    self->have_render_rect = FALSE;
  }
  GST_DEBUG_OBJECT (self, "render_rect is %d,%d - %dX%d",
      self->render_rect.x, self->render_rect.y,
      self->render_rect.w, self->render_rect.h);
}

static void
gst_dri2videosink_video_overlay_init (GstVideoOverlayInterface * iface)
{
  iface->set_window_handle = gst_dri2videosink_set_window_handle;
  iface->expose = gst_dri2videosink_expose;
  iface->handle_events = gst_dri2videosink_set_event_handling;
  iface->set_render_rectangle = gst_dri2videosink_set_render_rectangle;
}

/*
 * GstNavigation Interface:
 */

static void
gst_dri2videosink_send_event (GstNavigation * navigation,
    GstStructure * structure)
{
  GstDRI2VideoSink *self = GST_DRI2VIDEOSINK (navigation);
  GstPad *peer;

  if ((peer = gst_pad_get_peer (GST_VIDEO_SINK_PAD (self)))) {
    GstVideoRectangle result;
    gdouble x, y, xscale = 1.0, yscale = 1.0;

    if (self->keep_aspect) {
      GstVideoRectangle src = {
        .w = GST_VIDEO_SINK_WIDTH (self),
        .h = GST_VIDEO_SINK_HEIGHT (self),
      };
      GstVideoRectangle dst = {
        .w = self->render_rect.w,
        .h = self->render_rect.h,
      };

      gst_video_sink_center_rect (src, dst, &result, TRUE);
      result.x += self->render_rect.x;
      result.y += self->render_rect.y;
    } else {
      result = self->render_rect;
    }

    /* We calculate scaling using the original video frames geometry to
     * include pixel aspect ratio scaling.
     */
    xscale = (gdouble) self->video_width / result.w;
    yscale = (gdouble) self->video_height / result.h;

    /* Note: this doesn't account for crop top/left offsets.. which
     * is probably not quite right.. OTOH, I don't think the ducati
     * decoder elements subtract back out the crop offsets as the
     * event propagates upstream, so as long as the one receiving
     * the event is upstream of the decoder, the net effect will be
     * correct..  although this might be worth fixing correctly at
     * some point.
     */

    /* Converting pointer coordinates to the non scaled geometry */
    if (gst_structure_get_double (structure, "pointer_x", &x)) {
      x = MIN (x, result.x + result.w);
      x = MAX (x - result.x, 0);
      gst_structure_set (structure, "pointer_x", G_TYPE_DOUBLE,
          (gdouble) x * xscale, NULL);
    }
    if (gst_structure_get_double (structure, "pointer_y", &y)) {
      y = MIN (y, result.y + result.h);
      y = MAX (y - result.y, 0);
      gst_structure_set (structure, "pointer_y", G_TYPE_DOUBLE,
          (gdouble) y * yscale, NULL);
    }

    gst_pad_send_event (peer, gst_event_new_navigation (structure));
    gst_object_unref (peer);
  }
}

static void
gst_dri2videosink_navigation_init (GstNavigationInterface * iface)
{
  iface->send_event = gst_dri2videosink_send_event;
}

static void
gst_dri2videosink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstDRI2VideoSink *self;

  g_return_if_fail (GST_IS_DRI2VIDEOSINK (object));

  self = GST_DRI2VIDEOSINK (object);

  switch (prop_id) {
    case PROP_FORCE_ASPECT_RATIO:
      self->keep_aspect = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_dri2videosink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstDRI2VideoSink *self;

  g_return_if_fail (GST_IS_DRI2VIDEOSINK (object));

  self = GST_DRI2VIDEOSINK (object);

  switch (prop_id) {
    case PROP_FORCE_ASPECT_RATIO:
      g_value_set_boolean (value, self->keep_aspect);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_dri2videosink_reset (GstDRI2VideoSink * self)
{
  GThread *thread;

  GST_OBJECT_LOCK (self);
  self->running = FALSE;
  thread = self->event_thread;
  self->event_thread = NULL;
  GST_OBJECT_UNLOCK (self);

  if (thread)
    g_thread_join (thread);

  gst_buffer_replace (&self->last_buf, NULL);
  gst_buffer_replace (&self->display_buf, NULL);
  gst_caps_replace (&self->current_caps, NULL);

  self->render_rect.x = self->render_rect.y = 0;
  self->render_rect.w = self->render_rect.h = 0;
  self->have_render_rect = FALSE;

  if (self->xwindow) {
    gst_dri2window_delete (self->xwindow);
    self->xwindow = NULL;
  }

  g_free (self->display_par);
  self->display_par = NULL;
  GST_OBJECT_LOCK (self);
  self->dcontext = NULL;
  GST_OBJECT_UNLOCK (self);

  gst_video_info_init (&self->info);
}

static void
gst_dri2videosink_finalize (GObject * object)
{
  GstDRI2VideoSink *self;

  self = GST_DRI2VIDEOSINK (object);

  gst_dri2videosink_reset (self);

  g_mutex_clear (&self->flow_lock);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_dri2videosink_init (GstDRI2VideoSink * self)
{
  self->running = FALSE;

  self->video_width = 0;
  self->video_height = 0;

  g_mutex_init (&self->flow_lock);

  self->keep_aspect = FALSE;
  self->current_caps = NULL;
  self->dcontext = NULL;
  self->xwindow = NULL;
  self->display_buf = NULL;
  self->event_thread = NULL;
  self->display_par = NULL;
}

static void
gst_dri2videosink_class_init (GstDRI2VideoSinkClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstVideoSinkClass *videosink_class;
  GstBaseSinkClass *gstbasesink_class;


  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  videosink_class = (GstVideoSinkClass *) klass;
  gstbasesink_class = (GstBaseSinkClass *) klass;
  parent_class = g_type_class_peek_parent (klass);

  gobject_class->finalize = gst_dri2videosink_finalize;
  gobject_class->set_property = gst_dri2videosink_set_property;
  gobject_class->get_property = gst_dri2videosink_get_property;

  g_object_class_install_property (gobject_class, PROP_FORCE_ASPECT_RATIO,
      g_param_spec_boolean ("force-aspect-ratio", "Force aspect ratio",
          "When enabled, reverse caps negotiation (scaling) will respect "
          "original aspect ratio", FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));


  gst_element_class_set_static_metadata (gstelement_class,
      "DRI2 Video sink", "Sink/Video",
      "dri2videosink", "Rob Clark <rob@ti.com>");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_dri2videosink_sink_template_factory));

  gstelement_class->change_state = gst_dri2videosink_change_state;

  gstbasesink_class->propose_allocation = GST_DEBUG_FUNCPTR (_propose_alloc);
  gstbasesink_class->set_caps = GST_DEBUG_FUNCPTR (gst_dri2videosink_setcaps);
  gstbasesink_class->get_caps = GST_DEBUG_FUNCPTR (gst_dri2videosink_getcaps);
  gstbasesink_class->get_times =
      GST_DEBUG_FUNCPTR (gst_dri2videosink_get_times);

  videosink_class->show_frame =
      GST_DEBUG_FUNCPTR (gst_dri2videosink_show_frame);
}
