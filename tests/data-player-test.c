/* data-player-test.c
 *
 * Copyright 2018-2019 Screen LLC, Bugrov Sergey <bugrov@screen-co.ru>
 * Copyright 2019 Screen LLC, Maxim Sidorenko <sidormax@mail.ru>
 *
 * This file is part of HyScanCore.
 *
 * HyScanCore is dual-licensed: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * HyScanCore is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Alternatively, you can license this code under a commercial license.
 * Contact the Screen LLC in this case - <info@screen-co.ru>.
 */

/* HyScanCore имеет двойную лицензию.
 *
 * Во-первых, вы можете распространять HyScanCore на условиях Стандартной
 * Общественной Лицензии GNU версии 3, либо по любой более поздней версии
 * лицензии (по вашему выбору). Полные положения лицензии GNU приведены в
 * <http://www.gnu.org/licenses/>.
 *
 * Во-вторых, этот программный код можно использовать по коммерческой
 * лицензии. Для этого свяжитесь с ООО Экран - <info@screen-co.ru>.
 */
#include <hyscan-data-player.h>
#include <hyscan-db.h>

#include <glib.h>
#include <glib-object.h>

#define TEST_WAIT (G_USEC_PER_SEC / 4)

#define TEST_END (FALSE)
#define TEST_CONTINUE (TRUE)

#define STEPS_COUNT {1, 6, -1, -3}

#define PROJECT_NAME "test-project"
#define TRACK_NAME "test-track"
#define TRACK_NAME2 "test-track-change"
#define CHANNEL_NAME "test-channel"

enum
{
  TEST_TIMER,
  CHANGE_DB,
  TEST_STEP,
  CHANGE_RANGE,
  TEST_WAIT_CHANNEL,
  FINAL,
  LAST
};

typedef gboolean (*test_func) (gpointer);

typedef struct
{
  guint32           type;
  gchar            *name;
  test_func         loop_func;

  gint64            fps;
  gfloat            speed;
  guint32           stage;
} TestInfo;

typedef struct
{
  gint32            project_id;
  gint32            track_id;

  HyScanSourceType  source;
  guint             num;
  HyScanChannelType type;
  const gchar      *name;

  gint32            id;
  gint32            player_id;
} ChannelData;

HyScanDataPlayer   *player;
gint64              player_time;
gint64              prev_player_time;
gint64              prev_prepare_time;

HyScanDB           *db;
gchar              *db_uri = NULL;
gchar              *project_name = PROJECT_NAME;
gchar              *track_name   = TRACK_NAME;
gchar              *track_copy_name = TRACK_NAME2;
gchar              *channel_name = CHANNEL_NAME;

gint32              project_id = -1;
gint32              track_id = -1;

ChannelData        *channels;
ChannelData        *wait_channel;
gint32              n_channels = 1;
gint32              n_lines = 150;
gdouble             speed = 4;
guint32             fps = 20;

gint64              stage_start_time;
guint32             cur_test = 0;

GMainLoop          *main_loop;

gint64              min_time;
gint64              max_time;

guint32             saves_count;
gboolean            data_ready = TRUE;
gboolean            track_opened = FALSE;
gchar             **set_db_test_message;
gint32              steps[] = STEPS_COUNT;
gint64              ready_time;

gint                debug = 0;

gboolean            range_static = FALSE;
gboolean            range_realtime = FALSE;
gboolean            set_track = FALSE;
gboolean            autoremove_channel = FALSE;
gboolean            add_channel = FALSE;
gboolean            remove_channel = FALSE;
gboolean            timer = TRUE;
gboolean            step = TRUE;

/* Возвращает временнУю метку для индекса index данных канала channel.*/
gint64
get_time_by_index (guint32 channel,
                   guint32 index)
{
  static gint64 base_offset = 100000;
  static gint64 channel_offset = 10000;
  static gint64 data_period = 50000;

  return (gint64) (base_offset + channel_offset * channel + index * data_period);
}

/* Добавляет строку данных в канал.*/
gboolean
channel_add_line (gint32 ch_id,
                  gint64 time,
                  gint32 data)
{
  gboolean result;
  HyScanBuffer *buffer = hyscan_buffer_new ();

  hyscan_buffer_set (buffer, HYSCAN_DATA_AMPLITUDE_INT32LE, &data, sizeof (gint32));
  result = hyscan_db_channel_add_data (db, ch_id, time, buffer, NULL);

  g_object_unref (buffer);
  return result;
}

/* Обработчик сигнала process. Сохраняет значения текущей и предыдущей временнЫх меток.*/
void
process_callback (HyScanDataPlayer *player,
                  gint64            time,
                  gpointer          user_data)
{
  prev_player_time = player_time;

  player_time = time;

  if (debug)
    g_print ("Process signal time: %"G_GINT64_FORMAT"\n", player_time);
}

/* Обработчик сигнала process на завершающей стадии теста, приостанавливает текущий поток на секунду.*/
void
process_long_callback (HyScanDataPlayer *player,
                       gint64            time,
                       gpointer          user_data)
{
  prev_player_time = player_time;

  player_time = time;

  g_usleep (G_USEC_PER_SEC);
}

/* Обработчик сигнала ready. Фиксирует время готовности данных и выставляет флаг готовности данных. */
void
ready_callback (HyScanDataPlayer *player,
                gint64            time,
                gpointer          user_data)
{
  ready_time = g_get_monotonic_time ();
  data_ready = TRUE;
}

/* Обработчик сигнала range. Фиксирует изменившийся диапазон данных min_time и max_time. */
void
range_callback (HyScanDataPlayer *player,
                gint64            min,
                gint64            max,
                gpointer          user_data)
{
  min_time = min;
  max_time = max;

  if (debug)
    g_print ("Got new range [%"G_GINT64_FORMAT", %"G_GINT64_FORMAT"].\n", min, max);
}

/* Обработчик сигнала open. Выставляет признак открытия каналов или галса.*/
void
open_callback (HyScanDataPlayer *player,
               HyScanDB         *db,                               
               const gchar      *project,                               
               const gchar      *track,
               gpointer          user_data)
{
  if (debug)
    g_print ("%s; %s is opened.\n", project, track);

  track_opened = TRUE;
}

/* Функция возвращает следующую метку времени идущей за меткой time.*/
gint64
get_step_time (gint64   time,
               gboolean next)
{
  HyScanDBFindStatus found;

  gint64 rtime, ltime;
  guint32 rind, lind;

  gint64 alter;
  gint64 result = -1;

  gint32 i;
  gint64 step = next ? 1 : -1;

  /* Для всех каналов ищет индексы с данными для следующей метки идущей после time.*/
  for (i = 0; i < n_channels; i++)
    {
      found = hyscan_db_channel_find_data (db, channels[i].id,
                                           time + step,
                                           &lind, &rind, &ltime, &rtime);

      if (found == HYSCAN_DB_FIND_OK)
        {
          alter = next ? rtime : ltime;
        }
      /* В случае когда метка времени оказалась раньше записаных данных, 
         возвращает крайний "левый" индекс диапазона.*/
      else if (next && found == HYSCAN_DB_FIND_LESS)
        {
          hyscan_db_channel_get_data_range (db, channels[i].id, &lind, &rind);
          alter = hyscan_db_channel_get_data_time (db, channels[i].id, lind);
        }
      /* В случае когда метка времени оказалась позже записаных данных 
         возвращает крайний "правый" индекс диапазона.*/  
      else if (!next && found == HYSCAN_DB_FIND_GREATER)
        {
          hyscan_db_channel_get_data_range (db, channels[i].id, &lind, &rind);
          alter = hyscan_db_channel_get_data_time (db, channels[i].id, rind);
        }
      else
        {
          continue;
        }

      if ((next && alter < result) || (!next && alter > result) || result < 0)
        result = alter;
    }
  return result;
}

/* Функция генерирует каналы для тестирования.*/
void
create_channel_data (ChannelData *ch,
                     gint32       project_id,
                     gint32       track_id)
{
  gint32 i;

  ch->project_id = project_id;
  ch->track_id = track_id;

  ch->source = HYSCAN_SOURCE_FORWARD_LOOK;
  ch->type = HYSCAN_CHANNEL_DATA;

  ch->name = hyscan_channel_get_id_by_types (ch->source,
                                               ch->type,
                                               ch->num + 1);

  /* Создает канал и заполняет его тестовыми данными.*/
  ch->id = hyscan_db_channel_create (db, ch->track_id, ch->name, NULL);

  for (i = 0; i < n_lines; i++)
    channel_add_line (ch->id, get_time_by_index (ch->num, i), (ch->num + 1) * 1000 + i + 1);
}

/* Функция устанавливает исходное состояние теста. */
void
  reset_player (TestInfo *test_state)
{
  gint32 i;
  gint64 start_time;

  g_print ("%s:\n", test_state->name);
  g_print (" Speed: %f\n", test_state->speed);

  hyscan_data_player_pause (player);

  /* Очищает старые каналы. */
  hyscan_data_player_clear_channels (player);

  /* Устанавливает галс. */
  hyscan_data_player_set_track (player, db, project_name, track_name);

  /* Добавляет новые каналы. */
  for (i = 0; i < n_channels; i++)
    {
      channels[i].player_id = hyscan_data_player_add_channel (player,
                                                              channels[i].source,
                                                              channels[i].num + 1,
                                                              HYSCAN_CHANNEL_DATA);
    }

  /* Установка стартовой временнОй метки в зависимости от направления воспроизведения.*/
  if (test_state->speed < 0)
    start_time = get_time_by_index (n_channels - 1, n_lines - 1);
  else
    start_time = get_time_by_index (0, 0);

  /* Стартует плеер.*/
  hyscan_data_player_play (player, test_state->speed);
  hyscan_data_player_seek (player, start_time);
  hyscan_data_player_set_fps (player, test_state->fps);

  player_time = start_time;
  prev_player_time = start_time;
}

/* Функция срабатывает по таймеру main_loop'а, организует последовательность работы тестов.*/
gboolean
test_selector (TestInfo *test_state)
{
  gboolean test_status;

  /* Завершение программы. */
  if (test_state[cur_test].type >= LAST)
    {
      g_print ("Test finished\n");
      g_main_loop_quit (main_loop);
      return G_SOURCE_REMOVE;
    }

  /* Переход к текущему тесту.*/
  test_status = test_state[cur_test].loop_func (&test_state[cur_test]);
  if (test_status == TEST_CONTINUE)
    return G_SOURCE_CONTINUE;

  /* Переход к следующему тесту.*/
  cur_test++;

  return G_SOURCE_CONTINUE;
}

/* Тест для проверки сигнала изменения диапазона данных. Сначала устанавливается
   исходное состояние, затем в следующей стадии теста проверяется правильность 
   начального назначения диапазона данных, далее добавляются строки данных с 
   измененным диапазоном, и в конечной стадии проверяется правильность установки
   новых границ.*/
gboolean
test_range (gpointer user_data)
{
  TestInfo *test_state = user_data;

  gint32 i;

  /* Пауза между стадиями теста.*/
  if (g_get_monotonic_time () - stage_start_time <= TEST_WAIT &&
      test_state->stage != 0)
    {
      return TEST_CONTINUE;
    }

  /* В первом цикле теста устанавливает исходное состояние. */
  if (test_state->stage == 0)
    {
      g_signal_connect (player, "range", G_CALLBACK (range_callback), NULL);
      reset_player (test_state);
    }
  /* Проверяет правильность назначения диапазона данных после установки исходного
     состояния, затем добавляет строки данных с измененным диапазоном.*/
  else if (test_state->stage == 1)
    {
      if (debug)
        g_print ("Theo range [%"G_GINT64_FORMAT", %"G_GINT64_FORMAT"].\n", 
                 get_time_by_index (0, 0), get_time_by_index (n_channels - 1, n_lines - 1));

      /* Проверяет правильность границ. */
      if (min_time == get_time_by_index (0, 0) &&
          max_time == get_time_by_index (n_channels - 1, n_lines - 1))
        {
          range_static = TRUE;
        }

      g_print ("  %s - Min time: %"G_GINT64_FORMAT". Max time: %"G_GINT64_FORMAT".\n",
               range_static ? "OK" : "FAIL", min_time, max_time);

      /* Добавляет во все каналы по 1 строке.*/
      for (i = 0; i < 2 * n_channels; i++)
        channel_add_line (channels[i].id, get_time_by_index (channels[i].num, n_lines), 0);

      n_lines++;
    }
  /* Проверяет правильность установки новых границ.*/
  else
    {
      if (min_time == get_time_by_index (0, 0) &&
          max_time == get_time_by_index (n_channels - 1, n_lines - 1))
        {
          range_realtime = TRUE;
        }

      g_print ("  %s - Min time: %"G_GINT64_FORMAT". Max time: %"G_GINT64_FORMAT".\n",
               range_realtime ? "OK" : "FAIL", min_time, max_time);

      return TEST_END;
    }

  min_time = -1;
  max_time = -1;

  test_state->stage++;
  stage_start_time = g_get_monotonic_time ();

  return TEST_CONTINUE;
}

/* Тест изменения списка каналов данных и рабочего галса.
 * Через заданные периоды времени выдаются команды по работе со списком каналов и/или галсом,
 * проверяется результат выполнения команд предыдущего периода на основании счетчика количества
 * считываний (saves_count).
 * Стадия 1 - проверка установки галса.
 * Стадия 2 - проверка автоматического очищения списка каналов при смене галса.
 * Стадия 3 - проверка добавления каналов данных.
 * Стадия 4 - проверка удаления каналов данных.*/
gboolean
test_set_db (gpointer user_data)
{
  TestInfo *test_state = user_data;

  gint32 i;
  gboolean *print_debug = NULL;

  /* Счетчик сигнала "ready" */
  if (data_ready && test_state->stage != 0)
    {
      data_ready = FALSE;
      saves_count++;
    }

  /* Пауза между стадиями теста. */
  if (g_get_monotonic_time () - stage_start_time <= TEST_WAIT)
    return TEST_CONTINUE;

  /* В первом цикле теста устанавливает исходное состояние. */
  if (test_state->stage == 0)
    {
      g_signal_connect (player, "open", G_CALLBACK (open_callback), NULL);
      reset_player (test_state);
    }
  /* Проверка установки галса. После установки исходного состояния,где происходит 
     операция установки галса, должен выполниться хотя бы один ready и open.*/
  else if (test_state->stage == 1)
    {
      if (saves_count > 0 && track_opened)
        {
          set_track = TRUE;
          track_opened = FALSE;
        }
      print_debug = &set_track;

      /* Переключение на копию галса, БЕЗ добавления каналов данных. 
         Все ранее установленные каналы должны автоматически удалиться.*/
      hyscan_data_player_set_track (player, db, project_name, track_copy_name);
      hyscan_data_player_seek (player, test_state->speed < 0 ? G_MAXINT64 : 0);
      hyscan_data_player_play (player, test_state->speed);
    }
  /* Проверка автоматического очищения списка каналов при смене галса. Тест считается
     пройденным в случае выполнения ready не более 1 раза.*/
  else if (test_state->stage == 2)
    {
      if (saves_count <= 1)
        autoremove_channel = TRUE;
      print_debug = &autoremove_channel;

      /* Добавление каналов из копии галса. */
      for (i = n_channels; i < 2 * n_channels; i++)
        {
          channels[i].player_id = hyscan_data_player_add_channel (player,
                                                                  channels[i].source,
                                                                  channels[i].num + 1,
                                                                  HYSCAN_CHANNEL_DATA);
        }
      hyscan_data_player_seek (player, test_state->speed < 0 ? G_MAXINT64 : 0);
      hyscan_data_player_play (player, test_state->speed);
    }
  /* Проверка добавления каналов данных.*/
  else if (test_state->stage == 3)
    {
      if (saves_count > 0)
        add_channel = TRUE;
      print_debug = &add_channel;

      /* Удаление каналов.*/
      for (i = n_channels; i < 2 * n_channels; i++)
        hyscan_data_player_remove_channel (player, channels[i].player_id);
    }
  /* Проверка удаления каналов данных.*/
  else if (test_state->stage == 4)
    {      
      if (saves_count <= 1)
        remove_channel = TRUE;
      print_debug = &remove_channel;

      g_free (set_db_test_message);
    }
  /* Тест окончен.*/
  else
    {
      return TEST_END;
    }

  /* Вывод результата теста.*/
  if (test_state->stage != 0)
    g_print ("  Stage %i - %s\n", test_state->stage, *print_debug ? "OK" : "FAIL");

  saves_count = 0;
  stage_start_time = g_get_monotonic_time ();
  test_state->stage++;

  return TEST_CONTINUE;
}

/* Тест выводит среднюю разницу периода плеера и периода, заданного пользователем. */
gboolean
test_timer (gpointer user_data)
{
  TestInfo *test_state = user_data;

  static gint64 min, max;

  gint64 theo_time;
  gint64 delta_time;

  /* В первом цикле теста устанавливает исходное состояние. */
  if (test_state->stage == 0)
    {
      reset_player (test_state);

      min = get_time_by_index (0, 0);
      max = get_time_by_index (n_channels - 1, n_lines - 1);

      if (debug)
        g_print ("Real range [%"G_GINT64_FORMAT", %"G_GINT64_FORMAT"]\n", min, max);

      test_state->stage++;
      stage_start_time = g_get_monotonic_time ();
      saves_count = 0;
    }

  /* Счетчик сигнала "ready" */
  if (data_ready)
    {
      data_ready = FALSE;
      saves_count++;
    }

  /* Ожидание пока не будет пройден весь диапазон данных.*/
  if ((player_time != min && test_state->speed < 0) || (player_time != max && test_state->speed >= 0))
    return TEST_CONTINUE;

  /* Расчет эталонного времени на проход от второго вызова до предпоследнего.*/
  theo_time = ABS ((max - min) / test_state->speed);
  /* Расчет реально затраченного времени на проход.*/
  delta_time = ABS (g_get_monotonic_time () - stage_start_time);

  g_print ("  Averange delta: %"G_GINT64_FORMAT", theo_time = %"G_GINT64_FORMAT", real_time = %"G_GINT64_FORMAT", saves: %i\n",
           ABS ((delta_time - theo_time) / (saves_count - 2)),
           theo_time,
           delta_time,
           saves_count - 2);

  return TEST_END;
}

/* Тест проверки функции hyscan_data_player_step. */
gboolean
test_step (gpointer user_data)
{
  TestInfo *test_state = user_data;

  gint32 i;
  gint64 theo_time;
  gint64 abs_steps;

  /* Пауза между стадиями теста. */
  if (g_get_monotonic_time () - stage_start_time <= TEST_WAIT &&
      test_state->stage != 0)
    {
      return TEST_CONTINUE;
    }

  /* В первом цикле теста устанавливает исходное состояние. */
  if (test_state->stage == 0)
    {
      reset_player (test_state);
      hyscan_data_player_step (player, steps[test_state->stage]);

      stage_start_time = g_get_monotonic_time ();
      test_state->stage++;

      data_ready = FALSE;

      return TEST_CONTINUE;
    }

  /* После выполнения ф-ии *_step, в prev_player_time записана метка времени до совершения шага.
   * От предыдущей метки необходимо последовательно совершить нужное кол-во шагов.*/
  theo_time = prev_player_time;
  abs_steps = ABS (steps[test_state->stage - 1]);

  for (i = 0; i < abs_steps; i++)
    theo_time = get_step_time (theo_time, (steps[test_state->stage - 1] > 0));

  /* Сверяет данные плеера (player_time) с полученными в тесте (theo_time) */
  if (player_time != theo_time)
    {
      g_print ("  FAIL - Step (%i) have not been done. Need: %"G_GINT64_FORMAT"; got: %"G_GINT64_FORMAT"\n",
               steps[test_state->stage - 1], theo_time, player_time);
      step = FALSE;
      return TEST_END;
    }
  stage_start_time = g_get_monotonic_time ();
  g_print ("  OK - Step (%i) have been succesfully done.\n", steps[test_state->stage - 1]);

  /* Завершение теста если протестированы все шаги.*/
  if (test_state->stage >= sizeof (steps)/sizeof (steps[0]))
    return TEST_END;

  /* Тестирование с новым значением шага.*/
  hyscan_data_player_step (player, steps[test_state->stage]);

  saves_count = 0;
  test_state->stage++;

  return TEST_CONTINUE;
}

/* Тест ожидания канала не имеющего данные. Добавляет канал для которого в базе еще нет данных.
 * Затем добавляются данные для канала. В ходе теста проверяется подключение каналов на всех стадиях.
*/
gboolean
test_wait_channel (gpointer user_data)
{
  TestInfo *test_state = user_data;

  gboolean *print_debug = NULL;

  /* Счетчик сигнала "ready" */
  if (data_ready && test_state->stage != 0)
    {
      data_ready = FALSE;
      saves_count++;
    }

  /* Пауза между стадиями теста.*/
  if (g_get_monotonic_time () - stage_start_time <= TEST_WAIT)
    return TEST_CONTINUE;

  /* В первом цикле теста устанавливает исходное состояние. */
  if (test_state->stage == 0)
    {
      reset_player (test_state);

      /* Создание канала для которого в базе еще нет данных.*/
      wait_channel = g_new (ChannelData, 1);
      wait_channel->num = 2 * n_channels;
      wait_channel->player_id = hyscan_data_player_add_channel (player,
                                                                HYSCAN_SOURCE_FORWARD_LOOK,
                                                                2 * n_channels + 1,
                                                                HYSCAN_CHANNEL_DATA);
      hyscan_data_player_seek (player, test_state->speed < 0 ? G_MAXINT64 : 0);
      hyscan_data_player_play (player, test_state->speed);
    }
  /* Проверка подключения канала. Должен выполниться хотя бы один ready и open.*/
  else if (test_state->stage == 1)
    {
      if (saves_count > 0 && track_opened)
        {
          add_channel = TRUE;
          track_opened = FALSE;
        }
      print_debug = &add_channel;

      /* Добавление данных для канала.*/
      create_channel_data (wait_channel, project_id, track_id);

    }
  /* Проверка подключения канала после добавления данных. 
     Должен выполниться хотя бы один ready и open.*/
  else if (test_state->stage == 2)
    {
      add_channel = FALSE;
      if (saves_count > 0 && track_opened)
        add_channel = TRUE;
      print_debug = &add_channel;
    }
  /* Тест окончен.*/
  else
    {      
      return TEST_END;
    }

  /* Вывод результата теста.*/
  if (test_state->stage != 0)
    g_print ("  Stage %i - %s\n", test_state->stage, *print_debug ? "OK" : "FAIL");

  saves_count = 0;
  stage_start_time = g_get_monotonic_time ();
  test_state->stage++;

  return TEST_CONTINUE;
}

/* Завершающий тест окончания работы плеера.*/
gboolean
test_finalize (gpointer user_data)
{
  TestInfo *test_state = user_data;

  /* В первом цикле теста устанавливает исходное состояние. */
  if (test_state->stage == 0)
    {
      reset_player (test_state);
      g_signal_connect (player, "process", G_CALLBACK (process_long_callback), NULL);
    }

  g_usleep (G_USEC_PER_SEC/4);
  hyscan_data_player_shutdown (player);
  g_clear_object (&player);
  g_usleep (G_USEC_PER_SEC/4);

  return TEST_END;
}

int
main (int    argc,
      char **argv)
{
  gint32 i;

  gint32 track_copy_id = -1;

  {
    gchar **args;
    GError *error = NULL;
    GOptionContext *context;
    GOptionEntry entries[] =
      {   
        { "lines",    'l', 0, G_OPTION_ARG_INT,    &n_lines,    "Number of lines",                   NULL },
        { "channels", 'c', 0, G_OPTION_ARG_INT,    &n_channels, "Number of channels for test",       NULL },
        { "speed",    's', 0, G_OPTION_ARG_DOUBLE, &speed,      "Time multiplier for test (double)", NULL },
        { "fps",      'f', 0, G_OPTION_ARG_INT,    &fps,        "Time between signaller calls",      NULL},
        { "debug",    'd', 0, G_OPTION_ARG_INT,    &debug,      "Debug info",                        NULL},
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
        return -1;
      }

    g_option_context_free (context);

    db_uri = g_strdup (args[1]);
    if (db_uri == NULL)
      return -1;

    g_strfreev (args);

    if (fps == 0)
      fps = 20;
  }

  /* Создает базу данных, проект, галс и его копию. */
  db = hyscan_db_new (db_uri);
  track_copy_name = g_strdup_printf ("%s-copy", track_name);

  project_id = hyscan_db_project_create (db, project_name, NULL);
  track_id = hyscan_db_track_create (db, project_id, track_name, NULL, 0);
  track_copy_id = hyscan_db_track_create (db, project_id, track_copy_name, NULL, 0);

  /* Массив данных каналов. */
  channels = g_new (ChannelData, 2 * n_channels);

  /* Заполняет каналы данными. */
  for (i = 0; i < 2 * n_channels; i++)
    {
      channels[i].num = i;
      create_channel_data (&channels[i], project_id, (i < n_channels) ? track_id : track_copy_id);
    }

  /* Настройка плеера.*/
  player = hyscan_data_player_new ();

  hyscan_data_player_set_fps (player, fps);
  g_signal_connect (player, "ready", G_CALLBACK (ready_callback), NULL);
  g_signal_connect (player, "process", G_CALLBACK (process_callback), NULL);

  TestInfo test_state[] = {
  /* type              | name                | test_function   | fps     | speed   | stage   */
    {CHANGE_RANGE,      "Range signal test",  test_range,        fps,      1,        0       },
    {CHANGE_DB,         "Change track test",  test_set_db,       fps,      1,        0       },
    {TEST_TIMER,        "Timer test",         test_timer,        fps,      speed,    0       },
    {TEST_TIMER,        "Timer test",         test_timer,        fps,      -speed,   0       },
    {TEST_STEP,         "Step test",          test_step,         fps,      0,        0       },
    {TEST_WAIT_CHANNEL, "Wait channel test",  test_wait_channel, fps,      1,        0       },
    {FINAL,             "Finalize test",      test_finalize,     fps,      1,        0       },
    {LAST,     NULL},
  };

  /* Каждую миллисекунду вызывает функцию test_selector. */
  g_timeout_add_full (G_PRIORITY_DEFAULT_IDLE, 1, (GSourceFunc) test_selector, &test_state, NULL);

  /* Создание цикла обработки событий. Выход осуществляется по завершении работы всех тестов.*/
  main_loop = g_main_loop_new (NULL, TRUE);
  g_main_loop_run (main_loop);

  /* Освобождение ресурсов.*/
  hyscan_db_close (db, track_id);
  hyscan_db_close (db, project_id);
  hyscan_db_project_remove (db, project_name);
  g_clear_object (&db);
  g_clear_pointer (&channels, g_free);
  g_clear_pointer (&wait_channel, g_free);

  /* Результаты тестов.*/
  if (!range_static || !range_realtime || !set_track || !autoremove_channel ||
      !add_channel || !remove_channel || !timer || !step)
    {
      g_message ("HyScanDataPlayer test failed.");
      return -1;
    }

  return 0;
}
