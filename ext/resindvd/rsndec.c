/* GStreamer
 * Copyright (C) <2009> Jan Schmidt <thaytan@noraisin.net>
 * Copyright (C) <2009> Sebastian Dröge <sebastian.droege@collabora.co.uk>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "rsndec.h"

GST_DEBUG_CATEGORY_STATIC (rsn_dec_debug);
#define GST_CAT_DEFAULT rsn_dec_debug

static GstStateChangeReturn rsn_dec_change_state (GstElement * element,
    GstStateChange transition);

static void rsn_dec_dispose (GObject * gobj);
static void cleanup_child (RsnDec * self);

static GstBinClass *rsn_dec_parent_class = NULL;

static void
rsn_dec_class_init (RsnDecClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  GST_DEBUG_CATEGORY_INIT (rsn_dec_debug, "rsndec",
      0, "Resin DVD stream decoder");

  rsn_dec_parent_class = (GstBinClass *) g_type_class_peek_parent (klass);
  object_class->dispose = rsn_dec_dispose;

  element_class->change_state = GST_DEBUG_FUNCPTR (rsn_dec_change_state);
}

static gboolean
rsn_dec_sink_event (GstPad * pad, GstEvent * event)
{
  RsnDec *self = RSN_DEC (gst_pad_get_parent (pad));
  gboolean ret = TRUE;
  const GstStructure *s = gst_event_get_structure (event);
  const gchar *name = (s ? gst_structure_get_name (s) : NULL);

  if (name && g_str_equal (name, "application/x-gst-dvd"))
    ret = gst_pad_push_event (GST_PAD_CAST (self->srcpad), event);
  else
    ret = self->sink_event_func (pad, event);

  gst_object_unref (self);

  return ret;
}

static void
rsn_dec_init (RsnDec * self, RsnDecClass * klass)
{
  GstPadTemplate *templ;

  templ =
      gst_element_class_get_pad_template (GST_ELEMENT_CLASS (klass), "sink");
  g_assert (templ != NULL);
  self->sinkpad =
      GST_GHOST_PAD_CAST (gst_ghost_pad_new_no_target_from_template ("sink",
          templ));
  self->sink_event_func = GST_PAD_EVENTFUNC (self->sinkpad);
  gst_pad_set_event_function (GST_PAD_CAST (self->sinkpad),
      GST_DEBUG_FUNCPTR (rsn_dec_sink_event));

  templ = gst_element_class_get_pad_template (GST_ELEMENT_CLASS (klass), "src");
  g_assert (templ != NULL);
  self->srcpad =
      GST_GHOST_PAD_CAST (gst_ghost_pad_new_no_target_from_template ("src",
          templ));
  gst_element_add_pad (GST_ELEMENT (self), GST_PAD_CAST (self->sinkpad));
  gst_element_add_pad (GST_ELEMENT (self), GST_PAD_CAST (self->srcpad));
}

static void
rsn_dec_dispose (GObject * object)
{
  RsnDec *self = (RsnDec *) object;
  cleanup_child (self);

  G_OBJECT_CLASS (rsn_dec_parent_class)->dispose (object);
}

static void
child_pad_added (GstElement * element, GstPad * pad, RsnDec * self)
{
  GST_DEBUG_OBJECT (self, "New pad: %" GST_PTR_FORMAT, pad);
  gst_ghost_pad_set_target (self->srcpad, pad);

  gst_element_sync_state_with_parent (element);
}

static gboolean
rsn_dec_set_child (RsnDec * self, GstElement * new_child)
{
  GstPad *child_pad;
  if (self->current_decoder) {
    gst_ghost_pad_set_target (self->srcpad, NULL);
    gst_ghost_pad_set_target (self->sinkpad, NULL);
    gst_bin_remove ((GstBin *) self, self->current_decoder);
    self->current_decoder = NULL;
  }

  if (new_child == NULL)
    return TRUE;

  if (!gst_bin_add ((GstBin *) self, new_child))
    return FALSE;

  child_pad = gst_element_get_static_pad (new_child, "sink");
  if (child_pad == NULL) {
    return FALSE;
  }
  gst_ghost_pad_set_target (self->sinkpad, child_pad);
  gst_object_unref (child_pad);

  /* Listen for new pads from the decoder */
  g_signal_connect (G_OBJECT (new_child), "pad-added",
      G_CALLBACK (child_pad_added), self);

  GST_DEBUG_OBJECT (self, "Add child %" GST_PTR_FORMAT, new_child);
  self->current_decoder = new_child;

  /* not sure if we need this here, or if the one in child_pad_added
   * is sufficient..
   */
  gst_element_sync_state_with_parent (new_child);

  return TRUE;
}

static void
cleanup_child (RsnDec * self)
{
  GST_DEBUG_OBJECT (self, "Removing child element");
  (void) rsn_dec_set_child (self, NULL);
}

typedef struct
{
  GstCaps *desired_caps;
  GstCaps *decoder_caps;
} RsnDecFactoryFilterCtx;

static GstStateChangeReturn
rsn_dec_change_state (GstElement * element, GstStateChange transition)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  RsnDec *self = RSN_DEC (element);
  RsnDecClass *klass = RSN_DEC_GET_CLASS (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:{
      GstElement *new_child;

      new_child = gst_element_factory_make ("decodebin2", NULL);
      if (new_child == NULL || !rsn_dec_set_child (self, new_child))
        ret = GST_STATE_CHANGE_FAILURE;
      break;
    }
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      break;
    default:
      break;
  }

  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;
  ret =
      GST_ELEMENT_CLASS (rsn_dec_parent_class)->change_state (element,
      transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      cleanup_child (self);
      break;
    default:
      break;
  }

  return ret;
}

GType
rsn_dec_get_type (void)
{
  static volatile gsize type = 0;

  if (g_once_init_enter (&type)) {
    GType _type;
    static const GTypeInfo type_info = {
      sizeof (RsnDecClass),
      NULL,
      NULL,
      (GClassInitFunc) rsn_dec_class_init,
      NULL,
      NULL,
      sizeof (RsnDec),
      0,
      (GInstanceInitFunc) rsn_dec_init,
    };

    _type = g_type_register_static (GST_TYPE_BIN,
        "RsnDec", &type_info, G_TYPE_FLAG_ABSTRACT);
    g_once_init_leave (&type, _type);
  }
  return type;
}

/** Audio decoder subclass */
static GstStaticPadTemplate audio_sink_template =
    GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/mpeg,mpegversion=(int)1;"
        "audio/x-private1-lpcm;"
        "audio/x-private1-ac3;" "audio/ac3;" "audio/x-ac3;"
        "audio/x-private1-dts; audio/x-raw-float")
    );

static GstStaticPadTemplate audio_src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw-float, "
        "rate = (int) [ 1, MAX ], "
        "channels = (int) [ 1, MAX ], "
        "endianness = (int) BYTE_ORDER, "
        "width = (int) { 32, 64 }; "
        "audio/x-raw-int, "
        "rate = (int) [ 1, MAX ], "
        "channels = (int) [ 1, MAX ], "
        "endianness = (int) { 1234, 4321 },"
        "width = (int) [ 1, 32 ], "
        "depth = (int) [ 1, 32 ], " "signed = (boolean) { false, true }")
    );

G_DEFINE_TYPE (RsnAudioDec, rsn_audiodec, RSN_TYPE_DEC);

static void
rsn_audiodec_class_init (RsnAudioDecClass * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  RsnDecClass *dec_class = RSN_DEC_CLASS (klass);

  gst_element_class_add_static_pad_template (element_class,
      &audio_src_template);
  gst_element_class_add_static_pad_template (element_class,
      &audio_sink_template);

  gst_element_class_set_details_simple (element_class, "RsnAudioDec",
      "Audio/Decoder",
      "Resin DVD audio stream decoder", "Jan Schmidt <thaytan@noraisin.net>");
}

static void
rsn_audiodec_init (RsnAudioDec * self)
{
}

/** Video decoder subclass */
static GstStaticPadTemplate video_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/mpeg, "
        "mpegversion = (int) [ 1, 2 ], " "systemstream = (bool) FALSE")
    );

static GstStaticPadTemplate video_src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw-yuv")
    );

G_DEFINE_TYPE (RsnVideoDec, rsn_videodec, RSN_TYPE_DEC);

static void
rsn_videodec_class_init (RsnAudioDecClass * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  RsnDecClass *dec_class = RSN_DEC_CLASS (klass);

  gst_element_class_add_static_pad_template (element_class,
      &video_src_template);
  gst_element_class_add_static_pad_template (element_class,
      &video_sink_template);

  gst_element_class_set_details_simple (element_class, "RsnVideoDec",
      "Video/Decoder",
      "Resin DVD video stream decoder", "Jan Schmidt <thaytan@noraisin.net>");
}

static void
rsn_videodec_init (RsnVideoDec * self)
{
}
