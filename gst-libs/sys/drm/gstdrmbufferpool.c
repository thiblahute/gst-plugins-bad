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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include <ext/dma/gstdmabufmeta.h>

#include "gstdrmbufferpool.h"
#include "gstdrmmeta.h"

GST_DEBUG_CATEGORY (drmbufferpool_debug);
#define GST_CAT_DEFAULT drmbufferpool_debug

/*
 * GstDRMBufferPool:
 */

#define gst_drm_buffer_pool_parent_class parent_class
G_DEFINE_TYPE (GstDRMBufferPool, gst_drm_buffer_pool, GST_TYPE_BUFFER_POOL);

void
gst_drm_buffer_pool_initialize (GstDRMBufferPool * pool,
    GstElement * elem, int fd)
{
  pool->element = gst_object_ref (elem);
  pool->fd = fd;
  pool->dev = omap_device_new (fd);
  pool->caps = NULL;

}

GstBufferPool *
gst_drm_buffer_pool_new (GstElement * element, int fd)
{
  GstDRMBufferPool *pool;

  pool = g_object_new (GST_TYPE_DRM_BUFFER_POOL, NULL);

  gst_drm_buffer_pool_initialize (pool, element, fd);

  GST_DEBUG_OBJECT (element, "Created DRM buffer pool");

  return (GstBufferPool *) pool;
}

static const gchar **
gst_drm_buffer_pool_get_options (GstBufferPool * pool)
{
  static const gchar *options[] = { GST_BUFFER_POOL_OPTION_VIDEO_META,
    NULL
  };

  return options;
}

static gboolean
gst_drm_buffer_pool_set_config (GstBufferPool * pool, GstStructure * config)
{
  GstCaps *caps;
  GstVideoInfo info;

  GstDRMBufferPool *self = (GstDRMBufferPool *) pool;

  if (!gst_buffer_pool_config_get_params (config, &caps, NULL, NULL, NULL))
    goto wrong_config;

  if (caps == NULL)
    goto no_caps;

  if (!gst_video_info_from_caps (&info, caps))
    goto wrong_caps;

  GST_LOG_OBJECT (pool, "%dx%d, caps %" GST_PTR_FORMAT, info.width, info.height,
      caps);

  if (self->caps)
    gst_caps_unref (self->caps);
  self->caps = gst_caps_ref (caps);

  self->info = info;

  return GST_BUFFER_POOL_CLASS (parent_class)->set_config (pool, config);

wrong_config:
  {
    GST_WARNING_OBJECT (pool, "invalid config");
    return FALSE;
  }
no_caps:
  {
    GST_WARNING_OBJECT (pool, "no caps in config");
    return FALSE;
  }
wrong_caps:
  {
    GST_WARNING_OBJECT (pool,
        "failed getting geometry from caps %" GST_PTR_FORMAT, caps);
    return FALSE;
  }
}

static GstFlowReturn
gst_drm_buffer_pool_alloc_buffer (GstBufferPool * pool, GstBuffer ** bufer,
    GstBufferPoolAcquireParams * paramts)
{
  GstDRMBufferPool *self = (GstDRMBufferPool *) pool;
  GstVideoInfo *info;
  GstBuffer *res;
  GstDRMMeta *meta;

  info = &self->info;

  res = gst_buffer_new ();

  /* Add DRM Meta (i.e. allocation) */
  meta = gst_buffer_add_drm_meta (res, pool, NULL);
  if (!meta) {
    gst_buffer_unref (res);
    goto no_buffer;
  }

  /* Add DMABuf meta */
  if (!gst_buffer_add_dma_buf_meta (res, omap_bo_dmabuf (meta->bo)))
    GST_WARNING_OBJECT (pool, "Failed to add DMABuf Meta");

  /* Add video meta */
  GST_DEBUG_OBJECT (pool, "Adding GstVideoMeta %dx%d",
      GST_VIDEO_INFO_WIDTH (info), GST_VIDEO_INFO_HEIGHT (info));
  gst_buffer_add_video_meta (res, GST_VIDEO_FRAME_FLAG_NONE,
      GST_VIDEO_INFO_FORMAT (info),
      GST_VIDEO_INFO_WIDTH (info), GST_VIDEO_INFO_HEIGHT (info));

  *bufer = res;

  return GST_FLOW_OK;

  /* ERROR */
no_buffer:
  {
    GST_WARNING_OBJECT (pool, "can't create image");
    return GST_FLOW_ERROR;
  }
}

static void
gst_drm_buffer_pool_finalize (GObject * object)
{
  GstDRMBufferPool *self = (GstDRMBufferPool *) object;

  GST_DEBUG_OBJECT (self->element, "finalize");

  if (self->caps)
    gst_caps_unref (self->caps);
  gst_object_unref (self->element);

  omap_device_del (self->dev);

  G_OBJECT_CLASS (gst_drm_buffer_pool_parent_class)->finalize (object);
}

static void
gst_drm_buffer_pool_class_init (GstDRMBufferPoolClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstBufferPoolClass *gstbufferpool_class = (GstBufferPoolClass *) klass;

  GST_DEBUG_CATEGORY_INIT (drmbufferpool_debug, "drmbufferpool", 0,
      "DRM buffer pool");

  gobject_class->finalize = gst_drm_buffer_pool_finalize;
  gstbufferpool_class->get_options = gst_drm_buffer_pool_get_options;
  gstbufferpool_class->set_config = gst_drm_buffer_pool_set_config;
  gstbufferpool_class->alloc_buffer = gst_drm_buffer_pool_alloc_buffer;
}

static void
gst_drm_buffer_pool_init (GstDRMBufferPool * self)
{
}

/*
 * GstDRMMeta:
 */

#if 0

/* FIXME : Let the pool do this */

void
gst_drm_buffer_initialize (GstDRMMeta * self,
    GstDRMMetaPool * pool, struct omap_bo *bo)
{
  self->bo = bo;

  GST_BUFFER_DATA (self) = omap_bo_map (self->bo);
  GST_BUFFER_SIZE (self) = pool->size;

/* attach dmabuf handle to buffer so that elements from other
 * plugins can access for zero copy hw accel:
 */
// XXX buffer doesn't take ownership of the GstDRM...
  gst_buffer_set_dma_buf (GST_BUFFER (self),
      gst_dma_buf_new (omap_bo_dmabuf (self->bo)));

  gst_drm_buffer_set_pool (self, pool);
}
#endif
