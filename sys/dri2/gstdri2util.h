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

#ifndef __GSTDRI2UTIL_H__
#define __GSTDRI2UTIL_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/drm/gstdrmbufferpool.h>

#include <string.h>
#include <math.h>
#include <fcntl.h>
#include <omap_drm.h>
#include <omap_drmif.h>
#include <xf86drmMode.h>
#include <xf86drm.h>
#include <drm.h>
#include <X11/Xlib.h>
#include <X11/Xmd.h>
#include <X11/Xutil.h>
#include <X11/extensions/dri2proto.h>
#include <X11/extensions/dri2.h>


GST_DEBUG_CATEGORY_EXTERN (gst_debug_dri2);
#define GST_CAT_DEFAULT gst_debug_dri2


typedef struct _GstDRI2Context GstDRI2Context;
typedef struct _GstDRI2Window GstDRI2Window;


/*
 * GstDRI2DrawContext
 */

struct _GstDRI2Context
{
  GstElement *elem;

  /* TODO: to handle par properly, we need a way to get physical
   * dimensions from display.. and way to post a buffer specifying
   * dst coords as well as src coords..
   */
  GValue *par;

  GMutex *x_lock;
  Display *x_display;
  gint screen_num;
  gulong black;
  char *driver;
  char *device;

  int drm_fd;
  struct omap_device *dev;
};


GstDRI2Context * gst_dri2context_new (GstElement * elem);
void gst_dri2context_delete (GstDRI2Context *dcontext);

/*
 * GstDRI2Window
 */

GType gst_dri2window_get_type (void);
#define GST_TYPE_DRI2WINDOW (gst_dri2window_get_type())
#define GST_IS_DRI2WINDOW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_DRI2WINDOW))
#define GST_DRI2WINDOW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_DRI2WINDOW, \
                               GstDRI2Window))

struct _GstDRI2Window
{
  GstMiniObject parent;

  GstDRI2Context *dcontext;

  Window window;
  gint width, height;
  gboolean internal;
  GC gc;
  GstVideoFormat format;
  int video_width, video_height;
  gboolean interlaced;

  /* The bufferpool is associated to the drawable because the
   * attachment points are associated with the drawable.. if
   * the app changes the drawable, we need to re-allocate the
   * buffers:
   */
  GMutex *pool_lock;
  GstDRMBufferPool *buffer_pool;
  gboolean pool_valid;

  /* we could be a bit more clever and dynamically size the table:
   */
  DRI2Buffer *dri2bufs[50];
};

GstDRI2Window * gst_dri2window_new_from_handle (GstDRI2Context *dcontext, XID xwindow_id);
GstDRI2Window * gst_dri2window_new (GstDRI2Context *dcontext, gint width, gint height);
void gst_dri2window_delete (GstDRI2Window * xwindow);
void gst_dri2window_update_geometry (GstDRI2Window * xwindow);
void gst_dri2window_set_pool_valid (GstDRI2Window * xwindow, gboolean valid);
void gst_dri2window_check_caps (GstDRI2Window * xwindow, GstCaps * caps);
GstFlowReturn gst_dri2window_buffer_show (GstDRI2Window * xwindow, GstBuffer * buf);
GstBuffer * gst_dri2window_buffer_prepare (GstDRI2Window * xwindow, GstBuffer * buf);
GstFlowReturn gst_dri2window_buffer_alloc (GstDRI2Window * xwindow, guint size, GstCaps * caps, GstBuffer ** buf);

/* used by GstDRI2BufferPool: */
DRI2Buffer * gst_dri2window_get_dri2buffer (GstDRI2Window * xwindow, gint width, gint height, guint32 format);
void gst_dri2window_free_dri2buffer (GstDRI2Window * xwindow, DRI2Buffer * dri2buf);

#endif /* __GSTDRI2UTIL_H__ */
