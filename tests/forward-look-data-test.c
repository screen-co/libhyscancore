
#include <hyscan-forward-look-data.h>
#include <hyscan-data-writer.h>
#include <hyscan-core-types.h>
#include <hyscan-cached.h>

#include <libxml/parser.h>
#include <string.h>
#include <math.h>

#define PROJECT_NAME           "test"
#define TRACK_NAME             "track"
#define SOUND_VELOCITY         1000.0

int main( int argc, char **argv )
{
  gchar *db_uri = NULL;           /* Путь к каталогу с базой данных. */
  guint n_lines = 100;            /* Число строк для теста. */
  guint n_points = 100;           /* Число точек в строке. */
  guint cache_size = 0;           /* Размер кэша, Мб. */

  HyScanDB *db;
  HyScanCache *cache = NULL;
  HyScanDataWriter *writer;
  HyScanForwardLookData *reader;

  HyScanAntennaPosition position;
  HyScanRawDataInfo raw_info1;
  HyScanRawDataInfo raw_info2;

  HyScanForwardLookDOA *doa;
  guint16 *raw_values1;
  guint16 *raw_values2;
  guint i, j, k;

  {
    gchar **args;
    GError *error = NULL;
    GOptionContext *context;
    GOptionEntry entries[] =
      {
        { "lines", 'l', 0, G_OPTION_ARG_INT, &n_lines, "Number of lines", NULL },
        { "points", 'n', 0, G_OPTION_ARG_INT, &n_points, "Number of points per line", NULL },
        { "cache", 'c', 0, G_OPTION_ARG_INT, &cache_size, "Use cache with size, Mb", NULL },
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
        return 0;
      }

    g_option_context_free (context);

    db_uri = g_strdup (args[1]);
    g_strfreev (args);
  }

  /* Параметры данных. */
  position.x = 0.0;
  position.y = 0.0;
  position.z = 0.0;
  position.psi = 0.0;
  position.gamma = 0.0;
  position.theta = 0.0;

  raw_info1.data.type = HYSCAN_DATA_COMPLEX_ADC_16LE;
  raw_info1.data.rate = 150000.0;
  raw_info1.antenna.offset.vertical = 0.0;
  raw_info1.antenna.offset.horizontal = 0.0;
  raw_info1.antenna.pattern.vertical = 10.0;
  raw_info1.antenna.pattern.horizontal = 50.0;
  raw_info1.antenna.frequency = 100000.0;
  raw_info1.antenna.bandwidth = 10000.0;
  raw_info1.adc.vref = 1.0;
  raw_info1.adc.offset = 0;

  raw_info2 = raw_info1;
  raw_info2.antenna.offset.horizontal = 0.01;

  /* Буферы для данных. */
  raw_values1 = g_new0 (guint16, 2 * n_points);
  raw_values2 = g_new0 (guint16, 2 * n_points);
  doa = g_new0 (HyScanForwardLookDOA, 2 * n_points);

  /* Открываем базу данных. */
  db = hyscan_db_new (db_uri);
  if (db == NULL)
    g_error ("can't open db at: %s", db_uri);

  /* Объект записи данных */
  writer = hyscan_data_writer_new (db);

  /* Местоположение приёмной антенны. */
  hyscan_data_writer_sonar_set_position (writer, HYSCAN_SOURCE_SIDE_SCAN_STARBOARD, &position);

  /* Проект для записи галсов. */
  if (!hyscan_data_writer_set_project (writer, PROJECT_NAME))
    g_error ("can't set working project");

  /* Создаём галс. */
  if (!hyscan_data_writer_start (writer, TRACK_NAME, HYSCAN_TRACK_SURVEY))
    g_error( "can't start write");

  /* Тестовые данные для проверки работы вперёдсмотрящего локатора. В каждой строке
   * разность фаз между двумя каналами изменяется от 0 до 2 Pi по дальности. */
  g_message ("Data generation");
  for (i = 0; i < n_lines; i++)
    {
      gint64 index_time;
      HyScanDataWriterData data;

      index_time = 1000 * (i + 1);

      /* Записываем сырые данные. */
      for (j = 0; j < n_points; j++)
        {
          gdouble mini_pi = 0.999 * G_PI;
          gdouble phase = mini_pi - 2.0 * mini_pi * ((gdouble)j / (gdouble)(n_points - 1));

          raw_values1[2 * j] = 65535;
          raw_values1[2 * j + 1] = 32767;
          raw_values2[2 * j] = 65535.0 * ((0.5 * cos (phase)) + 0.5);
          raw_values2[2 * j + 1] = 65535.0 * ((0.5 * sin (phase)) + 0.5);
        }

      data.time = index_time;
      data.size = 2 * n_points * sizeof(gint16);
      data.data = raw_values1;
      if (!hyscan_data_writer_raw_add_data (writer, HYSCAN_SOURCE_FORWARD_LOOK, 1, &raw_info1, &data))
        g_error ("can't add raw 1 data");

      data.data = raw_values2;
      if (!hyscan_data_writer_raw_add_data (writer, HYSCAN_SOURCE_FORWARD_LOOK, 2, &raw_info2, &data))
        g_error ("can't add raw 2 data");
    }

  /* Объект обработки данных вперёдсмотрящего локатора. */
  reader = hyscan_forward_look_data_new (db, PROJECT_NAME, TRACK_NAME, TRUE);
  if (reader == NULL)
    g_error ("can't create forward look data processor");

  /* Кэш данных */
  if (cache_size)
    {
      cache = HYSCAN_CACHE (hyscan_cached_new (cache_size));
      hyscan_forward_look_data_set_cache (reader, cache, NULL);
    }

  /* Скорость звука для тестовых данных. */
  hyscan_forward_look_data_set_sound_velocity (reader, SOUND_VELOCITY);

  /* Проверяем, что азимут цели изменяется в секторе обзора от минимального
   * до максимального значения по дальности. */
  for (k = 0; k < 2; k++)
    {
      if (k == 0)
        g_message ("Data check");
      else
        g_message ("Cached data check");

      for (i = 0; i < n_lines; i++)
        {
          guint32 doa_size;
          gint64 doa_time;

          doa_size = n_points;
          if (!hyscan_forward_look_data_get_doa_values (reader, i, doa, &doa_size, &doa_time))
            g_error ("can't get doa values");

          if (doa_size != n_points)
            g_error ("doa size error");

          for (j = 0; j < n_points; j++)
            {
              gdouble alpha;
              gdouble angle;
              gdouble distance;

              alpha = hyscan_forward_look_data_get_alpha (reader);
              angle = -alpha + (2 * alpha * ((gdouble)j / (gdouble)(n_points - 1)));
              distance = ((gdouble)j / raw_info1.data.rate) * SOUND_VELOCITY / 2.0;

              if ((fabs (doa[j].distance - distance) > 0.1) ||
                  (fabs (doa[j].angle - angle) > 0.1) ||
                  (fabs (doa[j].amplitude - 1.0) > 0.1))
                {
                  g_error ("doa data error");
                }
            }
        }

      if (cache == NULL)
        break;
    }

  g_message ("All done");

  g_clear_object (&writer);
  g_clear_object (&reader);

  hyscan_db_project_remove (db, PROJECT_NAME);

  g_clear_object (&db);
  g_clear_object (&cache);

  g_free (db_uri);
  g_free (raw_values1);
  g_free (raw_values2);
  g_free (doa);

  xmlCleanupParser ();

  return 0;
}
