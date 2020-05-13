/* hyscan-proj.h
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

#ifndef __HYSCAN_PROJ_H__
#define __HYSCAN_PROJ_H__

#include <hyscan-geo-projection.h>

#define HYSCAN_PROJ_MERC    "+proj=merc +ellps=WGS84"
#define HYSCAN_PROJ_WEBMERC "+proj=merc +a=6378137 +b=6378137"

G_BEGIN_DECLS

#define HYSCAN_TYPE_PROJ             (hyscan_proj_get_type ())
#define HYSCAN_PROJ(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_PROJ, HyScanProj))
#define HYSCAN_IS_PROJ(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_PROJ))
#define HYSCAN_PROJ_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_PROJ, HyScanProjClass))
#define HYSCAN_IS_PROJ_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_PROJ))
#define HYSCAN_PROJ_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_PROJ, HyScanProjClass))

typedef struct _HyScanProj HyScanProj;
typedef struct _HyScanProjPrivate HyScanProjPrivate;
typedef struct _HyScanProjClass HyScanProjClass;

struct _HyScanProj
{
  GObject parent_instance;

  HyScanProjPrivate *priv;
};

struct _HyScanProjClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                  hyscan_proj_get_type         (void);

HYSCAN_API
HyScanGeoProjection *  hyscan_proj_new              (const gchar *definition);

G_END_DECLS

#endif /* __HYSCAN_PROJ_H__ */