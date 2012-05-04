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

#include "gstducatikmsbuffer.h"
#include "gstducatidrmbuffer.h"
#include "gstducatibufferpool.h"
#include <omap_drm.h>
#include <omap_drmif.h>

G_DEFINE_TYPE (GstDucatiKMSBuffer, gst_ducati_kms_buffer,
    GST_TYPE_DUCATI_DRM_BUFFER);
G_DEFINE_TYPE (GstDucatiKMSBufferAllocator, gst_ducati_kms_buffer_allocator,
    GST_TYPE_DUCATI_DRM_BUFFER_ALLOCATOR);

static GstDucatiKMSBuffer *
gst_ducati_kms_buffer_new (GstDucatiBufferPool * pool,
    struct omap_device *device)
{
  GstDucatiKMSBuffer *self = (GstDucatiKMSBuffer *)
      gst_mini_object_new (GST_TYPE_DUCATI_KMS_BUFFER);

  GST_DUCATI_DRM_BUFFER (self)->device = device;
  self->fd = -1;
  self->fb_id = -1;

  gst_ducati_buffer_set_pool (GST_DUCATI_BUFFER (self), pool);
  gst_ducati_drm_buffer_setup_bos (GST_DUCATI_DRM_BUFFER (self));

  return self;
}

static void
gst_ducati_kms_buffer_finalize (GstDucatiKMSBuffer * self)
{
  GstDucatiBuffer *ducati_buf = (GstDucatiBuffer *) self;
  GstDucatiBufferPool *pool = ducati_buf->pool;
  gboolean resuscitated = FALSE;

  GST_LOG_OBJECT (pool->element, "finalizing buffer %p", self);

  resuscitated = gst_ducati_buffer_pool_put (pool, ducati_buf);
  if (resuscitated)
    return;

  GST_LOG_OBJECT (pool->element,
      "buffer %p (data %p, len %u) not recovered, freeing",
      self, GST_BUFFER_DATA (self), GST_BUFFER_SIZE (self));

  drmModeRmFB (self->fd, self->fb_id);

  /* only chain up if the buffer isn't being reused */
  GST_MINI_OBJECT_CLASS (gst_ducati_kms_buffer_parent_class)->finalize
      (GST_MINI_OBJECT (self));
}

static void
gst_ducati_kms_buffer_class_init (GstDucatiKMSBufferClass * klass)
{
  GstMiniObjectClass *mini_object_class = GST_MINI_OBJECT_CLASS (klass);

  mini_object_class->finalize = (GstMiniObjectFinalizeFunction)
      GST_DEBUG_FUNCPTR (gst_ducati_kms_buffer_finalize);
}

static void
gst_ducati_kms_buffer_init (GstDucatiKMSBuffer * buffer)
{
}

GstDucatiKMSBufferAllocator *
gst_ducati_kms_buffer_allocator_new (struct omap_device *device)
{
  GstDucatiKMSBufferAllocator *self = (GstDucatiKMSBufferAllocator *)
      gst_mini_object_new (GST_TYPE_DUCATI_KMS_BUFFER_ALLOCATOR);
  self->device = device;

  return self;
}

static void
gst_ducati_kms_buffer_allocator_finalize (GstDucatiKMSBufferAllocator * self)
{
  GST_MINI_OBJECT_CLASS (gst_ducati_kms_buffer_allocator_parent_class)->finalize
      (GST_MINI_OBJECT (self));
}

static GstBuffer *
allocator_alloc (GstDucatiBufferAllocator * allocator,
    GstDucatiBufferPool * pool)
{
  GstDucatiKMSBufferAllocator *self =
      GST_DUCATI_KMS_BUFFER_ALLOCATOR (allocator);
  GstDucatiKMSBuffer *buf = gst_ducati_kms_buffer_new (pool, self->device);
  return GST_BUFFER_CAST (buf);
}

static gboolean
allocator_check_compatible (GstDucatiBufferAllocator * allocator,
    GstBuffer * buffer)
{
  return GST_IS_DUCATI_KMS_BUFFER (buffer);
}

static void
gst_ducati_kms_buffer_allocator_class_init (GstDucatiKMSBufferAllocatorClass *
    klass)
{
  GstMiniObjectClass *mini_object_class = GST_MINI_OBJECT_CLASS (klass);
  GstDucatiBufferAllocatorClass *buffer_allocator_class =
      GST_DUCATI_BUFFER_ALLOCATOR_CLASS (klass);

  mini_object_class->finalize = (GstMiniObjectFinalizeFunction)
      GST_DEBUG_FUNCPTR (gst_ducati_kms_buffer_allocator_finalize);

  buffer_allocator_class->alloc = allocator_alloc;
  buffer_allocator_class->check_compatible = allocator_check_compatible;
}

static void
gst_ducati_kms_buffer_allocator_init (GstDucatiKMSBufferAllocator * allocator)
{
}
