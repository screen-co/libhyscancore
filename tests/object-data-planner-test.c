/* object-data-planner-test.c
 *
 * Copyright 2020 Screen LLC, Alexey Sakhnov <alexsakhnov@gmail.com>
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

#include <hyscan-data-writer.h>
#include <hyscan-object-data-planner.h>
#include <math.h>

#define PROJECT_NAME  "test"      /* Имя проекта. */
#define TRACK_NAME    "test"      /* Имя галса. */

static gboolean
geo_point_equal (HyScanGeoPoint *a,
                 HyScanGeoPoint *b)
{
  return fabs (a->lat - b->lat) < 1e-9 && fabs (a->lon - b->lon) < 1e-9;
}

static gboolean
tracks_equal (HyScanPlannerTrack *a,
              HyScanPlannerTrack *b)
{
  gint i;

  if (g_strcmp0 (a->zone_id, b->zone_id) != 0 ||
      g_strcmp0 (a->name, b->name) != 0 ||
      a->number != b->number ||
      fabs (a->plan.speed - b->plan.speed) > 1e-5 ||
      !geo_point_equal (&a->plan.start, &b->plan.start) ||
      !geo_point_equal (&a->plan.end, &b->plan.end))
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

static gboolean
zones_equal (HyScanPlannerZone *a,
             HyScanPlannerZone *b)
{
  guint i;
  
  if (a->type != b->type ||
      a->ctime != b->ctime ||
      a->mtime != b->mtime ||
      a->points_len != b->points_len ||
      g_strcmp0 (a->name, b->name) != 0)
    {
      return FALSE;
    }
    
  for (i = 0; i < a->points_len; i++)
    {
      if (!geo_point_equal (&a->points[i], &b->points[i]))
        return FALSE;
    }

  return TRUE;
}

/* Общий тест на работу класса HyScanObjectDataPlanner. */
static void
test_objects (HyScanObjectStore *data,
              HyScanObject *object0,
              HyScanObject *object1,
              GEqualFunc    equal_func)
{
  HyScanObject *db_object[2];
  gchar *db_id[2];
  GList *ids, *link;
  gboolean found0, found1;
  gboolean status;
  gint i;

  if (equal_func (object0, object1))
    g_error ("Objects must differ");

  /* Проверяем добавление объектов в БД. */
  status = hyscan_object_store_add (data, (HyScanObject *) object0, &db_id[0]) &&
           hyscan_object_store_add (data, (HyScanObject *) object1, &db_id[1]);

  if (!status)
    g_error ("Failed to add object");

  ids = hyscan_object_store_get_ids (data);
  found0 = found1 = FALSE;
  for (link = ids; link != NULL; link = link->next)
    {
      HyScanObjectId *id = link->data;

      if (g_strcmp0 (id->id, db_id[0]) == 0)
        found0 = TRUE;
      if (g_strcmp0 (id->id, db_id[1]) == 0)
        found1 = TRUE;
    }
  if (!found0 || !found1 || g_list_length (ids) != 2)
    {
      g_error ("Ids are incorrect");
    }
  g_list_free_full (ids, (GDestroyNotify) hyscan_object_id_free);

  db_object[0] = hyscan_object_store_get (data, object0->type, db_id[0]);
  db_object[1] = hyscan_object_store_get (data, object1->type, db_id[1]);
  if (!equal_func (db_object[0], object0) || !equal_func (db_object[1], object1))
    g_error ("Db objects differ from original objects");

  hyscan_object_free (db_object[0]);
  hyscan_object_free (db_object[1]);

  /* Проверяем изменение объекта в БД. */
  if (!hyscan_object_store_modify (data, db_id[0], object1))
    g_error ("Failed to modify object");

  db_object[0] = hyscan_object_store_get (data, object0->type, db_id[0]);
  if (!equal_func (db_object[0], object1))
    g_error ("Object has not been modified");
  hyscan_object_free (db_object[0]);

  /* Проверяем удаление объектов в БД. */
  for (i = 1; i >= 0; i--)
    {
      if (!hyscan_object_store_remove (data, object0->type, db_id[i]))
        g_error ("Failed to remove object");

      ids = hyscan_object_store_get_ids (data);
      if ((gint) g_list_length (ids) != i)
        g_error ("Object has not been removed");
      g_list_free_full (ids, (GDestroyNotify) hyscan_object_id_free);
    }

  g_free (db_id[0]);
  g_free (db_id[1]);
}

static void
test_zones (HyScanObjectStore *data)
{
  HyScanPlannerZone *zone2, *zone1;
  gint i;

  zone1 = hyscan_planner_zone_new ();
  zone1->ctime = 123;
  zone1->mtime = 345;
  zone1->name = g_strdup ("Zone 1");

  zone2 = (HyScanPlannerZone *) hyscan_object_copy ((HyScanObject *) zone1);
  if (!zones_equal (zone2, zone1))
    g_error ("Zone copy is not equal to the source");
  
  for (i = 0; i < 10; i++)
    {
      HyScanGeoPoint point = {i, i + 1};
      hyscan_planner_zone_vertex_append (zone2, point);
    }
  if (zone2->points_len != 10)
    g_error ("Failed to add vertex to zone");
  
  hyscan_planner_zone_vertex_dup (zone2, 1);
  if (!geo_point_equal (&zone2->points[1], &zone2->points[2]) || zone2->points_len != 11)
    g_error ("Failed to duplicate vertex");
  
  hyscan_planner_zone_vertex_remove (zone2, 1);
  if (zone2->points_len != 10)
    g_error ("Failed to remove vertex");

  test_objects (data, (HyScanObject *) zone1, (HyScanObject *) zone2, (GEqualFunc) zones_equal);

  hyscan_planner_zone_free (zone1);
  hyscan_planner_zone_free (zone2);
}

static void
test_tracks (HyScanObjectStore *data)
{
  HyScanPlannerTrack *track1, *track2;
  HyScanTrackPlan plan = { .start = {1, 2}, .end = {3, 4}, .speed = 5};

  track1 = hyscan_planner_track_new ();
  track1->number = 1;
  track1->plan = plan;
  track1->name = g_strdup ("Track 1");
  track1->zone_id = g_strdup ("abcdef");

  track2 = (HyScanPlannerTrack *) hyscan_object_copy ((HyScanObject *) track1);
  if (!tracks_equal (track2, track1))
    g_error ("Track copy is not equal to the source");

  hyscan_planner_track_record_append (track2, "track_id_1");
  hyscan_planner_track_record_append (track2, "track_id_2");
  hyscan_planner_track_record_append (track2, "track_id_3");
  if (g_strv_length (track2->records) != 3)
    g_error ("Failed to add records to track plan");

  hyscan_planner_track_record_delete (track2, "track_id_another");
  hyscan_planner_track_record_delete (track2, "track_id_1");
  if (g_strv_length (track2->records) != 2)
    g_error ("Failed to remove records from track plan");

  test_objects (data, (HyScanObject *) track1, (HyScanObject *) track2, (GEqualFunc) tracks_equal);

  hyscan_planner_track_free (track1);
  hyscan_planner_track_free (track2);
}

gboolean
make_track (HyScanDB *db)
{
  HyScanAcousticDataInfo info = {.data_type = HYSCAN_DATA_FLOAT, .data_rate = 1.0};
  HyScanDataWriter *writer = hyscan_data_writer_new ();
  HyScanBuffer *buffer = hyscan_buffer_new ();
  gint i;

  hyscan_data_writer_set_db (writer, db);
  if (!hyscan_data_writer_start (writer, PROJECT_NAME, TRACK_NAME, HYSCAN_TRACK_SURVEY, NULL, -1))
    g_error ("Couldn't start data writer.");

  /* Запишем что-то в галс. */
  for (i = 0; i < 2; i++)
    {
      gfloat vals = {0};
      hyscan_buffer_wrap_float (buffer, &vals, 1);
      hyscan_data_writer_acoustic_add_data (writer, HYSCAN_SOURCE_SIDE_SCAN_PORT,
                                            1, FALSE, 1 + i, &info, buffer);
    }

  g_object_unref (writer);
  g_object_unref (buffer);
  return TRUE;
}

int
main (int argc, char **argv)
{
  HyScanObjectData *data;
  HyScanDB *db;
  gchar *db_uri;

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
        return 0;
      }

    g_option_context_free (context);

    db_uri = g_strdup (args[1]);
    g_strfreev (args);
  }

  /* Откроем БД, создадим проект и галс. */
  db = hyscan_db_new (db_uri);
  if (db == NULL)
    g_error ("Can't open db at %s", db_uri);

  make_track (db);

  data = hyscan_object_data_planner_new ();
  hyscan_object_data_project_open (data, db, PROJECT_NAME);

  test_zones (HYSCAN_OBJECT_STORE (data));
  test_tracks (HYSCAN_OBJECT_STORE (data));

  hyscan_db_project_remove (db, PROJECT_NAME);

  g_free (db_uri);
  g_clear_object (&db);
  g_clear_object (&data);

  g_message ("Test passed!");
  return 0;
}
