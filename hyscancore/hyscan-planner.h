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
#include <hyscan-nav-model.h>
#include <hyscan-db.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_PLANNER             (hyscan_planner_get_type ())
#define HYSCAN_PLANNER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_PLANNER, HyScanPlanner))
#define HYSCAN_IS_PLANNER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_PLANNER))
#define HYSCAN_PLANNER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_PLANNER, HyScanPlannerClass))
#define HYSCAN_IS_PLANNER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_PLANNER))
#define HYSCAN_PLANNER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_PLANNER, HyScanPlannerClass))

typedef struct _HyScanPlanner HyScanPlanner;
typedef struct _HyScanPlannerTrack HyScanPlannerTrack;
typedef struct _HyScanPlannerZone HyScanPlannerZone;
typedef struct _HyScanPlannerPrivate HyScanPlannerPrivate;
typedef struct _HyScanPlannerClass HyScanPlannerClass;

/**
 * HyScanPlannerTrack:
 * @id: уникальный идентификатор
 * @zone_id: идентификатор зоны, в котрой находится галс
 * @number: порядковый номер галса
 * @speed: скокрость движения судна, м/с
 * @name: название
 * @start: геокоординаты начала
 * @start: геокоординаты конца
 *
 * Параметры запланированного галса
 */
struct _HyScanPlannerTrack
{
  gchar             *id;
  gchar             *zone_id;
  guint              number;
  gdouble            speed;
  gchar             *name;
  HyScanGeoGeodetic  start;
  HyScanGeoGeodetic  end;
};

/**
 * HyScanPlannerZone:
 * @id: уникальный идентификатор
 * @name: название зоны
 * @points: (element-type HyScanGeoGeodetic): список вершин многоугольника, ограничивающего зону
 * @points_len: число вершин многоугольника
 *
 * Параметры зоны исследования
 */
struct _HyScanPlannerZone
{
  gchar             *id;
  gchar             *name;
  HyScanGeoGeodetic *points;
  gsize              points_len;
  gint64             mtime;
  gint64             ctime;
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
HyScanPlanner *        hyscan_planner_new              (HyScanDB                 *db,
                                                        const gchar              *project_name);

HYSCAN_API
gchar **               hyscan_planner_zone_list        (HyScanPlanner            *planner);

HYSCAN_API
gchar *                hyscan_planner_zone_create      (HyScanPlanner            *planner,
                                                        const HyScanPlannerZone  *zone);

HYSCAN_API
gboolean               hyscan_planner_zone_set         (HyScanPlanner            *planner,
                                                        const HyScanPlannerZone  *zone);

HYSCAN_API
HyScanPlannerZone *    hyscan_planner_zone_get         (HyScanPlanner            *planner,
                                                        const gchar              *zone_id);

HYSCAN_API
gboolean               hyscan_planner_zone_remove      (HyScanPlanner            *planner,
                                                        const gchar              *zone_id);

HYSCAN_API
void                   hyscan_planner_zone_free        (HyScanPlannerZone        *zone);

HYSCAN_API
gchar *                hyscan_planner_track_create     (HyScanPlanner            *planner,
                                                        const HyScanPlannerTrack  *track);

HYSCAN_API
gboolean               hyscan_planner_track_set        (HyScanPlanner            *planner,
                                                        const HyScanPlannerTrack  *track);

HYSCAN_API
gchar **               hyscan_planner_track_list       (HyScanPlanner            *planner,
                                                        const gchar              *zone_id);

HYSCAN_API
HyScanPlannerTrack *   hyscan_planner_track_get        (HyScanPlanner            *planner,
                                                        const gchar              *track_id);

HYSCAN_API
gboolean               hyscan_planner_track_remove     (HyScanPlanner            *planner,
                                                        const gchar              *track_id);

HYSCAN_API
void                   hyscan_planner_track_free       (HyScanPlannerTrack        *track);

G_END_DECLS

#endif /* __HYSCAN_PLANNER_H__ */
