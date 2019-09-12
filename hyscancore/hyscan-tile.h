/* hyscan-tile.h
 *
 * Copyright 2019 Screen LLC, Alexander Dmitriev <m1n7@yandex.ru>
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

#ifndef __HYSCAN_TILE_H__
#define __HYSCAN_TILE_H__

#include <hyscan-api.h>
#include <hyscan-types.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_TILE             (hyscan_tile_get_type ())
#define HYSCAN_TILE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_TILE, HyScanTile))
#define HYSCAN_IS_TILE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_TILE))
#define HYSCAN_TILE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_TILE, HyScanTileClass))
#define HYSCAN_IS_TILE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_TILE))
#define HYSCAN_TILE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_TILE, HyScanTileClass))

typedef struct _HyScanTileInfo HyScanTileInfo;
typedef struct _HyScanTileCacheable HyScanTileCacheable;
typedef struct _HyScanTile HyScanTile;
typedef struct _HyScanTilePrivate HyScanTilePrivate;
typedef struct _HyScanTileClass HyScanTileClass;

typedef enum
{
  HYSCAN_TILE_GROUND   = 1 << 0,
  HYSCAN_TILE_PROFILER = 1 << 1
} HyScanTileFlags;

struct _HyScanTileInfo
{
  gint32               across_start; /**< Начальная координата поперек оси движения (мм). */
  gint32               along_start;  /**< Начальная координата вдоль оси движения (мм). */
  gint32               across_end;   /**< Конечная координата поперек оси движения (мм). */
  gint32               along_end;    /**< Конечная координата вдоль оси движения (мм). */

  gfloat               scale;        /**< Масштаб. */
  gfloat               ppi;          /**< PPI. */

  guint                upsample;     /**< Величина передискретизации. */
  gboolean             rotate;       /**< Поворот тайла. */

  HyScanSourceType     source;       /**< Канал данных для тайла. */
  HyScanTileFlags      flags;        /**< Флаги генерации. */
};

struct _HyScanTileCacheable
{
  gint32               w;            /**< Ширина тайла в пикселях. */
  gint32               h;            /**< Высота тайла в пикселях. */
  gboolean             finalized;    /**< Требуется ли перегенерация тайла. */    
};

struct _HyScanTile
{
  GObject              parent_instance;

  HyScanTilePrivate   *priv;

  HyScanTileInfo       info;         /* Это условно "константная" часть. Заполняется
  один раз при создании тайла, больше не трогается. Идёт в токен (и ключ кэширования). */
  HyScanTileCacheable  cacheable;    /* Это "вариативная" часть. Заполняется
  генератором, идёт в заголовок кэша, потом заполняется из кэша. */
};

struct _HyScanTileClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                  hyscan_tile_get_type         (void);

HYSCAN_API
HyScanTile *           hyscan_tile_new              (const gchar *track);

HYSCAN_API
const gchar *          hyscan_tile_get_track        (HyScanTile  *tile);

HYSCAN_API
const gchar *          hyscan_tile_get_token        (HyScanTile  *tile);

HYSCAN_API
gboolean               hyscan_tile_compare          (HyScanTile  *tile,
                                                     HyScanTile  *other);

/**
 *
 * Функция вычисляет количиство миллиметров в пикселе на данном
 * масштабе и PPI (points per inch).
 *
 * \param scale масштаб;
 * \param ppi PPI экрана.
 *
 * \return количество миллиметров на пиксель.
 */
HYSCAN_API
gfloat          hyscan_tile_common_mm_per_pixel         (gfloat scale,
                                                         gfloat ppi);
/**
 *
 * Функция вычисляет размер тайла (одной стороны) при заданных
 * координатах (в миллиметрах) и количестве миллиметров на пиксель.
 *
 * \param start начальная координата;
 * \param end конечная координата;
 * \param step количество миллиметров на пиксель.
 *
 * \return размер тайла в точках (пикселях).
 */
HYSCAN_API
gint32          hyscan_tile_common_tile_size            (gint32 start,
                                                         gint32 end,
                                                         gfloat step);

G_END_DECLS

#endif /* __HYSCAN_TILE_H__ */
