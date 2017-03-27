/**
 *
 * \file hyscan-track-rect.h
 *
 * \brief Заголовочный файл класса, вычисляющего геометрические параметры галса.
 * \author Dmitriev Alexander (m1n7@yandex.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanTrackRect HyScanTrackRect - геометрические параметры галса.
 *
 * HyScanTrackRect - геометрические параметры галса.
 * Данный класс используется для определения геометрических параметров галса: ширина
 * каждого из бортов, общая длина. В данный момент класс вычисляет максимальную ширину галса,
 * что и отражено в названии.
 *
 * - #hyscan_track_rect_new - создает объект;
 * - #hyscan_track_rect_set_ship_speed - устанавливает скорост судна;
 * - #hyscan_track_rect_set_sound_velocity - устанавливает скорость звука;
 * - #hyscan_track_rect_set_cache - устанавливает кэш;
 * - #hyscan_track_rect_set_depth_source - настраивает параметры определения глубины;
 * - #hyscan_track_rect_set_depth_filter_size - настраивает параметры определения глубины;
 * - #hyscan_track_rect_set_depth_time - настраивает параметры определения глубины;
 * - #hyscan_track_rect_set_type - задает тип отображения галса;
 * - #hyscan_track_rect_open - открывает галс;
 * - #hyscan_track_rect_close - закрывает галс;
 * - #hyscan_track_rect_get - получает параметры галса.
 */

#ifndef __HYSCAN_TRACK_RECT_H__
#define __HYSCAN_TRACK_RECT_H__

#include <hyscan-depth-acoustic.h>
#include <hyscan-tile-common.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_TRACK_RECT             (hyscan_track_rect_get_type ())
#define HYSCAN_TRACK_RECT(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_TRACK_RECT, HyScanTrackRect))
#define HYSCAN_IS_TRACK_RECT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_TRACK_RECT))
#define HYSCAN_TRACK_RECT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_TRACK_RECT, HyScanTrackRectClass))
#define HYSCAN_IS_TRACK_RECT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_TRACK_RECT))
#define HYSCAN_TRACK_RECT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_TRACK_RECT, HyScanTrackRectClass))

typedef struct _HyScanTrackRect HyScanTrackRect;
typedef struct _HyScanTrackRectPrivate HyScanTrackRectPrivate;
typedef struct _HyScanTrackRectClass HyScanTrackRectClass;

struct _HyScanTrackRect
{
  GObject parent_instance;

  HyScanTrackRectPrivate *priv;
};

struct _HyScanTrackRectClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                   hyscan_track_rect_get_type              (void);

/**
 * Функция создает новый объект \link HyScanTrackRect \endlink.
 *
 * \return указатель на объект \link HyScanTrackRect \endlink.
 */
HYSCAN_API
HyScanTrackRect        *hyscan_track_rect_new                   (void);

/**
 *
 * Функция устанавливает скорости звука и судна.
 *
 * \param track_rect указатель на объект \link HyScanTrackRect \endlink;
 * \param speed - скорость движения, м/с.
 *
 */
HYSCAN_API
void                    hyscan_track_rect_set_ship_speed       (HyScanTrackRect        *track_rect,
                                                                gfloat                  speed);
/**
 *
 * Функция устанавливает скорости звука и судна.
 *
 * \param track_rect указатель на объект \link HyScanTrackRect \endlink;
 * \param velocity - скорость звука, м/с. См \link HyScanSoundVelocity \endlink.
 *
 */
HYSCAN_API
void                    hyscan_track_rect_set_sound_velocity   (HyScanTrackRect        *track_rect,
                                                                GArray                 *velocity);

/**
 *
 * Функция устанавливает кэш.
 *
 * \param track_rect указатель на объект \link HyScanTrackRect \endlink;
 * \param cache указатель на интерфейс HyScanCache;
 * \param prefix префикс системы кэширования.
 *
 */
HYSCAN_API
void                    hyscan_track_rect_set_cache            (HyScanTrackRect        *track_rect,
                                                                HyScanCache            *cache,
                                                                const gchar            *prefix);

/**
 *
 * Функция настраивает объект измерения глубины.
 *
 * \param track_rect указатель на объект \link HyScanTrackRect \endlink;
 * \param source источник данных глубины;
 * \param channel номер канала данных.
 *
 */
HYSCAN_API
void                    hyscan_track_rect_set_depth_source     (HyScanTrackRect        *track_rect,
                                                                HyScanSourceType        source,
                                                                guint                   channel);
/**
 *
 * Функция настраивает объект измерения глубины.
 *
 * \param track_rect указатель на объект \link HyScanTrackRect \endlink;
 * \param size размер фильтра для сглаживания данных.
 *
 */
HYSCAN_API
void                    hyscan_track_rect_set_depth_filter_size(HyScanTrackRect        *track_rect,
                                                                gint                    size);
/**
 *
 * Функция настраивает объект измерения глубины.
 *
 * \param track_rect указатель на объект \link HyScanTrackRect \endlink;
 * \param usecs окно валидности данных.
 *
 */
HYSCAN_API
void                    hyscan_track_rect_set_depth_time       (HyScanTrackRect        *track_rect,
                                                                gulong                  usecs);

/**
 *
 * Функция устанавливает тип отображения (наклонная/горизонтальная дальность).
 * От типа отображения зависит ширина одного и того же галса.
 *
 * \param track_rect указатель на объект \link HyScanTrackRect \endlink;
 * \param type - тип отображения (см \link HyScanTileType\endlink).
 *
 * \return TRUE, если хотя бы один из каналов открыт в режиме записи.
 */
HYSCAN_API
void                    hyscan_track_rect_set_type             (HyScanTrackRect        *track_rect,
                                                                HyScanTileType          type);

/**
 *
 * Функция открывает галс.
 *
 * \param track_rect указатель на объект \link HyScanTrackRect \endlink;
 * \param db указатель на \link HyScanDB \endlink;
 * \param project - имя проекта;
 * \param track - имя галса;
 * \param source - тип источника данных;
 * \param raw - использовать ли сырые данные.
 *
 */
HYSCAN_API
void                    hyscan_track_rect_open                 (HyScanTrackRect        *track_rect,
                                                                HyScanDB               *db,
                                                                const gchar            *project,
                                                                const gchar            *track,
                                                                HyScanSourceType        source,
                                                                gboolean                raw);

/**
 *
 * Функция возвращает параметры галса.
 * По умолчанию параметры возвращаются в наклонной дальности.
 *
 * \param track_rect указатель на объект \link HyScanTrackRect \endlink;
 * \param writeable - TRUE, если в КД возможна дозапись данных;
 * \param width - ширина галса в м.;
 * \param length - длина галса в м.
 *
 * \return TRUE, если хотя бы один из каналов открыт в режиме записи.
 */
HYSCAN_API
gboolean                hyscan_track_rect_get                  (HyScanTrackRect        *track_rect,
                                                                gboolean               *writeable,
                                                                gdouble                *width,
                                                                gdouble                *length);

G_END_DECLS

#endif /* __HYSCAN_TRACK_RECT_H__ */
