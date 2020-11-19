/* hyscan-map-track.h
 *
 * Copyright 2019 Screen LLC, Alexey Sakhnov <alexsakhnov@gmail.com>
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

#ifndef __HYSCAN_MAP_TRACK_H__
#define __HYSCAN_MAP_TRACK_H__

#include <hyscan-param.h>
#include <hyscan-geo-projection.h>
#include <hyscan-db.h>
#include <hyscan-cache.h>
#include <hyscan-track-proj-quality.h>
#include <hyscan-map-track-param.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_MAP_TRACK             (hyscan_map_track_get_type ())
#define HYSCAN_MAP_TRACK(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_MAP_TRACK, HyScanMapTrack))
#define HYSCAN_IS_MAP_TRACK(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_MAP_TRACK))
#define HYSCAN_MAP_TRACK_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_MAP_TRACK, HyScanMapTrackClass))
#define HYSCAN_IS_MAP_TRACK_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_MAP_TRACK))
#define HYSCAN_MAP_TRACK_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_MAP_TRACK, HyScanMapTrackClass))

typedef struct _HyScanMapTrack HyScanMapTrack;
typedef struct _HyScanMapTrackPrivate HyScanMapTrackPrivate;
typedef struct _HyScanMapTrackClass HyScanMapTrackClass;
typedef struct _HyScanMapTrackData HyScanMapTrackData;
typedef struct _HyScanMapTrackPoint HyScanMapTrackPoint;
typedef struct _HyScanMapTrackMod HyScanMapTrackMod;
typedef enum _HyScanMapTrackSource HyScanMapTrackSource;

/**
 * HyScanMapTrackModFunc:
 * @user_data: пользовательские данные
 * @start: точка начала изменённой области
 * @end: точка конца изменённой области
 *
 * Функция отмечает, что содержимое области между точками @start и @end было изменено.
 */
typedef void            (*HyScanMapTrackModFunc)             (gpointer                   user_data,
                                                               HyScanGeoCartesian2D      *start,
                                                               HyScanGeoCartesian2D      *end);

/**
 * HyScanMapTrackSource:
 * @HYSCAN_GTK_MAP_TRACK_DRAW_SOURCE_NAV: навигационные данные
 * @HYSCAN_GTK_MAP_TRACK_DRAW_SOURCE_LEFT: данные левого борта
 * @HYSCAN_GTK_MAP_TRACK_DRAW_SOURCE_RIGHT: данные правого борта
 *
 * Тип источника данных точки галса.
 */
enum _HyScanMapTrackSource
{
  HYSCAN_GTK_MAP_TRACK_DRAW_SOURCE_NAV,
  HYSCAN_GTK_MAP_TRACK_DRAW_SOURCE_LEFT,
  HYSCAN_GTK_MAP_TRACK_DRAW_SOURCE_RIGHT,
};

/**
 * HyScanMapTrackData:
 * @starboard: (element-type HyScanMapTrackPoint): точки правого борта
 * @port: (element-type HyScanMapTrackPoint): точки левого борта
 * @nav: (element-type HyScanMapTrackPoint): точки навигации
 * @from: минимальные координаты области, внути которой лежат все точки
 * @to: максимальные координаты области, внути которой лежат все точки
 *
 * Данные для отрисовки галса.
 */
struct _HyScanMapTrackData
{
  GList                  *starboard;
  GList                  *port;
  GList                  *nav;
  HyScanGeoCartesian2D    from;
  HyScanGeoCartesian2D    to;
};

struct _HyScanMapTrackMod
{
  HyScanGeoCartesian2D from;
  HyScanGeoCartesian2D to;
};

/**
 * HyScanMapTrackPoint:
 * @source: источник данных
 * @index: индекс записи в канале источника данных
 * @time: время фиксации данных
 * @geo: географические координаты точки
 * @course: курс движения
 * @b_angle: курс с поправкой на смещение антенн GPS и ГЛ, рад
 * @b_length_m: длина луча, метры
 * @nr_length_m: длина ближней зоны диаграммы направленности, метры
 * @scale: масштаб картографической проекции в текущей точке
 * @aperture: апертура антенны, ед. проекции
 * @ship_c2d: координаты судна
 * @start_c2d: координаты начала луча
 * @nr_c2d: координаты точки на конце ближней зоны
 * @fr_c2d: координаты центральной точки на конце дальней зоны
 * @fr1_c2d: координаты одной крайней точки на конце дальней зоны
 * @fr2_c2d: координаты второй крайней точки на конце дальней зоны
 * @dist_along: расстояние от начала галса до текущей точки вдоль линии галса, ед. проекции
 * @b_dist: длина луча, ед. проекции
 * @straight: признак того, что точка находится на относительно прямолинейном участке
 * @quality: (element-type: HyScanMapTrackQuality) (array-length=quality_len): отрезки луча, разбитые по качеству
 *
 * Информация о точке на галсе, соответствующая данным по индексу @index из источника @source.
 * Положение судна, антенна гидролокатора, характерные точки диаграммы направленности указаны
 * в координатах картографической проекции.
 *
 */
struct _HyScanMapTrackPoint
{
  HyScanMapTrackSource            source;
  guint32                         index;
  gint64                          time;

  HyScanGeoPoint                  geo;
  gdouble                         course;
  gdouble                         b_angle;
  gdouble                         b_length_m;
  gdouble                         nr_length_m;

  gdouble                         scale;
  gdouble                         aperture;
  HyScanGeoCartesian2D            ship_c2d;
  HyScanGeoCartesian2D            start_c2d;
  HyScanGeoCartesian2D            nr_c2d;
  HyScanGeoCartesian2D            fr_c2d;
  HyScanGeoCartesian2D            fr1_c2d;
  HyScanGeoCartesian2D            fr2_c2d;
  gdouble                         dist_along;
  gdouble                         b_dist;
  gboolean                        straight;
};

struct _HyScanMapTrack
{
  GObject                       parent_instance;
  HyScanMapTrackPrivate *priv;
};

struct _HyScanMapTrackClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                     hyscan_map_track_get_type              (void);

HYSCAN_API
HyScanMapTrack *          hyscan_map_track_new                   (HyScanDB                   *db,
                                                                  HyScanCache                *cache,
                                                                  const gchar                *project_name,
                                                                  const gchar                *track_name,
                                                                  HyScanGeoProjection        *projection);

HYSCAN_API
guint32                   hyscan_map_track_get_mod_count         (HyScanMapTrack             *track);

HYSCAN_API
HyScanMapTrackParam *     hyscan_map_track_get_param             (HyScanMapTrack             *track);

HYSCAN_API
HyScanTrackProjQuality *  hyscan_map_track_get_quality_port      (HyScanMapTrack             *track);

HYSCAN_API
HyScanTrackProjQuality *  hyscan_map_track_get_quality_starboard (HyScanMapTrack             *track);

HYSCAN_API
gboolean                  hyscan_map_track_get                   (HyScanMapTrack             *track,
                                                                  HyScanMapTrackData         *data);

HYSCAN_API
gboolean                  hyscan_map_track_view                  (HyScanMapTrack             *track,
                                                                  HyScanGeoCartesian2D       *from,
                                                                  HyScanGeoCartesian2D       *to);

HYSCAN_API
void                      hyscan_map_track_set_projection        (HyScanMapTrack             *track,
                                                                  HyScanGeoProjection        *projection);

HYSCAN_API
void                      hyscan_map_track_point_free            (HyScanMapTrackPoint        *point);

HYSCAN_API
HyScanMapTrackPoint *     hyscan_map_track_point_copy            (const HyScanMapTrackPoint  *point);

G_END_DECLS

#endif /* __HYSCAN_MAP_TRACK_H__ */
