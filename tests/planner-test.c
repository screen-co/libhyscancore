/* planner-test.c
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

#include <glib.h>
#include <hyscan-planner.h>
#include <math.h>
#include <gio/gio.h>

#define PROJECT_NAME "planner-project"

static gint n_zones;
static gint n_points;

static gchar *
create_project (HyScanDB *db)
{
  GBytes *project_schema;
  GDateTime *date_time;
  gint32 project_id;
  gchar *project_name;

  project_schema = g_resources_lookup_data ("/org/hyscan/schemas/project-schema.xml",
                                            G_RESOURCE_LOOKUP_FLAGS_NONE, NULL);
  g_assert (project_schema != NULL);
  
  /* Создаём проект. */
  date_time = g_date_time_new_now_local ();
  project_name = g_strdup_printf (PROJECT_NAME"-%ld", g_date_time_to_unix (date_time));
  project_id = hyscan_db_project_create (db, project_name, g_bytes_get_data (project_schema, NULL));
  g_assert (project_id > 0);
  
  hyscan_db_close (db, project_id);

  return project_name;
}

static HyScanGeoGeodetic *
create_points_array (gsize points_len)
{
  HyScanGeoGeodetic *points;
  gsize i;

  points = g_new0 (HyScanGeoGeodetic, points_len);

  for (i = 0; i < points_len; ++i)
    {
      HyScanGeoGeodetic *coord = &points[i];

      coord->lat = 55.5 + 0.01 * sin (2.0 * G_PI * i / points_len);
      coord->lon = 38.2 + 0.01 * cos (2.0 * G_PI * i / points_len);
    }
  
  return points;
}

void
test_zones (HyScanDB *db,
            gchar    *project_name)
{
  HyScanPlanner *planner;
  gchar *zone_id;
  gchar **zones;
  HyScanGeoGeodetic *points;
  HyScanPlannerZone *zone;
  HyScanPlannerZone new_zone;

  gsize points_len = 10;
  gsize i = 0;

  planner = hyscan_planner_new (db, project_name);

  /* Добавляем зону. */
  new_zone.points_len = 0;
  new_zone.points = NULL;
  new_zone.name = "Zone 1";
  new_zone.ctime = 1234;
  new_zone.mtime = new_zone.ctime;
  zone_id = hyscan_planner_zone_create (planner, &new_zone);
  g_assert (zone_id != NULL);

  /* Проверяем, что зона добавлена. */
  zones = hyscan_planner_zone_list (planner);
  g_assert_cmpint (g_strv_length (zones), ==, 1);
  g_assert (g_str_equal (zone_id, zones[0]));
  g_strfreev (zones);

  /* Устанавливаем границу зоны. */
  zone = hyscan_planner_zone_get (planner, zone_id);
  points = create_points_array (points_len);
  zone->points = create_points_array (points_len);
  zone->points_len = points_len;
  hyscan_planner_zone_set (planner, zone);
  hyscan_planner_zone_free (zone);

  /* Проверяем, что границы установились. */
  zone = hyscan_planner_zone_get (planner, zone_id);
  g_assert (zone != NULL);
  g_assert_cmpint (zone->points_len, ==, points_len);
  for (i = 0; i < zone->points_len; ++i)
    {
      g_assert (ABS (zone->points[i].lat - points[i].lat) < 1e-4);
      g_assert (ABS (zone->points[i].lon - points[i].lon) < 1e-4);
    }
  hyscan_planner_zone_free (zone);

  /* Проверяем несуществующие зоны. */
  zone = hyscan_planner_zone_get (planner, "nonexistent_id");
  g_assert (zone == NULL);

  /* Удаляем зону. */
  hyscan_planner_zone_remove (planner, zone_id);
  zones = hyscan_planner_zone_list (planner);
  g_assert (zones == NULL);

  g_free (points);
  g_object_unref (planner);
}

void
test_tracks (HyScanDB *db,
             gchar    *project_name)
{
  HyScanPlanner *planner;
  gboolean status;
  gchar *track_id;
  gchar **tracks;
  HyScanGeoGeodetic *points;
  HyScanPlannerTrack *track;
  HyScanPlannerTrack track_new;
  HyScanPlannerZone zone;

  gsize points_len = 10;

  planner = hyscan_planner_new (db, project_name);

  zone.name = "Тест";
  zone.points = NULL;
  zone.points_len = 0;
  zone.id = hyscan_planner_zone_create (planner, &zone);

  /* Добавляем плановый галс. */
  track_new.start.lat = 55.312;
  track_new.start.lon = 38.452;
  track_new.end.lat = 55.313;
  track_new.end.lon = 38.453;
  track_new.name = "Track 1";
  track_new.number = 0;
  track_new.speed = 1.3;
  track_new.zone_id = zone.id;
  track_id = hyscan_planner_track_create (planner, &track_new);
  g_assert (track_id != NULL);

  /* Проверяем, что галс добавлен. */
  tracks = hyscan_planner_track_list (planner, zone.id);
  g_assert_cmpint (g_strv_length (tracks), ==, 1);
  g_assert (g_str_equal (track_id, tracks[0]));
  g_strfreev (tracks);

  /* Меняем параметры галса. */
  track = hyscan_planner_track_get (planner, track_id);
  g_assert (track != NULL);
  points = create_points_array (points_len);
  track->speed = 1.0;
  hyscan_planner_track_set (planner, track);
  hyscan_planner_track_free (track);

  /* Проверяем, что параметры обновились. */
  track = hyscan_planner_track_get (planner, track_id);
  g_assert (track != NULL);
  g_assert (ABS (track->speed - 1.0) < 1e-4);
  hyscan_planner_track_free (track);

  /* Проверяем несуществующие галсы. */
  track = hyscan_planner_track_get (planner, "nonexistent_id");
  g_assert (track == NULL);

  /* Удаляем галс. */
  status = hyscan_planner_track_remove (planner, track_id);
  g_assert (status != FALSE);
  tracks = hyscan_planner_track_list (planner, zone.id);
  g_assert (tracks == NULL);

  g_free (zone.id);
  g_free (points);
  g_object_unref (planner);
}

int
main (int    argc,
      char **argv)
{
  gchar *db_uri;
  HyScanDB *db;
  gchar *project_name;

  {
    gchar **args;
    GError *error = NULL;
    GOptionContext *context;
    GOptionEntry entries[] =
      {
        { "zones", 'z', 0, G_OPTION_ARG_INT, &n_zones, "Number of zones", NULL },
        { "points", 'n', 0, G_OPTION_ARG_INT, &n_points, "Number of points per zone", NULL },
        { NULL }
      };

#ifdef G_OS_WIN32
    args = g_win32_get_command_line ();
#else
    args = g_strdupv (argv);
#endif

    context = g_option_context_new ("<db-uri>");
    g_option_context_set_help_enabled (context, TRUE);
    g_option_context_add_main_entries (context, entries, NULL);
    g_option_context_set_ignore_unknown_options (context, FALSE);
    if (!g_option_context_parse_strv (context, &args, &error))
      {
        g_print ("%s\n", error->message);
        return -1;
      }

    if (g_strv_length (args) != 2)
      {
        g_print ("%s", g_option_context_get_help (context, FALSE, NULL));
        return -1;
      }

    g_option_context_free (context);

    db_uri = g_strdup (args[1]);
    g_strfreev (args);
  }

  /* Открываем базу данных. */
  db = hyscan_db_new (db_uri);
  if (db == NULL)
    g_error ("Can't open db: %s", db_uri);

  /* Проводим тесты. */
  project_name = create_project (db);
  test_zones (db, project_name);
  test_tracks (db, project_name);

  /* Удаляем после себя проект. */
  hyscan_db_project_remove (db, project_name);

  g_print ("Test done successfully!\n");
  
  g_object_unref (db);
  
  return 0;
}
