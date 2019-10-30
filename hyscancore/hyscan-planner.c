/* hyscan-planner.c
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

/**
 * SECTION: hyscan-planner
 * @Short_description: структуры объектов планировщика галсов
 * @Title: HyScanPlanner
 *
 */

#include "hyscan-planner.h"
#include <string.h>
#include <math.h>
#define DEG2RAD(x) ((x) * G_PI / 180.0)   /* Перевод из градусов в радианы. */
#define EARTH_RADIUS        6378137.0     /* Радиус Земли. */

/**
 * hyscan_planner_origin_new:
 *
 * Создаёт пустую структуру #HyScanPlannerOrigin
 */
HyScanPlannerOrigin *
hyscan_planner_origin_new (void)
{
  HyScanPlannerOrigin *origin;

  origin = g_slice_new0 (HyScanPlannerOrigin);
  origin->type = HYSCAN_PLANNER_ORIGIN;

  return origin;
}

/**
 * hyscan_planner_origin_free:
 * @origin: указатель на структуру HyScanPlannerOrigin
 *
 * Копирует структуру #HyScanPlannerOrigin
 */
HyScanPlannerOrigin *
hyscan_planner_origin_copy (const HyScanPlannerOrigin *origin)
{
  HyScanPlannerOrigin *copy;

  if (origin == NULL)
    return NULL;

  copy = hyscan_planner_origin_new ();
  copy->origin = origin->origin;

  return copy;
}

/**
 * hyscan_planner_origin_free:
 * @origin: указатель на структуру HyScanPlannerOrigin
 *
 * Удаляет структуру #HyScanPlannerOrigin
 */
void
hyscan_planner_origin_free (HyScanPlannerOrigin *origin)
{
  if (origin == NULL)
    return;

  g_slice_free (HyScanPlannerOrigin, origin);
}

/**
 * hyscan_planner_track_new:
 *
 * Создаёт пустую структуру #HyScanPlannerTrack
 */
HyScanPlannerTrack *
hyscan_planner_track_new (void)
{
  HyScanPlannerTrack *track;
  
  track = g_slice_new0 (HyScanPlannerTrack);
  track->type = HYSCAN_PLANNER_TRACK;

  return track;
}

/**
 * hyscan_planner_track_copy:
 * @track: указатель на структуру HyScanPlannerTrack
 *
 * Копирует структуру #HyScanPlannerTrack
 */
HyScanPlannerTrack *
hyscan_planner_track_copy (const HyScanPlannerTrack *track)
{
  HyScanPlannerTrack *copy;

  if (track == NULL)
    return NULL;

  copy = hyscan_planner_track_new ();
  copy->zone_id = g_strdup (track->zone_id);
  copy->name = g_strdup (track->name);
  copy->number = track->number;
  copy->speed = track->speed;
  copy->start = track->start;
  copy->end = track->end;

  return copy;
}

/**
 * hyscan_planner_track_free:
 * @track: указатель на структуру HyScanPlannerTrack
 *
 * Удаляет структуру #HyScanPlannerTrack
 */
void
hyscan_planner_track_free (HyScanPlannerTrack *track)
{
  if (track == NULL)
    return;

  g_free (track->zone_id);
  g_free (track->name);
  g_slice_free (HyScanPlannerTrack, track);
}

/**
 * hyscan_planner_track_geo:
 * @track: указатель на структуру с галсом #HyScanPlannerTrack
 *
 * Создаёт объект #HyScanGeo, в топоцентрической системе координат которого
 * начало координат совпадает с началом галса @track, а ось OX направлена по
 * направлению движения на галсе.
 *
 * Returns: (transfer-full): новый объект #HyScanGeo
 */
HyScanGeo *
hyscan_planner_track_geo (const HyScanPlannerTrack *track)
{
  HyScanGeo *tmp_geo;
  HyScanGeoGeodetic origin;
  HyScanGeoCartesian2D start, end;

  origin = track->start;
  origin.h = 0;

  tmp_geo = hyscan_geo_new (origin, HYSCAN_GEO_ELLIPSOID_WGS84);
  hyscan_geo_geo2topoXY (tmp_geo, &start, track->start);
  hyscan_geo_geo2topoXY (tmp_geo, &end, track->end);
  origin.h = atan2 (end.x - start.x, end.y - start.y) / G_PI * 180.0;

  g_object_unref (tmp_geo);

  return hyscan_geo_new (origin, HYSCAN_GEO_ELLIPSOID_WGS84);
}

/**
 * hyscan_planner_track_angle:
 * @track: указатель на структуру с галсом #HyScanPlannerTrack
 *
 * Определяет азимут из начала галса к концу галса.
 *
 * Returns: значение азимута в градусах
 */
gdouble
hyscan_planner_track_angle (const HyScanPlannerTrack *track)
{
  gdouble lat1, lat2, lon1, lon2, dlon;

  lat1 = DEG2RAD (track->start.lat);
  lon1 = DEG2RAD (track->start.lon);
  lat2 = DEG2RAD (track->end.lat);
  lon2 = DEG2RAD (track->end.lon);
  dlon = lon2 - lon1;

  return atan2 (sin(dlon) * cos (lat2), cos (lat1) * sin (lat2) - sin (lat1) * cos (lat2) * cos (dlon)) / G_PI * 180.0;
}

/**
 * hyscan_planner_track_length:
 * @track: указатель на структуру с галсом #HyScanPlannerTrack
 *
 * Определяет длину галса.
 *
 * Returns: длина галса в метрах
 */
gdouble
hyscan_planner_track_length (const HyScanPlannerTrack *track)
{
  gdouble lon1r;
  gdouble lat1r;

  gdouble lon2r;
  gdouble lat2r;

  gdouble u;
  gdouble v;

  lat1r = DEG2RAD (track->start.lat);
  lon1r = DEG2RAD (track->start.lon);
  lat2r = DEG2RAD (track->end.lat);
  lon2r = DEG2RAD (track->end.lon);

  u = sin ((lat2r - lat1r) / 2);
  v = sin ((lon2r - lon1r) / 2);

  return 2.0 * EARTH_RADIUS * asin (sqrt (u * u + cos (lat1r) * cos (lat2r) * v * v));
}

/**
 * hyscan_planner_zone_new:
 *
 * Создаёт пустую структуру #HyScanPlannerZone
 */
HyScanPlannerZone *
hyscan_planner_zone_new (void)
{
  HyScanPlannerZone *zone;

  zone = g_slice_new0 (HyScanPlannerZone);
  zone->type = HYSCAN_PLANNER_ZONE;

  return zone;
}

/**
 * hyscan_planner_zone_copy:
 * @zone: указатель на структуру HyScanPlannerZone
 *
 * Копирует структуру #HyScanPlannerZone
 */
HyScanPlannerZone *
hyscan_planner_zone_copy (const HyScanPlannerZone *zone)
{
  HyScanPlannerZone *copy;

  if (zone == NULL)
    return NULL;

  copy = hyscan_planner_zone_new ();
  copy->name = g_strdup (zone->name);
  copy->ctime = zone->ctime;
  copy->mtime = zone->mtime;
  copy->points_len = zone->points_len;
  copy->points = g_new0 (HyScanGeoGeodetic, zone->points_len);
  memcpy (copy->points, zone->points, sizeof (HyScanGeoGeodetic) * zone->points_len);

  return copy;
}

/**
 * hyscan_planner_zone_free:
 * @zone: указатель на структуру HyScanPlannerZone
 *
 * Удаляет структуру #HyScanPlannerZone
 */
void
hyscan_planner_zone_free (HyScanPlannerZone *zone)
{
  if (zone == NULL)
    return;

  g_free (zone->points);
  g_free (zone->name);
  g_slice_free (HyScanPlannerZone, zone);
}

/**
 * hyscan_planner_zone_vertex_remove:
 * @zone: указатель на структуру #HyScanPlannerZone
 * @index: индекс вершины, которую надо удалить
 *
 * Удаляет вершину @index из границ зоны @zone.
 */
void
hyscan_planner_zone_vertex_remove (HyScanPlannerZone *zone,
                                   gsize              index)
{
  gsize i;
  g_return_if_fail (index < zone->points_len);

  for (i = index + 1; i < zone->points_len; ++i)
    zone->points[i - 1] = zone->points[i];

  zone->points_len--;
}

/**
 * hyscan_planner_zone_vertex_dup:
 * @zone: указатель на структуру #HyScanPlannerZone
 * @index: индекс вершины, которую надо продублировать
 *
 * Вставляет после вершины @index копию этой вершины.
 */
void
hyscan_planner_zone_vertex_dup (HyScanPlannerZone *zone,
                                gsize              index)
{
  gsize i;
  g_return_if_fail (index < zone->points_len);

  zone->points_len++;
  zone->points = g_realloc_n (zone->points, zone->points_len, sizeof (*zone->points));

  for (i = zone->points_len - 1; i > index; --i)
    zone->points[i] = zone->points[i-1];
}
