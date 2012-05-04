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

#ifndef __GSTDUCATIBUFFERKMS_H__
#define __GSTDUCATIBUFFERKMS_H__

#include "gstducatidrmbuffer.h"

G_BEGIN_DECLS

#define GST_TYPE_DUCATI_KMS_BUFFER (gst_ducati_kms_buffer_get_type())
#define GST_IS_DUCATI_KMS_BUFFER(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_DUCATI_KMS_BUFFER))
#define GST_DUCATI_KMS_BUFFER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_DUCATI_KMS_BUFFER, GstDucatiKMSBuffer))

#define GST_TYPE_DUCATI_KMS_BUFFER_ALLOCATOR (gst_ducati_kms_buffer_allocator_get_type())
#define GST_IS_DUCATI_KMS_BUFFER_ALLOCATOR(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_DUCATI_KMS_BUFFER_ALLOCATOR))
#define GST_DUCATI_KMS_BUFFER_ALLOCATOR(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_DUCATI_KMS_BUFFER_ALLOCATOR, GstDucatiKMSBufferAllocator))

typedef struct _GstDucatiKMSBufferAllocator GstDucatiKMSBufferAllocator;
typedef struct _GstDucatiKMSBufferAllocatorClass GstDucatiKMSBufferAllocatorClass;
typedef struct _GstDucatiKMSBuffer GstDucatiKMSBuffer;
typedef struct _GstDucatiKMSBufferClass GstDucatiKMSBufferClass;
typedef struct _GstDucatiKMSBufferAllocator GstDucatiKMSBufferAllocator;
typedef struct _GstDucatiKMSBufferAllocatorClass GstDucatiKMSBufferAllocatorClass;

struct _GstDucatiKMSBuffer
{
  GstDucatiDRMBuffer parent;
  int fd;
  uint32_t fb_id;
};

struct _GstDucatiKMSBufferClass
{
  GstDucatiDRMBufferClass parent_class;
};

struct _GstDucatiKMSBufferAllocator
{
  GstDucatiBufferAllocator parent;
  struct omap_device *device;
};

struct _GstDucatiKMSBufferAllocatorClass
{
  GstDucatiBufferAllocatorClass parent_class;
};

GType gst_ducati_kms_buffer_get_type (void);
void gst_ducati_kms_buffer_setup_bos (GstDucatiKMSBuffer *self);

GType gst_ducati_kms_buffer_allocator_get_type (void);
GstDucatiKMSBufferAllocator * gst_ducati_kms_buffer_allocator_new (struct omap_device *device);
G_END_DECLS

#endif /* __GSTDUCATIBUFFERKMS_H__ */
