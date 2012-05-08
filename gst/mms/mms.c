/**
 * Gstreamer
 *
 * Copyright (C) 2012 Collabora Ltd.
 *
 * Author: Thibault Saunier <thibault.saunier@collabora.com
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

#include "mms.h"
#include <string.h>

GST_DEBUG_CATEGORY (mms_utils_debug);
#define GST_CAT_DEFAULT mms_utils_debug

#define MMS_TIMEOUT          5  /* Seconds */
#define MMS_USER_AGENT       "NSPlayer/7.0.0.1956"
#define HEADER_SIZE          32
#define ASF_HEADER_SIZE      8

static gboolean debug_initialized = FALSE;

static gboolean send_receive_dance (MMSSession * session, GError ** err);
static inline void communication_sequence_next (MMSSession * session,
    GError ** err);
static void mms_prepare_command_packet (MMSSession * session,
    MMSPacket * packet, MMSCommand command, MMSCommand expected_resp);

/* Callback headers */
static void session_connected_cb (GSocketClient * socket_clt,
    GAsyncResult * res, MMSSession * session);

/* MMSPacket related  function */
static guint16 *
mms_utf8_to_utf16le (const gchar * utf8, guint * p_size)
{
  gunichar2 *utf16, *p;

  *p_size = 0;

  utf16 = g_utf8_to_utf16 (utf8, -1, NULL, NULL, NULL);

  if (utf16 == NULL)
    return NULL;

  for (p = utf16; *p != 0; ++p)
    *p = GUINT16_TO_LE (*p);

  *p_size = (guint) ((guint8 *) p - (guint8 *) utf16);

  return utf16;
}

static void
mms_packet_clean (MMSPacket * packet, gboolean free)
{
  if (free)
    g_free (packet->data);

  packet->mmsdata = NULL;
  packet->data = NULL;
  packet->size = -1;
  packet->mms_size = -1;
}

static inline void
add_command_header (MMSSession * session, GstByteWriter * bw, gsize data_len,
    gsize packet_len, MMSCommand command)
{
  gst_byte_writer_put_uint32_le_unchecked (bw, 0x00000001);
  gst_byte_writer_put_uint32_le_unchecked (bw, 0xb00bface);
  gst_byte_writer_put_uint32_le_unchecked (bw, data_len - 16);  /* Length */
  gst_byte_writer_put_uint32_le_unchecked (bw, GST_MAKE_FOURCC ('M', 'M', 'S', ' '));   /* Seal */
  gst_byte_writer_put_uint32_le_unchecked (bw, (packet_len - 16) / 8);  /* Chunk count */
  gst_byte_writer_put_uint32_le_unchecked (bw, session->seq_num++);
  gst_byte_writer_put_uint64_le_unchecked (bw, 0);      /* Timestamp */
  gst_byte_writer_put_uint32_le_unchecked (bw, (packet_len - HEADER_SIZE) / 8);
  gst_byte_writer_put_uint16_le_unchecked (bw, command);
  gst_byte_writer_put_uint16_le_unchecked (bw, MMS_DIRECTION_TO_SERVER);
  /* 40 command data */
}

static inline void
add_command_prefixes (GstByteWriter * bw, guint32 prefix1, guint32 prefix2)
{
  gst_byte_writer_put_uint32_le_unchecked (bw, prefix1);
  gst_byte_writer_put_uint32_le_unchecked (bw, prefix2);
}

static inline gboolean
prepare_command_protocol_select (MMSSession * session, GstByteWriter * bw,
    MMSPacket * packet)
{
  gchar *s;
  guint16 port;
  gchar *ip;
  guint16 *utf16le;
  GInetAddress *address;
  GSocketAddress *sockadd;
  guint utf16le_len, data_len, packet_len;

  GError *err = NULL;

  GST_DEBUG ("Preparing protocol selection packet (0x02");

  /* Get local adress and port */
  sockadd = g_socket_connection_get_local_address (session->con, &err);

  if (err != NULL) {
    ip = g_strdup ("192.168.0.129");
    port = 1037;

    GST_WARNING_OBJECT (session->elem, "Could not retrive local address,"
        "trying with standard address: %s, port %u", ip, port);
  } else {
    GInetSocketAddress *inetadd = G_INET_SOCKET_ADDRESS (sockadd);

    address = g_inet_socket_address_get_address (inetadd);
    ip = g_inet_address_to_string (address);
    port = g_inet_socket_address_get_port (inetadd);

    GST_DEBUG ("Local adress found: %s port: %u", ip, port);
  }

  /* FIXME Wy can't we use Real host/port ? */
  ip = g_strdup ("192.168.0.129");
  port = 1037;
  s = g_strdup_printf ("\\\\%s\\TCP\\%u", ip, port);
  utf16le = mms_utf8_to_utf16le (s, &utf16le_len);

  if (sockadd)
    g_object_unref (sockadd);
  g_clear_error (&err);
  g_free (s);
  g_free (ip);

  /* Calculating data/packet size */
  data_len = 40 + 8 + 14 + (utf16le_len + 2);
  packet_len = GST_ROUND_UP_8 (data_len);
  if (!gst_byte_writer_ensure_free_space (bw, packet_len)) {
    GST_WARNING_OBJECT (session->elem, "Could not retrive %u byte", packet_len);

    return FALSE;
  }

  add_command_header (session, bw, data_len, packet_len,
      MMS_COMMAND_PROTOCOL_SELECT);
  /* playIncarnation + maxBlockBytes */
  add_command_prefixes (bw, 0x00000000, 0xffffffff);
  gst_byte_writer_put_uint32_le_unchecked (bw, 0x00000000);     /* MaxFunnelBytes */
  gst_byte_writer_put_uint32_le_unchecked (bw, 0x00989680);     /* maxbiteRate */
  gst_byte_writer_put_uint32_le_unchecked (bw, 0x00000002);     /* funnelMode */

  gst_byte_writer_put_data_unchecked (bw, (guint8 *) utf16le, utf16le_len + 2);

  packet->mms_size = -1;        /* Unknown */
  packet->data = gst_byte_writer_reset_and_get_data (bw);
  packet->size = packet_len;

  g_free (utf16le);

  return TRUE;
}

static inline gboolean
prepare_command_timing_data_request (MMSSession * session, GstByteWriter * bw,
    MMSPacket * packet)
{
  guint data_len, packet_len;

  data_len = 40 + 8;
  packet_len = GST_ROUND_UP_8 (data_len);
  if (gst_byte_writer_ensure_free_space (bw, packet_len) == FALSE)
    return FALSE;

  GST_DEBUG ("Preparing timing data request packet %u", packet_len);

  add_command_header (session, bw, data_len, packet_len,
      MMS_COMMAND_TIMING_DATA_REQUEST);
  add_command_prefixes (bw, 0x00f0f0f0, 0x0004000b);

  /*GST_MEMDUMP ("timing data request", data, data_len); */
  packet->mms_size = -1;        /* Unknown */
  packet->data = gst_byte_writer_reset_and_get_data (bw);
  packet->size = packet_len;

  return TRUE;
}

static void
mms_gen_guid (gchar guid[])
{
  static char digit[16] = "0123456789ABCDEF";
  int i = 0;

  for (i = 0; i < 36; i++) {
    guid[i] = digit[g_random_int_range (0, 15)];
  }
  guid[8] = '-';
  guid[13] = '-';
  guid[18] = '-';
  guid[23] = '-';
  guid[36] = '\0';
}

static inline gboolean
prepare_command_initial (MMSSession * session, GstByteWriter * bw,
    MMSPacket * packet)
{
  gchar *s;
  guint16 *utf16le;
  guint utf16le_len, data_len, packet_len;

  s = g_strdup_printf ("%s; {%s}; Host: %s", MMS_USER_AGENT, session->guid,
      session->host);
  utf16le = mms_utf8_to_utf16le (s, &utf16le_len);
  g_free (s);

  if (utf16le == NULL)
    return FALSE;               /* FIXME: error out */

  data_len = 200;
  packet_len = GST_ROUND_UP_8 (data_len);
  if (!gst_byte_writer_ensure_free_space (bw, packet_len)) {
    GST_WARNING_OBJECT (session->elem, "Could not retrive %u byte", packet_len);
    return FALSE;
  }

  GST_DEBUG ("Preparing initial command packet");

  add_command_header (session, bw, data_len, packet_len, MMS_COMMAND_INITIAL);
  add_command_prefixes (bw, 0xf0f0f0f0, 0x0004000b);
  /* ViewerToMacProtocolRevision */
  gst_byte_writer_put_uint32_le_unchecked (bw, 0x0003001c);
  /* FIXME subscriberName */
  gst_byte_writer_put_data_unchecked (bw, (guint8 *) utf16le, utf16le_len + 2);

  packet->mms_size = -1;        /* Unknown */
  packet->data = gst_byte_writer_reset_and_get_data (bw);
  packet->size = packet_len;

  return TRUE;
}

static inline gboolean
prepare_command_media_file_request (MMSSession * session, GstByteWriter * bw,
    MMSPacket * packet)
{
  guint16 *utf16le;
  guint utf16le_len, data_len, packet_len;

  utf16le = mms_utf8_to_utf16le (session->path + 1, &utf16le_len);

  if (utf16le == NULL)
    return FALSE;               /* FIXME: error out */

  data_len = 88;
  packet_len = GST_ROUND_UP_8 (data_len);
  if (!gst_byte_writer_ensure_free_space (bw, packet_len)) {
    GST_WARNING_OBJECT (session->elem, "Could not retrive %u byte", packet_len);
    return FALSE;
  }

  GST_DEBUG ("Preparing media file request command packet %i, path %s",
      packet->type, session->path + 1);

  add_command_header (session, bw, data_len, packet_len,
      MMS_COMMAND_MEDIA_FILE_REQUEST);
  add_command_prefixes (bw, 1, 0xffffffff);
  gst_byte_writer_put_uint32_be_unchecked (bw, 0);
  gst_byte_writer_put_uint32_be_unchecked (bw, 0);
  gst_byte_writer_put_data_unchecked (bw, (guint8 *) utf16le, utf16le_len + 2);

  packet->mms_size = -1;
  packet->size = packet_len;
  packet->data = gst_byte_writer_reset_and_get_data (bw);

  return TRUE;
}

static inline gboolean
prepare_command_media_header_request (MMSSession * session, GstByteWriter * bw,
    MMSPacket * packet)
{
  guint data_len, packet_len;

  if (session->finfos == NULL) {
    GST_WARNING ("No MMSFileInfo yet, can't create media header request");
    return FALSE;
  }

  data_len = 40 + 48;
  packet_len = GST_ROUND_UP_8 (data_len);
  if (!gst_byte_writer_ensure_free_space (bw, packet_len)) {
    GST_WARNING_OBJECT (session->elem, "Could not retrive %u byte", packet_len);
    return FALSE;
  }

  GST_DEBUG ("Preparing media file request command packet %i");

  add_command_header (session, bw, data_len, packet_len,
      MMS_COMMAND_MEDIA_HEADER_REQUEST);
  add_command_prefixes (bw, 0, 1);      //session->finfos->open_file_id, 0x00000000);
  gst_byte_writer_put_uint32_le_unchecked (bw, 0x00000000);     /* Offset */
  gst_byte_writer_put_uint32_le_unchecked (bw, 0x00008000);     /* length */
  gst_byte_writer_put_uint32_le_unchecked (bw, 0xffffffff);     /* flags */
  gst_byte_writer_put_uint32_le_unchecked (bw, 0x00000000);     /* padding */
  gst_byte_writer_put_float64_le_unchecked (bw, 0x0000000);     /* tEarliest */
  gst_byte_writer_put_float64_le_unchecked (bw, 3600);  /* tDeadline */
  gst_byte_writer_put_uint32_le_unchecked (bw, 0x0000002);      /* playIncarnation */
  gst_byte_writer_put_uint32_le_unchecked (bw, 0x0000000);      /* playSequence */

  packet->mms_size = -1;
  packet->size = packet_len;
  packet->data = gst_byte_writer_reset_and_get_data (bw);

  return TRUE;
}

static inline gboolean
prepare_command_start_from_id (MMSSession * session, GstByteWriter * bw,
    MMSPacket * packet)
{
  guint data_len, packet_len;

  if (session->finfos == NULL)
    return FALSE;

  data_len = 40 + 32;
  packet_len = GST_ROUND_UP_8 (data_len);
  if (!gst_byte_writer_ensure_free_space (bw, packet_len)) {
    GST_WARNING_OBJECT (session->elem, "Could not retrive %u byte", packet_len);
    return FALSE;
  }

  GST_DEBUG ("Preparing start from id command packet");

  add_command_header (session, bw, data_len, packet_len,
      MMS_COMMAND_START_FROM_ID);
  add_command_prefixes (bw, 1, 0x0001ffff);
  gst_byte_writer_put_uint64_le_unchecked (bw, 0);
  gst_byte_writer_put_uint32_le_unchecked (bw, 0xffffffff);
  gst_byte_writer_put_uint32_le_unchecked (bw, 0xffffffff);
  gst_byte_writer_put_uint32_le_unchecked (bw, 0xffffff00);
  gst_byte_writer_put_uint32_le_unchecked (bw, 0x04000000);

  packet->mms_size = -1;
  packet->size = packet_len;
  packet->data = gst_byte_writer_reset_and_get_data (bw);

  return TRUE;
}

static inline gboolean
prepare_command_stream_id_request (MMSSession * session, GstByteWriter * bw,
    MMSPacket * packet)
{
  guint data_len, packet_len;

  if (session->finfos == NULL)
    return FALSE;

  data_len = 40 + 24;
  packet_len = GST_ROUND_UP_8 (data_len);
  if (!gst_byte_writer_ensure_free_space (bw, packet_len)) {
    GST_WARNING_OBJECT (session->elem, "Could not retrive %u byte", packet_len);
    return FALSE;
  }

  GST_DEBUG ("Preparing stream id request command packet packet %i");

  add_command_header (session, bw, data_len, packet_len,
      MMS_COMMAND_STREAM_ID_REQUEST);
  /*add_command_prefixes (bw, 8, 0xffff); */
  gst_byte_writer_put_uint32_le_unchecked (bw, 0);
  gst_byte_writer_put_uint16_le_unchecked (bw, 0xffff);
  gst_byte_writer_put_uint16_le_unchecked (bw, 1);
  gst_byte_writer_put_uint16_le_unchecked (bw, 0);
  gst_byte_writer_put_uint16_le_unchecked (bw, 0x0000);

  packet->mms_size = -1;
  packet->size = packet_len;
  packet->data = gst_byte_writer_reset_and_get_data (bw);

  return TRUE;
}

static void
mms_prepare_command_packet (MMSSession * session, MMSPacket * packet,
    MMSCommand command, MMSCommand expected_resp)
{
  GstByteWriter bw;
  const gchar *command_type;

  gst_byte_writer_init (&bw);

  packet->direction = MMS_DIRECTION_TO_SERVER;
  packet->expected_resp = expected_resp;

  switch (command) {
    case MMS_COMMAND_NONE:
      packet->command = MMS_COMMAND_NONE;
      packet->type = MMS_PACKET_COMMAND;

      return;
    case MMS_COMMAND_INITIAL:
      if (!prepare_command_initial (session, &bw, packet))
        return;

      command_type = "Initial command";
      break;
    case MMS_COMMAND_TIMING_DATA_REQUEST:
    {
      if (!prepare_command_timing_data_request (session, &bw, packet))
        return;

      command_type = "Time request command";
      break;
    }
    case MMS_COMMAND_PROTOCOL_SELECT:
      if (!prepare_command_protocol_select (session, &bw, packet))
        return;

      command_type = "Protocol selection command";
      break;
    case MMS_COMMAND_MEDIA_FILE_REQUEST:
      if (!prepare_command_media_file_request (session, &bw, packet))
        return;

      command_type = "File request command";
      break;
    case MMS_COMMAND_MEDIA_HEADER_REQUEST:
      if (!prepare_command_media_header_request (session, &bw, packet))
        return;

      command_type = "Media header request command";
      break;
    case MMS_COMMAND_STREAM_ID_REQUEST:
      if (!prepare_command_stream_id_request (session, &bw, packet))
        return;

      command_type = "Media header request command";
      break;
    case MMS_COMMAND_START_FROM_ID:
      if (!prepare_command_start_from_id (session, &bw, packet))
        return;

      command_type = "Start from ID request command";
      break;
    default:
      GST_WARNING ("Can't prepare %u packets", command);
      /* FIXME what should we do here? */
      return;
  }

  packet->type = MMS_PACKET_COMMAND;
  packet->command = command;
  GST_MEMDUMP (command_type, packet->data, packet->size);
}

static inline gboolean
packet_is_header (guint8 * data, gsize len)
{
  if (len < 16)
    return FALSE;

  /* Make sure the protocol is correctly set to MMS */
  if (GST_READ_UINT32_LE (data + 12) != 0x20534d4d)
    return FALSE;

  return TRUE;
}

static inline gboolean
parse_header (MMSPacket * packet, guint8 * data, gsize len)
{
  gsize missing_size, hdr_size = HEADER_SIZE;

  if (len < 32)
    goto missing_data;

  /* Command packet */
  if (GST_READ_UINT32_BE (data + 4) == 0xcefa0bb0) {

    packet->flags = data[3];
    packet->mms_size = (GST_READ_UINT32_LE (data + 16) * 8) - 16;

    GST_DEBUG ("Command packet found, MMS size %" G_GSIZE_FORMAT,
        packet->mms_size);

    if (packet->mms_size > MMS_MAX_PACKET_SIZE - 12) {
      GST_WARNING ("Declared buffer size %" G_GSIZE_FORMAT
          " (%u) bigger than the maximum (%u)", packet->mms_size,
          MMS_MAX_PACKET_SIZE);
      goto error;
    }

    missing_size = packet->mms_size;
    packet->type = MMS_PACKET_COMMAND;
  } else {

    packet->sequence = GST_READ_UINT32_LE (data);
    packet->type = data[4];
    packet->flags = data[5];
    packet->mms_size = ((GST_READ_UINT16_LE (data + 6)) & 0xffff);
    hdr_size = ASF_HEADER_SIZE;

    if (packet->type == MMS_PACKET_ASF_HEADER) {
      GST_DEBUG ("MMS_PACKET_ASF_HEADER packet found, size %" G_GSIZE_FORMAT,
          packet->mms_size);
      /* We have already got 32 bytes, make sure to take it into account */
      missing_size = packet->mms_size - 32;

      packet->mms_size -= ASF_HEADER_SIZE;

    } else {
      packet->type = MMS_PACKET_ASF_MEDIA;
      missing_size = packet->mms_size;
      packet->mms_size -= ASF_HEADER_SIZE;

      GST_DEBUG ("MMS_PACKET_ASF_MEDIA packet found, size %" G_GSIZE_FORMAT,
          packet->mms_size);
    }
  }

  packet->size = len;
  /* Make sure we have space for the full packet size */
  packet->data = g_realloc (data, packet->mms_size + HEADER_SIZE);
  packet->mmsdata = packet->data + hdr_size;
  packet->cdata = packet->data + HEADER_SIZE;
  packet->missing_size = missing_size;
  GST_DEBUG ("MISSING %i", missing_size);

  return TRUE;

missing_data:
  {
    GST_WARNING ("Missing data to get packet size %" G_GSIZE_FORMAT, len);
    packet->type = MMS_PACKET_MISSING_DATA;

    return FALSE;
  }
error:
  {
    packet->type = MMS_PACKET_ERROR;
    return FALSE;
  }
}

static inline void
set_data (MMSPacket * packet, guint8 * data, gsize size)
{
  packet->data = packet->mmsdata = packet->cdata = data;
  packet->size = packet->missing_size = size;
}

static inline void
read_check32 (GstByteReader * br, guint32 * val, guint32 expected)
{
  *val = gst_byte_reader_get_uint32_le_unchecked (br);
  if (*val != expected)
    GST_DEBUG ("Wrong value 0x%08x, expected 0x%08x", *val, expected);
}

static inline void
parse_report_open_file (MMSPacket * packet)
{
  GstByteReader br;
  MMSFileInfos *finfos = &packet->finfos;

  GST_DEBUG ("Parsing initial command");
  gst_byte_reader_init (&br, packet->mmsdata, packet->mms_size);


  finfos->chunk_len = gst_byte_reader_get_uint32_le_unchecked (&br);
  read_check32 (&br, &finfos->mid, 0x00040006);
  finfos->hr = gst_byte_reader_get_uint32_le_unchecked (&br);
  finfos->play_incarnation = gst_byte_reader_get_uint32_le_unchecked (&br);
  finfos->open_file_id = gst_byte_reader_get_uint32_le_unchecked (&br);
  read_check32 (&br, &finfos->padding, 0x00000000);
  read_check32 (&br, &finfos->name, 0x00000000);
  finfos->attributes = gst_byte_reader_get_uint32_le_unchecked (&br);
  finfos->duration = gst_byte_reader_get_float64_le_unchecked (&br);
  finfos->blocks = gst_byte_reader_get_uint32_le_unchecked (&br);

  /* Unused 1 */
  gst_byte_reader_skip_unchecked (&br, 16);

  finfos->packet_size = gst_byte_reader_get_uint32_le_unchecked (&br);
  finfos->packet_count = gst_byte_reader_get_uint64_le_unchecked (&br);
  finfos->bit_rate = gst_byte_reader_get_uint32_le_unchecked (&br);
  finfos->header_size = gst_byte_reader_get_uint32_le_unchecked (&br);
  /* Unused 2 */

  GST_DEBUG ("File ID: 0x%08x, Attributes: 0x%08x, duration (seconds): %f "
      "Blocks: %u, packet size: %u, packet count: %u, BitRate: %u,"
      "ASF header size: %u", finfos->open_file_id, finfos->attributes,
      finfos->duration, finfos->blocks, finfos->packet_size,
      finfos->packet_count, finfos->bit_rate, finfos->header_size);
}

static inline void
parse_report_connected (MMSPacket * packet)
{
  GstByteReader br;
  LinkMacToViewerReportConnectedEX *report = &packet->initial;

  GST_DEBUG ("Parsing initial command");
  gst_byte_reader_init (&br, packet->mmsdata, packet->mms_size);

  report->chunk_len = gst_byte_reader_get_uint32_le_unchecked (&br);
  report->mid = gst_byte_reader_get_uint32_le_unchecked (&br);
  report->hr = gst_byte_reader_get_uint32_le_unchecked (&br);
  report->play_incarnation = gst_byte_reader_get_uint32_le_unchecked (&br);
  report->mac_to_viewer_protocol_revision =
      gst_byte_reader_get_uint32_le_unchecked (&br);
  report->viewer_to_mac_protocol_revision =
      gst_byte_reader_get_uint32_le_unchecked (&br);
  report->block_group_play_time =
      gst_byte_reader_get_float32_le_unchecked (&br);
  report->block_group_blocks = gst_byte_reader_get_uint32_le_unchecked (&br);
  report->n_max_open_files = gst_byte_reader_get_uint32_le_unchecked (&br);
  report->n_block_max_bytes = gst_byte_reader_get_uint32_le_unchecked (&br);
  report->max_bit_rate = gst_byte_reader_get_uint32_le_unchecked (&br);
  report->cb_server_version_info =
      gst_byte_reader_get_uint32_le_unchecked (&br);
  report->cb_version_info = gst_byte_reader_get_uint32_le_unchecked (&br);
  report->cb_version_url = gst_byte_reader_get_uint32_le_unchecked (&br);
  report->cb_authen_package = gst_byte_reader_get_uint32_le_unchecked (&br);
  report->server_version_info =
      (gunichar2 *) gst_byte_reader_get_data_unchecked (&br,
      report->cb_server_version_info);
  report->version_info =
      (gunichar2 *) gst_byte_reader_get_data_unchecked (&br,
      report->cb_version_info);
  report->version_url =
      (gunichar2 *) gst_byte_reader_get_data_unchecked (&br,
      report->cb_version_url);
}

static inline gboolean
parse_command (MMSPacket * packet, const guint8 * data, gsize size)
{
  packet->command = GST_READ_UINT32_LE (data + 36) & 0xffff;

  GST_DEBUG ("received command = 0x%02x", packet->command);

  switch (packet->command) {
    case MMS_COMMAND_INITIAL:
      parse_report_connected (packet);
      break;
    case MMS_COMMAND_MEDIA_FILE_OPEN:
      parse_report_open_file (packet);
      break;
      break;
    default:
      GST_DEBUG ("Unhandled command received 0x%02x", packet->command);
      break;
  }
  return TRUE;
}

static gboolean
mms_packet_parse (MMSPacket * packet)
{                               /* /, guint8 * data, gsize size) */
  gboolean res = FALSE;

  GST_DEBUG ("Parsing packet");

  switch (packet->type) {
    case MMS_PACKET_ERROR:
    case MMS_PACKET_MISSING_DATA:
      break;
    case MMS_PACKET_COMMAND:
      res = parse_command (packet, packet->data, packet->size);
      break;
    default:
      /*FIXME do something */
      break;
  }

  return res;
}

static inline gboolean
handle_packet_data (MMSSession * session, MMSPacket * packet, guint8 * data,
    gint64 rcv_size, gboolean is_header, GError ** err)
{
  GST_DEBUG ("Handle data %p, size %i, err %p, is_header %i",
      data, rcv_size, *err, is_header);

  if (rcv_size > 0) {
    g_clear_error (err);

    if (is_header || packet_is_header (packet->cdata, rcv_size)) {
      if (!parse_header (packet, data, rcv_size))
        goto error_parsing;

      GST_MEMDUMP ("Header:", data, rcv_size);
    }

    return TRUE;

  } else if (rcv_size == 0) {
    g_clear_error (err);

    GST_DEBUG ("EOF.... this shouldn't happend here");

    session->flow = GST_FLOW_ERROR;
    g_cond_signal (session->connected_cond);

    return FALSE;

  } else {
    /* An error accured */
    GST_WARNING ("Didn't receive data, reson %s", *err ? (*err)->message :
        "Unknown");

    return FALSE;
  }

error_parsing:
  GST_WARNING_OBJECT (session->elem, "Error parsing");

  return FALSE;
}

static gboolean
send (MMSSession * session, MMSPacket * packet, GError ** err)
{
  gsize written;

  GST_DEBUG ("Sending %" G_GSIZE_FORMAT " bytes", packet->size);
  written = g_output_stream_write (session->ostream,
      packet->data, packet->size, NULL, err);

  if (g_error_matches (*err, G_IO_ERROR, G_IO_ERROR_WOULD_BLOCK)) {
    GST_DEBUG ("Writing would block");

    return FALSE;

  } else if (written > 0) {
    g_clear_error (err);

    GST_DEBUG ("%u bytes sent", written);
    session->needs_write_watch = FALSE;
  } else if (written == 0) {
    if (*err)
      GST_WARNING ("Error writing: ", (*err)->message);

    g_clear_error (err);

    GST_DEBUG ("EOF makes no sense...");

    return FALSE;
  } else {
    /* An error accured */
    GST_WARNING ("Didn't write data, reson %s", *err ? (*err)->message :
        "Unknown");

    g_clear_error (err);
    return FALSE;
  }

  return TRUE;
}


static gboolean
receive_packet (MMSSession * session, MMSPacket * packet, GError ** err)
{
  gint rcv_size;
  guint8 *data = NULL;

  if (packet->data == NULL) {
    /* Getting Header */
    GST_DEBUG ("Getting the %i bytes Header", HEADER_SIZE);

    data = g_malloc (HEADER_SIZE);
    rcv_size = g_input_stream_read (session->istream, data,
        HEADER_SIZE, NULL, err);

    if (handle_packet_data (session, packet, data, rcv_size, TRUE,
            err) == FALSE)
      return FALSE;

  }

  GST_DEBUG ("Getting %" G_GSIZE_FORMAT, packet->missing_size);
  rcv_size = g_input_stream_read (session->istream,
      packet->cdata, packet->missing_size, NULL, err);

  if (rcv_size < packet->missing_size) {
    /* Properly reset packet attributes */
    packet->cdata += rcv_size;
    packet->missing_size -= rcv_size;
    packet->size += rcv_size;

    /* We check we didn't get a command, in case we were expecting data */
    if (packet_is_header (packet->cdata, rcv_size)) {
      parse_header (packet, packet->cdata, rcv_size);

      mms_packet_parse (packet);

      /* If it is the case, we just reply to the command if needed */
      if (packet->type == MMS_PACKET_COMMAND &&
          session->r_packet.expected_resp == MMS_COMMAND_NONE) {
        g_clear_error (err);

        GST_DEBUG ("Got a command packet, following up");
        communication_sequence_next (session, err);
        return FALSE;
      }
    }

    GST_DEBUG ("Data still missing  %" G_GSIZE_FORMAT, packet->missing_size);

    return FALSE;
  }

  packet->size = packet->mms_size + HEADER_SIZE;

  if (!handle_packet_data (session, packet, packet->mmsdata, rcv_size, FALSE,
          err))
    return FALSE;

  mms_packet_parse (packet);
  GST_MEMDUMP ("Received packet: ", packet->mmsdata, packet->mms_size);

  if (packet->type == MMS_PACKET_COMMAND &&
      packet->command != packet->expected_resp) {
    GST_WARNING_OBJECT (session->elem, "Unexpected command: 0x%02x received"
        " instead of 0x%02x", packet->command, packet->expected_resp);
  }

  /* Make sure to keep the File infos in the session */
  if (packet->type == MMS_PACKET_COMMAND &&
      packet->command == MMS_COMMAND_MEDIA_FILE_OPEN) {

    if (session->finfos != NULL)
      memcpy (session->finfos, &packet->finfos, sizeof (MMSFileInfos));
    else
      session->finfos = g_memdup (&packet->finfos, sizeof (MMSFileInfos));
  }

  GST_DEBUG ("Packet type: 0x%02x received, next sequence...", packet->type);

  g_clear_error (err);
  communication_sequence_next (session, err);

  return TRUE;
}

static gboolean
send_receive_dance (MMSSession * session, GError ** err)
{
  MMSPacket *wpckt = &session->w_packet, *rpckt = &session->r_packet;

  if (send (session, wpckt, err) == FALSE)
    return FALSE;

  mms_packet_clean (wpckt, TRUE);
  mms_packet_clean (rpckt, TRUE);
  g_clear_error (err);
  if (receive_packet (session, rpckt, err) == FALSE)
    return FALSE;

  return TRUE;
}

static inline void
communication_sequence_next (MMSSession * session, GError ** err)
{
  MMSPacket *wpckt = &session->w_packet, *rpckt = &session->r_packet;

  switch (rpckt->type) {
    case MMS_PACKET_COMMAND:
    {
      MMSCommand next_command = MMS_COMMAND_NONE;
      MMSCommand expected_resp = MMS_COMMAND_NONE;

      switch (rpckt->command) {
        case MMS_COMMAND_NONE:
          expected_resp = next_command = MMS_COMMAND_INITIAL;
          break;
        case MMS_COMMAND_INITIAL:
          expected_resp = next_command = MMS_COMMAND_PROTOCOL_SELECT;
          break;
        case MMS_COMMAND_MEDIA_HEADER_REQUEST:
          expected_resp = next_command = MMS_COMMAND_PROTOCOL_SELECT;
          break;
        case MMS_COMMAND_PROTOCOL_SELECT_ERROR:
          GST_WARNING ("Protocol selection error ");
          /* Keep trying anyway */
        case MMS_COMMAND_PROTOCOL_SELECT:
          next_command = MMS_COMMAND_MEDIA_FILE_REQUEST;
          expected_resp = MMS_COMMAND_MEDIA_FILE_OPEN;
          break;
        case MMS_COMMAND_MEDIA_FILE_OPEN:
          next_command = MMS_COMMAND_MEDIA_HEADER_REQUEST;
          expected_resp = MMS_COMMAND_MEDIA_HEADER_RESPONSE;
          break;
        case MMS_COMMAND_MEDIA_HEADER_RESPONSE:
          next_command = MMS_COMMAND_STREAM_ID_REQUEST;
          expected_resp = MMS_COMMAND_MEDIA_FILE_REQUEST;
          break;
        case MMS_COMMAND_STREAM_SELECTION_INDICATOR:
          next_command = MMS_COMMAND_START_FROM_ID;
          expected_resp = MMS_COMMAND_MEDIA_FILE_REQUEST;
          /* Getting ASF headers */
          receive_packet (session, rpckt, err);
          break;
        case MMS_COMMAND_MEDIA_FILE_REQUEST:
          next_command = MMS_COMMAND_NONE;
          rpckt->expected_resp = MMS_COMMAND_NONE;
          mms_session_fill_buffer (session, session->cbuf, err);
          break;
        default:
          GST_ERROR ("FIXME, handle command type 0x%02x", rpckt->command);
          break;
      }

      if (next_command != MMS_COMMAND_NONE) {
        mms_prepare_command_packet (session, wpckt, next_command,
            expected_resp);
        rpckt->expected_resp = expected_resp;
        send_receive_dance (session, err);
      }

      break;
    }
    default:
      if (rpckt->type == MMS_PACKET_ASF_HEADER) {
        GST_DEBUG ("Keeping ASF header");
        /* We keep the ASF header so we can use it when needed */
        g_clear_error (err);
        session->initialized = TRUE;
        session->asf_header = gst_buffer_new ();
        GST_BUFFER_DATA (session->asf_header) = rpckt->mmsdata;
        GST_BUFFER_SIZE (session->asf_header) = rpckt->mms_size;

        /* Lose our reference to the data */
        rpckt->data = NULL;
        /* And let the flow happen */
        g_cond_signal (session->connected_cond);

        receive_packet (session, rpckt, err);
      } else {
        /* The buffer is filled */
        session->filled = TRUE;
        g_cond_signal (session->buf_ready);
      }

      break;
  }
}

/* MMS session API */
void
mms_session_clean (MMSSession * session)
{
  if (session->con) {
    g_object_unref (session->con);
    session->con = NULL;
  }

  if (session->connectable) {
    g_object_unref (session->connectable);
    session->connectable = NULL;
  }

  if (session->istream) {
    g_object_unref (session->istream);
    session->istream = NULL;
  }

  if (session->read_watch) {
    g_object_unref (session->read_watch);
    session->read_watch = NULL;
  }

  if (session->context) {
    g_main_context_unref (session->context);
    session->context = NULL;
  }

  if (session->path) {
    g_free (session->path);
    session->path = NULL;
  }

  if (session->finfos) {
    g_free (session->finfos);
    session->finfos = NULL;
  }

  if (session->connected_cond) {
    g_cond_free (session->connected_cond);
    session->connected_cond = NULL;
  }

  if (session->buf_ready) {
    g_cond_free (session->buf_ready);
    session->buf_ready = NULL;
  }

  mms_packet_clean (&session->w_packet, TRUE);
  mms_packet_clean (&session->r_packet, TRUE);
}

void
mms_session_init (MMSSession * session, GstElement * elem)
{
  if (!debug_initialized) {
    GST_DEBUG_CATEGORY_INIT (mms_utils_debug, "mms", 0, "MMS utils");
    debug_initialized = TRUE;
  }

  session->seq_num = 0;
  mms_gen_guid (session->guid);

  session->con = NULL;
  session->host = NULL;
  session->path = NULL;
  session->elem = elem;
  session->finfos = NULL;
  session->istream = NULL;
  session->read_watch = NULL;
  session->write_watch = NULL;
  session->flow = GST_FLOW_OK;
  session->initialized = FALSE;
  session->asf_header = NULL;
  session->connected_cond = g_cond_new ();
  session->buf_ready = g_cond_new ();
}


gint
mms_session_connect (MMSSession * session, const gchar * uri,
    GMainContext * context)
{
  GError *err = NULL;
  GNetworkAddress *add;
  const gchar *path;

  if (uri == NULL || *uri == '\0')
    goto no_uri;

  session->context = context;

  session->connectable = g_network_address_parse_uri (uri, 1755, &err);
  if (err)
    goto no_uri;

  GST_INFO_OBJECT (session->elem, "Connecting to %s:%u ...", uri, 1755);

  /* Connecting to the host */
  session->socket_clt = g_socket_client_new ();
  g_socket_client_set_timeout (session->socket_clt, MMS_TIMEOUT);
  g_socket_client_connect_async (session->socket_clt,
      session->connectable, NULL, (GAsyncReadyCallback) session_connected_cb,
      session);

  add = G_NETWORK_ADDRESS (session->connectable);
  session->host = g_network_address_get_hostname (add);
  path = strstr (uri, session->host) + strlen (session->host);

  session->path = *path == 0 ? g_strdup ("/") : g_strdup (path);

  return 1;

no_uri:
  return -1;
}

gboolean
mms_session_is_seekable (MMSSession * session)
{
  if (session->finfos == NULL)
    return FALSE;

  return session->finfos->attributes & MMS_FILE_ATTRIBUTE_CAN_SEEK;
}

gboolean
mms_session_fill_buffer (MMSSession * session, GstBuffer ** buf, GError ** err)
{
  GError *merr = NULL;

  session->filled = FALSE;

  if (G_UNLIKELY (session->asf_header)) {
    GST_DEBUG ("Filling with ASF header");

    /* Use the asf header */
    *buf = session->asf_header;
    session->asf_header = NULL;
    session->filled = TRUE;
    g_cond_signal (session->buf_ready);

    return TRUE;
  }

  /* Set the buffer we are currently filling up */
  session->cbuf = buf;

  /* We are still waiting for command packet, let's get them first */
  if (G_UNLIKELY (session->r_packet.expected_resp != MMS_COMMAND_NONE)) {
    mms_packet_clean (&session->r_packet, TRUE);
    receive_packet (session, &session->r_packet, &merr);

    return FALSE;
  }

  GST_DEBUG ("Filling buffer, size %" G_GSIZE_FORMAT,
      (gsize) session->finfos->packet_size);

  if (*buf == NULL) {
    *buf = gst_buffer_try_new_and_alloc (session->finfos->packet_size);

    if (*buf == NULL) {
      return FALSE;
    }
  }

  GST_DEBUG ("Filling buffer, size %" G_GSIZE_FORMAT, GST_BUFFER_SIZE (*buf));
  set_data (&session->r_packet, GST_BUFFER_DATA (*buf), GST_BUFFER_SIZE (*buf));
  session->r_packet.type = MMS_PACKET_ASF_MEDIA;
  session->w_packet.type = MMS_PACKET_NONE;

  receive_packet (session, &session->r_packet, &merr);

  return TRUE;
}

/* Callbacks */
static void
session_connected_cb (GSocketClient * socket_clt, GAsyncResult * res,
    MMSSession * session)
{
  GIOStream *ios;

  GError *err = NULL;

  GST_DEBUG ("Connection Done");

  session->con = g_socket_client_connect_finish (socket_clt, res, &err);
  if (err) {
    GST_ERROR_OBJECT (session->elem, "Connection error: %s", err->message);

    /* Can't connect, stop can't go further */
    session->flow = GST_FLOW_ERROR;
    g_cond_signal (session->connected_cond);

    return;
  }

  /* Keeping connection informations */
  ios = G_IO_STREAM (session->con);
  session->istream = g_io_stream_get_input_stream (ios);
  session->ostream = g_io_stream_get_output_stream (ios);

  GST_INFO_OBJECT (session->elem, "Connection successfully done.. "
      "start communications");

  mms_prepare_command_packet (session, &session->r_packet, MMS_COMMAND_NONE,
      MMS_COMMAND_NONE);

  /* Start communication chain */
  communication_sequence_next (session, &err);
  if (err) {
    GST_DEBUG ("Got error \"%s\", let the connection chain running anyway",
        err->message);

    g_clear_error (&err);
  }

}
