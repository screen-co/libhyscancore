/**
 *
 * \file hyscan-waterfall-tile.h
 *
 * \brief Заголовочный файл генератора тайлов для режима водопад.
 * \author Dmitriev Alexander (m1n7@yandex.ru)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanWaterfallTile HyScanWaterfallTile - генератор тайлов в режиме "водопад".
 *
 * HyScanWaterfallTile - класс для генерации тайлов в режиме водопад в наклонной или горизонтальной дальности.
 *
 * Данный класс занимается исключительно генерацией тайлов.
 * Перед вызовом функции #hyscan_waterfall_tile_generate необходимо задать параметры генерации.
 * Для этого используются функции #hyscan_waterfall_tile_set_depth, #hyscan_waterfall_tile_set_speeds,
 * #hyscan_waterfall_tile_set_tile.
 *
 * Структура \link HyScanTile \endlink, передающаяся в методе #hyscan_waterfall_tile_set_tile,
 * должна быть заполнена.
 * \code
 * typedef struct
 * {
 *   gint32               across_start; // Вход.
 *   gint32               along_start;  // Вход.
 *   gint32               across_end;   // Вход.
 *   gint32               along_end;    // Вход.
 *
 *   gfloat               scale;        // Вход.
 *   gfloat               ppi;          // Вход.
 *
 *   gint32               w;            // Будет заполнено генератором.
 *   gint32               h;            // Будет заполнено генератором.
 *
 *   guint                upsample;     // Вход.
 *   HyScanTileType       type;         // Вход.
 *   gboolean             rotate;       // Вход.
 *
 *   HyScanSourceType     source;       // Для этого класса не требуется.
 *
 *   gboolean             finalized;    // Будет заполнено генератором.
 * } HyScanTile;
 * \endcode
 *
 * Генерацию тайла можно досрочно прекратить функцией #hyscan_waterfall_tile_terminate.
 * Это блокирующая функция, она ждет, пока генерация не будет завершена.
 *
 * \warning Догенерация тайлов пока что не поддерживается, поэтому функции #hyscan_waterfall_tile_set_cache
 * и #hyscan_waterfall_tile_set_path не приносят никакой пользы.
 */

#ifndef __HYSCAN_WATERFALL_TILE_H__
#define __HYSCAN_WATERFALL_TILE_H__

#include "hyscan-depthometer.h"
#include "hyscan-amplitude.h"
#include "hyscan-tile.h"

G_BEGIN_DECLS

#define HYSCAN_TYPE_WATERFALL_TILE              (hyscan_waterfall_tile_get_type ())
#define HYSCAN_WATERFALL_TILE(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_WATERFALL_TILE, HyScanWaterfallTile))
#define HYSCAN_IS_WATERFALL_TILE(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_WATERFALL_TILE))
#define HYSCAN_WATERFALL_TILE_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_WATERFALL_TILE, HyScanWaterfallTileClass))
#define HYSCAN_IS_WATERFALL_TILE_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_WATERFALL_TILE))
#define HYSCAN_WATERFALL_TILE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_WATERFALL_TILE, HyScanWaterfallTileClass))

typedef struct _HyScanWaterfallTile HyScanWaterfallTile;
typedef struct _HyScanWaterfallTilePrivate HyScanWaterfallTilePrivate;
typedef struct _HyScanWaterfallTileClass HyScanWaterfallTileClass;

struct _HyScanWaterfallTile
{
  GObject parent_instance;

  HyScanWaterfallTilePrivate *priv;
};

struct _HyScanWaterfallTileClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                   hyscan_waterfall_tile_get_type          (void);

/**
 *
 * Функция создает новый объект генерации тайлов.
 *
 * \return Указатель на объект \link HyScanWaterfallTile \endlink.
 *
 */
HYSCAN_API
HyScanWaterfallTile *   hyscan_waterfall_tile_new               (void);

/**
 *
 * Функция устанавливает кэш.
 *
 * \param wfall - указатель на объект HyScanWaterfallTile;
 * \param cache - указатель на интерфейс HyScanCache;
 * \param prefix - префикс системы кэширования.
 *
 */
HYSCAN_API
void                    hyscan_waterfall_tile_set_cache         (HyScanWaterfallTile *wfall,
                                                                 HyScanCache         *cache,
                                                                 const gchar         *prefix);
/**
 *
 * Функция устанавливает путь к БД, проекту и галсу.
 *
 * Для кэширования промежуточных результатов требуется обеспечить
 * уникальность ключа кэширования для конкретного галса. Поскольку объект
 * ничего не знает про открытый проект и галс, необходимо сообщить ему эту информацию.
 *
 * \param wfall - указатель на объект HyScanWaterfallTile;
 * \param path - путь к БД, проекту и галсу.
 *
 */
HYSCAN_API
void                    hyscan_waterfall_tile_set_path          (HyScanWaterfallTile *wfall,
                                                                 const gchar         *path);
/**
 *
 * Функция устанавливает скорость судна и звука.
 *
 * \param wfall - указатель на объект HyScanWaterfallTile;
 * \param sound_speed - скорость звука, м/с;
 * \param ship_speed - скорость судна, м/с.
 *
 * \return TRUE, если удалось установить параметры.
 *
 */
HYSCAN_API
gboolean                hyscan_waterfall_tile_set_speeds        (HyScanWaterfallTile *wfall,
                                                                 gfloat               ship_speed,
                                                                 gfloat               sound_speed);

/**
 *
 * Функция устанавливает параметры тайла и канал данных.
 * FALSE возращается в следующих случаях:
 * - при равенстве начальной и конечной координаты по любой из осей;
 * - при некорректном канале данных;
 * - если в данный момент идет генерация.
 *
 * \param wfall - указатель на объект HyScanWaterfallTile;
 * \param dc - канал данных, используемый для генерации;
 * \param depth - указатель на объект определения глубины.
 *
 * \return TRUE, если удалось установить параметры.
 *
 */
HYSCAN_API
gboolean                hyscan_waterfall_tile_set_dc            (HyScanWaterfallTile *wfall,
                                                                 HyScanAmplitude     *dc,
                                                                 HyScanDepthometer   *depth);
/**
 *
 * Функция генерирует тайл.
 * NULL возвращается в случае досрочного прекращения генерации.
 *
 * \param wfall - указатель на объект HyScanWaterfallTile;
 * \param tile - структура для записи параметров сгенерированного тайла;
 * \param size - размер сгенерированного тайла в байтах.
 *
 * \return указатель на тайл или NULL. Эту память требуется освободить.
 *
 */
HYSCAN_API
gfloat*                 hyscan_waterfall_tile_generate          (HyScanWaterfallTile *wfall,
                                                                 HyScanCancellable   *cancellable,
                                                                 HyScanTile          *tile,
                                                                 guint32             *size);

/**
 *
 * Функция прекращает генерацию тайла.
 *
 * \param wfall - указатель на объект HyScanWaterfallTile;
 *
 * \return TRUE, когда поток генерации завершается.
 *
 */
HYSCAN_API
gboolean                hyscan_waterfall_tile_terminate         (HyScanWaterfallTile *wfall);

G_END_DECLS

#endif /* __HYSCAN_WATERFALL_TILE_H__ */
