/* hyscan-planner.h
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

#ifndef __HYSCAN_PLANNER_H__
#define __HYSCAN_PLANNER_H__

#include <hyscan-geo.h>
#include <hyscan-object-data.h>

G_BEGIN_DECLS

/**
 * HYSCAN_PLANNER_ORIGIN_ID:
 * Идентификатор объекта референсной точки в параметрах проекта
 */
#define HYSCAN_PLANNER_ORIGIN_ID "origin"

#define HYSCAN_TYPE_PLANNER_ORIGIN   (hyscan_planner_origin_get_type ())
#define HYSCAN_TYPE_PLANNER_TRACK    (hyscan_planner_track_get_type ())
#define HYSCAN_TYPE_PLANNER_ZONE     (hyscan_planner_zone_get_type ())

#define HYSCAN_IS_PLANNER_ZONE(x)   ((x) != NULL && (x)->type == HYSCAN_TYPE_PLANNER_ZONE)
#define HYSCAN_IS_PLANNER_TRACK(x)  ((x) != NULL && (x)->type == HYSCAN_TYPE_PLANNER_TRACK)
#define HYSCAN_IS_PLANNER_ORIGIN(x) ((x) != NULL && (x)->type == HYSCAN_TYPE_PLANNER_ORIGIN)

typedef struct _HyScanPlannerTrack HyScanPlannerTrack;
typedef struct _HyScanPlannerZone HyScanPlannerZone;
typedef struct _HyScanPlannerOrigin HyScanPlannerOrigin;

/**
 * HyScanPlannerTrack:
 * @id: уникальный идентификатор
 * @zone_id: идентификатор зоны, в котрой находится галс
 * @number: порядковый номер галса
 * @plan: запланированные параметры движения
 * @name: название
 * @records: NULL-терминированный список идентификаторов записанных галсов
 *
 * Параметры запланированного галса
 */
struct _HyScanPlannerTrack
{
  GType                    type;
  gchar                   *zone_id;
  guint                    number;
  HyScanTrackPlan          plan;
  gchar                   *name;
  gchar                  **records;
};

/**
 * HyScanPlannerZone:
 * @type: тип объекта
 * @name: название зоны
 * @points: (element-type HyScanGeoPoint): список вершин многоугольника, ограничивающего зону
 * @points_len: число вершин многоугольника
 *
 * Параметры зоны исследования.
 */
struct _HyScanPlannerZone
{
  GType                    type;
  gchar                   *name;
  HyScanGeoPoint          *points;
  gsize                    points_len;
  gint64                   ctime;
  gint64                   mtime;
};

/**
 * HyScanPlannerZone:
 * @type: тип объекта
 * @origin: координаты точки начала отсчёта
 * @ox: направление оси OX, градусы
 *
 * Референсная точка, которая считается началом координат для местной системы координат
 */
struct _HyScanPlannerOrigin
{
  GType                    type;
  HyScanGeoPoint           origin;
  gdouble                  ox;
};

HYSCAN_API
GType                  hyscan_planner_origin_get_type     (void);

HYSCAN_API
GType                  hyscan_planner_track_get_type      (void);

HYSCAN_API
GType                  hyscan_planner_zone_get_type       (void);

HYSCAN_API
HyScanPlannerOrigin *  hyscan_planner_origin_new          (void);

HYSCAN_API
HyScanPlannerOrigin *  hyscan_planner_origin_copy         (const HyScanPlannerOrigin *origin);

HYSCAN_API
void                   hyscan_planner_origin_free         (HyScanPlannerOrigin       *origin);

HYSCAN_API
HyScanPlannerTrack *   hyscan_planner_track_new           (void);

HYSCAN_API
HyScanPlannerTrack *   hyscan_planner_track_copy          (const HyScanPlannerTrack  *track);

HYSCAN_API
void                   hyscan_planner_track_free          (HyScanPlannerTrack        *track);

HYSCAN_API
void                   hyscan_planner_track_record_append (HyScanPlannerTrack        *track,
                                                           const gchar               *record_id);

HYSCAN_API
void                   hyscan_planner_track_record_delete (HyScanPlannerTrack        *track,
                                                           const gchar               *record_id);

HYSCAN_API
HyScanTrackPlan *      hyscan_planner_track_get_plan     (HyScanPlannerTrack        *track);

HYSCAN_API
HyScanGeo *            hyscan_planner_track_geo          (const HyScanTrackPlan     *plan,
                                                          gdouble                   *angle);

HYSCAN_API
gdouble                hyscan_planner_track_angle        (const HyScanPlannerTrack  *track);

HYSCAN_API
gdouble                hyscan_planner_track_length       (const HyScanTrackPlan     *plan);

HYSCAN_API
gdouble                hyscan_planner_track_transit      (const HyScanTrackPlan     *plan1,
                                                          const HyScanTrackPlan     *plan2);

HYSCAN_API
HyScanPlannerTrack *   hyscan_planner_track_extend       (const HyScanPlannerTrack  *track,
                                                          const HyScanPlannerZone   *zone);

HYSCAN_API
gboolean               hyscan_planner_plan_equal         (const HyScanTrackPlan     *plan1,
                                                          const HyScanTrackPlan     *plan2);

HYSCAN_API
HyScanPlannerZone *    hyscan_planner_zone_new            (void);

HYSCAN_API
HyScanPlannerZone *    hyscan_planner_zone_copy           (const HyScanPlannerZone   *zone);

HYSCAN_API
void                   hyscan_planner_zone_free           (HyScanPlannerZone         *zone);

HYSCAN_API
void                   hyscan_planner_zone_vertex_remove  (HyScanPlannerZone         *zone,
                                                           gsize                      index);

HYSCAN_API
void                   hyscan_planner_zone_vertex_append  (HyScanPlannerZone         *zone,
                                                           HyScanGeoPoint             point);

HYSCAN_API
void                   hyscan_planner_zone_vertex_dup     (HyScanPlannerZone         *zone,
                                                           gsize                      index);

G_END_DECLS

#endif /* __HYSCAN_PLANNER_H__ */
