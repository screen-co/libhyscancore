/* control-test.c
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

#include <hyscan-control-proxy.h>
#include <hyscan-acoustic-data.h>
#include <hyscan-nmea-data.h>

#include "hyscan-dummy-device.h"

#include <string.h>

#define PROJECT_NAME   "test"
#define TRACK_NAME     "test"

#define LUCKY_SENSOR   "nmea-1"
#define LUCKY_SOURCE   HYSCAN_SOURCE_PROFILER

#define OPERATOR_NAME  "Operator name"

gchar *db_uri = NULL;
gchar *project_name = NULL;
gchar *track_name = NULL;
gchar *schema_file = NULL;

HyScanDB *db;
HyScanDummyDevice *device1;
HyScanDummyDevice *device2;
HyScanControlProxy *proxy;
HyScanControl *control1;
HyScanControl *control2;
HyScanControl *control;
HyScanDevice *device;
HyScanSensor *sensor;
HyScanSonar *sonar;
HyScanParam *param;

const gchar *orig_dev_id;

HyScanDummyDevice *
get_sensor_device (const gchar *sensor)
{
  HyScanDummyDeviceType device_type;

  device_type = hyscan_dummy_device_get_type_by_sensor (sensor);
  if (device_type == HYSCAN_DUMMY_DEVICE_SIDE_SCAN)
    return device1;
  if (device_type == HYSCAN_DUMMY_DEVICE_PROFILER)
    return device2;

  return NULL;
}

HyScanDummyDevice *
get_sonar_device (HyScanSourceType source)
{
  HyScanDummyDeviceType device_type;

  device_type = hyscan_dummy_device_get_type_by_source (source);
  if (device_type == HYSCAN_DUMMY_DEVICE_SIDE_SCAN)
    return device1;
  if (device_type == HYSCAN_DUMMY_DEVICE_PROFILER)
    return device2;

  return NULL;
}

void
verify_sensor (const HyScanSensorInfoSensor *sensor1,
               const HyScanSensorInfoSensor *sensor2)
{
  if (g_strcmp0 (sensor1->name, sensor2->name) != 0)
    g_error ("name failed");
  if (g_strcmp0 (sensor1->dev_id, sensor2->dev_id) != 0)
    g_error ("dev-id failed");
  if (g_strcmp0 (sensor1->description, sensor2->description) != 0)
    g_error ("decripton failed");

  if ((sensor1->offset != NULL) ||
      (sensor2->offset != NULL))
    {
      if ((sensor1->offset->starboard != sensor2->offset->starboard) ||
          (sensor1->offset->forward != sensor2->offset->forward) ||
          (sensor1->offset->vertical != sensor2->offset->vertical) ||
          (sensor1->offset->yaw != sensor2->offset->yaw) ||
          (sensor1->offset->pitch != sensor2->offset->pitch) ||
          (sensor1->offset->roll != sensor2->offset->roll))
        {
          g_error ("offset failed");
        }
    }
}

void
verify_source (const HyScanSonarInfoSource *source1,
               const HyScanSonarInfoSource *source2)
{
  if ((source1 == NULL) || (source2 == NULL))
    g_error ("failed");

  /* Тип источника данных. */
  if (source1->source != source2->source)
    g_error ("source failed");

  /* Уникальный идентификатор устройства. */
  if (g_strcmp0 (source1->dev_id, source2->dev_id) != 0)
    g_error ("dev-id failed");

  /* Описание источника данных. */
  if (g_strcmp0 (source1->description, source2->description) != 0)
    g_error ("description failed");

  /* Смещение антенн по умолчанию. */
  if ((source1->offset != NULL) ||
      (source2->offset != NULL))
    {
      if ((source1->offset->starboard != source2->offset->starboard) ||
          (source1->offset->forward != source2->offset->forward) ||
          (source1->offset->vertical != source2->offset->vertical) ||
          (source1->offset->yaw != source2->offset->yaw) ||
          (source1->offset->pitch != source2->offset->pitch) ||
          (source1->offset->roll != source2->offset->roll))
        {
          g_error ("offset failed");
        }
    }

  /* Параметры приёмника. */
  if ((source1->receiver != NULL) ||
      (source2->receiver != NULL))
    {
      if ((source1->receiver->capabilities != source2->receiver->capabilities) ||
          (source1->receiver->min_time != source2->receiver->min_time) ||
          (source1->receiver->max_time != source2->receiver->max_time))
        {
          g_error ("receiver failed");
        }
    }

  /* Параметры генератора. */
  if ((source1->presets != NULL) ||
      (source2->presets != NULL))
    {
      GList *presets1 = source1->presets;

      /* Преднастройки генератора. */
      while (presets1 != NULL)
        {
          HyScanDataSchemaEnumValue *preset1 = presets1->data;
          GList *presets2 = source2->presets;
          gboolean status = FALSE;

          while (presets2 != NULL)
            {
              HyScanDataSchemaEnumValue *preset2 = presets2->data;

              if ((preset1->value == preset2->value) &&
                  (g_strcmp0 (preset1->name, preset2->name) == 0) &&
                  (g_strcmp0 (preset1->description, preset2->description) == 0))
                {
                  if (status)
                    g_error ("%s dup", preset1->name);

                  status = TRUE;
                }

              presets2 = g_list_next (presets2);
            }
          if (!status)
            g_error ("%s failed", preset1->name);

          presets1 = g_list_next (presets1);
        }
    }

  /* Параметры ВАРУ. */
  if ((source1->tvg != NULL) ||
      (source2->tvg != NULL))
    {
      if ((source1->tvg->capabilities != source2->tvg->capabilities) ||
          (source1->tvg->min_gain != source2->tvg->min_gain) ||
          (source1->tvg->max_gain != source2->tvg->max_gain) ||
          (source1->tvg->decrease != source2->tvg->decrease))
        {
          g_error ("tvg failed");
        }
    }
}

void
verify_offset (const HyScanAntennaOffset *offset1,
               const HyScanAntennaOffset *offset2)
{
  if ((offset1->starboard != offset2->starboard) ||
      (offset1->forward != offset2->forward) ||
      (offset1->vertical != offset2->vertical) ||
      (offset1->yaw != offset2->yaw) ||
      (offset1->pitch != offset2->pitch) ||
      (offset1->roll != offset2->roll))
    {
      g_error ("offset failed");
    }
}

void
verify_acoustic_info (const HyScanAcousticDataInfo *info1,
                      const HyScanAcousticDataInfo *info2)
{
  if ((info1->data_type != info2->data_type) ||
      (info1->data_rate != info2->data_rate) ||
      (info1->signal_frequency != info2->signal_frequency) ||
      (info1->signal_bandwidth != info2->signal_bandwidth) ||
      (info1->signal_heterodyne != info2->signal_heterodyne) ||
      (info1->antenna_voffset != info2->antenna_voffset) ||
      (info1->antenna_hoffset != info2->antenna_hoffset) ||
      (info1->antenna_vaperture != info2->antenna_vaperture) ||
      (info1->antenna_haperture != info2->antenna_haperture) ||
      (info1->antenna_frequency != info2->antenna_frequency) ||
      (info1->antenna_bandwidth != info2->antenna_bandwidth) ||
      (info1->antenna_group != info2->antenna_group) ||
      (info1->adc_vref != info2->adc_vref) ||
      (info1->adc_offset != info2->adc_offset))
    {
      g_error ("acoustic data info failed");
    }
}

void
device_state_cb (HyScanControl *control,
                 const gchar   *dev_id)
{
  if (g_strcmp0 (orig_dev_id, dev_id) != 0)
    g_error ("failed cb: %s, %s", dev_id, orig_dev_id);

  orig_dev_id = NULL;
}

void
check_params (HyScanDummyDevice *device)
{
  HyScanDataSchema *schema = hyscan_param_schema (param);
  HyScanParamList *list = hyscan_param_list_new ();

  GVariant *vvalue;
  gint32 value;

  const gchar *prefix;
  gchar *info_key;
  gchar *param_key;
  gchar *system_key;
  gint64 info_id;

  prefix = hyscan_dummy_device_get_id (device);
  info_key = g_strdup_printf ("/info/%s/id", prefix);
  param_key = g_strdup_printf ("/params/%s/id", prefix);
  system_key = g_strdup_printf ("/system/%s/id", prefix);
  value = g_random_int_range (0, 1024);

  vvalue = hyscan_data_schema_key_get_default (schema, info_key);
  info_id = g_variant_get_int64 (vvalue);
  g_variant_unref (vvalue);

  hyscan_param_list_add (list, "/unknown/id");
  if (hyscan_param_set (param, list))
    g_error ("incorect set call processed");
  if (hyscan_param_get (param, list))
    g_error ("incorect get call processed");

  hyscan_param_list_clear (list);
  hyscan_param_list_set_integer (list, param_key, value + 1);
  hyscan_param_list_set_integer (list, system_key, value + 2);
  if (!hyscan_param_set (param, list))
    g_error ("set call failed");

  if (!hyscan_dummy_device_check_params (device, info_id, value + 1, value + 2))
    g_error ("param failed");

  hyscan_param_list_clear (list);
  hyscan_param_list_add (list, param_key);
  hyscan_param_list_add (list, system_key);
  if (!hyscan_param_get (param, list))
    g_error ("get2 call failed");

  if ((hyscan_param_list_get_integer (list, param_key) != value + 1) ||
      (hyscan_param_list_get_integer (list, system_key) != value + 2))
    {
      g_error ("param2 failed");
    }

  g_free (info_key);
  g_free (param_key);
  g_free (system_key);

  g_object_unref (schema);
  g_object_unref (list);
}

void
check_sensors (void)
{
  const gchar * orig_sensors[] =
    {
      "nmea-1", "nmea-2",
      "nmea-3", "nmea-4",
      NULL
    };

  const gchar * const *sensors;
  guint32 n_sensors;
  guint32 i, j;

  /* Число датчиков. */
  sensors = hyscan_control_sensors_list (control);
  n_sensors = g_strv_length ((gchar**)sensors);
  if (n_sensors != ((sizeof (orig_sensors) / sizeof (gchar*)) - 1))
    g_error ("n_sensors mismatch");

  for (i = 0; i < n_sensors; i++)
    {
      const HyScanSensorInfoSensor *info;
      HyScanSensorInfoSensor *orig_info;

      orig_info = hyscan_dummy_device_get_sensor_info (orig_sensors[i]);

      /* Проверяем список датчиков. */
      sensors = hyscan_control_sensors_list (control);
      for (j = 0; j < n_sensors; j++)
        if (g_strcmp0 (orig_sensors[i], sensors[j]) == 0)
          break;
      if (j == n_sensors)
        g_error ("sensors list failed");

      /* Для LUCKY_SENSOR задано смещение антенны по умолчанию. */
      g_message ("Check sensor %s", sensors[i]);
      info = hyscan_control_sensor_get_info (control, orig_sensors[i]);
      if (g_strcmp0 (orig_sensors[i], LUCKY_SENSOR) == 0)
        orig_info->offset = hyscan_dummy_device_get_sensor_offset (orig_sensors[i]);
      verify_sensor (orig_info, info);

      hyscan_sensor_info_sensor_free (orig_info);
    }
}

void
check_sources (void)
{
  HyScanSourceType orig_sources[] =
  {
    HYSCAN_SOURCE_SIDE_SCAN_PORT,
    HYSCAN_SOURCE_SIDE_SCAN_STARBOARD,
    HYSCAN_SOURCE_PROFILER,
    HYSCAN_SOURCE_PROFILER_ECHO
  };

  const HyScanSourceType *sources;
  guint32 n_sources;
  guint32 i, j;

  /* Число источников данных. */
  sources = hyscan_control_sources_list (control, &n_sources);
  if (n_sources != (sizeof (orig_sources) / sizeof (HyScanSourceType)))
    g_error ("n_sources mismatch");

  for (i = 0; i < n_sources; i++)
    {
      const HyScanSonarInfoSource *info;
      HyScanSonarInfoSource *orig_info;

      orig_info = hyscan_dummy_device_get_source_info (orig_sources[i]);

      /* Проверяем список источников данных. */
      sources = hyscan_control_sources_list (control, &n_sources);
      for (j = 0; j < n_sources; j++)
        if (orig_sources[i] == sources[j])
          break;
      if (j == n_sources)
        g_error ("sources list failed");

      /* Для LUCKY_SOURCE добавлено смещение антенны. */
      g_message ("Check source %s", hyscan_source_get_id_by_type (orig_sources[i]));
      info = hyscan_control_source_get_info (control, orig_sources[i]);
      if (orig_sources[i] == LUCKY_SOURCE)
        orig_info->offset = hyscan_dummy_device_get_source_offset (orig_sources[i]);
      verify_source (orig_info, info);

      hyscan_sonar_info_source_free (orig_info);
    }
}

void
check_state_signal (void)
{
  const gchar * const *devices;
  guint32 n_devices;
  guint i;

  devices = hyscan_control_devices_list (control);
  n_devices = g_strv_length ((gchar**)devices);

  /* Проверяем список устройств. */
  if ((devices == NULL) || (n_devices == 0))
    g_error ("dev-id number mismatch");

  for (i = 0; i < n_devices; i++)
    {
      gboolean used = FALSE;
      const gchar *dev_id;

      dev_id = hyscan_dummy_device_get_id (device1);
      if (g_strcmp0 (devices[i], dev_id) == 0)
        used = TRUE;

      dev_id = hyscan_dummy_device_get_id (device2);
      if (g_strcmp0 (devices[i], dev_id) == 0)
        used = TRUE;

      if (!used)
        g_error ("unknown dev-id %s", devices[i]);
    }

  /* Проверяем что устройства находятся в отключенном состоянии. */
  for (i = 0; i < n_devices; i++)
    {
      if (hyscan_control_device_get_status (control, devices[i]) != HYSCAN_DEVICE_STATUS_ERROR)
        g_error ("%s activated", devices[i]);
    }

  /* Проверяем прохождение сигнала "device-state". */
  orig_dev_id = hyscan_dummy_device_get_id (device1);
  hyscan_dummy_device_change_state (device1);
  if (orig_dev_id != NULL)
    g_error ("failed %s", hyscan_dummy_device_get_id (device1));

  orig_dev_id = hyscan_dummy_device_get_id (device2);
  hyscan_dummy_device_change_state (device2);
  if (orig_dev_id != NULL)
    g_error ("failed %s", hyscan_dummy_device_get_id (device2));

  /* Проверяем что устройства находятся в активном состоянии. */
  for (i = 0; i < n_devices; i++)
    {
      if (hyscan_control_device_get_status (control, devices[i]) != HYSCAN_DEVICE_STATUS_OK)
        g_error ("%s isn't activated", devices[i]);
    }
}

void
check_device_set_sound_velocity (void)
{
  HyScanSoundVelocity orig_value[2];
  GList *orig_svp = NULL;

  orig_value[0].depth = 1.0;
  orig_value[0].velocity = 1.0;
  orig_value[1].depth = 2.0;
  orig_value[1].velocity = 2.0;
  orig_svp = g_list_append (orig_svp, &orig_value[0]);
  orig_svp = g_list_append (orig_svp, &orig_value[1]);

  if (!hyscan_device_set_sound_velocity (device, orig_svp))
    g_error ("call failed");

  if (!hyscan_dummy_device_check_sound_velocity (device1, orig_svp) ||
      !hyscan_dummy_device_check_sound_velocity (device2, orig_svp))
    {
      g_error ("param failed");
    }

  g_list_free (orig_svp);
}

void
check_device_disconnect (void)
{
  if (!hyscan_device_disconnect (device))
    g_error ("call failed");

  if (!hyscan_dummy_device_check_disconnect (device1) ||
      !hyscan_dummy_device_check_disconnect (device2))
    {
      g_error ("param failed");
    }
}

void
check_sensor_set_enable (const gchar *sensor_name)
{
  HyScanDummyDevice *device = get_sensor_device (sensor_name);

  if (!hyscan_sensor_set_enable (sensor, sensor_name, TRUE))
    g_error ("call failed");

  if (!hyscan_dummy_device_check_sensor_enable (device, sensor_name))
    g_error ("param failed");
}

void
check_sonar_receiver_set_time (HyScanSourceType source)
{
  HyScanDummyDevice *device = get_sonar_device (source);
  gdouble receive_time = g_random_double ();
  gdouble wait_time = g_random_double ();

  if (!hyscan_sonar_receiver_set_time (sonar, source, receive_time, wait_time))
    g_error ("call failed");

  if (!hyscan_dummy_device_check_receiver_time (device, receive_time, wait_time))
    g_error ("param failed");
}

void
check_sonar_receiver_set_auto (HyScanSourceType source)
{
  HyScanDummyDevice *device = get_sonar_device (source);

  if (!hyscan_sonar_receiver_set_auto (sonar, source))
    g_error ("call failed");

  if (!hyscan_dummy_device_check_receiver_auto (device))
    g_error ("param failed");
}

void
check_sonar_receiver_disable (HyScanSourceType source)
{
  HyScanDummyDevice *device = get_sonar_device (source);

  if (!hyscan_sonar_receiver_disable (sonar, source))
    g_error ("call failed");

  if (!hyscan_dummy_device_check_receiver_disable (device))
    g_error ("param failed");
}

void
check_sonar_generator_set_preset (HyScanSourceType source)
{
  HyScanDummyDevice *device = get_sonar_device (source);
  guint preset = g_random_int ();

  if (!hyscan_sonar_generator_set_preset (sonar, source, preset))
    g_error ("call failed");

  if (!hyscan_dummy_device_check_generator_preset (device, preset))
    g_error ("param failed");
}

void
check_sonar_generator_disable (HyScanSourceType source)
{
  HyScanDummyDevice *device = get_sonar_device (source);

  if (!hyscan_sonar_generator_disable (sonar, source))
    g_error ("call failed");

  if (!hyscan_dummy_device_check_generator_disable (device))
    g_error ("param failed");
}

void
check_sonar_tvg_set_auto (HyScanSourceType source)
{
  HyScanDummyDevice *device = get_sonar_device (source);
  gdouble level = g_random_double ();
  gdouble sensitivity = g_random_double ();

  if (!hyscan_sonar_tvg_set_auto (sonar, source, level, sensitivity))
    g_error ("call failed");

  if (!hyscan_dummy_device_check_tvg_auto (device, level, sensitivity))
    g_error ("param failed");
}

void
check_sonar_tvg_set_constant (HyScanSourceType source)
{
  HyScanDummyDevice *device = get_sonar_device (source);
  gdouble gain = g_random_double ();

  if (!hyscan_sonar_tvg_set_constant (sonar, source, gain))
    g_error ("call failed");

  if (!hyscan_dummy_device_check_tvg_constant (device, gain))
    g_error ("param failed");
}

void
check_sonar_tvg_set_linear_db (HyScanSourceType source)
{
  HyScanDummyDevice *device = get_sonar_device (source);
  gdouble gain0 = g_random_double ();
  gdouble gain_step = g_random_double ();

  if (!hyscan_sonar_tvg_set_linear_db (sonar, source, gain0, gain_step))
    g_error ("call failed");

  if (!hyscan_dummy_device_check_tvg_linear_db (device, gain0, gain_step))
    g_error ("param failed");
}

void
check_sonar_tvg_set_logarithmic (HyScanSourceType source)
{
  HyScanDummyDevice *device = get_sonar_device (source);
  gdouble gain0 = g_random_double ();
  gdouble beta = g_random_double ();
  gdouble alpha = g_random_double ();

  if (!hyscan_sonar_tvg_set_logarithmic (sonar, source, gain0, beta, alpha))
    g_error ("call failed");

  if (!hyscan_dummy_device_check_tvg_logarithmic (device, gain0, beta, alpha))
    g_error ("param failed");
}

void
check_sonar_tvg_disable (HyScanSourceType source)
{
  HyScanDummyDevice *device = get_sonar_device (source);

  if (!hyscan_sonar_tvg_disable (sonar, source))
    g_error ("call failed");

  if (!hyscan_dummy_device_check_tvg_disable (device))
    g_error ("param failed");
}

void
check_sonar_start (void)
{
  HyScanTrackType track_type = HYSCAN_TRACK_SURVEY;

  if (!hyscan_sonar_start (sonar, project_name, track_name, track_type, NULL))
    g_error ("call failed");

  if (!hyscan_dummy_device_check_start (device1, project_name, track_name, track_type) ||
      !hyscan_dummy_device_check_start (device2, project_name, track_name, track_type))
    {
      g_error ("param_failed");
    }
}

void
check_sonar_stop (void)
{
  if (!hyscan_sonar_stop (sonar))
    g_error ("call failed");

  if (!hyscan_dummy_device_check_stop (device1) ||
      !hyscan_dummy_device_check_stop (device2))
    {
      g_error ("param failed");
    }
}

void
check_sonar_sync (void)
{
  if (!hyscan_sonar_sync (sonar))
    g_error ("call failed");

  if (!hyscan_dummy_device_check_sync (device1) ||
      !hyscan_dummy_device_check_sync (device2))
    {
      g_error ("param failed");
    }
}

void
check_sensor_data (const gchar *sensor)
{
  HyScanNMEAData *nmea;

  HyScanAntennaOffset  offset;
  HyScanAntennaOffset *orig_offset;

  gchar *orig_data;
  const gchar *data;
  gint64 orig_time;
  gint64 time;

  /* Проверочные данные. */
  orig_offset = hyscan_dummy_device_get_sensor_offset (sensor);

  /* открываем канал данных. */
  nmea = hyscan_nmea_data_new_sensor (db, NULL, project_name, track_name, sensor);
  if (nmea == NULL)
    g_error ("can't open nmea channel for sensor %s", sensor);

  /* Проверяем смещение антенны. */
  offset = hyscan_nmea_data_get_offset (nmea);
  verify_offset (orig_offset, &offset);

  /* Проверяем данные. */
  orig_data = hyscan_dummy_device_get_sensor_data (sensor, &orig_time);
  data = hyscan_nmea_data_get (nmea, 0, &time);
  if ((g_strcmp0 (orig_data, data) != 0) ||
      (orig_time != time))
    {
      g_error ("sensor %s data error", sensor);
    }

  hyscan_antenna_offset_free (orig_offset);
  g_free (orig_data);

  g_object_unref (nmea);
}

void
check_sonar_data (HyScanSourceType source)
{
  HyScanAcousticData *reader;

  HyScanAntennaOffset  offset;
  HyScanAntennaOffset *orig_offset;

  HyScanAcousticDataInfo orig_info;
  HyScanAcousticDataInfo info;

  HyScanComplexFloat *orig_cdata;
  const HyScanComplexFloat *cdata;
  gfloat *orig_fdata;
  const gfloat *fdata;
  gint64 orig_time;
  gint64 time;
  guint32 orig_n_points;
  guint32 n_points;

  /* Проверочные данные. */
  orig_offset = hyscan_dummy_device_get_source_offset (source);
  orig_info = hyscan_dummy_device_get_acoustic_info (source);

  /* Открываем канал данных. */
  reader = hyscan_acoustic_data_new (db, NULL, project_name, track_name, source, 1, FALSE);

  if (reader == NULL)
    g_error ("can't open %s data", hyscan_source_get_id_by_type (source));

  /* Проверяем смещение антенны. */
  offset = hyscan_acoustic_data_get_offset (reader);
  verify_offset (orig_offset, &offset);

  /* Проверяем параметры каналов данных. */
  info = hyscan_acoustic_data_get_info (reader);
  verify_acoustic_info (&orig_info, &info);

  orig_cdata = hyscan_dummy_device_get_complex_float_data (source, &orig_n_points, &orig_time);
  orig_fdata = hyscan_dummy_device_get_float_data (source, &orig_n_points, &orig_time);

  /* Проверяем образ сигнала. */
  cdata = hyscan_acoustic_data_get_signal (reader, 0, &n_points, &time);
  if ((orig_time != time) ||
      (orig_n_points != n_points) ||
      (memcmp (orig_cdata, cdata, n_points * sizeof (HyScanComplexFloat)) != 0))
    {
      g_error ("%s signal error", hyscan_source_get_id_by_type (source));
    }

  /* Проверяем ВАРУ. */
  fdata = hyscan_acoustic_data_get_tvg (reader, 0, &n_points, &time);
  if ((orig_time != time) ||
      (orig_n_points != n_points) ||
      (memcmp (orig_fdata, fdata, n_points * sizeof (gfloat)) != 0))
    {
      g_error ("%s tvg error", hyscan_source_get_id_by_type (source));
    }

  /* Проверяем данные. */
  hyscan_acoustic_data_set_convolve (reader, FALSE, 1.0);
  cdata = hyscan_acoustic_data_get_complex (reader, 0, &n_points, &time);
  if ((orig_time != time) ||
      (orig_n_points != n_points) ||
      (memcmp (orig_cdata, cdata, n_points * sizeof (HyScanComplexFloat)) != 0))
    {
      g_error ("%s data error", hyscan_source_get_id_by_type (source));
    }

  hyscan_antenna_offset_free (orig_offset);

  g_free (orig_cdata);
  g_free (orig_fdata);

  g_object_unref (reader);
}

int
main (int    argc,
      char **argv)
{
  HyScanAntennaOffset *offset;
  const HyScanSourceType *sources;
  const gchar * const *sensors;
  guint n_sources;
  guint i;

  /* Параметры командной строки. */
  {
    gchar **args;
    GError *error = NULL;
    GOptionContext *context;
    GOptionEntry entries[] =
      {
        { "project", 'p', 0, G_OPTION_ARG_STRING, &project_name, "Project name", NULL },
        { "track", 't', 0, G_OPTION_ARG_STRING, &track_name, "Track name", NULL },
        { "dump-schema", 'd', 0, G_OPTION_ARG_STRING, &schema_file, "Dump device schema to file", NULL },
        { NULL }
      };

#ifdef G_OS_WIN32
    args = g_win32_get_command_line ();
#else
    args = g_strdupv (argv);
#endif

    context = g_option_context_new ("<db-uri>");
    g_option_context_set_help_enabled (context, TRUE);
    g_option_context_add_main_entries (context, entries, NULL);
    g_option_context_set_ignore_unknown_options (context, FALSE);
    if (!g_option_context_parse_strv (context, &args, &error))
      {
        g_print ("%s\n", error->message);
        return -1;
      }

    if ((g_strv_length (args) != 2))
      {
        g_print ("%s", g_option_context_get_help (context, FALSE, NULL));
        return 0;
      }

    g_option_context_free (context);

    if (project_name == NULL)
      project_name = g_strdup (PROJECT_NAME);
    if (track_name == NULL)
      track_name = g_strdup (TRACK_NAME);
    db_uri = g_strdup (args[1]);

    g_strfreev (args);
  }

  /* Открываем базу данных. */
  db = hyscan_db_new (db_uri);
  if (db == NULL)
    g_error ("can't open db at: %s", db_uri);

  /* Виртуальные устройства. */
  device1 = hyscan_dummy_device_new (HYSCAN_DUMMY_DEVICE_SIDE_SCAN);
  device2 = hyscan_dummy_device_new (HYSCAN_DUMMY_DEVICE_PROFILER);

  /* Объекты управления. */
  control1 = hyscan_control_new ();
  control2 = hyscan_control_new ();

  /* Добавляем устройства в объекты управления. */
  hyscan_control_device_add (control1, HYSCAN_DEVICE (device1));
  hyscan_control_device_add (control2, HYSCAN_DEVICE (device2));

  /* Смещение антенн по умолчанию. */
  offset = hyscan_dummy_device_get_sensor_offset (LUCKY_SENSOR);
  hyscan_control_sensor_set_default_offset (control1, LUCKY_SENSOR, offset);
  hyscan_antenna_offset_free (offset);

  offset = hyscan_dummy_device_get_source_offset (LUCKY_SOURCE);
  hyscan_control_source_set_default_offset (control2, LUCKY_SOURCE, offset);
  hyscan_antenna_offset_free (offset);

  /* Завершаем конфигурирование устройств. */
  hyscan_control_device_bind (control1);
  hyscan_control_device_bind (control2);

  /* Составноное устройство. */
  control = hyscan_control_new ();
  hyscan_control_device_add (control, HYSCAN_DEVICE (control2));
  hyscan_control_device_add (control, HYSCAN_DEVICE (control1));
  hyscan_control_device_bind (control);

  /* Параметры записи данных. */
  hyscan_control_writer_set_db (control, db);
  hyscan_control_writer_set_operator_name (control, OPERATOR_NAME);

  /* Прокси устройство. */
  proxy = hyscan_control_proxy_new (control, NULL);
  g_object_unref (control);
  control = hyscan_control_new ();
  hyscan_control_device_add (control, HYSCAN_DEVICE (proxy));
  hyscan_control_device_bind (control);

  device = HYSCAN_DEVICE (control);
  sensor = HYSCAN_SENSOR (control);
  sonar = HYSCAN_SONAR (control);
  param = HYSCAN_PARAM (control);

  /* Обработчик сигнала device-state. */
  g_signal_connect (control, "device-state", G_CALLBACK (device_state_cb), NULL);

  /* Схема устройства. */
  if (schema_file != NULL)
    {
      HyScanDataSchema *schema = hyscan_param_schema (param);
      const gchar *data = hyscan_data_schema_get_data (schema);

      g_file_set_contents (schema_file, data, -1, NULL);
      g_object_unref (schema);
    }

  /* Список датчиков и источников данных. */
  sensors = hyscan_control_sensors_list (control);
  sources = hyscan_control_sources_list (control, &n_sources);

  /* Смещение антенн источников данных. */
  g_message ("Check hyscan_sonar_antenna_set_offset");
  for (i = 0; i < n_sources; i++)
    {
      const HyScanSonarInfoSource *info;
      HyScanDummyDevice *device;

      info = hyscan_control_source_get_info (control, sources[i]);
      if (info->offset != NULL)
        continue;

      offset = hyscan_dummy_device_get_source_offset (sources[i]);
      hyscan_sonar_antenna_set_offset (sonar, sources[i], offset);

      device = get_sonar_device (sources[i]);
      if (!hyscan_dummy_device_check_antenna_offset (device, offset))
        g_error ("%s: offset failed", hyscan_source_get_id_by_type (sources[i]));

      hyscan_antenna_offset_free (offset);
    }

  /* Смещение антенн датчиков. */
  g_message ("Check hyscan_sensor_antenna_set_offset");
  for (i = 0; sensors[i] != NULL; i++)
    {
      const HyScanSensorInfoSensor *info;
      HyScanDummyDevice *device;

      info = hyscan_control_sensor_get_info (control, sensors[i]);
      if (info->offset != NULL)
        continue;

      offset = hyscan_dummy_device_get_sensor_offset (sensors[i]);
      hyscan_sensor_antenna_set_offset (sensor, sensors[i], offset);

      device = get_sensor_device (sensors[i]);
      if (!hyscan_dummy_device_check_antenna_offset (device, offset))
        g_error ("%s: offset failed", sensors[i]);

      hyscan_antenna_offset_free (offset);
    }

  /* Проверка информации о датчиках. */
  g_message ("Check sensors info");
  check_sensors ();

  /* Проверка информации об источниках данных. */
  g_message ("Check sources info");
  check_sources ();

  /* Проверка интерфейса HyScanParam. */
  g_message ("Check hyscan_param");
  check_params (device1);
  check_params (device2);

  /* Проверка сигнала device-state. */
  g_message ("Check device-state signal");
  check_state_signal ();

  /* Проверка интерфейса HyScanDevice. */
  g_message ("Check hyscan_device_set_sound_velocity");
  check_device_set_sound_velocity ();

  /* Проверка интерфейса HyScanSensor. */
  g_message ("Check hyscan_sensor_set_enable");
  for (i = 0; sensors[i] != NULL; i++)
    check_sensor_set_enable (sensors[i]);

  /* Проверка интерфейса HyScanSonar. */
  g_message ("Check hyscan_sonar_receiver_set_time");
  for (i = 0; i < n_sources; i++)
    check_sonar_receiver_set_time (sources[i]);

  g_message ("Check hyscan_sonar_receiver_set_auto");
  for (i = 0; i < n_sources; i++)
    check_sonar_receiver_set_auto (sources[i]);

  g_message ("Check hyscan_sonar_receiver_disable");
  for (i = 0; i < n_sources; i++)
    check_sonar_receiver_disable (sources[i]);

  g_message ("Check hyscan_sonar_generator_set_preset");
  for (i = 0; i < n_sources; i++)
    check_sonar_generator_set_preset (sources[i]);

  g_message ("Check hyscan_sonar_generator_disable");
  for (i = 0; i < n_sources; i++)
    check_sonar_generator_disable (sources[i]);

  g_message ("Check hyscan_sonar_tvg_set_auto");
  for (i = 0; i < n_sources; i++)
    check_sonar_tvg_set_auto (sources[i]);

  g_message ("Check hyscan_sonar_tvg_set_constant");
  for (i = 0; i < n_sources; i++)
    check_sonar_tvg_set_constant (sources[i]);

  g_message ("Check hyscan_sonar_tvg_set_linear_db");
  for (i = 0; i < n_sources; i++)
    check_sonar_tvg_set_linear_db (sources[i]);

  g_message ("Check hyscan_sonar_tvg_set_logarithmic");
  for (i = 0; i < n_sources; i++)
    check_sonar_tvg_set_logarithmic (sources[i]);

  g_message ("Check hyscan_sonar_tvg_disable");
  for (i = 0; i < n_sources; i++)
    check_sonar_tvg_disable (sources[i]);

  g_message ("Check hyscan_sonar_start");
  check_sonar_start ();
  hyscan_dummy_device_send_data (device1);
  hyscan_dummy_device_send_data (device2);

  g_message ("Check hyscan_sonar_stop");
  check_sonar_stop ();

  g_message ("Check hyscan_sonar_sync");
  check_sonar_sync ();

  g_message ("Check sensor data");
  for (i = 0; sensors[i] != NULL; i++)
    check_sensor_data (sensors[i]);

  g_message ("Check sonar data");
  for (i = 0; i < n_sources; i++)
    check_sonar_data (sources[i]);

  g_message ("Check hyscan_device_disconnect");
  check_device_disconnect ();

  hyscan_db_project_remove (db, project_name);

  g_object_unref (proxy);
  g_object_unref (control);
  g_object_unref (control1);
  g_object_unref (control2);
  g_object_unref (device1);
  g_object_unref (device2);
  g_object_unref (db);

  g_free (project_name);
  g_free (track_name);
  g_free (db_uri);
  g_free (schema_file);

  g_message ("All done");

  return 0;
}
