/*
 * \file hyscan-tile-common.c
 *
 * \brief Исходный код вспомогательных функций для тайлов.
 * \author Dmitriev Alexander (m1n7@yandex.ru)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 *
 */
#include <hyscan-tile-common.h>
#include <math.h>

gfloat
hyscan_tile_common_mm_per_pixel (gfloat scale,
                                 gfloat ppi)
{
  return 25.4 * scale / ppi;
}

gint32
hyscan_tile_common_tile_size (gint32 start,
                              gint32 end,
                              gfloat step)
{
  return ceil ((end - start) / step);
}
