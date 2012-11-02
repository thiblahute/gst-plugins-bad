/* GStreamer
 * Copyright (C) <2012> Edward Hervey <edward@collabora.com>
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

#include <string.h>
#include <unistd.h>

#include "gstdmabufmeta.h"

static void
gst_dma_buf_meta_free (GstMeta * meta, GstBuffer * buffer)
{
  GstDmaBufMeta *dmeta = (GstDmaBufMeta *) meta;

  close (dmeta->fd);
}

static gboolean
gst_dma_buf_meta_transform (GstBuffer * dest, GstMeta * meta,
    GstBuffer * buffer, GQuark type, gpointer data)
{
  GST_WARNING ("Do we need to dup fd ?");

  return TRUE;
}


/**
 * gst_buffer_add_dma_buf_meta:
 * @buffer: a #GstBuffer
 * @fd: the dmabuf fd
 *
 * Returns: the #GstDmaBufMeta on @buffer.
 */
GstDmaBufMeta *
gst_buffer_add_dma_buf_meta (GstBuffer * buffer, int fd)
{
  GstDmaBufMeta *meta;

  meta =
      (GstDmaBufMeta *) gst_buffer_add_meta (buffer,
      GST_DMA_BUF_META_INFO, NULL);

  meta->fd = fd;

  return meta;
}

GType
gst_dma_buf_meta_api_get_type (void)
{
  static volatile GType type;
  static const gchar *tags[] = { NULL };

  if (g_once_init_enter (&type)) {
    GType _type = gst_meta_api_type_register ("GstDmaBufMetaAPI", tags);
    g_once_init_leave (&type, _type);
  }
  return type;
}

const GstMetaInfo *
gst_dma_buf_meta_get_info (void)
{
  static const GstMetaInfo *dma_buf_meta_info = NULL;

  if (dma_buf_meta_info == NULL) {
    dma_buf_meta_info = gst_meta_register (GST_DMA_BUF_META_API_TYPE, "GstDmaBufMeta", sizeof (GstDmaBufMeta), NULL,    /* No init needed */
        gst_dma_buf_meta_free, gst_dma_buf_meta_transform);
  }
  return dma_buf_meta_info;
}
