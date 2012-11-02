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

#ifndef __GST_DMA_BUF_META_H__
#define __GST_DMA_BUF_META_H__

#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_DMA_BUF_META_API_TYPE (gst_dma_buf_meta_api_get_type())
#define GST_DMA_BUF_META_INFO  (gst_dma_buf_meta_get_info())

typedef struct _GstDmaBufMeta GstDmaBufMeta;

/**
 * GstDmaBufMeta:
 * @meta: parent #GstMeta
 * @fd: the fd
 *
 */
struct _GstDmaBufMeta {
  GstMeta      meta;

  int          fd;

  void         *padding[GST_PADDING];
};

GType gst_dma_buf_meta_api_get_type (void);
const GstMetaInfo * gst_dma_buf_meta_get_info (void);

#define gst_buffer_get_dma_buf_meta(b) ((GstDmaBufMeta*)gst_buffer_get_meta((b),GST_DMA_BUF_META_API_TYPE))

GstDmaBufMeta * gst_buffer_add_dma_buf_meta (GstBuffer    *buffer,
					     int fd);

G_END_DECLS

#endif /* __GST_DMA_BUF_META_H__ */
