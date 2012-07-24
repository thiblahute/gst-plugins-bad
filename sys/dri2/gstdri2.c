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
#  include <config.h>
#endif

#include "gstdri2videosink.h"

GST_DEBUG_CATEGORY (gst_debug_dri2);

static gboolean
plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (gst_debug_dri2, "dri2videosink", 0, "dri2videosink");

  return gst_element_register (plugin, "dri2videosink", GST_RANK_PRIMARY + 2,
      GST_TYPE_DRI2VIDEOSINK);
}

/* PACKAGE: this is usually set by autotools depending on some _INIT macro
 * in configure.ac and then written into and defined in config.h, but we can
 * just set it ourselves here in case someone doesn't use autotools to
 * compile this code. GST_PLUGIN_DEFINE needs PACKAGE to be defined.
 */
#ifndef PACKAGE
#  define PACKAGE "dri2"
#endif

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "dri2",
    "DRI2 based plugin",
    plugin_init, VERSION, "LGPL", "GStreamer", "http://gstreamer.net/")
