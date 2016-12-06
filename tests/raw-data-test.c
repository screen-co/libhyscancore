
#include <hyscan-data-writer.h>
#include <hyscan-raw-data.h>
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
  guint cache_size = 0;           /* Размер кэша, Мб. */

  HyScanDB *db;
  HyScanCache *cache = NULL;
  HyScanDataWriter *writer;
  HyScanRawData *reader;

  HyScanAntennaPosition position;
  HyScanRawDataInfo info;
  HyScanDataWriterData data;
  HyScanDataWriterSignal signal;

  gboolean status;

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
        g_message (error->message);
        return -1;
      }

    if (g_strv_length (args) != 2 || discretization < 1.0 || frequency < 1.0 || duration < 1e-7 ||
        n_signals < 1 || n_signals > 100 || n_lines < 1 || n_lines > 100)
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
  info.data.type = HYSCAN_DATA_COMPLEX_ADC_16LE;
  info.data.rate = discretization;
  info.antenna.offset.vertical = 0.0;
  info.antenna.offset.horizontal = 0.0;
  info.antenna.pattern.vertical = 40.0;
  info.antenna.pattern.horizontal = 2.0;
  info.adc.vref = 1.0;
  info.adc.offset = 0;

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
  if (!hyscan_data_writer_sonar_set_position (writer, HYSCAN_SOURCE_SIDE_SCAN_STARBOARD, &position))
    g_error ("can't set antenna position");

  /* Создаём галс. */
  if (!hyscan_data_writer_start (writer, PROJECT_NAME, TRACK_NAME, HYSCAN_TRACK_SURVEY))
    g_error( "can't start write");

  /* Тестовые данные для проверки свёртки. Массив размером 100 * signal_size.
     Сигнал располагается со смещением в две длительности. Все остальные индексы
     массива заполнены нулями. Используется тональный сигнал. */
  {
    guint32 signal_size = discretization * duration;
    guint32 data_size = 100 * signal_size;
    HyScanComplexFloat *signal_points;
    guint16 *data_values;
    guint i, j;

    signal_points = g_malloc (signal_size * sizeof (HyScanComplexFloat));
    data_values = g_malloc (2 * data_size * sizeof (guint16));

    g_message ("signal size = %d", signal_size);
    g_message ("data size = %d", data_size);

    for (j = 0; j < n_signals; j++)
      {
        gdouble work_frequency = (frequency - ((j * frequency) / (5.0 * n_signals)));

        memset (signal_points, 0, 2 * signal_size * sizeof(gfloat));
        for (i = 0; i < signal_size; i++)
          {
            gdouble time = (1.0 / discretization) * i;
            gdouble phase = 2.0 * G_PI * work_frequency * time;
            signal_points[i].re = cos (phase);
            signal_points[i].im = sin (phase);
          }

        signal.time = 1000 * (j + 1);
        signal.rate = info.data.rate;
        signal.n_points = signal_size;
        signal.points = signal_points;

        status = hyscan_data_writer_raw_add_signal (writer, HYSCAN_SOURCE_SIDE_SCAN_STARBOARD, &signal);
        if (!status)
          g_error ("can't add signal image");

        for (i = 0; i < 2 * data_size; i++)
          data_values[i] = 32767;

        for (i = 2 * signal_size; i < 3 * signal_size; i++)
          {
            gdouble time = (1.0 / discretization) * i;
            gdouble phase = 2.0 * G_PI * work_frequency * time;
            data_values[2 * i] = 65535.0 * ((0.5 * cos (phase)) + 0.5);
            data_values[2 * i + 1] = 65535.0 * ((0.5 * sin (phase)) + 0.5);
          }

        for (i = 0; i < n_lines; i++)
          {
            data.time = 1000 * (j + 1) + (i * 10);
            data.size = 2 * data_size * sizeof(gint16);
            data.data = data_values;

            status = hyscan_data_writer_raw_add_data (writer, HYSCAN_SOURCE_SIDE_SCAN_STARBOARD, 1,
                                                      &info, &data);
            if (!status)
              g_error ("can't add data");
          }

      }

    g_free (signal_points);
    g_free (data_values);
  }

  /* Объект чтения данных. */
  reader = hyscan_raw_data_new_with_cache (db, PROJECT_NAME, TRACK_NAME,
                                           hyscan_channel_get_name_by_types (HYSCAN_SOURCE_SIDE_SCAN_STARBOARD, TRUE, 1),
                                           cache);

  /* Для тонального сигнала проверяем, что его свёртка совпадает с треугольником,
     начинающимся с signal_size, пиком на 2 * signal_size и спадающим до 3 * signal_size. */
  {

    guint32 signal_size = discretization * duration;
    guint32 data_size = 100 * signal_size;
    guint32 readings;

    gfloat *amp1;
    gfloat *amp2;
    gdouble delta;

    guint i, j;

    amp1 = g_malloc0 (data_size * sizeof(gfloat));
    amp2 = g_malloc0 (data_size * sizeof(gfloat));

    /* Аналитический вид амплитуды свёртки. */
    for (i = signal_size, j = 0; j < signal_size; i++, j++)
      amp1[i] = (gfloat) j / signal_size;
    for (i = 2 * signal_size, j = 0; j < signal_size; i++, j++)
      amp1[i] = 1.0 - (gfloat) j / signal_size;

    /* Проверяем вид свёртки. */
    delta = 0.0;
    for (j = 0; j < n_signals * n_lines; j++)
      {
        readings = data_size;
        if (!hyscan_raw_data_get_amplitude_values (reader, j, amp2, &readings, NULL))
          g_error ("can't get amplitude");

        for (i = 0; i < data_size; i++)
          delta += fabs (amp1[i] - amp2[i]);
      }
    g_message ("amplitude error = %f", delta / (n_signals * n_lines * data_size));

    /* Проверяем работу кэша. */
    if (cache != NULL)
      {
        delta = 0.0;
        for (j = 0; j < n_signals * n_lines; j++)
          {
            readings = data_size;
            if (!hyscan_raw_data_get_amplitude_values (reader, j, amp2, &readings, NULL))
              g_error ("can't get amplitude");

            for (i = 0; i < data_size; i++)
              delta += fabs (amp1[i] - amp2[i]);
          }
        g_message ("amplitude error = %f from cache", delta / (n_signals * n_lines * data_size));
      }

    g_free (amp1);
    g_free (amp2);
  }

  g_clear_object (&writer);
  g_clear_object (&reader);

  hyscan_db_project_remove (db, PROJECT_NAME);

  g_clear_object (&db);
  g_clear_object (&cache);

  g_free (db_uri);

  xmlCleanupParser ();

  return 0;
}
