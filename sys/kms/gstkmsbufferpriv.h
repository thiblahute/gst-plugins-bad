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

#ifndef __GSTKMSBUFFERPRIV_H__
#define __GSTKMSBUFFERPRIV_H__

#include <stdint.h>
#include <gst/gst.h>

G_BEGIN_DECLS

/*
 * per-buffer private data so kmssink can attach a drm_framebuffer
 * handle (fb_id) to a buffer, which gets deleted when the buffer
 * is finalized
 */

#define GST_TYPE_KMS_BUFFER_PRIV      \
  (gst_kms_buffer_priv_get_type ())
#define GST_KMS_BUFFER_PRIV(obj)      \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_KMS_BUFFER_PRIV, GstKMSBufferPriv))
#define GST_IS_KMS_BUFFER_PRIV(obj)     \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_KMS_BUFFER_PRIV))

typedef struct _GstKMSBufferPriv      GstKMSBufferPriv;
typedef struct _GstKMSBufferPrivClass GstKMSBufferPrivClass;


struct _GstKMSBufferPriv
{
  GstMiniObject parent;

  struct omap_bo *bo;
  int fd;
  uint32_t fb_id;
};

struct _GstKMSBufferPrivClass
{
  GstMiniObjectClass parent_class;
};


GType gst_kms_buffer_priv_get_type (void);

GstKMSBufferPriv * gst_kms_buffer_priv (GstKMSSink *sink, GstBuffer * buf);

G_END_DECLS


#endif /* __GSTKMSBUFFERPRIV_H__ */
