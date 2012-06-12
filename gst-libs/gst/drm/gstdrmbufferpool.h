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

#ifndef __GSTDRMBUFFERPOOL_H__
#define __GSTDRMBUFFERPOOL_H__

#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_TYPE_DRM_BUFFER_POOL (gst_drm_buffer_pool_get_type())
#define GST_IS_DRM_BUFFER_POOL(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_DRM_BUFFER_POOL))
#define GST_DRM_BUFFER_POOL(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_DRM_BUFFER_POOL, GstDRMBufferPool))

#define GST_DRM_BUFFER_POOL_LOCK(self)     g_mutex_lock ((self)->lock)
#define GST_DRM_BUFFER_POOL_UNLOCK(self)   g_mutex_unlock ((self)->lock)

typedef struct _GstDRMBufferPool GstDRMBufferPool;
typedef struct _GstDRMBufferPoolClass GstDRMBufferPoolClass;

GType gst_drm_buffer_pool_get_type (void);
GstDRMBufferPool * gst_drm_buffer_pool_new (GstElement * element,
    int fd, GstCaps * caps, guint size);
void gst_drm_buffer_pool_destroy (GstDRMBufferPool * self);
guint gst_drm_buffer_pool_size (GstDRMBufferPool * self);
void gst_drm_buffer_pool_set_caps (GstDRMBufferPool * self, GstCaps * caps);
gboolean gst_drm_buffer_pool_check_caps (GstDRMBufferPool * self,
    GstCaps * caps);
GstBuffer * gst_drm_buffer_pool_get (GstDRMBufferPool * self,
    gboolean force_alloc);

G_END_DECLS

#endif /* __GSTDRMBUFFERPOOL_H__ */
