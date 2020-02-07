/* hyscan-nav-smooth.h
 *
 * Copyright 2020 Screen LLC, Alexey Sakhnov <alexsakhnov@gmail.com>
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

#ifndef __HYSCAN_NAV_SMOOTH_H__
#define __HYSCAN_NAV_SMOOTH_H__

#include <hyscan-nav-data.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_NAV_SMOOTH             (hyscan_nav_smooth_get_type ())
#define HYSCAN_NAV_SMOOTH(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_NAV_SMOOTH, HyScanNavSmooth))
#define HYSCAN_IS_NAV_SMOOTH(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_NAV_SMOOTH))
#define HYSCAN_NAV_SMOOTH_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_NAV_SMOOTH, HyScanNavSmoothClass))
#define HYSCAN_IS_NAV_SMOOTH_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_NAV_SMOOTH))
#define HYSCAN_NAV_SMOOTH_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_NAV_SMOOTH, HyScanNavSmoothClass))

typedef struct _HyScanNavSmooth HyScanNavSmooth;
typedef struct _HyScanNavSmoothPrivate HyScanNavSmoothPrivate;
typedef struct _HyScanNavSmoothClass HyScanNavSmoothClass;

struct _HyScanNavSmooth
{
  GObject parent_instance;

  HyScanNavSmoothPrivate *priv;
};

struct _HyScanNavSmoothClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                  hyscan_nav_smooth_get_type         (void);

HYSCAN_API
HyScanNavSmooth *      hyscan_nav_smooth_new              (HyScanNavData     *nav_data);

HYSCAN_API
HyScanNavSmooth *      hyscan_nav_smooth_new_circular     (HyScanNavData     *nav_data);

HYSCAN_API
gboolean               hyscan_nav_smooth_get              (HyScanNavSmooth   *nav_smooth,
                                                           HyScanCancellable *cancellable,
                                                           gint64             time,
                                                           gdouble           *value);

HYSCAN_API
HyScanNavData *        hyscan_nav_smooth_get_data         (HyScanNavSmooth   *nav_smooth);

G_END_DECLS

#endif /* __HYSCAN_NAV_SMOOTH_H__ */
