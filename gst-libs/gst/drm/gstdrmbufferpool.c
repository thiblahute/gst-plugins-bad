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

#include <string.h>

#include <gst/dmabuf/dmabuf.h>

#include "gstdrmbufferpool.h"

static GstDRMBuffer * gst_drm_buffer_new (GstDRMBufferPool * pool);
static void gst_drm_buffer_set_pool (GstDRMBuffer * self,
    GstDRMBufferPool * pool);

/*
 * GstDRMBufferPool:
 */

G_DEFINE_TYPE (GstDRMBufferPool, gst_drm_buffer_pool,
    GST_TYPE_MINI_OBJECT);

void
gst_drm_buffer_pool_initialize (GstDRMBufferPool * self,
    GstElement * element, int fd, GstCaps * caps, guint size)
{
  self->element = gst_object_ref (element);
  self->fd   = fd;
  self->dev  = omap_device_new (fd);
  self->caps = NULL;
  gst_drm_buffer_pool_set_caps (self, caps);
  self->size = size;
  self->head = NULL;
  self->tail = NULL;
  self->lock = g_mutex_new ();
  self->running = TRUE;
}

GstDRMBufferPool *
gst_drm_buffer_pool_new (GstElement * element,
    int fd, GstCaps * caps, guint size)
{
  GstDRMBufferPool *self = (GstDRMBufferPool *)
      gst_mini_object_new (GST_TYPE_DRM_BUFFER_POOL);

  gst_drm_buffer_pool_initialize (self, element, fd, caps, size);

  return self;
}

/** get size of individual buffers within the bufferpool */
guint
gst_drm_buffer_pool_size (GstDRMBufferPool * self)
{
  return self->size;
}

void
gst_drm_buffer_pool_set_caps (GstDRMBufferPool * self, GstCaps * caps)
{
  gst_caps_replace (&self->caps, caps);
  if (caps) {
    GstStructure *s = gst_caps_get_structure (caps, 0);

    self->strided =
        !strcmp (gst_structure_get_name (s), "video/x-raw-yuv-strided");

    gst_structure_get_int (s, "width", &self->width);
    gst_structure_get_int (s, "height", &self->height);
    gst_structure_get_fourcc (s, "format", &self->fourcc);
  } else {
    self->width = 0;
    self->height = 0;
    self->strided = FALSE;
  }
}

gboolean
gst_drm_buffer_pool_check_caps (GstDRMBufferPool * self,
    GstCaps * caps)
{
  return gst_caps_is_strictly_equal (self->caps, caps);
}

/** destroy existing bufferpool */
void
gst_drm_buffer_pool_destroy (GstDRMBufferPool * self)
{
  g_return_if_fail (self);

  GST_DRM_BUFFER_POOL_LOCK (self);
  self->running = FALSE;

  GST_DEBUG_OBJECT (self->element, "destroy pool");

  /* free all buffers on the freelist */
  while (self->head) {
    GstDRMBuffer *buf = self->head;
    self->head = buf->next;
    buf->next = NULL;
    GST_DEBUG_OBJECT (self, "unreffing %p from freelist", buf);
    GST_DRM_BUFFER_POOL_UNLOCK (self);
    gst_buffer_unref (GST_BUFFER (buf));
    GST_DRM_BUFFER_POOL_LOCK (self);
  }
  self->tail = NULL;
  GST_DRM_BUFFER_POOL_UNLOCK (self);
  gst_mini_object_unref (GST_MINI_OBJECT (self));
}

#if 0
static void
dump_list (GstDRMBufferPool * pool, GstDRMBuffer * buf)
{
  GST_ERROR_OBJECT (pool->element, "LIST");
  while (buf) {
    GST_ERROR_OBJECT (pool->element, "BUF: %p", buf);
    buf = buf->next;
  }
}
#endif

/** get buffer from bufferpool, allocate new buffer if needed */
GstBuffer *
gst_drm_buffer_pool_get (GstDRMBufferPool * self, gboolean force_alloc)
{
  GstDRMBuffer *buf = NULL;

  g_return_val_if_fail (self, NULL);

  GST_DRM_BUFFER_POOL_LOCK (self);
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
      buf = GST_DRM_BUFFER_POOL_GET_CLASS (self)->buffer_alloc (self);
    }
    if (self->caps)
      gst_buffer_set_caps (GST_BUFFER (buf), self->caps);
  }
  GST_DRM_BUFFER_POOL_UNLOCK (self);

  GST_LOG_OBJECT (self->element, "returning buf %p", buf);

  return GST_BUFFER (buf);
}

static gboolean
gst_drm_buffer_pool_put (GstDRMBufferPool * self, GstDRMBuffer * buf)
{
  gboolean reuse = FALSE;

  if (buf->remove_from_pool)
    return FALSE;

  GST_DRM_BUFFER_POOL_LOCK (self);
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
  GST_DRM_BUFFER_POOL_UNLOCK (self);

  return reuse;
}

static void
gst_drm_buffer_pool_finalize (GstDRMBufferPool * self)
{
  g_mutex_free (self->lock);
  if (self->caps)
    gst_caps_unref (self->caps);
  gst_object_unref (self->element);
  omap_device_del (self->dev);
  GST_MINI_OBJECT_CLASS (gst_drm_buffer_pool_parent_class)->finalize
      (GST_MINI_OBJECT (self));
}

static void
gst_drm_buffer_pool_class_init (GstDRMBufferPoolClass * klass)
{
  GstMiniObjectClass *mini_object_class = GST_MINI_OBJECT_CLASS (klass);
  klass->buffer_alloc =
      GST_DEBUG_FUNCPTR (gst_drm_buffer_new);
  mini_object_class->finalize = (GstMiniObjectFinalizeFunction)
      GST_DEBUG_FUNCPTR (gst_drm_buffer_pool_finalize);
}

static void
gst_drm_buffer_pool_init (GstDRMBufferPool * self)
{
}

/*
 * GstDRMBuffer:
 */

G_DEFINE_TYPE (GstDRMBuffer, gst_drm_buffer, GST_TYPE_BUFFER);

void
gst_drm_buffer_initialize (GstDRMBuffer * self,
    GstDRMBufferPool * pool, struct omap_bo * bo)
{
  self->bo = bo;

  GST_BUFFER_DATA (self) = omap_bo_map (self->bo);
  GST_BUFFER_SIZE (self) = pool->size;

  /* attach dmabuf handle to buffer so that elements from other
   * plugins can access for zero copy hw accel:
   */
  // XXX buffer doesn't take ownership of the GstDmaBuf...
  gst_buffer_set_dma_buf (GST_BUFFER (self),
      gst_dma_buf_new (omap_bo_dmabuf (self->bo)));

  gst_drm_buffer_set_pool (self, pool);
}

static GstDRMBuffer *
gst_drm_buffer_new (GstDRMBufferPool * pool)
{
  GstDRMBuffer *self = (GstDRMBuffer *)
      gst_mini_object_new (GST_TYPE_DRM_BUFFER);

  /* TODO: if allocation could be handled via libkms then this
   * bufferpool implementation could be completely generic..
   * otherwise we might want some support for various different
   * drm drivers here:
   */
  struct omap_bo *bo = omap_bo_new (pool->dev, pool->size, OMAP_BO_WC);

  gst_drm_buffer_initialize (self, pool, bo);

  return self;
}

static void
gst_drm_buffer_set_pool (GstDRMBuffer * self, GstDRMBufferPool * pool)
{

  GST_LOG_OBJECT (pool->element, "creating buffer %p in pool %p", self, pool);

  self->pool = (GstDRMBufferPool *)
      gst_mini_object_ref (GST_MINI_OBJECT (pool));
  self->remove_from_pool = FALSE;

  if (pool->caps)
    gst_buffer_set_caps (GST_BUFFER (self), pool->caps);
}

static void
gst_drm_buffer_finalize (GstDRMBuffer * self)
{
  GstDRMBufferPool *pool = self->pool;
  gboolean resuscitated = FALSE;

  GST_LOG_OBJECT (pool->element, "finalizing buffer %p", self);

  resuscitated = gst_drm_buffer_pool_put (pool, self);
  if (resuscitated)
    return;

  if (GST_DRM_BUFFER_POOL_GET_CLASS (self->pool)->buffer_cleanup) {
    GST_DRM_BUFFER_POOL_GET_CLASS (self->pool)->buffer_cleanup (
        self->pool, self);
  }

  GST_BUFFER_DATA (self) = NULL;
  omap_bo_del (self->bo);

  gst_mini_object_unref (GST_MINI_OBJECT (pool));

  GST_MINI_OBJECT_CLASS (gst_drm_buffer_parent_class)->finalize
      (GST_MINI_OBJECT (self));
}

static void
gst_drm_buffer_class_init (GstDRMBufferClass * klass)
{
  GstMiniObjectClass *mini_object_class = GST_MINI_OBJECT_CLASS (klass);

  mini_object_class->finalize = (GstMiniObjectFinalizeFunction)
      GST_DEBUG_FUNCPTR (gst_drm_buffer_finalize);
}

static void
gst_drm_buffer_init (GstDRMBuffer * buffer)
{
}
