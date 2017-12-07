#include <hyscan-depthometer.h>
#include <hyscan-depth-nmea.h>
#include <hyscan-cached.h>
#include <hyscan-data-writer.h>
#include <string.h>

#define SIZE 1000
#define SAMPLES 100
#define NMEA_DPT_CHANNEL 3
#define DB_TIME_START 1e10
#define DB_TIME_INC 1e6
#define MORE 100
#define LESS 10

void update_nmea_data     (gchar            **data,
                           gint               i);
void set_get_check        (gchar             *log_prefix,
                           HyScanDepthometer *depth,
                           gint               filter,
                           gint64             time,
                           gfloat             expected);

void test                 (gchar             *log_prefix,
                           HyScanDepth       *idepth,
                           HyScanCache       *cache);

int
main (int argc, char **argv)
{
  GError                 *error;
  gchar                  *db_uri = NULL;
  gchar                  *name = "test";
  HyScanDB               *db;
  HyScanCache            *cache;

  /* Запись данных. */
  gchar                  *nmea_data = NULL;
  HyScanDataWriter       *writer;
  HyScanDataWriterData    data;
  HyScanAntennaPosition   position = {0};

  /* Тестируемые объекты.*/
  HyScanDepthNMEA        *depth_nmea;
  gint64 time;
  gint i;

  /* Парсим аргументы. */
  {
    gchar **args;
    error = NULL;
    GOptionContext *context;
   #ifdef G_OS_WIN32
    args = g_win32_get_command_line ();
   #else
    args = g_strdupv (argv);
   #endif
    context = g_option_context_new ("<db-uri>");
    g_option_context_set_help_enabled (context, TRUE);
    g_option_context_set_ignore_unknown_options (context, TRUE);
    if (!g_option_context_parse_strv (context, &args, &error))
      {
        g_print ("%s\n", error->message);
        return -1;
      }

    if (g_strv_length (args) == 2)
      {
        db_uri = g_strdup (args[1]);
      }
    else
      {
        g_print ("%s", g_option_context_get_help (context, FALSE, NULL));
        return 0;
      }

    g_option_context_free (context);
    g_strfreev (args);
  }

  cache = HYSCAN_CACHE (hyscan_cached_new (512));
  db = hyscan_db_new (db_uri);
  if (db == NULL)
    g_error ("can't open db");

  /* Создаем объект записи данных. */
  writer = hyscan_data_writer_new (db);
  if (!hyscan_data_writer_start (writer, name, name, HYSCAN_TRACK_SURVEY))
    g_error ("can't start write");

  /* Местоположение приёмных антенн. */
  hyscan_data_writer_sensor_set_position (writer, "sensor", &position);

  /* Наполняем данными. */
  for (i = 0, time = DB_TIME_START; i < SAMPLES; i++, time += DB_TIME_INC)
    {
      update_nmea_data (&nmea_data, i);

      data.time = time;

      data.size = strlen(nmea_data);
      data.data = nmea_data;
      hyscan_data_writer_sensor_add_data (writer, "sensor", HYSCAN_SOURCE_NMEA_DPT, NMEA_DPT_CHANNEL, &data);

    }

  /* Тестируем определение глубины по NMEA. */
  depth_nmea = hyscan_depth_nmea_new (db, name, name, NMEA_DPT_CHANNEL);
  test ("nmea", HYSCAN_DEPTH (depth_nmea), cache);
  g_clear_object (&depth_nmea);

  /* Удаляем созданный проект. */
  hyscan_db_project_remove (db, name);

  /* Освобождаем память. */
  g_clear_object (&db);
  g_clear_object (&cache);
  g_clear_object (&writer);

  g_free (nmea_data);

  g_free (db_uri);

  g_message ("test passed");
  return 0;
}

void
update_nmea_data (gchar **data, gint i)
{
  gint csum = 0;
  gchar *ch, *sentence;

  g_free (*data);

  i = (i == 50) ? MORE : LESS;
  sentence = g_strdup_printf ("xxDPT,%03i.0,", i);
  for (ch = sentence; *ch != '\0'; ch++)
    csum ^= *ch;

  *data = g_strdup_printf ("$%s*%x", sentence, csum);

  g_free (sentence);
}

void
set_get_check (gchar             *log_prefix,
               HyScanDepthometer *depth,
               gint               filter,
               gint64             time,
               gfloat             expected)
{
  gfloat val;

  hyscan_depthometer_set_filter_size (depth, filter);
  val = hyscan_depthometer_get (depth, time);
  if (val == expected)
    return;

  g_error ("%s: Time: %li Filter: %i. Expected %f, got %f",
           log_prefix, time, filter, expected, val);
}


void test (gchar             *log_prefix,
           HyScanDepth       *idepth,
           HyScanCache       *cache)
{
  HyScanDepthometer *meter = hyscan_depthometer_new (idepth);
  gint64 t;

  t = DB_TIME_START - 50 * DB_TIME_INC;
  set_get_check (log_prefix, meter, 2, t, -1.0);

  t = DB_TIME_START + 50 * DB_TIME_INC;
  set_get_check (log_prefix, meter, 2, t, MORE);
  set_get_check (log_prefix, meter, 4, t, (MORE + LESS) / 2.0);

  t = DB_TIME_START + 50.5 * DB_TIME_INC;
  set_get_check (log_prefix, meter, 2, t, (MORE + LESS) / 2.0);
  set_get_check (log_prefix, meter, 4, t, (MORE + 3 * LESS) / 4.0);

  hyscan_depth_set_cache (idepth, cache, "test_prefix");
  hyscan_depthometer_set_cache (meter, cache, "test_prefix");
  hyscan_depthometer_set_validity_time (meter, DB_TIME_INC);

  t = DB_TIME_START + 50 * DB_TIME_INC;
  set_get_check (log_prefix, meter, 2, t, MORE);
  set_get_check (log_prefix, meter, 2, t, MORE);
  t = DB_TIME_START + 50.6 * DB_TIME_INC;
  set_get_check (log_prefix, meter, 2, t, LESS);
  set_get_check (log_prefix, meter, 2, t, LESS);

  hyscan_depthometer_set_validity_time (meter, DB_TIME_INC / 2);
  set_get_check (log_prefix, meter, 2, t, (MORE + LESS) / 2);
  set_get_check (log_prefix, meter, 2, t, (MORE + LESS) / 2);

  g_clear_object (&meter);
}
