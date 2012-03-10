/* GStreamer
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

#ifndef _GST_MMS_SRC_H_
#define _GST_MMS_SRC_H_

#include <gst/base/gstpushsrc.h>

#include "mms.h"

G_BEGIN_DECLS

#define GST_TYPE_MMS_SRC   (gst_mms_src_get_type())
#define GST_MMS_SRC(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_MMS_SRC,GstMMSSrc))
#define GST_MMS_SRC_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_MMS_SRC,GstMMSSrcClass))
#define GST_IS_MMS_SRC(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_MMS_SRC))
#define GST_IS_MMS_SRC_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_MMS_SRC))

#define GST_MMS_SRC_GET_LOCK(src)      (GST_MMS_SRC(src)->stream_lock)
#define GST_MMS_SRC_STREAM_LOCK(src)   (g_static_rec_mutex_lock (GST_MMS_SRC_GET_LOCK(src)))
#define GST_MMS_SRC_STREAM_UNLOCK(src) (g_static_rec_mutex_unlock (GST_MMS_SRC_GET_LOCK(src)))

typedef struct _GstMMSSrc GstMMSSrc;
typedef struct _GstMMSSrcClass GstMMSSrcClass;

struct _GstMMSSrc
{
  GstPushSrc          base_mms_src;

  GstPad             *srcpad;
  gchar              *uri;


  MMSSession          mms_session;      /* The main MMS session */

  GMainContext       *context;          /* I/O context. */
  GMainLoop          *loop;             /* Event loop. */
  GstTask            *task;             /* I/O thread creation */
  GStaticRecMutex    *stream_lock;      /* Protects the polling thread */
};

struct _GstMMSSrcClass
{
  GstPushSrcClass base_mms_src_class;
};

GType gst_mms_src_get_type (void);

G_END_DECLS

#endif
