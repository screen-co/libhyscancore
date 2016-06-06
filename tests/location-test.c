#include <hyscan-data-channel-writer.h>
#include <hyscan-location.h>
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
#define FILENM "/home/dmitriev.a/loca/outpoot.txt"

int
main (int argc, char **argv)
{
  #include "/home/dmitriev.a/loca/rmc_test_data"
/*
#include "/home/dmitriev.a/loca/6/rmc-c"
  #include "/home/dmitriev.a/loca/1/rmc-c"
  #include "/home/dmitriev.a/loca/5/rmc-c"
  #include "/home/dmitriev.a/loca/2/rmc-c"
  #include "/home/dmitriev.a/loca/3/rmc-c"
  #include "/home/dmitriev.a/loca/4/rmc-c"
*/
  #include "/home/dmitriev.a/loca/gga_test_data"
  #include "/home/dmitriev.a/loca/depth_test_data"
  gchar *db_uri = NULL; /* Путь к каталогу с базой данных. */
  gint cache_size = 0; /* Размер кэша, Мб. */
  int i = 0;
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

  gint32 db_index;
  gint64 db_time;



  HyScanSensorChannelInfo nmea_channel_info = {0, 0 , 0, 0, 0, 0};

  {
    gchar **args;

#ifdef G_OS_WIN32
    args = g_win32_get_command_line ();
#else
    args = g_strdupv (argv);
#endif

    db_uri = g_strdup (args[1]);
    g_strfreev (args);
  }

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

  ///* ГГА. */
  channel_name2 = hyscan_channel_get_name_by_types (HYSCAN_SONAR_DATA_NMEA_GGA, FALSE, FALSE, HYSCAN_SONAR_CHANNEL_1);
  channel_id2 = hyscan_channel_sensor_create (db,"project", "track", channel_name2, &nmea_channel_info);

  /* Глубина. */
  channel_name3 = hyscan_channel_get_name_by_types (HYSCAN_SONAR_DATA_ECHOSOUNDER, FALSE, FALSE, HYSCAN_SONAR_CHANNEL_1);
  dc_writer = hyscan_data_channel_writer_new(db, "project", "track", channel_name3, &dc_info);
  //channel_id3 = hyscan_db_channel_create (db, track_id, channel_name3, NULL);

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
  location = hyscan_location_new (db, cache, "locacache", "project", "track", 0);

  FILE *outfile = g_fopen(FILENM, "w+");
  i = 0;
  parameter = HYSCAN_LOCATION_PARAMETER_LATLONG
              | HYSCAN_LOCATION_PARAMETER_TRACK
              | HYSCAN_LOCATION_PARAMETER_SPEED
              | HYSCAN_LOCATION_PARAMETER_DEPTH
              | HYSCAN_LOCATION_PARAMETER_ALTITUDE
              | HYSCAN_LOCATION_PARAMETER_DATETIME
              ;
  while (i < AOP)
    {
    data.validity = FALSE;
      while (data.validity == FALSE)
        {
          data = hyscan_location_get (location, parameter, 1e10 + i*2*5e5, 0, 0, 0, 0, 0, 0);
          if (cache != NULL)
            {
              data_cached = hyscan_location_get (location, parameter, 1e10 + i*2*5e5, 0, 0, 0, 0, 0, 0);
              if (data.validity && data.validity != data_cached.validity)
                g_printf ("cache error @ step %i\n",i);
            }
        }
      g_fprintf (outfile, "%f,%f,%f,%f,%f,%f\n", data.latitude, data.longitude, data.track, data.speed, data.depth, data.altitude);
      //g_fprintf (outfile, "%10.8f,%10.8f,%f\n", data.latitude, data.longitude, data.track);
      //g_fprintf (outfile, "%10.8f,%10.8f\n", data.latitude, data.longitude);
      i++;
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
