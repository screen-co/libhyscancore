/* hyscan-object-data-planner.h
 *
 * Copyright 2019 Screen LLC, Alexey Sakhnov <alexsakhnov@gmail.com>
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

#ifndef __HYSCAN_OBJECT_DATA_PLANNER_H__
#define __HYSCAN_OBJECT_DATA_PLANNER_H__

#include <hyscan-planner.h>
#include <hyscan-object-data.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_OBJECT_DATA_PLANNER             (hyscan_object_data_planner_get_type ())
#define HYSCAN_OBJECT_DATA_PLANNER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_OBJECT_DATA_PLANNER, HyScanObjectDataPlanner))
#define HYSCAN_IS_OBJECT_DATA_PLANNER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_OBJECT_DATA_PLANNER))
#define HYSCAN_OBJECT_DATA_PLANNER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_OBJECT_DATA_PLANNER, HyScanObjectDataPlannerClass))
#define HYSCAN_IS_OBJECT_DATA_PLANNER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_OBJECT_DATA_PLANNER))
#define HYSCAN_OBJECT_DATA_PLANNER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_OBJECT_DATA_PLANNER, HyScanObjectDataPlannerClass))

typedef struct _HyScanObjectDataPlanner HyScanObjectDataPlanner;
typedef struct _HyScanObjectDataPlannerPrivate HyScanObjectDataPlannerPrivate;
typedef struct _HyScanObjectDataPlannerClass HyScanObjectDataPlannerClass;

struct _HyScanObjectDataPlanner
{
  HyScanObjectData parent_instance;

  HyScanObjectDataPlannerPrivate *priv;
};

struct _HyScanObjectDataPlannerClass
{
  HyScanObjectDataClass parent_class;
};

HYSCAN_API
GType                  hyscan_object_data_planner_get_type         (void);

HYSCAN_API
HyScanObjectData *     hyscan_object_data_planner_new              (void);

G_END_DECLS

#endif /* __HYSCAN_OBJECT_DATA_PLANNER_H__ */
