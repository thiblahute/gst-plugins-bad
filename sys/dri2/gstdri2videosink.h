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

#ifndef __GST_DRI2VIDEOSINK_H__
#define __GST_DRI2VIDEOSINK_H__

#include <gst/video/video-crop.h>
#include <gst/video/gstvideosink.h>

#include "gstdri2util.h"

G_BEGIN_DECLS
#define GST_TYPE_DRI2VIDEOSINK (gst_dri2videosink_get_type())
#define GST_DRI2VIDEOSINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_DRI2VIDEOSINK, GstDRI2VideoSink))
#define GST_DRI2VIDEOSINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_DRI2VIDEOSINK, GstDRI2VideoSinkClass))
#define GST_IS_DRI2VIDEOSINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_DRI2VIDEOSINK))
#define GST_IS_DRI2VIDEOSINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_DRI2VIDEOSINK))

typedef struct _GstDRI2VideoSink GstDRI2VideoSink;
typedef struct _GstDRI2VideoSinkClass GstDRI2VideoSinkClass;

struct _GstDRI2VideoSink
{
  /* Our parent class */
  GstVideoSink videosink;

  gboolean running;

  /* Framerate numerator and denominator */
  gint fps_n, fps_d;
  /* size of incoming video */
  guint video_width, video_height;

  GstVideoFormat format;
  gint rowstride;
  gboolean interlaced;

  GThread *event_thread;
  GMutex *flow_lock;

  gboolean keep_aspect;

  GstCaps *current_caps;
  GstDRI2Context *dcontext;
  GstDRI2Window *xwindow;

  GstVideoRectangle render_rect;
  gboolean have_render_rect;
  GstVideoCrop *crop_rect;

  GValue *display_par;
  gint video_par_n;
  gint video_par_d;
  gint display_par_n;
  gint display_par_d;

  gchar *media_title;
  GstBuffer *display_buf;

  /* also keep the last buffer, to give the GPU some time to finish
   * it's blit.. the decoder will block if the buffer isn't done yet
   * for correctness, but really we'd like to minimize that and keep
   * the GPU pipeline full, so we hold back the last buffer from the
   * pool for one extra frame.
   */
  GstBuffer *last_buf;
};

struct _GstDRI2VideoSinkClass
{
  GstVideoSinkClass parent_class;
};

GType gst_dri2videosink_get_type (void);

G_END_DECLS
#endif /* __GST_DRI2VIDEOSINK_H__ */
