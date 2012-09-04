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

#include "gstdri2bufferpool.h"

/*
 * GstDRI2BufferPool:
 */

static GstMiniObjectClass * gst_dri2bufferpool_parent_class = NULL;

G_DEFINE_TYPE (GstDRI2BufferPool, gst_dri2_buffer_pool,
    GST_TYPE_DRM_BUFFER_POOL);

GstDRI2BufferPool *
gst_dri2_buffer_pool_new (GstDRI2Window * xwindow,
    int fd, GstCaps * caps, guint size)
{
  GstDRI2BufferPool *self = (GstDRI2BufferPool *)
      gst_mini_object_new (GST_TYPE_DRI2_BUFFER_POOL);

  gst_drm_buffer_pool_initialize (GST_DRM_BUFFER_POOL (self),
      xwindow->dcontext->elem, fd, caps, size);

  gst_mini_object_ref (GST_MINI_OBJECT (xwindow));
  self->xwindow = xwindow;

  return self;
}

static GstDRMBuffer *
gst_dri2_buffer_alloc (GstDRMBufferPool * pool)
{
  GstDRI2BufferPool *dri2pool = GST_DRI2_BUFFER_POOL (pool);
  GstDRI2Buffer *self = (GstDRI2Buffer *)
      gst_mini_object_new (GST_TYPE_DRI2_BUFFER);
  struct omap_bo *bo;

  gst_mini_object_ref (GST_MINI_OBJECT (pool));

  GST_DEBUG_OBJECT (pool, "Allocating new buffer");
  self->dri2buf = gst_dri2window_get_dri2buffer (dri2pool->xwindow,
      pool->width, pool->height, pool->fourcc);

  /* we can't really properly support multi-planar w/ separate buffers
   * in gst0.10..  we need a way to indicate this to the server!
   */
  g_warn_if_fail (self->dri2buf->names[1] == 0);

  bo = omap_bo_from_name (pool->dev, self->dri2buf->names[0]);
  gst_drm_buffer_initialize (GST_DRM_BUFFER (self), pool, bo);

  return GST_DRM_BUFFER (self);
}

static void
gst_dri2_buffer_cleanup (GstDRMBufferPool * pool, GstDRMBuffer * buf)
{
  gst_dri2window_free_dri2buffer (
      GST_DRI2_BUFFER_POOL (pool)->xwindow,
      GST_DRI2_BUFFER (buf)->dri2buf);
  gst_mini_object_unref (GST_MINI_OBJECT (pool));
}

static void
gst_dri2_buffer_pool_finalize (GstDRMBufferPool * pool)
{
  gst_mini_object_unref (GST_MINI_OBJECT (GST_DRI2_BUFFER_POOL(pool)->xwindow));
  GST_MINI_OBJECT_CLASS (gst_dri2bufferpool_parent_class)->finalize (GST_MINI_OBJECT
      (pool));
}

static void
gst_dri2_buffer_pool_class_init (GstDRI2BufferPoolClass * klass)
{
  GST_DRM_BUFFER_POOL_CLASS (klass)->buffer_alloc =
      GST_DEBUG_FUNCPTR (gst_dri2_buffer_alloc);
  GST_DRM_BUFFER_POOL_CLASS (klass)->buffer_cleanup =
      GST_DEBUG_FUNCPTR (gst_dri2_buffer_cleanup);
  GST_MINI_OBJECT_CLASS (klass)->finalize =
      (GstMiniObjectFinalizeFunction) GST_DEBUG_FUNCPTR (gst_dri2_buffer_pool_finalize);
  gst_dri2bufferpool_parent_class = g_type_class_peek_parent (klass);
}

static void
gst_dri2_buffer_pool_init (GstDRI2BufferPool * self)
{
}

/*
 * GstDRI2Buffer:
 */


G_DEFINE_TYPE (GstDRI2Buffer, gst_dri2_buffer, GST_TYPE_DRM_BUFFER);

static void
gst_dri2_buffer_class_init (GstDRI2BufferClass * klass)
{
}

static void
gst_dri2_buffer_init (GstDRI2Buffer * buffer)
{
}
