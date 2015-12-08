#include <hyscan-data-channel.h>
#include <hyscan-db-file.h>
#include <hyscan-cached.h>

#include <glib/gstdio.h>
#include <string.h>
#include <math.h>

int main( int argc, char **argv )
{
  gchar *db_path = NULL;          /* Путь к каталогу с базой данных. */
  gdouble frequency = 0.0;        /* Частота сигнала. */
  gdouble duration = 0.0;         /* Длительность сигнала. */
  gdouble discretization = 0.0;   /* Частота дискретизации. */
  gint n_signals = 10;            /* Число сигналов. */
  gint n_lines = 10;              /* Число строк для каждого сигнала. */
  gint cache_size = 0;            /* Размер кэша, Мб. */

  HyScanDB *db;
  HyScanCache *cache = NULL;
  HyScanDataChannel *reader;
  HyScanDataChannel *writer;

  gboolean status;

  gint32 project_id;
  gint32 track_id;

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

    context = g_option_context_new ("<db-path>");
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

    db_path = g_strdup (args[1]);
    g_strfreev (args);
  }

  /* Создаём каталог для базы данных. */
  {
      GDir *dir;

      if (g_mkdir_with_parents (db_path, 0777) != 0)
        g_error( "can't create directory '%s'", db_path);

      dir = g_dir_open (db_path, 0, NULL);
      if (dir == NULL)
        g_error( "can't open directory '%s'", db_path);

      if (g_dir_read_name (dir) != NULL)
        g_error( "db directory '%s' must be empty", db_path);

      g_dir_close (dir);
  }

  /* Открываем базу данных. */
  db = g_object_new (HYSCAN_TYPE_DB_FILE, "path", db_path, NULL);

  /* Кэш данных */
  if (cache_size)
    cache = HYSCAN_CACHE (hyscan_cached_new (cache_size));

  /* Создаём проект. */
  project_id = hyscan_db_create_project (db, "project", NULL);
  if (project_id < 0)
    g_error( "can't create project");

  /* Создаём галс. */
  track_id = hyscan_db_create_track (db, project_id, "track");
  if (track_id < 0)
    g_error( "can't create track");

  /* Объекты обработки данных */
  reader = hyscan_data_channel_new (db, cache, NULL);
  writer = hyscan_data_channel_new (db, cache, NULL);

  /* Создаём канал данных. */
  status = hyscan_data_channel_create (writer, "project", "track", "channel",
                                       HYSCAN_DATA_TYPE_COMPLEX_ADC_16BIT,
                                       discretization);
  if (!status)
    g_error ("can't create data channel");

  /* Открываем созданный канал данных. */
  status = hyscan_data_channel_open (reader, "project", "track", "channel");
  if (!status)
    g_error ("can't create data channel");

  /* Тестовые данные для проверки свёртки. Массив размером 100 * signal_size.
     Сигнал располагается со смещением в две длительности. Все остальные индексы
     массива заполнены нулями. Используется тональный сигнал. */
  {
    gint32 signal_size = discretization * duration;
    gint32 data_size = 100 * signal_size;
    HyScanComplexFloat *signal;
    gint16 *data;
    gint i, j;

    signal = g_malloc (signal_size * sizeof (HyScanComplexFloat));
    data = g_malloc (2 * data_size * sizeof (gint16));

    g_message ("signal size = %d", signal_size);
    g_message ("data size = %d", data_size);

    for (j = 0; j < n_signals; j++)
      {
        gdouble work_frequency = (frequency - ((j * frequency) / (5.0 * n_signals)));

        memset (signal, 0, 2 * signal_size * sizeof(gfloat));
        for (i = 0; i < signal_size; i++)
            {
              gdouble time = (1.0 / discretization) * i;
              gdouble phase = 2.0 * G_PI * work_frequency * time;
              signal[i].re = cos (phase);
              signal[i].im = sin (phase);
            }

        hyscan_data_channel_add_signal_image (writer, 1000 * (j + 1), signal, signal_size);

        memset (data, 0, 2 * data_size * sizeof(gint16));
        for (i = 2 * signal_size; i < 3 * signal_size; i++)
          {
            gdouble time = (1.0 / discretization) * i;
            gdouble phase = 2.0 * G_PI * work_frequency * time;
            data[2 * i] = 32767.0 * cos (phase);
            data[2 * i + 1] = 32767.0 * sin (phase);
          }

        for (i = 0; i < n_lines; i++)
          hyscan_data_channel_add_data (writer, 1000 * (j + 1) + (i * 10), data, 2 * data_size * sizeof(gint16), NULL);

      }

    g_free (signal);
    g_free (data);
  }

  /* Для тонального сигнала проверяем, что его свёртка совпадает с треугольником,
     начинающимся с signal_size, пиком на 2 * signal_size и спадающим до 3 * signal_size. */
  {

    gint32 signal_size = discretization * duration;
    gint32 data_size = 100 * signal_size;
    gint32 readings;

    gfloat *amp1;
    gfloat *amp2;
    gdouble delta;

    gint i, j;

    amp1 = g_malloc (data_size * sizeof(gfloat));
    amp2 = g_malloc (data_size * sizeof(gfloat));

    /* Аналитический вид амплитуды свёртки. */
    memset (amp1, 0, data_size * sizeof(gfloat));
    for (i = signal_size, j = 0; j < signal_size; i++, j++)
      amp1[i] = (gfloat) j / signal_size;
    for (i = 2 * signal_size, j = 0; j < signal_size; i++, j++)
      amp1[i] = 1.0 - (gfloat) j / signal_size;

    /* Проверяем вид свёртки в объекте записи данных. */
    delta = 0.0;
    for (j = 0; j < n_signals * n_lines; j++)
      {
        readings = data_size;
        hyscan_data_channel_get_amplitude_values (writer, j, amp2, &readings, NULL);
        for (i = 0; i < data_size; i++)
          delta += fabs (amp1[i] - amp2[i]);
      }
    g_message ("writer channel mean amplitude error = %f", delta / (j * data_size));

    /* Проверяем вид свёртки в объекте чтения данных. */
    delta = 0.0;
    for (j = 0; j < n_signals * n_lines; j++)
      {
        readings = data_size;
        hyscan_data_channel_get_amplitude_values (reader, j, amp2, &readings, NULL);
        for (i = 0; i < data_size; i++)
          delta += fabs (amp1[i] - amp2[i]);
      }
    g_message ("reader channel mean amplitude error = %f", delta / ( j * data_size ));
  }

  /* Закрываем каналы данных. */
  g_clear_object (&writer);
  g_clear_object (&reader);

  /* Закрываем галс и проект. */
  hyscan_db_close_project (db, project_id);
  hyscan_db_close_track (db, track_id);

  /* Удаляем проект. */
  hyscan_db_remove_project (db, "project");

  /* Закрываем базу данных. */
  g_clear_object (&db);

  /* Удаляем кэш. */
  g_clear_object (&cache);

  /* Удаляем каталог проектов. */
  if (g_rmdir (db_path) != 0)
    g_error ("can't delete directory '%s'", db_path);

  return 0;

}
