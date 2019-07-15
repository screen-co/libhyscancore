/* cartesian-test.c
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

#include <hyscan-cartesian.h>

static HyScanGeoCartesian2D points_inside[][2] = {
  {{ -1.0, -1.0}, {  2.0,  2.0   }},
  {{  0.5,  1.0}, {  0.5, -100.0 }},
  {{  0.5,  0.5}, { -2.0,  2.0   }},
  {{  0.5,  2.0}, {  0.6, -2.0   }},
};

static HyScanGeoCartesian2D points_outside[][2] = {
  {{  -1.0,  0.0}, { -2.0, 2.0}},
};

void
test_distance (void)
{
  struct
  {
    HyScanGeoCartesian2D p1;
    HyScanGeoCartesian2D p2;
    gdouble distance;
  } data[] = {{ {0.0, 0.0}, {1.0, 0.0}, 1.0 },
              { {3.0, 3.0}, {3.0, 3.0}, 0.0 },
              { {0.0, 3.0}, {4.0, 0.0}, 5.0 }};

  gsize i;

  for (i = 0; i < G_N_ELEMENTS (data); ++i)
    {
      gdouble distance;

      distance = hyscan_cartesian_distance (&data[i].p1, &data[i].p2);
      g_assert_cmpfloat (ABS (data[i].distance - distance), <, 1e-6);
    }

  g_message ("Distance test done!");
}

void
test_distance_to_line (void)
{
  struct
  {
    HyScanGeoCartesian2D line1;
    HyScanGeoCartesian2D line2;
    HyScanGeoCartesian2D point;
    gdouble              distance;
    HyScanGeoCartesian2D nearest;
  } data[] = {{ {0.0, 0.0}, {1.0, 0.0}, {0.0, 2.0}, 2.0,         {0.0, 0.0} },
              { {3.0, 3.0}, {1.0, 1.0}, {0.0, 0.0}, 0.0,         {0.0, 0.0} },
              { {3.0, 3.0}, {1.0, 1.0}, {0.0, 1.0}, 0.707106781, {0.5, 0.5} }};

  gsize i;

  for (i = 0; i < G_N_ELEMENTS (data); ++i)
    {
      HyScanGeoCartesian2D nearest;
      gdouble distance;

      distance = hyscan_cartesian_distance_to_line (&data[i].line1, &data[i].line2, &data[i].point, &nearest);
      g_assert_cmpfloat (ABS (data[i].distance - distance), <, 1e-6);
      g_assert_cmpfloat (ABS (data[i].nearest.x - nearest.x), <, 1e-6);
      g_assert_cmpfloat (ABS (data[i].nearest.y - nearest.y), <, 1e-6);
    }

  g_message ("Distance test done!");
}

void
test_rotate (void)
{
 struct
  {
    HyScanGeoCartesian2D point;
    HyScanGeoCartesian2D center;
    gdouble              angle;
    HyScanGeoCartesian2D rotated;
  } data[] = {{ {1.0, 0.0}, {0.0, 0.0}, G_PI / 2.0, { 0.0,   1.0} },
              { {3.0, 3.0}, {3.0, 3.0}, G_PI / 1.2, { 3.0,   3.0} },
              { {3.0, 3.0}, {0.0, 0.0}, G_PI,       { -3.0, -3.0} },
              { {3.0, 3.0}, {1.0, 0.0}, G_PI,       { -1.0, -3.0} }};

  gsize i;

  for (i = 0; i < G_N_ELEMENTS (data); ++i)
    {
      HyScanGeoCartesian2D rotated;

      hyscan_cartesian_rotate (&data[i].point, &data[i].center, data[i].angle, &rotated);
      g_assert_cmpfloat (ABS (data[i].rotated.x - rotated.x), <, 1e-6);
      g_assert_cmpfloat (ABS (data[i].rotated.y - rotated.y), <, 1e-6);
    }

  g_message ("Rotate test done!");
}

void
test_rotate_area (void)
{
 struct
  {
    HyScanGeoCartesian2D area_from;
    HyScanGeoCartesian2D area_to;
    HyScanGeoCartesian2D center;
    gdouble              angle;
    HyScanGeoCartesian2D rotated_from;
    HyScanGeoCartesian2D rotated_to;
  } data[] = {{ {1.0, 1.0}, {0.0, 0.0}, {1.0, 1.0},  G_PI / 2.0, {1.0, 0.0}, {2.0, 1.0} },
              { {3.0, 3.0}, {2.0, 1.0}, {2.0, 2.0}, -G_PI / 2.0, {1.0, 1.0}, {3.0, 2.0} } };

  gsize i;

  for (i = 0; i < G_N_ELEMENTS (data); ++i)
    {
      HyScanGeoCartesian2D rotated_from;
      HyScanGeoCartesian2D rotated_to;

      hyscan_cartesian_rotate_area (&data[i].area_from, &data[i].area_to,
                                    &data[i].center, data[i].angle,
                                    &rotated_from, &rotated_to);
      g_assert_cmpfloat (ABS (data[i].rotated_from.x - rotated_from.x), <, 1e-6);
      g_assert_cmpfloat (ABS (data[i].rotated_from.y - rotated_from.y), <, 1e-6);
      g_assert_cmpfloat (ABS (data[i].rotated_to.x - rotated_to.x), <, 1e-6);
      g_assert_cmpfloat (ABS (data[i].rotated_to.y - rotated_to.y), <, 1e-6);
    }

  g_message ("Rotate test done!");
}

int
main (int    argc,
      char **argv)
{
  gsize i;

  HyScanGeoCartesian2D from = {.x = 0.0, .y = 0.0 };
  HyScanGeoCartesian2D   to = {.x = 1.0, .y = 1.0 };

  for (i = 0; i < G_N_ELEMENTS (points_inside); ++i)
    {
      g_assert_true (hyscan_cartesian_is_inside (&points_inside[i][0], &points_inside[i][1], &from, &to));
      g_assert_true (hyscan_cartesian_is_inside (&points_inside[i][1], &points_inside[i][0], &from, &to));
      g_assert_true (hyscan_cartesian_is_inside (&points_inside[i][0], &points_inside[i][1], &to, &from));
      g_assert_true (hyscan_cartesian_is_inside (&points_inside[i][1], &points_inside[i][0], &to, &from));
    }

  for (i = 0; i < G_N_ELEMENTS (points_outside); ++i)
    {
      g_assert_false (hyscan_cartesian_is_inside (&points_outside[i][0], &points_outside[i][1], &from, &to));
      g_assert_false (hyscan_cartesian_is_inside (&points_outside[i][1], &points_outside[i][0], &from, &to));
      g_assert_false (hyscan_cartesian_is_inside (&points_outside[i][0], &points_outside[i][1], &to, &from));
      g_assert_false (hyscan_cartesian_is_inside (&points_outside[i][1], &points_outside[i][0], &to, &from));
    }

  test_distance ();
  test_distance_to_line ();
  test_rotate ();
  test_rotate_area ();

  g_message ("Test done successfully");

  return 0;
}
