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

#ifndef __GSTDRI2BUFFERPOOL_H__
#define __GSTDRI2BUFFERPOOL_H__

#include <gst/gst.h>

#include "gstdri2util.h"

G_BEGIN_DECLS

/*
 * GstDRI2BufferPool:
 */

#define GST_TYPE_DRI2_BUFFER_POOL (gst_dri2_buffer_pool_get_type())
#define GST_IS_DRI2_BUFFER_POOL(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_DRI2_BUFFER_POOL))
#define GST_DRI2_BUFFER_POOL(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_DRI2_BUFFER_POOL, GstDRI2BufferPool))
#define GST_DRI2_BUFFER_POOL_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_DRI2_BUFFER_POOL, GstDRI2BufferPoolClass))
#define GST_DRI2_BUFFER_POOL_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_DRI2_BUFFER_POOL, GstDRI2BufferPoolClass))

#define GST_DRI2_BUFFER_POOL_LOCK(self)     g_mutex_lock ((self)->lock)
#define GST_DRI2_BUFFER_POOL_UNLOCK(self)   g_mutex_unlock ((self)->lock)

typedef struct _GstDRI2BufferPool GstDRI2BufferPool;
typedef struct _GstDRI2BufferPoolClass GstDRI2BufferPoolClass;

struct _GstDRI2BufferPool {
  GstDRMBufferPool parent;
  GstDRI2Window *xwindow;
};

struct _GstDRI2BufferPoolClass {
  GstDRMBufferPoolClass klass;
};

GType gst_dri2_buffer_pool_get_type (void);

GstBufferPool * gst_dri2_buffer_pool_new (GstDRI2Window * xwindow,
                                          int fd);
void gst_dri2_buffer_pool_destroy        (GstDRI2BufferPool * self);

/*
 * GstDRI2Meta:
 */

#define GST_DRI2_META_API_TYPE (gst_dri2_meta_api_get_type())
#define GST_DRI2_META_INFO  (gst_dri2_meta_get_info())

typedef struct _GstDRI2Meta GstDRI2Meta;

struct _GstDRI2Meta {
  GstMeta parent;

  DRI2Buffer *dri2buf;

  GstDRI2Window *xwindow;
  /* FIXME Do we need that? */
  guint width, height;
  guint32 format;
};

GType gst_dri2_meta_api_get_type (void);
const GstMetaInfo * gst_dri2_meta_get_info (void);

#define gst_buffer_get_dri2_meta(b) ((GstDRI2Meta*)gst_buffer_get_meta((b),GST_DRI2_META_API_TYPE))

G_END_DECLS

#endif /* __GSTDRI2BUFFERPOOL_H__ */
