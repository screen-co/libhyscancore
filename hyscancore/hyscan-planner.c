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
 * - hyscan_planner_origin_equal() - сравнение,
 * - hyscan_planner_origin_free() - удаление.
 *
 * #HyScanPlannerTrack - непосредственно план прямолинейного галса.
 *
 * - hyscan_planner_track_new() - создание,
 * - hyscan_planner_track_copy() - копирование,
 * - hyscan_planner_track_equal() - сравнение,
 * - hyscan_planner_track_free() - удаление,
 * - hyscan_planner_track_record_append() - добавление галса в список записанных по этому плану,
 * - hyscan_planner_track_record_delete()- удаление галса из списка записанных по этому плану.
 *
 * #HyScanPlannerZone - границы полигона:
 *
 * - hyscan_planner_zone_new() - создание,
 * - hyscan_planner_zone_copy() - копирование,
 * - hyscan_planner_zone_equal() - сравнение,
 * - hyscan_planner_zone_free() - удаление,
 * - hyscan_planner_zone_vertex_append() - добавление вершины,
 * - hyscan_planner_zone_vertex_dup() - дублирование вершины,
 * - hyscan_planner_zone_vertex_remove() - удаление вершины.
 *
 */

#include "hyscan-planner.h"
#include "hyscan-cartesian.h"
#include <string.h>
#include <math.h>
#define DEG2RAD(x) ((x) * G_PI / 180.0)   /* Перевод из градусов в радианы. */
#define EARTH_RADIUS        6378137.0     /* Радиус Земли. */

static gboolean      hyscan_planner_geo_point_equal  (const HyScanGeoPoint   *a,
                                                      const HyScanGeoPoint   *b);

G_DEFINE_BOXED_TYPE_WITH_CODE (HyScanPlannerOrigin, hyscan_planner_origin,
                               hyscan_planner_origin_copy, hyscan_planner_origin_free,
                               HYSCAN_OBJECT_DEFINE_EQUAL_FUNC (hyscan_planner_origin_equal))
G_DEFINE_BOXED_TYPE_WITH_CODE (HyScanPlannerTrack, hyscan_planner_track,
                               hyscan_planner_track_copy, hyscan_planner_track_free,
                               HYSCAN_OBJECT_DEFINE_EQUAL_FUNC (hyscan_planner_track_equal))
G_DEFINE_BOXED_TYPE_WITH_CODE (HyScanPlannerZone, hyscan_planner_zone,
                               hyscan_planner_zone_copy, hyscan_planner_zone_free,
                               HYSCAN_OBJECT_DEFINE_EQUAL_FUNC (hyscan_planner_zone_equal))

/* Функция сравнивает значения двух #HyScanGeoPoint. */
static gboolean
hyscan_planner_geo_point_equal (const HyScanGeoPoint *a,
                                const HyScanGeoPoint *b)
{
  return fabs (a->lat - b->lat) < 1e-9 && fabs (a->lon - b->lon) < 1e-9;
}

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
 * hyscan_planner_origin_equal:
 * @origin1: указатель на структуру HyScanPlannerOrigin
 * @origin2: указатель на структуру HyScanPlannerOrigin для сравнения
 *
 * Функция сравнивает значения двух структур #HyScanPlannerOrigin.
 *
 * Returns: %TRUE, если значения двух структур равны; иначе %FALSE.
 */
gboolean
hyscan_planner_origin_equal (const HyScanPlannerOrigin *origin1,
                             const HyScanPlannerOrigin *origin2)
{
  return fabs (origin1->azimuth - origin2->azimuth) < 1e-9 &&
         hyscan_planner_geo_point_equal (&origin1->origin, &origin2->origin);
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
 * hyscan_planner_track_equal:
 * @zone1: указатель на структуру HyScanPlannerTrack
 * @zone2: указатель на структуру HyScanPlannerTrack для сравнения
 *
 * Функция сравнивает значения двух структур #HyScanPlannerTrack.
 *
 * Returns: %TRUE, если значения двух структур равны; иначе %FALSE.
 */
gboolean
hyscan_planner_track_equal (const HyScanPlannerTrack *a,
                            const HyScanPlannerTrack *b)
{
  gint i;

  if (g_strcmp0 (a->zone_id, b->zone_id) != 0 ||
      g_strcmp0 (a->name, b->name) != 0 ||
      a->number != b->number ||
      fabs (a->plan.speed - b->plan.speed) > 1e-5 ||
      !hyscan_planner_geo_point_equal(&a->plan.start, &b->plan.start) ||
      !hyscan_planner_geo_point_equal(&a->plan.end, &b->plan.end))
    {
      return FALSE;
    }

  if (a->records == NULL && b->records == NULL)
    return TRUE;
  else if (a->records == NULL || b->records == NULL)
    return FALSE;

  if (g_strv_length (a->records) != g_strv_length (b->records))
    return FALSE;

  for (i = 0; a->records[i] != NULL; i++)
    {
      if (g_strcmp0 (a->records[i], b->records[i]) != 0)
        return FALSE;
    }

  return TRUE;
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
 * hyscan_planner_track_get_plan:
 * @track: указатель на #HyScanPlannerTrack
 *
 * Returns: (transfer full): параметры галса, для удаления hyscan_track_plan_free().
 */
HyScanTrackPlan *
hyscan_planner_track_get_plan (HyScanPlannerTrack *track)
{
  return hyscan_track_plan_copy (&track->plan);
}

/**
 * hyscan_planner_track_geo:
 * @track: указатель на структуру с галсом #HyScanPlannerTrack
 * @angle: (out) (optional): используемое направление оси OX, градусы
 *
 * Создаёт объект #HyScanGeo, в топоцентрической системе координат которого
 * начало координат совпадает с началом галса @track, а ось OX направлена по
 * направлению движения на галсе.
 *
 * Returns: (transfer-full): новый объект #HyScanGeo
 */
HyScanGeo *
hyscan_planner_track_geo (const HyScanTrackPlan *plan,
                          gdouble               *angle)
{
  HyScanGeo *tmp_geo;
  HyScanGeoGeodetic origin;
  HyScanGeoCartesian2D start, end;

  origin.lat = plan->start.lat;
  origin.lon = plan->start.lon;
  origin.h = 0.0;

  tmp_geo = hyscan_geo_new (origin, HYSCAN_GEO_ELLIPSOID_WGS84);
  if (!hyscan_geo_geo2topoXY0 (tmp_geo, &start, plan->start) ||
      !hyscan_geo_geo2topoXY0 (tmp_geo, &end, plan->end))
    {
      g_warning ("HyScanPlanner: failed to transform coordinates");

      g_object_unref (tmp_geo);
      return NULL;
    }

  origin.h = atan2 (-end.y + start.y, end.x - start.x) / G_PI * 180.0;

  g_object_unref (tmp_geo);

  if (angle != NULL)
    *angle = origin.h < 0 ? origin.h + 360.0 : origin.h;

  return hyscan_geo_new (origin, HYSCAN_GEO_ELLIPSOID_WGS84);
}

/**
 * hyscan_planner_track_angle:
 * @track: указатель на структуру с галсом #HyScanPlannerTrack
 *
 * Определяет приближенное значение азимута из начала галса к концу галса.
 *
 * Returns: значение азимута в радианах
 */
gdouble
hyscan_planner_track_angle (const HyScanPlannerTrack *track)
{
  gdouble lat1, lat2, lon1, lon2, dlon;

  lat1 = DEG2RAD (track->plan.start.lat);
  lon1 = DEG2RAD (track->plan.start.lon);
  lat2 = DEG2RAD (track->plan.end.lat);
  lon2 = DEG2RAD (track->plan.end.lon);
  dlon = lon2 - lon1;

  return atan2 (sin (dlon) * cos (lat2), cos (lat1) * sin (lat2) - sin (lat1) * cos (lat2) * cos (dlon));
}

/**
 * hyscan_planner_track_length:
 * @plan: указатель на структуру с галсом #HyScanTrackPlan
 *
 * Определяет длину галса.
 *
 * Returns: длина галса в метрах
 */
gdouble
hyscan_planner_track_length (const HyScanTrackPlan *plan)
{
  HyScanGeo *geo;
  HyScanGeoCartesian2D end;

  geo = hyscan_planner_track_geo (plan, NULL);
  hyscan_geo_geo2topoXY0 (geo, &end, plan->end);
  g_object_unref (geo);

  return end.x;
}

/**
 * hyscan_planner_track_transit:
 * @plan1: план первого галса
 * @plan2: план второго галса
 *
 * Оценивает длину траектории перехода с конца галса @plan1 к началу галса @plan2.
 *
 * Returns: оценочная длина перехода от первого ко второму плана галса.
 */
gdouble
hyscan_planner_track_transit (const HyScanTrackPlan *plan1,
                              const HyScanTrackPlan *plan2)
{
  HyScanGeo *geo;
  HyScanGeoCartesian2D end;
  gdouble dist;

/* Рассмотрим несколько вариантов для оценки длины перехода между галсами:
 * (1) расстояние перехода меньше тактического диаметра → судно делает U-разворот,
 * (2) расстояние перехода много больше тактического диаметра → судно покрывает расстояние и делает U-разворот,
 * (3) для промежуточных вариантов линейно интерполируем. */

/* Тактический диаметр для судна длиной L = 2 метра: ~4L = 8 метров. */
#define TACTICAL_DIAMETER 8
/* Длина U-разворота для тактического диаметра = πD / 2. */
#define U_TURN_LENGTH 12.5

  geo = hyscan_planner_track_geo (plan2, NULL);
  if (!hyscan_geo_geo2topoXY0 (geo, &end, plan1->end))
    end.x = end.y = 0;
  g_object_unref (geo);

  dist = hypot (end.x, end.y);
  if (dist < TACTICAL_DIAMETER)
    return U_TURN_LENGTH;

  if (dist > 4 * U_TURN_LENGTH)
    return dist + U_TURN_LENGTH;

  return U_TURN_LENGTH + (dist - TACTICAL_DIAMETER) * (4 * U_TURN_LENGTH) / (4 * U_TURN_LENGTH - TACTICAL_DIAMETER);
}

/**
 * hyscan_planner_track_extend:
 * @track: указатель на галс #HyScanPlannerTrack
 * @zone: указатель на зону полигона #HyScanPlannerZone
 *
 * Функция создаёт копию галса @track, растягивая (или сжимая) галс до
 * границ указанного полигона @zone.
 *
 * Направление исходного галса сохраняется.
 *
 * Returns: (transfer full): указатель на растянутую (сжатую) копию галса.
 * Для удаления hyscan_planner_track_free().
 */
HyScanPlannerTrack *
hyscan_planner_track_extend (const HyScanPlannerTrack  *track,
                             const HyScanPlannerZone   *zone)
{
  HyScanPlannerTrack *modified_track;
  HyScanGeoCartesian2D *vertices;
  HyScanGeoCartesian2D *points;
  HyScanGeoCartesian2D start, end;
  HyScanGeo *geo;
  gsize i, vertices_len;
  gsize end_i;
  guint points_len;
  gdouble dx, dy;

  modified_track = hyscan_planner_track_copy (track);
  geo = hyscan_planner_track_geo (&track->plan, NULL);

  vertices_len = zone->points_len;
  vertices = g_new (HyScanGeoCartesian2D, zone->points_len);
  for (i = 0; i < zone->points_len; ++i)
    hyscan_geo_geo2topoXY0 (geo, &vertices[i], zone->points[i]);

  hyscan_geo_geo2topoXY0 (geo, &start, track->plan.start);
  hyscan_geo_geo2topoXY0 (geo, &end, track->plan.end);

  /* Находим точки пересечения отрезка с прямой. */
  points = hyscan_cartesian_polygon_cross (vertices, vertices_len, &start, &end, &points_len);
  if (points_len < 2)
    goto exit;

  dx = end.x - start.x;
  dy = end.y - start.y;

  /* Ищем индекс точки пересечения, следующей за концом галса. */
  end_i = points_len;
  for (i = 0; i < points_len; i += 2)
    {
      gdouble end_tx, end_ty;

      end_tx = dx != 0 ? (points[i].x - end.x) / dx : -1;
      end_ty = dy != 0 ? (points[i].y - end.y) / dy : -1;
      if (end_i == points_len && (end_tx >= 0 || end_ty >= 0))
        end_i = i;
    }

  /* Индекс точки конца галса должен быть нечётным. */
  if (end_i == 0)
    end_i = 1;
  else if (end_i % 2 == 0)
    end_i -= 1;

  hyscan_geo_topoXY2geo0 (geo, &modified_track->plan.start, points[end_i - 1]);
  hyscan_geo_topoXY2geo0 (geo, &modified_track->plan.end, points[end_i]);

exit:
  g_object_unref (geo);
  g_free (points);
  g_free (vertices);

  return modified_track;
}

/**
 * hyscan_planner_plan_equal:
 * @plan1: первый план галса
 * @plan2: второй план галса
 *
 * Проверяет, что планы галса совпадают. Все параметры планов сравниваются
 * с некоторой допустимой ошибкой, так что функция не гарантируется точное
 * совпадение значений параметров.
 *
 * Returns: %TRUE, если планы одинаковые; иначе %FALSE
 */
gboolean
hyscan_planner_plan_equal (const HyScanTrackPlan *plan1,
                           const HyScanTrackPlan *plan2)
{
  if (plan1 == NULL && plan2 == NULL)
    return TRUE;

  if (plan1 == NULL || plan2 == NULL)
    return FALSE;

  return fabs (plan1->speed - plan2->speed) < .001 &&
         fabs (plan1->start.lat - plan2->start.lat) < 1e-6 &&
         fabs (plan1->start.lon - plan2->start.lon) < 1e-6 &&
         fabs (plan1->end.lat - plan2->end.lat) < 1e-6 &&
         fabs (plan1->end.lon - plan2->end.lon) < 1e-6;
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
 * hyscan_planner_zone_equal:
 * @zone1: указатель на структуру HyScanPlannerZone
 * @zone2: указатель на структуру HyScanPlannerZone для сравнения
 *
 * Функция сравнивает значения двух структур #HyScanPlannerZone.
 *
 * Returns: %TRUE, если значения двух структур равны; иначе %FALSE.
 */
gboolean
hyscan_planner_zone_equal (const HyScanPlannerZone *zone1,
                           const HyScanPlannerZone *zone2)
{
  guint i;

  if (zone1->type != zone2->type ||
      zone1->ctime != zone2->ctime ||
      zone1->mtime != zone2->mtime ||
      zone1->points_len != zone2->points_len ||
      g_strcmp0 (zone1->name, zone2->name) != 0)
    {
      return FALSE;
    }

  for (i = 0; i < zone1->points_len; i++)
    {
      if (!hyscan_planner_geo_point_equal (&zone1->points[i], &zone2->points[i]))
        return FALSE;
    }

  return TRUE;
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
