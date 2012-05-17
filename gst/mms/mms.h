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
#include <glib-object.h>
#include <gst/base/gstbytewriter.h>

G_BEGIN_DECLS

#define MMS_TYPE_SESSION (mms_session_get_type())
#define MMS_SESSION(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj),MMS_TYPE_SESSION,MMSSession))
#define MMS_SESSION_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass),MMS_TYPE_SESSION,MMSSessionClass))
#define MMS_IS_SESSION(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj),MMS_TYPE_SESSION))
#define MMS_IS_SESSION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),MMS_TYPE_SESSION))


typedef struct _MMSPacket MMSPacket;
typedef struct _MMSSession MMSSession;
typedef struct _MMSSessionClass MMSSessionClass;
typedef struct _MMSSessionPrivate MMSSessionPrivate;


struct _MMSSession {
  GObject parent;

  MMSSessionPrivate *priv;
};

struct _MMSSessionClass
{
  GObjectClass parent_class;
};

GType mms_session_get_type        (void);

MMSSession *mms_session_new       (GstElement * elem);

gboolean mms_session_connect      (MMSSession *session,
                                   const gchar *uri,
                                   GError **err);

void mms_session_stop             (MMSSession *session);

gboolean mms_session_is_seekable  (MMSSession * session);

GstFlowReturn
mms_session_fill_buffer           (MMSSession * session,
                                   GstBuffer **buf,
                                   GError **err);

G_END_DECLS
#endif /* MMS_H */
