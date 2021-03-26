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
#include <hyscan-object-data-planner.h>
#include <math.h>
#include <gio/gio.h>

#define PROJECT_NAME "planner-project"
#define ASSERT_POINTS_EQUAL(a,b)  G_STMT_START { \
                                    HyScanGeoPoint *x = &(a), *y = &(b);  \
                                    g_assert (ABS (x->lat - y->lat) < 1e-6); \
                                    g_assert (ABS (x->lon - y->lon) < 1e-6); \
                                  } G_STMT_END

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

  g_bytes_unref (project_schema);
  g_date_time_unref (date_time);
  hyscan_db_close (db, project_id);

  return project_name;
}

static HyScanGeoPoint *
create_points_array (gsize points_len)
{
  HyScanGeoPoint *points;
  gsize i;

  points = g_new0 (HyScanGeoPoint, points_len);

  for (i = 0; i < points_len; ++i)
    {
      HyScanGeoPoint *coord = &points[i];

      coord->lat = 55.5 + 0.01 * sin (2.0 * G_PI * i / points_len);
      coord->lon = 38.2 + 0.01 * cos (2.0 * G_PI * i / points_len);
    }
  
  return points;
}

void
test_zones (HyScanDB *db,
            gchar    *project_name)
{
  HyScanObjectData *planner;
  gchar *zone_id;
  GList *zones;
  HyScanGeoPoint *points;
  HyScanPlannerZone *zone;
  HyScanPlannerZone new_zone;

  gboolean status;

  gsize points_len = 10;
  gsize i;

  planner = hyscan_object_data_planner_new ();
  if (!hyscan_object_data_project_open (planner, db, project_name))
    g_error ("Failed to open project");

  /* Добавляем зону. */
  new_zone.type = HYSCAN_TYPE_PLANNER_ZONE;
  new_zone.points_len = 0;
  new_zone.points = NULL;
  new_zone.name = "Zone 1";
  new_zone.ctime = 1234;
  new_zone.mtime = 1234;
  status = hyscan_object_store_add (HYSCAN_OBJECT_STORE (planner), (HyScanObject *) &new_zone, &zone_id);
  g_assert (status != FALSE);

  /* Проверяем, что зона добавлена. */
  zones = hyscan_object_store_get_ids (HYSCAN_OBJECT_STORE (planner));
  g_assert_cmpint (g_list_length (zones), ==, 1);
  g_assert_cmpstr (zone_id, ==, ((HyScanObjectId *) g_list_nth_data (zones, 0))->id);
  g_list_free_full (zones, (GDestroyNotify) hyscan_object_id_free);

  zone = (HyScanPlannerZone *) hyscan_object_store_get (HYSCAN_OBJECT_STORE (planner), HYSCAN_TYPE_PLANNER_ZONE, zone_id);
  g_assert (HYSCAN_IS_PLANNER_ZONE (zone));
  g_assert_cmpstr (zone->name, ==, new_zone.name);
  g_assert_cmpint (zone->ctime, ==, new_zone.ctime);
  g_assert_cmpint (zone->mtime, ==, new_zone.mtime);
  g_assert_cmpint (zone->points_len, ==, new_zone.points_len);
  g_assert (zone->points == NULL);

  /* Устанавливаем границу зоны. */
  points = create_points_array (points_len);
  zone->points = create_points_array (points_len);
  zone->points_len = points_len;
  hyscan_object_store_modify (HYSCAN_OBJECT_STORE (planner), zone_id, (const HyScanObject *) zone);
  hyscan_planner_zone_free (zone);

  /* Проверяем, что границы установились. */
  zone = (HyScanPlannerZone *) hyscan_object_store_get (HYSCAN_OBJECT_STORE (planner), HYSCAN_TYPE_PLANNER_ZONE, zone_id);
  g_assert (zone != NULL);
  g_assert_cmpint (zone->points_len, ==, points_len);
  for (i = 0; i < zone->points_len; ++i)
    ASSERT_POINTS_EQUAL (zone->points[i], points[i]);

  /* Дублируем и удаляем вершины. */
  hyscan_planner_zone_vertex_dup (zone, 1);
  g_assert_cmpint (zone->points_len, ==, points_len + 1);
  ASSERT_POINTS_EQUAL (zone->points[1], zone->points[2]);

  hyscan_planner_zone_vertex_remove (zone, 2);
  g_assert_cmpint (zone->points_len, ==, points_len);

  hyscan_planner_zone_free (zone);

  /* Проверяем несуществующие зоны. */
  zone = (HyScanPlannerZone *) hyscan_object_store_get (HYSCAN_OBJECT_STORE (planner), HYSCAN_TYPE_PLANNER_ZONE, "zone-nonexistent_id");
  g_assert (zone == NULL);

  /* Удаляем зону. */
  hyscan_object_store_remove (HYSCAN_OBJECT_STORE (planner), HYSCAN_TYPE_PLANNER_ZONE, zone_id);
  zones = hyscan_object_store_get_ids (HYSCAN_OBJECT_STORE (planner));
  g_assert (zones == NULL);

  g_free (zone_id);
  g_free (points);
  g_object_unref (planner);
}

void
test_tracks (HyScanDB *db,
             gchar    *project_name)
{
  HyScanObjectData *planner;
  gboolean status;
  gchar *track_id = NULL;
  gchar *zone_id = NULL;
  GList *tracks;
  HyScanPlannerTrack *track_obj;
  HyScanPlannerTrack track_new = {.type = HYSCAN_TYPE_PLANNER_TRACK};
  HyScanPlannerZone zone = {.type = HYSCAN_TYPE_PLANNER_ZONE};

  planner = hyscan_object_data_planner_new ();
  if (!hyscan_object_data_project_open (planner, db, project_name))
    g_error ("Failed to open project");

  zone.name = "Тест";
  zone.points = NULL;
  zone.points_len = 0;
  zone.mtime = 0;
  zone.ctime = 0;
  status = hyscan_object_store_add (HYSCAN_OBJECT_STORE (planner), (HyScanObject *) &zone, &zone_id);
  g_assert_true (status);

  /* Добавляем плановый галс. */
  track_new.plan.start.lat = 55.312;
  track_new.plan.start.lon = 38.452;
  track_new.plan.end.lat = 55.313;
  track_new.plan.end.lon = 38.453;
  track_new.plan.speed = 1.3;
  track_new.name = "Track 1";
  track_new.number = 0;
  track_new.zone_id = zone_id;
  status = hyscan_object_store_add (HYSCAN_OBJECT_STORE (planner), (HyScanObject *) &track_new, &track_id);
  g_assert_true (status);
  g_assert (track_id != NULL);

#define nth_id(list, n) (((HyScanObjectId *) g_list_nth_data ((list), (n)))->id)

  /* Проверяем, что галс добавлен. */
  tracks = hyscan_object_store_get_ids (HYSCAN_OBJECT_STORE (planner));
  g_assert_cmpint (g_list_length (tracks), ==, 2);
  g_assert ((g_str_equal (track_id, nth_id (tracks, 0)) && g_str_equal (zone_id, nth_id (tracks, 1))) ||
            (g_str_equal (track_id, nth_id (tracks, 1)) && g_str_equal (zone_id, nth_id (tracks, 0))));
  g_list_free_full (tracks, (GDestroyNotify) hyscan_object_id_free);

  /* Меняем параметры галса. */
  track_obj = (HyScanPlannerTrack *) hyscan_object_store_get (HYSCAN_OBJECT_STORE (planner),
                                                              HYSCAN_TYPE_PLANNER_TRACK,
                                                              track_id);
  g_assert (track_obj != NULL);
  track_obj->plan.speed = 1.0;
  hyscan_planner_track_record_append (track_obj, "rec1");
  hyscan_planner_track_record_append (track_obj, "rec2");
  hyscan_planner_track_record_append (track_obj, "rec3");
  hyscan_planner_track_record_delete (track_obj, "rec3");
  hyscan_object_store_modify (HYSCAN_OBJECT_STORE (planner), track_id, (const HyScanObject *) track_obj);
  hyscan_planner_track_free (track_obj);

  /* Проверяем, что параметры обновились. */
  track_obj = (HyScanPlannerTrack *) hyscan_object_store_get (HYSCAN_OBJECT_STORE (planner),
                                                              HYSCAN_TYPE_PLANNER_TRACK,
                                                              track_id);
  g_assert (track_obj != NULL);
  g_assert_cmpfloat (ABS (track_obj->plan.speed - 1.0), <, 1e-4);
  g_assert_cmpint (g_strv_length (track_obj->records), ==, 2);
  g_assert (g_strv_contains ((const gchar * const *) track_obj->records, "rec1"));
  g_assert (g_strv_contains ((const gchar * const *) track_obj->records, "rec2"));
  g_assert (!g_strv_contains ((const gchar * const *) track_obj->records, "rec3"));
  hyscan_planner_track_free (track_obj);

  /* Проверяем несуществующие галсы. */
  track_obj = (HyScanPlannerTrack *) hyscan_object_store_get (HYSCAN_OBJECT_STORE (planner),
                                                              HYSCAN_TYPE_PLANNER_TRACK,
                                                              "track-nonexistent_id");
  g_assert (track_obj == NULL);

  /* Удаляем галс. */
  status = hyscan_object_store_remove (HYSCAN_OBJECT_STORE (planner), HYSCAN_TYPE_PLANNER_TRACK, track_id) &&
           hyscan_object_store_remove (HYSCAN_OBJECT_STORE (planner), HYSCAN_TYPE_PLANNER_ZONE, zone_id);
  g_assert (status != FALSE);
  tracks = hyscan_object_store_get_ids (HYSCAN_OBJECT_STORE (planner));
  g_assert (tracks == NULL);

  g_free (track_id);
  g_free (zone_id);
  g_object_unref (planner);
}

void
test_origin (HyScanDB *db,
             gchar    *project_name)
{
  HyScanObjectData *planner;
  gboolean status;
  gchar *origin_id = NULL;
  HyScanPlannerOrigin *origin_obj;
  HyScanPlannerOrigin origin = {.type = HYSCAN_TYPE_PLANNER_ORIGIN};

  planner = hyscan_object_data_planner_new ();
  if (!hyscan_object_data_project_open (planner, db, project_name))
    g_error ("Failed to open project");

  /* Записываем положение точки отсчета. */
  origin.origin.lat = 50.0;
  origin.origin.lon = 40.0;
  status = hyscan_object_store_add (HYSCAN_OBJECT_STORE (planner), (HyScanObject *) &origin, &origin_id);
  g_assert_true (status);

  /* Меняем параметры ТО. */
  origin_obj = (HyScanPlannerOrigin *) hyscan_object_store_get (HYSCAN_OBJECT_STORE (planner),
                                                                HYSCAN_TYPE_PLANNER_ORIGIN,
                                                                origin_id);
  g_assert (origin_obj != NULL);
  g_assert_cmpint (origin.type, ==, HYSCAN_TYPE_PLANNER_ORIGIN);

  g_assert_cmpfloat (ABS (origin_obj->origin.lat - origin.origin.lat), <, 1e-6);
  g_assert_cmpfloat (ABS (origin_obj->origin.lon - origin.origin.lon), <, 1e-6);

  hyscan_object_store_modify (HYSCAN_OBJECT_STORE (planner), origin_id, (const HyScanObject *) origin_obj);
  hyscan_planner_origin_free (origin_obj);

  g_free (origin_id);
  g_object_unref (planner);
}

static void
test_extend (void)
{
  HyScanPlannerTrack track_in = {
    .type = HYSCAN_TYPE_PLANNER_TRACK,
    .plan = { .start = { 36.983408,  55.937443 }, .end = { 36.983409, 55.937443 }}
  };
  HyScanPlannerTrack track_out = {
    .type = HYSCAN_TYPE_PLANNER_TRACK,
    .plan = { .start = { 36.983401,  55.937440 }, .end = { 36.983409, 55.937440 }}
  };
  gdouble angle, angle_ext;
  HyScanPlannerTrack *track_ext, *track_ext2;
  HyScanPlannerZone zone = { .type = HYSCAN_TYPE_PLANNER_ZONE };
  HyScanGeoPoint points[] = {
    { 36.983409,  55.937442 },
    { 36.983408,  55.937442 },
    { 36.983407,  55.937442 },

    { 36.983407,  55.937443 },
    { 36.983407,  55.937444 },
    { 36.983407,  55.937445 },

    { 36.983409,  55.937445 },
  };

  zone.points = points;
  zone.points_len = G_N_ELEMENTS (points);

  track_ext = hyscan_planner_track_extend (&track_in, &zone);

  angle = hyscan_planner_track_angle (&track_in);
  angle_ext = hyscan_planner_track_angle (track_ext);
  g_assert_cmpfloat (ABS (angle - angle_ext), <, 1e-3);

  /* Повторное преоборазование не должно ничего поменять. */
  track_ext2 = hyscan_planner_track_extend (track_ext, &zone);
  ASSERT_POINTS_EQUAL (track_ext->plan.start, track_ext2->plan.start);
  ASSERT_POINTS_EQUAL (track_ext->plan.end, track_ext2->plan.end);

  hyscan_planner_track_free (track_ext);
  hyscan_planner_track_free (track_ext2);

  /* Галс за пределами зоны - никаких изменений не должно быть. */
  track_ext = hyscan_planner_track_extend (&track_out, &zone);
  ASSERT_POINTS_EQUAL (track_ext->plan.start, track_out.plan.start);
  ASSERT_POINTS_EQUAL (track_ext->plan.end, track_out.plan.end);
  hyscan_planner_track_free (track_ext);
}

static void
test_angle_length (void)
{
  struct
  {
    HyScanTrackPlan plan;
    gdouble         angle;
    gdouble         length;
  } data[] = {
    { {.start = {  36.983409,  55.937442 }, .end = { 36.983433,  55.937123 } }, 275.38,   28.44 },
    { {.start = {  36.983433,  55.937123 }, .end = { 36.983409,  55.937442 } },  95.38,   28.44 },
    { {.start = {  -8.807154,  13.235269 }, .end = { -8.797848,  13.227887 } }, 321.90, 1311.08 },
    { {.start = {  50.983433, -90.937123 }, .end = { 50.977422, -90.998773 } }, 261.22, 4380.84 },
  };

  guint i;

  for (i = 0; i < G_N_ELEMENTS (data); i++)
    {
      HyScanPlannerTrack track = { .type = HYSCAN_TYPE_PLANNER_TRACK };
      gdouble angle, length;
      HyScanGeo *geo;
      HyScanGeoCartesian2D start, end;

      track.plan = data[i].plan;

      angle = hyscan_planner_track_angle (&track);
      angle = (angle < 0.0 ? angle + 2 * G_PI : angle) / G_PI * 180.0;
      length = hyscan_planner_track_length (&track.plan);

      g_assert_cmpfloat (ABS (angle - data[i].angle), <, 1e-2);
      g_assert_cmpfloat (ABS (length - data[i].length), <, 1e-1);

      geo = hyscan_planner_track_geo (&track.plan, &angle);
      hyscan_geo_geo2topoXY0 (geo, &start, track.plan.start);
      hyscan_geo_geo2topoXY0 (geo, &end, track.plan.end);

      /* HyScanGeo переводит точку старта в (0, 0), движение вдоль оси X (y = 0). */
      g_assert_cmpfloat (ABS (0.0 - start.x), <, 1e-1);
      g_assert_cmpfloat (ABS (0.0 - start.y), <, 1e-1);
      g_assert_cmpfloat (ABS (0.0 - end.y), <, 1e-1);

      /* Поскольку HyScanGeo использует не сферический геоид,
       * то отличие в длине и азимуте будут более существенными. */
      g_assert_cmpfloat (ABS (length - end.x), <, 1e-2 * length);
      g_assert_cmpfloat (ABS (angle - data[i].angle), <, 5e-1);

      g_object_unref (geo);
    }
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

#ifdef G_OS_WIN32
    args = g_win32_get_command_line ();
#else
    args = g_strdupv (argv);
#endif

    context = g_option_context_new ("<db-uri>");
    g_option_context_set_help_enabled (context, TRUE);
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
  test_origin (db, project_name);
  test_angle_length ();
  test_extend ();

  /* Удаляем после себя проект. */
  hyscan_db_project_remove (db, project_name);

  g_print ("Test done successfully!\n");

  g_free (project_name);
  g_object_unref (db);
  g_free (db_uri);
  
  return 0;
}
