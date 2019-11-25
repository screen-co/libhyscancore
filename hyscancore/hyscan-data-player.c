/* hyscan-data-player.c
 *
 * Copyright 2018-2019 Screen LLC, Bugrov Sergey <bugrov@screen-co.ru>
 * Copyright 2019 Screen LLC, Maxim Sidorenko <sidormax@mail.ru>
 *
 * This file is part of HyScanCore library.
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
 * Contact the Screen LLC in this case - info@screen-co.ru
 */

/* HyScanCore имеет двойную лицензию.
 *
 * Во-первых, вы можете распространять HyScanCore на условиях Стандартной
 * Общественной Лицензии GNU версии 3, либо по любой более поздней версии
 * лицензии (по вашему выбору). Полные положения лицензии GNU приведены в
 * <http://www.gnu.org/licenses/>.
 *
 * Во-вторых, этот программный код можно использовать по коммерческой
 * лицензии. Для этого свяжитесь с ООО Экран - info@screen-co.ru.
 */

/**
 * SECTION: hyscan-data-player
 * @Short_description: класс воспроизведения данных
 * @Title: HyScanDataPlayer
 *
 * Класс HyScanDataPlayer используется для воспроизведения данных. Класс обрабатывает данные
 * в фоновом потоке, из которого посылает сигнал на подготовку данных (::process), а так же сигнал
 * смены рабочего галса. Класс добавляет таймер в контекст по умолчанию (MainLoop), по срабатыванию
 * которого при наличии новых данных отправляет сигнал ::ready, а при изменении рабочего диапазона -
 * ::range. Все функции являются неблокирующими и потокобезопасными.
 *
 * Функции класса можно условно разделить на следующие категории:
 * - создание объекта: #hyscan_data_player_new;
 * - инициализация: #hyscan_data_player_set_fps,
 *                  #hyscan_data_player_set_track;
 * - настройка списка каналов: #hyscan_data_player_add_channel,
 *                             #hyscan_data_player_remove_channel,
 *                             #hyscan_data_player_clear_channels;
 * - управление воспроизведением: #hyscan_data_player_play,
 *                                #hyscan_data_player_pause,
 *                                #hyscan_data_player_stop,
 *                                #hyscan_data_player_real_time,
 *                                #hyscan_data_player_seek,
 *                                #hyscan_data_player_seek_next,
 *                                #hyscan_data_player_seek_prev,
 *                                #hyscan_data_player_step;
 * - завершение работы: #hyscan_data_player_shutdown.
 * - получения данных: #hyscan_data_player_get_db,
 *                     #hyscan_data_player_get_project,
 *                     #hyscan_data_player_get_track.
 *                     #hyscan_data_player_is_played.
 *
 * При воспроизведении класс старается максимально точно по времени выдать сигнал подготовки новых
 * данных (::process). Сигналы готовности (::ready) и изменения диапозона (::range) выдаются при
 * наличии соответствующих условий с частотой, заданной пользователем функцией #hyscan_data_player_set_fps.
 * Возможны пропуски некоторых строк, для поддержания требуемой скорости воспроизведения.
 *
 * Навигацию по временнЫм меткам класс совершает на основании списка отслеживаемых каналов данных.
 * Временная шкала объекта HyScanDataPlayer ограничена минимальной и максимальной временными метками
 * среди всех входящих в список каналов. Эта шкала обновляется в режиме реального времени. Список
 * каналов открывается из галса установленного посредством функции #hyscan_data_player_set_track. При
 * смене галса предыдущий список автоматически удаляется. Редактирование данного списка осуществляется
 * функциями #hyscan_data_player_add_channel, #hyscan_data_player_remove_channel и #hyscan_data_player_clear_channels.
 * В случае добавления канала по которому нет данных в базе, класс продолжает свою работу с каналами которые
 * удалось открыть. Если имеются неоткрытые каналы класс будет постоянно предпринимать попытки по их открытию.
 *
 * Скорость воспроизведения настраивается функцией #hyscan_data_player_play. Текущая временная метка
 * #HyScanDataPlayer'а не может выйти за временную шкалы. При достижении границы, текущая метка будет
 * установлена в крайнее положение. Воспроизведение не будет остановлено, таким образом, при обнаружении
 * новых данных воспроизведение будет продолжено с ранее установленной скоростью. Приостановка воспроизведения
 * производится функцией #hyscan_data_player_pause, текущее положение останется неизменным. Функция
 * #hyscan_data_player_stop останавливает воспроизведение и перемещает временной указатель к левой границе
 * временной шкалы. Воспроизведение в режиме реального времени производится с помощью функции
 * #hyscan_data_player_real_time. Данные будут отображены максимально быстро после записи.
 * Чтобы узнать статус проигрывателя имеется функция #hyscan_data_player_is_played.
 *
 * Переместиться на определенную временную метку можно с помощью функции #hyscan_data_player_seek.
 * Перемещения относительно текущего положения осуществляются последством функций #hyscan_data_player_seek_next
 * и #hyscan_data_player_seek_prev. Они позволяют перемещаться к ближайшей метке, содержащей данные.
 * Функция #hyscan_data_player_step является альтернативой предыдущих для более чем одного перемещения.
 *
 * Обработчики сигналов ::process и ::open могут получить информацию о текущем рабочем галсе с помощью
 * функций #hyscan_data_player_get_db, #hyscan_data_player_get_project и #hyscan_data_player_get_track.
 *
 * Перед удалением экземпляра данного класса необходимо вызвать функцию #hyscan_data_player_shutdown,
 * для остановки внутреннего потока и освобождения связанной с ним памяти.
 */

#include <hyscan-data-player.h>
#include <hyscan-core-marshallers.h>
#include <math.h>

#define HYSCAN_DATA_PLAYER_RECONNECT_WAIT (100 * G_TIME_SPAN_MILLISECOND)
#define HYSCAN_DATA_PLAYER_DEFAULT_FPS 20
#define HYSCAN_DATA_PLAYER_DEFAULT_SPEED (0.0)
#define HYSCAN_DATA_PLAYER_REAL_TIME_SPEED (500.0)

enum
{
  SIGNAL_PROCESS,
  SIGNAL_DATA_READY,
  SIGNAL_RANGE_CHANGED,
  SIGNAL_TRACK_CHANGED,
  SIGNAL_LAST,
};

/* Канал данных для навигации во времени. */
typedef struct
{
  HyScanDB *db;                                 /* Интерфейс базы данных канала данных. */
  gchar    *name;                               /* Название канала данных. */
  gint32    id;                                 /* id канала данных. */
} HyScanDataPlayerChannel;

/* Данные для работы фонового потока */
typedef struct
{
  gint32    project_id;                         /* id проекта. */
  gint32    track_id;                           /* id галса. */

  gint64    prev_loop_time;                     /* Метка времени предыдущего цикла функции watcher. */
} HyScanDataPlayerWatcher;

/* Состояние класса */
typedef struct
{
  HyScanDB *db;                                 /* Интерфейс базы данных. */
  gchar    *project_name;                       /* Название проекта. */
  gchar    *track_name;                         /* Название галса. */
  gboolean  track_changed;                      /* Признак изменения галса. */

  GSList   *channels;                           /* Общий список отслеживаемых каналов данных. */
  GSList   *channels_opened;                    /* Список открывшихся каналов данных.*/
  gboolean  channels_changed;                   /* Признак изменения списка используемых каналов. */

  gint64    cur_time;                           /* Метка времени запроса данных. */
  gboolean  time_changed;                       /* Признак изменения метки времени. */

  gdouble   time_speed;                         /* Скорость воспроизведения. */
  gboolean  speed_changed;                      /* Признак изменения скорости воспроизведения. */

  gint32    steps;                              /* Счетчик перемещений к ближайшей метке отслеживаемых каналов. */
} HyScanDataPlayerState;

struct _HyScanDataPlayerPrivate
{
  HyScanDataPlayerState   new_st;               /* Копия user_st. */
  HyScanDataPlayerState   user_st;              /* Состояние плеера, задаваемое пользователем. */
  HyScanDataPlayerState   main_st;              /* Текущее состояние. */
  
  GMutex                  lock;                 /* Блокировка состояния. */
  GCond                   cond;                 /* Сигнал завершения считывания данных из буфера. */

  gint                    range_changed;        /* Признак изменения диапазона */
  gint                    data_ready;           /* Признак завершения подготовки данных обработчиками. */
  gint                    destroy;              /* Признак удаления класса. */

  GThread                *loop_thread;          /* Поток. */

  guint                   timer;                /* Таймер считывания данных. */

  gboolean                any_changes;          /* Признак наличия любого изменения в состоянии. */
  gboolean                played;               /* Признак проигрывания данных. */

  gint64                  min_time;             /* Минимальная временная метка, содержащая данные. */
  gint64                  max_time;             /* Максимальная временная метка, содержащая данные. */

  gint64                  cur_time;             /* Метка текущего времени класса. */
};

static void             hyscan_data_player_object_constructed    (GObject                 *object);

static void             hyscan_data_player_object_finalize       (GObject                 *object);

static gpointer         hyscan_data_player_watcher               (HyScanDataPlayer*        player);

static gboolean         hyscan_data_player_ready_signaller       (HyScanDataPlayer*        player);

static void             hyscan_data_player_copy_state            (HyScanDataPlayerState   *result,
                                                                  HyScanDataPlayerState   *source);

static void             hyscan_data_player_open_all              (HyScanDataPlayer        *player,
                                                                  HyScanDataPlayerState   *state,
                                                                  HyScanDataPlayerWatcher *info);

static gboolean         hyscan_data_player_open_track            (HyScanDataPlayerState   *state,
                                                                  HyScanDataPlayerWatcher *info);

static gboolean         hyscan_data_player_open_channels         (HyScanDataPlayerState   *state,
                                                                  HyScanDataPlayerWatcher *info);

static gboolean         hyscan_data_player_update_time           (HyScanDataPlayer        *player,
                                                                  HyScanDataPlayerState   *state,
                                                                  HyScanDataPlayerWatcher *info);

static void             hyscan_data_player_update_range          (HyScanDataPlayer        *player,
                                                                  HyScanDataPlayerState   *state);

static gint64           hyscan_data_player_get_step_time         (GSList                  *list,
                                                                  gint64                   time,
                                                                  gboolean                 next);

static gint64           hyscan_data_player_take_steps            (gint64                   cur_time,
                                                                  HyScanDataPlayerState   *state);

static void             hyscan_data_player_clear_state           (HyScanDataPlayerState   *state);

static void             hyscan_data_player_set_state             (HyScanDataPlayerState   *state,
                                                                  HyScanDB                *db,
                                                                  const gchar             *project_name,
                                                                  const gchar             *track_name);

static HyScanDataPlayerChannel *
                        hyscan_data_player_new_channel           (HyScanDB                *db,
                                                                  const gchar             *name);

static gpointer         hyscan_data_player_copy_channel          (HyScanDataPlayerChannel *data);

static void             hyscan_data_player_free_channel          (HyScanDataPlayerChannel *data);

static GSList *         hyscan_data_player_find_channel          (GSList                  *list,
                                                                  const gchar             *name);

static inline void      hyscan_data_player_check_changing        (HyScanDataPlayerPrivate *priv);

static guint     hyscan_data_player_signal[SIGNAL_LAST] = {0};

G_DEFINE_TYPE_WITH_PRIVATE (HyScanDataPlayer, hyscan_data_player, G_TYPE_OBJECT)

static void
hyscan_data_player_class_init (HyScanDataPlayerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = hyscan_data_player_object_constructed;
  object_class->finalize = hyscan_data_player_object_finalize;

  /**
   * HyScanDataPlayer::process:
   * @player: указатель на #HyScanDataPlayer
   * @time: метка времени для которой необходимо записать данные
   *
   * Сигнал ::process посылается при перемещении внутренней метки времени, после проверки
   * подключения базы данных и отслеживаемых каналов. Сигнал отправляется из фонового потока не
   * чаще одного раза за период работы класса и НЕ отправляется дважды для одной и той же временнОй
   * метки. Предполагается, что буфер будет передан пользователем посредством user_data при
   * подключении сигнала.
   *
   */
  hyscan_data_player_signal[SIGNAL_PROCESS] =
                  g_signal_new ("process", HYSCAN_TYPE_DATA_PLAYER, G_SIGNAL_RUN_LAST, 0,
                  NULL, NULL,
                  hyscan_core_marshal_VOID__INT64,
                  G_TYPE_NONE,
                  1, G_TYPE_INT64);

  /**
   * HyScanDataPlayer::ready:
   * @player: указатель на #HyScanDataPlayer
   * @time: метка времени данных, содержащихся в буфере
   *
   * Сигнал ::ready посылается каждый заданный период (#hyscan_data_player_set_fps), при
   * условии того, что данные были подготовлены. Отправка сигнала производится внутри контекста
   * по умолчанию.
   *
   */
  hyscan_data_player_signal[SIGNAL_DATA_READY] =
                  g_signal_new ("ready", HYSCAN_TYPE_DATA_PLAYER, G_SIGNAL_RUN_LAST, 0,
                  NULL, NULL,
                  hyscan_core_marshal_VOID__INT64,
                  G_TYPE_NONE,
                  1, G_TYPE_INT64);

  /**
   * HyScanDataPlayer::range:
   * @player: указатель на #HyScanDataPlayer
   * @min_time: минимальная метка времени среди всех отслеживаемых каналов
   * @max_time: максимальня метка времени среди всех отслеживаемых каналов
   *
   * Сигнал ::range посылается непосредственно перед сигналом ::ready, при условии изменения
   * допустимого временнОго диапазона класса в течение последнего периода.
   *
   */
  hyscan_data_player_signal[SIGNAL_RANGE_CHANGED] =
                  g_signal_new ("range", HYSCAN_TYPE_DATA_PLAYER, G_SIGNAL_RUN_LAST, 0,
                  NULL, NULL,
                  hyscan_core_marshal_VOID__INT64_INT64,
                  G_TYPE_NONE,
                  2, G_TYPE_INT64, G_TYPE_INT64);

  /**
   * HyScanDataPlayer::open:
   * @player: указатель на #HyScanDataPlayer
   * @db: указатель на #HyScanDB;
   * @project_name: имя нового проекта;
   * @track_name: имя нового галса.
   *
   * Сигнал ::open посылается непосредственно после удачного открытия нового галса
   * или любой смены каналов.
   *
   */
  hyscan_data_player_signal[SIGNAL_TRACK_CHANGED] =
                  g_signal_new ("open", HYSCAN_TYPE_DATA_PLAYER, G_SIGNAL_RUN_LAST, 0,
                  NULL, NULL,
                  hyscan_core_marshal_VOID__OBJECT_STRING_STRING,
                  G_TYPE_NONE,
                  3, G_TYPE_OBJECT, G_TYPE_STRING, G_TYPE_STRING);

}

static void
hyscan_data_player_init (HyScanDataPlayer *player)
{
  player->priv = hyscan_data_player_get_instance_private (player);
}

static void
hyscan_data_player_object_constructed (GObject *object)
{
  HyScanDataPlayer *player = HYSCAN_DATA_PLAYER (object);
  HyScanDataPlayerPrivate *priv = player->priv;

  priv->min_time = -1;
  priv->max_time = -1;

  g_mutex_init (&priv->lock);
  g_cond_init (&priv->cond);

  hyscan_data_player_set_fps (player, HYSCAN_DATA_PLAYER_DEFAULT_FPS);
  hyscan_data_player_play (player, HYSCAN_DATA_PLAYER_DEFAULT_SPEED);

  priv->played = FALSE;
  priv->loop_thread = g_thread_new ("data-player", (GThreadFunc) hyscan_data_player_watcher, player);
}

static void
hyscan_data_player_object_finalize (GObject *object)
{
  HyScanDataPlayer *player = HYSCAN_DATA_PLAYER (object);
  HyScanDataPlayerPrivate *priv = player->priv;

  if (priv->loop_thread != NULL)
    g_message ("HyScanDataPlayer was not shut down. Some memory leaked.");

  if (priv->timer > 0)
    g_source_remove (priv->timer);

  g_mutex_clear (&priv->lock);
  g_cond_clear (&priv->cond);

  hyscan_data_player_clear_state (&priv->main_st);
  hyscan_data_player_clear_state (&priv->new_st);
  hyscan_data_player_clear_state (&priv->user_st);

  G_OBJECT_CLASS (hyscan_data_player_parent_class)->finalize (object);
}

/* Функция выдает сигнал содержащий метку времени данных и буфер для записи через заданные
 * интервалы времени.*/
static gpointer
hyscan_data_player_watcher (HyScanDataPlayer* player)
{
  HyScanDataPlayerPrivate *priv = player->priv;

  gint64 time, wait_time;

  gboolean any_changes;
  gboolean time_updated;
  gboolean awaken;

  HyScanDataPlayerState *new_st = &priv->new_st;
  HyScanDataPlayerState *user_st = &priv->user_st;
  HyScanDataPlayerState *main_st = &priv->main_st;

  HyScanDataPlayerWatcher info;
  
  main_st->cur_time = -1;

  info.project_id = -1;
  info.track_id = -1;
  info.prev_loop_time = g_get_monotonic_time ();

  while (!g_atomic_int_get (&priv->destroy))
    {
      g_mutex_lock (&priv->lock);
      /* Ожидание пользовательского события или g_cond сигнала от signaller.*/
      wait_time = g_get_monotonic_time () + HYSCAN_DATA_PLAYER_RECONNECT_WAIT;
      awaken = g_cond_wait_until (&priv->cond, &priv->lock, wait_time);
      if (!awaken)
        {
          g_mutex_unlock (&priv->lock);
          continue;
        }

      any_changes = priv->any_changes;
      g_mutex_unlock (&priv->lock);
      if (g_atomic_int_get (&priv->data_ready))
        continue;

      /* Копирует новые данные заданные пользователем.*/
      if (any_changes)
        {
          g_mutex_lock (&priv->lock);
          hyscan_data_player_copy_state (new_st, user_st);
          priv->any_changes = FALSE;
          g_mutex_unlock (&priv->lock);

          hyscan_data_player_copy_state (main_st, new_st);
        }
      
      /* Если сменен галс или есть неоткрытые каналы пытается открыть их.*/
      if (main_st->channels != NULL || main_st->track_changed)
        hyscan_data_player_open_all (player, main_st, &info);

      /* Если открытых каналов нет переход в блок ожидания.*/
      if (main_st->channels_opened == NULL)
        continue;

      /* Обновление диапазона данных, расчет внутреннего времени класса.*/
      hyscan_data_player_update_range (player, main_st);
      time_updated = hyscan_data_player_update_time (player, main_st, &info);
      
      if (time_updated || any_changes)
        {
          g_mutex_lock (&priv->lock);
          time = priv->cur_time;
          g_mutex_unlock (&priv->lock);

          if (time >= 0)
            g_signal_emit (player, hyscan_data_player_signal[SIGNAL_PROCESS], 0, time);

          g_atomic_int_set (&priv->data_ready, 1);
        }
    }

  if (info.project_id > 0)
    hyscan_db_close (main_st->db, info.project_id);
  if (info.track_id > 0)
    hyscan_db_close (main_st->db, info.track_id);

  return NULL;
}

/* Функция периодически выдает сигнал считывания данных из буфера.*/
static gboolean
hyscan_data_player_ready_signaller (HyScanDataPlayer *player)
{
  HyScanDataPlayerPrivate *priv = player->priv;

  gint64 time;
  gint64 min_time, max_time;
  gboolean data_ready;
  gboolean range;

  g_mutex_lock (&priv->lock);
  min_time = priv->min_time;
  max_time = priv->max_time;
  time = priv->cur_time;
  g_mutex_unlock (&priv->lock);

  range = g_atomic_int_get (&priv->range_changed);
  data_ready = g_atomic_int_get (&priv->data_ready);

  if (range)
    {
      g_signal_emit (player, hyscan_data_player_signal[SIGNAL_RANGE_CHANGED], 0, min_time, max_time);
      g_atomic_int_set (&priv->range_changed, 0);
    }

  if (data_ready)
    {
      if (time >= 0)
        g_signal_emit (player, hyscan_data_player_signal[SIGNAL_DATA_READY], 0, time);

      g_atomic_int_set (&priv->data_ready, 0);
    }

  g_mutex_lock (&priv->lock);
  g_cond_signal (&priv->cond);
  g_mutex_unlock (&priv->lock);

  return G_SOURCE_CONTINUE;
}

/* Функция копирует состояние класса, выставляя необходимые признаки.*/
static void
hyscan_data_player_copy_state (HyScanDataPlayerState *result,
                               HyScanDataPlayerState *source)
{
  if (source->track_changed)
    {
      hyscan_data_player_set_state (result,
                                    source->db,
                                    source->project_name,
                                    source->track_name);

      result->track_changed = TRUE;
      source->track_changed = FALSE;

      source->channels_changed = TRUE;
    }

  /* Если состав каналов изменился, общий список каналов обновляется новым,
   * а список открытых каналов очищается.*/
  if (source->channels_changed)
    {
      g_slist_free_full (result->channels, (GDestroyNotify) hyscan_data_player_free_channel);
      result->channels = g_slist_copy_deep (source->channels, 
                                            (GCopyFunc) hyscan_data_player_copy_channel, NULL);

      g_slist_free_full (result->channels_opened, (GDestroyNotify) hyscan_data_player_free_channel);
      result->channels_opened = NULL;

      result->channels_changed = TRUE;
      source->channels_changed = FALSE;
    }
  
  if (source->speed_changed)
    {
      result->time_speed = source->time_speed;

      result->speed_changed = TRUE;
      source->speed_changed = FALSE;
    }

  if (source->time_changed)
    {
      result->cur_time = source->cur_time;

      result->time_changed = TRUE;
      source->time_changed = FALSE;
    }

  if (source->steps != 0)
    {
      result->steps = source->steps;
      source->steps = 0;
    }
}

/* Функция осуществляет попытку открытия галса и отслеживаемых каналов данных.*/
static void
hyscan_data_player_open_all (HyScanDataPlayer        *player,
                             HyScanDataPlayerState   *state,
                             HyScanDataPlayerWatcher *info)
{
  if (state->db == NULL || state->project_name == NULL || state->track_name == NULL)
    return;

  if (state->track_changed)
    {
      if (!hyscan_data_player_open_track (state, info))
        return;
      g_signal_emit (player, hyscan_data_player_signal[SIGNAL_TRACK_CHANGED], 0,
                     state->db, state->project_name, state->track_name);
      state->track_changed = FALSE;
    }

  if (!hyscan_data_player_open_channels (state, info))
    return;

  /* Сообщает об изменении списка каналов.*/
  g_signal_emit (player, hyscan_data_player_signal[SIGNAL_TRACK_CHANGED], 0,
                 state->db, state->project_name, state->track_name);
}

/* Функция подключения к базе данных и открытия галса.*/
static gboolean
hyscan_data_player_open_track (HyScanDataPlayerState   *state,
                               HyScanDataPlayerWatcher *info)
{
  /* Закрытие старого галса.*/
  if (info->project_id > 0)
    {
      hyscan_db_close (state->db, info->project_id);
      info->project_id = -1;
    }

  if (info->track_id > 0)
    {
      hyscan_db_close (state->db, info->track_id);
      info->track_id = -1;
    }

  /* Проверяет подключение к новой базе данных.*/
  info->project_id = hyscan_db_project_open (state->db,
                                             state->project_name);

  if (info->project_id < 1)
    return FALSE;

  info->track_id = hyscan_db_track_open (state->db,
                                         info->project_id,
                                         state->track_name);
  if (info->track_id < 1)
    return FALSE;

  return TRUE;
}

/* Функция подключения отслеживаемых каналов данных. В случае открытия канала 
   добавляет его в список открытых, удаляя при этом из общего списка.
   В случае открытия хотя бы одного канала возвращает TRUE, иначе FALSE.*/
static gboolean
hyscan_data_player_open_channels (HyScanDataPlayerState   *state,
                                  HyScanDataPlayerWatcher *info)
{
  GSList *link, *next;
  HyScanDataPlayerChannel *data;
  gint64 id;
  gboolean opened = FALSE;

  if (state->channels == NULL)
    return FALSE;

  for (link = state->channels; link != NULL; link = next)
    {
      next = link->next;
      data = link->data;

      id = hyscan_db_channel_open (data->db, info->track_id, data->name);

      if (id < 0)
        continue;

      /* В случае успешного открытия удаляет канал из общего списка и 
         добавляет его в список открытых. */
      data->id = id;
      state->channels = g_slist_remove_link (state->channels, link);
      state->channels_opened = g_slist_concat (state->channels_opened, link);
      opened = TRUE;
    }

  return opened;
}

/* Функция обновления временнЫх меток и интервалов класса.*/
static gboolean
hyscan_data_player_update_time (HyScanDataPlayer        *player,
                                HyScanDataPlayerState   *state,
                                HyScanDataPlayerWatcher *info)
{
  HyScanDataPlayerPrivate *priv = player->priv;

  gint64 cur_time;
  gint64 delta_time;

  gboolean result = FALSE;

  delta_time = g_get_monotonic_time () - info->prev_loop_time;
  info->prev_loop_time += delta_time;

  g_mutex_lock (&priv->lock);
  cur_time = priv->cur_time;
  g_mutex_unlock (&priv->lock);

  if (state->time_changed)
    cur_time = state->cur_time;
  else
    cur_time += delta_time * state->time_speed;

  state->time_changed = FALSE;

  if (state->steps != 0)
    cur_time = hyscan_data_player_take_steps (cur_time, state);

  /* Проверка допустимых значений */
  cur_time = CLAMP (cur_time, priv->min_time, priv->max_time);

  g_mutex_lock (&priv->lock);
  if (priv->cur_time != cur_time)
    result = TRUE;
  priv->cur_time = cur_time;
  g_mutex_unlock (&priv->lock);

  return result;
}

/* Функция обновления минимальной и максимальной меток, содержащих данные в отслеживаемых каналах.*/
static void
hyscan_data_player_update_range (HyScanDataPlayer      *player,
                                 HyScanDataPlayerState *state)
{
  HyScanDataPlayerPrivate *priv = player->priv;

  GSList                  *link;
  HyScanDataPlayerChannel *data;

  gint64  prev_min, prev_max;
  gint64  cur_min, cur_max;
  guint32 lindex, rindex;
  gint64  ltime, rtime;

  g_mutex_lock (&priv->lock);
  prev_min = priv->min_time;
  prev_max = priv->max_time;
  g_mutex_unlock (&priv->lock);

  cur_min = -1;
  cur_max = -1;

  /* Пересчет границ.*/
  for (link = state->channels_opened; link != NULL; link = link->next)
    {
      data = link->data;
      hyscan_db_channel_get_data_range (state->db, data->id, &lindex, &rindex);
      ltime = hyscan_db_channel_get_data_time (data->db, data->id, lindex);
      rtime = hyscan_db_channel_get_data_time (data->db, data->id, rindex);

      if (ltime < cur_min || cur_min < 0)
        cur_min = ltime;
      if (rtime > cur_max)
        cur_max = rtime;
    }

  /* Если границы валидны и изменились, выставляет признак изменения.*/
  if (cur_min >= 0 && cur_max >= 0 && (cur_min != prev_min || cur_max != prev_max))
    {
      g_mutex_lock (&priv->lock);
      priv->min_time = cur_min;
      priv->max_time = cur_max;
      g_mutex_unlock (&priv->lock);

      g_atomic_int_set (&priv->range_changed, 1);
    }
}

/* Функция перемещения к ближайшей временной метке, содержащей данные.*/
static gint64
hyscan_data_player_get_step_time (GSList  *list,
                                  gint64   time,
                                  gboolean next)
{
  GSList                  *link;
  HyScanDataPlayerChannel *data;

  HyScanDBFindStatus       found;

  gint64                   rtime, ltime;
  guint32                  index;

  gint64                   alter;
  gint64                   result = -1;
  gint64                   step = next ? 1 : -1;

  for (link = list; link != NULL; link = link->next)
    {
      data = link->data;

      found = hyscan_db_channel_find_data (data->db, data->id,
                                           time + step,
                                           NULL, NULL,
                                           &ltime, &rtime);
      /* При обычном результате, берется ближайшая метка в текущем канале, и если это первая
       * валидная метка или она ближе предыдущего варианта, то записывается в результат. */
      if (found == HYSCAN_DB_FIND_OK)
        {
          alter = next ? rtime : ltime;
        }
      /* Если искомая метка находится раньше первой метки канала, то рассматривается только
       * вариант при движении вперед и берется крайняя левая метка. При движении назад предполагается,
       * что левая граница канала уже была пройдена, и, в случае необходимости, это значение будет
       * установлено в конце функции hysan_data_player_update_time. Поэтому нет необходимости дважды
       * его проверять. */
      else if (next && found == HYSCAN_DB_FIND_LESS)
        {
          hyscan_db_channel_get_data_range (data->db, data->id, &index, NULL);
          alter = hyscan_db_channel_get_data_time (data->db, data->id, index);
        }
      else if (!next && found == HYSCAN_DB_FIND_GREATER)
        {
          hyscan_db_channel_get_data_range (data->db, data->id, NULL, &index);
          alter = hyscan_db_channel_get_data_time (data->db, data->id, index);
        }
      else
        {
          continue;
        }

      if ((next && alter <= result) || (!next && alter >= result) || result < 0)
        result = alter;
    }
  return result;
}

/* Функция следит за количеством шагов.*/
static gint64
hyscan_data_player_take_steps (gint64                 cur_time,
                               HyScanDataPlayerState *state)
{
  gint64   step_time = cur_time;
  gint32   steps;

  gboolean forward = state->steps > 0;
  gint32   incr = forward ? -1 : 1;

  for (steps = state->steps; steps != 0; steps += incr)
    {
      step_time = hyscan_data_player_get_step_time (state->channels_opened,
                                                    step_time, forward);
      if (step_time < 0)
        break;
    }

  state->steps = 0;
  return step_time;
}

/* Функция освобождения памяти, занимаемой состоянием.*/
static void
hyscan_data_player_clear_state (HyScanDataPlayerState *state)
{
  g_clear_object (&state->db);
  g_clear_pointer (&state->project_name, g_free);
  g_clear_pointer (&state->track_name, g_free);

  g_slist_free_full (state->channels, (GDestroyNotify)hyscan_data_player_free_channel);
  state->channels = NULL;
  g_slist_free_full (state->channels_opened, (GDestroyNotify)hyscan_data_player_free_channel);
  state->channels_opened = NULL;
}

/* Функция установки состояния.*/
static void
hyscan_data_player_set_state (HyScanDataPlayerState *state,
                              HyScanDB              *db,
                              const gchar           *project_name,
                              const gchar           *track_name)
{
  hyscan_data_player_clear_state (state);

  if (db != NULL)
    state->db = g_object_ref (db);

  if (project_name != NULL)
    state->project_name = g_strdup (project_name);

  if (track_name != NULL)
    state->track_name = g_strdup (track_name);
}

/* Функция создания структуры данных для отслеживания канала.*/
static HyScanDataPlayerChannel *
hyscan_data_player_new_channel (HyScanDB    *db,
                                const gchar *name)
{
  HyScanDataPlayerChannel *data;

  data = g_slice_new (HyScanDataPlayerChannel);
  if (db != NULL)
    data->db = g_object_ref (db);
  data->name = g_strdup (name);

  data->id = -1;

  return data;
}

/* Функция копирования структуры данных, отслеживаемого канала.*/
static gpointer
hyscan_data_player_copy_channel (HyScanDataPlayerChannel *data)
{
  return hyscan_data_player_new_channel (data->db, data->name);
}

/* Функция удаления структуры данных отслеживаемого канала.*/
static void
hyscan_data_player_free_channel (HyScanDataPlayerChannel *data)
{
  if (data->id > 0)
    hyscan_db_close (data->db, data->id);
  g_clear_pointer (&data->name, g_free);
  g_object_unref (data->db);
  g_slice_free (HyScanDataPlayerChannel, data);
}

/* Поиск канала в списке по имени.*/
static GSList *
hyscan_data_player_find_channel (GSList      *list,
                                 const gchar *name)
{
  GSList *link = list;
  HyScanDataPlayerChannel *data;

  for (link = list; link != NULL; link = link->next)
    {
      data = link->data;
      if (g_str_equal (data->name, name))
        break;
    }

  return link;
}

/* Выставляет все необходимые флаги при внесении изменений.*/
static inline void
hyscan_data_player_check_changing (HyScanDataPlayerPrivate *priv)
{
  priv->any_changes = TRUE;
  g_atomic_int_set (&priv->data_ready, 0);
  g_cond_signal (&priv->cond);
}

/**
 * hyscan_data_player_new:
 *
 * Функция создания нового объекта воспроизведения данных.
 *
 * Returns: #HyScanDataPlayer или NULL. Для удаления #g_object_unref.
 */
HyScanDataPlayer *
hyscan_data_player_new (void)
{
  return g_object_new (HYSCAN_TYPE_DATA_PLAYER, NULL);
}

/**
 * hyscan_data_player_shutdown:
 * @player: указатель на #HyScanDataPlayer
 *
 * Функция завершения работы плеера. Необходимо вызывать в MainLoop перед 
 * освобождением объекта для избегания утечки памяти.
 */
void
hyscan_data_player_shutdown (HyScanDataPlayer *player)
{
  HyScanDataPlayerPrivate *priv;

  g_return_if_fail (HYSCAN_IS_DATA_PLAYER (player));
  priv = player->priv;

  /* Остановка потока */
  g_atomic_int_set (&priv->destroy, 1);
  g_mutex_lock (&priv->lock);
  g_cond_signal (&priv->cond);
  g_mutex_unlock (&priv->lock);

  g_clear_pointer (&priv->loop_thread, g_thread_join);
}

/**
 * hyscan_data_player_set_track:
 * @player: указатель на #HyScanDataPlayer
 * @db: указатель на #HyScanDB
 * @project_name: название проекта
 * @track_name: название галса
 *
 * Функция устанавливает рабочий галс. После смены галса необходимо заново задать
 * список каналов, текущее время и скорость воспроизведения.
 */
void
hyscan_data_player_set_track (HyScanDataPlayer *player,
                              HyScanDB         *db,
                              const gchar      *project_name,
                              const gchar      *track_name)
{
  HyScanDataPlayerPrivate *priv;

  g_return_if_fail (HYSCAN_IS_DATA_PLAYER (player));
  priv = player->priv;

  g_mutex_lock (&priv->lock);

  hyscan_data_player_set_state (&priv->user_st,
                                db,
                                project_name,
                                track_name);

  priv->user_st.track_changed = TRUE;
  hyscan_data_player_check_changing (priv);
  g_mutex_unlock (&priv->lock);
}

/**
 * hyscan_data_player_set_fps:
 * @player: указатель на #HyScanDataPlayer
 * @fps: целевое количество обработок в секунду.
 *
 * Функция устанавливает период попыток выдачи сигнала ::ready.
 */
void
hyscan_data_player_set_fps (HyScanDataPlayer *player,
                            guint32           fps)
{
  HyScanDataPlayerPrivate *priv;

  g_return_if_fail (HYSCAN_IS_DATA_PLAYER (player));
  priv = player->priv;

  if (fps == 0)
    return;

  if (priv->timer > 0)
    g_source_remove (priv->timer);

  priv->timer = g_timeout_add ((G_USEC_PER_SEC / fps) / 1000,
                               (GSourceFunc)hyscan_data_player_ready_signaller,
                                player);
}

/**
 * hyscan_data_player_get_db:
 * @player: указатель на #HyScanDataPlayer
 *
 * Функция возвращает указатель на используемый #HyScanDB. Следует использовать только 
 * в callback'ах сигналов HyScanDataPlayer::process и HyScanDataPlayer::open (внутри потока
 * обработки данного #HyScanDataPlayer'а), в противном случае функция вернет NULL.
 *
 * Returns: (transfer none): указатель на #HyScanDB.
 */
HyScanDB *
hyscan_data_player_get_db (HyScanDataPlayer *player)
{
  HyScanDataPlayerState *state;

  g_return_val_if_fail (HYSCAN_IS_DATA_PLAYER (player), NULL);
  g_return_val_if_fail (g_thread_self() == player->priv->loop_thread, NULL);

  state = &player->priv->main_st;

  return state->db;
}

/**
 * hyscan_data_player_get_project_name:
 * @player: указатель на #HyScanDataPlayer
 *
 * Функция возвращает название используемого проекта. Следует использовать только в callback'ах
 * сигналов HyScanDataPlayer::process и HyScanDataPlayer::open (внутри потока обработки данного
 * #HyScanDataPlayer'а), в противном случае функция вернет NULL.
 *
 * Returns: (transfer none): название проекта.
 */
const gchar *
hyscan_data_player_get_project_name (HyScanDataPlayer *player)
{
  HyScanDataPlayerState *state;

  g_return_val_if_fail (HYSCAN_IS_DATA_PLAYER (player), NULL);
  g_return_val_if_fail (g_thread_self() == player->priv->loop_thread, NULL);

  state = &player->priv->main_st;

  return state->project_name;
}

/**
 * hyscan_data_player_get_track_name:
 * @player: указатель на #HyScanDataPlayer
 *
 * Функция возвращает название используемого галса. Следует использовать только в callback'ах сигналов
 * HyScanDataPlayer::process и HyScanDataPlayer::open (внутри потока обработки данного
 * #HyScanDataPlayer'а), в противном случае функция вернет NULL.
 *
 * Returns: (transfer none): название галса.
 */
const gchar *
hyscan_data_player_get_track_name (HyScanDataPlayer *player)
{
  HyScanDataPlayerState *state;

  g_return_val_if_fail (HYSCAN_IS_DATA_PLAYER (player), NULL);
  g_return_val_if_fail (g_thread_self() == player->priv->loop_thread, NULL);

  state = &player->priv->main_st;

  return state->track_name;
}

/**
 * hyscan_data_player_is_played:
 * @player: указатель на #HyScanDataPlayer
 *
 * Функция возвращает признак проигрывания данных.
 *
 * Returns: TRUE в случае проигрывания данных, иначе FALSE.
 */
gboolean
hyscan_data_player_is_played (HyScanDataPlayer *player)
{
  g_return_val_if_fail (HYSCAN_IS_DATA_PLAYER (player), FALSE);

  return player->priv->played;
}

/**
 * hyscan_data_player_add_channel:
 * @player: указатель на #HyScanDataPlayer
 * @source: тип источника данных
 * @channel: индекс канала данных
 * @type: тип канала данных
 *
 * Функция добавляет канал данных в список отслеживаемых. По данному списку формируются допустимые
 * временные метки, а так же производятся переходы фунцией #hysca_data_player_step.
 *
 * Returns: id канала, необходимый для удаления. id < 0 при некорректных параметрах.
 */
gint32
hyscan_data_player_add_channel (HyScanDataPlayer *player,
                                HyScanSourceType  source,
                                guint             channel,
                                HyScanChannelType type)
{
  HyScanDataPlayerPrivate *priv;
  HyScanDataPlayerChannel *channel_data;
  const gchar *name;

  g_return_val_if_fail (HYSCAN_IS_DATA_PLAYER (player), -1);
  priv = player->priv;

  name = hyscan_channel_get_id_by_types (source, type, channel);

  if (name == NULL)
    return -1;

  g_mutex_lock (&priv->lock);
  if (hyscan_data_player_find_channel (priv->user_st.channels, name))
    goto exit;

  channel_data = hyscan_data_player_new_channel (priv->user_st.db, name);
  priv->user_st.channels = g_slist_prepend (priv->user_st.channels, channel_data);

  priv->user_st.channels_changed = TRUE;
  hyscan_data_player_check_changing (priv);

exit:
  g_mutex_unlock (&priv->lock);

  return 10000 * source + 10 * channel + type;
}

/**
 * hyscan_data_player_remove_channel:
 * @player: указатель на #HyScanDataPlayer
 * @id: id канала данных
 *
 * Функция удаляет канал данных из списка отслеживаемых.
 *
 * Returns: TRUE, если канал найден в списке и удален, FALSE - в остальных случаях.
 */
gboolean
hyscan_data_player_remove_channel (HyScanDataPlayer *player,
                                   gint32            id)
{
  HyScanDataPlayerPrivate *priv;

  HyScanSourceType  source;
  guint32           channel;
  HyScanChannelType type;
  const gchar      *name;

  GSList *link;

  g_return_val_if_fail (HYSCAN_IS_DATA_PLAYER (player), FALSE);
  priv = player->priv;

  if (id < 0)
    return FALSE;

  source = id / 10000;
  channel = (id % 10000) / 10;
  type = id % 10;

  name = hyscan_channel_get_id_by_types (source, type, channel);

  if (name == NULL)
    return FALSE;

  g_mutex_lock (&priv->lock);
  link = hyscan_data_player_find_channel (priv->user_st.channels, name);

  if (link == NULL)
    {
      g_mutex_unlock (&priv->lock);
      return FALSE;
    }

  priv->user_st.channels = g_slist_remove_link (priv->user_st.channels, link);

  priv->user_st.channels_changed = TRUE;
  hyscan_data_player_check_changing (priv);
  g_mutex_unlock (&priv->lock);

  hyscan_data_player_free_channel (link->data);
  g_slist_free (link);

  return TRUE;
}

/**
 * hyscan_data_player_clear_channels:
 * @player: указатель на #HyScanDataPlayer
 *
 * Функция удаляет все каналы из списка отслеживаемых.
 */
void
hyscan_data_player_clear_channels (HyScanDataPlayer *player)
{
  HyScanDataPlayerPrivate *priv;

  GSList *del_list, *del_opened_list;

  g_return_if_fail (HYSCAN_IS_DATA_PLAYER (player));
  priv = player->priv;

  g_mutex_lock (&priv->lock);
  del_list = priv->user_st.channels;
  priv->user_st.channels = NULL;
  del_opened_list = priv->user_st.channels_opened;
  priv->user_st.channels_opened = NULL;

  priv->min_time = -1;
  priv->max_time = -1;

  priv->user_st.channels_changed = TRUE;
  hyscan_data_player_check_changing (priv);
  g_mutex_unlock (&priv->lock);

  g_slist_free_full (del_list, (GDestroyNotify)hyscan_data_player_free_channel);
  g_slist_free_full (del_opened_list, (GDestroyNotify)hyscan_data_player_free_channel);
}

/**
 * hyscan_data_player_channel_is_exist:
 * @player: указатель на #HyScanDataPlayer
 * @id: id канала данных
 *
 * Функция проверяет канал на наличие в проигрывателе.
 *
 * Returns: TRUE, если канал найден в списке, иначе FALSE.
 */
gboolean
hyscan_data_player_channel_is_exist (HyScanDataPlayer *player,
                                     gint32            id)
{
  HyScanDataPlayerPrivate *priv;

  HyScanSourceType  source;
  guint32           channel;
  HyScanChannelType type;
  const gchar      *name;

  GSList *link;

  g_return_val_if_fail (HYSCAN_IS_DATA_PLAYER (player), FALSE);
  priv = player->priv;

  if (id < 0)
    return FALSE;

  source = id / 10000;
  channel = (id % 10000) / 10;
  type = id % 10;

  name = hyscan_channel_get_id_by_types (source, type, channel);

  if (name == NULL)
    return FALSE;

  g_mutex_lock (&priv->lock);
  link = hyscan_data_player_find_channel (priv->user_st.channels, name);

  if (link == NULL)
    {
      g_mutex_unlock (&priv->lock);
      return FALSE;
    }
  g_mutex_unlock (&priv->lock);

  return TRUE;
}

/**
 * hyscan_data_player_play:
 * @player: указатель на #HyScanDataPlayer
 * @speed: новое значение скорости
 *
 * Функция устанавливает скорость воспроизведения. Скорость воспроизведения замедляется (< 1.0)
 * или ускоряется (> 1.0) относительно нормального течения времени. Если скорость воспроизведения
 * отрицательная, проигрывание осуществляется в обратном направлении.
 */
void
hyscan_data_player_play (HyScanDataPlayer *player,
                         gdouble           speed)
{
  HyScanDataPlayerPrivate *priv;

  g_return_if_fail (HYSCAN_IS_DATA_PLAYER (player));

  priv = player->priv;

  if (speed == 0.0)
    priv->played = FALSE;
  else
    priv->played = TRUE;

  g_mutex_lock (&priv->lock);
  priv->user_st.time_speed = speed;
  priv->user_st.speed_changed = TRUE;
  hyscan_data_player_check_changing (priv);
  g_mutex_unlock (&priv->lock);
}

/**
 * hyscan_data_player_pause:
 * @player: указатель на #HyScanDataPlayer
 *
 * Функция приостанавливает проигрывание данных.
 */
void
hyscan_data_player_pause (HyScanDataPlayer *player)
{
  g_return_if_fail (HYSCAN_IS_DATA_PLAYER (player));
  
  hyscan_data_player_play (player, 0.0);
}

/**
 * hyscna_data_player_stop:
 * @player: указатель на #HyScanDataPlayer
 *
 * Функция останавливает воспроизведение данных и устанавливает положение 
 * на временную метку с наименьшим значением.
 */
void
hyscan_data_player_stop (HyScanDataPlayer *player)
{
  g_return_if_fail (HYSCAN_IS_DATA_PLAYER (player));

  hyscan_data_player_play (player, 0.0);
  hyscan_data_player_seek (player, -1);
}

/**
 * hyscan_data_player_real_time:
 * @player: указатель на #HyScanDataPlayer
 *
 * Функция устанавливает плеер в режим воспроизведения в реальном времени.
 */
void
hyscan_data_player_real_time (HyScanDataPlayer *player)
{
  g_return_if_fail (HYSCAN_IS_DATA_PLAYER (player));

  hyscan_data_player_play (player, HYSCAN_DATA_PLAYER_REAL_TIME_SPEED);
  hyscan_data_player_seek (player, G_MAXINT64);
}

/**
 * hyscan_data_player_seek:
 * @player: указатель на #HyScanDataPlayer
 * @time: новое значение метки.
 *
 * Функция устанавливает метку времени на заданное значение. В фоновом потоке 
 * обработка данной функции производится ДО обработки функции #hyscan_data_player_step.
 */
void
hyscan_data_player_seek (HyScanDataPlayer *player,
                         gint64            time)
{
  HyScanDataPlayerPrivate *priv;

  g_return_if_fail (HYSCAN_IS_DATA_PLAYER (player));
  priv = player->priv;

  g_mutex_lock (&priv->lock);
  priv->user_st.cur_time = time;
  priv->user_st.steps = 0;
  priv->user_st.time_changed = TRUE;
  hyscan_data_player_check_changing (priv);
  g_mutex_unlock (&priv->lock);
}

/**
 * hyscan_data_player_seek_next:
 * @player: указатель на #HyScanDataPlayer
 *
 * Функция перемещает #HyScanDataPlayer к ближайшей временной метке с данными 
 * в положительном напралении.
 */
void
hyscan_data_player_seek_next (HyScanDataPlayer *player)
{
  g_return_if_fail (HYSCAN_IS_DATA_PLAYER (player));

  hyscan_data_player_step (player, 1);
}

/**
 * hyscan_data_player_seek_prev:
 * @player: указатель на #HyScanDataPlayer
 *
 * Функция перемещает #HyScanDataPlayer к ближайшей временной метке с данными 
 * в отрицательном напралении.
 */
void
hyscan_data_player_seek_prev (HyScanDataPlayer *player)
{
  g_return_if_fail (HYSCAN_IS_DATA_PLAYER (player));

  hyscan_data_player_step (player, -1);
}

/**
 * hyscan_data_player_step:
 * @player: указатель на #HyScanDataPlayer
 * @steps: количество перемещений
 *
 * Функция перемещает #HyScanDataPlayer к временной метке с данными на заданное число перемещений.
 * При steps > 0 выполняется заданное число перемещений в положительном направлении, 
 * при steps < 0 - в отрицательном. При steps = 0 ничего не происходит.
 */
void
hyscan_data_player_step (HyScanDataPlayer *player,
                         gint32            steps)
{
  HyScanDataPlayerPrivate *priv;

  g_return_if_fail (HYSCAN_IS_DATA_PLAYER (player));
  priv = player->priv;

  g_mutex_lock (&priv->lock);
  priv->user_st.steps += steps;
  hyscan_data_player_check_changing (priv);
  g_mutex_unlock (&priv->lock);
}
