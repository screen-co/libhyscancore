#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <glib.h>
#include <gio/gio.h>
#include <string.h>
#include "hyscan-geo.h"

#define KYLW "\x1b[33;22m"
#define KGRN "\x1b[32;22m"
#define KRED "\x1b[31;22m"
#define KNRM "\x1b[0m"

/**
 * Тест класса HyScanGeo.
 * Состоит из двух частей:
 *  - тест перевода из топоцентрической в геодезическую и обратно;
 *  - тест перевода между геодезическими СК.
 */

int main (void)
{
  HyScanGeo *geo;

  /* */
  gint num_of_points = 16;
  gint i, j;
  HyScanGeoGeodetic buf1[16],
                    buf2[16],
                    bla0;
  HyScanGeoCartesian3D topo3d;
  HyScanGeoCartesian2D topo2d;
  HyScanGeoGeodetic input[16] =
                    {
                      {55.58555742, 38.42775731, 0.1},
                      {55.57618942, 38.97619825, 0.2},
                      {55.43911307, 38.25593806, 0.4},
                      {55.76511304, 38.34382719, 0.8},
                      {55.21229310, 38.78216305, 1.6},
                      {55.91902146, 38.29926900, 3.2},
                      {55.89934783, 38.43437577, 6.4},
                      {55.13953211, 38.53554243, 12.8},
                      {55.61453892, 38.09938242, 25.6},
                      {55.94318303, 38.92132794, 51.2},
                      {55.90070780, 38.02519500, 102.4},
                      {55.50752240, 38.84080550, 204.8},
                      {55.91287844, 38.59837877, 409.6},
                      {55.79220155, 38.40850472, 819.2},
                      {55.47685463, 38.99638715, 1638.4},
                      {55.53506921, 38.55914543, 3276.8}
                    };

  for (i = 0; i < num_of_points; i++)
    {
      buf1[i].lat = input[i].lat;
      buf1[i].lon = input[i].lon;
      buf1[i].h = input[i].h;
    }

  /* Проверяем перевод между различными СК. */
  for (i = 0; i < 100; i++)
    {
      for (j = 0; j < num_of_points; j++)
        {
          hyscan_geo_cs_transform (&buf2[j], buf1[j], HYSCAN_GEO_CS_WGS84, HYSCAN_GEO_CS_SK42);
          hyscan_geo_cs_transform (&buf1[j], buf2[j], HYSCAN_GEO_CS_SK42, HYSCAN_GEO_CS_SK95);
          hyscan_geo_cs_transform (&buf2[j], buf1[j], HYSCAN_GEO_CS_SK95, HYSCAN_GEO_CS_PZ90);
          hyscan_geo_cs_transform (&buf1[j], buf2[j], HYSCAN_GEO_CS_PZ90, HYSCAN_GEO_CS_PZ90_02);
          hyscan_geo_cs_transform (&buf2[j], buf1[j], HYSCAN_GEO_CS_PZ90_02, HYSCAN_GEO_CS_PZ90_11);
          hyscan_geo_cs_transform (&buf1[j], buf2[j], HYSCAN_GEO_CS_PZ90_11, HYSCAN_GEO_CS_WGS84);
        }
    }

  g_print ("Coordinate system transformation test\nWGS-84->SK-42->SK-95->PZ-90->PZ-90.02->PZ-90.11->WGS-84\n");
  g_print (KNRM "After 100 iterations we have the following values (" KGRN "in" KNRM "/" KYLW "out" KNRM "):\n");
  for (i = 0; i < num_of_points; i++)
    {
      g_print (KGRN "%i: %f %f %f\n" KNRM, i, input[i].lat, input[i].lon, input[i].h);
      g_print (KYLW "%i: %f %f %f\n" KNRM, i, buf1[i].lat, buf1[i].lon, buf1[i].h);
    }

  /* Очищаем буфер. */
  for (i = 0; i < num_of_points; i++)
    {
      buf1[i].lat = input[i].lat;
      buf1[i].lon = input[i].lon;
      buf1[i].h = input[i].h;
    }
  bla0.lat = 55.0;
  bla0.lon = 38.0;
  bla0.h = 0.0;

  geo = hyscan_geo_new (bla0, HYSCAN_GEO_ELLIPSOID_WGS84);

  /* Проверяем перевод в топоцентрическую и обратно. */
  g_print ("\n");
  g_print ("Geodetic to topocentric in 3 dimensions transformation test\nWGS-84->topocentric->WGS-84\n");

  for (i = 0; i < 100000; i++)
    {
      for (j = 0; j < num_of_points; j++)
        {
          hyscan_geo_geo2topo (geo, &topo3d, buf1[j]);
          hyscan_geo_topo2geo (geo, &buf1[j], topo3d);
        }
    }

  g_print (KNRM "After 100 000 iterations we have the following values (" KGRN "in" KNRM "/" KYLW "out" KNRM "):\n");
  for (i = 0; i < num_of_points; i++)
    {
      g_print (KGRN "%i: %f %f %f\n" KNRM, i, input[i].lat, input[i].lon, input[i].h);
      g_print (KYLW "%i: %f %f %f\n" KNRM, i, buf1[i].lat, buf1[i].lon, buf1[i].h);
    }

  /* Очищаем буфер. */
  for (i = 0; i < num_of_points; i++)
    {
      buf1[i].lat = input[i].lat;
      buf1[i].lon = input[i].lon;
      buf1[i].h = input[i].h;
    }

  /* Проверяем перевод в топоцентрическую двумерную и обратно. */
  g_print ("\n");
  g_print ("Geodetic to topocentric  in 2 dimensions transformation test\nWGS-84->topocentric->WGS-84\n");


  for (i = 0; i < 100000; i++)
    {
      for (j = 0; j < num_of_points; j++)
        {
          hyscan_geo_geo2topoXY (geo, &topo2d, buf1[j]);
          hyscan_geo_topoXY2geo (geo, &buf1[j], topo2d, buf1[j].h, 2);
        }
    }

  g_print (KNRM "After 100 000 iterations we have the following values (" KGRN "in" KNRM "/" KYLW "out" KNRM "):\n");
  for (i = 0; i < num_of_points; i++)
    {
      g_print (KGRN "%i: %f %f %f\n" KNRM, i, input[i].lat, input[i].lon, input[i].h);
      g_print (KYLW "%i: %f %f %f\n" KNRM, i, buf1[i].lat, buf1[i].lon, buf1[i].h);
    }

  g_object_unref (geo);

  return 0;
}
