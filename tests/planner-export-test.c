#include <hyscan-planner-export.h>
#include <hyscan-object-data-planner.h>
#include <hyscan-data-writer.h>
#include <string.h>

#define PROJECT_NAME "planner-export-test"

static void
assert_points_equal (HyScanGeoPoint *point1,
                     HyScanGeoPoint *point2)
{
  g_assert_cmpfloat (ABS (point1->lat - point2->lat), <, 1e-6);
  g_assert_cmpfloat (ABS (point1->lon - point2->lon), <, 1e-6);
}

static void
assert_double_equal (gdouble value1,
                     gdouble value2)
{
  g_assert_cmpfloat (ABS (value1 - value2), <, 1e-6);
}

/* Проверяет, что в хэш таблицах объекты с одинаковыми параметрами. */
static void
compare_objects (GHashTable *table1,
                 GHashTable *table2)
{
  GHashTableIter iter;
  gchar *key;
  HyScanObject *value1, *value2;
  guint size1, size2;

  g_message ("Comparing hash tables...");

  size1 = g_hash_table_size (table1);
  size2 = g_hash_table_size (table2);

  g_message ("Size of table1 = %d, table2 = %d", size1, size2);
  g_assert_cmpint (size1, ==, size2);

  g_hash_table_iter_init (&iter, table1);
  while (g_hash_table_iter_next (&iter, (gpointer *) &key, (gpointer *) &value1))
    {
      value2 = g_hash_table_lookup (table2, key);
      
      g_message ("Processing id = \"%s\"", key);

      if (HYSCAN_IS_PLANNER_ORIGIN (value1))
        {
          HyScanPlannerOrigin *origin1, *origin2;

          g_message ("Compare origin properties");

          g_assert_true (HYSCAN_IS_PLANNER_ORIGIN (value2));
          origin1 = (HyScanPlannerOrigin *) value1;
          origin2 = (HyScanPlannerOrigin *) value2;
          assert_points_equal (&origin1->origin, &origin2->origin);
        }

      else if (HYSCAN_IS_PLANNER_TRACK (value1))
        {
          HyScanPlannerTrack *track1, *track2;

          g_message ("Compare track properties");

          g_assert_true (HYSCAN_IS_PLANNER_TRACK (value2));
          track1 = (HyScanPlannerTrack *) value1;
          track2 = (HyScanPlannerTrack *) value2;
          g_assert_cmpstr (track1->zone_id, ==, track2->zone_id);
          g_assert_cmpstr (track1->name, ==, track2->name);
          g_assert_cmpint (track1->number, ==, track2->number);
          assert_points_equal (&track1->plan.start, &track2->plan.start);
          assert_points_equal (&track1->plan.end, &track2->plan.end);
          assert_double_equal (track1->plan.speed, track2->plan.speed);
        }

      else if (HYSCAN_IS_PLANNER_ZONE (value1))
        {
          HyScanPlannerZone *zone1, *zone2;
          guint i;

          g_message ("Compare zone properties");

          g_assert_true (HYSCAN_IS_PLANNER_ZONE (value2));
          zone1 = (HyScanPlannerZone *) value1;
          zone2 = (HyScanPlannerZone *) value2;
          g_assert_cmpstr (zone1->name, ==, zone2->name);
          g_assert_cmpint (zone1->ctime, ==, zone2->ctime);
          g_assert_cmpint (zone1->mtime, ==, zone2->mtime);
          g_assert_cmpint (zone1->points_len, ==, zone2->points_len);
          for (i = 0; i < zone1->points_len; i++)
            assert_points_equal (&zone1->points[i], &zone2->points[i]);
        }

      else
        {
          g_error ("Unknown object");
        }
    }
}

static GHashTable *
create_ht (void)
{
  GHashTable *ht;

  ht = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) hyscan_object_free);

  return ht;
}

static GHashTable *
get_from_db (HyScanDB    *db,
             const gchar *project)
{

  GHashTable *objects;
  HyScanObjectData *data;
  gint i;
  gchar **ids;

  data = hyscan_object_data_planner_new (db, project);
  objects = create_ht ();

  ids = hyscan_object_data_get_ids (data, NULL);
  if (ids != NULL)
    {
      for (i = 0; ids[i] != NULL; i++)
        g_hash_table_insert (objects, g_strdup (ids[i]), hyscan_object_data_get (data, ids[i]));

      g_strfreev (ids);
    }

  g_object_unref (data);

  return objects;
}

/* Создает хэш-таблицу с объектами. */
static GHashTable *
generate_objects (void)
{
  GHashTable *objects;
  HyScanPlannerZone *zone_ptr, zone = {.type = HYSCAN_PLANNER_ZONE};
  HyScanPlannerTrack *track_ptr, track = {.type = HYSCAN_PLANNER_TRACK};
  HyScanPlannerOrigin *origin_ptr, origin = {.type = HYSCAN_PLANNER_ORIGIN};
  HyScanGeoPoint vertex[] = { {11, 12}, {13, 14}, {15, 16}, {17, 18} };
  guint i;

  objects = create_ht ();

  zone.name = "Полигон";
  zone.ctime = 123;
  zone.mtime = 456;
  zone_ptr = hyscan_planner_zone_copy (&zone);
  for (i = 0; i < G_N_ELEMENTS (vertex); i++)
    hyscan_planner_zone_vertex_append (zone_ptr, vertex[i]);
  g_hash_table_insert (objects, g_strdup ("1"), zone_ptr);

  track.name = "Галс";
  track.number = 1;
  track.zone_id = "1";
  track.plan.start.lat = 11;
  track.plan.start.lon = 12;
  track.plan.end.lat = 11;
  track.plan.end.lon = 12;
  track.plan.speed = 2.0;
  track_ptr = hyscan_planner_track_copy (&track);
  g_hash_table_insert (objects, g_strdup ("2"), track_ptr);

  origin.origin.lat = 22;
  origin.origin.lon = 23;
  origin.ox = 24;
  origin_ptr = hyscan_planner_origin_copy (&origin);
  g_hash_table_insert (objects, g_strdup (HYSCAN_PLANNER_ORIGIN_ID), origin_ptr);

  return objects;
}

/* Функция создаёт проект. */
static gchar *
create_project (HyScanDB *db)
{
  GDateTime *date_time;
  gchar *project;
  HyScanDataWriter *writer;

  /* Создаём проект. */
  date_time = g_date_time_new_now_local ();
  project = g_strdup_printf (PROJECT_NAME"-%ld", g_date_time_to_unix (date_time));

  writer = hyscan_data_writer_new ();
  hyscan_data_writer_set_db (writer, db);
  hyscan_data_writer_start (writer, project, project, HYSCAN_TRACK_SURVEY, NULL, -1);
  hyscan_data_writer_stop (writer);

  g_object_unref (writer);
  g_date_time_unref (date_time);

  return project;
}

static void
test_import (const gchar *db_uri,
             GHashTable  *objects)
{
  HyScanDB *db;
  gchar *project;
  GHashTable *db_objects;

  db = hyscan_db_new (db_uri);
  project = create_project (db);

  g_message ("Created project: %s/%s", db_uri, project);

  db_objects = get_from_db (db, project);
  g_message ("Number of objects in db before import: %d", g_hash_table_size (db_objects));
  g_assert_cmpint (g_hash_table_size (db_objects), ==, 0);
  g_hash_table_unref (db_objects);

  g_message ("Importing tracks in db...");
  g_assert_true (hyscan_planner_import_to_db (db, project, objects, FALSE));

  db_objects = get_from_db (db, project);
  g_message ("Number of objects in db after import: %d", g_hash_table_size (db_objects));
  g_assert_cmpint (g_hash_table_size (db_objects), ==, g_hash_table_size (objects));
  g_hash_table_unref (db_objects);

  hyscan_db_project_remove (db, project);

  g_object_unref (db);
  g_free (project);
}

int
main (int argc,
      char **argv)
{
  gchar *filename = NULL;
  gchar *read_uri = NULL;
  gchar *read_project = NULL;
  gchar *import_uri = NULL;

  GHashTable *exported_ht, *imported_ht;
  gsize length;
  gchar *content;
  guint exported_size;

  /* Парсим аргументы. */
  {
    gchar **args;
    GError *error = NULL;
    GOptionContext *context;
    GOptionEntry entries[] = {
      {"export-db",       's', 0, G_OPTION_ARG_STRING, &read_uri,     "Database uri to export data from", "EXPORT_DB"},
      {"export-project",  'p', 0, G_OPTION_ARG_STRING, &read_project, "Project name in EXPORT_DB", NULL},
      {"write-db",        'd', 0, G_OPTION_ARG_STRING, &import_uri,   "Database uri to import data in", NULL},
      {NULL}
    };

#ifdef G_OS_WIN32
    args = g_win32_get_command_line ();
#else
    args = g_strdupv (argv);
#endif

    context = g_option_context_new ("<filename>");
    g_option_context_set_help_enabled (context, TRUE);
    g_option_context_set_summary (context, "Test functionality of planner data export and import.");
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
        return 0;
      }

    g_option_context_free (context);

    filename = g_strdup (args[1]);
    g_strfreev (args);
  }

  if (read_uri != NULL && read_project != NULL)
    {
      HyScanDB *db;

      g_message ("Loading data for export from database");
      db = hyscan_db_new (read_uri);
      exported_ht = get_from_db (db, read_project);
      g_object_unref (db);
    }
  else
    {
      g_message ("Creating sample objects for export");
      exported_ht = generate_objects ();
    }

  exported_size = g_hash_table_size (exported_ht);
  g_assert_cmpint (exported_size, >, 0);

  g_message ("Exporting XML: %d objects", exported_size);
  hyscan_planner_export_xml_to_file (filename, exported_ht);
  g_assert_true (g_file_get_contents (filename, &content, &length, NULL));
  g_assert_cmpint (length, >, 0);
  g_free (content);

  g_message ("Importing XML data from %s", filename);
  imported_ht = hyscan_planner_import_xml_from_file (filename);
  compare_objects (exported_ht, imported_ht);

  g_message ("Exporting KML: %d objects", exported_size);
  content = hyscan_planner_export_kml_to_str (exported_ht);
  g_assert_nonnull (content);
  g_assert_cmpint (strlen (content), >, 0);
  g_free (content);

  g_message ("---- Test write to database");
  if (import_uri != NULL)
    test_import (import_uri, exported_ht);
  else
    g_message ("Import database uri is not set. Skip.");

  g_message ("Test done!");

  g_hash_table_destroy (exported_ht);
  g_hash_table_destroy (imported_ht);
  g_free (filename);
  g_free (read_uri);
  g_free (read_project);
  g_free (import_uri);

  return 0;
}
