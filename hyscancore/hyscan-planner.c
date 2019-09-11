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
 * @Short_description: модель данных списку запланированных галсов
 * @Title: HyScanPlanner
 *
 * Планировщик галсов позволяет загружать и сохранять схему плановых галсов проекта.
 *
 */

#include "hyscan-planner.h"
#include <string.h>

/**
 * hyscan_planner_zone_free:
 * @zone: указатель на структуру #HyScanPlannerZone
 *
 * Освобождает память, занятую структурой #HyScanPlannerZone
 */
void
hyscan_planner_zone_free (HyScanPlannerZone *zone)
{
  g_free (zone->points);
  g_free (zone->name);
  g_slice_free (HyScanPlannerZone, zone);
}

/**
 * hyscan_planner_track_free:
 * @track: указатель на структуру #HyScanPlannerTrack
 *
 * Освобождает память, занятую структурой #HyScanPlannerTrack
 */
void
hyscan_planner_track_free (HyScanPlannerTrack *track)
{
  g_free (track->zone_id);
  g_free (track->name);
  g_slice_free (HyScanPlannerTrack, track);
}

void
hyscan_planner_origin_free (HyScanPlannerOrigin *origin)
{
  g_slice_free (HyScanPlannerOrigin, origin);
}

HyScanPlannerTrack *
hyscan_planner_track_copy (const HyScanPlannerTrack *track)
{
  HyScanPlannerTrack *copy;

  copy = g_slice_new (HyScanPlannerTrack);
  copy->type = track->type;
  copy->zone_id = g_strdup (track->zone_id);
  copy->name = g_strdup (track->name);
  copy->number = track->number;
  copy->speed = track->speed;
  copy->start = track->start;
  copy->end = track->end;

  return copy;
}

HyScanPlannerOrigin *
hyscan_planner_origin_copy (const HyScanPlannerOrigin *origin)
{
  HyScanPlannerOrigin *copy;

  copy = g_slice_new (HyScanPlannerOrigin);
  copy->type = origin->type;
  copy->origin = origin->origin;

  return copy;
}

HyScanPlannerZone *
hyscan_planner_zone_copy (const HyScanPlannerZone *zone)
{
  HyScanPlannerZone *copy;

  copy = g_slice_new (HyScanPlannerZone);
  copy->type = zone->type;
  copy->name = g_strdup (zone->name);
  copy->ctime = zone->ctime;
  copy->mtime = zone->mtime;
  copy->points_len = zone->points_len;
  copy->points = g_new0 (HyScanGeoGeodetic, zone->points_len);
  memcpy (copy->points, zone->points, sizeof (HyScanGeoGeodetic) * zone->points_len);

  return copy;
}
