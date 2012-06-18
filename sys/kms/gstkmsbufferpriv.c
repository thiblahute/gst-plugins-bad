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

#include <stdint.h>
#include <gst/gst.h>
#include <gst/dmabuf/dmabuf.h>

#include <omap_drm.h>
#include <omap_drmif.h>
#include <xf86drmMode.h>

#include "gstkmssink.h"
#include "gstkmsbufferpriv.h"

#define GST_KMS_BUFFER_PRIV_QUARK gst_kms_buffer_priv_quark_get_type()
static GST_BOILERPLATE_QUARK (GstKMSBufferPriv, gst_kms_buffer_priv_quark);

#define KMS_BUFFER_PRIV_QUARK kms_buffer_priv_quark_get_type()
static GST_BOILERPLATE_QUARK (KMSBufferPriv, kms_buffer_priv_quark);

GST_BOILERPLATE_MINI_OBJECT (GstKMSBufferPriv, gst_kms_buffer_priv);

static void
set_kms_buffer_priv (GstBuffer * buf, GstKMSBufferPriv * priv)
{
  gst_buffer_set_qdata (buf, GST_KMS_BUFFER_PRIV_QUARK,
      gst_structure_id_new (GST_KMS_BUFFER_PRIV_QUARK,
          KMS_BUFFER_PRIV_QUARK, GST_TYPE_KMS_BUFFER_PRIV, priv, NULL));
}

static GstKMSBufferPriv *
get_kms_buffer_priv (GstBuffer * buf)
{
  const GstStructure *s;
  const GValue *val;

  s = gst_buffer_get_qdata (buf, GST_KMS_BUFFER_PRIV_QUARK);
  if (s == NULL)
    return NULL;

  val = gst_structure_id_get_value (s, KMS_BUFFER_PRIV_QUARK);
  if (val == NULL)
    return NULL;

  return GST_KMS_BUFFER_PRIV (gst_value_get_mini_object (val));
}

static void
gst_kms_buffer_priv_finalize (GstMiniObject * mini_obj)
{
  GstKMSBufferPriv *priv = (GstKMSBufferPriv *) mini_obj;

  drmModeRmFB (priv->fd, priv->fb_id);
  omap_bo_del (priv->bo);

  /* not chaining up to GstMiniObject's finalize for now, we know it's empty */
}

static void
gst_kms_buffer_priv_class_init (GstKMSBufferPrivClass * klass)
{
  GST_MINI_OBJECT_CLASS (klass)->finalize = gst_kms_buffer_priv_finalize;
}

static int
create_fb (GstKMSBufferPriv * priv, GstKMSSink * sink)
{
  /* TODO get format, etc from caps.. and query device for
   * supported formats, and make this all more flexible to
   * cope with various formats:
   */
  uint32_t fourcc = GST_MAKE_FOURCC ('N', 'V', '1', '2');

  uint32_t handles[4] = {
    omap_bo_handle (priv->bo), omap_bo_handle (priv->bo),
  };
  uint32_t pitches[4] = {
    sink->input_width, sink->input_width,
  };
  uint32_t offsets[4] = {
    0, pitches[0] * sink->input_height
  };

  return drmModeAddFB2 (priv->fd, sink->input_width, sink->input_height,
      fourcc, handles, pitches, offsets, &priv->fb_id, 0);
}

GstKMSBufferPriv *
gst_kms_buffer_priv (GstKMSSink * sink, GstBuffer * buf)
{
  GstKMSBufferPriv *priv = get_kms_buffer_priv (buf);
  if (!priv) {
    GstDmaBuf *dmabuf = gst_buffer_get_dma_buf (buf);

    /* if it isn't a dmabuf buffer that we can import, then there
     * is nothing we can do with it:
     */
    if (!dmabuf) {
      GST_DEBUG_OBJECT (sink, "not importing non dmabuf buffer");
      return NULL;
    }

    priv = (GstKMSBufferPriv *) gst_mini_object_new (GST_TYPE_KMS_BUFFER_PRIV);

    priv->bo = omap_bo_from_dmabuf (sink->dev, gst_dma_buf_get_fd (dmabuf));
    priv->fd = sink->fd;

    if (create_fb (priv, sink)) {
      GST_WARNING_OBJECT (sink, "could not create framebuffer: %s",
          strerror (errno));
      gst_mini_object_unref (GST_MINI_OBJECT_CAST (priv));
      return NULL;
    }

    set_kms_buffer_priv (buf, priv);
  }
  return priv;
}
