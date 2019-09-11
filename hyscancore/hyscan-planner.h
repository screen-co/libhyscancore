/* hyscan-planner.h
 *
 * Copyright 2019 Screen LLC, Alexey Sakhnov <alexsakhnov@gmail.com>
 *
 * This file is part of HyScanGui library.
 *
 * HyScanGui is dual-licensed: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * HyScanGui is distributed in the hope that it will be useful,
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

/* HyScanGui имеет двойную лицензию.
 *
 * Во-первых, вы можете распространять HyScanGui на условиях Стандартной
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
#include <hyscan-nav-model.h>
#include <hyscan-db.h>

G_BEGIN_DECLS

#define HYSCAN_PLANNER_ORIGIN_ID "origin"

typedef union _HyScanPlannerObject HyScanPlannerObject;
typedef struct _HyScanPlannerTrack HyScanPlannerTrack;
typedef struct _HyScanPlannerZone HyScanPlannerZone;
typedef struct _HyScanPlannerOrigin HyScanPlannerOrigin;

typedef enum
{
  HYSCAN_PLANNER_INVALID,
  HYSCAN_PLANNER_ZONE,
  HYSCAN_PLANNER_TRACK,
  HYSCAN_PLANNER_ORIGIN,
} HyScanPlannerObjectType;

/**
 * HyScanPlannerTrack:
 * @id: уникальный идентификатор
 * @zone_id: идентификатор зоны, в котрой находится галс
 * @number: порядковый номер галса
 * @speed: скокрость движения судна, м/с
 * @name: название
 * @start: геокоординаты начала
 * @start: геокоординаты конца
 *
 * Параметры запланированного галса
 */
struct _HyScanPlannerTrack
{
  HyScanPlannerObjectType  type;
  gchar                   *zone_id;
  guint                    number;
  gdouble                  speed;
  gchar                   *name;
  HyScanGeoGeodetic        start;
  HyScanGeoGeodetic        end;
};

/**
 * HyScanPlannerZone:
 * @type: тип объекта
 * @name: название зоны
 * @points: (element-type HyScanGeoGeodetic): список вершин многоугольника, ограничивающего зону
 * @points_len: число вершин многоугольника
 *
 * Параметры зоны исследования
 */
struct _HyScanPlannerZone
{
  HyScanPlannerObjectType  type;
  gchar                   *name;
  HyScanGeoGeodetic       *points;
  gsize                    points_len;
  gint64                   ctime;
  gint64                   mtime;
};

/**
 * HyScanPlannerZone:
 * @type: тип объекта
 * @origin:
 *
 * Референсная точка, которая считается началом координат для топографической системы координат
 */
struct _HyScanPlannerOrigin
{
  HyScanPlannerObjectType  type;
  HyScanGeoGeodetic        origin;
};

union _HyScanPlannerObject
{
  HyScanPlannerObjectType type;
  HyScanPlannerTrack      track;
  HyScanPlannerZone       zone;
  HyScanPlannerOrigin     ref_point;
};

HYSCAN_API
void                   hyscan_planner_zone_free        (HyScanPlannerZone        *zone);

HYSCAN_API
void                   hyscan_planner_track_free       (HyScanPlannerTrack        *track);

HYSCAN_API
void                   hyscan_planner_origin_free      (HyScanPlannerOrigin       *ref_point);

HYSCAN_API
HyScanPlannerTrack *   hyscan_planner_track_copy       (const HyScanPlannerTrack  *track);

HYSCAN_API
HyScanPlannerOrigin *  hyscan_planner_origin_copy      (const HyScanPlannerOrigin *origin);

HYSCAN_API
HyScanPlannerZone *    hyscan_planner_zone_copy        (const HyScanPlannerZone   *zone);

G_END_DECLS

#endif /* __HYSCAN_PLANNER_H__ */
