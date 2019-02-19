/*
 * \file hyscan-forward-look-player.c
 *
 * \brief Исходный файл класса управления просмотром данных вперёдсмотрящего локатора
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-forward-look-player.h"
#include "hyscan-core-marshallers.h"

#define DEFAULT_FPS   30
#define DEFAULT_DELAY 100000

enum
{
  SIGNAL_RANGE,
  SIGNAL_DATA,
  SIGNAL_LAST
};

typedef enum
{
  HYSCAN_FORWARD_LOOK_PLAYER_STOP,
  HYSCAN_FORWARD_LOOK_PLAYER_START,
  HYSCAN_FORWARD_LOOK_PLAYER_PLAY,
  HYSCAN_FORWARD_LOOK_PLAYER_PAUSE,
  HYSCAN_FORWARD_LOOK_PLAYER_REAL_TIME
} HyScanForwardLookPlayerMode;

typedef struct
{
  HyScanDB                    *db;                     /* Интерфейс базы данных. */
  HyScanCache                 *cache;                  /* Интерфейс системы кэширования. */
  gchar                       *project_name;           /* Название проекта. */
  gchar                       *track_name;             /* Название галса. */
  gboolean                     track_changed;          /* Признак изменения галса. */

  gdouble                      sound_velocity;         /* Скорость звука, м/с. */
  gboolean                     sound_velocity_changed; /* Признак изменения скорости звука. */

  guint32                      index;                  /* Индекс обрабатывамых данных. */
  gboolean                     index_changed;          /* Признак изменения индекса обрабатываемых данных. */
} HyScanForwardLookPlayerState;

typedef struct
{
  HyScanForwardLookData       *data;                   /* Объект обработки данных. */

  guint32                      first_index;            /* Первый индекс данных. */
  guint32                      last_index;             /* Последний индекс данных. */
  gboolean                     range_changed;          /* Признак изменения диапазона индексов данных. */

  const HyScanForwardLookDOA  *doa;                    /* Текущий массив целей. */
  guint32                      n_points;               /* Число точек данных. */
  gint64                       doa_time;               /* Метка времени текущих данных. */
  guint32                      doa_index;              /* Индекс текущих данных. */
  gboolean                     doa_changed;            /* Признак изменения данных. */

  HyScanAntennaOffset          offset;                 /* Смещение приёмной антенны. */
  gdouble                      alpha;                  /* Максимальный/минимальный угол по азимуту, рад. */

  GMutex                       lock;                   /* Блокировка данных. */
} HyScanForwardLookPlayerData;

struct _HyScanForwardLookPlayerPrivate
{
  HyScanForwardLookPlayerState new_state;              /* Новые значения объектов класса. */
  HyScanForwardLookPlayerState cur_state;              /* Текущие значения объектов класса. */
  HyScanForwardLookPlayerData  data;                   /* Данные и информация о них. */

  HyScanForwardLookPlayerMode  mode;                   /* Режим отображения данных. */
  gdouble                      speed;                  /* Множитель скорости проигрывания данных. */
  gint                         delay;                  /* Задержка таймера для текущего уровня FPS. */

  gint                         shutdown;               /* Признак завершения работы. */
  GThread                     *processor;              /* Поток фоновой обработки данных. */

  GMutex                       ctl_lock;               /* Блокировка управления. */

  guint                        signaller_tag;          /* Тег функции-сигнализатора. */
};

static void      hyscan_forward_look_player_object_constructed (GObject                       *object);
static void      hyscan_forward_look_player_object_finalize    (GObject                       *object);

static void      hyscan_forward_look_player_sync_state         (HyScanForwardLookPlayerState  *new_state,
                                                                HyScanForwardLookPlayerState  *cur_state);

static void      hyscan_forward_look_player_open_data          (HyScanForwardLookPlayerState  *state,
                                                                HyScanForwardLookPlayerData   *data);

static void      hyscan_forward_look_player_check_range        (HyScanForwardLookPlayerState  *state,
                                                                HyScanForwardLookPlayerData   *data);

static gint64    hyscan_forward_look_player_play_index         (HyScanForwardLookPlayerData   *data,
                                                                gint64                         time,
                                                                gboolean                       reverse);

static gpointer  hyscan_forward_look_player_processor          (gpointer                       data);
static gboolean  hyscan_forward_look_player_signaller          (gpointer                       data);

static guint     hyscan_forward_look_player_signals[SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (HyScanForwardLookPlayer, hyscan_forward_look_player, G_TYPE_OBJECT)

static void
hyscan_forward_look_player_class_init (HyScanForwardLookPlayerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = hyscan_forward_look_player_object_constructed;
  object_class->finalize = hyscan_forward_look_player_object_finalize;

  hyscan_forward_look_player_signals[SIGNAL_RANGE] =
    g_signal_new ("range", HYSCAN_TYPE_FORWARD_LOOK_PLAYER, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  hyscan_core_marshal_VOID__UINT_UINT,
                  G_TYPE_NONE,
                  2, G_TYPE_UINT, G_TYPE_UINT);

  hyscan_forward_look_player_signals[SIGNAL_DATA] =
    g_signal_new ("data", HYSCAN_TYPE_FORWARD_LOOK_PLAYER, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  hyscan_core_marshal_VOID__POINTER_POINTER_POINTER_UINT,
                  G_TYPE_NONE,
                  4, G_TYPE_POINTER, G_TYPE_POINTER, G_TYPE_POINTER, G_TYPE_UINT);
}

static void
hyscan_forward_look_player_init (HyScanForwardLookPlayer *player)
{
  player->priv = hyscan_forward_look_player_get_instance_private (player);
}

static void
hyscan_forward_look_player_object_constructed (GObject *object)
{
  HyScanForwardLookPlayer *player = HYSCAN_FORWARD_LOOK_PLAYER (object);
  HyScanForwardLookPlayerPrivate *priv = player->priv;

  g_mutex_init (&priv->ctl_lock);
  g_mutex_init (&priv->data.lock);

  priv->mode = HYSCAN_FORWARD_LOOK_PLAYER_STOP;
  priv->speed = 1.0;
  priv->delay = G_USEC_PER_SEC / DEFAULT_FPS;

  priv->processor = g_thread_new ("fl-processor", hyscan_forward_look_player_processor, priv);

  priv->signaller_tag = g_timeout_add (priv->delay / 1000,
                                       hyscan_forward_look_player_signaller,
                                       player);
}

static void
hyscan_forward_look_player_object_finalize (GObject *object)
{
  HyScanForwardLookPlayer *player = HYSCAN_FORWARD_LOOK_PLAYER (object);
  HyScanForwardLookPlayerPrivate *priv = player->priv;

  g_source_remove (priv->signaller_tag);

  g_atomic_int_set (&priv->shutdown, 1);
  g_clear_pointer (&priv->processor, g_thread_join);

  g_mutex_clear (&priv->data.lock);
  g_mutex_clear (&priv->ctl_lock);

  G_OBJECT_CLASS (hyscan_forward_look_player_parent_class)->finalize (object);
}

/* Функция синхронизирует состояние внутренних обектов класс при смене
 * галса, кэша, скорости звука и отображаемой строки. */
static void
hyscan_forward_look_player_sync_state (HyScanForwardLookPlayerState *new_state,
                                       HyScanForwardLookPlayerState *cur_state)
{
  /* Новый галс для отображения. */
  if (new_state->track_changed)
    {
      g_clear_object (&cur_state->db);
      g_clear_object (&cur_state->cache);
      g_clear_pointer (&cur_state->project_name, g_free);
      g_clear_pointer (&cur_state->track_name, g_free);

      cur_state->db = new_state->db;
      cur_state->cache = new_state->cache;
      cur_state->project_name = new_state->project_name;
      cur_state->track_name = new_state->track_name;
      cur_state->index = 0;
      cur_state->track_changed = TRUE;

      new_state->db = NULL;
      new_state->cache = NULL;
      new_state->project_name = NULL;
      new_state->track_name = NULL;
      new_state->index = 0;
      new_state->track_changed = FALSE;
    }

  /* Новая скорость звука. */
  if (new_state->sound_velocity_changed)
    {
      cur_state->sound_velocity = new_state->sound_velocity;
      cur_state->sound_velocity_changed = TRUE;

      new_state->sound_velocity_changed = FALSE;
    }

  /* Принудительная обработка строки с указанным индексом. */
  if (new_state->index_changed)
    {
      cur_state->index = new_state->index;
      cur_state->index_changed = TRUE;

      new_state->index_changed = FALSE;
    }
}

/* Функция открывает галс на обработку и устанавливает параметры
 * кэширования и скорости звука. */
static void
hyscan_forward_look_player_open_data (HyScanForwardLookPlayerState *state,
                                      HyScanForwardLookPlayerData  *data)
{
  /* Проверка изменения текущего галса. */
  if (state->track_changed)
    {
      /* Закрываем предыдущий галс, открываем новый. */
      g_clear_object (&data->data);
      data->data = hyscan_forward_look_data_new (state->db, state->cache, state->project_name, state->track_name);
    }

  /* Устанавливаем скорость звука. */
  if (data->data != NULL)
    {
      if (state->sound_velocity_changed || state->track_changed)
        {
          hyscan_forward_look_data_set_sound_velocity (data->data, state->sound_velocity);
          state->sound_velocity_changed = FALSE;
        }
    }

  /* Обнуляем текущий буфер массива целей и индексов данных. */
  if (state->track_changed)
    {
      g_mutex_lock (&data->lock);

      if (data->data != NULL)
        {
          data->offset = hyscan_forward_look_data_get_offset (data->data);
          data->alpha = hyscan_forward_look_data_get_alpha (data->data);
        }

      data->doa = NULL;
      data->n_points = 0;
      data->doa_time = 0;
      data->doa_index = 0;
      data->doa_changed = TRUE;

      data->first_index = 0;
      data->last_index = 0;
      data->range_changed = TRUE;

      g_mutex_unlock (&data->lock);

      state->index = 0;
      state->index_changed = TRUE;

      if (data->data != NULL)
        state->track_changed = FALSE;
    }
}

/* Функция проверяет текущий диапазон данных. */
static void
hyscan_forward_look_player_check_range (HyScanForwardLookPlayerState *state,
                                        HyScanForwardLookPlayerData  *data)
{
  guint32 first_index = 0;
  guint32 last_index = 0;

  /* Текущий диапазон данных. */
  hyscan_forward_look_data_get_range (data->data, &first_index, &last_index);

  g_mutex_lock (&data->lock);

  /* Изменение диапазона данных. */
  if ((data->first_index != first_index) || (data->last_index != last_index))
    {
      data->first_index = first_index;
      data->last_index = last_index;
      data->range_changed = TRUE;
    }

  g_mutex_unlock (&data->lock);
}

/* Функция ищет индекс данных для отображения в текущий момент времени. */
static gint64
hyscan_forward_look_player_play_index (HyScanForwardLookPlayerData *data,
                                       gint64                       time,
                                       gboolean                     reverse)
{
  HyScanDBFindStatus status;
  guint32 lindex, rindex;

  /* Ищем индекс для текущего времени проигрывания. */
  status = hyscan_forward_look_data_find_data (data->data, time, &lindex, &rindex, NULL, NULL);
  if (status == HYSCAN_DB_FIND_OK)
    return reverse ? rindex : lindex;
  else if (status == HYSCAN_DB_FIND_LESS)
    return data->first_index;
  else if (status == HYSCAN_DB_FIND_GREATER)
    return data->last_index;

  return -1;
}

/* Поток фоновой обработки данных. */
static gpointer
hyscan_forward_look_player_processor (gpointer user_data)
{
  HyScanForwardLookPlayerPrivate *priv = user_data;

  gint64 start_time = 0;

  GTimer *delay_timer = g_timer_new ();
  GTimer *play_timer = g_timer_new ();

  while (!g_atomic_int_get (&priv->shutdown))
    {
      HyScanForwardLookPlayerMode mode;
      gdouble speed;
      gint delay;

      /* Начало текущего периода. */
      g_timer_start (delay_timer);

      g_mutex_lock (&priv->ctl_lock);

      /* Синхронизируем состояние внутренних объектов класса. */
      hyscan_forward_look_player_sync_state (&priv->new_state, &priv->cur_state);

      /* Текущий режим отображения данных. */
      mode = priv->mode;
      speed = priv->speed;
      delay = priv->delay;

      /* Запуск воспроизведения. */
      if (priv->mode == HYSCAN_FORWARD_LOOK_PLAYER_START)
        priv->mode = HYSCAN_FORWARD_LOOK_PLAYER_PLAY;

      g_mutex_unlock (&priv->ctl_lock);

      /* Открываем галс для отображения. */
      hyscan_forward_look_player_open_data (&priv->cur_state, &priv->data);

      /* Нет галса для обработки или режим остановки. */
      if ((priv->data.data == NULL) || (mode == HYSCAN_FORWARD_LOOK_PLAYER_STOP))
        {
          g_usleep (DEFAULT_DELAY);

          continue;
        }

      /* Проверяем диапазон данных. */
      hyscan_forward_look_player_check_range (&priv->cur_state, &priv->data);

      /* Запуск воспроизведения. */
      if (mode == HYSCAN_FORWARD_LOOK_PLAYER_START)
        {
          g_timer_start (play_timer);
          priv->cur_state.index_changed = TRUE;
        }

      /* Режим воспроизведения. */
      else if (mode == HYSCAN_FORWARD_LOOK_PLAYER_PLAY)
        {
          gint64 cur_time, cur_index;

          /* Текущее время проигрывателя от момента запуска воспроизведения. */
          cur_time = G_USEC_PER_SEC * g_timer_elapsed (play_timer, NULL) * speed;
          cur_time = start_time + cur_time;

          /* Ищем индекс для текущего времени проигрывания. */
          cur_index = hyscan_forward_look_player_play_index (&priv->data, cur_time,
                                                                   (speed > 0.0) ? FALSE : TRUE);

          if ((cur_index >= 0) && (cur_index != priv->cur_state.index))
            {
              priv->cur_state.index = cur_index;
              priv->cur_state.index_changed = TRUE;
            }
        }

      /* Режим просмотра в реальном времени. */
      else if (mode == HYSCAN_FORWARD_LOOK_PLAYER_REAL_TIME)
        {
          if (priv->cur_state.index != priv->data.last_index)
            {
              priv->cur_state.index = priv->data.last_index;
              priv->cur_state.index_changed = TRUE;
            }
        }

      /* Корректируем отображаемый индекс по диапазону данных. */
      if (priv->cur_state.index < priv->data.first_index)
        {
          priv->cur_state.index = priv->data.first_index;
          priv->cur_state.index_changed = TRUE;
        }
      if (priv->cur_state.index > priv->data.last_index)
        {
          priv->cur_state.index = priv->data.last_index;
          priv->cur_state.index_changed = TRUE;
        }

      /* Запрашиваем данные для текущего индекса. */
      if ((priv->cur_state.index_changed) && (!priv->data.doa_changed))
        {
          const HyScanForwardLookDOA *doa;
          guint32 n_points;
          gint64 doa_time;

          doa = hyscan_forward_look_data_get_doa (priv->data.data, priv->cur_state.index,
                                                         &n_points, &doa_time);
          if (doa != NULL)
            {
              g_mutex_lock (&priv->data.lock);

              priv->data.doa = doa;
              priv->data.n_points = n_points;
              priv->data.doa_index = priv->cur_state.index;
              priv->data.doa_time = doa_time;
              priv->data.doa_changed = TRUE;
              priv->cur_state.index_changed = FALSE;

              if (mode == HYSCAN_FORWARD_LOOK_PLAYER_START)
                start_time = doa_time;

              g_mutex_unlock (&priv->data.lock);
            }
        }

      /* Ожидание новых команд или данных. */
      delay -= g_timer_elapsed (delay_timer, NULL) * G_USEC_PER_SEC;
      if (delay > 0)
        g_usleep (delay);
    }

  g_clear_object  (&priv->data.data);

  g_clear_object  (&priv->cur_state.db);
  g_clear_pointer (&priv->cur_state.project_name, g_free);
  g_clear_pointer (&priv->cur_state.track_name, g_free);

  g_clear_object  (&priv->cur_state.cache);

  g_timer_destroy (delay_timer);
  g_timer_destroy (play_timer);

  return NULL;
}

/* Функция сигнализации об изменении данных. */
static gboolean
hyscan_forward_look_player_signaller (gpointer user_data)
{
  HyScanForwardLookPlayer *player = user_data;
  HyScanForwardLookPlayerPrivate *priv = player->priv;

  g_mutex_lock (&priv->data.lock);

  /* Сигнал об изменении диапазона индексов данных. */
  if (priv->data.range_changed)
    {
      g_signal_emit (player, hyscan_forward_look_player_signals[SIGNAL_RANGE], 0,
                     priv->data.first_index, priv->data.last_index);

      priv->data.range_changed = FALSE;
    }

  /* Сигнал об изменении данных. */
  if (priv->data.doa_changed)
    {
      if (priv->data.n_points != 0)
        {
          HyScanForwardLookPlayerInfo info;

          info.index = priv->data.doa_index;
          info.time = priv->data.doa_time;
          info.alpha = priv->data.alpha;
          info.distance = priv->data.doa[priv->data.n_points - 1].distance;

          g_signal_emit (player, hyscan_forward_look_player_signals[SIGNAL_DATA], 0,
                         &info, &priv->data.offset, priv->data.doa, priv->data.n_points);
        }
      else
        {
          g_signal_emit (player, hyscan_forward_look_player_signals[SIGNAL_DATA], 0,
                         NULL, NULL, NULL, 0);
        }

      priv->data.doa_changed = FALSE;
    }

  g_mutex_unlock (&priv->data.lock);

  return G_SOURCE_CONTINUE;
}

/* Функция создаёт новый объект обработки и воспроизведения данных. */
HyScanForwardLookPlayer *
hyscan_forward_look_player_new (void)
{
  return g_object_new (HYSCAN_TYPE_FORWARD_LOOK_PLAYER, NULL);
}

/* Функция устанавливает целевой уровень числа отображаемых кадров в секунду. */
void
hyscan_forward_look_player_set_fps (HyScanForwardLookPlayer *player,
                                    guint                    fps)
{
  HyScanForwardLookPlayerPrivate *priv;
  gint delay;

  g_return_if_fail (HYSCAN_IS_FORWARD_LOOK_PLAYER (player));

  priv = player->priv;

  fps = CLAMP (fps, 1, 100);
  delay = G_USEC_PER_SEC / fps;
  g_source_remove_by_user_data (player);

  g_mutex_lock (&priv->ctl_lock);

  priv->delay = delay;

  g_mutex_unlock (&priv->ctl_lock);

  if (priv->signaller_tag > 0)
    g_source_remove (priv->signaller_tag);
  priv->signaller_tag = g_timeout_add (delay / 1000, hyscan_forward_look_player_signaller, player);
}

/* Функция задаёт скорость звука в воде. */
void
hyscan_forward_look_player_set_sv (HyScanForwardLookPlayer *player,
                                   gdouble                  sound_velocity)
{
  HyScanForwardLookPlayerPrivate *priv;

  g_return_if_fail (HYSCAN_IS_FORWARD_LOOK_PLAYER (player));

  priv = player->priv;

  g_mutex_lock (&priv->ctl_lock);

  priv->new_state.sound_velocity = sound_velocity;
  priv->new_state.sound_velocity_changed = TRUE;

  g_mutex_unlock (&priv->ctl_lock);
}

/* Функция открывает галс для обработки и воспроизведения. */
void
hyscan_forward_look_player_open (HyScanForwardLookPlayer *player,
                                 HyScanDB                *db,
                                 HyScanCache             *cache,
                                 const gchar             *project_name,
                                 const gchar             *track_name)
{
  HyScanForwardLookPlayerPrivate *priv;

  g_return_if_fail (HYSCAN_IS_FORWARD_LOOK_PLAYER (player));

  priv = player->priv;

  g_mutex_lock (&priv->ctl_lock);

  g_clear_object (&priv->new_state.db);
  g_clear_object (&priv->new_state.cache);
  g_clear_pointer (&priv->new_state.project_name, g_free);
  g_clear_pointer (&priv->new_state.track_name, g_free);

  if (db != NULL)
      priv->new_state.db = g_object_ref (db);
  if (cache != NULL)
      priv->new_state.cache = g_object_ref (cache);
  if (project_name != NULL)
    priv->new_state.project_name = g_strdup (project_name);
  if (track_name != NULL)
    priv->new_state.track_name = g_strdup (track_name);

  priv->new_state.track_changed = TRUE;

  priv->mode = HYSCAN_FORWARD_LOOK_PLAYER_STOP;

  g_mutex_unlock (&priv->ctl_lock);
}

/* Функция закрывает текущий галс. */
void
hyscan_forward_look_player_close (HyScanForwardLookPlayer *player)
{
  hyscan_forward_look_player_open (player, NULL, NULL, NULL, NULL);
}

/* Функция включает режим отображения данных в реальном времени. */
void
hyscan_forward_look_player_real_time (HyScanForwardLookPlayer *player)
{
  HyScanForwardLookPlayerPrivate *priv;

  g_return_if_fail (HYSCAN_IS_FORWARD_LOOK_PLAYER (player));

  priv = player->priv;

  g_mutex_lock (&priv->ctl_lock);

  priv->mode = HYSCAN_FORWARD_LOOK_PLAYER_REAL_TIME;

  g_mutex_unlock (&priv->ctl_lock);
}

/* Функция включает режим воспроизведения записанных данных. */
void
hyscan_forward_look_player_play (HyScanForwardLookPlayer *player,
                                 gdouble                  speed)
{
  HyScanForwardLookPlayerPrivate *priv;

  g_return_if_fail (HYSCAN_IS_FORWARD_LOOK_PLAYER (player));

  priv = player->priv;

  g_mutex_lock (&priv->ctl_lock);

  priv->mode = HYSCAN_FORWARD_LOOK_PLAYER_START;
  priv->speed = speed;

  g_mutex_unlock (&priv->ctl_lock);
}

/* Функция приостанавливает воспроизведение записанных данных. */
void
hyscan_forward_look_player_pause (HyScanForwardLookPlayer *player)
{
  HyScanForwardLookPlayerPrivate *priv;

  g_return_if_fail (HYSCAN_IS_FORWARD_LOOK_PLAYER (player));

  priv = player->priv;

  g_mutex_lock (&priv->ctl_lock);

  priv->mode = HYSCAN_FORWARD_LOOK_PLAYER_PAUSE;

  g_mutex_unlock (&priv->ctl_lock);
}

/* Функция останавливает воспроизведение записанных данных. */
void
hyscan_forward_look_player_stop (HyScanForwardLookPlayer *player)
{
  HyScanForwardLookPlayerPrivate *priv;

  g_return_if_fail (HYSCAN_IS_FORWARD_LOOK_PLAYER (player));

  priv = player->priv;

  g_mutex_lock (&priv->ctl_lock);

  priv->mode = HYSCAN_FORWARD_LOOK_PLAYER_STOP;

  g_mutex_unlock (&priv->ctl_lock);
}

/* Функция перемещает текущую позицию воспроизведения в указанное место. */
void
hyscan_forward_look_player_seek (HyScanForwardLookPlayer *player,
                                 guint32                  index)
{
  HyScanForwardLookPlayerPrivate *priv;

  g_return_if_fail (HYSCAN_IS_FORWARD_LOOK_PLAYER (player));

  priv = player->priv;

  g_mutex_lock (&priv->ctl_lock);

  if (priv->mode == HYSCAN_FORWARD_LOOK_PLAYER_PLAY)
    priv->mode = HYSCAN_FORWARD_LOOK_PLAYER_START;

  priv->new_state.index = index;
  priv->new_state.index_changed = TRUE;

  g_mutex_unlock (&priv->ctl_lock);
}
