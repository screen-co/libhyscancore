/**
 *
 * \file hyscan-tile-queue.h
 *
 * \brief Заголовочный файл очереди генераторов тайлов.
 * \author Dmitriev Alexander (m1n7@yandex.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanTileQueue HyScanTileQueue - очередь генераторов тайлов.
 *
 * HyScanTileQueue - очередь генераторов тайлов.
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
 * На вход #hyscan_tile_queue_add подается структура \link HyScanTile \endlink.
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
 * "tile-queue-image" эмиттируется из потока генерации тайла сразу же после успешной генерации тайла,
 * то есть если генерация не была прервана досрочно. Этот сигнал можно использовать для того, чтобы
 * использовать тот поток, в котором он сгенерирован, например, для раскрашивания тайла.
 * Его коллбэк выглядит так:
 * \code
 *  void tile_queue_image_cb (HyScanTileQueue *queue,
 *                            HyScanTile      *tile,
 *                            gfloat          *image,
 *                            gint             image_size, // В байтах!
 *                            gpointer         user_data);
 * \endcode
 *
 * "tile-queue-ready" эмиттируется из основного потока класса. Он ничего не передает, его цель -
 * сообщить, что поток генерации тайла гарантированно завершён.
 * Соответствующий коллбэк должен выглядеть следующим образом:
 * \code
 *  void tile_queue_ready_callback (HyScanTileQueue *queue,
 *                                  gpointer         user_data);
 * \endcode
 *
 * - #hyscan_tile_queue_new - создает объект очереди;
 * - #hyscan_tile_queue_set_cache - установка кэша;
 * - #hyscan_tile_queue_set_depth_source - установка параметров определения глубины;
 * - #hyscan_tile_queue_set_depth_filter_size - установка параметров определения глубины;
 * - #hyscan_tile_queue_set_depth_time - установка параметров определения глубины;
 * - #hyscan_tile_queue_set_ship_speed - установка скорости судна;
 * - #hyscan_tile_queue_set_sound_velocity - установка скорости звука;
 * - #hyscan_tile_queue_open - открытие галса;
 * - #hyscan_tile_queue_close - закрытие галса;
 * - #hyscan_tile_queue_check - поиск тайла в кэше;
 * - #hyscan_tile_queue_get - получение тайла из кэша;
 * - #hyscan_tile_queue_add - добавление тайла во временную очередь;
 * - #hyscan_tile_queue_add_finished - копирование временной очереди в реальную.
 *
 */

#ifndef __HYSCAN_TILE_QUEUE_H__
#define __HYSCAN_TILE_QUEUE_H__

#include <hyscan-tile-common.h>
#include <hyscan-depthometer.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_TILE_QUEUE             (hyscan_tile_queue_get_type ())
#define HYSCAN_TILE_QUEUE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_TILE_QUEUE, HyScanTileQueue))
#define HYSCAN_IS_TILE_QUEUE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_TILE_QUEUE))
#define HYSCAN_TILE_QUEUE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_TILE_QUEUE, HyScanTileQueueClass))
#define HYSCAN_IS_TILE_QUEUE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_TILE_QUEUE))
#define HYSCAN_TILE_QUEUE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_TILE_QUEUE, HyScanTileQueueClass))

typedef struct _HyScanTileQueue HyScanTileQueue;
typedef struct _HyScanTileQueuePrivate HyScanTileQueuePrivate;
typedef struct _HyScanTileQueueClass HyScanTileQueueClass;

struct _HyScanTileQueue
{
  GObject parent_instance;

  HyScanTileQueuePrivate *priv;
};

struct _HyScanTileQueueClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                  hyscan_tile_queue_get_type              (void);

/**
 *
 * Функция создает новый объект \link HyScanTileQueue \endlink.
 *
 * \param max_generators - число потоков генерации.
 *
 * \return Указатель на объект очереди.
 */
HYSCAN_API
HyScanTileQueue        *hyscan_tile_queue_new                  (gint                    max_generators);

/**
 *
 * Функция устанавливает кэш.
 *
 * \param tilequeue - указатель на \link HyScanTileQueue \endlink;
 * \param cache - указатель на систему кэширования;
 *
 */
HYSCAN_API
void                    hyscan_tile_queue_set_cache            (HyScanTileQueue        *tilequeue,
                                                                HyScanCache            *cache);
/**
 *
 * Функция настройки объекта измерения глубины.
 *
 * \param tilequeue - указатель на \link HyScanTileQueue \endlink;
 * \param source - источник данных глубины;
 * \param channel - номер канала.
 *
 */
HYSCAN_API
void                    hyscan_tile_queue_set_depth_source     (HyScanTileQueue        *tilequeue,
                                                                HyScanSourceType        source,
                                                                guint                   channel);
/**
 *
 * Функция настройки объекта измерения глубины.
 *
 * \param tilequeue - указатель на \link HyScanTileQueue \endlink;
 * \param size - размер фильтра.
 *
 */
HYSCAN_API
void                    hyscan_tile_queue_set_depth_filter_size(HyScanTileQueue        *tilequeue,
                                                                guint                   size);
/**
 *
 * Функция настройки объекта измерения глубины.
 *
 * \param tilequeue - указатель на \link HyScanTileQueue \endlink;
 * \param usecs - окно валидности данных.
 *
 */
HYSCAN_API
void                    hyscan_tile_queue_set_depth_time       (HyScanTileQueue        *tilequeue,
                                                                gulong                  usecs);

/**
 *
 * Функция задания скорости судна.
 *
 * \param tilequeue - указатель на \link HyScanTileQueue \endlink;
 * \param speed  - скорость судна в м/с.
 *
 */
HYSCAN_API
void                    hyscan_tile_queue_set_ship_speed       (HyScanTileQueue        *tilequeue,
                                                                gfloat                  speed);
/**
 *
 * Функция задания скорости звука.
 *
 * \param tilequeue - указатель на \link HyScanTileQueue \endlink;
 * \param velocity - профиль скорости звука. См. \link HyScanSoundVelocity \endlink.
 *
 */
HYSCAN_API
void                    hyscan_tile_queue_set_sound_velocity   (HyScanTileQueue        *tilequeue,
                                                                GArray                 *velocity);

/**
 *
 * Функция открывает галс.
 *
 * \param tilequeue - указатель на \link HyScanTileQueue \endlink;
 * \param db - указатель на базу данных;
 * \param project - имя проекта;
 * \param track - имя галса;
 * \param raw - использовать ли сырые данные.
 *
 */
HYSCAN_API
void                    hyscan_tile_queue_open                 (HyScanTileQueue        *tilequeue,
                                                                HyScanDB               *db,
                                                                const gchar            *project,
                                                                const gchar            *track,
                                                                gboolean                raw);
/**
 *
 * Функция закрывает галс.
 *
 * \param tilequeue - указатель на \link HyScanTileQueue \endlink;
 *
 */
HYSCAN_API
void                    hyscan_tile_queue_close                (HyScanTileQueue        *tilequeue);

/**
 *
 * Функция ищет тайл в кэше, сравнивает его параметры генерации с запрошенными и
 * определяет, нужно ли перегенерировать этот тайл.
 *
 * \param tilequeue - указатель на \link HyScanTileQueue \endlink;
 * \param requested_tile - запрошенный тайл;
 * \param cached_tile - тайл в системе кэширования;
 * \param regenerate - флаг, показывающий, требуется ли перегенерировать этот тайл.
 *
 * \return TRUE, если тайл есть в кэше.
 */
HYSCAN_API
gboolean                hyscan_tile_queue_check                (HyScanTileQueue        *tilequeue,
                                                                HyScanTile             *requested_tile,
                                                                HyScanTile             *cached_tile,
                                                                gboolean               *regenerate);
/**
 *
 * Функция отдает тайл из кэша.
 *
 * \param tilequeue - указатель на \link HyScanTileQueue \endlink;
 * \param requested_tile - запрошенный тайл;
 * \param cached_tile - тайл в системе кэширования;
 * \param buffer - буффер для тайла из системы кэширования (память будет выделена внутри функции);
 * \param size - размер считанных данных.
 *
 * \return TRUE, если тайл успешно получен из кэша, иначе FALSE.
 */
HYSCAN_API
gboolean                hyscan_tile_queue_get                  (HyScanTileQueue        *tilequeue,
                                                                HyScanTile             *requested_tile,
                                                                HyScanTile             *cached_tile,
                                                                gfloat                **buffer,
                                                                guint32                *size);
/**
 *
 * Функция добавляет новый тайл во временную очередь.
 *
 * \param tilequeue - указатель на \link HyScanTileQueue \endlink;
 * \param tile - тайл.
 *
 */
HYSCAN_API
void                    hyscan_tile_queue_add                  (HyScanTileQueue        *tilequeue,
                                                                HyScanTile             *tile);
/**
 *
 * Функция копирует тайлы из временной очереди в реальную.
 * При этом она сама определяет, какие тайлы уже генерируются, а какие
 * можно больше не генерировать. Для определения старых и новых тайлов
 * используется переменная view_id, значение которой не важно, а важен
 * только сам факт отличия от предыдущего значения.
 *
 * \param tilequeue - указатель на \link HyScanTileQueue \endlink;
 * \param view_id - идентификатор текущего отображения.
 *
 */
HYSCAN_API
void                    hyscan_tile_queue_add_finished         (HyScanTileQueue        *tilequeue,
                                                                guint64                 view_id);

G_END_DECLS

#endif /* __HYSCAN_TILE_QUEUE_H__ */
