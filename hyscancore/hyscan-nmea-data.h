/* hyscan-nmea-data.h
 *
 * Copyright 2018 Screen LLC, Alexander Dmitriev <m1n7@yandex.ru>
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
 * лицензии. Для этого свяжитесь с ООО Экран - info@screen:-co.ru.
 */

#ifndef __HYSCAN_NMEA_DATA_H__
#define __HYSCAN_NMEA_DATA_H__

#include <hyscan-db.h>
#include <hyscan-cache.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_NMEA_DATA             (hyscan_nmea_data_get_type ())
#define HYSCAN_NMEA_DATA(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_NMEA_DATA, HyScanNMEAData))
#define HYSCAN_IS_NMEA_DATA(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_NMEA_DATA))
#define HYSCAN_NMEA_DATA_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_NMEA_DATA, HyScanNMEADataClass))
#define HYSCAN_IS_NMEA_DATA_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_NMEA_DATA))
#define HYSCAN_NMEA_DATA_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_NMEA_DATA, HyScanNMEADataClass))

typedef struct _HyScanNMEAData HyScanNMEAData;
typedef struct _HyScanNMEADataPrivate HyScanNMEADataPrivate;
typedef struct _HyScanNMEADataClass HyScanNMEADataClass;

struct _HyScanNMEAData
{
  GObject parent_instance;

  HyScanNMEADataPrivate *priv;
};

struct _HyScanNMEADataClass
{
  GObjectClass parent_class;
};

/**
 * HyScanNmeaDataType:
 * @HYSCAN_NMEA_DATA_INVALID: недопустимый тип, ошибка
 * @HYSCAN_NMEA_DATA_ANY: любая строка NMEA
 * @HYSCAN_NMEA_DATA_RMC: строка NMEA RMC
 * @HYSCAN_NMEA_DATA_GGA: строка NMEA GGA
 * @HYSCAN_NMEA_DATA_DPT: строка NMEA DPT
 *
 * Тип NMEA-0183 строки.
 */
typedef enum
{
  HYSCAN_NMEA_DATA_INVALID   = 0,
  HYSCAN_NMEA_DATA_ANY       = 1,
  HYSCAN_NMEA_DATA_RMC       = 1 << 1,
  HYSCAN_NMEA_DATA_GGA       = 1 << 2,
  HYSCAN_NMEA_DATA_DPT       = 1 << 3
} HyScanNmeaDataType;

HYSCAN_API
GType                   hyscan_nmea_data_get_type              (void);


HYSCAN_API
HyScanNMEAData *        hyscan_nmea_data_new                   (HyScanDB         *db,
                                                                const gchar      *project_name,
                                                                const gchar      *track_name,
                                                                guint             source_channel);

HYSCAN_API
HyScanNMEAData *        hyscan_nmea_data_new_sensor            (HyScanDB         *db,
                                                                const gchar      *project_name,
                                                                const gchar      *track_name,
                                                                const gchar      *sensor_name);

HYSCAN_API
void                    hyscan_nmea_data_set_cache             (HyScanNMEAData   *data,
                                                                HyScanCache      *cache);

HYSCAN_API
HyScanAntennaPosition   hyscan_nmea_data_get_position          (HyScanNMEAData   *data);


HYSCAN_API
const gchar *           hyscan_nmea_data_get_sensor_name       (HyScanNMEAData   *data);

HYSCAN_API
guint                   hyscan_nmea_data_get_channel           (HyScanNMEAData   *data);


HYSCAN_API
gboolean                hyscan_nmea_data_is_writable           (HyScanNMEAData   *data);


HYSCAN_API
gboolean                hyscan_nmea_data_get_range             (HyScanNMEAData   *data,
                                                                guint32          *first,
                                                                guint32          *last);

HYSCAN_API
HyScanDBFindStatus      hyscan_nmea_data_find_data             (HyScanNMEAData   *data,
                                                                gint64            time,
                                                                guint32          *lindex,
                                                                guint32          *rindex,
                                                                gint64           *ltime,
                                                                gint64           *rtime);

HYSCAN_API
const gchar            *hyscan_nmea_data_get_sentence          (HyScanNMEAData   *data,
                                                                guint32           index,
                                                                gint64           *time);

HYSCAN_API
guint32                 hyscan_nmea_data_get_mod_count         (HyScanNMEAData   *data);

HYSCAN_API
HyScanNmeaDataType      hyscan_nmea_data_check_sentence        (const gchar      *sentence);

HYSCAN_API
gchar **                hyscan_nmea_data_split_sentence        (const gchar      *sentence,
                                                                guint32           length);

G_END_DECLS

#endif /* __HYSCAN_NMEA_DATA_H__ */
