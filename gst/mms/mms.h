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

#ifndef MMS_H
#define MMS_H

#include <gio/gio.h>
#include <gst/gst.h>
#include <gst/base/gstbytewriter.h>

#define MMS_MAX_PACKET_SIZE          102400
#define MMS_MAX_ASF_HEADER_SIZE      (8192 * 2)

typedef struct _MMSPacket MMSPacket;
typedef struct _MMSContext MMSSession;

typedef enum
{
  MMS_DIRECTION_TO_SERVER = 0x03,
  MMS_DIRECTION_TO_CLIENT = 0x04
} MMSDirection;

typedef enum {
  MMS_PACKET_ERROR        = -1,
  MMS_PACKET_NONE         = 0,
  MMS_PACKET_COMMAND      = 1,
  MMS_PACKET_ASF_HEADER   = 2,
  MMS_PACKET_MISSING_DATA = 3, /* To determine the size of the packet */
  MMS_PACKET_ASF_MEDIA    = 4
} MMSPacketType;


typedef enum {
  MMS_COMMAND_NONE                       = 0x00,
  MMS_COMMAND_INITIAL                    = 0x01,
  MMS_COMMAND_PROTOCOL_SELECT            = 0x02,
  MMS_COMMAND_PROTOCOL_SELECT_ERROR      = 0x03,
  MMS_COMMAND_MEDIA_FILE_REQUEST         = 0x05,
  MMS_COMMAND_MEDIA_FILE_OPEN            = 0x06,
  MMS_COMMAND_START_FROM_ID              = 0x07,
  MMS_COMMAND_STREAM_PAUSE               = 0x09,
  MMS_COMMAND_STREAM_CLOSE               = 0x0d,
  MMS_COMMAND_MEDIA_HEADER_RESPONSE      = 0x11,
  MMS_COMMAND_MEDIA_HEADER_REQUEST       = 0x15,
  MMS_COMMAND_TIMING_DATA_REQUEST        = 0x18,
  MMS_COMMAND_USER_PASSWORD              = 0x1a,
  MMS_COMMAND_KEEPALIVE                  = 0x1b,
  MMS_COMMAND_STREAM_SELECTION_INDICATOR = 0x21,
  MMS_COMMAND_STREAM_ID_REQUEST          = 0x33,
} MMSCommand;

typedef enum {
  MMS_FILE_ATTRIBUTE_CAN_STRIDE = 0x00800000,
  MMS_FILE_ATTRIBUTE_CAN_SEEK   = 0x01000000,
  MMS_FILE_ATTRIBUTE_BROADCAST  = 0x02000000,
  MMS_FILE_ATTRIBUTE_LIVE       = 0x04000000,
  MMS_FILE_ATTRIBUTE_PLAYLIST   = 0x40000000
} MMSFileAttribute;

typedef struct {
  guint32    chunk_len;
  guint32    mid;
  guint32    hr;
  guint32    play_incarnation;
  guint32    mac_to_viewer_protocol_revision;
  guint32    viewer_to_mac_protocol_revision;
  gfloat     block_group_play_time;
  guint32    block_group_blocks;
  guint32    n_max_open_files;
  guint32    n_block_max_bytes;
  guint32    max_bit_rate;
  guint32    cb_server_version_info;
  guint32    cb_version_info;
  guint32    cb_version_url;
  guint32    cb_authen_package;
  gunichar2 *server_version_info;
  gunichar2 *version_info;
  gunichar2 *version_url;
  gunichar2 *authen_package;

} LinkMacToViewerReportConnectedEX;

typedef struct {
  guint32           chunk_len;
  guint32           mid;
  guint32           hr;
  guint32           play_incarnation;
  guint32           open_file_id;
  guint32           padding;
  guint32           name;
  MMSFileAttribute  attributes;
  gdouble           duration;
  guint32           blocks;
  guint32           packet_size;
  guint64           packet_count;
  guint32           bit_rate;
  guint32           header_size;
} MMSFileInfos;

struct _MMSPacket {
  MMSPacketType       type;

  /* Wether the packet is meant to be written or read */
  MMSDirection  direction;

  /* Wheter the packet is complet or not */
  gboolean            complete;

  /* Headers + mmsdata  */
  guint8             *data;
  gsize               mms_size;

  /* Pointer to the mmsdata */
  guint8             *mmsdata;
  gsize               size;

  /* Current position of the data read or written */
  guint8             *cdata;
  gsize               missing_size;

  guint8              flags;

  /* Not present for COMMAND packets */
  guint32             sequence;

  /* Only for command packets */
  MMSCommand          command;
  MMSCommand          expected_resp;

  union {
    LinkMacToViewerReportConnectedEX  initial;
    MMSFileInfos                      finfos;
  };
};

struct _MMSContext {
  /* Connection variable */
  GSocketClient         *socket_clt;
  GSocketConnection     *con;
  GSocketConnectable    *connectable;
  GPollableInputStream  *istream;
  GPollableOutputStream *ostream;
  GSource               *write_watch;
  gboolean               needs_write_watch;
  GSource               *read_watch;
  gboolean               needs_read_watch;
  const gchar           *host;
  gchar                 *path;

  /* Signal that the connection is done (Can have succeeded or not)*/
  GCond                 *connected_cond;
  /* Signal that a full buffer has been receive and can be used */
  GCond                 *buf_ready;

  MMSFileInfos          *finfos;

 /* weather we initialized the connection or not  */
  volatile gboolean      initialized;
  volatile GstFlowReturn flow;

  guint32                seq_num;   /* sequence number                */
 gchar                  guid[37];   /* randomly-generated client GUID */

 /* Debugging puporses */
  GstElement            *elem;

  /* Byte writer */
  GstByteWriter          bw;

  MMSPacket              w_packet;
  MMSPacket              r_packet;
  GMainContext          *context; /*  */


  GstBuffer             **cbuf;
  GstBuffer             *asf_header;
  gboolean               filled;
};

void mms_session_init             (MMSSession * session,
                                   GstElement * elem);

gint mms_session_connect          (MMSSession *session,
                                   const gchar *uri,
                                   GMainContext *context);

void mms_session_clean            (MMSSession *session);

gboolean mms_session_is_seekable  (MMSSession * session);

gboolean mms_session_fill_buffer  (MMSSession * session,
                                   GstBuffer **buf,
                                   GError **err);

#endif /* MMS_H */
