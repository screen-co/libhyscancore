/*
 * \file hyscan-tile-queue.c
 *
 * \brief Исходный файл класса очереди тайлов.
 * \author Dmitriev Alexander (m1n7@yandex.ru)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 *
 * === Таски (задачи) ===
 * Это ключевое понятие. Эта структура хранит информацию
 * о статусе, акутальности и сам тайл.
 * Жизненный цикл выглядит так:
 *
 * IDLE -(1)-> BUSY -(2)-> CLEANABLE
 *      \------(3)------/
 * Переходы 1 и 3 возможны только в hyscan_tile_queue_processing
 * Переход 2 возможен только в hyscan_tile_queue_task_processor
 *
 * При этом данные идут так:
 * new -(1)-> prequeue -(2)-> queue -(3)-> free
 *       ^               ^            ^ (3) в функции hyscan_tile_queue_processing;
 *       |               '------------- (2) в функции hyscan_tile_queue_add_finished;
 *       '----------------------------- (1) в функции hyscan_tile_queue_add;
 *
 * === Блокировки доступов ===
 * Методы класса предназначены для вызова из g_main_loop.
 * Это накладывает определенные ограничения: вся работа с БД должна
 * быть вынесена в отдельные потоки. Поэтому приходится разграничивать
 * доступ к разным данным.
 * +-----------+-------------+-----------------+-----------+
 * |   _open   | _processing | _task_processor |   _get    |
 * +-----------+-------------+-----------------+-----------+
 * | des_state | des_state   | cur_state       | cur_state |
 * |           | cur_state   | dctable         |           |
 * |           | dctable     |                 |           |
 * +-----------+-------------+-----------------+-----------+
 * Для всего (des_state, cur_state, dctable) есть свой мьютекс.
 *
 * Установив следующее:
 * а) _processing завершает генераторы перед синхронизацией,
 * б) вызовы извне должны минимально использовать мьютексы,
 *    поскольку это (с большой долей вероятности) g_main_loop,
 * можно выстроить следующую схему синхронизации состояний:
 *
 * 1) Ожидается завершение работы всех генераторов
 * 2) Ожидается завершение всех вызовов из main_loop, сбрасывается allow_work
 * 3) За мьютексом переписываются параметры из des_state в cur_state
 * 4) Копируются параметры из cur_state в dctable
 * 5) Поднимается allow_work
 *
 * === Внешняя синхронизация ===
 * Пользователю может быть полезно знать, в какой момент внутреннее состояние
 * стало идентично желаемому (тому, что настраивается методами _set*).
 * Для этого введено понятие хэша состояния.
 */

#include "hyscan-tile-queue.h"
#include <hyscan-core-marshallers.h>
#include <hyscan-waterfall-tile.h>
#include <hyscan-nmea-parser.h>
#include <string.h>
#include <zlib.h>

/* Множители для составления ключей для хэш-таблицы. */
#define xSOURCE 999983
#define xDEPTHOMETER 99991
#define xDEPTH  9973

#define WAIT_TIME 250 * G_TIME_SPAN_MILLISECOND

typedef struct
{
  HyScanTile tile;    /* Требуемый тайл. */
  guint64    view_id; /* Некий идентификатор. */
  gint       status;  /* Состояние задачи. */
  gint       gen_id;  /* Номер генератора. */
} HyScanTileQueueTask;

enum
{
  IDLE      = 1001,
  BUSY      = 1002,
  CLEANABLE = 1004
};

enum
{
  SIGNAL_READY,
  SIGNAL_IMAGE,
  SIGNAL_HASH,
  SIGNAL_LAST
};

enum
{
  PROP_MAX_GENERATORS = 1
};

typedef struct
{
  /* БД, проект, галс. */
  HyScanDB               *db;                    /* Интерфейс базы данных. */
  gchar                  *project;               /* Название проекта. */
  gchar                  *track;                 /* Название галса. */
  gboolean                raw;                   /* Использовать сырые данные. */

  /* Кэш. */
  HyScanCache            *cache;                 /* Интерфейс системы кэширования. */

  /* Определение глубины. */
  HyScanSourceType        depth_source;          /* Источник данных. */
  guint                   depth_channel;         /* Номер канала. */
  gulong                  depth_time;            /* Время округления. */
  gint                    depth_size;            /* Размер фильтра. */

  gfloat                  ship_speed;            /* Скорость движения. */

  GArray                 *sound_velocity;        /* Скорость звука. */
  gfloat                  sound_velocity1;       /* Скорость звука для тех, кто не умеет в профиль скорости звука. */

  gboolean                track_changed;         /* Флаг на смену галса. */
  gboolean                cache_changed;         /* Флаг на смену кэша. */
  gboolean                depth_source_changed;  /* Флаг на смену источника данных глубины. */
  gboolean                depth_time_changed;    /* Флаг на смену временного окна данных глубины. */
  gboolean                depth_size_changed;    /* Флаг на смену размера фильтра. */
  gboolean                speed_changed;         /* Флаг на смену скорости движения. */
  gboolean                velocity_changed;      /* Флаг на смену скорости звука. */

  guint32                 hash;                  /* Хэш состояния. Перед тем, как менять порядок переменных
                                                  * в этой структуре, посмотри, как считается хэш. */
} HyScanTileQueueState;

struct _HyScanTileQueuePrivate
{
  /* Параметры генерации. */
  HyScanTileQueueState    cur_state;             /* Текущее состояние. */
  HyScanTileQueueState    des_state;             /* Обновленное состояние. */
  GMutex                  state_lock;            /* Блокировка обновленного состояния. */
  gint                    state_changed;         /* Флаг на изменение состояния извне. */
  gboolean                allow_work;            /* Флаг разрешение/запрет работы. */

  /* Очередь заданий. */
  GArray                 *prequeue;              /* Список тайлов, которые необходимо будет сгенерировать. */
  GQueue                 *queue;                 /* Очередь заданий. */
  GMutex                  qlock;                 /* Блокировка множественного доступа к очереди. */
  gint                    qflag;                 /* Флаг на наличие новых заданий или освободившийся генератор. */
  GCond                   qcond;                 /* Аналогично. */

  /* Поток обработки заданий*/
  GThread                *processing;            /* Основной поток раздачи заданий. */
  gint                    stop;                  /* Галс открыт. */

  /* Генераторы*/
  HyScanWaterfallTile   **generator;             /* Массив генераторов. */
  gint                   *generator_state;       /* Статусы генераторов. */
  guint                   max_generators;        /* Максимальное число генераторов. */
  guint                   available_generators;  /* Количество незанятых генераторов. */

  GHashTable             *dctable;               /* Каналы данных и объекты определения глубины. */
  GMutex                  dc_lock;               /* Блокировка доступа к каналам данных. */

  guint64                 view_id;               /* Идентификатор. */
};

static void                hyscan_tile_queue_set_property       (GObject                *object,
                                                                 guint                   prop_id,
                                                                 const GValue           *value,
                                                                 GParamSpec             *pspec);
static void                hyscan_tile_queue_object_constructed (GObject                *object);
static void                hyscan_tile_queue_object_finalize    (GObject                *object);

static HyScanAcousticData *hyscan_tile_queue_open_dc            (HyScanTileQueueState   *state,
                                                                 HyScanSourceType        source);
static HyScanNavData        *hyscan_tile_queue_open_depth         (HyScanTileQueueState   *state);
static HyScanDepthometer  *hyscan_tile_queue_open_depthometer   (HyScanTileQueueState   *state,
                                                                 HyScanNavData            *depth);
static HyScanAcousticData *hyscan_tile_queue_get_dc             (HyScanTileQueuePrivate *priv,
                                                                 HyScanSourceType        source,
                                                                 gint                    gen_id);
static HyScanDepthometer  *hyscan_tile_queue_get_depthometer    (HyScanTileQueuePrivate *priv,
                                                                 gint                    gen_id);

static gint                hyscan_tile_queue_check_gen_state    (HyScanTileQueuePrivate *priv,
                                                                 gint                    index);
static void                hyscan_tile_queue_stop_all_gen       (HyScanTileQueuePrivate *priv);

static void                hyscan_tile_queue_sync_states        (HyScanTileQueue        *self);
static void                hyscan_tile_queue_apply_updates      (HyScanTileQueue        *self);
static gpointer            hyscan_tile_queue_processing         (gpointer                user_data);
static void                hyscan_tile_queue_task_processor     (gpointer                data,
                                                                 gpointer                user_data);
static gint                hyscan_tile_queue_task_finder        (gconstpointer           task,
                                                                 gconstpointer           tile);

static void                hyscan_tile_queue_state_hash         (HyScanTileQueueState   *state);
static gchar              *hyscan_tile_queue_cache_key          (const HyScanTile       *tile,
                                                                 guint32                 hash);

static guint   hyscan_tile_queue_signals[SIGNAL_LAST] = {0};

G_DEFINE_TYPE_WITH_PRIVATE (HyScanTileQueue, hyscan_tile_queue, G_TYPE_OBJECT);

static void
hyscan_tile_queue_class_init (HyScanTileQueueClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_tile_queue_set_property;

  object_class->constructed = hyscan_tile_queue_object_constructed;
  object_class->finalize = hyscan_tile_queue_object_finalize;

  /* Инициализируем сигналы. */
  hyscan_tile_queue_signals[SIGNAL_READY] =
    g_signal_new ("tile-queue-ready", HYSCAN_TYPE_TILE_QUEUE, G_SIGNAL_RUN_LAST, 0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0);
  hyscan_tile_queue_signals[SIGNAL_IMAGE] =
    g_signal_new ("tile-queue-image", HYSCAN_TYPE_TILE_QUEUE, G_SIGNAL_RUN_LAST, 0,
                  NULL, NULL,
                  hyscan_core_marshal_VOID__POINTER_POINTER_INT_ULONG,
                  G_TYPE_NONE,
                  4, G_TYPE_POINTER, G_TYPE_POINTER, G_TYPE_INT, G_TYPE_ULONG);
  hyscan_tile_queue_signals[SIGNAL_HASH] =
    g_signal_new ("tile-queue-hash", HYSCAN_TYPE_TILE_QUEUE, G_SIGNAL_RUN_LAST, 0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__ULONG,
                  G_TYPE_NONE,
                  1, G_TYPE_ULONG);
  g_object_class_install_property (object_class, PROP_MAX_GENERATORS,
      g_param_spec_uint ("max_generators", "MaxGenerators", "Maximum number of tile generators", 1, 128, 1,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

}

static void
hyscan_tile_queue_init (HyScanTileQueue *self)
{
  self->priv = hyscan_tile_queue_get_instance_private (self);
}

static void
hyscan_tile_queue_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  HyScanTileQueue *self = HYSCAN_TILE_QUEUE (object);
  HyScanTileQueuePrivate *priv = self->priv;

  if (prop_id == PROP_MAX_GENERATORS)
    priv->max_generators = g_value_get_uint (value);
  else
    G_OBJECT_WARN_INVALID_PROPERTY_ID (self, prop_id, pspec);
}

static void
hyscan_tile_queue_object_constructed (GObject *object)
{
  HyScanTileQueue *self = HYSCAN_TILE_QUEUE (object);
  HyScanTileQueuePrivate *priv = self->priv;
  guint i;

  g_mutex_init (&priv->dc_lock);
  g_mutex_init (&priv->qlock);

  g_mutex_init (&priv->state_lock);
  g_cond_init (&priv->qcond);

  /* Инициализируем очередь. */
  priv->queue = g_queue_new ();
  priv->prequeue = g_array_new (FALSE, TRUE, sizeof(HyScanTile));

  priv->dctable = g_hash_table_new_full (NULL, NULL, NULL, g_object_unref);
  priv->generator = g_malloc0 (priv->max_generators * sizeof (HyScanWaterfallTile*));
  priv->generator_state = g_malloc0 (priv->max_generators * sizeof (gint));
  priv->available_generators = priv->max_generators;

  /* Создаем объекты генерации тайлов. */
  for (i = 0; i < priv->max_generators; i++)
    {
      priv->generator_state[i] = IDLE;
      priv->generator[i] = hyscan_waterfall_tile_new ();
    }

  /* По умолчанию считаем скорость судна равной 1 м/с, звука - 1500 м/с. */
  hyscan_tile_queue_set_ship_speed (self, 1.0);
  hyscan_tile_queue_set_sound_velocity (self, NULL);

  g_atomic_int_set (&priv->stop, FALSE);
  priv->processing = g_thread_new ("tilequeue", hyscan_tile_queue_processing, self);

  priv->allow_work = TRUE;

  G_OBJECT_CLASS (hyscan_tile_queue_parent_class)->constructed (object);
}

static void
hyscan_tile_queue_object_finalize (GObject *object)
{
  HyScanTileQueue *self = HYSCAN_TILE_QUEUE (object);
  HyScanTileQueuePrivate *priv = self->priv;

  guint i;

  g_atomic_int_set (&priv->stop, TRUE);
  g_clear_pointer (&priv->processing, g_thread_join);

  /* Глушим генераторы, закрываем КД. */
  for (i = 0; i < priv->max_generators; i++)
    g_clear_object (&priv->generator[i]);

  g_hash_table_destroy (priv->dctable);
  g_free (priv->generator);
  g_free (priv->generator_state);
  g_queue_free_full (priv->queue, g_free);
  g_array_unref (priv->prequeue);

  /* Чистим стейты. */
  g_clear_object (&priv->cur_state.db);
  g_clear_pointer (&priv->cur_state.project, g_free);
  g_clear_pointer (&priv->cur_state.track, g_free);
  g_clear_object (&priv->cur_state.cache);
  g_clear_pointer (&priv->cur_state.sound_velocity, g_array_unref);

  g_clear_object (&priv->des_state.db);
  g_clear_pointer (&priv->des_state.project, g_free);
  g_clear_pointer (&priv->des_state.track, g_free);
  g_clear_object (&priv->des_state.cache);
  g_clear_pointer (&priv->des_state.sound_velocity, g_array_unref);

  /* Очищаем список заданий. */

  g_cond_clear (&priv->qcond);
  g_mutex_clear (&priv->qlock);
  g_mutex_clear (&priv->dc_lock);
  g_mutex_clear (&priv->state_lock);

  G_OBJECT_CLASS (hyscan_tile_queue_parent_class)->finalize (object);
}

/* Функция открывает канал данных. */
static HyScanAcousticData*
hyscan_tile_queue_open_dc (HyScanTileQueueState *state,
                           HyScanSourceType      source)
{
  HyScanAcousticData *dc;

  dc = hyscan_acoustic_data_new (state->db, state->project, state->track, source, state->raw);

  if (dc == NULL)
    return NULL;

  /* Если получилось открыть, устанавливаем кэш. */
  hyscan_acoustic_data_set_cache (dc, state->cache, "PREFIX");

  return dc;
}

/* Функция создает объект определения глубины. */
static HyScanNavData*
hyscan_tile_queue_open_depth (HyScanTileQueueState *state)
{
  HyScanSourceType source = state->depth_source;
  HyScanNavData *depth = NULL;

  if (HYSCAN_SOURCE_NMEA_DPT == source)
    {
      HyScanNMEAParser *dnmea;
      dnmea = hyscan_nmea_parser_new (state->db, state->project, state->track,
                                      state->depth_channel,
                                      HYSCAN_SOURCE_NMEA_DPT,
                                      HYSCAN_NMEA_FIELD_DEPTH);
      depth = HYSCAN_NAV_DATA (dnmea);
    }

  /* Устанавливаем кэш, создаем объект измерения глубины. */
  hyscan_nav_data_set_cache (depth, state->cache);

  return depth;
}

/* Функция создает объект определения глубины. */
static HyScanDepthometer*
hyscan_tile_queue_open_depthometer (HyScanTileQueueState *state,
                                    HyScanNavData          *depth)
{
  HyScanDepthometer *meter = NULL;

  if (depth == NULL)
    return NULL;

  meter = hyscan_depthometer_new (depth);
  if (meter == NULL)
    return NULL;

  hyscan_depthometer_set_cache (meter, state->cache);
  hyscan_depthometer_set_filter_size (meter, state->depth_size);
  hyscan_depthometer_set_validity_time (meter, state->depth_time);

  return meter;
}

/* Функция возвращает КД из ХТ или создает и помещает туда. */
static HyScanAcousticData*
hyscan_tile_queue_get_dc (HyScanTileQueuePrivate *priv,
                          HyScanSourceType        source,
                          gint                    gen_id)
{
  HyScanAcousticData *dc = NULL;
  gint64 key = gen_id + source * xSOURCE;

  dc = g_hash_table_lookup (priv->dctable, GINT_TO_POINTER (key));

  /* Если нашли КД, отдаем его. */
  if (dc != NULL)
    return dc;

  /* Иначе пробуем открыть. */
  dc = hyscan_tile_queue_open_dc (&priv->cur_state, source);

  /* Либо он не открылся и тут ничего не попишешь...  */
  if (dc == NULL)
    return NULL;

  /* Либо открылся! */
  g_hash_table_insert (priv->dctable, GINT_TO_POINTER (key), dc);

  return dc;
}

/* Функция возвращает глубиномер из ХТ или создает и помещает туда. */
static HyScanDepthometer*
hyscan_tile_queue_get_depthometer (HyScanTileQueuePrivate *priv,
                                   gint                    gen_id)
{
  HyScanNavData *depth = NULL;
  HyScanDepthometer *meter = NULL;

  gint64 depth_key = gen_id + xDEPTH;
  gint64 meter_key = gen_id + xDEPTHOMETER;

  meter = g_hash_table_lookup (priv->dctable, GINT_TO_POINTER (meter_key));

  /* Если нашли объект, отдаем его. */
  if (meter != NULL)
    return meter;

  /* Иначе создаем интерфейс и depthometer. */
  depth = hyscan_tile_queue_open_depth (&priv->cur_state);

  if (depth == NULL)
    return NULL;

  meter = hyscan_tile_queue_open_depthometer (&priv->cur_state, depth);

  /* Либо он не открылся и тут ничего не попишешь...  */
  if (meter == NULL)
    {
      g_clear_object (&depth);
      return NULL;
    }

  /* Либо открылся! */
  g_hash_table_insert (priv->dctable, GINT_TO_POINTER (depth_key), depth);
  g_hash_table_insert (priv->dctable, GINT_TO_POINTER (meter_key), meter);

  return meter;
}

/* Функция проверяет статус генератора и обрабатывает смену состояний. */
static gint
hyscan_tile_queue_check_gen_state (HyScanTileQueuePrivate *priv,
                                   gint                    index)
{
  /* Получаем состояние. */
  gint state = g_atomic_int_get (&(priv->generator_state[index]));

  /* Специальной обработки требует только статус CLEANABLE.
   * Я специально возвращаю CLEANABLE, хотя фактически уже IDLE.
   * Это нужно для того, чтобы _processing смог
   * (при необходимости) эмиттировать сигнал. */
  if (CLEANABLE == state)
    {
      priv->generator_state[index] = IDLE;
      priv->available_generators++;
    }

  /* Возвращаем состояние генератора. */
  return state;
}

/* Функция останавливает все генераторы. */
static void
hyscan_tile_queue_stop_all_gen (HyScanTileQueuePrivate *priv)
{
  guint i;
  gboolean status;

  /* Даем команду на остановку. */
  for (i = 0; i < priv->max_generators; i++)
    hyscan_waterfall_tile_terminate (priv->generator[i]);

  /* Дожидаемся фактического завершения генерации. */
  while (TRUE)
    {
      status = TRUE;
      for (i = 0; i < priv->max_generators; i++)
        {
          if (BUSY == hyscan_tile_queue_check_gen_state (priv, i))
            status = FALSE;
        }

      if (status)
        break;
    }
}

/* Функция синхронизирует состояния. */
static void
hyscan_tile_queue_sync_states (HyScanTileQueue *self)
{
  HyScanTileQueuePrivate *priv = self->priv;
  HyScanTileQueueState *new_st = &priv->des_state;
  HyScanTileQueueState *cur_st = &priv->cur_state;

  if (new_st->track_changed)
    {
      /* Очищаем текущее. */
      g_clear_object (&cur_st->db);
      g_clear_pointer (&cur_st->project, g_free);
      g_clear_pointer (&cur_st->track, g_free);

      /* Копируем из нового. */
      cur_st->db = g_object_ref (new_st->db);
      cur_st->project = g_strdup (new_st->project);
      cur_st->track = g_strdup (new_st->track);
      cur_st->raw = new_st->raw;

      /* Выставляем флаги. */
      new_st->track_changed = FALSE;
      cur_st->track_changed = TRUE;
    }

  if (new_st->cache_changed)
    {
      g_clear_object (&cur_st->cache);

      cur_st->cache = g_object_ref (new_st->cache);

      new_st->cache_changed = FALSE;
      cur_st->cache_changed = TRUE;
    }

  if (new_st->depth_source_changed)
    {
      cur_st->depth_source = new_st->depth_source;
      cur_st->depth_channel = new_st->depth_channel;
      cur_st->depth_time = new_st->depth_time;
      cur_st->depth_size = new_st->depth_size;

      new_st->depth_source_changed = FALSE;
      new_st->depth_time_changed = FALSE;
      new_st->depth_size_changed = FALSE;
      cur_st->depth_source_changed = TRUE;
      cur_st->depth_time_changed = TRUE;
      cur_st->depth_size_changed = TRUE;
    }

  if (new_st->depth_time_changed)
    {
      cur_st->depth_time = new_st->depth_time;
      cur_st->depth_size = new_st->depth_size;

      new_st->depth_time_changed = FALSE;
      cur_st->depth_time_changed = TRUE;
    }
  if (new_st->depth_size_changed)
    {
      cur_st->depth_time = new_st->depth_time;
      cur_st->depth_size = new_st->depth_size;

      new_st->depth_size_changed = FALSE;
      cur_st->depth_size_changed = TRUE;
    }

  if (new_st->speed_changed)
    {
      cur_st->ship_speed = new_st->ship_speed;

      cur_st->speed_changed = TRUE;
      new_st->speed_changed = FALSE;
    }
  if (new_st->velocity_changed)
    {
      if (cur_st->sound_velocity != NULL)
        g_clear_pointer (&cur_st->sound_velocity, g_array_unref);

      cur_st->sound_velocity = g_array_ref (new_st->sound_velocity);
      cur_st->sound_velocity1 = new_st->sound_velocity1;

      cur_st->velocity_changed = TRUE;
      new_st->velocity_changed = FALSE;
    }
}

/* Функция применяет обновления в текущем состоянии. */
static void
hyscan_tile_queue_apply_updates (HyScanTileQueue *self)
{
  GHashTable *dctable = self->priv->dctable;
  HyScanTileQueueState *state = &self->priv->cur_state;
  GHashTableIter iter;
  gpointer key, value;

  /* В случае смены галса надо уничтожить все старые
   * объекты и проигнорировать все остальные изменения.
   * Потоки генерации тайлов не найдут нужные объекты
   * и сами их создадут. */
  if (state->track_changed)
    {
      g_hash_table_remove_all (dctable);

      goto exit;
    }

  /* Если изменился источник глубины, уничтожаем
   * все объекты измерения глубины и интерфейсы. */
  if (state->depth_source_changed)
    {
      g_hash_table_iter_init (&iter, dctable);
      while (g_hash_table_iter_next (&iter, &key, &value))
        {
          if (HYSCAN_IS_NAV_DATA (value) || HYSCAN_IS_DEPTHOMETER (value))
            g_hash_table_remove (dctable, key);
        }
      goto cache_setup;
    }

  /* Для упрощения будем настраивать HyScanDepthometer'ы все сразу. */
  if (state->depth_time_changed || state->depth_size_changed)
    {
      g_hash_table_iter_init (&iter, dctable);
      while (g_hash_table_iter_next (&iter, &key, &value))
        {
          if (HYSCAN_IS_DEPTHOMETER (value))
            {
              hyscan_depthometer_set_filter_size (HYSCAN_DEPTHOMETER (value), state->depth_size);
              hyscan_depthometer_set_validity_time (HYSCAN_DEPTHOMETER (value), state->depth_time);
            }
        }
      goto cache_setup;
    }

cache_setup:
  if (state->cache_changed)
    {
      g_hash_table_iter_init (&iter, dctable);
      while (g_hash_table_iter_next (&iter, &key, &value))
        {
          if (HYSCAN_IS_DEPTHOMETER (value))
            hyscan_depthometer_set_cache (HYSCAN_DEPTHOMETER (value), state->cache);
          if (HYSCAN_IS_NAV_DATA (value))
            hyscan_nav_data_set_cache (HYSCAN_NAV_DATA (value), state->cache);
        }
      goto exit;
    }

exit:
  state->track_changed = FALSE;
  state->cache_changed = FALSE;
  state->depth_source_changed = FALSE;
  state->depth_time_changed = FALSE;
  state->depth_size_changed = FALSE;
  state->speed_changed = FALSE;
  state->velocity_changed = FALSE;

  hyscan_tile_queue_state_hash (state);
}

/* Поток обработки заданий. */
static gpointer
hyscan_tile_queue_processing (gpointer user_data)
{
  GThreadPool *pool;
  HyScanTileQueue *self = user_data;
  HyScanTileQueuePrivate *priv = self->priv;

  HyScanTileQueueTask *task;
  GList *link, *next;
  guint i;
  gint64 end_time;

  /* Запускаем пул потоков для генерации тайлов. */
  pool = g_thread_pool_new (hyscan_tile_queue_task_processor, self, priv->max_generators, TRUE, NULL);

  /* Обрабатываем до тех пор, пока не будет установлен флаг остановки. */
  while (!g_atomic_int_get (&priv->stop))
    {
      /* Проверяем, требуется ли синхронизация. */
      if (g_atomic_int_get (&priv->state_changed))
        {
          /* 1) Ожидается завершение работы всех генераторов
           * 2) Ожидается завершение всех вызовов из main_loop, сбрасывается allow_work
           * 3) За мьютексом переписываются параметры из des_state в cur_state.
           * 4) Копируются параметры из cur_state в dctable
           * 5) Поднимается allow_work
           */

          hyscan_tile_queue_stop_all_gen (priv);      /*< 1. */

          while (!g_atomic_int_compare_and_exchange (&priv->allow_work, TRUE, FALSE))
            ;                                         /*< 2, 3. */

          g_mutex_lock (&priv->state_lock);
          hyscan_tile_queue_sync_states (self);       /*< 4. */
          priv->state_changed = FALSE;
          g_mutex_unlock (&priv->state_lock);

          hyscan_tile_queue_apply_updates (self);     /*< 5. */
          g_atomic_int_set (&priv->allow_work, TRUE); /*< 6. */
        }

      /* Ждем появления новых заданий или завершения работы потоков генерации. */
      end_time = g_get_monotonic_time () + WAIT_TIME;

      g_mutex_lock (&(priv->qlock));

      if (g_atomic_int_get (&priv->qflag) == 0)
        if (!g_cond_wait_until (&priv->qcond, &priv->qlock, end_time))
          goto next_iteration;

      /* Во время ожидания параметры могли измениться. Чтобы не генерировать
       * со старыми параметрами, выполняем следующую проверку. */
      if (g_atomic_int_get (&priv->state_changed))
        goto next_iteration;

      g_atomic_int_set (&priv->qflag, 0);

      /* Смотрим, нет ли у нас генераторов, завершивших работу. */
      for (i = 0; i < priv->max_generators; i++)
        {
          if (CLEANABLE == hyscan_tile_queue_check_gen_state (priv, i))
            g_signal_emit (self, hyscan_tile_queue_signals[SIGNAL_READY], 0, NULL);
        }

      /* Обрабатываем текущие задачи: CLEANABLE удаляем, IDLE раздаем, BUSY пропускаем. */
      for (link = priv->queue->head; link != NULL; link = next)
        {
          next = link->next;
          task = link->data;

          /* Старые задачи. */
          if (task->view_id != priv->view_id)
            {
              /* Здесь возможно 3 случая:
               * 1. Наиболее типичный: тайл уже генерируется.
               * 2. Самый редкий: задача отдана  на генерацию
               *    (g_thread_pool_push, чуть ниже), но генератор
               *    ещё не найден и не занят.
               * 3. Задача не отдавалась на генерацию.
               *
               * Я специально не проверяю task->gen_id < priv->max_generators.
               * Если это так, то я надеюсь словить сегфолт.
               */

              if (task->status == BUSY && task->gen_id >= 0)      /*< 1. */
                hyscan_waterfall_tile_terminate (priv->generator[task->gen_id]);
              else if (task->status == BUSY && task->gen_id < 0)  /*< 2. */
                ;
              else /* if (task->status == IDLE) */                /*< 3. */
                g_atomic_int_set (&task->status, CLEANABLE);
            }

          /* Актуальные задачи. */
          if (task->status == IDLE && priv->available_generators > 0)
            {
              g_atomic_int_set (&task->status, BUSY);
              priv->available_generators--;
              g_thread_pool_push (pool, task, NULL);
            }
          else if (task->status == CLEANABLE)
            {
              g_free (link->data);
              g_queue_delete_link (priv->queue, link);
            }
        }

    next_iteration:
      g_mutex_unlock (&(priv->qlock));
    }

  /* Тормозим все генераторы. */
  hyscan_tile_queue_stop_all_gen (priv);

  /* Завершаем пул потоков. */
  g_thread_pool_free (pool, TRUE, FALSE);
  return NULL;
}

/* Функция выполнения одного задания. */
static void
hyscan_tile_queue_task_processor (gpointer data,
                                  gpointer user_data)
{
  HyScanTileQueue *self = user_data;
  HyScanTileQueuePrivate *priv = self->priv;
  HyScanTileQueueTask *task = data;
  HyScanTileQueueState *state = &priv->cur_state;
  HyScanTile tile;

  HyScanSourceType source;

  HyScanAcousticData *dc = NULL;
  HyScanDepthometer  *depth = NULL;

  /* Поскольку гарантируется, что одновременно task_processor и
   * _processing НЕ полезут в cur_state, блокировку не ставим.
   * Чтение при этом считается тредсейф операцией. */
  HyScanCache *cache = state->cache;
  gfloat ship_speed = state->ship_speed;
  gfloat sound_velocity1 = state->sound_velocity1;
  gchar *key;

  gfloat *image;
  guint32 image_size;
  guint i;
  gint gen_id = -1;
  gboolean setup = TRUE;

  /* Помечаем задачу, ищем генератор и сразу же занимаем его.*/
  for (i = 0; i < priv->max_generators; i++)
    if (g_atomic_int_compare_and_exchange (&priv->generator_state[i], IDLE, BUSY))
      {
        gen_id = i;
        break;
      }

  /* Если генератор не найден, выходим. */
  if (gen_id == -1)
    {
      g_warning ("HyScanTileQueue: Gen not found");
      goto exit;
    }

  g_atomic_int_set (&task->gen_id, gen_id);

  /* Начинаем работу с хэш-таблицей каналов данных.
   * Блокировать надо, т.к. несколько потоков пула могут
   * одновременно туда писать. */
  g_mutex_lock (&priv->dc_lock);

  source = task->tile.source;

  dc = hyscan_tile_queue_get_dc (priv, source, gen_id);
  if (dc == NULL)
    {
      g_mutex_unlock (&priv->dc_lock);
      goto exit;
    }

  /* В случае тайла в наклонной дальности глубиномер не требуется. */
  if (task->tile.type == HYSCAN_TILE_GROUND)
    {
      depth = hyscan_tile_queue_get_depthometer (priv, gen_id);
      if (depth == NULL)
        {
          g_mutex_unlock (&priv->dc_lock);
          goto exit;
        }
    }

  /* Закончили работу с хэш-таблицей. */
  g_mutex_unlock (&priv->dc_lock);

  /* Устанавливаем параметры генерации. */
  setup &= hyscan_waterfall_tile_set_depth (priv->generator[gen_id], depth);
  setup &= hyscan_waterfall_tile_set_speeds (priv->generator[gen_id], ship_speed, sound_velocity1);
  setup &= hyscan_waterfall_tile_set_tile (priv->generator[gen_id], dc, task->tile);

  if (!setup)
    {
      g_warning ("HyScanTileQueue: Setup failed");
      goto exit;
    }

  /* Генерируем тайл. */
  image = hyscan_waterfall_tile_generate (priv->generator[gen_id], &tile, &image_size);

  /* В случае досрочного прекращения генерации вернется NULL. Тогда мы просто выходим. */
  if (image == NULL)
    goto exit;

  /* Иначе эмиттируем сигнал с тайлом. */
  g_signal_emit (self, hyscan_tile_queue_signals[SIGNAL_IMAGE], 0, &tile, image, image_size, state->hash);

  /* И кладем в кэш при возможности. */
  if (cache != NULL)
    {
      key = hyscan_tile_queue_cache_key (&tile, state->hash);
      hyscan_cache_set2 (cache, key, NULL, &tile, sizeof (HyScanTile), image, image_size);
      g_free (key);
    }

  g_free (image);

exit:

  /* Помечаем генератор как бездействующий. */
  if (gen_id != -1)
    g_atomic_int_set (&(priv->generator_state[gen_id]), CLEANABLE);

  /* Помечаем задачу как пригодную к удалению. */
  g_mutex_lock (&priv->qlock);
  g_atomic_int_set (&task->status, CLEANABLE);
  g_atomic_int_set (&priv->qflag, 1);
  g_cond_signal (&priv->qcond);
  g_mutex_unlock (&priv->qlock);
}

/* Функция сравнивает параметры нового тайла и уже имеющегося. */
static gint
hyscan_tile_queue_task_finder (gconstpointer _task,
                               gconstpointer _tile)
{
  const HyScanTileQueueTask* task = _task;
  const HyScanTile* tile = _tile;

  if (task->tile.across_start != tile->across_start ||
      task->tile.along_start  != tile->along_start  ||
      task->tile.across_end   != tile->across_end   ||
      task->tile.along_end    != tile->along_end    ||
      task->tile.ppi          != tile->ppi          ||
      task->tile.scale        != tile->scale)
    {
      return 1;
    }
  return 0;
}

/* Функция вычисляет хэш состояния. */
static void
hyscan_tile_queue_state_hash (HyScanTileQueueState *state)
{
  gchar *str;
  guint32 hash;

  if (state->project == NULL || state->track == NULL)
    {
      state->hash = 0;
      return;
    }

  str = g_strdup_printf ("%p.%s.%s.%i.%i.%u.%lu.%i.%f",
                          state->db,
                          state->project,
                          state->track,
                          state->raw,
                          state->depth_source,
                          state->depth_channel,
                          state->depth_time,
                          state->depth_size,
                          state->ship_speed);

  hash = crc32 (0L, (gpointer)(str), strlen (str));
  g_free (str);

  if (state->sound_velocity != NULL)
    hash = crc32 (hash, (gpointer)(state->sound_velocity->data), state->sound_velocity->len * sizeof (HyScanSoundVelocity));

  state->hash = hash;
}

/* Функция генерирует ключ для системы кэширования. */
static gchar*
hyscan_tile_queue_cache_key (const HyScanTile *tile,
                             guint32           hash)
{
  gchar *key = g_strdup_printf ("tilequeue.%u|%i.%i.%i.%i.%010.3f.%06.3f|%u.%i.%i.%i",
                                hash,

                                tile->across_start,
                                tile->along_start,
                                tile->across_end,
                                tile->along_end,
                                tile->scale,
                                tile->ppi,

                                tile->upsample,
                                tile->type,
                                tile->rotate,
                                tile->source);
  return key;
}

/* Функция создает новый объект HyScanTileQueue. */
HyScanTileQueue*
hyscan_tile_queue_new (gint max_generators)
{
  return g_object_new (HYSCAN_TYPE_TILE_QUEUE,
                       "max_generators", max_generators,
                       NULL);
}

/* Функция устанавливает кэш. */
void
hyscan_tile_queue_set_cache (HyScanTileQueue *self,
                             HyScanCache     *cache)
{
  HyScanTileQueuePrivate *priv;
  HyScanTileQueueState *state;

  g_return_if_fail (HYSCAN_IS_TILE_QUEUE (self));
  priv = self->priv;
  state = &priv->des_state;

  g_mutex_lock (&priv->state_lock);

  g_clear_object (&state->cache);

  state->cache = (cache != NULL) ? g_object_ref (cache) : NULL;
  state->cache_changed = TRUE;
  priv->state_changed = TRUE;

  hyscan_tile_queue_state_hash (state);
  g_signal_emit (self, hyscan_tile_queue_signals[SIGNAL_HASH], 0, state->hash);

  g_mutex_unlock (&priv->state_lock);
}

/* Функция настраивает глубину. */
void
hyscan_tile_queue_set_depth_source (HyScanTileQueue   *self,
                                    HyScanSourceType   source,
                                    guint              channel)
{
  HyScanTileQueuePrivate *priv;
  g_return_if_fail (HYSCAN_IS_TILE_QUEUE (self));
  priv = self->priv;

  g_mutex_lock (&priv->state_lock);

  priv->des_state.depth_source = source;
  priv->des_state.depth_channel = channel;
  priv->des_state.depth_source_changed = TRUE;
  priv->state_changed = TRUE;

  hyscan_tile_queue_state_hash (&priv->des_state);
  g_signal_emit (self, hyscan_tile_queue_signals[SIGNAL_HASH], 0, priv->des_state.hash);

  g_mutex_unlock (&priv->state_lock);
}

/* Функция настраивает глубину. */
void
hyscan_tile_queue_set_depth_filter_size (HyScanTileQueue   *self,
                                         guint              size)
{
  HyScanTileQueuePrivate *priv;

  g_return_if_fail (HYSCAN_IS_TILE_QUEUE (self));
  priv = self->priv;

  g_mutex_lock (&priv->state_lock);

  priv->des_state.depth_size = size;
  priv->des_state.depth_size_changed = TRUE;
  priv->state_changed = TRUE;

  hyscan_tile_queue_state_hash (&priv->des_state);
  g_signal_emit (self, hyscan_tile_queue_signals[SIGNAL_HASH], 0, priv->des_state.hash);

  g_mutex_unlock (&priv->state_lock);
}

/* Функция настраивает глубину. */
void
hyscan_tile_queue_set_depth_time (HyScanTileQueue   *self,
                                  gulong             usecs)
{
  HyScanTileQueuePrivate *priv;

  g_return_if_fail (HYSCAN_IS_TILE_QUEUE (self));
  priv = self->priv;

  g_mutex_lock (&priv->state_lock);

  priv->des_state.depth_time = usecs;
  priv->des_state.depth_time_changed = TRUE;
  priv->state_changed = TRUE;

  hyscan_tile_queue_state_hash (&priv->des_state);
  g_signal_emit (self, hyscan_tile_queue_signals[SIGNAL_HASH], 0, priv->des_state.hash);

  g_mutex_unlock (&priv->state_lock);
}

/* Функция устанавливает скорости. */
void
hyscan_tile_queue_set_ship_speed (HyScanTileQueue *self,
                                  gfloat           ship)
{
  HyScanTileQueuePrivate *priv;

  g_return_if_fail (HYSCAN_IS_TILE_QUEUE (self));
  priv = self->priv;

  if (ship <= 0.0)
    return;

  g_mutex_lock (&priv->state_lock);

  priv->des_state.ship_speed = ship;
  priv->des_state.speed_changed = TRUE;
  priv->state_changed = TRUE;

  hyscan_tile_queue_state_hash (&priv->des_state);
  g_signal_emit (self, hyscan_tile_queue_signals[SIGNAL_HASH], 0, priv->des_state.hash);

  g_mutex_unlock (&priv->state_lock);
}

/* Функция устанавливает скорости. */
void
hyscan_tile_queue_set_sound_velocity (HyScanTileQueue *self,
                                      GArray          *sound)
{
  HyScanTileQueuePrivate *priv;
  HyScanTileQueueState *state;
  HyScanSoundVelocity *link;

  g_return_if_fail (HYSCAN_IS_TILE_QUEUE (self));
  priv = self->priv;
  state = &priv->des_state;

  g_mutex_lock (&priv->state_lock);

  if (state->sound_velocity != NULL)
    g_clear_pointer (&state->sound_velocity, g_array_unref);

  if (sound == NULL || sound->len == 0)
    {
      HyScanSoundVelocity sv = {.depth = 0.0, .velocity = 1500.0};
      state->sound_velocity = g_array_new (FALSE, FALSE, sizeof (HyScanSoundVelocity));
      g_array_append_val (state->sound_velocity, sv);
    }
  else
    {
      state->sound_velocity = g_array_ref (sound);
    }

  link = &g_array_index (state->sound_velocity, HyScanSoundVelocity, 0);
  state->sound_velocity1 = link->velocity;

  state->velocity_changed = TRUE;
  priv->state_changed = TRUE;

  hyscan_tile_queue_state_hash (state);
  g_signal_emit (self, hyscan_tile_queue_signals[SIGNAL_HASH], 0, state->hash);

  g_mutex_unlock (&priv->state_lock);
}

/* Функция открывает галс. */
void
hyscan_tile_queue_open (HyScanTileQueue  *self,
                        HyScanDB         *db,
                        const gchar      *project,
                        const gchar      *track,
                        gboolean          raw)
{
  HyScanTileQueuePrivate *priv;
  HyScanTileQueueState *state;

  g_return_if_fail (HYSCAN_IS_TILE_QUEUE (self));
  priv = self->priv;
  state = &priv->des_state;

  g_mutex_lock (&priv->state_lock);

  g_clear_pointer (&state->db, g_object_unref);
  g_clear_pointer (&state->project, g_free);
  g_clear_pointer (&state->track, g_free);

  if (db != NULL)
    state->db = g_object_ref (db);
  if (project != NULL)
    state->project = g_strdup (project);
  if (track != NULL)
    state->track = g_strdup (track);
  state->raw = raw;
  state->track_changed = TRUE;

  priv->state_changed = TRUE;

  hyscan_tile_queue_state_hash (state);
  g_signal_emit (self, hyscan_tile_queue_signals[SIGNAL_HASH], 0, state->hash);

  g_mutex_unlock (&priv->state_lock);
}

/* Функция закрывает галс. */
void
hyscan_tile_queue_close (HyScanTileQueue *self)
{
  hyscan_tile_queue_open (self, NULL, NULL, NULL, FALSE);
}

/* Функция ищет тайл в кэше. */
gboolean
hyscan_tile_queue_check (HyScanTileQueue *self,
                         HyScanTile      *requested_tile,
                         HyScanTile      *cached_tile,
                         gboolean        *regenerate)
{
  gchar *key;
  gboolean found = FALSE;
  gboolean regen = TRUE;
  guint32 size = sizeof (HyScanTile);
  HyScanTileQueuePrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_TILE_QUEUE (self), FALSE);
  priv = self->priv;

  if (requested_tile == NULL || cached_tile == NULL)
    return FALSE;

  /* Проверяем, разрешена ли работа. */
  if (!g_atomic_int_compare_and_exchange (&priv->allow_work, TRUE, FALSE))
    return FALSE;

  if (priv->cur_state.cache == NULL)
    goto exit;

  key = hyscan_tile_queue_cache_key (requested_tile, priv->des_state.hash);

  /* Ищем тайл в кэше. */
  found = hyscan_cache_get (priv->cur_state.cache, key, NULL, cached_tile, &size);

  /* Разрешаем всю работу. */
  g_atomic_int_set (&priv->allow_work, TRUE);

  /* Если тайл найден, сравниваем его с запрошенным.
   * При этом убираем из сравнения то, что попало в ключ: координаты, масштаб и ppi. */
  if (found && cached_tile->finalized)
    regen = FALSE;

  if (regenerate != NULL)
    *regenerate = regen;

  g_free (key);

exit:
  g_atomic_int_set (&priv->allow_work, TRUE);
  return found;
}

/* Функция забирает тайл из кэша. */
gboolean
hyscan_tile_queue_get (HyScanTileQueue  *self,
                       HyScanTile       *requested_tile,
                       HyScanTile       *cached_tile,
                       gfloat          **buffer,
                       guint32          *size)
{
  gchar *key;
  gboolean status = FALSE;
  guint32 size1, size2;
  HyScanTileQueuePrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_TILE_QUEUE (self), FALSE);
  priv = self->priv;

  if (requested_tile == NULL || cached_tile == NULL)
    return FALSE;

  /* Проверяем, разрешена ли работа. */
  if (!g_atomic_int_compare_and_exchange (&priv->allow_work, TRUE, FALSE))
    return FALSE;

  if (priv->cur_state.cache == NULL)
    goto exit;

  key = hyscan_tile_queue_cache_key (requested_tile, priv->des_state.hash);

  /* Ищем тайл в кэше. */
  status = hyscan_cache_get (priv->cur_state.cache, key, NULL, NULL, &size2);
  if (status)
    {
      size1 = sizeof (HyScanTile);
      size2 -= size1;
      *buffer = g_malloc0 (size2 * sizeof (gfloat));
      hyscan_cache_get2 (priv->cur_state.cache, key, NULL, cached_tile, &size1, *buffer, &size2);
      *size = size2;
    }
  else
    {
      *size = 0;
    }

  g_free (key);

exit:
  g_atomic_int_set (&priv->allow_work, TRUE);
  return status;
}

/* Функция добавляет тайл во временный кэш. */
void
hyscan_tile_queue_add (HyScanTileQueue *self,
                       HyScanTile      *tile)
{
  g_return_if_fail (HYSCAN_IS_TILE_QUEUE (self));
  if (!g_atomic_int_compare_and_exchange (&self->priv->allow_work, TRUE, FALSE))
    return;

  g_array_append_val (self->priv->prequeue, *tile);
  g_atomic_int_set (&self->priv->allow_work, TRUE);
}

/* Функция копирует тайлы из временного кэша в реальный. */
void
hyscan_tile_queue_add_finished (HyScanTileQueue *self,
                                guint64          view_id)
{
  HyScanTileQueuePrivate *priv;

  guint i;
  HyScanTile *tile;
  HyScanTileQueueTask *task;
  GList *link;

  g_return_if_fail (HYSCAN_IS_TILE_QUEUE (self));
  priv = self->priv;

  if (priv->prequeue->len == 0)
    return;

  g_mutex_lock (&(priv->qlock));

  /* Тайлы из prequeue переписываем в queue.  При этом если такой
   * тайл уже генерируется, то просто обновляем для него view_id. */
  for (i = 0; i < priv->prequeue->len; i++)
    {
      tile = &g_array_index (priv->prequeue, HyScanTile, i);
      link = g_queue_find_custom (priv->queue, tile, hyscan_tile_queue_task_finder);

      if (link != NULL)
        {
          task = link->data;
          task->view_id = view_id;
        }
      /* Если такой же тайл не найден, добавляем его в список заданий. */
      else
        {
          task = g_new (HyScanTileQueueTask, 1);
          task->tile = *tile;
          task->view_id = view_id;
          task->status = IDLE;
          task->gen_id = -1;
          g_queue_push_tail (priv->queue, task);
        }
    }

  /* Очищаем временный список тайлов. */
  if (priv->prequeue->len > 0)
    g_array_remove_range (priv->prequeue, 0, priv->prequeue->len);

  priv->view_id = view_id;
  g_atomic_int_set (&priv->qflag, 1);
  g_cond_signal (&priv->qcond);
  g_mutex_unlock (&priv->qlock);
}
