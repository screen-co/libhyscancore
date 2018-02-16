/**
 * \file hyscan-nmea-parser.h
 *
 * \brief Заголовочный файл класса HyScanNMEAParser
 * \author Alexander Dmitriev (m1n7@yandex.ru)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 * \defgroup HyScanNMEAParser HyScanNMEAParser - класс извлечения глубины из NMEA-сообщений.
 *
 * Класс HyScanNMEAParser реализует интерфейс \link HyScanDepth \endlink.
 * Обрабатываются только NMEA-DPT строки.
 * Публично доступен только метод #hyscan_nmea_parser_new, создающий новый объект.
 *
 * Класс не является потокобезопасным.
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

typedef enum
{
 HYSCAN_NMEA_FIELD_TIME = 0,
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

/**
 *
 * Функция создает новый объект обработки строк NMEA.
 *
 * \param db указатель на интерфейс HyScanDB;
 * \param project название проекта;
 * \param track название галса;
 * \param source_channel номер канала.
 *
 * \return Указатель на объект \link HyScanNMEAParser \endlink или NULL, если не
 * удалось открыть канал данных.
 *
 */
HYSCAN_API
HyScanNMEAParser*        hyscan_nmea_parser_new           (HyScanDB         *db,
                                                           const gchar      *project,
                                                           const gchar      *track,
                                                           guint             source_channel,
                                                           HyScanSourceType  source_type,
                                                           guint             field_type);
G_END_DECLS

#endif /* __HYSCAN_NMEA_PARSER_H__ */
