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

#include "gstpvrvideosink.h"
#include "gstpvrbufferpriv.h"

#define LINUX
#include <dri2_omap_ws.h>

#define GST_PVR_BUFFER_PRIV_QUARK gst_pvr_buffer_priv_quark_get_type()
static GST_BOILERPLATE_QUARK (GstPVRBufferPriv, gst_pvr_buffer_priv_quark);

#define PVR_BUFFER_PRIV_QUARK pvr_buffer_priv_quark_get_type()
static GST_BOILERPLATE_QUARK (PVRBufferPriv, pvr_buffer_priv_quark);

static void
set_pvr_buffer_priv (GstBuffer * buf, GstPVRBufferPriv * priv)
{
  gst_buffer_set_qdata (buf, GST_PVR_BUFFER_PRIV_QUARK,
      gst_structure_id_new (GST_PVR_BUFFER_PRIV_QUARK,
          PVR_BUFFER_PRIV_QUARK, GST_TYPE_PVR_BUFFER_PRIV, priv, NULL));
}

static GstPVRBufferPriv *
get_pvr_buffer_priv (GstBuffer * buf)
{
  const GstStructure *s;
  const GValue *val;

  s = gst_buffer_get_qdata (buf, GST_PVR_BUFFER_PRIV_QUARK);
  if (s == NULL)
    return NULL;

  val = gst_structure_id_get_value (s, PVR_BUFFER_PRIV_QUARK);
  if (val == NULL)
    return NULL;

  return GST_PVR_BUFFER_PRIV (gst_value_get_mini_object (val));
}

static void
gst_pvr_buffer_priv_finalize (GstMiniObject * mini_obj)
{
  GstPVRBufferPriv *priv = (GstPVRBufferPriv *) mini_obj;

  PVR2DMemFree (priv->context, priv->mem_info);
  gst_display_handle_unref (priv->display_handle);
}

static void
gst_pvr_buffer_priv_class_init (GstPVRBufferPrivClass * klass)
{
  GST_MINI_OBJECT_CLASS (klass)->finalize = gst_pvr_buffer_priv_finalize;
}

GST_BOILERPLATE_MINI_OBJECT (GstPVRBufferPriv, gst_pvr_buffer_priv);

GstPVRBufferPriv *
gst_pvr_buffer_priv (GstPVRVideoSink * sink, GstBuffer * buf)
{
  GstPVRBufferPriv *priv = get_pvr_buffer_priv (buf);
  if (!priv) {
    GstDmaBuf *dmabuf = gst_buffer_get_dma_buf (buf);

    /* if it isn't a dmabuf buffer that we can import, then there
     * is nothing we can do with it:
     */
    if (!dmabuf) {
      GST_DEBUG_OBJECT (sink, "not importing non dmabuf buffer");
      return NULL;
    }

    priv = (GstPVRBufferPriv *) gst_mini_object_new (GST_TYPE_PVR_BUFFER_PRIV);

    priv->display_handle =
        gst_display_handle_ref (sink->dcontext->gst_display_handle);
    priv->context =
        ((DRI2Display *) priv->display_handle->display_handle)->hContext;
    if (PVR2DImportDmaBuf (priv->context, gst_dma_buf_get_fd (dmabuf),
            &priv->mem_info)) {
      GST_ERROR_OBJECT (sink, "could not import bo: %s", strerror (errno));
      gst_mini_object_unref (GST_MINI_OBJECT_CAST (priv));
      return NULL;
    }

    set_pvr_buffer_priv (buf, priv);

    /* set_pvr_buffer_priv will ref priv via gst_structure_id_new */
    gst_mini_object_unref (GST_MINI_OBJECT_CAST (priv));
  }
  return priv;
}
