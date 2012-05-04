/*
 * GStreamer
 * Copyright (c) 2010, Texas Instruments Incorporated
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

#include "gstducatibufferpool.h"
#include <string.h>

G_DEFINE_TYPE (GstDucatiBufferPool, gst_ducati_buffer_pool,
    GST_TYPE_MINI_OBJECT);

GstDucatiBufferPool *
gst_ducati_buffer_pool_new (GstElement * element,
    GstDucatiBufferAllocator * allocator, GstCaps * caps, guint size)
{
  GstDucatiBufferPool *self = (GstDucatiBufferPool *)
      gst_mini_object_new (GST_TYPE_DUCATI_BUFFER_POOL);
  int i;

  self->element = gst_object_ref (element);
  self->allocator =
      GST_DUCATI_BUFFER_ALLOCATOR (gst_mini_object_ref (GST_MINI_OBJECT
          (allocator)));
  self->caps = NULL;
  gst_ducati_buffer_pool_set_caps (self, caps);
  self->size = size;
  self->head = NULL;
  self->tail = NULL;
  self->lock = g_mutex_new ();
  self->running = TRUE;


  for (i = 0; i < 5; i++) {
    GstDucatiBuffer *tmp = gst_ducati_buffer_pool_get (self, NULL, TRUE);
    gst_buffer_unref (GST_BUFFER_CAST (tmp));
  }

  return self;
}

void
gst_ducati_buffer_pool_set_caps (GstDucatiBufferPool * self, GstCaps * caps)
{
  gst_caps_replace (&self->caps, caps);
  if (caps) {
    GstStructure *s = gst_caps_get_structure (caps, 0);

    self->strided =
        !strcmp (gst_structure_get_name (s), "video/x-raw-yuv-strided");

    gst_structure_get_int (s, "width", &self->padded_width);
    gst_structure_get_int (s, "height", &self->padded_height);
  } else {
    self->padded_width = 0;
    self->padded_height = 0;
    self->strided = FALSE;
  }
}

/** destroy existing bufferpool */
void
gst_ducati_buffer_pool_destroy (GstDucatiBufferPool * self)
{
  g_return_if_fail (self);

  GST_DUCATI_BUFFER_POOL_LOCK (self);
  self->running = FALSE;

  GST_DEBUG_OBJECT (self->element, "destroy pool");

  /* free all buffers on the freelist */
  while (self->head) {
    GstDucatiBuffer *buf = self->head;
    self->head = buf->next;
    buf->next = NULL;
    GST_DEBUG_OBJECT (self, "unreffing %p from freelist", buf);
    GST_DUCATI_BUFFER_POOL_UNLOCK (self);
    gst_buffer_unref (GST_BUFFER (buf));
    GST_DUCATI_BUFFER_POOL_LOCK (self);
  }
  self->tail = NULL;
  GST_DUCATI_BUFFER_POOL_UNLOCK (self);
  gst_mini_object_unref (GST_MINI_OBJECT (self));
}

#if 0
static void
dump_list (GstDucatiBufferPool * pool, GstDucatiBuffer * buf)
{
  GST_ERROR_OBJECT (pool->element, "LIST");
  while (buf) {
    GST_ERROR_OBJECT (pool->element, "BUF: %p", buf);
    buf = buf->next;
  }
}
#endif

/** get buffer from bufferpool, allocate new buffer if needed */
GstDucatiBuffer *
gst_ducati_buffer_pool_get (GstDucatiBufferPool * self, GstBuffer * orig,
    gboolean force_alloc)
{
  GstDucatiBuffer *buf = NULL;

  g_return_val_if_fail (self, NULL);

  GST_DUCATI_BUFFER_POOL_LOCK (self);
  if (self->running) {
    /* re-use a buffer off the freelist if any are available
     */
    if (!force_alloc && self->head) {
//      dump_list (self, self->head);
      buf = self->head;
      self->head = buf->next;
      if (self->head == NULL)
        self->tail = NULL;
    } else {
      GstDucatiBufferAllocatorClass *alloc_class;

      alloc_class = GST_DUCATI_BUFFER_ALLOCATOR_GET_CLASS (self->allocator);
      buf = GST_DUCATI_BUFFER (alloc_class->alloc (self->allocator, self));
    }
    buf->orig = orig;
    if (self->caps)
      gst_buffer_set_caps (GST_BUFFER (buf), self->caps);
  }
  GST_DUCATI_BUFFER_POOL_UNLOCK (self);

  if (buf && orig) {
    GST_BUFFER_TIMESTAMP (buf) = GST_BUFFER_TIMESTAMP (orig);
    GST_BUFFER_DURATION (buf) = GST_BUFFER_DURATION (orig);
  }

  GST_LOG_OBJECT (self->element, "returning buf %p", buf);

  return buf;
}

gboolean
gst_ducati_buffer_pool_put (GstDucatiBufferPool * self, GstDucatiBuffer * buf)
{
  gboolean reuse = FALSE;

  if (buf->remove_from_pool)
    return FALSE;

  GST_DUCATI_BUFFER_POOL_LOCK (self);
  if (self->running) {
    reuse = TRUE;

    GST_LOG_OBJECT (self->element, "reviving buffer %p", buf);
    gst_buffer_ref (GST_BUFFER (buf));

    buf->next = NULL;
    if (self->tail)
      self->tail->next = buf;
    self->tail = buf;
    if (self->head == NULL)
      self->head = self->tail;
    buf->remove_from_pool = FALSE;
  } else {
    GST_INFO_OBJECT (self->element, "the pool is shutting down");
    buf->remove_from_pool = TRUE;
  }
  GST_DUCATI_BUFFER_POOL_UNLOCK (self);

  return reuse;
}

static void
gst_ducati_buffer_pool_finalize (GstDucatiBufferPool * self)
{
  g_mutex_free (self->lock);
  if (self->caps)
    gst_caps_unref (self->caps);
  gst_object_unref (self->element);
  gst_mini_object_unref (GST_MINI_OBJECT (self->allocator));
  GST_MINI_OBJECT_CLASS (gst_ducati_buffer_pool_parent_class)->finalize
      (GST_MINI_OBJECT (self));
}

static void
gst_ducati_buffer_pool_class_init (GstDucatiBufferPoolClass * klass)
{
  GstMiniObjectClass *mini_object_class = GST_MINI_OBJECT_CLASS (klass);

  mini_object_class->finalize = (GstMiniObjectFinalizeFunction)
      GST_DEBUG_FUNCPTR (gst_ducati_buffer_pool_finalize);
}

static void
gst_ducati_buffer_pool_init (GstDucatiBufferPool * self)
{
}
