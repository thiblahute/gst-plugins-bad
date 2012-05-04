/*
 * GStreamer
 *
 * Copyright (C) 2012 Texas Instruments 
 * Copyright (C) 2012 Collabora Ltd
 *
 * Authors:
 *  Alessandro Decina <alessandro.decina@collabora.co.uk>
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

#include "gstducatidrmbuffer.h"
#include "gstducatibufferpool.h"
#include <omap_drm.h>
#include <omap_drmif.h>

G_DEFINE_TYPE (GstDucatiDRMBuffer, gst_ducati_drm_buffer,
    GST_TYPE_DUCATI_BUFFER);
G_DEFINE_TYPE (GstDucatiDRMBufferAllocator, gst_ducati_drm_buffer_allocator,
    GST_TYPE_DUCATI_BUFFER_ALLOCATOR);

static GstDucatiDRMBuffer *
gst_ducati_drm_buffer_new (GstDucatiBufferPool * pool,
    struct omap_device *device)
{
  GstDucatiDRMBuffer *self = (GstDucatiDRMBuffer *)
      gst_mini_object_new (GST_TYPE_DUCATI_DRM_BUFFER);

  self->device = device;
  gst_ducati_buffer_set_pool (GST_DUCATI_BUFFER (self), pool);
  gst_ducati_drm_buffer_setup_bos (self);

  return self;
}

void
gst_ducati_drm_buffer_setup_bos (GstDucatiDRMBuffer * self)
{
  GstDucatiBufferPool *pool = GST_DUCATI_BUFFER (self)->pool;

  if (pool->strided)
    g_assert (FALSE);

  self->bo[0] = omap_bo_new (self->device, pool->size, OMAP_BO_WC);
  self->bo[1] = 0;
  self->bo[2] = 0;
  self->bo[3] = 0;

  self->handles[0] = omap_bo_handle (self->bo[0]);
  self->handles[1] = self->handles[0];
  self->handles[2] = 0;
  self->handles[3] = 0;

  self->pitches[0] = pool->padded_width;
  self->pitches[1] = pool->padded_width;
  self->pitches[2] = 0;
  self->pitches[3] = 0;

  self->offsets[0] = 0;
  self->offsets[1] = self->pitches[0] * pool->padded_height;
  self->offsets[2] = 0;
  self->offsets[3] = 0;

  GST_BUFFER_DATA (self) = omap_bo_map (self->bo[0]);
  GST_BUFFER_SIZE (self) = pool->size;
}

static void
gst_ducati_drm_buffer_finalize (GstDucatiDRMBuffer * self)
{
  GstDucatiBuffer *ducati_buf = (GstDucatiBuffer *) self;
  GstDucatiBufferPool *pool = ducati_buf->pool;
  gboolean resuscitated = FALSE;
  int i;

  GST_LOG_OBJECT (pool->element, "finalizing buffer %p", self);

  resuscitated = gst_ducati_buffer_pool_put (pool, ducati_buf);
  if (resuscitated)
    return;

  GST_LOG_OBJECT (pool->element,
      "buffer %p (data %p, len %u) not recovered, freeing",
      self, GST_BUFFER_DATA (self), GST_BUFFER_SIZE (self));

  for (i = 0; i < G_N_ELEMENTS (self->bo); i++) {
    if (self->bo[i]) {
      omap_bo_del (self->bo[i]);
      self->bo[i] = 0;
    }
  }

  /* only chain up if the buffer isn't being reused */
  GST_MINI_OBJECT_CLASS (gst_ducati_drm_buffer_parent_class)->finalize
      (GST_MINI_OBJECT (self));
}

static void
gst_ducati_drm_buffer_class_init (GstDucatiDRMBufferClass * klass)
{
  GstMiniObjectClass *mini_object_class = GST_MINI_OBJECT_CLASS (klass);

  mini_object_class->finalize = (GstMiniObjectFinalizeFunction)
      GST_DEBUG_FUNCPTR (gst_ducati_drm_buffer_finalize);
}

static void
gst_ducati_drm_buffer_init (GstDucatiDRMBuffer * buffer)
{
}

GstDucatiDRMBufferAllocator *
gst_ducati_drm_buffer_allocator_new (struct omap_device *device)
{
  GstDucatiDRMBufferAllocator *self = (GstDucatiDRMBufferAllocator *)
      gst_mini_object_new (GST_TYPE_DUCATI_DRM_BUFFER_ALLOCATOR);
  self->device = device;

  return self;
}

static void
gst_ducati_drm_buffer_allocator_finalize (GstDucatiDRMBufferAllocator * self)
{
  GST_MINI_OBJECT_CLASS (gst_ducati_drm_buffer_allocator_parent_class)->finalize
      (GST_MINI_OBJECT (self));
}

static GstBuffer *
allocator_alloc (GstDucatiBufferAllocator * allocator,
    GstDucatiBufferPool * pool)
{
  GstDucatiDRMBufferAllocator *self =
      GST_DUCATI_DRM_BUFFER_ALLOCATOR (allocator);
  GstDucatiDRMBuffer *buf = gst_ducati_drm_buffer_new (pool, self->device);
  return GST_BUFFER_CAST (buf);
}

static gboolean
allocator_check_compatible (GstDucatiBufferAllocator * allocator,
    GstBuffer * buffer)
{
  static GType kms_type = -1;
  if (kms_type == -1)
    kms_type = g_type_from_name ("GstDucatiKMSBuffer");

  return GST_IS_DUCATI_DRM_BUFFER (buffer) ||
      (kms_type != -1 && G_TYPE_CHECK_INSTANCE_TYPE (buffer, kms_type));
}

static void
allocator_setup_codec_output_buffers (GstDucatiBufferAllocator * allocator,
    GstVideoFormat format, int width, int height, int stride,
    GstBuffer * buffer, XDM2_BufDesc * bufs)
{
  GstDucatiDRMBuffer *drmbuf = GST_DUCATI_DRM_BUFFER (buffer);
  int uv_offset, size;

  uv_offset = gst_video_format_get_component_offset (format, 1, stride, height);
  size = gst_video_format_get_size (format, stride, height);

  bufs->numBufs = 2;
  bufs->descs[0].memType = XDM_MEMTYPE_BO;
  bufs->descs[0].buf = (XDAS_Int8 *) omap_bo_handle (drmbuf->bo[0]);
  bufs->descs[0].bufSize.bytes = uv_offset;
  bufs->descs[1].memType = XDM_MEMTYPE_BO_OFFSET;
  bufs->descs[1].buf = (XDAS_Int8 *) uv_offset;
  bufs->descs[1].bufSize.bytes = size - uv_offset;
}

static void
gst_ducati_drm_buffer_allocator_class_init (GstDucatiDRMBufferAllocatorClass *
    klass)
{
  GstMiniObjectClass *mini_object_class = GST_MINI_OBJECT_CLASS (klass);
  GstDucatiBufferAllocatorClass *buffer_allocator_class =
      GST_DUCATI_BUFFER_ALLOCATOR_CLASS (klass);

  mini_object_class->finalize = (GstMiniObjectFinalizeFunction)
      GST_DEBUG_FUNCPTR (gst_ducati_drm_buffer_allocator_finalize);

  buffer_allocator_class->alloc = allocator_alloc;
  buffer_allocator_class->check_compatible = allocator_check_compatible;
  buffer_allocator_class->setup_codec_output_buffers =
      allocator_setup_codec_output_buffers;
}

static void
gst_ducati_drm_buffer_allocator_init (GstDucatiDRMBufferAllocator * allocator)
{
}
