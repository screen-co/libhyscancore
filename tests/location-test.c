#include <hyscan-location.h>
#include <hyscan-data-channel-writer.h>
#include <hyscan-db-file.h>
#include <hyscan-cached.h>
#include <hyscan-data-channel.h>
#include <hyscan-core-exports.h>

#include <glib/gstdio.h>
#include <gio/gio.h>

#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>

#define AOP (10000)
//#define FILENM "/home/dmitriev.a/loca/output.txt"

int
main (int argc, char **argv)
{
  GError *error = NULL;
  gchar *resource_name;
  gsize test_data_size;

  GBytes *rmc_resource;
  GBytes *gga_resource;
  //GBytes *dpt_resource;
  const gchar *rmc_data;
  const gchar *gga_data;
  gchar **rmc_test_data;
  gchar **gga_test_data;
  gfloat depth_test_data[5000] = {0};
  gint i;
  gchar *filename;
  gint64 points_num = 10000,
         points_freq = 0;

  resource_name = g_strdup_printf ("/org/hyscan/location-test/rmc_test_data");
  rmc_resource = g_resources_lookup_data (resource_name, G_RESOURCE_LOOKUP_FLAGS_NONE, &error);
  test_data_size = g_bytes_get_size (rmc_resource);
  rmc_data = (const gchar*) g_bytes_get_data (rmc_resource, &test_data_size);

  resource_name = g_strdup_printf ("/org/hyscan/location-test/gga_test_data");
  gga_resource = g_resources_lookup_data (resource_name, G_RESOURCE_LOOKUP_FLAGS_NONE, &error);
  test_data_size = g_bytes_get_size (gga_resource);
  gga_data = (const gchar*) g_bytes_get_data (gga_resource, &test_data_size);

  for (i = 0; i < 5000; i++)
    depth_test_data[i] = (i > 1000 && i < 1010) ? 32767 : 0;

  g_bytes_unref (rmc_resource);
  g_bytes_unref (gga_resource);


  rmc_test_data = g_strsplit (rmc_data, "\n", -1);
  gga_test_data = g_strsplit (gga_data, "\n", -1);

  gchar *db_uri = NULL; /* Путь к каталогу с базой данных. */
  gint cache_size = 0; /* Размер кэша, Мб. */
  HyScanDB *db;
  HyScanCache *cache = NULL;
  gint parameter;
  HyScanLocation *location;
  HyScanLocationData data = {0},
		     data_cached = {0};
  gint32 project_id;
  gint32 channel_id1;
  gint32 channel_id2;
  const gchar *channel_name1;
  const gchar *channel_name2;
  const gchar *channel_name3;
  HyScanDataChannelInfo dc_info = {HYSCAN_DATA_COMPLEX_ADC_16LE, 750, 0};
  HyScanDataChannelWriter *dc_writer;

  HyScanLocationSources ** source_list;

  gint32 db_index;
  gint64 db_time;

  HyScanSensorChannelInfo nmea_channel_info = {0, 0, 0, 0, 0, 0};

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
        g_message (error->message);
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

  /* Создаём проект. */
  project_id = hyscan_db_project_create (db, "project", NULL);
  if (project_id < 0)
    g_error ("can't create project");

  /* Создаём галс. */
  if (!hyscan_track_create (db, "project", "track", HYSCAN_TRACK_SURVEY))
    g_error( "can't create track");

  /* Создаём каналы данных. */
  /* РМЦ. */
  channel_name1 = hyscan_channel_get_name_by_types (HYSCAN_SONAR_DATA_NMEA_RMC, FALSE, FALSE, HYSCAN_SONAR_CHANNEL_3);
  channel_id1 = hyscan_channel_sensor_create (db,"project", "track", channel_name1, &nmea_channel_info);

  /* ГГА. */
  channel_name2 = hyscan_channel_get_name_by_types (HYSCAN_SONAR_DATA_NMEA_GGA, FALSE, FALSE, HYSCAN_SONAR_CHANNEL_2);
  channel_id2 = hyscan_channel_sensor_create (db,"project", "track", channel_name2, &nmea_channel_info);

  /* Глубина. */
  channel_name3 = hyscan_channel_get_name_by_types (HYSCAN_SONAR_DATA_ECHOSOUNDER, FALSE, FALSE, HYSCAN_SONAR_CHANNEL_1);
  dc_writer = hyscan_data_channel_writer_new(db, "project", "track", channel_name3, &dc_info);

  /* Заполняем каналы тестовыми данными. */
  for (i = 0, db_time = 1e10, db_index = 2^16; i < AOP; i++, db_index++, db_time+=1e6)
    {
      hyscan_db_channel_add_data (db, channel_id1, db_time, rmc_test_data[i], strlen(rmc_test_data[i]), NULL);
      hyscan_db_channel_add_data (db, channel_id2, db_time, gga_test_data[i], strlen(gga_test_data[i]), NULL);
      hyscan_data_channel_writer_add_data (dc_writer, db_time, &depth_test_data, 5000 * sizeof(gfloat));
    }

  /* Переводим каналы в режим "только для чтения". */
  hyscan_db_channel_finalize (db, channel_id1);
  hyscan_db_channel_finalize (db, channel_id2);

  /* Создаем объект. */
  location = hyscan_location_new_with_cache_prefix (db, cache, "locacache", "project", "track", 0);

  /* Проверка получения списка источников. */
  source_list = hyscan_location_source_list (location, HYSCAN_LOCATION_PARAMETER_LATLONG);
  for (i = 0; source_list[i] != NULL; i++)
    g_printf("LATLONG: index %i, source_type %i, sensor_channel %i\n",
             source_list[i]->index, source_list[i]->source_type, source_list[i]->sensor_channel);
  hyscan_location_source_list_free (&source_list);

  source_list = hyscan_location_source_list (location, HYSCAN_LOCATION_PARAMETER_TRACK);
  for (i = 0; source_list[i] != NULL; i++)
    g_printf("TRACK: index %i, source_type %i, sensor_channel %i\n",
             source_list[i]->index, source_list[i]->source_type, source_list[i]->sensor_channel);
  hyscan_location_source_list_free (&source_list);

  source_list = hyscan_location_source_list (location, HYSCAN_LOCATION_PARAMETER_SPEED);
  for (i = 0; source_list[i] != NULL; i++)
    g_printf("SPEED: index %i, source_type %i, sensor_channel %i\n",
             source_list[i]->index, source_list[i]->source_type, source_list[i]->sensor_channel);
  hyscan_location_source_list_free (&source_list);

  source_list = hyscan_location_source_list (location, HYSCAN_LOCATION_PARAMETER_DEPTH);
  for (i = 0; source_list[i] != NULL; i++)
    g_printf("DEPTH: index %i, source_type %i, sensor_channel %i\n",
             source_list[i]->index, source_list[i]->source_type, source_list[i]->sensor_channel);
  hyscan_location_source_list_free (&source_list);

  source_list = hyscan_location_source_list (location, HYSCAN_LOCATION_PARAMETER_ALTITUDE);
  for (i = 0; source_list[i] != NULL; i++)
    g_printf("ALTITUDE: index %i, source_type %i, sensor_channel %i\n",
             source_list[i]->index, source_list[i]->source_type, source_list[i]->sensor_channel);
  hyscan_location_source_list_free (&source_list);

  source_list = hyscan_location_source_list (location, HYSCAN_LOCATION_PARAMETER_DATETIME);
  for (i = 0; source_list[i] != NULL; i++)
    g_printf("DATETIME: index %i, source_type %i, sensor_channel %i\n",
             source_list[i]->index, source_list[i]->source_type, source_list[i]->sensor_channel);
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
  if (points_num>0)
    {
      while (i < points_num)
        {
          data.validity = FALSE;
          while (data.validity == FALSE)
            {
              data = hyscan_location_get (location, parameter, 1e10 + i*points_freq, 0, 0, 0, 0, 0, 0);
              if (cache != NULL)
                {
                  data_cached = hyscan_location_get (location, parameter, 1e10 + i*points_freq, 0, 0, 0, 0, 0, 0);
                  if (data.validity && data.validity != data_cached.validity)
                    g_printf ("cache error @ step %i\n",i);
                }
            }
          g_fprintf (outfile, "%10.8f,%10.8f,%f,%f,%f,%f\n", data.latitude, data.longitude, data.track, data.speed, data.depth, data.altitude);
          i++;
        }
    }
  g_printf("data points obtained: %i\n",i);
  fclose(outfile);
  g_object_unref(location);
  /* Закрываем каналы данных. */
  hyscan_db_close (db, channel_id1);

  /* Закрываем галс и проект. */
  hyscan_db_close (db, project_id);

  /* Удаляем проект. */
  hyscan_db_project_remove (db, "project");
  g_printf ("data remove ok\n");

  /* Закрываем базу данных. */
  g_clear_object (&db);

  /* Удаляем кэш. */
  g_clear_object (&cache);

  return 0;
}
