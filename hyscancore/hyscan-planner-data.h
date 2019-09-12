/* hyscan-planner-data.h
 *
 * Copyright 2019 Screen LLC, Alexey Sakhnov <alexsakhnov@gmail.com>
 *
 * This file is part of HyScanGui library.
 *
 * HyScanGui is dual-licensed: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * HyScanGui is distributed in the hope that it will be useful,
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

/* HyScanGui имеет двойную лицензию.
 *
 * Во-первых, вы можете распространять HyScanGui на условиях Стандартной
 * Общественной Лицензии GNU версии 3, либо по любой более поздней версии
 * лицензии (по вашему выбору). Полные положения лицензии GNU приведены в
 * <http://www.gnu.org/licenses/>.
 *
 * Во-вторых, этот программный код можно использовать по коммерческой
 * лицензии. Для этого свяжитесь с ООО Экран - <info@screen-co.ru>.
 */

#ifndef __HYSCAN_PLANNER_DATA_H__
#define __HYSCAN_PLANNER_DATA_H__

#include <hyscan-planner.h>
#include <hyscan-object-data.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_PLANNER_DATA             (hyscan_planner_data_get_type ())
#define HYSCAN_PLANNER_DATA(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_PLANNER_DATA, HyScanPlannerData))
#define HYSCAN_IS_PLANNER_DATA(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_PLANNER_DATA))
#define HYSCAN_PLANNER_DATA_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_PLANNER_DATA, HyScanPlannerDataClass))
#define HYSCAN_IS_PLANNER_DATA_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_PLANNER_DATA))
#define HYSCAN_PLANNER_DATA_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_PLANNER_DATA, HyScanPlannerDataClass))

typedef struct _HyScanPlannerData HyScanPlannerData;
typedef struct _HyScanPlannerDataPrivate HyScanPlannerDataPrivate;
typedef struct _HyScanPlannerDataClass HyScanPlannerDataClass;

struct _HyScanPlannerData
{
  HyScanObjectData parent_instance;

  HyScanPlannerDataPrivate *priv;
};

struct _HyScanPlannerDataClass
{
  HyScanObjectDataClass parent_class;
};

HYSCAN_API
GType                  hyscan_planner_data_get_type         (void);

HYSCAN_API
HyScanObjectData *     hyscan_planner_data_new              (HyScanDB    *db,
                                                             const gchar *project);

G_END_DECLS

#endif /* __HYSCAN_PLANNER_DATA_H__ */
