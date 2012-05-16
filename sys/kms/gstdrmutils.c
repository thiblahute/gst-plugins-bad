#include "gstdrmutils.h"

gboolean
gst_drm_connector_find_mode_and_plane (int fd, int width, int height,
    drmModeRes * resources, drmModePlaneRes * plane_resources,
    struct connector *c, drmModePlane ** out_plane)
{
  drmModeConnector *connector = NULL;
  int i;

  /* First, find the connector & mode */
  c->mode = NULL;
  c->encoder = NULL;
  connector = drmModeGetConnector (fd, c->id);
  if (!connector)
    goto error_no_connector;

  if (!connector->count_modes)
    goto error_no_mode;

  for (i = 0; i < connector->count_modes; i++) {
    c->mode = &connector->modes[i];
    if (c->mode->hdisplay == width && c->mode->vdisplay == height)
      break;
    else
      c->mode = NULL;
  }

  if (c->mode == NULL) {
    /* XXX: just pick the first available mode. Not sure this is correct... */
    c->mode = &connector->modes[0];
#if 0
    goto error_no_mode;
#endif
  }

  /* Now get the encoder */
  c->encoder = drmModeGetEncoder (fd, connector->encoder_id);
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

  return TRUE;

fail:
  if (c->encoder)
    drmModeFreeEncoder (c->encoder);

  if (connector)
    drmModeFreeConnector (connector);

  return FALSE;

error_no_connector:
  GST_ERROR ("could not get connector %s", strerror (errno));
  goto fail;

error_no_mode:
  GST_ERROR ("could not find mode %dx%d (count_modes %d)",
      width, height, connector->count_modes);
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
