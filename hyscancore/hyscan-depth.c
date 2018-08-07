/*
 * \file hyscan-depth.c
 *
 * \brief Исходный файл интерфейса HyScanDepth
 * \author Alexander Dmitriev (m1n7@yandex.ru)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-depth.h"

G_DEFINE_INTERFACE (HyScanDepth, hyscan_depth, G_TYPE_OBJECT);

static void
hyscan_depth_default_init (HyScanDepthInterface *iface)
{
}

/* Функция установки скорости звука. */
void
hyscan_depth_set_sound_velocity (HyScanDepth         *depth,
                                 HyScanSoundVelocity *velocity,
                                 guint                length)
{
  HyScanDepthInterface *iface;

  g_return_if_fail (HYSCAN_IS_DEPTH (depth));
  iface = HYSCAN_DEPTH_GET_IFACE (depth);

  if (iface->set_sound_velocity != NULL)
    (*iface->set_sound_velocity) (depth, velocity, length);
}

/* Функция установки кэша. */
void
hyscan_depth_set_cache (HyScanDepth *depth,
                        HyScanCache *cache)
{
  HyScanDepthInterface *iface;

  g_return_if_fail (HYSCAN_IS_DEPTH (depth));
  iface = HYSCAN_DEPTH_GET_IFACE (depth);

  if (iface->set_cache != NULL)
    (*iface->set_cache) (depth, cache);
}

/* Функция получения глубины. */
gdouble
hyscan_depth_get (HyScanDepth *depth,
                  guint32      index,
                  gint64      *time)
{
  HyScanDepthInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DEPTH (depth), -1.0);
  iface = HYSCAN_DEPTH_GET_IFACE (depth);

  if (iface->get != NULL)
    return (*iface->get) (depth, index, time);

  return -1.0;
}

/* Функция поиска данных. */
HyScanDBFindStatus
hyscan_depth_find_data (HyScanDepth *depth,
                        gint64       time,
                        guint32     *lindex,
                        guint32     *rindex,
                        gint64      *ltime,
                        gint64      *rtime)
{
  HyScanDepthInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DEPTH (depth), HYSCAN_DB_FIND_FAIL);
  iface = HYSCAN_DEPTH_GET_IFACE (depth);

  if (iface->find_data != NULL)
    return (*iface->find_data) (depth, time, lindex, rindex, ltime, rtime);

  return HYSCAN_DB_FIND_FAIL;
}

/* Функция определения диапазона. */
gboolean
hyscan_depth_get_range (HyScanDepth *depth,
                        guint32     *first,
                        guint32     *last)
{
  HyScanDepthInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DEPTH (depth), FALSE);
  iface = HYSCAN_DEPTH_GET_IFACE (depth);

  if (iface->get_range != NULL)
    return (*iface->get_range) (depth, first, last);

  return FALSE;
}

/* Функция получения местоположения антенны. */
HyScanAntennaPosition
hyscan_depth_get_position (HyScanDepth *depth)
{
  HyScanDepthInterface *iface;
  HyScanAntennaPosition zero = {0};

  g_return_val_if_fail (HYSCAN_IS_DEPTH (depth), zero);
  iface = HYSCAN_DEPTH_GET_IFACE (depth);

  if (iface->get_position != NULL)
    return (*iface->get_position) (depth);

  return zero;
}

/* Функция определяет, возможна ли дозапись в канал данных. */
gboolean
hyscan_depth_is_writable (HyScanDepth *depth)
{
  HyScanDepthInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DEPTH (depth), FALSE);
  iface = HYSCAN_DEPTH_GET_IFACE (depth);

  if (iface->is_writable != NULL)
    return (*iface->is_writable) (depth);

  return FALSE;
}

/* Функция возвращает токен объекта. */
const gchar*
hyscan_depth_get_token (HyScanDepth *depth)
{
  HyScanDepthInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DEPTH (depth), NULL);
  iface = HYSCAN_DEPTH_GET_IFACE (depth);

  if (iface->get_token != NULL)
    return (*iface->get_token) (depth);

  return NULL;
}

/* Функция возвращает номер изменения. */
guint32
hyscan_depth_get_mod_count (HyScanDepth *depth)
{
  HyScanDepthInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_DEPTH (depth), 0);
  iface = HYSCAN_DEPTH_GET_IFACE (depth);

  if (iface->get_mod_count != NULL)
    return (*iface->get_mod_count) (depth);

  return 0;
}
