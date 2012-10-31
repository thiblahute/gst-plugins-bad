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

#ifndef __GSTDRMMETA_H__
#define __GSTDRMMETA_H__

#include <gst/gst.h>

G_BEGIN_DECLS

#include <stdint.h>
#include <omap_drm.h>
#include <omap_drmif.h>

#include <gst/video/gstvideometa.h>

#define GST_DRM_META_API_TYPE (gst_drm_meta_api_get_type())
#define GST_DRM_META_INFO  (gst_drm_meta_get_info())

typedef struct _GstDRMMeta GstDRMMeta;

struct _GstDRMMeta {
  GstMeta         parent;

  /* FIXME : Do we really want to call this DRMMeta and not OMAPDRMMeta ? */
  struct omap_bo *bo;

  guint           uv_offset;
  gsize           size;

  void           *padding[GST_PADDING];
};


GType gst_drm_meta_api_get_type (void);
const GstMetaInfo * gst_drm_meta_get_info (void);

#define gst_buffer_get_drm_meta(b) ((GstDRMMeta*)gst_buffer_get_meta((b),GST_DRM_META_API_TYPE))

GstDRMMeta * gst_buffer_add_drm_meta (GstBuffer * buffer, GstBufferPool * pool,
    struct omap_bo *bo);

G_END_DECLS

#endif /* __GSTDRMMETA_H__ */
