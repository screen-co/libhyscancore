#include <hyscan-data-channel.h>
#include <hyscan-seabed-echosounder.h>
#include <hyscan-seabed-sonar.h>
#include <hyscan-seabed.h>
#include <hyscan-db-file.h>
#include <hyscan-cached.h>
#include <hyscan-types.h>

#include <hyscan-core-exports.h>
#include <glib/gstdio.h>
#include <string.h>

#include <time.h>
#include <stdlib.h>

#define KGRN "\x1b[32;1m"
#define KRED "\x1b[31;22m"
#define KNRM "\x1b[0m"
#define KIT  "\x1b[3m"

int
main (int argc, char **argv)
{
  gchar *db_uri = NULL; /* Путь к каталогу с базой данных. */
  gint cache_size = 0; /* Размер кэша, Мб. */
  int i = 0, j = 0;
  HyScanDB *db;
  HyScanCache *cache = NULL;
  HyScanDataChannel *datachan;
  HyScanDataChannel *writer;
  HyScanSeabed *seabed_echo;
  HyScanSeabed *seabed_sonar;

  srand (time (NULL));
  gboolean status;

  gint32 project_id;
  gint32 track_id;

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
  project_id = hyscan_db_create_project (db, "t_project", NULL);
  if (project_id < 0)
    g_error ("can't create project");

  /* Создаём галс. */
  track_id = hyscan_db_create_track (db, project_id, "t_track");
  if (track_id < 0)
    g_error ("can't create track");

  /* Объекты обработки данных */
  datachan = hyscan_data_channel_new (db, cache, NULL);
  writer = hyscan_data_channel_new (db, cache, NULL);

  /* Создаём канал данных.
   * 750 - частота дискретизации, чтобы расстояние в метрах соответствовало расстоянию в дискретах
   */
  status = hyscan_data_channel_create (writer, "t_project", "t_track",
				       "t_channel",
				       HYSCAN_DATA_TYPE_COMPLEX_ADC_16BIT, 750);
  if (!status)
    g_error ("can't create data channel");

  /* Открываем созданный канал данных. */
  status = hyscan_data_channel_open (datachan, "t_project", "t_track",
				     "t_channel");
  if (!status)
    g_error ("can't open data channel");

  /* Тестовые данные для проверки алгоритма. Массив размером 1 * 5000.
   *
   *	0-99    - чистый сигнал от номера индекса длительностью 10+индекс
   *  100-199 - + шум
   *  200-299 - бывший сигнал стал засветкой, сигнал от 1000 до 1050
   *  таким образом, сигнал должен определяться при индексах <40
   */
  gint16 *data;
  gint32 data_size = 5000;
  gint32 lines = 100;
  data = g_malloc (2 * data_size * sizeof(gint16));
  for (j = 0; j < lines; j++)
  {
    memset (data, 0, 2 * data_size * sizeof(gint16));

    /* Сигнал */
    for (i = j; i < j + j + 10; i++)
      data[i * 2] = 32767;

    hyscan_data_channel_add_data (writer, 1000 * (j + 1), data,
				  data_size * sizeof(gfloat), NULL);
  }

  for (j = 0; j < lines; j++)
  {
    memset (data, 0, 2 * data_size * sizeof(gint16));

    /* Шум */
    for (i = 0; i < data_size; i++)
      data[i * 2] = (1024 * (1 + j % 6)) + (rand () % 128);

    /* Сигнал */
    for (i = j; i < j + j + 10; i++)
      data[i * 2] = 32767;

    hyscan_data_channel_add_data (writer, 1000 * (j + lines + 1), data,
				  data_size * sizeof(gfloat), NULL);
  }

  for (j = 0; j < lines; j++)
  {
    memset (data, 0, 2 * data_size * sizeof(gint16));

    /* Шум */
    for (i = 0; i < data_size; i++)
      //data[i*2] = rand()%(1024*(1+j%10));//+(rand()%128);
      data[i * 2] = (1024 * (1 + j % 10)) + (rand () % 128);

    /* Сигнал */
    for (i = 1000; i < 1050; i++)
      data[i * 2] = 32767;

    /* Засветка */
    for (i = j; i < j + j + 10; i++)
      data[i * 2] = 32767;

    hyscan_data_channel_add_data (writer, 1000 * (j + 2 * lines + 1), data,
				  data_size * sizeof(gfloat), NULL);
  }
  g_clear_object (&writer);

  g_free (data);
  g_printf (KNRM "data add ok\ntime=1000*(index+1)\n");

  /* Задаем таблицу глубин*/

  SoundSpeedTable sst;
  sst.depth = 0;
  sst.soundspeed = 1500;

  GArray *sst1, *sst2;
  sst1 = g_array_new (FALSE, FALSE, sizeof(SoundSpeedTable));
  sst2 = g_array_new (FALSE, FALSE, sizeof(SoundSpeedTable));
  g_array_append_val (sst1, sst);
  g_array_append_val (sst2, sst);
  sst.depth = 20;
  sst.soundspeed = 100;
  g_array_append_val (sst2, sst);
  /* Начинаем определять глубину.
   * Формат вывода следующий:
   * - индекс
   * - глубина по эхолоту
   * - ICE - by Index Cache Error - ошибка, если значение в кэше не совпадает с измеренным
   * - глубина по ГБО
   * - ICE
   */

  seabed_echo = HYSCAN_SEABED(hyscan_seabed_echosounder_new (db, cache, "echocash", "t_project", "t_track", "t_channel", 0));
  seabed_sonar = HYSCAN_SEABED(hyscan_seabed_sonar_new (db, cache, "sonarcash", "t_project", "t_track","t_channel", 0));

  hyscan_seabed_set_soundspeed (seabed_echo, sst1);
  hyscan_seabed_set_soundspeed (seabed_sonar, sst2);
  gfloat depth;
  gfloat depth2;
  g_printf ("format: " KGRN "index:" KRED "depth" KIT "(echo)|" KNRM KRED "depth" KIT"(sonar) \n" KNRM);
  for (i = 0; i < lines * 3; i++)
  {
    depth = hyscan_seabed_get_depth_by_index (seabed_echo, i);
    g_printf (KGRN"%3i: " KRED "%6.2f", i, depth);

    depth2 = hyscan_seabed_get_depth_by_index (seabed_echo, i);
    if (depth != depth2)
      g_printf (KNRM "|ICE" KRED);

    depth = hyscan_seabed_get_depth_by_index (seabed_sonar, i);
    g_printf ("|%6.2f", depth);

    depth2 = hyscan_seabed_get_depth_by_index (seabed_sonar, i);
    if (depth != depth2)
      g_printf (KNRM "|ICE" KRED);

    g_printf ("\b\t");

    /* Выводим по 5 значений на строку */
    if (i != 0 && (i + 1) % 5 == 0)
      g_printf ("\n");
    if (i != 0 && (i + 1) % 100 == 0)
      g_printf ("\n");
  }

  g_printf ("\n");

  /* Закрываем каналы данных. */
  g_clear_object (&datachan);
  g_clear_object (&writer);

  /* Закрываем галс и проект. */
  hyscan_db_close_project (db, project_id);
  hyscan_db_close_track (db, track_id);

  /* Удаляем проект. */
  hyscan_db_remove_project (db, "t_project");
  g_printf ("data remove ok\n");
  /* Закрываем базу данных. */
  g_clear_object (&db);

  /* Удаляем кэш. */
  g_clear_object (&cache);

  return 0;

}
