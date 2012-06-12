#ifndef __GST_DRMUTILS_H__
#define __GST_DRMUTILS_H__

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>
#include <xf86drmMode.h>
#include <omap_drm.h>
#include <omap_drmif.h>
#include <drm_fourcc.h>
#include <gst/gst.h>

struct connector {
	uint32_t id;
	char mode_str[64];
	drmModeConnector *connector;
	drmModeModeInfo *mode;
	drmModeEncoder *encoder;
	int fb_id;
	struct omap_bo *fb_bo;
	int crtc;
	int pipe;
};

gboolean gst_drm_connector_find_mode_and_plane (int fd,
    struct omap_device * dev, int width, int height,
    drmModeRes * resources, drmModePlaneRes * plane_resources,
    struct connector *c, drmModePlane ** out_plane);

#endif /* __GST_DRMUTILS_H__ */
