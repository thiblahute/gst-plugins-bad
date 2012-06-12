/* GStreamer
 *
 * Copyright (C) 2012 Texas Instruments
 * Copyright (C) 2012 Collabora Ltd
 *
 * Authors:
 *  Alessandro Decina <alessandro.decina@collabora.co.uk>
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

#include "gstdrmutils.h"

void
gst_drm_connector_cleanup (int fd, struct connector * c)
{
  if (c->connector) {
    drmModeFreeConnector (c->connector);
    c->connector = NULL;
  }
  if (c->mode) {
    drmModeFreeModeInfo (c->mode);
    c->mode = NULL;
  }
  if (c->encoder) {
    drmModeFreeEncoder (c->encoder);
    c->encoder = NULL;
  }
  if (c->fb_id) {
    drmModeRmFB (fd, c->fb_id);
    c->fb_id = 0;
  }
  if (c->fb_bo) {
    omap_bo_del (c->fb_bo);
    c->fb_bo = NULL;
  }
}

gboolean
gst_drm_connector_find_mode_and_plane (int fd,
    struct omap_device * dev, int width, int height,
    drmModeRes * resources, drmModePlaneRes * plane_resources,
    struct connector *c, drmModePlane ** out_plane)
{
  int i, best_area = 0, ret;

  /* free old stuff: */
  if (*out_plane) /* TODO maybe move into 'struct connector'?? */
    drmModeFreePlane (*out_plane);
  gst_drm_connector_cleanup (fd, c);

  /* First, find the connector & mode */
  c->connector = drmModeGetConnector (fd, c->id);
  if (!c->connector)
    goto error_no_connector;

  if (!c->connector->count_modes)
    goto error_no_mode;

  /* just look for the highest resolution: */
  for (i = 0; i < c->connector->count_modes; i++) {
    drmModeModeInfo *mode = &c->connector->modes[i];
    int area = mode->hdisplay * mode->vdisplay;

    if (area > best_area) {
      c->mode = mode;
      best_area = area;
    }
  }

  if (c->mode == NULL) {
    /* XXX: just pick the first available mode. Not sure this is correct... */
    c->mode = &c->connector->modes[0];
#if 0
    goto error_no_mode;
#endif
  }

  /* Now get the encoder */
  c->encoder = drmModeGetEncoder (fd, c->connector->encoder_id);
  if (!c->encoder)
    goto error_no_encoder;

  if (c->crtc == -1)
    c->crtc = c->encoder->crtc_id;

  /* and figure out which crtc index it is: */
  c->pipe = -1;
  for (i = 0; i < resources->count_crtcs; i++) {
    if (c->crtc == (int) resources->crtcs[i]) {
      c->pipe = i;
      break;
    }
  }

  if (c->pipe == -1)
    goto error_no_crtc;

  *out_plane = NULL;
  for (i = 0; i < plane_resources->count_planes; i++) {
    drmModePlane *plane = drmModeGetPlane (fd, plane_resources->planes[i]);
    if (plane->possible_crtcs & (1 << c->pipe)) {
      *out_plane = plane;
      break;
    }
  }

  if (*out_plane == NULL)
    goto error_no_plane;

  c->fb_bo = omap_bo_new (dev, best_area * 2, OMAP_BO_WC);
  if (c->fb_bo) {
    uint32_t fourcc = DRM_FORMAT_RGB565;
    uint32_t handles[4] = { omap_bo_handle (c->fb_bo) };
    uint32_t pitches[4] = { c->mode->hdisplay * 2 };
    uint32_t offsets[4] = { 0 };
    ret = drmModeAddFB2 (fd, c->mode->hdisplay, c->mode->vdisplay,
        fourcc, handles, pitches, offsets, &c->fb_id, 0);
    if (ret) {
      /* TODO */
    }
  }

  /* now set the desired mode: */
  ret = drmModeSetCrtc (fd, c->crtc, c->fb_id, 0, 0, &c->id, 1, c->mode);
  if (ret) {
    /* TODO */
  }

  return TRUE;

fail:
  gst_drm_connector_cleanup (fd, c);

  return FALSE;

error_no_connector:
  GST_ERROR ("could not get connector %s", strerror (errno));
  goto fail;

error_no_mode:
  GST_ERROR ("could not find mode %dx%d (count_modes %d)",
      width, height, c->connector->count_modes);
  goto fail;

error_no_encoder:
  GST_ERROR ("could not get encoder: %s", strerror (errno));
  goto fail;

error_no_crtc:
  GST_ERROR ("couldn't find a crtc");
  goto fail;

error_no_plane:
  GST_ERROR ("couldn't find a plane");
  goto fail;
}
