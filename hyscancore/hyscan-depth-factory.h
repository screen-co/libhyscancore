/* hyscan-depth-factory.h
 *
 * Copyright 2018-2019 Screen LLC, Alexander Dmitriev <m1n7@yandex.ru>
 *
 * This file is part of HyScanCore.
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

#ifndef __HYSCAN_DEPTH_FACTORY_H__
#define __HYSCAN_DEPTH_FACTORY_H__

#include <hyscan-depthometer.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_DEPTH_FACTORY             (hyscan_depth_factory_get_type ())
#define HYSCAN_DEPTH_FACTORY(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_DEPTH_FACTORY, HyScanDepthFactory))
#define HYSCAN_IS_DEPTH_FACTORY(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_DEPTH_FACTORY))
#define HYSCAN_DEPTH_FACTORY_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_DEPTH_FACTORY, HyScanDepthFactoryClass))
#define HYSCAN_IS_DEPTH_FACTORY_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_DEPTH_FACTORY))
#define HYSCAN_DEPTH_FACTORY_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_DEPTH_FACTORY, HyScanDepthFactoryClass))

typedef struct _HyScanDepthFactory HyScanDepthFactory;
typedef struct _HyScanDepthFactoryPrivate HyScanDepthFactoryPrivate;
typedef struct _HyScanDepthFactoryClass HyScanDepthFactoryClass;

struct _HyScanDepthFactory
{
  GObject parent_instance;

  HyScanDepthFactoryPrivate *priv;
};

struct _HyScanDepthFactoryClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                  hyscan_depth_factory_get_type         (void);

HYSCAN_API
HyScanDepthFactory *   hyscan_depth_factory_new              (HyScanCache            *cache);

HYSCAN_API
gchar *                hyscan_depth_factory_get_token        (HyScanDepthFactory     *factory);

HYSCAN_API
void                   hyscan_depth_factory_set_track        (HyScanDepthFactory     *factory,
                                                              HyScanDB               *db,
                                                              const gchar            *project_name,
                                                              const gchar            *track_name);

HYSCAN_API
HyScanDepthometer *    hyscan_depth_factory_produce          (HyScanDepthFactory     *factory);

G_END_DECLS

#endif /* __HYSCAN_DEPTH_FACTORY_H__ */
