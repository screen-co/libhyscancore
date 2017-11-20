/**
 * \file hyscan-depth-nmea.h
 *
 * \brief Заголовочный файл класса HyScanDepthNMEA
 * \author Alexander Dmitriev (m1n7@yandex.ru)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 * \defgroup HyScanDepthNMEA HyScanDepthNMEA - класс извлечения глубины из NMEA-сообщений.
 *
 * Класс HyScanDepthNMEA реализует интерфейс \link HyScanDepth \endlink.
 * Обрабатываются только NMEA-DPT строки.
 * Публично доступен только метод #hyscan_depth_nmea_new, создающий новый объект.
 *
 * Класс не является потокобезопасным.
 */

#ifndef __HYSCAN_DEPTH_NMEA_H__
#define __HYSCAN_DEPTH_NMEA_H__

#include <hyscan-depth.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_DEPTH_NMEA             (hyscan_depth_nmea_get_type ())
#define HYSCAN_DEPTH_NMEA(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_DEPTH_NMEA, HyScanDepthNMEA))
#define HYSCAN_IS_DEPTH_NMEA(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_DEPTH_NMEA))
#define HYSCAN_DEPTH_NMEA_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_DEPTH_NMEA, HyScanDepthNMEAClass))
#define HYSCAN_IS_DEPTH_NMEA_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_DEPTH_NMEA))
#define HYSCAN_DEPTH_NMEA_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_DEPTH_NMEA, HyScanDepthNMEAClass))

typedef struct _HyScanDepthNMEA HyScanDepthNMEA;
typedef struct _HyScanDepthNMEAPrivate HyScanDepthNMEAPrivate;
typedef struct _HyScanDepthNMEAClass HyScanDepthNMEAClass;

struct _HyScanDepthNMEA
{
  GObject parent_instance;

  HyScanDepthNMEAPrivate *priv;
};

struct _HyScanDepthNMEAClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                   hyscan_depth_nmea_get_type      (void);

/**
 *
 * Функция создает новый объект обработки строк NMEA-DPT.
 *
 * \param db указатель на интерфейс HyScanDB;
 * \param project название проекта;
 * \param track название галса;
 * \param source_channel номер канала.
 *
 * \return Указатель на объект \link HyScanDepthNMEA \endlink или NULL, если не
 * удалось открыть канал данных.
 *
 */
HYSCAN_API
HyScanDepthNMEA*        hyscan_depth_nmea_new           (HyScanDB               *db,
                                                         const gchar            *project,
                                                         const gchar            *track,
                                                         guint                   source_channel);
G_END_DECLS

#endif /* __HYSCAN_DEPTH_NMEA_H__ */
