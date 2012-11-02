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

#include <stdlib.h>

#include <gst/video/videooverlay.h>
#include <gst/video/gstvideometa.h>

/* Debugging category */
#include <gst/gstinfo.h>

#include "gstdri2util.h"
#include "gstdri2bufferpool.h"

GST_DEBUG_CATEGORY_EXTERN (GST_CAT_PERFORMANCE);

static Bool
WireToEvent (Display * dpy, XExtDisplayInfo * info,
    XEvent * event, xEvent * wire)
{
  switch ((wire->u.u.type & 0x7f) - info->codes->first_event) {

    case DRI2_BufferSwapComplete:{
      //    xDRI2BufferSwapComplete *awire = (xDRI2BufferSwapComplete *)wire;
      // TODO use this to know when the previous buffer is no longer visible..
      GST_LOG ("BufferSwapComplete");
      return True;
    }
    case DRI2_InvalidateBuffers:{
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

static Status
EventToWire (Display * dpy, XExtDisplayInfo * info,
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

static DRI2Buffer *get_buffer (GstDRI2Window * xwindow, guint attach,
    gint width, gint height, guint32 format);

static Bool
is_fourcc (guint32 val)
{
  return g_ascii_isalnum ((val >> 24) & 0xff)
      && g_ascii_isalnum ((val >> 16) & 0xff)
      && g_ascii_isalnum ((val >> 8) & 0xff)
      && g_ascii_isalnum ((val >> 0) & 0xff);
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

  dcontext = g_slice_new0 (GstDRI2Context);
  dcontext->elem = elem;
  g_mutex_init (&dcontext->x_lock);

  dcontext->x_display = XOpenDisplay (NULL);
  if (!dcontext->x_display) {
    GST_ERROR_OBJECT (elem, "Failed to open X display");
    goto fail;
  }

  if (!DRI2InitDisplay (dcontext->x_display, &ops)) {
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

  GST_DEBUG_OBJECT (elem, "DRI2QueryVersion: major=%d, minor=%d", major, minor);

  root = RootWindow (dcontext->x_display, DefaultScreen (dcontext->x_display));

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
    if (is_fourcc (formats[i])) {
      GST_DEBUG_OBJECT (elem, "  %d: %08x (\"%.4s\")", i, formats[i],
          (char *) &formats[i]);
    } else {
      GST_DEBUG_OBJECT (elem, "  %d: %08x (device dependent)", i, formats[i]);
    }
  }

  free (formats);

  gst_dri2context_calculate_pixel_aspect_ratio (dcontext);

  dcontext->black = XBlackPixel (dcontext->x_display, dcontext->screen_num);

  return dcontext;

fail:
  free (formats);
  if (dcontext->dev)
    omap_device_del (dcontext->dev);

  /* TODO: the code in _delete uses drmClose, but the fd is from open(2) ?? */
  if (fd >= 0)
    drmClose (fd);

  g_mutex_clear (&dcontext->x_lock);
  g_free (dcontext);

  return NULL;
}

void
gst_dri2context_delete (GstDRI2Context * dcontext)
{
  GST_DEBUG_OBJECT (dcontext, "Deleting context");

  if (dcontext->par) {
    g_value_unset (dcontext->par);
    g_free (dcontext->par);
  }

  GST_DRI2CONTEXT_LOCK_X (dcontext);
  XCloseDisplay (dcontext->x_display);
  GST_DRI2CONTEXT_UNLOCK_X (dcontext);
  g_mutex_clear (&dcontext->x_lock);

  omap_device_del (dcontext->dev);
  drmClose (dcontext->drm_fd);

  XFree (dcontext->driver);
  XFree (dcontext->device);

  g_slice_free (GstDRI2Context, dcontext);
}

/*
 * GstDRI2Window
 */

GType _gst_dri2window_type = 0;

GST_DEFINE_MINI_OBJECT_TYPE (GstDRI2Window, gst_dri2window);

#if 0
/*FIXME Check how that works*/
static void
_priv_gst_dri2window_initialize (void)
{
  _gst_dri2window_type = gst_dri2window_get_type ();
}
#endif

static void
gst_dri2window_finalize (GstDRI2Window * xwindow)
{
  GstDRI2Context *dcontext = xwindow->dcontext;

  GST_DEBUG_OBJECT (xwindow, "Finalize window");

  GST_DRI2WINDOW_LOCK_POOL (xwindow);
  xwindow->pool_valid = FALSE;
  if (xwindow->buffer_pool) {
    gst_object_unref (xwindow->buffer_pool);
    xwindow->buffer_pool = NULL;
  }
  GST_DRI2WINDOW_UNLOCK_POOL (xwindow);

  g_mutex_clear (&xwindow->pool_lock);

  GST_DRI2CONTEXT_LOCK_X (dcontext);

  DRI2DestroyDrawable (dcontext->x_display, xwindow->window);
  /* If we did not create the window we just free the GC and let it live */
  if (xwindow->internal)
    XDestroyWindow (dcontext->x_display, xwindow->window);
  else
    XSelectInput (dcontext->x_display, xwindow->window, 0);

  XFreeGC (dcontext->x_display, xwindow->gc);
  XSync (dcontext->x_display, FALSE);

  /* XXX free xwindow->dri2bufs
   * TODO we probably want xwindow to be a refcnt'd miniobj so we don't end w/
   * dri2buffer's referencing deleted xwindow's..
   */

  GST_DRI2CONTEXT_UNLOCK_X (dcontext);

  gst_dri2context_delete (dcontext);
}

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
gst_dri2window_new_from_handle (GstDRI2Context * dcontext, XID xwindow_id)
{
  GstDRI2Window *xwindow;
  XWindowAttributes attr;

  xwindow = g_slice_new (GstDRI2Window);
  gst_mini_object_init (GST_MINI_OBJECT_CAST (xwindow),
      0, _gst_dri2window_type, NULL, NULL,
      (GstMiniObjectFreeFunction) gst_dri2window_finalize);

  xwindow->dcontext = dcontext;
  xwindow->window = xwindow_id;
  xwindow->buffer_pool = NULL;
  xwindow->pool_valid = FALSE;

  g_mutex_init (&xwindow->pool_lock);

  /* Set the event we want to receive and create a GC */
  GST_DRI2CONTEXT_LOCK_X (dcontext);

  XGetWindowAttributes (dcontext->x_display, xwindow->window, &attr);

  xwindow->width = attr.width;
  xwindow->height = attr.height;

  /* We have to do that to prevent X from redrawing the background on
   * ConfigureNotify. This takes away flickering of video when resizing. */
  XSetWindowBackgroundPixmap (dcontext->x_display, xwindow->window, None);

  XMapWindow (dcontext->x_display, xwindow->window);

  xwindow->gc = XCreateGC (dcontext->x_display, xwindow->window, 0, NULL);
  GST_DRI2CONTEXT_UNLOCK_X (dcontext);

  DRI2CreateDrawable (dcontext->x_display, xwindow->window);

  /* request the front buffer.. we don't need to keep it, just to
   * request it.. otherwise DRI2 core on xserver side gets miffed:
   *   [DRI2] swap_buffers: drawable has no back or front?
   */
  free (get_buffer (xwindow, DRI2BufferFrontLeft,
          xwindow->width, xwindow->height, 32));

  gst_video_overlay_got_window_handle (GST_VIDEO_OVERLAY (dcontext->elem),
      xwindow->window);

  return xwindow;
}

GstDRI2Window *
gst_dri2window_new (GstDRI2Context * dcontext, gint width, gint height)
{
  GstDRI2Window *xwindow;
  Window root;
  Atom wm_delete;
  XID xwindow_id;

  GST_DRI2CONTEXT_LOCK_X (dcontext);

  GST_DEBUG_OBJECT (dcontext->elem, "creating window: %dx%d", width, height);

  root = DefaultRootWindow (dcontext->x_display);
  xwindow_id = XCreateSimpleWindow (dcontext->x_display, root, 0, 0,
      width, height, 2, 2, dcontext->black);


  /* Tell the window manager we'd like delete client messages instead of
   * being killed */
  wm_delete = XInternAtom (dcontext->x_display, "WM_DELETE_WINDOW", True);
  if (wm_delete != None) {
    (void) XSetWMProtocols (dcontext->x_display, xwindow_id, &wm_delete, 1);
  }

  GST_DRI2CONTEXT_UNLOCK_X (dcontext);

  xwindow = gst_dri2window_new_from_handle (dcontext, xwindow_id);
  xwindow->internal = TRUE;

  return xwindow;
}

void
gst_dri2window_delete (GstDRI2Window * xwindow)
{
  GST_DRI2WINDOW_LOCK_POOL (xwindow);
  xwindow->pool_valid = FALSE;
  if (xwindow->buffer_pool) {
    gst_object_unref (xwindow->buffer_pool);
    xwindow->buffer_pool = NULL;
  }
  GST_DRI2WINDOW_UNLOCK_POOL (xwindow);

  gst_dri2window_unref (GST_MINI_OBJECT (xwindow));
}

/* call with x_lock held
 *
 * Returns: %TRUE if the geormetry has been updated, %FALSE otherwize */
gboolean
gst_dri2window_update_geometry (GstDRI2Window * xwindow)
{
  XWindowAttributes attr;

  XGetWindowAttributes (xwindow->dcontext->x_display, xwindow->window, &attr);

  if (xwindow->width != attr.width || xwindow->height != attr.height) {
    xwindow->width = attr.width;
    xwindow->height = attr.height;

    return TRUE;
  }

  return FALSE;
}

void
gst_dri2window_set_pool_valid (GstDRI2Window * xwindow, gboolean valid)
{
  GST_DRI2WINDOW_LOCK_POOL (xwindow);
  xwindow->pool_valid = valid;
  GST_DRI2WINDOW_UNLOCK_POOL (xwindow);
}

gboolean
gst_dri2window_create_pool (GstDRI2Window * xwindow, GstVideoInfo * info,
    GstCaps * caps)
{
  GstStructure *structure;
  GstBufferPool *newpool, *oldpool;

  GST_DRI2WINDOW_LOCK_POOL (xwindow);
  if (xwindow->buffer_pool) {
    /* we don't deactivate, some elements might still be using it, it will
     * be deactivated when the last ref is gone */
    gst_object_unref (xwindow->buffer_pool);
  }

  xwindow->info = *info;

  newpool = gst_dri2_buffer_pool_new (xwindow, xwindow->dcontext->drm_fd);
  structure = gst_buffer_pool_get_config (newpool);
  /* FIXME DRI2 on OMAP has a 32 quantization step for strides... check if it is the right place? */
  gst_video_info_set_format (info, info->finfo->format,
      GET_COMPATIBLE_STRIDE (info->finfo->format, info->width), info->height);
  gst_buffer_pool_config_set_params (structure, caps, info->size, 2, 0);

  /* FIXME: Do we need to specify allocator parameters ?
   * gst_buffer_pool_config_set_allocator (structure, NULL, &params);*/
  if (!gst_buffer_pool_set_config (newpool, structure))
    goto config_failed;

  oldpool = xwindow->buffer_pool;
  /* we don't activate the pool yet, this will be done by downstream after it
   * has configured the pool. If downstream does not want our pool we will
   * activate it when we render into it */
  xwindow->buffer_pool = newpool;
  GST_DRI2WINDOW_UNLOCK_POOL (xwindow);

  /* unref the old sink */
  if (oldpool) {
    /* we don't deactivate, some elements might still be using it, it will
     * be deactivated when the last ref is gone */
    gst_object_unref (oldpool);
  }

  return TRUE;

config_failed:
  {
    GST_DEBUG_OBJECT (xwindow->dcontext->elem, "failed setting config");
    gst_object_unref (newpool);

    return FALSE;
  }
}

static inline GstDRI2Meta *
ok_buffer (GstDRI2Window * xwindow, GstBuffer * buf)
{
  GstDRI2Meta *meta;

  if (buf) {
    meta = gst_buffer_get_dri2_meta (buf);
    if (meta && meta->xwindow == xwindow)
      return meta;
    else {
      return NULL;
    }
  }

  return NULL;
}

GstFlowReturn
gst_dri2window_buffer_show (GstDRI2Window * xwindow, GstBuffer * buf)
{
  BoxRec b;
  CARD64 count;
  GstVideoCropMeta *cropmeta;
  GstDRI2Meta *dri2meta;

  GstDRI2Context *dcontext = xwindow->dcontext;

  dri2meta = ok_buffer (xwindow, buf);
  if (!dri2meta) {
    GST_WARNING_OBJECT (dcontext->elem, "unexpected buffer: %" GST_PTR_FORMAT,
        buf);

    return GST_FLOW_ERROR;
  }

  cropmeta = gst_buffer_get_video_crop_meta (buf);
  if (cropmeta) {
    b.x1 = cropmeta->x;
    b.y1 = cropmeta->y;
    b.x2 = b.x1 + cropmeta->width - 1;
    b.y2 = b.y1 + cropmeta->height - 1;
  } else {
    b.x1 = 0;
    b.y1 = 0;
    b.x2 = GST_DRM_BUFFER_POOL (buf->pool)->info.width - 1;
    b.y2 = GST_DRM_BUFFER_POOL (buf->pool)->info.height - 1;
  }

  GST_DRI2CONTEXT_LOCK_X (dcontext);
  DRI2SwapBuffersVid (dcontext->x_display, xwindow->window, 0, 0, 0,
      &count, dri2meta->dri2buf->attachment, &b);

  /* TODO: probably should wait for DRI2_BufferSwapComplete instead..
   * although that probably depends on someone making an x11 call to
   * dispatch the events
   */
  DRI2WaitSBC (dcontext->x_display, xwindow->window, count,
      /* just re-use count as a valid ptr.. we don't need ust/msc/sbc: */
      &count, &count, &count);
  GST_DRI2CONTEXT_UNLOCK_X (dcontext);

  return GST_FLOW_OK;
}

/* Returns: transfer full */
GstBuffer *
gst_dri2window_buffer_prepare (GstDRI2Window * xwindow, GstBuffer * buf)
{
  GstVideoInfo info, *cinfo;
  GstVideoFormat format;
  GstVideoFrame src, dest;

  GstBuffer *newbuf = NULL;
  GstBufferPoolAcquireParams params = { 0, };
  GstElement *sink = xwindow->dcontext->elem;

  if (ok_buffer (xwindow, buf)) {
    GST_DEBUG_OBJECT (sink, "Buffer %" GST_PTR_FORMAT " is OK, using it", buf);
    return gst_buffer_ref (buf);
  }

  GST_LOG_OBJECT (sink, "Got a wrong buffer, make our own");
  /* DRI2 on OMAP has a 32 quantization step for strides, so we copy
     the buffer into another buffer with a size that's to its liking */
  format = xwindow->info.finfo->format;
  cinfo = &xwindow->info;
  gst_video_info_set_format (&info, format, GET_COMPATIBLE_STRIDE (format,
          cinfo->width), cinfo->height);

  /* we should have a pool, configured in setcaps */
  if (xwindow->buffer_pool == NULL)
    goto no_pool;

  /* Else we have to copy the data into our private image, */
  /* if we have one... */
  GST_LOG_OBJECT (xwindow, "buffer %p not from our pool, copying", buf);

  /* we should have a pool, configured in setcaps */
  if (xwindow->buffer_pool == NULL)
    goto no_pool;

  if (!gst_buffer_pool_set_active (xwindow->buffer_pool, TRUE))
    goto activate_failed;

  /* take a buffer from our pool, if there is no buffer in the pool something
   * is seriously wrong, waiting for the pool here might deadlock when we try
   * to go to PAUSED because we never flush the pool then. */
  params.flags = GST_BUFFER_POOL_ACQUIRE_FLAG_DONTWAIT;
  if (gst_buffer_pool_acquire_buffer (xwindow->buffer_pool,
          &newbuf, &params) != GST_FLOW_OK)
    goto no_buffer;

  GST_CAT_LOG_OBJECT (GST_CAT_PERFORMANCE, xwindow,
      "slow copy into bufferpool buffer %p", newbuf);

  if (!gst_video_frame_map (&src, &xwindow->info, buf, GST_MAP_READ))
    goto invalid_buffer;

  if (!gst_video_frame_map (&dest, &xwindow->info, newbuf, GST_MAP_WRITE)) {
    gst_video_frame_unmap (&src);
    goto invalid_buffer;
  }

  gst_video_frame_copy (&dest, &src);

  gst_video_frame_unmap (&dest);
  gst_video_frame_unmap (&src);

  return newbuf;

  /* ERRORS */
no_pool:
  {
    GST_ELEMENT_ERROR (sink, RESOURCE, WRITE,
        ("Internal error: can't allocate images"),
        ("We don't have a bufferpool negotiated"));
    return NULL;
  }
no_buffer:
  {
    /* No image available. That's very bad ! */
    GST_WARNING_OBJECT (sink, "could not create image");
    return NULL;
  }
invalid_buffer:
  {
    /* No Window available to put our image into */
    GST_WARNING_OBJECT (sink, "could not map image");
    return NULL;
  }
activate_failed:
  {
    GST_ERROR_OBJECT (sink, "failed to activate bufferpool.");
    return NULL;
  }
}

#if 0
/* FIXME Check if we somehow need that! */
GstFlowReturn
gst_dri2window_buffer_alloc (GstDRI2Window * xwindow, guint size,
    GstCaps * caps, GstBuffer ** buf)
{
  GstDRI2Context *dcontext = xwindow->dcontext;
  GstFlowReturn ret = GST_FLOW_ERROR;
  guint dri2_good_width, dri2_good_size;

  *buf = NULL;

  /* If we'll have to memcpy to match stride, just give away
     a normal buffer */
  dri2_good_width = gst_dri2window_get_compatible_stride (xwindow->format,
      xwindow->video_width);
  dri2_good_size = gst_video_format_get_size (xwindow->format, dri2_good_width,
      xwindow->video_height);

  if (dri2_good_size != size) {
    GstBuffer *buffer = gst_buffer_new_allocate (NULL, size, NULL);
    gst_buffer_set_caps (buffer, caps);
    *buf = buffer;
    GST_WARNING_OBJECT (dcontext->elem, "Creating normal buffer, will memcpy");
    return GST_FLOW_OK;
  }

  GST_DRI2WINDOW_LOCK_POOL (xwindow);

#if 0
  /* double check if we need this.. if we do, we probably need to
   * move pool_valid back to dri2videosink itself, because the
   * window can be created after the PAUSED->READY state transition
   */
  if (G_UNLIKELY (!xwindow->pool_valid)) {
    GST_DEBUG_OBJECT (dcontext->elem, "the pool is flushing");
    ret = GST_FLOW_WRONG_STATE;
    GST_DRI2WINDOW_UNLOCK_POOL (xwindow);
    goto beach;
  }
#endif

  /* initialize the buffer pool if not initialized yet */
  if (G_UNLIKELY (!xwindow->buffer_pool ||
          gst_buffer_pool_get_size (xwindow->buffer_pool) != size)) {

    if (xwindow->buffer_pool) {
      GST_INFO_OBJECT (dcontext->elem, "size change");
      gst_drm_buffer_pool_destroy (xwindow->buffer_pool);
    }

    GST_LOG_OBJECT (dcontext->elem, "Creating buffer pool");
    xwindow->buffer_pool =
        GST_DRM_BUFFER_POOL (gst_dri2_buffer_pool_new (xwindow,
            dcontext->drm_fd, caps, size));
    GST_LOG_OBJECT (dcontext->elem, "Created buffer pool %p",
        xwindow->buffer_pool);
    if (!xwindow->buffer_pool) {
      goto beach;
    }
  }

  *buf = GST_BUFFER (gst_drm_buffer_pool_get (xwindow->buffer_pool, FALSE));

  if (*buf)
    ret = GST_FLOW_OK;

beach:
  GST_DRI2WINDOW_UNLOCK_POOL (xwindow);
  return ret;
}
#endif

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
  int nbufs = 1;
  DRI2Buffer *dri2buf;
  GstDRI2Context *dcontext = xwindow->dcontext;
  unsigned attachments[] = { attach, format };

  GST_DRI2CONTEXT_LOCK_X (dcontext);
  dri2buf = DRI2GetBuffersVid (dcontext->x_display, xwindow->window,
      width, height, attachments, nbufs, &nbufs);
  GST_DRI2CONTEXT_UNLOCK_X (dcontext);


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
