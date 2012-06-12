/* GStreamer
 *
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
 */

#ifndef __GST_KMS_SINK_H__
#define __GST_KMS_SINK_H__

#include <gst/video/video.h>
#include <gst/video/gstvideosink.h>
#include <gst/drm/gstdrmbufferpool.h>

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>

#include "gstdrmutils.h"

G_BEGIN_DECLS
#define GST_TYPE_KMS_SINK \
  (gst_kms_sink_get_type())
#define GST_KMS_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_KMS_SINK, GstKMSSink))
#define GST_KMS_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_KMS_SINK, GstKMSSinkClass))
#define GST_IS_KMS_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_KMS_SINK))
#define GST_IS_KMS_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_KMS_SINK))

typedef struct _GstKMSSink GstKMSSink;
typedef struct _GstKMSSinkClass GstKMSSinkClass;

struct _GstKMSSink
{
  GstVideoSink videosink;
  gint input_width, input_height;
  GstVideoFormat format;
  gint par_n, par_d;
  gint fps_n, fps_d;
  gboolean keep_aspect;
  GstVideoRectangle src_rect;
  GstVideoRectangle dst_rect;
  int fd;
  struct omap_device *dev;
  drmModeRes *resources;
  drmModePlaneRes *plane_resources;
  struct connector conn;
  drmModePlane *plane;
  GstDRMBufferPool *pool;
  /* current displayed buffer and last displayed buffer: */
  GstBuffer *display_buf, *last_buf;
  gboolean scale;
};

struct _GstKMSSinkClass
{
  GstVideoSinkClass parent_class;
};

GType gst_kms_sink_get_type (void);

G_END_DECLS
#endif /* __GST_KMS_SINK_H__ */
