/**
 *
 * \file hyscan-tile-common.h
 *
 * \brief Структуры, перечисления и вспомогательные функции для тайлов.
 * \author Dmitriev Alexander (m1n7@yandex.ru)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanTileCommon HyScanTileCommon - Структуры, перечисления
 * и вспомогательные функции для тайлов.
 *
 */

#ifndef __HYSCAN_TILE_COMMON_H__
#define __HYSCAN_TILE_COMMON_H__

#include <glib.h>
#include <hyscan-core-types.h>

/** \brief Типы тайлов по способу отображения. */
typedef enum
{
  HYSCAN_TILE_SLANT    = 100,        /**< Наклонная дальность. */
  HYSCAN_TILE_GROUND   = 101,        /**< Горизонтальная дальность. */
} HyScanTileType;

/** \brief Информация о тайле. */
typedef struct
{
  gint32               across_start; /**< Начальная координата поперек оси движения (мм). */
  gint32               along_start;  /**< Начальная координата вдоль оси движения (мм). */
  gint32               across_end;   /**< Конечная координата поперек оси движения (мм). */
  gint32               along_end;    /**< Конечная координата вдоль оси движения (мм). */

  gfloat               scale;        /**< Масштаб. */
  gfloat               ppi;          /**< PPI. */

  gint32               w;            /**< Ширина тайла в пикселях. */
  gint32               h;            /**< Высота тайла в пикселях. */

  guint                upsample;     /**< Величина передискретизации. */
  HyScanTileType       type;         /**< Тип отображения. */
  gboolean             rotate;       /**< Поворот тайла. */

  HyScanSourceType     source;       /**< Канал данных для тайла. */

  gboolean             finalized;    /**< Требуется ли перегенерация тайла. */
} HyScanTile;

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

#endif /* __HYSCAN_TILE_COMMON_H__ */
