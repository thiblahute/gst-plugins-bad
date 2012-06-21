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

#ifndef __GSTPVRBUFFERPRIV_H__
#define __GSTPVRBUFFERPRIV_H__

#include <stdint.h>
#include <gst/gst.h>
#include <pvr2d.h>

G_BEGIN_DECLS

/*
 * per-buffer private data so pvrvideosink can attach a PVR2DMEMINFO
 * handle to a buffer, which gets deleted when the buffer
 * is finalized
 */

#define GST_TYPE_PVR_BUFFER_PRIV      \
  (gst_pvr_buffer_priv_get_type ())
#define GST_PVR_BUFFER_PRIV(obj)      \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_PVR_BUFFER_PRIV, GstPVRBufferPriv))
#define GST_IS_PVR_BUFFER_PRIV(obj)     \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_PVR_BUFFER_PRIV))

typedef struct _GstPVRBufferPriv      GstPVRBufferPriv;
typedef struct _GstPVRBufferPrivClass GstPVRBufferPrivClass;


struct _GstPVRBufferPriv
{
  GstMiniObject parent;

  PVR2DCONTEXTHANDLE context;
  GstDisplayHandle *display_handle;
  PVR2DMEMINFO *mem_info;
};

struct _GstPVRBufferPrivClass
{
  GstMiniObjectClass parent_class;
};


GType gst_pvr_buffer_priv_get_type (void);

GstPVRBufferPriv * gst_pvr_buffer_priv (GstPVRVideoSink *sink, GstBuffer * buf);

G_END_DECLS


#endif /* __GSTPVRBUFFERPRIV_H__ */
