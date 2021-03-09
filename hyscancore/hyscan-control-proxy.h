/* hyscan-control-proxy.h
 *
 * Copyright 2019 Screen LLC, Andrei Fadeev <andrei@webcontrol.ru>
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

#ifndef __HYSCAN_CONTROL_PROXY_H__
#define __HYSCAN_CONTROL_PROXY_H__

#include <hyscan-control.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_CONTROL_PROXY             (hyscan_control_proxy_get_type ())
#define HYSCAN_CONTROL_PROXY(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_CONTROL_PROXY, HyScanControlProxy))
#define HYSCAN_IS_CONTROL_PROXY(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_CONTROL_PROXY))
#define HYSCAN_CONTROL_PROXY_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_CONTROL_PROXY, HyScanControlProxyClass))
#define HYSCAN_IS_CONTROL_PROXY_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_CONTROL_PROXY))
#define HYSCAN_CONTROL_PROXY_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_CONTROL_PROXY, HyScanControlProxyClass))

typedef struct _HyScanControlProxy HyScanControlProxy;
typedef struct _HyScanControlProxyPrivate HyScanControlProxyPrivate;
typedef struct _HyScanControlProxyClass HyScanControlProxyClass;

struct _HyScanControlProxy
{
  GObject parent_instance;

  HyScanControlProxyPrivate *priv;
};

struct _HyScanControlProxyClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                              hyscan_control_proxy_get_type          (void);

HYSCAN_API
HyScanControlProxy *               hyscan_control_proxy_new               (HyScanControl                  *control,
                                                                           const gchar                    *dev_id);

HYSCAN_API
void                               hyscan_control_proxy_set_scale         (HyScanControlProxy             *proxy,
                                                                           HyScanSourceType                source,
                                                                           guint                           line_scale,
                                                                           guint                           point_scale);

HYSCAN_API
void                               hyscan_control_proxy_set_data_type     (HyScanControlProxy             *proxy,
                                                                           HyScanSourceType                source,
                                                                           HyScanDataType                  type);

HYSCAN_API
const gchar * const *              hyscan_control_proxy_sensors_list      (HyScanControlProxy              *proxy);

HYSCAN_API
const HyScanSourceType *           hyscan_control_proxy_sources_list      (HyScanControlProxy             *proxy,
                                                                           guint32                        *n_sources);

HYSCAN_API
const gchar * const *              hyscan_control_proxy_actuators_list    (HyScanControlProxy             *control);

HYSCAN_API
const HyScanSensorInfoSensor *     hyscan_control_proxy_sensor_get_info   (HyScanControlProxy             *proxy,
                                                                           const gchar                    *sensor);

HYSCAN_API
const HyScanSonarInfoSource *      hyscan_control_proxy_source_get_info   (HyScanControlProxy             *proxy,
                                                                           HyScanSourceType                source);

HYSCAN_API
const HyScanActuatorInfoActuator * hyscan_control_proxy_actuator_get_info (HyScanControlProxy             *proxy,
                                                                           const gchar                    *actuator);

HYSCAN_API
void                               hyscan_control_proxy_sensor_set_sender (HyScanControlProxy             *proxy,
                                                                           const gchar                    *sensor,
                                                                           gboolean                        enable);

HYSCAN_API
void                               hyscan_control_proxy_source_set_sender (HyScanControlProxy             *proxy,
                                                                           HyScanSourceType                source,
                                                                           gboolean                        enable);

G_END_DECLS

#endif /* __HYSCAN_CONTROL_PROXY_H__ */
