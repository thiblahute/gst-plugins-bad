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

#include "gstdri2bufferpool.h"

#include <sys/drm/gstdrmmeta.h>

/*
 * GstDRI2BufferPool:
 */

#define gst_dri2_buffer_pool_parent_class parent_class
G_DEFINE_TYPE (GstDRI2BufferPool, gst_dri2_buffer_pool,
    GST_TYPE_DRM_BUFFER_POOL);

GstBufferPool *
gst_dri2_buffer_pool_new (GstDRI2Window * xwindow, int fd)
{
  GstDRI2BufferPool *self = (GstDRI2BufferPool *)
      g_object_new (GST_TYPE_DRI2_BUFFER_POOL, NULL);

  gst_drm_buffer_pool_initialize (GST_DRM_BUFFER_POOL (self),
      xwindow->dcontext->elem, fd);

  self->xwindow =
      (GstDRI2Window *) gst_mini_object_ref (GST_MINI_OBJECT (xwindow));

  return GST_BUFFER_POOL (self);
}

static GstFlowReturn
gst_dri2_pool_alloc_buffer (GstBufferPool * pool, GstBuffer ** buffer,
    GstBufferPoolAcquireParams * params)
{
  struct omap_bo *bo;
  GstDRI2Meta *dri2meta;

  GstVideoInfo *info = &GST_DRM_BUFFER_POOL (pool)->info;
  GstDRI2BufferPool *dri2pool = GST_DRI2_BUFFER_POOL (pool);

  *buffer = gst_buffer_new ();
  dri2meta = (GstDRI2Meta *) gst_buffer_add_meta (*buffer, GST_DRI2_META_INFO,
      NULL);

  if (!dri2meta) {
    gst_buffer_unref (*buffer);
    GST_WARNING_OBJECT (pool, "Could not add DRI2 meta");

    return GST_FLOW_ERROR;
  }

  GST_DEBUG_OBJECT (pool, "Allocating new buffer");
  dri2meta->dri2buf = gst_dri2window_get_dri2buffer (dri2pool->xwindow,
      info->width, info->height,
      gst_video_format_to_fourcc (info->finfo->format));
  dri2meta->xwindow = gst_dri2window_ref (dri2pool->xwindow);

  /* we can't really properly support multi-planar w/ separate buffers
   * in gst0.10..  we need a way to indicate this to the server!
   */
  g_warn_if_fail (dri2meta->dri2buf->names[1] == 0);

  bo = omap_bo_from_name (GST_DRM_BUFFER_POOL (pool)->dev,
      dri2meta->dri2buf->names[0]);
  gst_buffer_add_drm_meta (*buffer, pool, bo);

  /* We chain up to parent class */
  return GST_BUFFER_POOL_CLASS (parent_class)->alloc_buffer (pool, buffer,
      params);
}

static void
gst_dri2_buffer_pool_finalize (GObject * object)
{
  gst_dri2window_unref (GST_DRI2_BUFFER_POOL (object)->xwindow);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_dri2_buffer_pool_class_init (GstDRI2BufferPoolClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstBufferPoolClass *gstbufferpool_class = (GstBufferPoolClass *) klass;

  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_dri2_buffer_pool_finalize);

  gstbufferpool_class->alloc_buffer =
      GST_DEBUG_FUNCPTR (gst_dri2_pool_alloc_buffer);
}

static void
gst_dri2_buffer_pool_init (GstDRI2BufferPool * self)
{
}

/*
 * GstDRI2Meta:
 */

static void
gst_dri2_meta_free (GstDRI2Meta * meta, GstBuffer * buf)
{
  gst_dri2window_free_dri2buffer (meta->xwindow, meta->dri2buf);
  gst_dri2window_unref (meta->xwindow);

  return;
}

GType
gst_dri2_meta_api_get_type (void)
{
  static volatile GType type;
  static const gchar *tags[] = {
    NULL
  };
  if (g_once_init_enter (&type)) {
    GType _type = gst_meta_api_type_register ("GstDRI2MetaAPI", tags);
    g_once_init_leave (&type, _type);
  }
  return type;
}

const GstMetaInfo *
gst_dri2_meta_get_info (void)
{
  static const GstMetaInfo *dri2_meta_info = NULL;

  if (dri2_meta_info == NULL) {
    dri2_meta_info = gst_meta_register (GST_DRI2_META_API_TYPE, "GstDRI2Meta", sizeof (GstDRI2Meta), NULL,      /* No init needed */
        (GstMetaFreeFunction) gst_dri2_meta_free, NULL);
  }
  return dri2_meta_info;
}
