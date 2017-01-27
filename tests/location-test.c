#include <hyscan-location.h>
#include <hyscan-data-writer.h>
#include <hyscan-cached.h>

#include <glib/gstdio.h>
#include <gio/gio.h>

#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>

#define KYLW "\x1b[33;22m"
#define KGRN "\x1b[32;22m"
#define KRED "\x1b[31;22m"
#define KNRM "\x1b[0m"

gboolean location_metrics (gdouble orig_lat[10000],
                           gdouble orig_lon[10000],
                           gdouble orig_trk[10000],
                           gdouble lat[10000],
                           gdouble lon[10000],
                           gdouble trk[10000]);

int
main (int argc, char **argv)
{
  gchar *source_types[8] = {
    "NMEA",
    "NMEA_COMPUTED",
    "ECHOSOUNDER",
    "SONAR_PORT",
    "SONAR_STARBOARD",
    "SONAR_HIRES_PORT",
    "SONAR_HIRES_STARBOARD",
    "SAS"};

  GError *error = NULL;
  gchar *db_uri = NULL; /* Путь к каталогу с базой данных. */
  gint cache_size = 0; /* Размер кэша, Мб. */
  HyScanDB *db;
  HyScanCache *cache = NULL;
  gint parameter;
  HyScanLocation *location;
  gint32 location_mod_count;
  HyScanLocationData data = {0},
                     data_cached = {0};
  gint32 project_id;

  HyScanAntennaPosition position = {0, 0, 0, 0, 0, 0};
  HyScanAcousticDataInfo info = {{HYSCAN_DATA_COMPLEX_ADC_16LE, 750}, {{0}}};
  HyScanDataWriter *writer;

  HyScanLocationSources **source_list;

  gint32 db_index;
  gint64 db_time;

  GBytes *rmc_resource;
  GBytes *gga_resource;
  GBytes *orig_resource;
  GBytes *schema_resource;

  const gchar *rmc_data;
  const gchar *gga_data;
  const gchar *orig_data;
  const gchar *schema_data;

  gchar **rmc_test_data;
  gchar **gga_test_data;
  gchar **orig_test_data;

  gfloat depth_test_data[5000] = {0};

  gdouble orig_lat[10000] = {0};
  gdouble orig_lon[10000] = {0};
  gdouble orig_trk[10000] = {0};
  gdouble lat[10000] = {0};
  gdouble lon[10000] = {0};
  gdouble trk[10000] = {0};
  gboolean metric;

  gint i;
  gchar *char_ptr;
  gchar *filename;
  gint64 points_num = 10000,
         points_freq = 0;

  gint32 project_param_id;

  rmc_resource = g_resources_lookup_data ("/org/hyscan/data/location-test-rmc", G_RESOURCE_LOOKUP_FLAGS_NONE, &error);
  rmc_data = (const gchar*) g_bytes_get_data (rmc_resource, NULL);

  gga_resource = g_resources_lookup_data ("/org/hyscan/data/location-test-gga", G_RESOURCE_LOOKUP_FLAGS_NONE, &error);
  gga_data = (const gchar*) g_bytes_get_data (gga_resource, NULL);

  orig_resource = g_resources_lookup_data ("/org/hyscan/data/location-test-clean-track", G_RESOURCE_LOOKUP_FLAGS_NONE, &error);
  orig_data = (const gchar*) g_bytes_get_data (orig_resource, NULL);

  rmc_test_data = g_strsplit (rmc_data, "\n", -1);
  gga_test_data = g_strsplit (gga_data, "\n", -1);
  orig_test_data = g_strsplit (orig_data, "\n", -1);

  g_bytes_unref (rmc_resource);
  g_bytes_unref (gga_resource);
  g_bytes_unref (orig_resource);

  for (i = 0; i < 5000; i++)
    depth_test_data[i] = (i > 1000 && i < 1010) ? 32767 : 0;

  for (i = 0; i < 10000; i++)
    {
      char_ptr = orig_test_data[i];
      orig_lat[i] = g_ascii_strtod (char_ptr,&char_ptr);
      orig_lon[i] = g_ascii_strtod (char_ptr,&char_ptr);
      orig_trk[i] = g_ascii_strtod (char_ptr,&char_ptr);
    }

  {
    gchar **args;
    error = NULL;
    GOptionContext *context;
    GOptionEntry entries[] = {
      {"output", 'o', 0, G_OPTION_ARG_STRING, &filename, "Path and name of output file;", NULL},
      {"points", 'n', 0, G_OPTION_ARG_INT, &points_num, "Number of data points to obtain;", NULL},
      {"frequency", 'f', 0, G_OPTION_ARG_INT, &points_freq, "Number of data points between actual points. Can be negative. ", NULL},
      {NULL}
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
        return 0;
      }

    g_option_context_free (context);

    db_uri = g_strdup (args[1]);
    g_strfreev (args);
  }

  if (points_freq < 0)
    points_freq = 10e5 * ABS(points_freq);
  else
    points_freq = 10e5 / (points_freq+1);

  /* Открываем базу данных. */
  db = hyscan_db_new (db_uri);
  if (db == NULL)
    g_error ("can't open db at: %s", db_uri);

  /* Кэш данных */
  cache_size = 1024;
  if (cache_size)
    cache = HYSCAN_CACHE (hyscan_cached_new (cache_size));

  /* Считываем содержание xml-файла со схемой данных. */
  schema_resource = g_resources_lookup_data ("/org/hyscan/schema/location-schema.xml", G_RESOURCE_LOOKUP_FLAGS_NONE, &error);
  schema_data = (const gchar*) g_bytes_get_data (orig_resource, NULL);

  /* Создаём проект. */
  project_id = hyscan_db_project_create (db, "project", schema_data);
  if (project_id < 0)
    g_error ("can't create project");

  g_bytes_unref (schema_resource);

  /* Создаём галс. */
  writer = hyscan_data_writer_new (db);

  if (!hyscan_data_writer_project_set (writer, "project"))
    g_error ("can't set working project");

  if (!hyscan_data_writer_start (writer, "track", HYSCAN_TRACK_SURVEY))
    g_error ("can't start write");

  /* Местоположение приёмных антенн. */
  hyscan_data_writer_sensor_set_position (writer, "sensor", &position);
  hyscan_data_writer_sonar_set_position (writer, HYSCAN_SOURCE_ECHOSOUNDER, &position);

  /* Заполняем каналы тестовыми данными. */
  for (i = 0, db_time = 1e10, db_index = 2^16; i < 10000; i++, db_index++, db_time+=1e6)
    {
      HyScanDataWriterData data;

      data.time = db_time;
      data.size = strlen(rmc_test_data[i]);
      data.data = rmc_test_data[i];
      hyscan_data_writer_sensor_add_data (writer, "sensor", HYSCAN_SOURCE_NMEA_RMC, 3, &data);

      data.time = db_time;
      data.size = strlen(gga_test_data[i]);
      data.data = gga_test_data[i];
      hyscan_data_writer_sensor_add_data (writer, "sensor", HYSCAN_SOURCE_NMEA_GGA, 2, &data);

      data.time = db_time;
      data.size = 5000 * sizeof (gfloat);
      data.data = depth_test_data;
      hyscan_data_writer_acoustic_add_data (writer, HYSCAN_SOURCE_ECHOSOUNDER, &info, &data);
    }

  hyscan_data_writer_stop (writer);
  g_object_unref (writer);

  /* Создаем объект. */
  location = hyscan_location_new_with_cache_prefix (db, cache, "locacache", "project", "track", NULL , 0);

  /* Проверка получения списка источников. */
  source_list = hyscan_location_source_list (location, HYSCAN_LOCATION_PARAMETER_LATLONG);
  for (i = 0; source_list[i] != NULL; i++)
    g_printf (KYLW "%-8s" KNRM " index: %i type: %-13s channel: %i\n","LATLONG",
             source_list[i]->index, source_types[(source_list[i]->source_type)], source_list[i]->sensor_channel);
  hyscan_location_source_list_free (&source_list);

  source_list = hyscan_location_source_list (location, HYSCAN_LOCATION_PARAMETER_TRACK);
  for (i = 0; source_list[i] != NULL; i++)
    g_printf (KYLW "%-8s" KNRM " index: %i type: %-13s channel: %i\n","TRACK",
             source_list[i]->index, source_types[(source_list[i]->source_type)], source_list[i]->sensor_channel);
  hyscan_location_source_list_free (&source_list);

  source_list = hyscan_location_source_list (location, HYSCAN_LOCATION_PARAMETER_SPEED);
  for (i = 0; source_list[i] != NULL; i++)
    g_printf (KYLW "%-8s" KNRM " index: %i type: %-13s channel: %i\n","SPEED",
             source_list[i]->index, source_types[(source_list[i]->source_type)], source_list[i]->sensor_channel);
  hyscan_location_source_list_free (&source_list);

  source_list = hyscan_location_source_list (location, HYSCAN_LOCATION_PARAMETER_DEPTH);
  for (i = 0; source_list[i] != NULL; i++)
    g_printf (KYLW "%-8s" KNRM " index: %i type: %-13s channel: %i\n","DEPTH",
             source_list[i]->index, source_types[(source_list[i]->source_type)], source_list[i]->sensor_channel);
  hyscan_location_source_list_free (&source_list);

  source_list = hyscan_location_source_list (location, HYSCAN_LOCATION_PARAMETER_ALTITUDE);
  for (i = 0; source_list[i] != NULL; i++)
    g_printf (KYLW "%-8s" KNRM " index: %i type: %-13s channel: %i\n","ALTITUDE",
             source_list[i]->index, source_types[(source_list[i]->source_type)], source_list[i]->sensor_channel);
  hyscan_location_source_list_free (&source_list);

  source_list = hyscan_location_source_list (location, HYSCAN_LOCATION_PARAMETER_DATETIME);
  for (i = 0; source_list[i] != NULL; i++)
    g_printf (KYLW "%-8s" KNRM " index: %i type: %-13s channel: %i\n","DATETIME",
             source_list[i]->index, source_types[(source_list[i]->source_type)], source_list[i]->sensor_channel);
  hyscan_location_source_list_free (&source_list);

  FILE *outfile = g_fopen(filename, "w+");
  i = 0;
  parameter = HYSCAN_LOCATION_PARAMETER_LATLONG
              | HYSCAN_LOCATION_PARAMETER_TRACK
              | HYSCAN_LOCATION_PARAMETER_SPEED
              | HYSCAN_LOCATION_PARAMETER_DEPTH
              | HYSCAN_LOCATION_PARAMETER_ALTITUDE
              | HYSCAN_LOCATION_PARAMETER_DATETIME
              ;

  while (i < 10000)
    {
      data.validity = 0;
      while (data.validity == 0)
        {
          data = hyscan_location_get (location, parameter, 1e10 + i*points_freq, 0, 0, 0, 0, 0, 0, FALSE);
          if (cache != NULL)
            {
              data_cached = hyscan_location_get (location, parameter, 1e10 + i*points_freq, 0, 0, 0, 0, 0, 0, FALSE);
              if (data.validity && data.validity != data_cached.validity)
                g_printf ("cache error @ step %i\n",i);
            }
        }
      //g_fprintf (outfile, "%10.8f,%10.8f,%f,%f,%f,%f\n", data.latitude, data.longitude, data.track, data.speed, data.depth, data.altitude);
      g_fprintf (outfile, "%10.8f %10.8f %10.8f\n", data.latitude, data.longitude, data.track);
      lat[i] = data.latitude;
      lon[i] = data.longitude;
      trk[i] = data.track;
      i++;
    }

g_printf ("data points obtained:" KGRN " %i\n" KNRM, i);
g_fprintf (outfile, "latitude,longitude,track\n");

location_mod_count = hyscan_location_get_mod_count (location);

/* Открываем группу параметров проекта. */
project_param_id = hyscan_db_project_param_open (db, project_id, "location.default.track");

/* Создаем объекты обработки. */
/* latlong-edit. */
hyscan_db_param_object_create (db, project_param_id, "location-edit-latlong3", "location-edit-latlong");
hyscan_db_param_set_integer (db, project_param_id, "location-edit-latlong3", "/time", 1e10+1e6*2);
hyscan_db_param_set_double (db, project_param_id, "location-edit-latlong3", "/new-latitude", 55.0);
hyscan_db_param_set_double (db, project_param_id, "location-edit-latlong3", "/new-longitude", 36.0);

hyscan_db_param_object_create (db, project_param_id, "location-bulk-remove1", "location-bulk-remove");
hyscan_db_param_set_integer (db, project_param_id, "location-bulk-remove1", "/ltime", 1e10+1e6*450);
hyscan_db_param_set_integer (db, project_param_id, "location-bulk-remove1", "/rtime", 1e10+1e6*500);

while (location_mod_count == hyscan_location_get_mod_count (location));
location_mod_count = hyscan_location_get_mod_count (location);

  i = 0;
  while (i < 10000)
    {
      data.validity = 0;
      while (data.validity == 0)
        {
          data = hyscan_location_get (location, parameter, 1e10 + i*points_freq, 0, 0, 0, 0, 0, 0, FALSE);
          if (cache != NULL)
            {
              data_cached = hyscan_location_get (location, parameter, 1e10 + i*points_freq, 0, 0, 0, 0, 0, 0, FALSE);
              if (data.validity && data.validity != data_cached.validity)
                g_printf ("cache error @ step %i\n",i);
            }
        }
      //g_fprintf (outfile, "%10.8f,%10.8f,%f,%f,%f,%f\n", data.latitude, data.longitude, data.track, data.speed, data.depth, data.altitude);
      g_fprintf (outfile, "%10.8f,%10.8f,%10.8f\n", data.latitude, data.longitude, data.track);
      i++;
    }

  /* Вычисляем значение метрики. */
  metric = location_metrics (orig_lat, orig_lon, orig_trk, lat, lon, trk);

  g_object_unref (location);
  fclose (outfile);

  /* Закрываем галс и проект. */
  hyscan_db_close (db, project_id);

  /* Удаляем проект. */
  hyscan_db_project_remove (db, "project");
  g_printf ("data remove ok\n");

  /* Закрываем базу данных. */
  g_clear_object (&db);

  /* Удаляем кэш. */
  g_clear_object (&cache);

  g_free (rmc_test_data);
  g_free (gga_test_data);
  g_free (orig_test_data);

  if (metric)
    {
      g_printf (KGRN "TEST PASSED\n" KNRM);
      return 0;
    }
  else
    {
      g_printf (KRED "TEST NOT PASSED\n" KNRM);
      return 1;
    }
}

/* Метрика для автоматизации теста.
 * Для каждой пары точек сглаженного и незашумленного трека вычисляется расстояние и курс.
 * После чего эти величины усредняются.
 * Положим, что если средняя разница в расстояниях не должна превышать 0.1 м (10 см),
 * а по курсу - 0.1 градус. В таком случае вовращается TRUE.
 */
gboolean location_metrics (gdouble orig_lat[10000],
                          gdouble orig_lon[10000],
                          gdouble orig_trk[10000],
                          gdouble lat[10000],
                          gdouble lon[10000],
                          gdouble trk[10000])
{
  gint i = 0;

  gdouble dlat = 0;
  gdouble orig_dlat = 0;
  gdouble dlon = 0;
  gdouble orig_dlon = 0;

  gdouble dist_delta[10000] = {0};
  gdouble trk_delta[10000] = {0};

  gdouble dist_mean = 0.0,
          trk_mean = 0.0,
          ok_dist_mean = 0.1,
          ok_trk_mean = 0.1;

  /* Вычисляем значения отклонений по расстоянию и изменения курса. */
  for (i = 0; i < 10000; i++)
    {
      if (i == 0)
        {
          trk_delta[i] = fabs(trk[0] - trk[10000]);
          dlat = (lat[0] - lat[10000]) * (G_PI / 180.0) * 6474423.1;
          dlon = (lon[0] - lon[10000]) * (G_PI / 180.0) * 6474423.1 * cos (lat[0] * (G_PI / 180.0));
          orig_dlat = (orig_lat[0] - orig_lat[10000]) * (G_PI / 180.0) * 6474423.1;
          orig_dlon = (orig_lon[0] - orig_lon[10000]) * (G_PI / 180.0) * 6474423.1 * cos (orig_lat[0] * (G_PI / 180.0));
          dist_delta[i] = fabs (sqrt (dlat*dlat + dlon*dlon) - sqrt (orig_dlat*orig_dlat + orig_dlon*orig_dlon));
        }
      else
        {
          trk_delta[i] = fabs(trk[i] - trk[i - 1]);
          dlat = (lat[i] - lat[i-1]) * (G_PI / 180.0) * 6474423.1;
          dlon = (lon[i] - lon[i-1]) * (G_PI / 180.0) * 6474423.1 * cos (lat[i] * (G_PI / 180.0));
          orig_dlat = (orig_lat[i] - orig_lat[i-1]) * (G_PI / 180.0) * 6474423.1;
          orig_dlon = (orig_lon[i] - orig_lon[i-1]) * (G_PI / 180.0) * 6474423.1 * cos (orig_lat[i] * (G_PI / 180.0));
          dist_delta[i] = fabs (sqrt (dlat*dlat + dlon*dlon) - sqrt (orig_dlat*orig_dlat + orig_dlon*orig_dlon));
        }

      if (trk_delta[i] > 180.0)
        trk_delta[i] = 360.0 - trk_delta[i];
    }

  for (i = 0; i < 10000; i++)
    {
      dist_mean += dist_delta[i];
      trk_mean += trk_delta[i];
    }

  dist_mean /= 10000.0;
  trk_mean /= 10000.0;

  if (dist_mean < ok_dist_mean)
    g_printf ("mean distance delta: " KGRN "%f" KNRM " m.\n" KNRM, dist_mean);
  else
    g_printf ("mean distance delta: " KYLW "%f" KNRM " m.\n" KNRM, dist_mean);

  if (trk_mean < ok_trk_mean)
    g_printf ("mean track delta: " KGRN "%f" KNRM "°\n" , trk_mean);
  else
    g_printf ("mean track delta: " KYLW "%f" KNRM "°\n" , trk_mean);


  if (dist_mean <= 0.1 && trk_mean <= 0.1)
    {
      g_printf (KGRN "metric: %f\n" KNRM, dist_mean * trk_mean);
      return TRUE;
    }
  else
    {
      g_printf (KRED "metric: %f\n" KNRM, dist_mean * trk_mean);
      return FALSE;
    }
}
