/* hyscan-planner.h
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

#ifndef __HYSCAN_PLANNER_H__
#define __HYSCAN_PLANNER_H__

#include <hyscan-geo.h>
#include <hyscan-navigation-model.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_PLANNER             (hyscan_planner_get_type ())
#define HYSCAN_PLANNER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_PLANNER, HyScanPlanner))
#define HYSCAN_IS_PLANNER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_PLANNER))
#define HYSCAN_PLANNER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_PLANNER, HyScanPlannerClass))
#define HYSCAN_IS_PLANNER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_PLANNER))
#define HYSCAN_PLANNER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_PLANNER, HyScanPlannerClass))

typedef struct _HyScanPlanner HyScanPlanner;
typedef struct _HyScanPlannerTrack HyScanPlannerTrack;
typedef struct _HyScanPlannerPrivate HyScanPlannerPrivate;
typedef struct _HyScanPlannerClass HyScanPlannerClass;

/**
 * HyScanPlannerTrack:
 * @id: уникальный идентификатор
 * @name: название
 * @start: геокоординаты начала
 * @start: геокоординаты конца
 *
 * Параметры запланированного галса
 */
struct _HyScanPlannerTrack
{
  gchar             *id;
  gchar             *name;
  HyScanGeoGeodetic  start;
  HyScanGeoGeodetic  end;
};

struct _HyScanPlanner
{
  GObject parent_instance;

  HyScanPlannerPrivate *priv;
};

struct _HyScanPlannerClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                  hyscan_planner_get_type         (void);

HYSCAN_API
HyScanPlanner *        hyscan_planner_new              (void);

HYSCAN_API
void                   hyscan_planner_load_ini         (HyScanPlanner            *planner,
                                                        const gchar              *file_name);

HYSCAN_API
void                   hyscan_planner_track_free       (HyScanPlannerTrack       *track);

HYSCAN_API
HyScanPlannerTrack *   hyscan_planner_track_copy       (const HyScanPlannerTrack *track);

HYSCAN_API
void                   hyscan_planner_set_nav_model    (HyScanPlanner            *planner,
                                                        HyScanNavigationModel    *model);

HYSCAN_API
void                   hyscan_planner_create           (HyScanPlanner            *planner,
                                                        HyScanPlannerTrack       *track);

HYSCAN_API
void                   hyscan_planner_update           (HyScanPlanner            *planner,
                                                        const HyScanPlannerTrack *track);

HYSCAN_API
void                   hyscan_planner_delete           (HyScanPlanner            *planner,
                                                        const gchar              *id);

HYSCAN_API
GHashTable *           hyscan_planner_get              (HyScanPlanner            *planner);

HYSCAN_API
gboolean               hyscan_planner_delta            (HyScanPlanner            *planner,
                                                        gdouble                  *distance,
                                                        gdouble                  *angle,
                                                        guint64                  *time_passed);

HYSCAN_API
void                   hyscan_planner_save_ini         (HyScanPlanner            *planner,
                                                        const gchar              *file_name);

G_END_DECLS

#endif /* __HYSCAN_PLANNER_H__ */
