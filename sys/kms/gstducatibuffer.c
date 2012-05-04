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

#include "gstducatibuffer.h"
#include "gstducatibufferpool.h"

G_DEFINE_TYPE (GstDucatiBuffer, gst_ducati_buffer, GST_TYPE_BUFFER);
G_DEFINE_TYPE (GstDucatiBufferAllocator, gst_ducati_buffer_allocator,
    GST_TYPE_MINI_OBJECT);

GstDucatiBuffer *
gst_ducati_buffer_new (GstDucatiBufferPool * pool)
{
  GstDucatiBuffer *self = (GstDucatiBuffer *)
      gst_mini_object_new (GST_TYPE_DUCATI_BUFFER);
  gst_ducati_buffer_set_pool (self, pool);

  return self;
}

void
gst_ducati_buffer_set_pool (GstDucatiBuffer * self, GstDucatiBufferPool * pool)
{

  GST_LOG_OBJECT (pool->element, "creating buffer %p in pool %p", self, pool);

  self->pool = (GstDucatiBufferPool *)
      gst_mini_object_ref (GST_MINI_OBJECT (pool));
  self->remove_from_pool = FALSE;

  if (pool->caps)
    gst_buffer_set_caps (GST_BUFFER (self), pool->caps);
}

static void
gst_ducati_buffer_finalize (GstDucatiBuffer * self)
{
  GstDucatiBufferPool *pool = self->pool;
  gboolean resuscitated = FALSE;

  GST_LOG_OBJECT (pool->element, "finalizing buffer %p", self);

  resuscitated = gst_ducati_buffer_pool_put (pool, self);
  if (resuscitated)
    return;

  if (self->orig) {
    gst_buffer_unref (self->orig);
    self->orig = NULL;
  }

  GST_BUFFER_DATA (self) = NULL;
  gst_mini_object_unref (GST_MINI_OBJECT (pool));

  GST_MINI_OBJECT_CLASS (gst_ducati_buffer_parent_class)->finalize
      (GST_MINI_OBJECT (self));
}

static void
gst_ducati_buffer_class_init (GstDucatiBufferClass * klass)
{
  GstMiniObjectClass *mini_object_class = GST_MINI_OBJECT_CLASS (klass);

  mini_object_class->finalize = (GstMiniObjectFinalizeFunction)
      GST_DEBUG_FUNCPTR (gst_ducati_buffer_finalize);
}

static void
gst_ducati_buffer_init (GstDucatiBuffer * buffer)
{
}

/* Get the original buffer, or whatever is the best output buffer.
 * Consumes the input reference, produces the output reference
 */
GstBuffer *
gst_ducati_buffer_get (GstDucatiBuffer * self)
{
  if (self->orig) {
    // TODO copy to orig buffer.. if needed.
    gst_buffer_unref (self->orig);
    self->orig = NULL;
  }
  return GST_BUFFER (self);
}

/* GstDucatiBufferAllocator */

GstDucatiBufferAllocator *
gst_ducati_buffer_allocator_new (void)
{
  GstDucatiBufferAllocator *self = (GstDucatiBufferAllocator *)
      gst_mini_object_new (GST_TYPE_DUCATI_BUFFER_ALLOCATOR);

  return self;
}

static void
gst_ducati_buffer_allocator_finalize (GstDucatiBufferAllocator * self)
{
  GST_MINI_OBJECT_CLASS (gst_ducati_buffer_allocator_parent_class)->finalize
      (GST_MINI_OBJECT (self));
}

static void
gst_ducati_buffer_allocator_class_init (GstDucatiBufferAllocatorClass * klass)
{
  GstMiniObjectClass *mini_object_class = GST_MINI_OBJECT_CLASS (klass);

  mini_object_class->finalize = (GstMiniObjectFinalizeFunction)
      GST_DEBUG_FUNCPTR (gst_ducati_buffer_allocator_finalize);
}

static void
gst_ducati_buffer_allocator_init (GstDucatiBufferAllocator * buffer)
{
}
