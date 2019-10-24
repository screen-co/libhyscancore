/* hyscan-cartesian.c
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
 * SECTION: hyscan-cartesian
 * @Short_description: Набор функций для работы с двумерным пространством
 * @Title: HyScanCartesian
 *
 * Класс включает в себя функции для работы с декартовой системой координат.
 * Функции находят применение при рисовании и взаимодействии пользователя
 * с отрисованным интерфейсом, когда необходимо определять взаимное
 * расположение элементов и выполнять геометрические преобразования.
 *
 * Проверка, что элемент находится внутри заданной области:
 * - hyscan_cartesian_is_point_inside() - точка внутри области;
 * - hyscan_cartesian_is_inside() - отрезок внутри области.
 *
 * Вычисление расстояния:
 * - hyscan_cartesian_distance() - расстояние между двумя точками;
 * - hyscan_cartesian_distance_to_line() - расстояние от линии до точки.
 *
 * Поворот:
 * - hyscan_cartesian_rotate() - поворот точки;
 * - hyscan_cartesian_rotate_area() - поворот прямоугольной области.
 *
 */

#include "hyscan-cartesian.h"
#include <math.h>

static inline gboolean   hyscan_cartesian_is_between         (gdouble               val1,
                                                              gdouble               val2,
                                                              gdouble               boundary);

static gboolean          hyscan_cartesian_segments_intersect (HyScanGeoCartesian2D *p1,
                                                              HyScanGeoCartesian2D *q1,
                                                              HyScanGeoCartesian2D *p2,
                                                              HyScanGeoCartesian2D *q2);
static gboolean          hyscan_cartesian_on_segment         (HyScanGeoCartesian2D *p,
                                                              HyScanGeoCartesian2D *q,
                                                              HyScanGeoCartesian2D *r);
static gint              hyscan_cartesian_orientation        (HyScanGeoCartesian2D *p,
                                                              HyScanGeoCartesian2D *q,
                                                              HyScanGeoCartesian2D *r);

/* Для трёх заданных точек p, q, r на одной прямой, функция проверяет
 * лежит ли точка q на отрезке 'pr' */
static gboolean
hyscan_cartesian_on_segment (HyScanGeoCartesian2D *p,
                             HyScanGeoCartesian2D *q,
                             HyScanGeoCartesian2D *r)
{
  if (q->x <= MAX (p->x, r->x) && q->x >= MIN (p->x, r->x) &&
      q->y <= MAX (p->y, r->y) && q->y >= MIN (p->y, r->y))
    {
      return TRUE;
    }

  return FALSE;
}

/* Находит ориентацию тройки (p, q, r).
 * Функция возвращает следующие значения:
 * 0 --> p, q и r колллинеарны,
 * 1 --> по часовой стрелке,
 * 2 --> против часовой стрелки. */
static gint
hyscan_cartesian_orientation (HyScanGeoCartesian2D *p,
                              HyScanGeoCartesian2D *q,
                              HyScanGeoCartesian2D *r)
{
  /* См. https://www.geeksforgeeks.org/orientation-3-ordered-points/. */
  gdouble val;

  val = (q->y - p->y) * (r->x - q->x) -
        (q->x - p->x) * (r->y - q->y);

  if (val == 0)
    return 0;               /* Точки лежат на одной прямой. */

  return (val > 0) ? 1 : 2; /* Точки расположены по часовой или против часовой стрелки. */
}

/* Функция возвращает TRUE, если отрезки 'p1q1' и 'p2q2' пересекаются.
 * https://www.geeksforgeeks.org/check-if-two-given-line-segments-intersect/ */
static gboolean
hyscan_cartesian_segments_intersect (HyScanGeoCartesian2D *p1,
                                     HyScanGeoCartesian2D *q1,
                                     HyScanGeoCartesian2D *p2,
                                     HyScanGeoCartesian2D *q2)
{
  gint o1, o2, o3, o4;

  /* Находим 4 ориентации, необходимые для общего и частных случаев. */
  o1 = hyscan_cartesian_orientation (p1, q1, p2);
  o2 = hyscan_cartesian_orientation (p1, q1, q2);
  o3 = hyscan_cartesian_orientation (p2, q2, p1);
  o4 = hyscan_cartesian_orientation (p2, q2, q1);

  /* Общий случай. */
  if (o1 != o2 && o3 != o4)
    return TRUE;

  /* Частные случаи
   * p1, q1 и p2 на одной прямой и p2 лежит на отрезке p1q1 */
  if (o1 == 0 && hyscan_cartesian_on_segment (p1, p2, q1))
    return TRUE;

  /* p1, q1 и q2 на одной прямой и q2 лежит на отрезке p1q1 */
  if (o2 == 0 && hyscan_cartesian_on_segment (p1, q2, q1))
    return TRUE;

  /* p2, q2 и p1 на одной прямой и p1 лежит на отрезке p2q2. */
  if (o3 == 0 && hyscan_cartesian_on_segment (p2, p1, q2))
    return TRUE;

  /* p2, q2 и q1 на одной прямой и q1 лежит на отрезке p2q2. */
  if (o4 == 0 && hyscan_cartesian_on_segment (p2, q1, q2))
    return TRUE;

  return FALSE; /* Ни один из предыдущих случаев. */
}

static inline gboolean
hyscan_cartesian_is_between (gdouble val1,
                             gdouble val2,
                             gdouble boundary)
{
  return (MIN (val1, val2) <= boundary) && (MAX(val1, val2) >= boundary);
}

/**
 * hyscan_cartesian_is_point_inside:
 * @point: координаты точки
 * @area_from: координаты одной границы области
 * @area_to: координаты второй границы области
 *
 * Определяет, находится ли точка @point внутри прямоугольной области,
 * ограниченной точками @area_from и @area_to.
 *
 * Returns: %TRUE, если точка @point находится внтури указанной области
 */
gboolean
hyscan_cartesian_is_point_inside (HyScanGeoCartesian2D *point,
                                  HyScanGeoCartesian2D *area_from,
                                  HyScanGeoCartesian2D *area_to)
{
  return hyscan_cartesian_is_between (area_from->x, area_to->x, point->x) &&
         hyscan_cartesian_is_between (area_from->y, area_to->y, point->y);

}

/**
 * hyscan_cartesian_is_inside:
 * @segment_start: координаты начала отрезка
 * @segment_end: координаты конца отрезка
 * @area_from: точка с минимальными координатами внутри области
 * @area_to: точка с максимальными координатами внутри области
 *
 * Проверяет, находится ли отрезок с концами @segment_end, @segment_start внутри
 * области с координатами @area_from, @area_to.
 *
 * Returns: %TRUE, если часть отрезка или весь отрезок лежит внутри области
 */
gboolean
hyscan_cartesian_is_inside (HyScanGeoCartesian2D *segment_start,
                            HyScanGeoCartesian2D *segment_end,
                            HyScanGeoCartesian2D *area_from,
                            HyScanGeoCartesian2D *area_to)
{
  HyScanGeoCartesian2D vertex1, vertex2;

  /* 1. Один из концов отрезка внутри области. */
  if (hyscan_cartesian_is_point_inside (segment_start, area_from, area_to) ||
      hyscan_cartesian_is_point_inside (segment_end,   area_from, area_to))
    {
      return TRUE;
    }

  /* 2. Отрезок пересекает одну из сторон прямоугольника. */
  vertex1.x = area_from->x;
  vertex1.y = area_to->y;
  vertex2.x = area_to->x;
  vertex2.y = area_from->y;

  return hyscan_cartesian_segments_intersect (segment_start, segment_end, area_from, &vertex1) ||
         hyscan_cartesian_segments_intersect (segment_start, segment_end, area_from, &vertex2) ||
         hyscan_cartesian_segments_intersect (segment_start, segment_end, area_to, &vertex1) ||
         hyscan_cartesian_segments_intersect (segment_start, segment_end, area_to, &vertex2);
}

/**
 * hyscan_cartesian_distance_to_line:
 * @p1: координаты первой точки на прямой
 * @p2: координаты второй точки на прямой
 * @point: координаты целевой точки
 * @nearest_point: (out) (nullable): координаты проекции точки @point на прямую
 * Returns: расстояние от точки @point до прямой @p1 - @p2
 */
gdouble
hyscan_cartesian_distance_to_line (HyScanGeoCartesian2D *p1,
                                   HyScanGeoCartesian2D *p2,
                                   HyScanGeoCartesian2D *point,
                                   HyScanGeoCartesian2D *nearest_point)
{
  gdouble dist;
  gdouble a, b, c;
  gdouble dist_ab2;

  HyScanGeoCartesian2D shift, m1, m2, pt;

  /* Делаем сдвиг всех точек, помещая начало отрезка p1 в ноль.
   * Это повысит точность вычислений, когда dist << MAX (p1->x, p2->x). */
  shift = *p1;

  m1.x = 0;
  m1.y = 0;

  m2.x = p2->x - shift.x;
  m2.y = p2->y - shift.y;

  pt.x = point->x - shift.x;
  pt.y = point->y - shift.y;

  p1 = &m1;
  p2 = &m2;
  point = &pt;

  a = p1->y - p2->y;
  b = p2->x - p1->x;
  c = p1->x * p2->y - p2->x * p1->y;

  dist_ab2 = pow (a, 2) + pow (b, 2);
  dist = fabs (a * point->x + b * point->y + c) / sqrt (dist_ab2);

  if (nearest_point != NULL)
    {
      nearest_point->x = shift.x + (b * (b * point->x - a * point->y) - a * c) / dist_ab2;
      nearest_point->y = shift.y + (a * (-b * point->x + a * point->y) - b * c) / dist_ab2;
    }

  return dist;
}

/**
 * hyscan_cartesian_side:
 * @start: одна точка на прямой
 * @end: вторая точка на прямой
 * @point: исследуемая точка
 *
 * Функция определеяет положение точки @point относительно прямой, проходящей
 * через точки @start и @end.
 *
 * Returns: 0, если точка лежит на прямой, -1 - если точка с одной стороны,
 *   1 - если точка с другой стороны
 */
gint
hyscan_cartesian_side (HyScanGeoCartesian2D *start,
                       HyScanGeoCartesian2D *end,
                       HyScanGeoCartesian2D *point)
{
  gdouble d;

  d = (point->x - start->x) * (end->y - start->y) - (point->y - start->y) * (end->x - start->x);

  return d == 0 ? 0 : (d > 0 ? 1 : -1);
}

/**
 * hyscan_cartesian_distance:
 * @p1: координаты первой точки
 * @p2: координаты второй точки
 *
 * Returns: расстояние между точками @p1 и @p2
 */
gdouble
hyscan_cartesian_distance (HyScanGeoCartesian2D *p1,
                           HyScanGeoCartesian2D *p2)
{
  gdouble dx, dy;

  dx = p1->x - p2->x;
  dy = p1->y - p2->y;

  return sqrt (dx * dx + dy * dy);
}

/**
 * hyscan_cartesian_distance:
 * @p1: координаты первой точки
 * @p2: координаты второй точки
 * @normal: (out): нормаль к отрезку (p1, p2)
 *
 * Функция определяет координаты вектора, ортонального к отрезку @p1 - @p2
 */
void
hyscan_cartesian_normal (HyScanGeoCartesian2D *p1,
                         HyScanGeoCartesian2D *p2,
                         HyScanGeoCartesian2D *normal)
{
  gdouble normal_length;

  normal->y = p2->x - p1->x;
  normal->x = p1->y - p2->y;

  normal_length = hypot (normal->x, normal->y);
  normal->x /= normal_length;
  normal->y /= normal_length;
}

/**
 * hyscan_cartesian_rotate:
 * @point: координаты точки до поворота
 * @center: точка, вокруг которой происходит поворот
 * @angle: угол поворота в радианах
 * @rotated: координаты точки после поворота
 *
 * Поворачивает точку @point относительно @center на угол @angle
 */
void
hyscan_cartesian_rotate (HyScanGeoCartesian2D *point,
                         HyScanGeoCartesian2D *center,
                         gdouble               angle,
                         HyScanGeoCartesian2D *rotated)
{
  gdouble cos_angle, sin_angle;

  cos_angle = cos (angle);
  sin_angle = sin (angle);

  rotated->x = (point->x - center->x) * cos_angle - (point->y - center->y) * sin_angle + center->x;
  rotated->y = (point->x - center->x) * sin_angle + (point->y - center->y) * cos_angle + center->y;
}

/**
 * hyscan_cartesian_extent:
 * @area_from: одна из вершин прямоугольника
 * @area_to: противоположная вершина прямоугольника
 * @center: ось вращения
 * @angle: угол поворота прямоугльника, радианы
 * @rotated_from: координаты вершины повёрнутого прямоугольника
 * @rotated_to: координаты противоположной вершины повёрнутого прямоугольника
 *
 * Определяет область extent, внутри которой находится прямоугольник с вершинами
 * @area_from и @area_to после поворота на угол @angle вокруг точки @center.
 *
 * Extent - прямоугольник, стороны которого параллельны осям координаты, а вершины
 * имеют координаты @extent_from и @extent_to.
 */
void
hyscan_cartesian_rotate_area (HyScanGeoCartesian2D *area_from,
                              HyScanGeoCartesian2D *area_to,
                              HyScanGeoCartesian2D *center,
                              gdouble               angle,
                              HyScanGeoCartesian2D *rotated_from,
                              HyScanGeoCartesian2D *rotated_to)
{
  HyScanGeoCartesian2D vertex, rotated, rotated_from_ret, rotated_to_ret;
  
  /* Первая вершина. */
  vertex = *area_from;
  hyscan_cartesian_rotate (&vertex, center, angle, &rotated);
  rotated_from_ret = rotated;
  rotated_to_ret = rotated;

  /* Вторая вершина. */
  vertex.x = area_to->x;
  hyscan_cartesian_rotate (&vertex, center, angle, &rotated);
  rotated_from_ret.x = MIN (rotated.x, rotated_from_ret.x); 
  rotated_from_ret.y = MIN (rotated.y, rotated_from_ret.y);
  rotated_to_ret.x = MAX (rotated.x, rotated_to_ret.x);
  rotated_to_ret.y = MAX (rotated.y, rotated_to_ret.y);

  /* Третья вершина. */
  vertex = *area_to;
  hyscan_cartesian_rotate (&vertex, center, angle, &rotated);
  rotated_from_ret.x = MIN (rotated.x, rotated_from_ret.x);
  rotated_from_ret.y = MIN (rotated.y, rotated_from_ret.y);
  rotated_to_ret.x = MAX (rotated.x, rotated_to_ret.x);
  rotated_to_ret.y = MAX (rotated.y, rotated_to_ret.y);

  /* Четвёртая вершина. */
  vertex.x = area_from->x;
  hyscan_cartesian_rotate (&vertex, center, angle, &rotated);
  rotated_from_ret.x = MIN (rotated.x, rotated_from_ret.x);
  rotated_from_ret.y = MIN (rotated.y, rotated_from_ret.y);
  rotated_to_ret.x = MAX (rotated.x, rotated_to_ret.x);
  rotated_to_ret.y = MAX (rotated.y, rotated_to_ret.y);

  *rotated_from = rotated_from_ret;
  *rotated_to = rotated_to_ret;
}

/**
 * hyscan_cartesian_is_inside_polygon:
 * @vertices: (array length=@n) (element-type HyScanGeoCartesian2D): массив с вершинами многоугольника
 * @n: число вершин многоугольника
 * @p: координаты точки
 *
 * Проверяет, находится ли указанная точка @p внутри многоугольника с вершинами @vertices.
 *
 * Returns: %TRUE, если точка лежит внутри многоугольника
 */
gboolean
hyscan_cartesian_is_inside_polygon (HyScanGeoCartesian2D  *vertices,
                                    gint                   n,
                                    HyScanGeoCartesian2D  *p)
{
  HyScanGeoCartesian2D extreme = { G_MAXDOUBLE, p->y }; /* Точка на бесконечности. */
  gint count = 0, i = 0;

  /* Должно быть как минимум три вершины в многоугольнике. */
  if (n < 3)
    return FALSE;

  /* Считаем количество пересечний со сторонами многоугольника. */
  for (i = 0; i < n; ++i)
   {
     gint next = (i + 1) % n;

     if (hyscan_cartesian_segments_intersect (&vertices[i], &vertices[next], p, &extreme))
       count++;
   }

  /* Возвращает TRUE, если нечётное число пересечений; иначе FALSE. */
  return count & 1;  /* То же самое, что (count%2 == 1) */
}
