/* hyscan-dummy-device.h
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

#ifndef __HYSCAN_DUMMY_DEVICE_H__
#define __HYSCAN_DUMMY_DEVICE_H__

#include <hyscan-param.h>
#include <hyscan-sonar-schema.h>
#include <hyscan-sensor-schema.h>

G_BEGIN_DECLS

/**
 * HyScanDummyDeviceType:
 * @HYSCAN_DUMMY_DEVICE_INVALIDE: неизвестный тип
 * @HYSCAN_DUMMY_DEVICE_SIDE_SCAN: гидролокатор бокового обзора
 * @HYSCAN_DUMMY_DEVICE_PROFILER: профилограф со встроенным эхолотом
 *
 * Тип эмулируемого устройства.
 */
typedef enum
{
  HYSCAN_DUMMY_DEVICE_INVALID,
  HYSCAN_DUMMY_DEVICE_SIDE_SCAN,
  HYSCAN_DUMMY_DEVICE_PROFILER
} HyScanDummyDeviceType;

#define HYSCAN_TYPE_DUMMY_DEVICE             (hyscan_dummy_device_get_type ())
#define HYSCAN_DUMMY_DEVICE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_DUMMY_DEVICE, HyScanDummyDevice))
#define HYSCAN_IS_DUMMY_DEVICE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_DUMMY_DEVICE))
#define HYSCAN_DUMMY_DEVICE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_DUMMY_DEVICE, HyScanDummyDeviceClass))
#define HYSCAN_IS_DUMMY_DEVICE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_DUMMY_DEVICE))
#define HYSCAN_DUMMY_DEVICE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_DUMMY_DEVICE, HyScanDummyDeviceClass))

typedef struct _HyScanDummyDevice HyScanDummyDevice;
typedef struct _HyScanDummyDevicePrivate HyScanDummyDevicePrivate;
typedef struct _HyScanDummyDeviceClass HyScanDummyDeviceClass;

struct _HyScanDummyDevice
{
  GObject parent_instance;

  HyScanDummyDevicePrivate *priv;
};

struct _HyScanDummyDeviceClass
{
  GObjectClass parent_class;
};

GType                    hyscan_dummy_device_get_type                 (void);

HyScanDummyDevice *      hyscan_dummy_device_new                      (HyScanDummyDeviceType           type);

const gchar *            hyscan_dummy_device_get_id                   (HyScanDummyDevice              *dummy);

void                     hyscan_dummy_device_change_state             (HyScanDummyDevice              *dummy);

void                     hyscan_dummy_device_send_data                (HyScanDummyDevice              *dummy);

gboolean                 hyscan_dummy_device_check_sound_velocity     (HyScanDummyDevice              *dummy,
                                                                       GList                          *svp);

gboolean                 hyscan_dummy_device_check_receiver_time      (HyScanDummyDevice              *dummy,
                                                                       gdouble                         receive_time,
                                                                       gdouble                         wait_time);

gboolean                 hyscan_dummy_device_check_receiver_auto      (HyScanDummyDevice              *dummy);

gboolean                 hyscan_dummy_device_check_generator_preset   (HyScanDummyDevice              *dummy,
                                                                       gint64                          preset);

gboolean                 hyscan_dummy_device_check_generator_auto     (HyScanDummyDevice              *dummy,
                                                                       HyScanSonarGeneratorSignalType  signal);

gboolean                 hyscan_dummy_device_check_generator_simple   (HyScanDummyDevice              *dummy,
                                                                       HyScanSonarGeneratorSignalType  signal,
                                                                       gdouble                         power);

gboolean                 hyscan_dummy_device_check_generator_extended (HyScanDummyDevice              *dummy,
                                                                       HyScanSonarGeneratorSignalType  signal,
                                                                       gdouble                         duration,
                                                                       gdouble                         power);

gboolean                 hyscan_dummy_device_check_tvg_auto           (HyScanDummyDevice              *dummy,
                                                                       gdouble                         level,
                                                                       gdouble                         sensitivity);

gboolean                 hyscan_dummy_device_check_tvg_points         (HyScanDummyDevice              *dummy,
                                                                       gdouble                         time_step,
                                                                       const gdouble                  *gains,
                                                                       guint32                         n_gains);

gboolean                 hyscan_dummy_device_check_tvg_constant       (HyScanDummyDevice              *dummy,
                                                                       gdouble                         gain);

gboolean                 hyscan_dummy_device_check_tvg_linear_db      (HyScanDummyDevice              *dummy,
                                                                       gdouble                         gain0,
                                                                       gdouble                         gain_step);

gboolean                 hyscan_dummy_device_check_tvg_logarithmic    (HyScanDummyDevice              *dummy,
                                                                       gdouble                         gain0,
                                                                       gdouble                         beta,
                                                                       gdouble                         alpha);

gboolean                 hyscan_dummy_device_check_software_ping      (HyScanDummyDevice              *dummy);

gboolean                 hyscan_dummy_device_check_start              (HyScanDummyDevice              *dummy,
                                                                       const gchar                    *project_name,
                                                                       const gchar                    *track_name,
                                                                       HyScanTrackType                 track_type);

gboolean                 hyscan_dummy_device_check_stop               (HyScanDummyDevice              *dummy);

gboolean                 hyscan_dummy_device_check_sync               (HyScanDummyDevice              *dummy);

gboolean                 hyscan_dummy_device_check_ping               (HyScanDummyDevice              *dummy);

gboolean                 hyscan_dummy_device_check_sensor_enable      (HyScanDummyDevice              *dummy,
                                                                       const gchar                    *sensor);

gboolean                 hyscan_dummy_device_check_params             (HyScanDummyDevice              *dummy,
                                                                       gint32                          info_id,
                                                                       gint32                          param_id,
                                                                       gint32                          system_id,
                                                                       gint32                          state_id);

HyScanDummyDeviceType    hyscan_dummy_device_get_type_by_sensor       (const gchar                    *sensor);

HyScanDummyDeviceType    hyscan_dummy_device_get_type_by_source       (HyScanSourceType                source);

HyScanAntennaPosition *  hyscan_dummy_device_get_sensor_position      (const gchar                    *sensor);

HyScanAntennaPosition *  hyscan_dummy_device_get_source_position      (HyScanSourceType                source);

HyScanSensorInfoSensor * hyscan_dummy_device_get_sensor_info          (const gchar                    *sensor);

HyScanSonarInfoSource *  hyscan_dummy_device_get_source_info          (HyScanSourceType                source);

HyScanAcousticDataInfo * hyscan_dummy_device_get_acoustic_info        (HyScanSourceType                source);

gchar *                  hyscan_dummy_device_get_sensor_data          (const gchar                    *sensor,
                                                                       gint64                         *time);

HyScanComplexFloat *     hyscan_dummy_device_get_complex_float_data (HyScanSourceType                source,
                                                                       guint32                        *n_points,
                                                                       gint64                         *time);

gfloat *                 hyscan_dummy_device_get_float_data           (HyScanSourceType                source,
                                                                       guint32                        *n_points,
                                                                       gint64                         *time);

G_END_DECLS

#endif /* __HYSCAN_DUMMY_DEVICE_H__ */
