/* hyscan-map-track-param.h
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

#ifndef __HYSCAN_MAP_TRACK_PARAM_H__
#define __HYSCAN_MAP_TRACK_PARAM_H__

#include <hyscan-db.h>
#include <hyscan-param.h>
#include "hyscan-nav-data.h"
#include "hyscan-nmea-parser.h"
#include "hyscan-depthometer.h"

G_BEGIN_DECLS

#define HYSCAN_TYPE_MAP_TRACK_PARAM             (hyscan_map_track_param_get_type ())
#define HYSCAN_MAP_TRACK_PARAM(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_MAP_TRACK_PARAM, HyScanMapTrackParam))
#define HYSCAN_IS_MAP_TRACK_PARAM(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_MAP_TRACK_PARAM))
#define HYSCAN_MAP_TRACK_PARAM_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_MAP_TRACK_PARAM, HyScanMapTrackParamClass))
#define HYSCAN_IS_MAP_TRACK_PARAM_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_MAP_TRACK_PARAM))
#define HYSCAN_MAP_TRACK_PARAM_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_MAP_TRACK_PARAM, HyScanMapTrackParamClass))

typedef struct _HyScanMapTrackParam HyScanMapTrackParam;
typedef struct _HyScanMapTrackParamPrivate HyScanMapTrackParamPrivate;
typedef struct _HyScanMapTrackParamClass HyScanMapTrackParamClass;

struct _HyScanMapTrackParam
{
  GObject parent_instance;

  HyScanMapTrackParamPrivate *priv;
};

struct _HyScanMapTrackParamClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                  hyscan_map_track_param_get_type        (void);

HYSCAN_API
HyScanMapTrackParam *  hyscan_map_track_param_new             (gchar                *profile,
                                                               HyScanDB             *db,
                                                               const gchar          *project_name,
                                                               const gchar          *track_name);

HYSCAN_API
guint32                hyscan_map_track_param_get_mod_count   (HyScanMapTrackParam  *param);

HYSCAN_API
gboolean               hyscan_map_track_param_has_rmc         (HyScanMapTrackParam  *param);

HYSCAN_API
HyScanNavData *        hyscan_map_track_param_get_nav_data    (HyScanMapTrackParam  *param,
                                                               HyScanNMEAField       field,
                                                               HyScanCache          *cache);

HYSCAN_API
HyScanDepthometer *    hyscan_map_track_param_get_depthometer (HyScanMapTrackParam  *param,
                                                               HyScanCache          *cache);

HYSCAN_API
gboolean               hyscan_map_track_param_clear           (HyScanMapTrackParam  *param);

G_END_DECLS

#endif /* __HYSCAN_MAP_TRACK_PARAM_H__ */
