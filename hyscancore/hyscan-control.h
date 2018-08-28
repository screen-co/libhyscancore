/* hyscan-control.h
 *
 * Copyright 2016-2018 Screen LLC, Andrei Fadeev <andrei@webcontrol.ru>
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
 * Contact the Screen LLC in this case - info@screen-co.ru
 */

/* HyScanCore имеет двойную лицензию.
 *
 * Во-первых, вы можете распространять HyScanCore на условиях Стандартной
 * Общественной Лицензии GNU версии 3, либо по любой более поздней версии
 * лицензии (по вашему выбору). Полные положения лицензии GNU приведены в
 * <http://www.gnu.org/licenses/>.
 *
 * Во-вторых, этот программный код можно использовать по коммерческой
 * лицензии. Для этого свяжитесь с ООО Экран - info@screen-co.ru.
 */

#ifndef __HYSCAN_CONTROL_H__
#define __HYSCAN_CONTROL_H__

#include <hyscan-db.h>
#include <hyscan-param.h>
#include <hyscan-sonar-info.h>
#include <hyscan-sensor-info.h>
#include <hyscan-data-writer.h>
#include <hyscan-discover.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_CONTROL              (hyscan_control_get_type ())
#define HYSCAN_CONTROL(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_CONTROL, HyScanControl))
#define HYSCAN_IS_CONTROL(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_CONTROL))
#define HYSCAN_CONTROL_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_CONTROL, HyScanControlClass))
#define HYSCAN_IS_CONTROL_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_CONTROL))
#define HYSCAN_CONTROL_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_CONTROL, HyScanControlClass))

/**
 * HyScanDeviceStatus:
 * HYSCAN_DEVICE_STATUS_OK: устройство функционирует в штатном режиме
 * HYSCAN_DEVICE_STATUS_WARNING: в работе устройства присутствуют кратковременные сбои
 * HYSCAN_DEVICE_STATUS_CRITICAL: в работе устройства присутствуют постоянные сбои
 * HYSCAN_DEVICE_STATUS_ERROR: ошибка в работе, устройство не может продолжать работу
 *
 * Типы состояния устройства.
 */
typedef enum
{
  HYSCAN_DEVICE_STATUS_OK,
  HYSCAN_DEVICE_STATUS_WARNING,
  HYSCAN_DEVICE_STATUS_CRITICAL,
  HYSCAN_DEVICE_STATUS_ERROR
} HyScanDeviceStatus;

typedef struct _HyScanControl HyScanControl;
typedef struct _HyScanControlPrivate HyScanControlPrivate;
typedef struct _HyScanControlClass HyScanControlClass;

struct _HyScanControl
{
  GObject parent_instance;

  HyScanControlPrivate *priv;
};

struct _HyScanControlClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                          hyscan_control_get_type                 (void);

HYSCAN_API
HyScanControl *                hyscan_control_new                      (void);

HYSCAN_API
gboolean                       hyscan_control_device_add               (HyScanControl                  *control,
                                                                        HyScanDevice                   *device);

HYSCAN_API
gboolean                       hyscan_control_device_bind              (HyScanControl                  *control);

HYSCAN_API
const gchar * const *          hyscan_control_devices_list             (HyScanControl                  *control);

HYSCAN_API
const gchar * const *          hyscan_control_sensors_list             (HyScanControl                  *control);

HYSCAN_API
const HyScanSourceType *       hyscan_control_sources_list             (HyScanControl                  *control,
                                                                        guint32                        *n_sources);

HYSCAN_API
gboolean                       hyscan_control_get_software_ping        (HyScanControl                  *control);

HYSCAN_API
HyScanDeviceStatus             hyscan_control_device_get_status        (HyScanControl                  *control,
                                                                        const gchar                    *dev_id);

HYSCAN_API
const HyScanSensorInfoSensor * hyscan_control_sensor_get_info          (HyScanControl                  *control,
                                                                        const gchar                    *sensor);

HYSCAN_API
const HyScanSonarInfoSource *  hyscan_control_source_get_info          (HyScanControl                  *control,
                                                                        HyScanSourceType                source);

HYSCAN_API
gboolean                       hyscan_control_sensor_set_position      (HyScanControl                  *control,
                                                                        const gchar                    *sensor,
                                                                        const HyScanAntennaPosition    *position);

HYSCAN_API
gboolean                       hyscan_control_source_set_position      (HyScanControl                  *control,
                                                                        HyScanSourceType                source,
                                                                        const HyScanAntennaPosition    *position);

HYSCAN_API
gboolean                       hyscan_control_sensor_set_channel       (HyScanControl                  *control,
                                                                        const gchar                    *sensor,
                                                                        guint                           channel);

HYSCAN_API
void                           hyscan_control_writer_set_db            (HyScanControl                  *control,
                                                                        HyScanDB                       *db);

HYSCAN_API
void                           hyscan_control_writer_set_operator_name (HyScanControl                  *control,
                                                                        const gchar                    *name);

HYSCAN_API
void                           hyscan_control_writer_set_chunk_size    (HyScanControl                  *control,
                                                                        gint32                          chunk_size);

G_END_DECLS

#endif /* __HYSCAN_CONTROL_H__ */
