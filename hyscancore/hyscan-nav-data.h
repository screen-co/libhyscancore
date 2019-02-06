/* hyscan-nav-data.h
 *
 * Copyright 2018 Screen LLC, Alexander Dmitriev <m1n7@yandex.ru>
 *
 * This file is part of HyScanCore library.
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

#ifndef __HYSCAN_NAV_DATA_H__
#define __HYSCAN_NAV_DATA_H__

#include <hyscan-db.h>
#include <hyscan-cache.h>
#include <hyscan-types.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_NAV_DATA            (hyscan_nav_data_get_type ())
#define HYSCAN_NAV_DATA(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_NAV_DATA, HyScanNavData))
#define HYSCAN_IS_NAV_DATA(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_NAV_DATA))
#define HYSCAN_NAV_DATA_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), HYSCAN_TYPE_NAV_DATA, HyScanNavDataInterface))

typedef struct _HyScanNavData HyScanNavData;
typedef struct _HyScanNavDataInterface HyScanNavDataInterface;

/**
 * HyScanNavDataInterface:
 * @g_iface: Базовый интерфейс.
 * @get: Возвращает значение.
 * @find_data: Ищет данные.
 * @get_range: Определяет диапазон.
 * @get_offset: Возвращает смещение антенны.
 * @is_writable: Определяет возможность записи в канал данных.
 * @get_token: Возвращает токен.
 * @get_mod_count: Возвращает счётчик изменений.
 */
struct _HyScanNavDataInterface
{
  GTypeInterface            g_iface;

  gboolean                (*get)                          (HyScanNavData       *ndata,
                                                           guint32              index,
                                                           gint64              *time,
                                                           gdouble             *value);
  HyScanDBFindStatus      (*find_data)                    (HyScanNavData       *ndata,
                                                           gint64               time,
                                                           guint32             *lindex,
                                                           guint32             *rindex,
                                                           gint64              *ltime,
                                                           gint64              *rtime);
  gboolean                (*get_range)                    (HyScanNavData       *ndata,
                                                           guint32             *first,
                                                           guint32             *last);
  HyScanAntennaOffset     (*get_offset)                   (HyScanNavData       *ndata);
  gboolean                (*is_writable)                  (HyScanNavData       *ndata);
  const gchar            *(*get_token)                    (HyScanNavData       *ndata);
  guint32                 (*get_mod_count)                (HyScanNavData       *ndata);
};

HYSCAN_API
GType                   hyscan_nav_data_get_type        (void);

HYSCAN_API
gboolean                hyscan_nav_data_get             (HyScanNavData         *ndata,
                                                         guint32                index,
                                                         gint64                *time,
                                                         gdouble               *value);

HYSCAN_API
HyScanDBFindStatus      hyscan_nav_data_find_data       (HyScanNavData         *ndata,
                                                         gint64                 time,
                                                         guint32               *lindex,
                                                         guint32               *rindex,
                                                         gint64                *ltime,
                                                         gint64                *rtime);

HYSCAN_API
gboolean                hyscan_nav_data_get_range       (HyScanNavData         *ndata,
                                                         guint32               *first,
                                                         guint32               *last);

HYSCAN_API
HyScanAntennaOffset     hyscan_nav_data_get_offset      (HyScanNavData         *ndata);

HYSCAN_API
gboolean                hyscan_nav_data_is_writable     (HyScanNavData         *ndata);


HYSCAN_API
const gchar            *hyscan_nav_data_get_token       (HyScanNavData         *ndata);

HYSCAN_API
guint32                 hyscan_nav_data_get_mod_count   (HyScanNavData          *ndata);

G_END_DECLS

#endif /* __HYSCAN_NAV_DATA_H__ */
