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

#ifndef __GSTDUCATIBUFFER_H__
#define __GSTDUCATIBUFFER_H__

#include "gstducati.h"
#include <gst/video/video.h>

G_BEGIN_DECLS

#define GST_TYPE_DUCATI_BUFFER (gst_ducati_buffer_get_type())
#define GST_IS_DUCATI_BUFFER(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_DUCATI_BUFFER))
#define GST_DUCATI_BUFFER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_DUCATI_BUFFER, GstDucatiBuffer))

#define GST_TYPE_DUCATI_BUFFER_ALLOCATOR (gst_ducati_buffer_allocator_get_type())
#define GST_IS_DUCATI_BUFFER_ALLOCATOR(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_DUCATI_BUFFER_ALLOCATOR))
#define GST_DUCATI_BUFFER_ALLOCATOR(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_DUCATI_BUFFER_ALLOCATOR, GstDucatiBufferAllocator))
#define GST_DUCATI_BUFFER_ALLOCATOR_GET_CLASS(obj)               (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_DUCATI_BUFFER_ALLOCATOR, GstDucatiBufferAllocatorClass))
#define GST_DUCATI_BUFFER_ALLOCATOR_CLASS(klass)                 (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_DUCATI_BUFFER_ALLOCATOR, GstDucatiBufferAllocatorClass))

typedef struct _GstDucatiBufferAllocator GstDucatiBufferAllocator;
typedef struct _GstDucatiBufferAllocatorClass GstDucatiBufferAllocatorClass;
typedef struct _GstDucatiBuffer GstDucatiBuffer;
typedef struct _GstDucatiBufferClass GstDucatiBufferClass;

/* forward declaration */
struct _GstDucatiBufferPool;

struct _GstDucatiBufferAllocator
{
  GstMiniObject mini_object;
};

struct _GstDucatiBufferAllocatorClass
{
  GstMiniObjectClass klass;

  GstBuffer *(*alloc) (GstDucatiBufferAllocator * self,
      struct _GstDucatiBufferPool * pool);
  gboolean (*check_compatible) (GstDucatiBufferAllocator * allocator,
      GstBuffer * buffer);
  void (*setup_codec_output_buffers) (GstDucatiBufferAllocator * allocator,
      GstVideoFormat format, int width, int height, int stride, GstBuffer * buf,
      XDM2_BufDesc * bufs);
};

struct _GstDucatiBuffer {
  GstBuffer parent;

  struct _GstDucatiBufferPool *pool; /* buffer-pool that this buffer belongs to */
  GstBuffer       *orig;     /* original buffer, if we need to copy output */
  GstDucatiBuffer *next;     /* next in freelist, if not in use */
  gboolean remove_from_pool;
};

struct _GstDucatiBufferClass {
  GstBufferClass klass;
};

GType gst_ducati_buffer_allocator_get_type (void);
GstDucatiBufferAllocator *gst_ducati_buffer_allocator_new (void);

GType gst_ducati_buffer_get_type (void);
GstDucatiBuffer *gst_ducati_buffer_new (struct _GstDucatiBufferPool * pool);
void gst_ducati_buffer_set_pool (GstDucatiBuffer * self, struct _GstDucatiBufferPool * pool);
GstBuffer * gst_ducati_buffer_get (GstDucatiBuffer * self);

G_END_DECLS

#endif /* __GSTDUCATIBUFFER_H__ */
