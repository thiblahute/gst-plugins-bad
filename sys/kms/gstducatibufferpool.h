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

#ifndef __GSTDUCATIBUFFERPOOL_H__
#define __GSTDUCATIBUFFERPOOL_H__

#include "gstducatibuffer.h"

G_BEGIN_DECLS

#define GST_TYPE_DUCATI_BUFFER_POOL (gst_ducati_buffer_pool_get_type())
#define GST_IS_DUCATI_BUFFER_POOL(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_DUCATI_BUFFER_POOL))
#define GST_DUCATI_BUFFER_POOL(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_DUCATI_BUFFER_POOL, GstDucatiBufferPool))

#define GST_DUCATI_BUFFER_POOL_LOCK(self)     g_mutex_lock ((self)->lock)
#define GST_DUCATI_BUFFER_POOL_UNLOCK(self)   g_mutex_unlock ((self)->lock)

typedef struct _GstDucatiBufferPool GstDucatiBufferPool;
typedef struct _GstDucatiBufferPoolClass GstDucatiBufferPoolClass;

struct _GstDucatiBufferPool
{
  GstMiniObject parent;

  GstDucatiBufferAllocator *allocator;

  /* output (padded) size including any codec padding: */
  gint padded_width, padded_height;

  gboolean         strided;  /* 2d buffers? */
  GstCaps         *caps;
  GMutex          *lock;
  gboolean         running;  /* with lock */
  GstElement      *element;  /* the element that owns us.. */
  GstDucatiBuffer *head; /* list of available buffers */
  GstDucatiBuffer *tail;
  guint size;
};

struct _GstDucatiBufferPoolClass
{
  GstMiniObjectClass klass;
};

GType gst_ducati_buffer_pool_get_type (void);
GstDucatiBufferPool * gst_ducati_buffer_pool_new (GstElement * element,
    GstDucatiBufferAllocator * allocator, GstCaps * caps, guint size);
void gst_ducati_buffer_pool_destroy (GstDucatiBufferPool * pool);
void gst_ducati_buffer_pool_set_caps (GstDucatiBufferPool * self, GstCaps * caps);
GstDucatiBuffer * gst_ducati_buffer_pool_get (GstDucatiBufferPool * self,
    GstBuffer * orig, gboolean force_alloc);
gboolean gst_ducati_buffer_pool_put (GstDucatiBufferPool * self, GstDucatiBuffer * buf);

G_END_DECLS

#endif /* __GSTDUCATIBUFFERPOOL_H__ */
