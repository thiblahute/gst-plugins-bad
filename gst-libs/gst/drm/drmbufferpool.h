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

#ifndef __DRMBUFFERPOOL_H__
#define __DRMBUFFERPOOL_H__

/*
 * private header for gstdrmbufferpool
 */

#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>

#include <gst/gst.h>
#include <gst/dmabuf/dmabuf.h>

/* TODO replace dependency on libdrm_omap w/ libdrm.. the only thing
 * missing is way to allocate buffers, but this should probably be
 * done via libdrm?
 */
#include <omap_drm.h>
#include <omap_drmif.h>

G_BEGIN_DECLS

/*
 * GstDRMBuffer:
 */

#define GST_TYPE_DRM_BUFFER (gst_drm_buffer_get_type())
#define GST_IS_DRM_BUFFER(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_DRM_BUFFER))
#define GST_DRM_BUFFER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_DRM_BUFFER, GstDRMBuffer))

typedef struct _GstDRMBuffer GstDRMBuffer;
typedef struct _GstDRMBufferClass GstDRMBufferClass;

/* forward declaration */
struct _GstDRMBufferPool;

struct _GstDRMBuffer {
  GstBuffer parent;

  struct omap_bo *bo;

  struct _GstDRMBufferPool *pool; /* buffer-pool that this buffer belongs to */
  GstDRMBuffer *next;             /* next in freelist, if not in use */
  gboolean remove_from_pool;
};

struct _GstDRMBufferClass {
  GstBufferClass klass;
};

GType gst_drm_buffer_get_type (void);
GstDRMBuffer * gst_drm_buffer_new (struct _GstDRMBufferPool * pool);
void gst_drm_buffer_set_pool (GstDRMBuffer * self, struct _GstDRMBufferPool * pool);

/*
 * GstDRMBufferPool:
 */

/* TODO do we want GstDRMBufferPool to be subclassed.. if so, move this
 * back to public header.. for now I'm just keeping the implementation
 * entirely opaque
 */
struct _GstDRMBufferPool
{
  GstMiniObject parent;

  int fd;
  struct omap_device *dev;

  /* output (padded) size including any codec padding: */
  gint width, height;

  gboolean         strided;  /* 2d buffers? */
  GstCaps         *caps;
  GMutex          *lock;
  gboolean         running;  /* with lock */
  GstElement      *element;  /* the element that owns us.. */
  GstDRMBuffer    *head; /* list of available buffers */
  GstDRMBuffer    *tail;
  guint size;
};

struct _GstDRMBufferPoolClass
{
  GstMiniObjectClass klass;
};

gboolean gst_drm_buffer_pool_put (GstDRMBufferPool * self, GstDRMBuffer * buf);


G_END_DECLS

#endif /* __DRMBUFFERPOOL_H__ */
