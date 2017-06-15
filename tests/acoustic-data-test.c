
#include <hyscan-raw-data.h>
#include <hyscan-acoustic-data.h>
#include <hyscan-data-writer.h>
#include <hyscan-core-types.h>
#include <hyscan-cached.h>

#include <libxml/parser.h>
#include <string.h>
#include <math.h>

#define PROJECT_NAME           "test"
#define TRACK_NAME             "track"

int main( int argc, char **argv )
{
  gchar *db_uri = NULL;           /* Путь к каталогу с базой данных. */
  gdouble frequency = 0.0;        /* Частота сигнала. */
  gdouble duration = 0.0;         /* Длительность сигнала. */
  gdouble discretization = 0.0;   /* Частота дискретизации. */
  guint n_signals = 10;           /* Число сигналов. */
  guint n_lines = 10;             /* Число строк для каждого сигнала. */
  guint n_tvgs = 5;               /* Число "кривых" ВАРУ. */
  guint cache_size = 0;           /* Размер кэша, Мб. */
  gboolean noise = FALSE;         /* Использовать канал шумов для теста. */

  HyScanDB *db;
  HyScanCache *cache = NULL;
  HyScanDataWriter *writer;
  HyScanRawData *raw_reader;
  HyScanAcousticData *acoustic_reader;

  HyScanAntennaPosition position;
  HyScanRawDataInfo raw_info;
  HyScanRawDataInfo raw_info2;
  HyScanAcousticDataInfo acoustic_info;

  HyScanComplexFloat *signal_image;
  guint16 *data_values;
  gfloat *tvg_values;
  gfloat *amplitude;

  guint32 signal_size;
  guint32 data_size;
  guint i, j, k;

  {
    gchar **args;
    GError *error = NULL;
    GOptionContext *context;
    GOptionEntry entries[] =
      {
        { "discretization", 'd', 0, G_OPTION_ARG_DOUBLE, &discretization, "Signal discretization, Hz", NULL },
        { "frequency", 'f', 0, G_OPTION_ARG_DOUBLE, &frequency, "Signal frequency, Hz", NULL },
        { "duration", 't', 0, G_OPTION_ARG_DOUBLE, &duration, "Signal duration, s", NULL },
        { "signals", 's', 0, G_OPTION_ARG_INT, &n_signals, "Number of signals (1..100)", NULL },
        { "lines", 'l', 0, G_OPTION_ARG_INT, &n_lines, "Number of lines per signal (1..100)", NULL },
        { "tvgs", 'g', 0, G_OPTION_ARG_INT, &n_tvgs, "Number of tvgs (1..100)", NULL },
        { "cache", 'c', 0, G_OPTION_ARG_INT, &cache_size, "Use cache with size, Mb", NULL },
        { "noise", 'n', 0, G_OPTION_ARG_NONE, &noise, "Use noise channel for test", NULL },
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

    if ((g_strv_length (args) != 2) || (discretization < 1.0) || (frequency < 1.0) ||
        (duration < 1e-7) || (n_signals < 1) || (n_signals > 100) || (n_lines < 1) || (n_lines > 100))
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

  raw_info.data.type = HYSCAN_DATA_COMPLEX_ADC_16LE;
  raw_info.data.rate = discretization;
  raw_info.antenna.offset.vertical = 0.0;
  raw_info.antenna.offset.horizontal = 0.0;
  raw_info.antenna.pattern.vertical = 40.0;
  raw_info.antenna.pattern.horizontal = 2.0;
  raw_info.adc.vref = 1.0;
  raw_info.adc.offset = 0;

  acoustic_info.data.type = HYSCAN_DATA_UINT16;
  acoustic_info.data.rate = discretization;
  acoustic_info.antenna.pattern.vertical = 40.0;
  acoustic_info.antenna.pattern.horizontal = 2.0;

  signal_size = discretization * duration;
  data_size = 100 * signal_size;

  signal_image = g_new0 (HyScanComplexFloat, signal_size + 1);
  data_values = g_new0 (guint16, 2 * data_size + 1);
  tvg_values = g_new0 (gfloat, data_size + 1);
  amplitude = g_new0 (gfloat, data_size + 1);

  /* Открываем базу данных. */
  db = hyscan_db_new (db_uri);
  if (db == NULL)
    g_error ("can't open db at: %s", db_uri);

  /* Кэш данных */
  if (cache_size)
    cache = HYSCAN_CACHE (hyscan_cached_new (cache_size));

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

  /* Тестовые данные для проверки свёртки. Массив размером 100 * signal_size.
   * Сигнал располагается со смещением в две длительности. Все остальные индексы
   * массива заполнены нулями. Используется тональный сигнал. */
  g_message ("Data generation");
  for (i = 0; i < n_lines * n_signals; i++)
    {
      gint64 index_time;
      guint tvg_index;
      guint signal_index;
      gdouble work_frequency;
      HyScanDataWriterData data;

      index_time = 1000 * (i + 1);
      tvg_index = (i / n_tvgs);
      signal_index = (i / n_lines);
      work_frequency = (frequency - ((signal_index * frequency) / (5.0 * n_signals)));

      /* Записываем образ сигнала каждые n_lines строк. */
      if ((i % n_lines) == 0)
        {
          HyScanDataWriterSignal signal;
          for (j = 0; j < signal_size; j++)
            {
              gdouble time = (1.0 / discretization) * j;
              gdouble phase = 2.0 * G_PI * work_frequency * time;

              signal_image[j].re = cos (phase);
              signal_image[j].im = sin (phase);
            }

          signal.time = index_time;
          signal.rate = discretization;
          signal.n_points = signal_size;
          signal.points = signal_image;

          if (!hyscan_data_writer_raw_add_signal (writer, HYSCAN_SOURCE_SIDE_SCAN_STARBOARD, &signal))
            g_error ("can't add signal image");
        }

      /* Записываем "кривую" ВАРУ каждые n_tvgs строк. */
      if ((i % n_tvgs) == 0)
        {
          HyScanDataWriterTVG tvg;
          for (j = 0; j < data_size; j++)
            tvg_values[j] = tvg_index + (((gdouble)tvg_index / (gdouble)data_size) * j);

          tvg.time = index_time;
          tvg.rate = discretization;
          tvg.n_gains = data_size;
          tvg.gains = tvg_values;
          if (!hyscan_data_writer_raw_add_tvg (writer, HYSCAN_SOURCE_SIDE_SCAN_STARBOARD, 1, &tvg))
            g_error ("can't add tvg");
        }

      /* Записываем сырые данные. */
      for (j = 0; j < 2 * data_size; j++)
        data_values[j] = 32767;

      for (j = 2 * signal_size; j < 3 * signal_size; j++)
        {
          gdouble time = (1.0 / discretization) * j;
          gdouble phase = 2.0 * G_PI * work_frequency * time;
          data_values[2 * j] = 65535.0 * ((0.5 * cos (phase)) + 0.5);
          data_values[2 * j + 1] = 65535.0 * ((0.5 * sin (phase)) + 0.5);
        }

      data.time = index_time;
      data.size = 2 * data_size * sizeof(gint16);
      data.data = data_values;
      if (noise)
        {
          if (!hyscan_data_writer_raw_add_noise (writer, HYSCAN_SOURCE_SIDE_SCAN_STARBOARD, 1, &raw_info, &data))
            g_error ("can't add noise data");
        }
      else
        {
          if (!hyscan_data_writer_raw_add_data (writer, HYSCAN_SOURCE_SIDE_SCAN_STARBOARD, 1, &raw_info, &data))
            g_error ("can't add raw data");
        }

      /* Записываем амплитуду. */
      memset (data_values, 0, 2 * data_size);
      for (j = signal_size; j < 3 * signal_size; j++)
        {
          if ((j >= signal_size) && (j < 2 * signal_size))
            data_values[j] = 65535.0 * ((gdouble) (j - signal_size) / signal_size);
          if ((j >= 2 * signal_size) && (j < 3 * signal_size))
            data_values[j] = 65535.0 * (1.0 - (gdouble) (j - 2 * signal_size) / signal_size);
        }

      data.time = index_time;
      data.size = data_size * sizeof(guint16);
      data.data = data_values;
      if (!hyscan_data_writer_acoustic_add_data (writer, HYSCAN_SOURCE_SIDE_SCAN_STARBOARD, &acoustic_info, &data))
        g_error ("can't add acoustic data");
    }

  /* Завершаем запись. */
  g_clear_object (&writer);

  /* Объект чтения сырых данных. */
  if (noise)
    raw_reader = hyscan_raw_data_noise_new (db, PROJECT_NAME, TRACK_NAME, HYSCAN_SOURCE_SIDE_SCAN_STARBOARD, 1);
  else
    raw_reader = hyscan_raw_data_new (db, PROJECT_NAME, TRACK_NAME, HYSCAN_SOURCE_SIDE_SCAN_STARBOARD, 1);

  if (raw_reader == NULL)
    g_error ("can't open raw channel");
  hyscan_raw_data_set_cache (raw_reader, cache, NULL);

  /* Объект чтения акустических данных. */
  acoustic_reader = hyscan_acoustic_data_new (db, PROJECT_NAME, TRACK_NAME,
                                              HYSCAN_SOURCE_SIDE_SCAN_STARBOARD, FALSE);
  if (acoustic_reader == NULL)
    g_error ("can't open acoustic channel");
  hyscan_acoustic_data_set_cache (acoustic_reader, cache, NULL);

  /* Для тонального сигнала проверяем, что его свёртка совпадает с треугольником,
   * начинающимся с signal_size, пиком на 2 * signal_size и спадающим до 3 * signal_size. */
  for (k = 0; k < 2; k++)
    {
      if (k == 0)
        g_message ("Data check");
      else
        g_message ("Cached data check");

      for (i = 0; i < n_lines * n_signals; i++)
        {
          guint tvg_index;
          guint signal_index;
          gdouble work_frequency;
          gdouble square1;
          gdouble square2;
          guint32 io_size;

          tvg_index = (i / n_tvgs);
          signal_index = (i / n_lines);
          work_frequency = (frequency - ((signal_index * frequency) / (5.0 * n_signals)));

          /* Проверяем образ сигнала. */
          io_size = signal_size + 1;
          if (!hyscan_raw_data_get_signal_image (raw_reader, i, signal_image, &io_size, NULL))
            g_error ("can't get signal image");

          if ((hyscan_raw_data_get_signal_size (raw_reader, i) != signal_size) || (io_size != signal_size))
            g_error ("signal size mismatch");

          for (j = 0; j < signal_size; j++)
            {
              gdouble time = (1.0 / discretization) * j;
              gdouble phase = 2.0 * G_PI * work_frequency * time;

              if ((fabs (signal_image[j].re - cos (phase)) > 1e-5) ||
                  (fabs (signal_image[j].im - sin (phase)) > 1e-5))
                {
                  g_error ("signal image error");
                }
            }

          /* Проверяем "кривую" ВАРУ. */
          io_size = data_size + 1;
          if (!hyscan_raw_data_get_tvg_values (raw_reader, i, tvg_values, &io_size, NULL))
            g_error ("can't get tvg values");

          if (io_size != data_size)
            g_error ("tvg size mismatch");

          for (j = 0; j < data_size; j++)
            if (fabs (tvg_values[j] - (tvg_index + (((gdouble)tvg_index / (gdouble)data_size)) * j)) > 1e-5)
              g_error ("tvg error");

          /* Проверяем амплитуду сырых данных. */
          io_size = data_size + 1;
          if (!hyscan_raw_data_get_amplitude_values (raw_reader, i, amplitude, &io_size, NULL))
            g_error ("can't get raw amplitude");

          if (io_size != data_size)
            g_error ("raw amplitude size mismatch");

          square1 = 0.0;
          square2 = 0.0;
          for (j = 0; j < data_size; j++)
            {
              if ((j >= signal_size) && (j < 2 * signal_size))
                square1 += ((gdouble) (j - signal_size) / signal_size);
              if ((j >= 2 * signal_size) && (j < 3 * signal_size))
                square1 += (1.0 - (gdouble) (j - 2 * signal_size) / signal_size);

              square2 += amplitude[j];
            }

          if ((100.0 * (fabs (square1 - square2) / MAX (square1, square2))) > 0.1)
            g_error ("raw amplitude error");

          /* Проверяем амплитуду акустических данных. */
          io_size = data_size + 1;
          if (!hyscan_acoustic_data_get_values (acoustic_reader, i, amplitude, &io_size, NULL))
            g_error ("can't get acoustic amplitude");

          if (io_size != data_size)
            g_error ("acoustic amplitude size mismatch");

          square1 = 0.0;
          square2 = 0.0;
          for (j = 0; j < data_size; j++)
            {
              if ((j >= signal_size) && (j < 2 * signal_size))
                square1 += ((gdouble) (j - signal_size) / signal_size);
              if ((j >= 2 * signal_size) && (j < 3 * signal_size))
                square1 += (1.0 - (gdouble) (j - 2 * signal_size) / signal_size);

              square2 += amplitude[j];
            }

          if ((100.0 * (fabs (square1 - square2) / MAX (square1, square2))) > 0.1)
            g_error ("acoustic amplitude error");
        }

      if (cache == NULL)
        break;
    }

  g_message ("All done");

  g_clear_object (&raw_reader);
  g_clear_object (&acoustic_reader);

  hyscan_db_project_remove (db, PROJECT_NAME);

  g_clear_object (&db);
  g_clear_object (&cache);

  g_free (signal_image);
  g_free (data_values);
  g_free (tvg_values);
  g_free (amplitude);
  g_free (db_uri);

  xmlCleanupParser ();

  return 0;
}
