/* hyscan-nav-model.h
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
 * Во-первых, вы можете распространять HyScanGui на условиях Стандартной
 * Общественной Лицензии GNU версии 3, либо по любой более поздней версии
 * лицензии (по вашему выбору). Полные положения лицензии GNU приведены в
 * <http://www.gnu.org/licenses/>.
 *
 * Во-вторых, этот программный код можно использовать по коммерческой
 * лицензии. Для этого свяжитесь с ООО Экран - <info@screen-co.ru>.
 */

#ifndef __HYSCAN_NAV_MODEL_H__
#define __HYSCAN_NAV_MODEL_H__

#include <hyscan-sensor.h>
#include <hyscan-nav-state.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_NAV_MODEL             (hyscan_nav_model_get_type ())
#define HYSCAN_NAV_MODEL(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_NAV_MODEL, HyScanNavModel))
#define HYSCAN_IS_NAV_MODEL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_NAV_MODEL))
#define HYSCAN_NAV_MODEL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_NAV_MODEL, HyScanNavModelClass))
#define HYSCAN_IS_NAV_MODEL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_NAV_MODEL))
#define HYSCAN_NAV_MODEL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_NAV_MODEL, HyScanNavModelClass))

typedef struct _HyScanNavModel HyScanNavModel;
typedef struct _HyScanNavModelPrivate HyScanNavModelPrivate;
typedef struct _HyScanNavModelClass HyScanNavModelClass;

struct _HyScanNavModel
{
  GObject parent_instance;

  HyScanNavModelPrivate *priv;
};

struct _HyScanNavModelClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                    hyscan_nav_model_get_type         (void);

HYSCAN_API
HyScanNavModel *         hyscan_nav_model_new              (void);

HYSCAN_API
void                     hyscan_nav_model_set_sensor       (HyScanNavModel            *model,
                                                            HyScanSensor              *sensor);

HYSCAN_API
void                     hyscan_nav_model_set_sensor_name  (HyScanNavModel            *model,
                                                            const gchar               *name);

HYSCAN_API
void                     hyscan_nav_model_set_offset       (HyScanNavModel            *model,
                                                            const HyScanAntennaOffset *offset);

HYSCAN_API
void                     hyscan_nav_model_set_delay        (HyScanNavModel            *model,
                                                            gdouble                    delay);

G_END_DECLS

#endif /* __HYSCAN_NAV_MODEL_H__ */
