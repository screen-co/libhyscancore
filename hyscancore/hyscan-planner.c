/* hyscan-planner.c
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

/**
 * SECTION: hyscan-planner
 * @Short_description: структуры объектов планировщика галсов
 * @Title: HyScanPlanner
 *
 * Набор функций для создания объектов планировщика и их редактирования.
 *
 * #HyScanPlannerOrigin - параметры начала координат местной системы координат:
 *
 * - hyscan_planner_origin_new() - создание,
 * - hyscan_planner_origin_copy() - копирование,
 * - hyscan_planner_origin_free() - удаление.
 *
 * #HyScanPlannerTrack - непосредственно план прямолинейного галса.
 *
 * - hyscan_planner_track_new() - создание,
 * - hyscan_planner_track_copy() - копирование,
 * - hyscan_planner_track_free() - удаление,
 * - hyscan_planner_track_record_append() - добавление галса в список записанных по этому плану,
 * - hyscan_planner_track_record_delete()- удаление галса из списка записанных по этому плану.
 *
 * #HyScanPlannerZone - границы полигона:
 *
 * - hyscan_planner_zone_new() - создание,
 * - hyscan_planner_zone_copy() - копирование,
 * - hyscan_planner_zone_free() - удаление,
 * - hyscan_planner_zone_vertex_append() - добавление вершины,
 * - hyscan_planner_zone_vertex_dup() - дублирование вершины,
 * - hyscan_planner_zone_vertex_remove() - удаление вершины.
 *
 */

#include "hyscan-planner.h"
#include <string.h>

G_DEFINE_BOXED_TYPE (HyScanPlannerOrigin, hyscan_planner_origin, hyscan_planner_origin_copy, hyscan_planner_origin_free)
G_DEFINE_BOXED_TYPE (HyScanPlannerTrack, hyscan_planner_track, hyscan_planner_track_copy, hyscan_planner_track_free)
G_DEFINE_BOXED_TYPE (HyScanPlannerZone, hyscan_planner_zone, hyscan_planner_zone_copy, hyscan_planner_zone_free)

/**
 * hyscan_planner_origin_new:
 *
 * Создаёт пустую структуру #HyScanPlannerOrigin.
 */
HyScanPlannerOrigin *
hyscan_planner_origin_new (void)
{
  HyScanPlannerOrigin *origin;

  origin = g_slice_new0 (HyScanPlannerOrigin);
  origin->type = HYSCAN_TYPE_PLANNER_ORIGIN;

  return origin;
}

/**
 * hyscan_planner_origin_free:
 * @origin: указатель на структуру HyScanPlannerOrigin
 *
 * Копирует структуру #HyScanPlannerOrigin.
 */
HyScanPlannerOrigin *
hyscan_planner_origin_copy (const HyScanPlannerOrigin *origin)
{
  HyScanPlannerOrigin *copy;

  if (origin == NULL)
    return NULL;

  copy = hyscan_planner_origin_new ();
  copy->origin = origin->origin;
  copy->azimuth = origin->azimuth;

  return copy;
}

/**
 * hyscan_planner_origin_free:
 * @origin: указатель на структуру HyScanPlannerOrigin
 *
 * Удаляет структуру #HyScanPlannerOrigin.
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
 * Создаёт пустую структуру #HyScanPlannerTrack.
 */
HyScanPlannerTrack *
hyscan_planner_track_new (void)
{
  HyScanPlannerTrack *track;
  
  track = g_slice_new0 (HyScanPlannerTrack);
  track->type = HYSCAN_TYPE_PLANNER_TRACK;

  return track;
}

/**
 * hyscan_planner_track_copy:
 * @track: указатель на структуру HyScanPlannerTrack
 *
 * Копирует структуру #HyScanPlannerTrack.
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
  copy->records = g_strdupv (track->records);
  copy->number = track->number;
  copy->plan = track->plan;

  return copy;
}

/**
 * hyscan_planner_track_free:
 * @track: указатель на структуру #HyScanPlannerTrack
 *
 * Удаляет структуру #HyScanPlannerTrack.
 */
void
hyscan_planner_track_free (HyScanPlannerTrack *track)
{
  if (track == NULL)
    return;

  g_free (track->zone_id);
  g_free (track->name);
  g_strfreev (track->records);
  g_slice_free (HyScanPlannerTrack, track);
}

/**
 * hyscan_planner_track_record_append:
 * @track: указатель на #HyScanPlannerTrack
 * @record_id: идентификатор записанного галса
 *
 * Добавляет галс с идентификатором @record_id к списку записанных по этому плану галсов.
 */
void
hyscan_planner_track_record_append (HyScanPlannerTrack *track,
                                    const gchar        *record_id)
{
  guint n_records, n_blocks;

  /* Новый размер массива + NULL-терминирование. */
  n_records = track->records != NULL ? g_strv_length (track->records) + 1 : 1;
  n_blocks = n_records + 1;
  track->records = g_realloc_n (track->records, n_blocks, sizeof (gchar *));
  track->records[n_records - 1] = g_strdup (record_id);
  track->records[n_records] = NULL;
}

/**
 * hyscan_planner_track_record_delete:
 * @track: указатель на #HyScanPlannerTrack
 * @record_id: идентификатор записанного галса
 *
 * Удаляет запись @record_id из списка записей плана галса.
 */
void
hyscan_planner_track_record_delete (HyScanPlannerTrack *track,
                                    const gchar        *record_id)
{
  guint i;
  guint n_records;

  if (track->records == NULL)
    return;

  n_records = g_strv_length (track->records);
  for (i = 0; i < n_records; i++)
    {
      if (g_strcmp0 (record_id, track->records[i]) != 0)
        continue;

      g_free  (track->records[i]);
      track->records[i] = track->records[n_records - 1];
      track->records[n_records - 1] = NULL;
      break;
    }

  /* Если это был последний элемент, то удаляем весь массив. */
  if (g_strv_length (track->records) == 0)
    g_clear_pointer (&track->records, g_strfreev);

}

/**
 * hyscan_planner_zone_new:
 *
 * Создаёт пустую структуру #HyScanPlannerZone.
 */
HyScanPlannerZone *
hyscan_planner_zone_new (void)
{
  HyScanPlannerZone *zone;

  zone = g_slice_new0 (HyScanPlannerZone);
  zone->type = HYSCAN_TYPE_PLANNER_ZONE;

  return zone;
}

/**
 * hyscan_planner_zone_copy:
 * @zone: указатель на структуру HyScanPlannerZone
 *
 * Копирует структуру #HyScanPlannerZone.
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
  copy->points = g_new0 (HyScanGeoPoint, zone->points_len);
  memcpy (copy->points, zone->points, sizeof (HyScanGeoPoint) * zone->points_len);

  return copy;
}

/**
 * hyscan_planner_zone_free:
 * @zone: указатель на структуру HyScanPlannerZone
 *
 * Удаляет структуру #HyScanPlannerZone.
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
hyscan_planner_zone_vertex_append (HyScanPlannerZone *zone,
                                   HyScanGeoPoint     point)
{
  zone->points_len++;
  zone->points = g_realloc_n (zone->points, zone->points_len, sizeof (*zone->points));
  zone->points[zone->points_len - 1] = point;
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
