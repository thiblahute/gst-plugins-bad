/*
 * GStreamer
 *
 * Copyright (C) 2012 Texas Instruments
 *
 * Authors:
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

#include <ctype.h>

#include <gst/video/video-crop.h>

#include "gstdri2util.h"
#include "gstdri2bufferpool.h"

static GstMiniObjectClass *dri2window_parent_class = NULL;

static Bool WireToEvent (Display * dpy, XExtDisplayInfo * info,
    XEvent * event, xEvent * wire)
{
  switch ((wire->u.u.type & 0x7f) - info->codes->first_event) {

  case DRI2_BufferSwapComplete: {
    //    xDRI2BufferSwapComplete *awire = (xDRI2BufferSwapComplete *)wire;
    // TODO use this to know when the previous buffer is no longer visible..
    GST_LOG ("BufferSwapComplete");
    return True;
  }
  case DRI2_InvalidateBuffers: {
    //    xDRI2InvalidateBuffers *awire = (xDRI2InvalidateBuffers *)wire;
    GST_LOG ("InvalidateBuffers");
    //    dri2InvalidateBuffers(dpy, awire->drawable);
    return False;
  }
  default:
    /* client doesn't support server event */
    break;
  }

  return False;
}

static Status EventToWire (Display * dpy, XExtDisplayInfo * info,
    XEvent * event, xEvent * wire)
{
  switch (event->type) {
  default:
    /* client doesn't support server event */
    break;
  }

  return Success;
}

static const DRI2EventOps ops = {
    .WireToEvent = WireToEvent,
    .EventToWire = EventToWire,
};

static DRI2Buffer * get_buffer (GstDRI2Window * xwindow, guint attach,
    gint width, gint height, guint32 format);

static Bool is_fourcc(unsigned int val)
{
  char *str = (char *)&val;
  return isalnum(str[0]) && isalnum(str[1]) &&
      isalnum(str[2]) && isalnum(str[3]);
}

/*
 * GstDRI2DrawContext
 */

/* This function calculates the pixel aspect ratio based on the properties
 * in the xcontext structure and stores it there.
 */
static void
gst_dri2context_calculate_pixel_aspect_ratio (GstDRI2Context * dcontext)
{
  static const gint par[][2] = {
    {1, 1},                     /* regular screen */
    {16, 15},                   /* PAL TV */
    {11, 10},                   /* 525 line Rec.601 video */
    {54, 59},                   /* 625 line Rec.601 video */
    {64, 45},                   /* 1280x1024 on 16:9 display */
    {5, 3},                     /* 1280x1024 on 4:3 display */
    {4, 3}                      /* 800x600 on 16:9 display */
  };
  gint i;
  gint index;
  gdouble ratio;
  gdouble delta;

#define DELTA(idx) (ABS (ratio - ((gdouble) par[idx][0] / par[idx][1])))

  /* first calculate the "real" ratio; which is the "physical" w/h divided
   * by the w/h in pixels of the display
   *
   * TODO:
  ratio = (gdouble) (dcontext->physical_width * dcontext->display_height)
      / (dcontext->physical_height * dcontext->display_width);
   */

  /* XXX */
  ratio = 1;

  GST_DEBUG ("calculated pixel aspect ratio: %f", ratio);
  /* now find the one from par[][2] with the lowest delta to the real one */
  delta = DELTA (0);
  index = 0;

  for (i = 1; i < sizeof (par) / (sizeof (gint) * 2); ++i) {
    gdouble this_delta = DELTA (i);

    if (this_delta < delta) {
      index = i;
      delta = this_delta;
    }
  }

  GST_DEBUG ("Decided on index %d (%d/%d)", index,
      par[index][0], par[index][1]);

  if (dcontext->par) {
    g_value_unset (dcontext->par);
    g_free (dcontext->par);
  }
  dcontext->par = g_new0 (GValue, 1);
  g_value_init (dcontext->par, GST_TYPE_FRACTION);
  gst_value_set_fraction (dcontext->par, par[index][0], par[index][1]);
  GST_DEBUG ("set dcontext PAR to %d/%d",
      gst_value_get_fraction_numerator (dcontext->par),
      gst_value_get_fraction_denominator (dcontext->par));
}

GstDRI2Context *
gst_dri2context_new (GstElement * elem)
{
  GstDRI2Context *dcontext;
  Window root;
  drm_magic_t magic;
  int eventBase, errorBase, major, minor;
  unsigned int i, nformats, *formats = NULL;
  int fd = -1;

  dcontext = g_new0 (GstDRI2Context, 1);
  dcontext->elem = elem;
  dcontext->x_lock = g_mutex_new ();

  dcontext->x_display = XOpenDisplay (NULL);
  if (!dcontext->x_display) {
    GST_ERROR_OBJECT (elem, "Failed to open X display");
    goto fail;
  }

  if (!DRI2InitDisplay(dcontext->x_display, &ops)) {
    GST_ERROR_OBJECT (elem, "DRI2InitDisplay failed");
    goto fail;
  }

  if (!DRI2QueryExtension (dcontext->x_display, &eventBase, &errorBase)) {
    GST_ERROR_OBJECT (elem, "DRI2QueryExtension failed");
    goto fail;
  }

  GST_DEBUG_OBJECT (elem, "DRI2QueryExtension: "
      "eventBase=%d, errorBase=%d", eventBase, errorBase);

  if (!DRI2QueryVersion (dcontext->x_display, &major, &minor)) {
    GST_ERROR_OBJECT (elem, "DRI2QueryVersion failed");
    goto fail;
  }

  GST_DEBUG_OBJECT (elem, "DRI2QueryVersion: major=%d, minor=%d",
      major, minor);

  root = RootWindow (dcontext->x_display,
      DefaultScreen (dcontext->x_display));

  if (!DRI2Connect (dcontext->x_display, root,
      DRI2DriverXV, &dcontext->driver, &dcontext->device)) {
    GST_ERROR_OBJECT (elem, "DRI2Connect failed");
    goto fail;
  }

  GST_DEBUG_OBJECT (elem, "DRI2Connect: driver=%s, device=%s",
      dcontext->driver, dcontext->device);

  fd = open (dcontext->device, O_RDWR);
  if (fd < 0) {
    GST_ERROR_OBJECT (elem, "open failed");
    goto fail;
  }

  if (drmGetMagic (fd, &magic)) {
    GST_ERROR_OBJECT (elem, "drmGetMagic failed");
    goto fail;
  }

  if (!DRI2Authenticate (dcontext->x_display, root, magic)) {
    GST_ERROR_OBJECT (elem, "DRI2Authenticate failed");
    goto fail;
  }

  dcontext->drm_fd = fd;
  dcontext->dev = omap_device_new (fd);

  if (!DRI2GetFormats (dcontext->x_display, root, &nformats, &formats)) {
    GST_ERROR_OBJECT (elem, "DRI2GetFormats failed");
    goto fail;
  }

  if (nformats == 0) {
    GST_ERROR_OBJECT (elem, "no formats!");
    goto fail;
  }

  /* print out supported formats */
  GST_DEBUG_OBJECT (elem, "Found %d supported formats:", nformats);
  for (i = 0; i < nformats; i++) {
    if (is_fourcc(formats[i])) {
      GST_DEBUG_OBJECT (elem, "  %d: %08x (\"%.4s\")", i, formats[i],
          (char *)&formats[i]);
    } else {
      GST_DEBUG_OBJECT (elem, "  %d: %08x (device dependent)", i, formats[i]);
    }
  }

  free(formats);

  gst_dri2context_calculate_pixel_aspect_ratio (dcontext);

  dcontext->black = XBlackPixel (dcontext->x_display, dcontext->screen_num);

  return dcontext;

fail:
  free(formats);
  if (dcontext->dev)
    omap_device_del (dcontext->dev);

  /* TODO: the code in _delete uses drmClose, but the fd is from open(2) ?? */
  if (fd >= 0)
    drmClose (fd);

  g_mutex_free (dcontext->x_lock);
  g_free (dcontext);

  return NULL;
}

void
gst_dri2context_delete (GstDRI2Context *dcontext)
{
  if (dcontext->par) {
    g_value_unset (dcontext->par);
    g_free (dcontext->par);
  }

  g_mutex_lock (dcontext->x_lock);
  XCloseDisplay (dcontext->x_display);
  g_mutex_unlock (dcontext->x_lock);
  g_mutex_free (dcontext->x_lock);

  omap_device_del (dcontext->dev);
  drmClose (dcontext->drm_fd);

  XFree (dcontext->driver);
  XFree (dcontext->device);

  g_free (dcontext);
}

/*
 * GstDRI2Window
 */

/* NOTES:
 * at startup (or on first buffer allocation?) request front buffer..
 * otherwise I think we can do GetBuffers 1 at a time, w/ different
 * attachment points.. use width==0, height==0 to destroy the buffer
 * Keep the table of attachment->buffer globally, to handle resolution
 * changes.. the old bufferpool is torn down, but still goes via the
 * per video-sink table of attachments, because during the transition
 * period we could have some not-yet-displayed buffers at the previous
 * dimensions/format..
 */

GstDRI2Window *
gst_dri2window_new_from_handle (GstDRI2Context *dcontext, XID xwindow_id)
{
  GstDRI2Window *xwindow;
  XWindowAttributes attr;

  xwindow = (GstDRI2Window *)gst_mini_object_new (GST_TYPE_DRI2WINDOW);
  xwindow->dcontext = dcontext;
  xwindow->window = xwindow_id;
  xwindow->pool_lock = g_mutex_new ();
  xwindow->buffer_pool = NULL;
  xwindow->pool_valid = FALSE;

  /* Set the event we want to receive and create a GC */
  g_mutex_lock (dcontext->x_lock);

  XGetWindowAttributes (dcontext->x_display, xwindow->window,
      &attr);

  xwindow->width = attr.width;
  xwindow->height = attr.height;

  /* We have to do that to prevent X from redrawing the background on
   * ConfigureNotify. This takes away flickering of video when resizing. */
  XSetWindowBackgroundPixmap (dcontext->x_display,
      xwindow->window, None);

  XMapWindow (dcontext->x_display, xwindow->window);

  xwindow->gc = XCreateGC (dcontext->x_display,
      xwindow->window, 0, NULL);
  g_mutex_unlock (dcontext->x_lock);

  DRI2CreateDrawable (dcontext->x_display, xwindow->window);

  /* request the front buffer.. we don't need to keep it, just to
   * request it.. otherwise DRI2 core on xserver side gets miffed:
   *   [DRI2] swap_buffers: drawable has no back or front?
   */
  free (get_buffer (xwindow, DRI2BufferFrontLeft,
      xwindow->width, xwindow->height, 32));

  return xwindow;
}

GstDRI2Window *
gst_dri2window_new (GstDRI2Context * dcontext, gint width, gint height)
{
  GstDRI2Window *xwindow;
  Window root;
  Atom wm_delete;
  XID xwindow_id;

  g_mutex_lock (dcontext->x_lock);

  GST_DEBUG_OBJECT (dcontext->elem, "creating window: %dx%d", width, height);

  root = DefaultRootWindow (dcontext->x_display);
  xwindow_id = XCreateSimpleWindow (dcontext->x_display, root, 0, 0,
      width, height, 2, 2, dcontext->black);


  /* Tell the window manager we'd like delete client messages instead of
   * being killed */
  wm_delete = XInternAtom (dcontext->x_display, "WM_DELETE_WINDOW", True);
  if (wm_delete != None) {
    (void) XSetWMProtocols (dcontext->x_display, xwindow_id,
        &wm_delete, 1);
  }

  g_mutex_unlock (dcontext->x_lock);

  xwindow = gst_dri2window_new_from_handle (dcontext, xwindow_id);
  xwindow->internal = TRUE;

  return xwindow;
}

void
gst_dri2window_delete (GstDRI2Window * xwindow)
{
  g_mutex_lock (xwindow->pool_lock);
  xwindow->pool_valid = FALSE;
  if (xwindow->buffer_pool) {
    gst_drm_buffer_pool_destroy (xwindow->buffer_pool);
    xwindow->buffer_pool = NULL;
  }
  g_mutex_unlock (xwindow->pool_lock);

  gst_mini_object_unref (GST_MINI_OBJECT (xwindow));
}

static void
gst_dri2window_finalize (GstDRI2Window * xwindow)
{
  GstDRI2Context *dcontext = xwindow->dcontext;

  g_mutex_lock (xwindow->pool_lock);
  xwindow->pool_valid = FALSE;
  if (xwindow->buffer_pool) {
    gst_drm_buffer_pool_destroy (xwindow->buffer_pool);
    xwindow->buffer_pool = NULL;
  }
  g_mutex_unlock (xwindow->pool_lock);

  g_mutex_free (xwindow->pool_lock);

  g_mutex_lock (dcontext->x_lock);

  DRI2DestroyDrawable (dcontext->x_display, xwindow->window);

  /* If we did not create the window we just free the GC and let it live */
  if (xwindow->internal)
    XDestroyWindow (dcontext->x_display, xwindow->window);
  else
    XSelectInput (dcontext->x_display, xwindow->window, 0);

  XFreeGC (dcontext->x_display, xwindow->gc);

  XSync (dcontext->x_display, FALSE);

  // XXX free xwindow->dri2bufs
  // TODO we probably want xwindow to be a refcnt'd miniobj so we don't end w/
  // dri2buffer's referencing deleted xwindow's..

  g_mutex_unlock (dcontext->x_lock);

  GST_MINI_OBJECT_CLASS (dri2window_parent_class)->finalize (GST_MINI_OBJECT
      (xwindow));
}

/* call with x_lock held */
void
gst_dri2window_update_geometry (GstDRI2Window * xwindow)
{
  XWindowAttributes attr;

  XGetWindowAttributes (xwindow->dcontext->x_display,
      xwindow->window, &attr);

  xwindow->width  = attr.width;
  xwindow->height = attr.height;
}

void
gst_dri2window_set_pool_valid (GstDRI2Window * xwindow, gboolean valid)
{
  g_mutex_lock (xwindow->pool_lock);
  xwindow->pool_valid = valid;
  g_mutex_unlock (xwindow->pool_lock);
}

void
gst_dri2window_check_caps (GstDRI2Window * xwindow, GstCaps * caps)
{
  g_mutex_lock (xwindow->pool_lock);
  if (xwindow->buffer_pool) {
    if (gst_drm_buffer_pool_check_caps (xwindow->buffer_pool, caps)) {
      GST_INFO_OBJECT (xwindow->dcontext->elem, "caps change");
      gst_drm_buffer_pool_destroy (xwindow->buffer_pool);
      xwindow->buffer_pool = NULL;
    }
  }
  g_mutex_unlock (xwindow->pool_lock);
}

static inline gboolean
ok_buffer (GstDRI2Window * xwindow, GstBuffer * buf)
{
  return GST_IS_DRI2_BUFFER (buf) &&
      (GST_DRI2_BUFFER_POOL (GST_DRM_BUFFER (buf)->pool)->xwindow == xwindow);

}

GstFlowReturn
gst_dri2window_buffer_show (GstDRI2Window * xwindow, GstBuffer * buf)
{
  GstDRI2Context *dcontext = xwindow->dcontext;
  GstDRI2Buffer *dri2buf;
  GstVideoCrop *crop;
  CARD64 count;
  BoxRec b;

  if (! ok_buffer (xwindow, buf)) {
    GST_WARNING_OBJECT (dcontext->elem, "unexpected buffer: %p", buf);
    return GST_FLOW_UNEXPECTED;
  }

  dri2buf = GST_DRI2_BUFFER (buf);

  crop = gst_buffer_get_video_crop (buf);
  if (crop) {
    b.x1 = gst_video_crop_left (crop);
    b.y1 = gst_video_crop_top (crop);
    b.x2 = b.x1 + gst_video_crop_width (crop) - 1;
    b.y2 = b.y1 + gst_video_crop_height (crop) - 1;
  } else {
    b.x1 = 0;
    b.y1 = 0;
    b.x2 = GST_DRM_BUFFER (dri2buf)->pool->width - 1;
    b.y2 = GST_DRM_BUFFER (dri2buf)->pool->height - 1;
  }

  g_mutex_lock (dcontext->x_lock);
  DRI2SwapBuffersVid (dcontext->x_display, xwindow->window, 0, 0, 0,
      &count, dri2buf->dri2buf->attachment, &b);
  /* TODO: probably should wait for DRI2_BufferSwapComplete instead..
   * although that probably depends on someone making an x11 call to
   * dispatch the events
   */
  DRI2WaitSBC (dcontext->x_display, xwindow->window, count,
      /* just re-use count as a valid ptr.. we don't need ust/msc/sbc: */
      &count, &count, &count);
  g_mutex_unlock (dcontext->x_lock);

  return GST_FLOW_OK;
}

GstBuffer *
gst_dri2window_buffer_prepare (GstDRI2Window * xwindow, GstBuffer * buf)
{
  GstBuffer *newbuf = NULL;

  if (! ok_buffer (xwindow, buf)) {

    gst_dri2window_buffer_alloc (xwindow, GST_BUFFER_SIZE (buf),
        GST_BUFFER_CAPS (buf), &newbuf);

    if (newbuf) {
      GST_DEBUG_OBJECT (xwindow->dcontext->elem,
          "slow-path.. I got a %s so I need to memcpy",
          g_type_name (G_OBJECT_TYPE (buf)));
#if 0 // XXX
      memcpy (GST_BUFFER_DATA (newbuf),
          GST_BUFFER_DATA (buf),
          MIN (GST_BUFFER_SIZE (newbuf), GST_BUFFER_SIZE (buf)));
#else
      GST_DEBUG_OBJECT (xwindow->dcontext->elem,
          "stubbed: memcpy(%p, %p, %d)",
          GST_BUFFER_DATA (newbuf),
          GST_BUFFER_DATA (buf),
          MIN (GST_BUFFER_SIZE (newbuf), GST_BUFFER_SIZE (buf)));
#endif
    }
  }

  return newbuf;
}

GstFlowReturn
gst_dri2window_buffer_alloc (GstDRI2Window * xwindow, guint size,
    GstCaps * caps, GstBuffer ** buf)
{
  GstDRI2Context *dcontext = xwindow->dcontext;
  GstFlowReturn ret = GST_FLOW_ERROR;

  *buf = NULL;

  g_mutex_lock (xwindow->pool_lock);
#if 0
  /* double check if we need this.. if we do, we probably need to
   * move pool_valid back to dri2videosink itself, because the
   * window can be created after the PAUSED->READY state transition
   */
  if (G_UNLIKELY (! xwindow->pool_valid)) {
    GST_DEBUG_OBJECT (dcontext->elem, "the pool is flushing");
    ret = GST_FLOW_WRONG_STATE;
    g_mutex_unlock (xwindow->pool_lock);
    goto beach;
  }
#endif

  /* initialize the buffer pool if not initialized yet */
  if (G_UNLIKELY (!xwindow->buffer_pool ||
      gst_drm_buffer_pool_size (xwindow->buffer_pool) != size)) {

    if (xwindow->buffer_pool) {
      GST_INFO_OBJECT (dcontext->elem, "size change");
      gst_drm_buffer_pool_destroy (xwindow->buffer_pool);
    }

    GST_LOG_OBJECT (dcontext->elem, "Creating buffer pool");
    xwindow->buffer_pool = GST_DRM_BUFFER_POOL (gst_dri2_buffer_pool_new (
        xwindow, dcontext->drm_fd, caps, size));
    if (!xwindow->buffer_pool) {
      goto beach;
    }
  }

  *buf = GST_BUFFER (gst_drm_buffer_pool_get (xwindow->buffer_pool, FALSE));

  if (*buf)
    ret = GST_FLOW_OK;

beach:
  g_mutex_unlock (xwindow->pool_lock);
  return ret;
}

/*
 * These are used by the bufferpool to allocate buffers.. the bufferpool
 * needs to go thru the GstDRI2Window, because we need one place to track
 * which attachment points are in use and which are not to hande cases of
 * switching between resolutions, where the bufferpool is replaced but
 * with a transition period of having both buffers of the old and new size
 * floating around
 */

static DRI2Buffer *
get_buffer (GstDRI2Window * xwindow, guint attach, gint width, gint height,
    guint32 format)
{
  GstDRI2Context *dcontext = xwindow->dcontext;
  int nbufs = 1;
  unsigned attachments[] = { attach, format };
  DRI2Buffer *dri2buf;
  g_mutex_lock (dcontext->x_lock);
  dri2buf = DRI2GetBuffersVid(dcontext->x_display, xwindow->window,
      width, height, attachments, nbufs, &nbufs);
  g_mutex_unlock (dcontext->x_lock);
  GST_DEBUG_OBJECT (dcontext->elem, "got %d buffer(s)", nbufs);
  if (nbufs != 1) {
    free (dri2buf);
    return NULL;
  }
  return dri2buf;
}

DRI2Buffer *
gst_dri2window_get_dri2buffer (GstDRI2Window * xwindow, gint width, gint height,
    guint32 format)
{
  GstDRI2Context *dcontext = xwindow->dcontext;
  int idx;

  /* find an empty slot, note first slot is the (fake) front buffer,
   * attached when the GstDRI2Window is constructed:
   */
  for (idx = 0; idx < G_N_ELEMENTS (xwindow->dri2bufs); idx++) {
    if (!xwindow->dri2bufs[idx]) {
      xwindow->dri2bufs[idx] = get_buffer (xwindow, idx + 1,
          width, height, format);
      g_warn_if_fail ((xwindow->dri2bufs[idx]->attachment - 1) == idx);
      return xwindow->dri2bufs[idx];
    }
  }

  GST_ERROR_OBJECT (dcontext->elem, "out of buffer slots");

  return NULL;
}

void
gst_dri2window_free_dri2buffer (GstDRI2Window * xwindow, DRI2Buffer * dri2buf)
{
  int idx = dri2buf->attachment - 1;
  get_buffer (xwindow, dri2buf->attachment, 0, 0, 0);
  free (xwindow->dri2bufs[idx]);
  xwindow->dri2bufs[idx] = NULL;
}

static void
gst_dri2window_class_init (gpointer g_class, gpointer class_data)
{
  GstMiniObjectClass *mini_object_class = GST_MINI_OBJECT_CLASS (g_class);

  dri2window_parent_class = g_type_class_peek_parent (g_class);

  mini_object_class->finalize = (GstMiniObjectFinalizeFunction)
      GST_DEBUG_FUNCPTR (gst_dri2window_finalize);
}

GType
gst_dri2window_get_type (void)
{
  static GType type;

  if (G_UNLIKELY (type == 0)) {
    static const GTypeInfo info = {
      .class_size = sizeof (GstMiniObjectClass),
      .class_init = gst_dri2window_class_init,
      .instance_size = sizeof (GstDRI2Window),
    };
    type = g_type_register_static (GST_TYPE_MINI_OBJECT,
        "GstDRI2Window", &info, 0);
  }
  return type;
}
