/* hyscan-tile-queue.c
 *
 * Copyright 2016-2019 Screen LLC, Alexander Dmitriev <m1n7@yandex.ru>
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
/**
 * SECTION: hyscan-tile-queue
 * @Short_description: асинхронная генерация тайлов водопада
 * @Title: HyScanTileQueue
 *
 * Данный класс выступает прослойкой между потребителем и отдельными генераторами тайлов.
 * При создании объекта передается максимальное число генераторов, которые создаст класс.
 *
 * Пользователь отдает список тайлов, которые надо сгенерировать, в два этапа: сначала с помощью
 * помощью функции #hyscan_tile_queue_add тайлы записываются в предварительный список,
 * а потом с помощью функции #hyscan_tile_queue_add_finished тайлы переписываются в "рабочую" очередь.
 * При этом класс сам определяет, какие тайлы из уже генерирующихся надо оставить, какие удалить,
 * а какие добавить.
 *
 * На вход #hyscan_tile_queue_add подается структура #HyScanTile.
 * В ней должны быть заданы следующие поля:
 * \code
 *   gint32               across_start; // Начальная координата поперек оси движения (мм).
 *   gint32               along_start;  // Начальная координата вдоль оси движения (мм).
 *   gint32               across_end;   // Конечная координата поперек оси движения (мм).
 *   gint32               along_end;    // Конечная координата вдоль оси движения (мм).
 *   gfloat               scale;        // Масштаб.
 *   gfloat               ppi;          // PPI.
 *   guint                upsample;     // Величина передискретизации.
 *   HyScanTileType       type;         // Тип отображения.
 *   gboolean             rotate;       // Поворот тайла.
 *   HyScanSourceType     source;       // Канал данных для тайла.
 * \endcode
 *
 * Методы класса потокобезопасны, однако не предвидится ситуация, когда
 * два потока будут работать с одним и тем же генератором тайлов.
 * Все функции заточены под то, что они будут вызываны из главного потока.
 *
 * Класс эмиттирует 3 сигнала: "tile-queue-image", "tile-queue-ready" и "tile-queue-sync".
 * "tile-queue-image"
 */

/* Для разработчика.
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
 * |  _af_chg  | _processing | _task_processor |   _get    |
 * +-----------+-------------+-----------------+-----------+
 * | des_state | des_state   | cur_state       | cur_state |
 * |           | cur_state   | dctable         |           |
 * |           | dctable     |                 |           |
 * +-----------+-------------+-----------------+-----------+
 * Для всего state'ов и dctable есть свой мьютекс.
 *
 * Можно считать чтение из cur_state потокобезопасной операцией, а
 * неодновременность доступа разных потоков к cur_state и des_state будет
 * гарантирована если _processing завершит генераторы перед синхронизацией.
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
#define xDEPTH  9973

#define WAIT_TIME (250 * G_TIME_SPAN_MILLISECOND)

#define TILE_QUEUE_MAGIC 0x5bb0436f

typedef struct
{
  HyScanTile tile;    /* Требуемый тайл. */
  guint64    view_id; /* Некий идентификатор. */
  gint       status;  /* Состояние задачи. */
  gint       gen_id;  /* Номер генератора. */
} HyScanTileQueueTask;

typedef struct
{
  guint32    magic;
  guint32    size;
  HyScanTile tile;
} HyScanTileQueueCache;

/* Состояние задачи. */
enum
{
  IDLE,     /* Не обрабатывается. */
  BUSY,     /* Обрабатывается. */
  CLEANABLE /* Обработана. */
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
  PROP_MAX_GENERATORS = 1,
  PROP_CACHE,
  PROP_AMPLITUDE_FACTORY,
  PROP_DEPTH_FACTORY,
};

typedef struct
{
  gfloat                  ship_speed;            /* Скорость движения. */

  GArray                 *sound_velocity;        /* Скорость звука. */
  gfloat                  sound_velocity1;       /* Скорость звука для тех, кто не умеет в профиль скорости звука. */

  gboolean                amp_changed;           /* Флаг на смену временного окна данных глубины. */
  gboolean                dpt_changed;           /* Флаг на смену размера фильтра. */

  gboolean                speed_changed;         /* Флаг на смену скорости движения. */
  gboolean                velocity_changed;      /* Флаг на смену скорости звука. */

  gulong                  hash;                  /* Хэш состояния. Перед тем, как менять порядок переменных
                                                  * в этой структуре, посмотри, как считается хэш. */
} HyScanTileQueueState;

struct _HyScanTileQueuePrivate
{
  /* Кэш. */
  HyScanCache            *cache;                 /* Интерфейс системы кэширования. */
  HyScanAmplitudeFactory *amp_factory;           /* Фабрика объектов акустических данных. */
  HyScanDepthFactory     *dpt_factory;           /* Фабрика объектов глубины. */

  /* Параметры генерации. */
  HyScanTileQueueState    cur_state;             /* Текущее состояние. */
  HyScanTileQueueState    des_state;             /* Обновленное состояние. */
  GMutex                  state_lock;            /* Блокировка обновленного состояния. */
  gint                    state_changed;         /* Флаг на изменение состояния извне. */

  /* Очередь заданий. */
  GArray                 *prequeue;              /* Список тайлов, которые необходимо будет сгенерировать. */
  GQueue                 *queue;                 /* Очередь заданий. */
  GMutex                  qlock;                 /* Блокировка множественного доступа к очереди. */
  gint                    qflag;                 /* Флаг на наличие новых заданий или освободившийся генератор. */
  GCond                   qcond;                 /* Аналогично. */

  /* Поток обработки заданий*/
  GThread                *processing;            /* Основной поток раздачи заданий. */
  gint                    stop;                  /* Галс открыт. */

  /* Генераторы. */
  HyScanWaterfallTile   **generator;             /* Массив генераторов. */
  gint                   *generator_state;       /* Статусы генераторов. */
  guint                   max_generators;        /* Максимальное число генераторов. */
  guint                   available_generators;  /* Количество незанятых генераторов. */

  GHashTable             *dctable;               /* Каналы данных и объекты определения глубины. */
  GMutex                  dc_lock;               /* Блокировка доступа к каналам данных. */

  guint64                 view_id;               /* Идентификатор. */

  HyScanBuffer           *header;                /* Буфер для заголовка (HyScanTile). */
  HyScanBuffer           *data;                  /* Буфер для данных. */
};

static void                hyscan_tile_queue_set_property       (GObject                *object,
                                                                 guint                   prop_id,
                                                                 const GValue           *value,
                                                                 GParamSpec             *pspec);
static void                hyscan_tile_queue_object_constructed (GObject                *object);
static void                hyscan_tile_queue_object_finalize    (GObject                *object);

static HyScanAmplitude    *hyscan_tile_queue_get_dc             (HyScanTileQueuePrivate *priv,
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

static void                hyscan_tile_queue_state_hash         (HyScanAmplitudeFactory *af,
                                                                 HyScanDepthFactory     *df,
                                                                 HyScanTileQueueState   *state);
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

  /**
   * HyScanTileQueue::tile-queue-ready:
   * @tilequeue: объект, получивший сигнал
   * @user_data: данные, определенные в момент подключения к сигналу
   *
   * Сообщает, что поток генерации тайла гарантированно завершён.
   */
  hyscan_tile_queue_signals[SIGNAL_READY] =
    g_signal_new ("tile-queue-ready", HYSCAN_TYPE_TILE_QUEUE, G_SIGNAL_RUN_LAST, 0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0);
  /**
   * HyScanTileQueue::tile-queue-image:
   * @tilequeue: объект, получивший сигнал
   * @tile: (TYPE HyScanTile): тайл
   * @image: сгенерированное изображение
   * @image_size: размер изображения в байтах
   * @user_data: данные, определенные в момент подключения к сигналу
   *
   * Эмиттируется из потока генерации тайла сразу же после успешной генерации
   * тайла, то есть если генерация не была прервана досрочно. Этот сигнал можно
   * использовать для того, чтобы использовать тот поток, в котором он
   * сгенерирован, например, для раскрашивания тайла.
   */
  hyscan_tile_queue_signals[SIGNAL_IMAGE] =
    g_signal_new ("tile-queue-image", HYSCAN_TYPE_TILE_QUEUE, G_SIGNAL_RUN_LAST, 0,
                  NULL, NULL,
                  hyscan_core_marshal_VOID__POINTER_POINTER_INT_ULONG,
                  G_TYPE_NONE,
                  4, G_TYPE_POINTER, G_TYPE_POINTER, G_TYPE_INT, G_TYPE_ULONG);
  /**
   * HyScanTileQueue::tile-queue-hash:
   * @tilequeue: объект, получивший сигнал
   * @hash: хэш состояния
   * @user_data: данные, определенные в момент подключения к сигналу
   *
   * Передает хэш желаемого состояния. Ничего не говорит о том, когда это
   * состояние действительно будет установлено (спойлер: довольно скоро).
   */
  hyscan_tile_queue_signals[SIGNAL_HASH] =
    g_signal_new ("tile-queue-hash", HYSCAN_TYPE_TILE_QUEUE, G_SIGNAL_RUN_LAST, 0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__POINTER,
                  G_TYPE_NONE,
                  1, G_TYPE_POINTER);

  g_object_class_install_property (object_class, PROP_MAX_GENERATORS,
    g_param_spec_uint ("max_generators", "MaxGenerators",
                       "Maximum number of tile generators", 1, 128, 1,
                       G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_CACHE,
    g_param_spec_object ("cache", "Cache", "HyScanCache interface",
                         HYSCAN_TYPE_CACHE,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_AMPLITUDE_FACTORY,
    g_param_spec_object ("amp-factory", "AmplitudeFactory",
                         "HyScanAmplitudeFactory",
                         HYSCAN_TYPE_AMPLITUDE_FACTORY,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_DEPTH_FACTORY,
    g_param_spec_object ("dpt-factory", "DepthFactory",
                         "HyScanDepthFactory",
                         HYSCAN_TYPE_DEPTH_FACTORY,
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
  else if (prop_id == PROP_CACHE)
    priv->cache = g_value_dup_object (value);
  else if (prop_id == PROP_AMPLITUDE_FACTORY)
    priv->amp_factory = g_value_dup_object (value);
  else if (prop_id == PROP_DEPTH_FACTORY)
    priv->dpt_factory = g_value_dup_object (value);
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

  priv->header = hyscan_buffer_new ();
  priv->data = hyscan_buffer_new ();

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

  g_clear_object (&priv->header);
  g_clear_object (&priv->data);

  /* Глушим генераторы, закрываем КД. */
  for (i = 0; i < priv->max_generators; i++)
    g_clear_object (&priv->generator[i]);

  g_hash_table_destroy (priv->dctable);
  g_free (priv->generator);
  g_free (priv->generator_state);
  g_queue_free_full (priv->queue, g_free);
  g_array_unref (priv->prequeue);

  /* Чистим стейты. */
  g_clear_pointer (&priv->cur_state.sound_velocity, g_array_unref);
  g_clear_pointer (&priv->des_state.sound_velocity, g_array_unref);

  /* Очищаем список заданий. */
  g_cond_clear (&priv->qcond);
  g_mutex_clear (&priv->qlock);
  g_mutex_clear (&priv->dc_lock);
  g_mutex_clear (&priv->state_lock);

  g_clear_object (&priv->cache);
  g_clear_object (&priv->amp_factory);
  g_clear_object (&priv->dpt_factory);

  G_OBJECT_CLASS (hyscan_tile_queue_parent_class)->finalize (object);
}

/* Функция возвращает КД из ХТ или создает и помещает туда. */
static HyScanAmplitude*
hyscan_tile_queue_get_dc (HyScanTileQueuePrivate *priv,
                          HyScanSourceType        source,
                          gint                    gen_id)
{
  HyScanAmplitude *dc = NULL;
  gint64 key = gen_id + source * xSOURCE;

  dc = g_hash_table_lookup (priv->dctable, GINT_TO_POINTER (key));

  /* Если нашли КД, отдаем его. */
  if (dc != NULL)
    return dc;

  /* Иначе пробуем открыть. */
  dc = hyscan_amplitude_factory_produce (priv->amp_factory, source);

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
  HyScanDepthometer *depth = NULL;

  gint64 key = gen_id + xDEPTH;

  depth = g_hash_table_lookup (priv->dctable, GINT_TO_POINTER (key));

  /* Если нашли объект, отдаем его. */
  if (depth != NULL)
    return depth;

  /* Иначе запрашиваем объект. */
  depth = hyscan_depth_factory_produce (priv->dpt_factory);
  g_hash_table_insert (priv->dctable, GINT_TO_POINTER (key), depth);

  return depth;
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

  if (new_st->amp_changed)
    {
      /* Выставляем флаги. */
      new_st->amp_changed = FALSE;
      cur_st->amp_changed = TRUE;
    }

  if (new_st->dpt_changed)
    {
      /* Выставляем флаги. */
      new_st->amp_changed = FALSE;
      cur_st->amp_changed = TRUE;
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

  /* Если поменялись параметры фабрики акустических данных, уничтожаем
   * соответствующие объекты. Потоки генерации тайлов не найдут нужные объекты
   * и сами их создадут. */
  if (state->amp_changed)
    {
      g_hash_table_iter_init (&iter, dctable);
      while (g_hash_table_iter_next (&iter, &key, &value))
        {
          if (HYSCAN_IS_AMPLITUDE (value))
            g_hash_table_iter_remove (&iter);
        }
      g_hash_table_remove_all (dctable);
    }

  /* Если изменились параметры фабрики глубиномеров, уничтожаем все объекты
   * измерения глубины. */
  if (state->dpt_changed)
    {
      g_hash_table_iter_init (&iter, dctable);
      while (g_hash_table_iter_next (&iter, &key, &value))
        {
          if (HYSCAN_IS_DEPTHOMETER (value))
            g_hash_table_iter_remove (&iter);
        }
    }

  state->amp_changed = FALSE;
  state->dpt_changed = FALSE;

  hyscan_tile_queue_state_hash (self->priv->amp_factory,
                                self->priv->dpt_factory, state);
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
           * 2) За мьютексом переписываются параметры из des_state в cur_state.
           * 3) Копируются параметры из cur_state в dctable
           */
          hyscan_tile_queue_stop_all_gen (priv);      /*< 1. */

          g_mutex_lock (&priv->state_lock);
          hyscan_tile_queue_sync_states (self);       /*< 2. */
          priv->state_changed = FALSE;
          g_mutex_unlock (&priv->state_lock);

          hyscan_tile_queue_apply_updates (self);     /*< 3. */
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

  HyScanAmplitude *dc = NULL;
  HyScanDepthometer  *depth = NULL;

  /* Поскольку гарантируется, что одновременно task_processor и
   * _processing НЕ полезут в cur_state, блокировку не ставим.
   * Чтение при этом считается тредсейф операцией. */
  HyScanCache *cache = priv->cache;
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
  if (task->tile.flags & HYSCAN_TILE_GROUND)
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
      HyScanTileQueueCache header;
      HyScanBuffer *meta = hyscan_buffer_new ();
      HyScanBuffer *data = hyscan_buffer_new ();

      header.magic = TILE_QUEUE_MAGIC;
      header.size = sizeof (header) + image_size;
      header.tile = tile;
      hyscan_buffer_wrap (meta, HYSCAN_DATA_BLOB, &header, sizeof (header));
      hyscan_buffer_wrap (data, HYSCAN_DATA_BLOB, image, image_size);

      key = hyscan_tile_queue_cache_key (&tile, state->hash);
      hyscan_cache_set2 (cache, key, NULL, meta, data);

      g_object_unref (meta);
      g_object_unref (data);
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
hyscan_tile_queue_state_hash (HyScanAmplitudeFactory *af,
                              HyScanDepthFactory     *df,
                              HyScanTileQueueState   *state)
{
  gchar *af_token, *df_token;
  gchar *str;
  gulong hash;

  af_token = hyscan_amplitude_factory_get_token (af);
  df_token = hyscan_depth_factory_get_token (df);

  if (af_token == NULL)
    af_token = g_strdup ("none");
  if (df_token == NULL)
    df_token = g_strdup ("none");

  str = g_strdup_printf ("%s.%s.%f", af_token, df_token, state->ship_speed);
  hash = crc32 (0L, (guchar*)(str), strlen (str));

  if (state->sound_velocity != NULL)
    {
      hash = crc32 (hash,
                    (guchar*)(state->sound_velocity->data),
                    state->sound_velocity->len * sizeof (HyScanSoundVelocity));
    }

  state->hash = hash;
  g_free (str);
  g_free (af_token);
  g_free (df_token);
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
                                tile->flags,
                                tile->rotate,
                                tile->source);
  return key;
}

/**
 * hyscan_tile_queue_new:
 * @max_generators: число потоков генерации
 * @cache: указатель на систему кэширования
 *
 * Функция создает новый объект \link HyScanTileQueue \endlink.
 *
 * Returns: (transfer full): объект #HyScanTileQueue.
 */
HyScanTileQueue*
hyscan_tile_queue_new (gint                    max_generators,
                       HyScanCache            *cache,
                       HyScanAmplitudeFactory *amp_factory,
                       HyScanDepthFactory     *dpt_factory)
{
  return g_object_new (HYSCAN_TYPE_TILE_QUEUE,
                       "max_generators", max_generators,
                       "cache", cache,
                       "amp-factory", amp_factory,
                       "dpt-factory", dpt_factory,
                       NULL);
}

/**
 * hyscan_tile_queue_set_ship_speed:
 * @tilequeue: объект #HyScanTileQueue
 * @speed:  скорость судна в м/с
 *
 * Функция задания скорости судна.
 *
 */
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

  hyscan_tile_queue_state_hash (priv->amp_factory, priv->dpt_factory, &priv->des_state);
  g_signal_emit (self, hyscan_tile_queue_signals[SIGNAL_HASH], 0, priv->des_state.hash);

  g_mutex_unlock (&priv->state_lock);
}

/**
 * hyscan_tile_queue_set_sound_velocity:
 * @tilequeue: объект #HyScanTileQueue
 * @velocity: профиль скорости звука. См. \link HyScanSoundVelocity \endlink
 *
 * Функция задания профиля скорости звука.
 */
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

  hyscan_tile_queue_state_hash (priv->amp_factory, priv->dpt_factory, state);
  g_signal_emit (self, hyscan_tile_queue_signals[SIGNAL_HASH], 0, state->hash);

  g_mutex_unlock (&priv->state_lock);
}

/**
 * hyscan_tile_queue_amp_changed:
 * @tilequeue: объект #HyScanTileQueue
 *
 * Функция уведомляет объект, что изменились параметры каналов данных
 * и их требуется переоткрыть.
 */
void
hyscan_tile_queue_amp_changed (HyScanTileQueue *self)
{
  HyScanTileQueuePrivate *priv;
  HyScanTileQueueState *state;

  g_return_if_fail (HYSCAN_IS_TILE_QUEUE (self));
  priv = self->priv;
  state = &priv->des_state;

  g_mutex_lock (&priv->state_lock);

  priv->des_state.amp_changed = TRUE;
  priv->state_changed = TRUE;

  hyscan_tile_queue_state_hash (priv->amp_factory, priv->dpt_factory, state);
  g_signal_emit (self, hyscan_tile_queue_signals[SIGNAL_HASH], 0, state->hash);

  g_mutex_unlock (&priv->state_lock);
}

/**
 * hyscan_tile_queue_dpt_changed:
 * @tilequeue: объект #HyScanTileQueue
 *
 * Функция уведомляет объект, что изменились параметры измерения глубины и такие
 * объекты требуется переоткрыть.
 */
void
hyscan_tile_queue_dpt_changed (HyScanTileQueue *self)
{
  HyScanTileQueuePrivate *priv;
  HyScanTileQueueState *state;

  g_return_if_fail (HYSCAN_IS_TILE_QUEUE (self));
  priv = self->priv;
  state = &priv->des_state;

  g_mutex_lock (&priv->state_lock);

  priv->des_state.dpt_changed = TRUE;
  priv->state_changed = TRUE;

  hyscan_tile_queue_state_hash (priv->amp_factory, priv->dpt_factory, state);
  g_signal_emit (self, hyscan_tile_queue_signals[SIGNAL_HASH], 0, state->hash);

  g_mutex_unlock (&priv->state_lock);
}

/**
 * hyscan_tile_queue_check:
 * @tilequeue: объект #HyScanTileQueue
 * @requested_tile: (not nullable): запрошенный тайл
 * @cached_tile: (not nullable): тайл в системе кэширования
 * @regenerate: (nullable): флаг, показывающий, требуется ли перегенерировать этот тайл
 *
 * Функция ищет тайл в кэше, сравнивает его параметры генерации с запрошенными и
 * определяет, нужно ли перегенерировать этот тайл.
 *
 * Returns: TRUE, если тайл есть в кэше.
 */
gboolean
hyscan_tile_queue_check (HyScanTileQueue *self,
                         HyScanTile      *requested_tile,
                         HyScanTile      *cached_tile,
                         gboolean        *regenerate)
{
  gchar *key;
  gboolean found = FALSE;
  gboolean regen = TRUE;
  guint32 size = sizeof (HyScanTileQueueCache);
  HyScanTileQueueCache header = {.magic = 0};
  HyScanTileQueuePrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_TILE_QUEUE (self), FALSE);
  priv = self->priv;

  if (requested_tile == NULL || cached_tile == NULL)
    return FALSE;

  if (priv->cache == NULL)
    return FALSE;

  key = hyscan_tile_queue_cache_key (requested_tile, priv->des_state.hash);

  /* Ищем тайл в кэше и проверяем магическую константу. */
  hyscan_buffer_wrap (priv->header, HYSCAN_DATA_BLOB, &header, size);
  found = hyscan_cache_get2 (priv->cache, key, NULL, size, priv->header, NULL);
  found &= header.magic == TILE_QUEUE_MAGIC;

  *cached_tile = header.tile;

  if (found && header.tile.finalized)
    regen = FALSE;

  if (regenerate != NULL)
    *regenerate = regen;

  g_free (key);

  return found;
}

/**
 * hyscan_tile_queue_get:
 * @tilequeue: объект #HyScanTileQueue
 * @requested_tile: (not nullable): запрошенный тайл
 * @cached_tile: (not nullable): тайл в системе кэширования
 * @buffer: (not nullable): буффер для тайла из системы кэширования (память будет выделена внутри функции)
 * @size: (not nullable):  размер считанных данных
 *
 * Функция отдает тайл из кэша.
 *
 * Returns: TRUE, если тайл успешно получен из кэша, иначе FALSE.
 */
gboolean
hyscan_tile_queue_get (HyScanTileQueue  *self,
                       HyScanTile       *requested_tile,
                       HyScanTile       *cached_tile,
                       gfloat          **buffer,
                       guint32          *size)
{
  gchar *key;
  gboolean status = FALSE;
  guint32 header_size = sizeof (HyScanTileQueueCache);
  HyScanTileQueuePrivate *priv;
  HyScanTileQueueCache header;

  g_return_val_if_fail (HYSCAN_IS_TILE_QUEUE (self), FALSE);
  priv = self->priv;

  if (requested_tile == NULL || cached_tile == NULL)
    return FALSE;

  if (priv->cache == NULL)
    return FALSE;

  key = hyscan_tile_queue_cache_key (requested_tile, priv->des_state.hash);

  /* Ищем тайл в кэше. */
  hyscan_buffer_wrap (priv->header, HYSCAN_DATA_BLOB, &header, header_size);
  status = hyscan_cache_get2 (priv->cache, key, NULL,
                              header_size, priv->header, priv->data);
  if (status && header.magic == TILE_QUEUE_MAGIC)
    {
      *buffer = hyscan_buffer_get (priv->data, NULL, size);
      *cached_tile = header.tile;
    }
  else
    {
      *size = 0;
    }

  g_free (key);

  return status;
}

/**
 * hyscan_tile_queue_add:
 * @tilequeue: объект #HyScanTileQueue
 * @tile: (not nullable): тайл
 *
 * Функция добавляет новый тайл во временную очередь.
 */
void
hyscan_tile_queue_add (HyScanTileQueue *self,
                       HyScanTile      *tile)
{
  g_return_if_fail (HYSCAN_IS_TILE_QUEUE (self));

  g_array_append_val (self->priv->prequeue, *tile);
}

/**
 * hyscan_tile_queue_add_finished:
 * @tilequeue: объект #HyScanTileQueue
 * @view_id: идентификатор текущего отображения
 *
 * Функция копирует тайлы из временной очереди в реальную.
 * При этом она сама определяет, какие тайлы уже генерируются, а какие
 * можно больше не генерировать. Для определения старых и новых тайлов
 * используется переменная view_id, значение которой не важно, а важен
 * только сам факт отличия от предыдущего значения.
 */
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

  g_mutex_lock (&priv->qlock);

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
