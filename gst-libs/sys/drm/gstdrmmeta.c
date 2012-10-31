/**
 * Gstreamer
 *
 * Copyright (C) 2012 Texas Instruments
 * Copyright (C) 2012 Collabora Ltd
 *
 * Authors:
 *  Alessandro Decina <alessandro.decina@collabora.co.uk>
 *  Rob Clark <rob.clark@linaro.org>
 *  Thibault Saunier <thibault.saunier@collabora.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstdrmmeta.h"
#include "gstdrmbufferpool.h"

GstDRMMeta *
gst_buffer_add_drm_meta (GstBuffer * buffer, GstBufferPool * pool,
    struct omap_bo *bo)
{
  GstDRMMeta *meta;
  GstDRMBufferPool *drmpool = GST_DRM_BUFFER_POOL (pool);

  meta = (GstDRMMeta *) gst_buffer_add_meta (buffer, GST_DRM_META_INFO, NULL);

  if (bo)
    meta->bo = bo;
  else
    meta->bo =
        omap_bo_new (drmpool->dev, GST_VIDEO_INFO_SIZE (&drmpool->info),
        OMAP_BO_WC);

  GST_DEBUG ("Got new bo %p", meta->bo);

  meta->uv_offset = GST_VIDEO_INFO_PLANE_OFFSET (&drmpool->info, 1);
  meta->size = GST_VIDEO_INFO_SIZE (&drmpool->info);

  /* Add the memory */
  gst_buffer_append_memory (buffer,
      gst_memory_new_wrapped (GST_MEMORY_FLAG_NO_SHARE,
          omap_bo_map (meta->bo), GST_VIDEO_INFO_SIZE (&drmpool->info),
          0, GST_VIDEO_INFO_SIZE (&drmpool->info), NULL, NULL));

  return meta;
}

static void
gst_drm_meta_free (GstMeta * meta, GstBuffer * buffer)
{
  GstDRMMeta *self = (GstDRMMeta *) meta;

  omap_bo_del (self->bo);
}

GType
gst_drm_meta_api_get_type (void)
{
  static volatile GType type;
  static const gchar *tags[] = {
    NULL
  };
  if (g_once_init_enter (&type)) {
    GType _type = gst_meta_api_type_register ("GstDRMMetaAPI", tags);
    g_once_init_leave (&type, _type);
  }
  return type;
}

const GstMetaInfo *
gst_drm_meta_get_info (void)
{
  static const GstMetaInfo *drm_meta_info = NULL;
  if (drm_meta_info == NULL) {
    drm_meta_info = gst_meta_register (GST_DRM_META_API_TYPE, "GstDRMMeta", sizeof (GstDRMMeta), NULL,  /* No init needed */
        gst_drm_meta_free, NULL /* No transform needed */ );
  }
  return drm_meta_info;
}
