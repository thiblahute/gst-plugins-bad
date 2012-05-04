#include "gstdrmutils.h"

gboolean
gst_drm_connector_find_mode (int fd, drmModeRes * resources,
    drmModePlaneRes * plane_resources, struct connector *c)
{
  drmModeConnector *connector;
  int i, j;

  /* First, find the connector & mode */
  c->mode = NULL;
  for (i = 0; i < resources->count_connectors; i++) {
    connector = drmModeGetConnector (fd, resources->connectors[i]);

    if (!connector) {
      GST_ERROR ("could not get connector %i: %s",
          resources->connectors[i], strerror (errno));
      drmModeFreeConnector (connector);
      continue;
    }

    if (!connector->count_modes) {
      drmModeFreeConnector (connector);
      continue;
    }

    if (connector->connector_id != c->id) {
      drmModeFreeConnector (connector);
      continue;
    }

    for (j = 0; j < connector->count_modes; j++) {
      c->mode = &connector->modes[j];
      if (!strcmp (c->mode->name, c->mode_str))
        break;
    }

    /* Found it, break out */
    if (c->mode)
      break;

    drmModeFreeConnector (connector);
  }

  if (!c->mode) {
    GST_ERROR ("failed to find mode \"%s\"", c->mode_str);
    return FALSE;
  }

  /* Now get the encoder */
  for (i = 0; i < resources->count_encoders; i++) {
    c->encoder = drmModeGetEncoder (fd, resources->encoders[i]);

    if (!c->encoder) {
      GST_ERROR ("could not get encoder %i: %s",
          resources->encoders[i], strerror (errno));
      drmModeFreeEncoder (c->encoder);
      continue;
    }

    if (c->encoder->encoder_id == connector->encoder_id)
      break;

    drmModeFreeEncoder (c->encoder);
  }

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
    return FALSE;

  return TRUE;
}
