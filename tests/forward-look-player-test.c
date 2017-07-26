
#include <hyscan-forward-look-player.h>
#include <hyscan-cached.h>

#include <libxml/parser.h>
#include <math.h>

#include "hyscan-fl-gen.h"

#define PROJECT_NAME           "test"
#define STATIC_TRACK_NAME      "static"
#define DYNAMIC_TRACK_NAME     "dynamic"

enum {
  CONTROL_REAL_TIME_TEST,
  CONTROL_NORMAL_PLAY_TEST,
  CONTROL_REWIND_PLAY_TEST,
  CONTROL_SEEK_TEST,
  CONTROL_END_TEST
};

gchar                         *db_uri = NULL;
guint                          n_lines = 100;
guint                          n_points = 100;
guint                          n_fps = 100;
guint                          n_rate = 10;
guint                          cache_size = 0;

HyScanDB                      *db;
HyScanCache                   *cache;
HyScanForwardLookPlayer       *player;
HyScanFLGen                   *generator;

GMainLoop                     *loop;
GTimer                        *timer;

guint32                        check_first_index = 0;
guint32                        check_last_index = 0;
guint32                        check_index = 0;

gboolean                       range_checked = TRUE;
gboolean                       data_checked = TRUE;

/* Функция тестирования. */
gboolean
control_test (gpointer user_data)
{
  static guint test_step = CONTROL_REAL_TIME_TEST;
  static guint step_cnt = 0;
  static gdouble play_speed = 1.0;

  /* Проверка таймаута. */
  if (g_timer_elapsed (timer, NULL) > 1.0)
    g_error ("timeout");

  /* Проверка завершения шага. */
  if (!range_checked || !data_checked)
    return TRUE;

  /* Тест воспроизведения в режиме реального времени. */
  if (test_step == CONTROL_REAL_TIME_TEST)
    {
      if (step_cnt == 0)
        {
          g_message ("Real time test");

          /* Записываем новый галс. */
          if (!hyscan_fl_gen_set_track (generator, db, PROJECT_NAME, DYNAMIC_TRACK_NAME))
            g_error ("can't start track %s", DYNAMIC_TRACK_NAME);

          hyscan_forward_look_player_open (player, db, PROJECT_NAME, DYNAMIC_TRACK_NAME, TRUE);
          hyscan_forward_look_player_real_time (player);
        }

      /* На каждом шаге записываем одну строку тестовых данных. */
      if (!hyscan_fl_gen_generate (generator, (G_USEC_PER_SEC / n_rate) * step_cnt, n_points + step_cnt))
        g_error ("can't add data");

      /* На каждом шаге проверяем диапазон индексов данных и текущий индекс данных. */
      check_first_index = 0;
      check_last_index = step_cnt;
      check_index = step_cnt;
      range_checked = FALSE;
      data_checked = FALSE;
    }

  /* Тест прямого воспроизведения на скорости 2x. */
  else if (test_step == CONTROL_NORMAL_PLAY_TEST)
    {
      if (step_cnt == 0)
        {
          g_message ("Play test");

          /* Открываем предварительно записанный галс и включаем его воспроизведение. */
          play_speed = 2.0;
          hyscan_forward_look_player_open (player, db, PROJECT_NAME, STATIC_TRACK_NAME, TRUE);
          hyscan_forward_look_player_play (player, play_speed);

          /* Один раз необходимо проверить диапазон индексов данных. */
          check_first_index = 0;
          check_last_index = n_lines - 1;
          range_checked = FALSE;
        }

      /* В середине пути ставим на короткую паузу для проверки продолжения воспроизведения. */
      if (step_cnt == (n_lines / 2))
        {
          g_message ("Pause test");
          hyscan_forward_look_player_pause (player);
          hyscan_forward_look_player_play (player, play_speed);
          g_message ("Play test");
        }

      /* На каждом шаге проверяем текущий индекс данных. */
      check_index = step_cnt;
      data_checked = FALSE;
    }

  /* Тест обратного воспроизведения на скорости 0.5x. */
  else if (test_step == CONTROL_REWIND_PLAY_TEST)
    {
      if (step_cnt == 0)
        {
          g_message ("Rewind play test");

          /* Запускаем воспроизведение в обратную сторону. */
          play_speed = -0.5;
          hyscan_forward_look_player_play (player, play_speed);
        }

      /* На каждом шаге проверяем текущий индекс данных. */
      check_index = n_lines - step_cnt - 1;
      data_checked = FALSE;
    }

  /* Тест перемотки. */
  else if (test_step == CONTROL_SEEK_TEST)
    {
      if (step_cnt == 0)
        {
          g_message ("Seek test");

          /* Тест перемотки проводится в режиме паузы. */
          hyscan_forward_look_player_pause (player);
        }

      /* Устанавливаем новую позицию и проверяем её. */
      check_index = n_lines - step_cnt - 1;
      hyscan_forward_look_player_seek (player, check_index);
      data_checked = FALSE;
    }

  /* Завершение тестирования. */
  else
    {
      g_main_loop_quit (loop);
    }

  /* Проверка интервалов времени при воспроизведении. */
  if (step_cnt > 1)
    {
      gdouble time_diff;
      gdouble play_time_diff;

      if ((test_step == CONTROL_NORMAL_PLAY_TEST) || (test_step == CONTROL_REWIND_PLAY_TEST))
        play_time_diff = (1.0 / n_rate) / ABS (play_speed);
      else
        play_time_diff = 1.0 / n_fps;

      time_diff = fabs (g_timer_elapsed (timer, NULL) - play_time_diff);
      if (time_diff > (play_time_diff / 4.0))
        g_warning ("step %d time jitter %f > 25%%", step_cnt, time_diff);
    }

  /* Следующий шаг теста. */
  step_cnt += 1;

  /* Если прошли все шаги теста, переходим к началу следующего теста. */
  if (step_cnt == n_lines)
    {
      test_step += 1;
      step_cnt = 0;
    }

  g_timer_start (timer);

  return TRUE;
}

/* Функция проверки диапазонов индексов данных. */
void
range_check (HyScanForwardLookPlayer *player,
             guint32                  first_index,
             guint32                  last_index,
             gpointer                 user_data)
{
  if ((first_index == check_first_index) &&
      (last_index == check_last_index))
    {
      range_checked = TRUE;
    }
}

/* Функция проверки текущих данных. */
void
data_check (HyScanForwardLookPlayer     *player,
            HyScanForwardLookPlayerInfo *info,
            HyScanAntennaPosition       *position,
            HyScanForwardLookDOA        *doa,
            guint32                      n_doa,
            gpointer                     user_data)
{
  if ((info != NULL) &&
      (info->index == check_index) &&
      (info->time == ((G_USEC_PER_SEC / n_rate) * check_index)) &&
      (n_doa == check_index + n_points))
    {
      data_checked = TRUE;
    }
}

int main( int argc, char **argv )
{
  HyScanRawDataInfo raw_info;
  guint i;

  {
    gchar **args;
    GError *error = NULL;
    GOptionContext *context;
    GOptionEntry entries[] =
      {
        { "lines", 'l', 0, G_OPTION_ARG_INT, &n_lines, "Number of lines", NULL },
        { "points", 'n', 0, G_OPTION_ARG_INT, &n_points, "Number of points per line", NULL },
        { "fps", 'f', 0, G_OPTION_ARG_INT, &n_fps, "FPS rate", NULL },
        { "rate", 'r', 0, G_OPTION_ARG_INT, &n_rate, "Data rate", NULL },
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
  raw_info.data.type = HYSCAN_DATA_COMPLEX_ADC_16LE;
  raw_info.data.rate = 150000.0;
  raw_info.antenna.offset.vertical = 0.0;
  raw_info.antenna.offset.horizontal = 0.0;
  raw_info.antenna.pattern.vertical = 10.0;
  raw_info.antenna.pattern.horizontal = 50.0;
  raw_info.antenna.frequency = 100000.0;
  raw_info.antenna.bandwidth = 10000.0;
  raw_info.adc.vref = 1.0;
  raw_info.adc.offset = 0;

  /* Открываем базу данных. */
  db = hyscan_db_new (db_uri);
  if (db == NULL)
    g_error ("can't open db at: %s", db_uri);

  /* Кэш данных */
  if (cache_size)
    cache = HYSCAN_CACHE (hyscan_cached_new (cache_size));

  /* Объект управления просмотром данных врерёдсмотрящего локатора. */
  player = hyscan_forward_look_player_new ();
  hyscan_forward_look_player_set_cache (player, cache, NULL);
  hyscan_forward_look_player_set_fps (player, n_fps);
  hyscan_forward_look_player_set_sv (player, 1000.0);

  /* Генератор данных. */
  generator = hyscan_fl_gen_new ();
  hyscan_fl_gen_set_info (generator, &raw_info);

  /* Предварительная запись данных. */
  if (!hyscan_fl_gen_set_track (generator, db, PROJECT_NAME, STATIC_TRACK_NAME))
    g_error ("can't start track %s", STATIC_TRACK_NAME);

  for (i = 0; i < n_lines; i++)
    {
      if (!hyscan_fl_gen_generate (generator, (G_USEC_PER_SEC / n_rate) * i, n_points + i))
        g_error ("can't add data");
    }

  g_signal_connect (player, "range", G_CALLBACK (range_check), NULL);
  g_signal_connect (player, "data", G_CALLBACK (data_check), NULL);
  g_timeout_add_full (G_PRIORITY_DEFAULT_IDLE, 10, control_test, NULL, NULL);

  loop = g_main_loop_new (NULL, TRUE);
  timer = g_timer_new ();

  g_main_loop_run (loop);

  g_message ("All done");

  hyscan_db_project_remove (db, PROJECT_NAME);

  g_clear_object (&db);
  g_clear_object (&cache);
  g_clear_object (&player);
  g_clear_object (&generator);

  g_timer_destroy (timer);

  g_free (db_uri);

  xmlCleanupParser ();

  return 0;
}
