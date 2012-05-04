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

#ifndef __GSTDUCATIBUFFERDRM_H__
#define __GSTDUCATIBUFFERDRM_H__

#include <stdint.h>

#include "gstducatibuffer.h"

G_BEGIN_DECLS

#define GST_TYPE_DUCATI_DRM_BUFFER (gst_ducati_drm_buffer_get_type())
#define GST_IS_DUCATI_DRM_BUFFER(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_DUCATI_DRM_BUFFER))
#define GST_DUCATI_DRM_BUFFER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_DUCATI_DRM_BUFFER, GstDucatiDRMBuffer))

#define GST_TYPE_DUCATI_DRM_BUFFER_ALLOCATOR (gst_ducati_drm_buffer_allocator_get_type())
#define GST_IS_DUCATI_DRM_BUFFER_ALLOCATOR(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_DUCATI_DRM_BUFFER_ALLOCATOR))
#define GST_DUCATI_DRM_BUFFER_ALLOCATOR(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_DUCATI_DRM_BUFFER_ALLOCATOR, GstDucatiDRMBufferAllocator))

typedef struct _GstDucatiDRMBufferAllocator GstDucatiDRMBufferAllocator;
typedef struct _GstDucatiDRMBufferAllocatorClass GstDucatiDRMBufferAllocatorClass;
typedef struct _GstDucatiDRMBuffer GstDucatiDRMBuffer;
typedef struct _GstDucatiDRMBufferClass GstDucatiDRMBufferClass;
typedef struct _GstDucatiDRMBufferAllocator GstDucatiDRMBufferAllocator;
typedef struct _GstDucatiDRMBufferAllocatorClass GstDucatiDRMBufferAllocatorClass;

struct _GstDucatiDRMBuffer
{
  GstDucatiBuffer parent;
  struct omap_bo *bo[4];
  uint32_t handles[4];
  uint32_t pitches[4];
  uint32_t offsets[4];
  struct omap_device *device;
};

struct _GstDucatiDRMBufferClass
{
  GstDucatiBufferClass parent_class;
};

struct _GstDucatiDRMBufferAllocator
{
  GstDucatiBufferAllocator parent;
  struct omap_device *device;
};

struct _GstDucatiDRMBufferAllocatorClass
{
  GstDucatiBufferAllocatorClass parent_class;
};

GType gst_ducati_drm_buffer_get_type (void);
void gst_ducati_drm_buffer_setup_bos (GstDucatiDRMBuffer *self);

GType gst_ducati_drm_buffer_allocator_get_type (void);
GstDucatiDRMBufferAllocator * gst_ducati_drm_buffer_allocator_new (struct omap_device *device);
G_END_DECLS

#endif /* __GSTDUCATIBUFFERDRM_H__ */
