/* mercator-test.c
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

#include <hyscan-proj.h>
#include <math.h>

#define RADIUS_EARTH 6378137.0

typedef struct {
  HyScanGeoPoint geo;
  gdouble        x;
  gdouble        y;
} TestData;

void
test_hash (void)
{
  HyScanGeoEllipsoidParam p;
  HyScanGeoProjection *proj_wgs84_1, *proj_wgs84_2, *proj_sphere_1, *proj_sphere_2;
  HyScanGeoProjection *proj_pseudo_1, *proj_pseudo_2;

  hyscan_geo_init_ellipsoid (&p, HYSCAN_GEO_ELLIPSOID_WGS84);
  proj_wgs84_1 = HYSCAN_GEO_PROJECTION (hyscan_proj_new (HYSCAN_PROJ_MERC));
  proj_wgs84_2 = HYSCAN_GEO_PROJECTION (hyscan_proj_new (HYSCAN_PROJ_MERC));

  hyscan_geo_init_ellipsoid_user (&p, RADIUS_EARTH, 0.0);
  proj_sphere_1 = HYSCAN_GEO_PROJECTION (hyscan_proj_new (HYSCAN_PROJ_MERC " +ellps=sphere"));
  proj_sphere_2 = HYSCAN_GEO_PROJECTION (hyscan_proj_new (HYSCAN_PROJ_MERC " +ellps=sphere"));

  proj_pseudo_1 = hyscan_proj_new (HYSCAN_PROJ_WEBMERC);
  proj_pseudo_2 = hyscan_proj_new (HYSCAN_PROJ_WEBMERC);

  g_assert_cmpint (hyscan_geo_projection_hash (proj_wgs84_1), ==, hyscan_geo_projection_hash (proj_wgs84_2));
  g_assert_cmpint (hyscan_geo_projection_hash (proj_sphere_1), ==, hyscan_geo_projection_hash (proj_sphere_2));
  g_assert_cmpint (hyscan_geo_projection_hash (proj_pseudo_1), ==, hyscan_geo_projection_hash (proj_pseudo_2));
  g_assert_cmpint (hyscan_geo_projection_hash (proj_wgs84_1), !=, hyscan_geo_projection_hash (proj_sphere_2));
  g_assert_cmpint (hyscan_geo_projection_hash (proj_pseudo_1), !=, hyscan_geo_projection_hash (proj_sphere_2));

  g_object_unref (proj_wgs84_1);
  g_object_unref (proj_wgs84_2);
  g_object_unref (proj_sphere_1);
  g_object_unref (proj_sphere_2);
  g_object_unref (proj_pseudo_1);
  g_object_unref (proj_pseudo_2);
}

void
test_projection (HyScanGeoProjection *projection,
                 TestData            *data,
                 guint                n_elements,
                 gdouble              eps)
{
  guint i;

  /* Проверяем перевод точек из гео-СК в декартову и обратно. */
  for (i = 0; i < n_elements; ++i)
    {
      HyScanGeoPoint coord;
      HyScanGeoCartesian2D c2d;
      gdouble lat_err, lon_err;

      /* Переводим из гео в проекцию. */
      hyscan_geo_projection_geo_to_value (projection, data[i].geo, &c2d);
      g_message ("Projection coordinates: %f, %f", c2d.x, c2d.y);

      g_assert_cmpfloat (fabs (c2d.x - data[i].x) + fabs (c2d.y - data[i].y), <, eps);

      /* Переводим из проекции в гео. */
      hyscan_geo_projection_value_to_geo (projection, &coord, c2d.x, c2d.y);
      g_message ("Geo coordinates: %f, %f", coord.lat, coord.lon);

      lat_err = fabs (coord.lat - data[i].geo.lat);
      lon_err = fabs (coord.lon - data[i].geo.lon);

      g_message ("Error lat: %.2e, lon: %.2e", lat_err, lon_err);

      g_assert_true (lat_err < 1e-6);
      g_assert_true (lon_err < 1e-6);
    }

  /* Проверяем границы проекции. */
  {
    gdouble min_x, max_x, min_y, max_y;

    hyscan_geo_projection_get_limits (projection, &min_x, &max_x, &min_y, &max_y);
    g_assert_cmpfloat (min_x, <, max_x);
    g_assert_cmpfloat (min_y, <, max_y);
    g_assert_cmpfloat (max_x - min_x, ==, max_y - min_y);
  }
}

void
test_pseudo_mercator_scale (HyScanGeoProjection *projection)
{
  HyScanGeoPoint coords;
  gdouble scale0, scale20, scale40, scale40_1;

  coords.lon = 80;
  coords.lat = 0;
  scale0 = hyscan_geo_projection_get_scale (projection, coords);
  coords.lat = 20;
  scale20 = hyscan_geo_projection_get_scale (projection, coords);
  coords.lat = 40;
  scale40 = hyscan_geo_projection_get_scale (projection, coords);
  coords.lon = 90;
  scale40_1 = hyscan_geo_projection_get_scale (projection, coords);

  /* Масштаб уменьшается при приближении к полюсам. */
  g_assert_cmpfloat (scale0, >, scale20);
  g_assert_cmpfloat (scale20, >, scale40);
  /* При этом на одной параллели масштаб постоянный. */
  g_assert_cmpfloat (scale40_1, ==, scale40);
}

void
test_mercator_scale (HyScanGeoProjection *projection)
{
  HyScanGeoPoint coords;
  gdouble scale0;

  coords.lon = 80;
  coords.lat = 0;
  scale0 = hyscan_geo_projection_get_scale (projection, coords);

  /* Масштаб равен 1 на экваторе... */
  g_assert_cmpfloat (ABS (1.0 - scale0), <, 0.01);

  /* В остальном ведёт себя так же, как псевдомеркатор. */
  test_pseudo_mercator_scale (projection);
}

int main (int     argc,
          gchar **argv)
{
  HyScanGeoProjection *projection;

  TestData data_sphere[] = {
    { {.lat = 52.36, .lon = 4.9}, .x = 545465.50, .y = 6865481.66},
    { {.lat = 55.75, .lon = 37.61}, .x = 4186726.05, .y = 7508807.85},
  };

  TestData data_spheroid[] = {
    { {.lat = 52.36, .lon = 4.9}, .x = 545465.50, .y = 6831623.50},
    { {.lat = 55.75, .lon = 37.61}, .x = 4186726.05, .y = 7473460.43},
  };

  g_message ("EPSG:3857: WGS84 pseudo-Mercator (sphere) [https://epsg.io/3857]");
  projection = hyscan_proj_new (HYSCAN_PROJ_WEBMERC);
  test_projection (projection, data_sphere, G_N_ELEMENTS (data_sphere), 1e-2);
  test_pseudo_mercator_scale (projection);
  g_object_unref (projection);

  g_message ("EPSG:3395: WGS84 Mercator projection (spheroid) [https://epsg.io/3395]");
  projection = hyscan_proj_new (HYSCAN_PROJ_MERC);
  test_projection (projection, data_spheroid, G_N_ELEMENTS (data_spheroid), 1e-2);
  test_mercator_scale (projection);
  g_object_unref (projection);

  test_hash ();

  g_message ("Tests done!");

  return 0;
}