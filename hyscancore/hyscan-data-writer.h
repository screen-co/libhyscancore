/* hyscan-data-writer.h
 *
 * Copyright 2016-2018 Screen LLC, Andrei Fadeev <andrei@webcontrol.ru>
 *
 * This file is part of HyScanCore library.
 *
 * HyScanCore is dual-licensed: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * HyScanCore is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Alternatively, you can license this code under a commercial license.
 * Contact the Screen LLC in this case - <info@screen-co.ru>.
 */

/* HyScanCore имеет двойную лицензию.
 *
 * Во-первых, вы можете распространять HyScanCore на условиях Стандартной
 * Общественной Лицензии GNU версии 3, либо по любой более поздней версии
 * лицензии (по вашему выбору). Полные положения лицензии GNU приведены в
 * <http://www.gnu.org/licenses/>.
 *
 * Во-вторых, этот программный код можно использовать по коммерческой
 * лицензии. Для этого свяжитесь с ООО Экран - <info@screen-co.ru>.
 */

#ifndef __HYSCAN_DATA_WRITER_H__
#define __HYSCAN_DATA_WRITER_H__

#include <hyscan-db.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_DATA_WRITER             (hyscan_data_writer_get_type ())
#define HYSCAN_DATA_WRITER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_DATA_WRITER, HyScanDataWriter))
#define HYSCAN_IS_DATA_WRITER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_DATA_WRITER))
#define HYSCAN_DATA_WRITER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_DATA_WRITER, HyScanDataWriterClass))
#define HYSCAN_IS_DATA_WRITER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_DATA_WRITER))
#define HYSCAN_DATA_WRITER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_DATA_WRITER, HyScanDataWriterClass))

typedef struct _HyScanDataWriter HyScanDataWriter;
typedef struct _HyScanDataWriterPrivate HyScanDataWriterPrivate;
typedef struct _HyScanDataWriterClass HyScanDataWriterClass;

struct _HyScanDataWriter
{
  GObject parent_instance;

  HyScanDataWriterPrivate *priv;
};

struct _HyScanDataWriterClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                  hyscan_data_writer_get_type             (void);

HYSCAN_API
HyScanDataWriter      *hyscan_data_writer_new                  (void);

HYSCAN_API
void                   hyscan_data_writer_set_db               (HyScanDataWriter              *writer,
                                                                HyScanDB                      *db);

HYSCAN_API
HyScanDB *             hyscan_data_writer_get_db               (HyScanDataWriter              *writer);

HYSCAN_API
void                   hyscan_data_writer_set_operator_name    (HyScanDataWriter              *writer,
                                                                const gchar                   *name);

HYSCAN_API
void                   hyscan_data_writer_set_sonar_info       (HyScanDataWriter              *writer,
                                                                const gchar                   *info);

HYSCAN_API
void                   hyscan_data_writer_set_chunk_size       (HyScanDataWriter              *writer,
                                                                gint32                         chunk_size);

HYSCAN_API
void                   hyscan_data_writer_sensor_set_offset    (HyScanDataWriter              *writer,
                                                                const gchar                   *sensor,
                                                                const HyScanAntennaOffset     *offset);

HYSCAN_API
void                   hyscan_data_writer_sonar_set_offset     (HyScanDataWriter              *writer,
                                                                HyScanSourceType               source,
                                                                const HyScanAntennaOffset     *offset);

HYSCAN_API
gboolean               hyscan_data_writer_create_project       (HyScanDataWriter              *writer,
                                                                const gchar                   *project_name,
                                                                gint64                         date_time);

HYSCAN_API
gboolean               hyscan_data_writer_start                (HyScanDataWriter              *writer,
                                                                const gchar                   *project_name,
                                                                const gchar                   *track_name,
                                                                HyScanTrackType                track_type,
                                                                const HyScanTrackPlan         *track_plan,
                                                                gint64                         date_time);

HYSCAN_API
void                   hyscan_data_writer_stop                 (HyScanDataWriter              *writer);

HYSCAN_API
gboolean               hyscan_data_writer_log_add_message      (HyScanDataWriter              *writer,
                                                                const gchar                   *source,
                                                                gint64                         time,
                                                                HyScanLogLevel                 level,
                                                                const gchar                   *message);

HYSCAN_API
gboolean               hyscan_data_writer_sensor_add_data      (HyScanDataWriter              *writer,
                                                                const gchar                   *sensor,
                                                                HyScanSourceType               source,
                                                                guint                          channel,
                                                                gint64                         time,
                                                                HyScanBuffer                  *data);

HYSCAN_API
gboolean               hyscan_data_writer_acoustic_create      (HyScanDataWriter              *writer,
                                                                HyScanSourceType               source,
                                                                guint                          channel,
                                                                const gchar                   *description,
                                                                const gchar                   *actuator,
                                                                HyScanAcousticDataInfo        *info);

HYSCAN_API
gboolean               hyscan_data_writer_acoustic_add_data    (HyScanDataWriter              *writer,
                                                                HyScanSourceType               source,
                                                                guint                          channel,
                                                                gboolean                       noise,
                                                                gint64                         time,
                                                                HyScanAcousticDataInfo        *info,
                                                                HyScanBuffer                  *data);

HYSCAN_API
gboolean               hyscan_data_writer_acoustic_add_signal  (HyScanDataWriter              *writer,
                                                                HyScanSourceType               source,
                                                                guint                          channel,
                                                                gint64                         time,
                                                                HyScanBuffer                  *image);

HYSCAN_API
gboolean               hyscan_data_writer_acoustic_add_tvg     (HyScanDataWriter              *writer,
                                                                HyScanSourceType               source,
                                                                guint                          channel,
                                                                gint64                         time,
                                                                HyScanBuffer                  *gains);

G_END_DECLS

#endif /* __HYSCAN_DATA_WRITER_H__ */
