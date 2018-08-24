/* hyscan-nmea-parser.h
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
 * Contact the Screen LLC in this case - info@screen-co.ru
 */

/* HyScanCore имеет двойную лицензию.
 *
 * Во-первых, вы можете распространять HyScanCore на условиях Стандартной
 * Общественной Лицензии GNU версии 3, либо по любой более поздней версии
 * лицензии (по вашему выбору). Полные положения лицензии GNU приведены в
 * <http://www.gnu.org/licenses/>.
 *
 * Во-вторых, этот программный код можно использовать по коммерческой
 * лицензии. Для этого свяжитесь с ООО Экран - info@screen-co.ru.
 */

#ifndef __HYSCAN_NMEA_PARSER_H__
#define __HYSCAN_NMEA_PARSER_H__

#include <hyscan-nav-data.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_NMEA_PARSER             (hyscan_nmea_parser_get_type ())
#define HYSCAN_NMEA_PARSER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_NMEA_PARSER, HyScanNMEAParser))
#define HYSCAN_IS_NMEA_PARSER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_NMEA_PARSER))
#define HYSCAN_NMEA_PARSER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_NMEA_PARSER, HyScanNMEAParserClass))
#define HYSCAN_IS_NMEA_PARSER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_NMEA_PARSER))
#define HYSCAN_NMEA_PARSER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_NMEA_PARSER, HyScanNMEAParserClass))

typedef struct _HyScanNMEAParser HyScanNMEAParser;
typedef struct _HyScanNMEAParserPrivate HyScanNMEAParserPrivate;
typedef struct _HyScanNMEAParserClass HyScanNMEAParserClass;

struct _HyScanNMEAParser
{
  GObject parent_instance;

  HyScanNMEAParserPrivate *priv;
};

struct _HyScanNMEAParserClass
{
  GObjectClass parent_class;
};

/**
 * HyScanNMEAField:
 * @HYSCAN_NMEA_FIELD_TIME: время
 * @HYSCAN_NMEA_FIELD_LAT: широта
 * @HYSCAN_NMEA_FIELD_LON: долгота
 * @HYSCAN_NMEA_FIELD_SPEED: скорость
 * @HYSCAN_NMEA_FIELD_TRACK: курс
 * @HYSCAN_NMEA_FIELD_DATE: дата
 * @HYSCAN_NMEA_FIELD_MAG_VAR: магнитное склонение
 * @HYSCAN_NMEA_FIELD_FIX_QUAL: качество фикса
 * @HYSCAN_NMEA_FIELD_N_SATS: количество спутников
 * @HYSCAN_NMEA_FIELD_HDOP: HDOP
 * @HYSCAN_NMEA_FIELD_ALTITUDE: высота
 * @HYSCAN_NMEA_FIELD_HOG: HOG
 * @HYSCAN_NMEA_FIELD_DEPTH: глубина
 *
 * Типы извлекаемых из NMEA-строки данных.
 */
typedef enum
{
 HYSCAN_NMEA_FIELD_TIME,
 HYSCAN_NMEA_FIELD_LAT,
 HYSCAN_NMEA_FIELD_LON,
 HYSCAN_NMEA_FIELD_SPEED,
 HYSCAN_NMEA_FIELD_TRACK,
 HYSCAN_NMEA_FIELD_DATE,
 HYSCAN_NMEA_FIELD_MAG_VAR,
 HYSCAN_NMEA_FIELD_FIX_QUAL,
 HYSCAN_NMEA_FIELD_N_SATS,
 HYSCAN_NMEA_FIELD_HDOP,
 HYSCAN_NMEA_FIELD_ALTITUDE,
 HYSCAN_NMEA_FIELD_HOG,
 HYSCAN_NMEA_FIELD_DEPTH
} HyScanNMEAField;

HYSCAN_API
GType                   hyscan_nmea_parser_get_type      (void);

HYSCAN_API
HyScanNMEAParser*       hyscan_nmea_parser_new           (HyScanDB         *db,
                                                          const gchar      *project,
                                                          const gchar      *track,
                                                          HyScanSourceType  source_type,
                                                          guint             source_channel,
                                                          guint             field_type);
G_END_DECLS

#endif /* __HYSCAN_NMEA_PARSER_H__ */
