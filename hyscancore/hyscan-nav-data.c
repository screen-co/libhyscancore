/*
 * \file hyscan-navdata.c
 *
 * \brief Исходный файл интерфейса HyScanNavData
 * \author Alexander Dmitriev (m1n7@yandex.ru)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-nav-data.h"

G_DEFINE_INTERFACE (HyScanNavData, hyscan_nav_data, G_TYPE_OBJECT);

static void
hyscan_nav_data_default_init (HyScanNavDataInterface *iface)
{
}

/* Функция установки кэша. */
void
hyscan_nav_data_set_cache (HyScanNavData *navdata,
                           HyScanCache   *cache,
                           const gchar   *prefix)
{
  HyScanNavDataInterface *iface;

  g_return_if_fail (HYSCAN_IS_NAV_DATA (navdata));
  iface = HYSCAN_NAV_DATA_GET_IFACE (navdata);

  if (iface->set_cache != NULL)
    (*iface->set_cache) (navdata, cache, prefix);
}

/* Функция получения значения. */
gboolean
hyscan_nav_data_get (HyScanNavData *navdata,
                     guint32        index,
                     gint64        *time,
                     gdouble       *value)
{
  HyScanNavDataInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_NAV_DATA (navdata), FALSE);
  iface = HYSCAN_NAV_DATA_GET_IFACE (navdata);

  if (iface->get != NULL)
    return (*iface->get) (navdata, index, time, value);

  return FALSE;
}

/* Функция поиска данных. */
HyScanDBFindStatus
hyscan_nav_data_find_data (HyScanNavData *navdata,
                           gint64         time,
                           guint32       *lindex,
                           guint32       *rindex,
                           gint64        *ltime,
                           gint64        *rtime)
{
  HyScanNavDataInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_NAV_DATA (navdata), HYSCAN_DB_FIND_FAIL);
  iface = HYSCAN_NAV_DATA_GET_IFACE (navdata);

  if (iface->find_data != NULL)
    return (*iface->find_data) (navdata, time, lindex, rindex, ltime, rtime);

  return HYSCAN_DB_FIND_FAIL;
}

/* Функция определения диапазона. */
gboolean
hyscan_nav_data_get_range (HyScanNavData *navdata,
                           guint32       *first,
                           guint32       *last)
{
  HyScanNavDataInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_NAV_DATA (navdata), FALSE);
  iface = HYSCAN_NAV_DATA_GET_IFACE (navdata);

  if (iface->get_range != NULL)
    return (*iface->get_range) (navdata, first, last);

  return FALSE;
}

/* Функция получения местоположения антенны. */
HyScanAntennaPosition
hyscan_nav_data_get_position (HyScanNavData *navdata)
{
  HyScanNavDataInterface *iface;
  HyScanAntennaPosition zero = {0};

  g_return_val_if_fail (HYSCAN_IS_NAV_DATA (navdata), zero);
  iface = HYSCAN_NAV_DATA_GET_IFACE (navdata);

  if (iface->get_position != NULL)
    return (*iface->get_position) (navdata);

  return zero;
}

/* Функция определяет, возможна ли дозапись в канал данных. */
gboolean
hyscan_nav_data_is_writable (HyScanNavData *navdata)
{
  HyScanNavDataInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_NAV_DATA (navdata), FALSE);
  iface = HYSCAN_NAV_DATA_GET_IFACE (navdata);

  if (iface->is_writable != NULL)
    return (*iface->is_writable) (navdata);

  return FALSE;
}

/* Функция возвращает токен объекта. */
const gchar*
hyscan_nav_data_get_token (HyScanNavData *navdata)
{
  HyScanNavDataInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_NAV_DATA (navdata), NULL);
  iface = HYSCAN_NAV_DATA_GET_IFACE (navdata);

  if (iface->get_token != NULL)
    return (*iface->get_token) (navdata);

  return NULL;
}

/* Функция возвращает номер изменения. */
guint32
hyscan_nav_data_get_mod_count (HyScanNavData *navdata)
{
  HyScanNavDataInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_NAV_DATA (navdata), 0);
  iface = HYSCAN_NAV_DATA_GET_IFACE (navdata);

  if (iface->get_mod_count != NULL)
    return (*iface->get_mod_count) (navdata);

  return 0;
}
