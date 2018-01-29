
#include <hyscan-raw-data.h>
#include <hyscan-acoustic-data.h>
#include <hyscan-data-writer.h>
#include <hyscan-buffer.h>
#include <hyscan-cached.h>

#include <libxml/parser.h>
#include <string.h>
#include <math.h>

#define PROJECT_NAME           "test"
#define TRACK_NAME             "track"

typedef struct _test_info test_info;
struct _test_info
{
  const gchar         *name;
  HyScanDataType       type;
  gdouble              error;
};

test_info raw_test_types [] =
{
  { "adc-14le", HYSCAN_DATA_COMPLEX_ADC_14LE,  1e-6 },
  { "adc-16le", HYSCAN_DATA_COMPLEX_ADC_16LE,  1e-6 },
  { "adc-24le", HYSCAN_DATA_COMPLEX_ADC_24LE,  1e-8 },
  { "float",    HYSCAN_DATA_COMPLEX_FLOAT,     1e-8 },
};

test_info amp_test_types [] =
{
  { "float",    HYSCAN_DATA_FLOAT,             1e-9 },
  { "amp-i8",   HYSCAN_DATA_AMPLITUDE_INT8,    1e-4 },
  { "amp-i16",  HYSCAN_DATA_AMPLITUDE_INT16,   1e-6 },
  { "amp-i32",  HYSCAN_DATA_AMPLITUDE_INT32,   1e-9 },
  { "amp-f8",   HYSCAN_DATA_AMPLITUDE_FLOAT8,  1e-4 },
  { "amp-f16",  HYSCAN_DATA_AMPLITUDE_FLOAT16, 1e-6 }
};

int main( int argc, char **argv )
{
  gchar *db_uri = NULL;
  gchar *raw_type_name = g_strdup ("adc-16le");
  gchar *amp_type_name = g_strdup ("amp-i16");
  gdouble frequency = 0.0;
  gdouble duration = 0.0;
  gdouble discretization = 0.0;
  guint n_signals = 10;
  guint n_lines = 10;
  guint n_tvgs = 5;
  guint cache_size = 0;
  gboolean noise = FALSE;

  HyScanDB *db;
  HyScanCache *cache = NULL;
  HyScanDataWriter *writer;
  HyScanRawData *raw_reader;
  HyScanAcousticData *acoustic_reader;

  HyScanAntennaPosition position;
  HyScanRawDataInfo raw_info;
  HyScanAcousticDataInfo acoustic_info;

  HyScanBuffer *signal_buffer;
  HyScanBuffer *tvg_buffer;
  HyScanBuffer *cplx_buffer;
  HyScanBuffer *amp_buffer;
  HyScanBuffer *channel_buffer;

  HyScanDataType raw_type;
  HyScanDataType amp_type;

  guint32 n_signal_points;
  guint32 n_data_points;
  gdouble raw_error = 0.0;
  gdouble amp_error = 0.0;

  guint i, j, k;

  {
    gchar **args;
    GError *error = NULL;
    GOptionContext *context;
    GOptionEntry entries[] =
      {
        { "raw-type", 'r', 0, G_OPTION_ARG_STRING, &raw_type_name, "Raw data type (adc-14le, adc-16le, adc-24le, float)", NULL },
        { "amp-type", 'a', 0, G_OPTION_ARG_STRING, &amp_type_name, "amplitude data type (float, amp-i8, amp-i16, amp-i32, amp-f8, amp-f16)", NULL },
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

    if ((g_strv_length (args) != 2))
      {
        g_print ("%s", g_option_context_get_help (context, FALSE, NULL));
        return 0;
      }

    g_option_context_free (context);

    db_uri = g_strdup (args[1]);
    g_strfreev (args);
  }

  /* Входные параметры. */
  if ((discretization < 10.0) || (discretization > 10000000.0))
    g_error ("the discretization must be within 10Hz to 10Mhz");

  if ((frequency < 1.0) || (frequency > 1000000.0))
    g_error ("the signal frequency must be within 1Hz to 1Mhz");

  if ((duration < 1e-4) || (duration > 0.1))
    g_error ("the signal duration must be within 100us to 1ms");

  if ((n_signals < 1) || (n_signals > 100))
    g_error ("the number of signals must be within 1 to 100");

  if ((n_lines < 1) || (n_lines > 100))
    g_error ("the number of lines must be within 1 to 100");

  if ((n_tvgs < 1) || (n_tvgs > 100))
    g_error ("the number of tvgs must be within 1 to 100");

  /* Формат записи данных. */
  raw_type = HYSCAN_DATA_INVALID;
  amp_type = HYSCAN_DATA_INVALID;
  for (i = 0; i < sizeof (raw_test_types) / sizeof (test_info); i++)
    if (g_strcmp0 (raw_type_name, raw_test_types[i].name) == 0)
      {
        raw_type = raw_test_types[i].type;
        raw_error = raw_test_types[i].error;
      }
  for (i = 0; i < sizeof (amp_test_types) / sizeof (test_info); i++)
    if (g_strcmp0 (amp_type_name, amp_test_types[i].name) == 0)
      {
        amp_type = amp_test_types[i].type;
        amp_error = amp_test_types[i].error;
      }

  if (raw_type == HYSCAN_DATA_INVALID)
    g_error ("unsupported raw type %s", raw_type_name);
  if (amp_type == HYSCAN_DATA_INVALID)
    g_error ("unsupported amplitude type %s", amp_type_name);

  /* Параметры данных. */
  position.x = 0.0;
  position.y = 0.0;
  position.z = 0.0;
  position.psi = 0.0;
  position.gamma = 0.0;
  position.theta = 0.0;

  raw_info.data_type = raw_type;
  raw_info.data_rate = discretization;
  raw_info.antenna_voffset = 0.0;
  raw_info.antenna_hoffset = 0.0;
  raw_info.antenna_vpattern = 40.0;
  raw_info.antenna_hpattern = 2.0;
  raw_info.antenna_frequency = frequency;
  raw_info.antenna_bandwidth = 0.1 * frequency;
  raw_info.adc_vref = 1.0;
  raw_info.adc_offset = 0;

  acoustic_info.data_type = amp_type;
  acoustic_info.data_rate = discretization;
  acoustic_info.antenna_vpattern = 40.0;
  acoustic_info.antenna_hpattern = 2.0;

  /* Буферы данных. */
  signal_buffer = hyscan_buffer_new ();
  tvg_buffer = hyscan_buffer_new ();
  cplx_buffer = hyscan_buffer_new ();
  amp_buffer = hyscan_buffer_new ();
  channel_buffer = hyscan_buffer_new ();

  hyscan_buffer_set_data_type (signal_buffer, HYSCAN_DATA_COMPLEX_FLOAT);
  hyscan_buffer_set_data_type (tvg_buffer, HYSCAN_DATA_FLOAT);
  hyscan_buffer_set_data_type (cplx_buffer, HYSCAN_DATA_COMPLEX_FLOAT);
  hyscan_buffer_set_data_type (amp_buffer, HYSCAN_DATA_FLOAT);

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

  /* Создаём галс. */
  if (!hyscan_data_writer_start (writer, PROJECT_NAME, TRACK_NAME, HYSCAN_TRACK_SURVEY))
    g_error( "can't start write");

  /* Тестовые данные для проверки свёртки. Массив размером 100 * signal_size.
   * Сигнал располагается со смещением в две длительности. Все остальные индексы
   * массива заполнены нулями. Используется тональный сигнал. */
  g_message ("Data generation");
  n_signal_points = discretization * duration;
  n_data_points = 100 * n_signal_points;
  for (i = 0; i < n_lines * n_signals; i++)
    {
      gint64 index_time;
      guint tvg_index;
      guint signal_index;
      gdouble work_frequency;

      HyScanComplexFloat *raw_values;
      gfloat *amp_values;

      index_time = 1000 * (i + 1);
      tvg_index = (i / n_tvgs);
      signal_index = (i / n_lines);
      work_frequency = (frequency - ((signal_index * frequency) / (5.0 * n_signals)));

      /* Записываем образ сигнала каждые n_lines строк. */
      if ((i % n_lines) == 0)
        {
          HyScanComplexFloat *signal_image;

          hyscan_buffer_set_size (signal_buffer, n_signal_points * sizeof (HyScanComplexFloat));
          signal_image = hyscan_buffer_get_complex_float (signal_buffer, &n_signal_points);
          for (j = 0; j < n_signal_points; j++)
            {
              gdouble time = (1.0 / discretization) * j;
              gdouble phase = 2.0 * G_PI * work_frequency * time;

              signal_image[j].re = cos (phase);
              signal_image[j].im = sin (phase);
            }

          if (!hyscan_data_writer_raw_add_signal (writer, HYSCAN_SOURCE_SIDE_SCAN_STARBOARD, index_time, signal_buffer))
            g_error ("can't add signal image");
        }

      /* Записываем "кривую" ВАРУ каждые n_tvgs строк. */
      if ((i % n_tvgs) == 0)
        {
          gfloat *tvg_values;

          hyscan_buffer_set_size (tvg_buffer, n_data_points * sizeof (gfloat));
          tvg_values = hyscan_buffer_get_float (tvg_buffer, &n_data_points);
          for (j = 0; j < n_data_points; j++)
            tvg_values[j] = tvg_index + (((gdouble)tvg_index / (gdouble)n_data_points) * j);

          if (!hyscan_data_writer_raw_add_tvg (writer, HYSCAN_SOURCE_SIDE_SCAN_STARBOARD, 1, index_time, tvg_buffer))
            g_error ("can't add tvg");
        }

      /* Записываем сырые данные. */
      hyscan_buffer_set_size (cplx_buffer, n_data_points * sizeof (HyScanComplexFloat));
      raw_values = hyscan_buffer_get_complex_float (cplx_buffer, &n_data_points);
      for (j = 2 * n_signal_points; j < 3 * n_signal_points; j++)
        {
          gdouble time = (1.0 / discretization) * (j - 2 * n_signal_points);
          gdouble phase = 2.0 * G_PI * work_frequency * time;

          raw_values[j].re = cos (phase);
          raw_values[j].im = sin (phase);
        }

      if (!hyscan_buffer_export_data (cplx_buffer, channel_buffer, raw_type))
        g_error ("can't export complex data");

      if (noise)
        {
          if (!hyscan_data_writer_raw_add_noise (writer, HYSCAN_SOURCE_SIDE_SCAN_STARBOARD, 1, index_time, &raw_info, channel_buffer))
            g_error ("can't add noise data");
        }
      else
        {
          if (!hyscan_data_writer_raw_add_data (writer, HYSCAN_SOURCE_SIDE_SCAN_STARBOARD, 1, index_time, &raw_info, channel_buffer))
            g_error ("can't add raw data");
        }

      /* Записываем амплитуду. */
      hyscan_buffer_set_size (amp_buffer, n_data_points * sizeof (gfloat));
      amp_values = hyscan_buffer_get_float (amp_buffer, &n_data_points);
      for (j = n_signal_points; j < 3 * n_signal_points; j++)
        {
          if ((j >= n_signal_points) && (j < 2 * n_signal_points))
            amp_values[j] = ((gdouble) (j - n_signal_points) / n_signal_points);
          if ((j >= 2 * n_signal_points) && (j < 3 * n_signal_points))
            amp_values[j] = (1.0 - (gdouble) (j - 2 * n_signal_points) / n_signal_points);
        }

      if (!hyscan_buffer_export_data (amp_buffer, channel_buffer, amp_type))
        g_error ("can't export amplitude data");

      if (!hyscan_data_writer_acoustic_add_data (writer, HYSCAN_SOURCE_SIDE_SCAN_STARBOARD, index_time, &acoustic_info, channel_buffer))
        g_error ("can't add acoustic data");
    }

  /* Завершаем запись. */
  g_clear_object (&writer);

  /* Допустимые ошибки. */
  amp_error *= n_data_points;
  raw_error *= n_data_points;

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
  n_signal_points = discretization * duration;
  n_data_points = 100 * n_signal_points;
  for (k = 0; k < 2; k++)
    {
      GTimer *timer = g_timer_new ();
      gdouble elapsed = 0.0;

      if (k == 0)
        g_message ("Data check");
      else
        g_message ("Cached data check");

      for (i = 0; i < n_lines * n_signals; i++)
        {
          const HyScanComplexFloat *signal_image;
          const HyScanComplexFloat *quadratures;
          const gfloat *tvg_values;
          const gfloat *amplitudes;

          guint tvg_index;
          guint signal_index;
          gdouble work_frequency;
          gdouble amp_diff;
          guint32 io_size;

          tvg_index = (i / n_tvgs);
          signal_index = (i / n_lines);
          work_frequency = (frequency - ((signal_index * frequency) / (5.0 * n_signals)));

          /* Проверяем образ сигнала. */
          g_timer_start (timer);
          signal_image = hyscan_raw_data_get_signal_image (raw_reader, i, &io_size, NULL);
          elapsed += g_timer_elapsed (timer, NULL);
          if ((signal_image == NULL)|| (io_size != n_signal_points))
            g_error ("can't get signal image");

          for (j = 0; j < n_signal_points; j++)
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
          g_timer_start (timer);
          tvg_values = hyscan_raw_data_get_tvg_values (raw_reader, i, &io_size, NULL);
          elapsed += g_timer_elapsed (timer, NULL);
          if (tvg_values == NULL)
            g_error ("can't get tvg values");

          if (io_size != n_data_points)
            g_error ("tvg size mismatch");

          for (j = 0; j < n_data_points; j++)
            if (fabs (tvg_values[j] - (tvg_index + (((gdouble)tvg_index / (gdouble)n_data_points)) * j)) > 1e-5)
              g_error ("tvg error");

          /* Проверяем амплитуду акустических данных. */
          g_timer_start (timer);
          amplitudes = hyscan_acoustic_data_get_values (acoustic_reader, i, &io_size, NULL);
          elapsed += g_timer_elapsed (timer, NULL);
          if (amplitudes == NULL)
            g_error ("can't get acoustic amplitude");

          if (io_size != n_data_points)
            g_error ("acoustic amplitude size mismatch");

          amp_diff = 0.0;
          for (j = 0; j < n_data_points; j++)
            {
              gdouble amplitude = 0.0;

              if ((j >= n_signal_points) && (j < 2 * n_signal_points))
                amplitude = ((gdouble) (j - n_signal_points) / n_signal_points);
              if ((j >= 2 * n_signal_points) && (j < 3 * n_signal_points))
                amplitude = (1.0 - (gdouble) (j - 2 * n_signal_points) / n_signal_points);

              amp_diff += fabs (amplitudes[j] - amplitude);
            }

          if (amp_diff > amp_error)
            g_error ("acoustic amplitudes error");

          /* Проверяем амплитуду сырых данных. */
          g_timer_start (timer);
          amplitudes = hyscan_raw_data_get_amplitude_values (raw_reader, i, &io_size, NULL);
          elapsed += g_timer_elapsed (timer, NULL);
          if (amplitudes == NULL)
            g_error ("can't get raw amplitudes");

          if (io_size != n_data_points)
            g_error ("raw amplitudes size mismatch");

          amp_diff = 0.0;
          for (j = 0; j < n_data_points; j++)
            {
              gdouble amplitude = 0.0;

              if ((j >= n_signal_points) && (j < 2 * n_signal_points))
                amplitude = ((gdouble) (j - n_signal_points) / n_signal_points);
              if ((j >= 2 * n_signal_points) && (j < 3 * n_signal_points))
                amplitude = (1.0 - (gdouble) (j - 2 * n_signal_points) / n_signal_points);

              amp_diff += fabs (amplitudes[j] - amplitude);
            }

          if (amp_diff > raw_error)
            g_error ("raw amplitudes error");

          /* Проверяем квадратурные значения. */
          g_timer_start (timer);
          quadratures = hyscan_raw_data_get_quadrature_values (raw_reader, i, &io_size, NULL);
          elapsed += g_timer_elapsed (timer, NULL);

          if (quadratures == NULL)
            g_error ("can't get raw quadratures");

          if (io_size != n_data_points)
            g_error ("raw quadratures size mismatch");

          amp_diff = 0.0;
          for (j = 0; j < n_data_points; j++)
            {
              gdouble amplitude = 0.0;
              gfloat re = quadratures[j].re;
              gfloat im = quadratures[j].im;

              if ((j >= n_signal_points) && (j < 2 * n_signal_points))
                amplitude = ((gdouble) (j - n_signal_points) / n_signal_points);
              if ((j >= 2 * n_signal_points) && (j < 3 * n_signal_points))
                amplitude = (1.0 - (gdouble) (j - 2 * n_signal_points) / n_signal_points);

              amp_diff += fabs (sqrtf (re * re + im * im) - amplitude);
            }

          if (amp_diff > raw_error)
            g_error ("raw quadratures error");
        }

      g_message ("Elapsed %.6fs", elapsed);
      g_timer_destroy (timer);

      if (cache == NULL)
        break;
    }

  g_message ("All done");

  g_clear_object (&raw_reader);
  g_clear_object (&acoustic_reader);

  hyscan_db_project_remove (db, PROJECT_NAME);

  g_clear_object (&db);
  g_clear_object (&cache);

  g_object_unref (signal_buffer);
  g_object_unref (tvg_buffer);
  g_object_unref (cplx_buffer);
  g_object_unref (amp_buffer);
  g_object_unref (channel_buffer);

  g_free (raw_type_name);
  g_free (amp_type_name);
  g_free (db_uri);

  xmlCleanupParser ();

  return 0;
}
