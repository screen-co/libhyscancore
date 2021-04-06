/* hyscan-dummy-device.c
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

/**
 * SECTION: hyscan-dummy-device
 * @Short_description: драйвер псевдо-устройства для тестирования HyScanControl
 * @Title: HyScanDummyDevice
 *
 * Класс реализует драйвер псевдо-устройства, которое эмулирует гидролокатор
 * бокового обзора или профилограф с эхолотом, а также встроенные датчики. Тип
 * эмулируемого устройства задаётся при создании объекта драйвера. От выбранного
 * типа зависит только список источников данных и датчиков.
 *
 * Драйвер реализует интерфейсы #HyScanSonar, #HyScanSensor, #HyScanActuator
 * и #HyScanParam. Он запоминает значения параметров для последней вызваной
 * функции этих интерфейсов, а также содержит функции проверяющие эти параметры.
 * Логика тестирования предполагает вызов каждой функции интерфейса и сравнение
 * полученных параметров с эталоном.
 *
 * Кроме этого драйвер осуществляет однократную отправку данных каждого типа
 * для всех источников данных и датчиков. Программа тестирования затем может
 * сравнить хаписанные данные с эталоном.
 *
 * Проверка интерфейса #HyScanParam осуществляется через реализуемые драйвером
 * параметры: /info/DEV-ID/id, /param/DEV-ID/id, /system/DEV-ID/id и
 * /state/DEV-ID/id. Где DEV-ID имеет значения "ss" - для ГБО и "pf" - для
 * профилографа.
 *
 * Дополнительно эмулируется параметр /state/DEV-ID/enable. Изначально
 * в этом параметре записано значение FALSE. При вызове функции
 * #hyscan_dummy_device_change_state в этот параметр записывается
 * значение TRUE и отправляется сигнал "device-state".
 */

#include "hyscan-dummy-device.h"

#include <hyscan-param.h>
#include <hyscan-device.h>
#include <hyscan-buffer.h>
#include <string.h>

#define N_PRESETS              16
#define N_POINTS               16

enum
{
  PROP_O,
  PROP_TYPE
};

typedef enum
{
  HYSCAN_DUMMY_DEVICE_COMMAND_INVALID,
  HYSCAN_DUMMY_DEVICE_COMMAND_SYNC,
  HYSCAN_DUMMY_DEVICE_COMMAND_SET_SOUND_VELOCITY,
  HYSCAN_DUMMY_DEVICE_COMMAND_DISCONNECT,
  HYSCAN_DUMMY_DEVICE_COMMAND_ANTENNA_SET_OFFSET,
  HYSCAN_DUMMY_DEVICE_COMMAND_RECEIVER_SET_TIME,
  HYSCAN_DUMMY_DEVICE_COMMAND_RECEIVER_SET_AUTO,
  HYSCAN_DUMMY_DEVICE_COMMAND_RECEIVER_DISABLE,
  HYSCAN_DUMMY_DEVICE_COMMAND_GENERATOR_SET_PRESET,
  HYSCAN_DUMMY_DEVICE_COMMAND_GENERATOR_DISABLE,
  HYSCAN_DUMMY_DEVICE_COMMAND_TVG_SET_AUTO,
  HYSCAN_DUMMY_DEVICE_COMMAND_TVG_SET_CONSTANT,
  HYSCAN_DUMMY_DEVICE_COMMAND_TVG_SET_LINEAR_DB,
  HYSCAN_DUMMY_DEVICE_COMMAND_TVG_SET_LOGARITHMIC,
  HYSCAN_DUMMY_DEVICE_COMMAND_TVG_DISABLE,
  HYSCAN_DUMMY_DEVICE_COMMAND_START,
  HYSCAN_DUMMY_DEVICE_COMMAND_STOP,
  HYSCAN_DUMMY_DEVICE_COMMAND_SENSOR_ENABLE,
  HYSCAN_DUMMY_DEVICE_COMMAND_ACTUATOR_DISABLE,
  HYSCAN_DUMMY_DEVICE_COMMAND_ACTUATOR_SCAN,
  HYSCAN_DUMMY_DEVICE_COMMAND_ACTUATOR_MANUAL
} HyScanDummyDeviceCommand;

struct _HyScanDummyDevicePrivate
{
  HyScanDummyDeviceType            type;
  HyScanDataSchema                *schema;
  const gchar                     *device_id;

  gboolean                         connected;

  GHashTable                      *params;

  HyScanDummyDeviceCommand         command;

  GList                           *svp;
  HyScanAntennaOffset              offset;

  gdouble                          receiver_time;
  gdouble                          wait_time;

  gint64                           generator_preset;

  gdouble                          tvg_level;
  gdouble                          tvg_sensitivity;
  gdouble                          tvg_gain0;
  gdouble                          tvg_step;
  gdouble                          tvg_alpha;
  gdouble                          tvg_beta;
  gdouble                          tvg_time_step;

  const gchar                     *project_name;
  const gchar                     *track_name;
  HyScanTrackType                  track_type;
  const HyScanTrackPlan           *track_plan;

  const gchar                     *sensor_name;

  const gchar                     *actuator_name;
  gdouble                          actuator_from;
  gdouble                          actuator_to;
  gdouble                          actuator_speed;
  gdouble                          actuator_angle;
};

static void        hyscan_dummy_device_param_interface_init     (HyScanParamInterface    *iface);
static void        hyscan_dummy_device_device_interface_init    (HyScanDeviceInterface   *iface);
static void        hyscan_dummy_device_sonar_interface_init     (HyScanSonarInterface    *iface);
static void        hyscan_dummy_device_sensor_interface_init    (HyScanSensorInterface   *iface);
static void        hyscan_dummy_device_actuator_interface_init  (HyScanActuatorInterface *iface);

static void        hyscan_dummy_device_set_property             (GObject                 *object,
                                                                 guint                    prop_id,
                                                                 const GValue            *value,
                                                                 GParamSpec              *pspec);

static void        hyscan_dummy_device_object_constructed       (GObject                 *object);
static void        hyscan_dummy_device_object_finalize          (GObject                 *object);

static const gchar *hyscan_dummy_device_sensors[] =
{
  "nmea-1", "nmea-2",
  "nmea-3", "nmea-4",
  NULL
};

static HyScanSourceType hyscan_dummy_device_sources[] =
{
  HYSCAN_SOURCE_SIDE_SCAN_PORT,
  HYSCAN_SOURCE_SIDE_SCAN_STARBOARD,
  HYSCAN_SOURCE_PROFILER,
  HYSCAN_SOURCE_PROFILER_ECHO,
  HYSCAN_SOURCE_INVALID
};

G_DEFINE_TYPE_WITH_CODE (HyScanDummyDevice, hyscan_dummy_device, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (HyScanDummyDevice)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_PARAM,    hyscan_dummy_device_param_interface_init)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_DEVICE,   hyscan_dummy_device_device_interface_init)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_SONAR,    hyscan_dummy_device_sonar_interface_init)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_SENSOR,   hyscan_dummy_device_sensor_interface_init)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_ACTUATOR, hyscan_dummy_device_actuator_interface_init))

static void
hyscan_dummy_device_class_init (HyScanDummyDeviceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_dummy_device_set_property;

  object_class->constructed = hyscan_dummy_device_object_constructed;
  object_class->finalize = hyscan_dummy_device_object_finalize;

  g_object_class_install_property (object_class, PROP_TYPE,
    g_param_spec_int ("type", "Type", "Dummy sonar type",
                      HYSCAN_DUMMY_DEVICE_SIDE_SCAN,
                      HYSCAN_DUMMY_DEVICE_PROFILER,
                      HYSCAN_DUMMY_DEVICE_SIDE_SCAN,
                      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_dummy_device_init (HyScanDummyDevice *dummy)
{
  dummy->priv = hyscan_dummy_device_get_instance_private (dummy);
}

static void
hyscan_dummy_device_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  HyScanDummyDevice *dummy = HYSCAN_DUMMY_DEVICE (object);
  HyScanDummyDevicePrivate *priv = dummy->priv;

  switch (prop_id)
    {
    case PROP_TYPE:
      priv->type = g_value_get_int (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_dummy_device_object_constructed (GObject *object)
{
  HyScanDummyDevice *dummy = HYSCAN_DUMMY_DEVICE (object);
  HyScanDummyDevicePrivate *priv = dummy->priv;

  HyScanDeviceSchema *device_schema;
  HyScanSensorSchema *sensor_schema;
  HyScanActuatorSchema *actuator_schema;
  HyScanSonarSchema *sonar_schema;
  HyScanDataSchemaBuilder *builder;

  gint32 uniq_value;
  gchar *key_id;

  device_schema = hyscan_device_schema_new (HYSCAN_DEVICE_SCHEMA_VERSION);
  sensor_schema = hyscan_sensor_schema_new (device_schema);
  sonar_schema = hyscan_sonar_schema_new (device_schema);
  actuator_schema = hyscan_actuator_schema_new (device_schema);
  builder = HYSCAN_DATA_SCHEMA_BUILDER (device_schema);

  /* ГБО. */
  if (priv->type == HYSCAN_DUMMY_DEVICE_SIDE_SCAN)
    {
      HyScanSensorInfoSensor *sensor_info;
      HyScanSonarInfoSource *source_info;
      HyScanActuatorInfoActuator *actuator_info;

      sensor_info = hyscan_dummy_device_get_sensor_info ("nmea-1");
      hyscan_sensor_schema_add_full (sensor_schema, sensor_info);
      hyscan_sensor_info_sensor_free (sensor_info);

      sensor_info = hyscan_dummy_device_get_sensor_info ("nmea-2");
      hyscan_sensor_schema_add_full (sensor_schema, sensor_info);
      hyscan_sensor_info_sensor_free (sensor_info);

      source_info = hyscan_dummy_device_get_source_info (HYSCAN_SOURCE_SIDE_SCAN_PORT);
      hyscan_sonar_schema_source_add_full (sonar_schema, source_info);
      hyscan_sonar_info_source_free (source_info);

      source_info = hyscan_dummy_device_get_source_info (HYSCAN_SOURCE_SIDE_SCAN_STARBOARD);
      hyscan_sonar_schema_source_add_full (sonar_schema, source_info);
      hyscan_sonar_info_source_free (source_info);

      actuator_info = hyscan_dummy_device_get_actuator_info ("actuator-1");
      hyscan_actuator_schema_add_full (actuator_schema, actuator_info);
      hyscan_actuator_info_actuator_free (actuator_info);

      priv->device_id = "ss";
    }

  /* Профилограф. */
  else if (priv->type == HYSCAN_DUMMY_DEVICE_PROFILER)
    {
      HyScanSensorInfoSensor *sensor_info;
      HyScanSonarInfoSource *source_info;
      HyScanActuatorInfoActuator *actuator_info;

      sensor_info = hyscan_dummy_device_get_sensor_info ("nmea-3");
      hyscan_sensor_schema_add_full (sensor_schema, sensor_info);
      hyscan_sensor_info_sensor_free (sensor_info);

      sensor_info = hyscan_dummy_device_get_sensor_info ("nmea-4");
      hyscan_sensor_schema_add_full (sensor_schema, sensor_info);
      hyscan_sensor_info_sensor_free (sensor_info);

      source_info = hyscan_dummy_device_get_source_info (HYSCAN_SOURCE_PROFILER);
      hyscan_sonar_schema_source_add_full (sonar_schema, source_info);
      hyscan_sonar_info_source_free (source_info);

      source_info = hyscan_dummy_device_get_source_info (HYSCAN_SOURCE_PROFILER_ECHO);
      hyscan_sonar_schema_source_add_full (sonar_schema, source_info);
      hyscan_sonar_info_source_free (source_info);

      actuator_info = hyscan_dummy_device_get_actuator_info ("actuator-2");
      hyscan_actuator_schema_add_full (actuator_schema, actuator_info);
      hyscan_actuator_info_actuator_free (actuator_info);

      priv->device_id = "pf";
    }

  priv->params = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  uniq_value = g_random_int_range (0, 1024);
  key_id = g_strdup_printf ("/info/%s/id", priv->device_id);
  hyscan_data_schema_builder_key_integer_create (builder, key_id, "id", NULL, uniq_value);
  g_hash_table_insert (priv->params, key_id, GINT_TO_POINTER (uniq_value));

  key_id = g_strdup_printf ("/params/%s/id", priv->device_id);
  hyscan_data_schema_builder_key_integer_create (builder, key_id, "id", NULL, 0);
  g_hash_table_insert (priv->params, key_id, NULL);

  key_id = g_strdup_printf ("/system/%s/id", priv->device_id);
  hyscan_data_schema_builder_key_integer_create (builder, key_id, "id", NULL, 0);
  g_hash_table_insert (priv->params, key_id, NULL);

  key_id = g_strdup_printf ("/state/%s/status", priv->device_id);
  hyscan_data_schema_builder_key_enum_create (builder, key_id, "status", NULL,
                                              HYSCAN_DEVICE_STATUS_ENUM, HYSCAN_DEVICE_STATUS_ERROR);
  g_hash_table_insert (priv->params, key_id, GINT_TO_POINTER (HYSCAN_DEVICE_STATUS_ERROR));

  priv->schema = hyscan_data_schema_builder_get_schema (builder);

  priv->connected = TRUE;

  g_object_unref (device_schema);
  g_object_unref (sensor_schema);
  g_object_unref (sonar_schema);
  g_object_unref (actuator_schema);
}

static void
hyscan_dummy_device_object_finalize (GObject *object)
{
  HyScanDummyDevice *dummy = HYSCAN_DUMMY_DEVICE (object);
  HyScanDummyDevicePrivate *priv = dummy->priv;

  if (priv->connected)
    g_error ("device %s still connected", priv->device_id);

  g_hash_table_unref (priv->params);
  g_object_unref (priv->schema);

  G_OBJECT_CLASS (hyscan_dummy_device_parent_class)->finalize (object);
}

static HyScanDataSchema *
hyscan_dummy_device_param_schema (HyScanParam *param)
{
  HyScanDummyDevice *dummy = HYSCAN_DUMMY_DEVICE (param);

  return g_object_ref (dummy->priv->schema);
}

static gboolean
hyscan_dummy_device_param_set (HyScanParam     *param,
                               HyScanParamList *list)
{
  HyScanDummyDevice *dummy = HYSCAN_DUMMY_DEVICE (param);
  HyScanDummyDevicePrivate *priv = dummy->priv;
  const gchar * const *keys;
  guint32 i;

  keys = hyscan_param_list_params (list);
  for (i = 0; keys[i] != NULL; i++)
    {
      gint32 value = hyscan_param_list_get_integer (list, keys[i]);

      if (g_str_has_prefix (keys[i], "/info/") ||
          g_str_has_prefix (keys[i], "/state/"))

        {
          return FALSE;
        }

      if (g_hash_table_contains (priv->params, keys[i]))
        g_hash_table_replace (priv->params, g_strdup (keys[i]), GINT_TO_POINTER (value));
      else
        return FALSE;
    }

  return TRUE;
}

static gboolean
hyscan_dummy_device_param_get (HyScanParam     *param,
                               HyScanParamList *list)
{
  HyScanDummyDevice *dummy = HYSCAN_DUMMY_DEVICE (param);
  HyScanDummyDevicePrivate *priv = dummy->priv;
  const gchar * const *keys;
  guint32 i;

  keys = hyscan_param_list_params (list);
  for (i = 0; keys[i] != NULL; i++)
    {
      gpointer value;

      if (g_str_has_prefix (keys[i], "/info/"))
        return FALSE;

      if (g_hash_table_contains (priv->params, keys[i]))
        value = g_hash_table_lookup (priv->params, keys[i]);
      else
        return FALSE;

      hyscan_param_list_set_integer (list, keys[i], GPOINTER_TO_INT (value));
    }

  return TRUE;
}

static gboolean
hyscan_dummy_device_sync (HyScanDevice *device)
{
  HyScanDummyDevice *dummy = HYSCAN_DUMMY_DEVICE (device);
  HyScanDummyDevicePrivate *priv = dummy->priv;

  priv->command = HYSCAN_DUMMY_DEVICE_COMMAND_SYNC;

  return TRUE;
}

static gboolean
hyscan_dummy_device_set_sound_velocity (HyScanDevice *device,
                                        GList        *svp)
{
  HyScanDummyDevice *dummy = HYSCAN_DUMMY_DEVICE (device);
  HyScanDummyDevicePrivate *priv = dummy->priv;

  priv->svp = svp;
  priv->command = HYSCAN_DUMMY_DEVICE_COMMAND_SET_SOUND_VELOCITY;

  return TRUE;
}

static gboolean
hyscan_dummy_device_disconnect (HyScanDevice *device)
{
  HyScanDummyDevice *dummy = HYSCAN_DUMMY_DEVICE (device);
  HyScanDummyDevicePrivate *priv = dummy->priv;

  priv->connected = FALSE;
  priv->command = HYSCAN_DUMMY_DEVICE_COMMAND_DISCONNECT;

  return TRUE;
}

static gboolean
hyscan_dummy_device_sonar_antenna_set_offset (HyScanSonar               *sonar,
                                              HyScanSourceType           source,
                                              const HyScanAntennaOffset *offset)
{
  HyScanDummyDevice *dummy = HYSCAN_DUMMY_DEVICE (sonar);
  HyScanDummyDevicePrivate *priv = dummy->priv;

  priv->offset = *offset;
  priv->command = HYSCAN_DUMMY_DEVICE_COMMAND_ANTENNA_SET_OFFSET;

  return TRUE;
}

static gboolean
hyscan_dummy_device_sonar_receiver_set_time (HyScanSonar      *sonar,
                                             HyScanSourceType  source,
                                             gdouble           receive_time,
                                             gdouble           wait_time)
{
  HyScanDummyDevice *dummy = HYSCAN_DUMMY_DEVICE (sonar);
  HyScanDummyDevicePrivate *priv = dummy->priv;

  priv->receiver_time = receive_time;
  priv->wait_time = wait_time;
  priv->command = HYSCAN_DUMMY_DEVICE_COMMAND_RECEIVER_SET_TIME;

  return TRUE;
}

static gboolean
hyscan_dummy_device_sonar_receiver_set_auto (HyScanSonar      *sonar,
                                             HyScanSourceType  source)
{
  HyScanDummyDevice *dummy = HYSCAN_DUMMY_DEVICE (sonar);
  HyScanDummyDevicePrivate *priv = dummy->priv;

  priv->command = HYSCAN_DUMMY_DEVICE_COMMAND_RECEIVER_SET_AUTO;

  return TRUE;
}

static gboolean
hyscan_dummy_device_sonar_receiver_disable (HyScanSonar      *sonar,
                                            HyScanSourceType  source)
{
  HyScanDummyDevice *dummy = HYSCAN_DUMMY_DEVICE (sonar);
  HyScanDummyDevicePrivate *priv = dummy->priv;

  priv->command = HYSCAN_DUMMY_DEVICE_COMMAND_RECEIVER_DISABLE;

  return TRUE;
}

static gboolean
hyscan_dummy_device_sonar_generator_set_preset (HyScanSonar      *sonar,
                                                HyScanSourceType  source,
                                                gint64            preset)
{
  HyScanDummyDevice *dummy = HYSCAN_DUMMY_DEVICE (sonar);
  HyScanDummyDevicePrivate *priv = dummy->priv;

  priv->generator_preset = preset;
  priv->command = HYSCAN_DUMMY_DEVICE_COMMAND_GENERATOR_SET_PRESET;

  return TRUE;
}

static gboolean
hyscan_dummy_device_sonar_generator_disable (HyScanSonar      *sonar,
                                             HyScanSourceType  source)
{
  HyScanDummyDevice *dummy = HYSCAN_DUMMY_DEVICE (sonar);
  HyScanDummyDevicePrivate *priv = dummy->priv;

  priv->command = HYSCAN_DUMMY_DEVICE_COMMAND_GENERATOR_DISABLE;

  return TRUE;
}

static gboolean
hyscan_dummy_device_sonar_tvg_set_auto (HyScanSonar      *sonar,
                                        HyScanSourceType  source,
                                        gdouble           level,
                                        gdouble           sensitivity)
{
  HyScanDummyDevice *dummy = HYSCAN_DUMMY_DEVICE (sonar);
  HyScanDummyDevicePrivate *priv = dummy->priv;

  priv->tvg_level = level;
  priv->tvg_sensitivity = sensitivity;
  priv->command = HYSCAN_DUMMY_DEVICE_COMMAND_TVG_SET_AUTO;

  return TRUE;
}

static gboolean
hyscan_dummy_device_sonar_tvg_set_constant (HyScanSonar      *sonar,
                                            HyScanSourceType  source,
                                            gdouble           gain)
{
  HyScanDummyDevice *dummy = HYSCAN_DUMMY_DEVICE (sonar);
  HyScanDummyDevicePrivate *priv = dummy->priv;

  priv->tvg_gain0 = gain;
  priv->command = HYSCAN_DUMMY_DEVICE_COMMAND_TVG_SET_CONSTANT;

  return TRUE;
}

static gboolean
hyscan_dummy_device_sonar_tvg_set_linear_db (HyScanSonar      *sonar,
                                             HyScanSourceType  source,
                                             gdouble           gain0,
                                             gdouble           gain_step)
{
  HyScanDummyDevice *dummy = HYSCAN_DUMMY_DEVICE (sonar);
  HyScanDummyDevicePrivate *priv = dummy->priv;

  priv->tvg_gain0 = gain0;
  priv->tvg_step = gain_step;
  priv->command = HYSCAN_DUMMY_DEVICE_COMMAND_TVG_SET_LINEAR_DB;

  return TRUE;
}

static gboolean
hyscan_dummy_device_sonar_tvg_set_logarithmic (HyScanSonar      *sonar,
                                               HyScanSourceType  source,
                                               gdouble           gain0,
                                               gdouble           beta,
                                               gdouble           alpha)
{
  HyScanDummyDevice *dummy = HYSCAN_DUMMY_DEVICE (sonar);
  HyScanDummyDevicePrivate *priv = dummy->priv;

  priv->tvg_gain0 = gain0;
  priv->tvg_beta = beta;
  priv->tvg_alpha = alpha;
  priv->command = HYSCAN_DUMMY_DEVICE_COMMAND_TVG_SET_LOGARITHMIC;

  return TRUE;
}

static gboolean
hyscan_dummy_device_sonar_tvg_disable (HyScanSonar      *sonar,
                                       HyScanSourceType  source)
{
  HyScanDummyDevice *dummy = HYSCAN_DUMMY_DEVICE (sonar);
  HyScanDummyDevicePrivate *priv = dummy->priv;

  priv->command = HYSCAN_DUMMY_DEVICE_COMMAND_TVG_DISABLE;

  return TRUE;
}

static gboolean
hyscan_dummy_device_sonar_start (HyScanSonar           *sonar,
                                 const gchar           *project_name,
                                 const gchar           *track_name,
                                 HyScanTrackType        track_type,
                                 const HyScanTrackPlan *track_plan)
{
  HyScanDummyDevice *dummy = HYSCAN_DUMMY_DEVICE (sonar);
  HyScanDummyDevicePrivate *priv = dummy->priv;

  HyScanAcousticDataInfo info;
  HyScanSourceType source;

  priv->project_name = project_name;
  priv->track_name = track_name;
  priv->track_type = track_type;
  priv->track_plan = track_plan;
  priv->command = HYSCAN_DUMMY_DEVICE_COMMAND_START;

  if (priv->type == HYSCAN_DUMMY_DEVICE_SIDE_SCAN)
    {
      source = HYSCAN_SOURCE_SIDE_SCAN_PORT;
      info = hyscan_dummy_device_get_acoustic_info (source);
      hyscan_sonar_driver_send_source_info (sonar, source, 1,
                                            hyscan_source_get_name_by_type (source),
                                            "actuator-1",
                                            &info);

      source = HYSCAN_SOURCE_SIDE_SCAN_STARBOARD;
      info = hyscan_dummy_device_get_acoustic_info (source);
      hyscan_sonar_driver_send_source_info (sonar, source, 1,
                                            hyscan_source_get_name_by_type (source),
                                            "actuator-1",
                                            &info);
    }
  else
    {
      source = HYSCAN_SOURCE_PROFILER;
      info = hyscan_dummy_device_get_acoustic_info (source);
      hyscan_sonar_driver_send_source_info (sonar, source, 1,
                                            hyscan_source_get_name_by_type (source),
                                            "actuator-2",
                                            &info);

      source = HYSCAN_SOURCE_PROFILER_ECHO;
      info = hyscan_dummy_device_get_acoustic_info (source);
      hyscan_sonar_driver_send_source_info (sonar, source, 1,
                                            hyscan_source_get_name_by_type (source),
                                            "actuator-2",
                                            &info);
    }

  return TRUE;
}

static gboolean
hyscan_dummy_device_sonar_stop (HyScanSonar *sonar)
{
  HyScanDummyDevice *dummy = HYSCAN_DUMMY_DEVICE (sonar);
  HyScanDummyDevicePrivate *priv = dummy->priv;

  priv->command = HYSCAN_DUMMY_DEVICE_COMMAND_STOP;

  return TRUE;
}

static gboolean
hyscan_dummy_device_sensor_antenna_set_offset (HyScanSensor              *sensor,
                                               const gchar               *sensor_name,
                                               const HyScanAntennaOffset *offset)
{
  HyScanDummyDevice *dummy = HYSCAN_DUMMY_DEVICE (sensor);
  HyScanDummyDevicePrivate *priv = dummy->priv;

  priv->offset = *offset;
  priv->command = HYSCAN_DUMMY_DEVICE_COMMAND_ANTENNA_SET_OFFSET;

  return TRUE;
}

static gboolean
hyscan_dummy_device_sensor_set_enable (HyScanSensor *sensor,
                                       const gchar  *sensor_name,
                                       gboolean      enable)
{
  HyScanDummyDevice *dummy = HYSCAN_DUMMY_DEVICE (sensor);
  HyScanDummyDevicePrivate *priv = dummy->priv;

  if (!enable)
    return FALSE;

  priv->sensor_name = sensor_name;
  priv->command = HYSCAN_DUMMY_DEVICE_COMMAND_SENSOR_ENABLE;

  return TRUE;
}

static gboolean
hyscan_dummy_device_actuator_disable (HyScanActuator *actuator,
                                      const gchar    *actuator_name)
{
  HyScanDummyDevice *dummy = HYSCAN_DUMMY_DEVICE (actuator);
  HyScanDummyDevicePrivate *priv = dummy->priv;

  priv->actuator_name = actuator_name;
  priv->command = HYSCAN_DUMMY_DEVICE_COMMAND_ACTUATOR_DISABLE;

  return TRUE;
}

static gboolean
hyscan_dummy_device_actuator_scan (HyScanActuator *actuator,
                                   const gchar    *actuator_name,
                                   gdouble         from,
                                   gdouble         to,
                                   gdouble         speed)
{
  HyScanDummyDevice *dummy = HYSCAN_DUMMY_DEVICE (actuator);
  HyScanDummyDevicePrivate *priv = dummy->priv;

  priv->actuator_name = actuator_name;
  priv->actuator_from = from;
  priv->actuator_to = to;
  priv->actuator_speed = speed;
  priv->command = HYSCAN_DUMMY_DEVICE_COMMAND_ACTUATOR_SCAN;

  return TRUE;
}

static gboolean
hyscan_dummy_device_actuator_manual (HyScanActuator *actuator,
                                     const gchar    *actuator_name,
                                     gdouble         angle)
{
  HyScanDummyDevice *dummy = HYSCAN_DUMMY_DEVICE (actuator);
  HyScanDummyDevicePrivate *priv = dummy->priv;

  priv->actuator_name = actuator_name;
  priv->actuator_angle = angle;
  priv->command = HYSCAN_DUMMY_DEVICE_COMMAND_ACTUATOR_MANUAL;

  return TRUE;
}

/**
 * hyscan_dummy_device_new:
 * @type: тип эмулируемого стройства #HyScanDummyDeviceType
 *
 * Функция создаёт новый экземпляр драйвера псевдо-устройства указанного типа.
 *
 * Returns: #HyScanDummyDevice. Для удаления #g_object_unref.
 */
HyScanDummyDevice *
hyscan_dummy_device_new (HyScanDummyDeviceType type)
{
  return g_object_new (HYSCAN_TYPE_DUMMY_DEVICE, "type", type, NULL);
}

/**
 * hyscan_dummy_device_get_id:
 * @dummy: указатель на #HyScanDummyDevice
 *
 * Функция возвращает идентификатор устройства.
 *
 * Returns: Идентификатор устройства.
 */
const gchar *
hyscan_dummy_device_get_id (HyScanDummyDevice *dummy)
{
  return dummy->priv->device_id;
}

/**
 * hyscan_dummy_device_change_state:
 * @dummy: указатель на #HyScanDummyDevice
 *
 * Функция сигнализирует об изменении состояния устройства.
 */
void
hyscan_dummy_device_change_state (HyScanDummyDevice *dummy)
{
  HyScanDummyDevicePrivate *priv;
  gchar key_id[128];

  g_return_if_fail (HYSCAN_IS_DUMMY_DEVICE (dummy));

  priv = dummy->priv;

  g_snprintf (key_id, sizeof (key_id), "/state/%s/status", priv->device_id);
  g_hash_table_replace (priv->params, g_strdup (key_id), GINT_TO_POINTER (HYSCAN_DEVICE_STATUS_OK));

  hyscan_device_driver_send_state (dummy, priv->device_id);
}

/**
 * hyscan_dummy_device_send_data:
 * @dummy: указатель на #HyScanDummyDevice
 *
 * Функция отправляет тестовые данные от источников данных и датчиков.
 */
void
hyscan_dummy_device_send_data (HyScanDummyDevice *dummy)
{
  HyScanBuffer *data;
  guint i;

  g_return_if_fail (HYSCAN_IS_DUMMY_DEVICE (dummy));

  if (dummy->priv->command != HYSCAN_DUMMY_DEVICE_COMMAND_START)
    return;

  data = hyscan_buffer_new ();

  for (i = 0; hyscan_dummy_device_sensors[i] != NULL; i++)
    {
      const gchar *sensor = hyscan_dummy_device_sensors[i];
      gchar *sdata;
      gint64 time;

      sdata = hyscan_dummy_device_get_sensor_data (sensor, &time);
      hyscan_buffer_wrap (data, HYSCAN_DATA_STRING, sdata, strlen (sdata) + 1);

      hyscan_sensor_driver_send_data (dummy, sensor, HYSCAN_SOURCE_NMEA, time, data);

      g_free (sdata);
    }

  for (i = 0; hyscan_dummy_device_sources[i] != HYSCAN_SOURCE_INVALID; i++)
    {
      HyScanSourceType source;
      HyScanAcousticDataInfo info;
      HyScanComplexFloat *cdata;
      gfloat *fdata;
      guint32 n_points;
      gint64 time;

      source = hyscan_dummy_device_sources[i];
      info = hyscan_dummy_device_get_acoustic_info (source);

      cdata = hyscan_dummy_device_get_complex_float_data (source, &n_points, &time);
      hyscan_buffer_wrap (data, HYSCAN_DATA_COMPLEX_FLOAT32LE, cdata, n_points * sizeof (HyScanComplexFloat));
      hyscan_sonar_driver_send_signal (dummy, source, 1, time, data);
      hyscan_sonar_driver_send_acoustic_data (dummy, source, 1, FALSE, time, &info, data);

      fdata = hyscan_dummy_device_get_float_data (source, &n_points, &time);
      hyscan_buffer_wrap (data, HYSCAN_DATA_FLOAT32LE, fdata, n_points * sizeof (gfloat));
      hyscan_sonar_driver_send_tvg (dummy, source, 1, time, data);

      g_free (cdata);
      g_free (fdata);
    }

  if (dummy->priv->type == HYSCAN_DUMMY_DEVICE_SIDE_SCAN)
    {
      gint64 time = 0;
      hyscan_device_driver_send_log (dummy, "side-scan", time, HYSCAN_LOG_LEVEL_INFO, "sonar-log");
      hyscan_device_driver_send_log (dummy, "side-scan", time, HYSCAN_LOG_LEVEL_INFO, "sensor-log");
    }
  else if (dummy->priv->type == HYSCAN_DUMMY_DEVICE_PROFILER)
    {
      gint64 time = 0;
      hyscan_device_driver_send_log (dummy, "profiler", time, HYSCAN_LOG_LEVEL_INFO, "sonar-log");
      hyscan_device_driver_send_log (dummy, "profiler", time, HYSCAN_LOG_LEVEL_INFO, "sensor-log");
    }

  dummy->priv->command = HYSCAN_DUMMY_DEVICE_COMMAND_INVALID;

  g_object_unref (data);
}

/**
 * hyscan_dummy_device_check_sound_velocity:
 * @dummy: указатель на #HyScanDummyDevice
 * @svp: профиль скорости звука
 *
 * Функция проверяет параметры функции #hyscan_device_set_sound_velocity.
 */
gboolean
hyscan_dummy_device_check_sound_velocity (HyScanDummyDevice *dummy,
                                          GList             *svp)
{
  HyScanDummyDevicePrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_DUMMY_DEVICE (dummy), FALSE);

  priv = dummy->priv;

  if (priv->command != HYSCAN_DUMMY_DEVICE_COMMAND_SET_SOUND_VELOCITY)
    return FALSE;

  if (priv->svp != svp)
    return FALSE;

  priv->command = HYSCAN_DUMMY_DEVICE_COMMAND_INVALID;
  priv->svp = NULL;

  return TRUE;
}

/**
 * hyscan_dummy_device_check_disconnect:
 * @dummy: указатель на #HyScanDummyDevice
 *
 * Функция проверяет параметры функций #hyscan_sonar_disconnect.
 */
gboolean
hyscan_dummy_device_check_disconnect (HyScanDummyDevice *dummy)
{
  HyScanDummyDevicePrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_DUMMY_DEVICE (dummy), FALSE);

  priv = dummy->priv;

  if (priv->command != HYSCAN_DUMMY_DEVICE_COMMAND_DISCONNECT)
    return FALSE;

  if (priv->connected)
    return FALSE;

  priv->command = HYSCAN_DUMMY_DEVICE_COMMAND_INVALID;

  return TRUE;
}

/**
 * hyscan_dummy_device_check_sound_velocity:
 * @dummy: указатель на #HyScanDummyDevice
 * @offset: смещение приёмной антенны
 *
 * Функция проверяет параметры функций #hyscan_sonar_antenna_set_offset и
 * #hyscan_sensor_antenna_set_offset.
 */
gboolean
hyscan_dummy_device_check_antenna_offset (HyScanDummyDevice   *dummy,
                                          HyScanAntennaOffset *offset)
{
  HyScanDummyDevicePrivate *priv;

   g_return_val_if_fail (HYSCAN_IS_DUMMY_DEVICE (dummy), FALSE);

   priv = dummy->priv;

   if (priv->command != HYSCAN_DUMMY_DEVICE_COMMAND_ANTENNA_SET_OFFSET)
     return FALSE;

   if ((offset->starboard != priv->offset.starboard) ||
       (offset->forward != priv->offset.forward) ||
       (offset->vertical != priv->offset.vertical) ||
       (offset->yaw != priv->offset.yaw) ||
       (offset->pitch != priv->offset.pitch) ||
       (offset->roll != priv->offset.roll))
     {
       return FALSE;
     }

   priv->command = HYSCAN_DUMMY_DEVICE_COMMAND_INVALID;
   priv->offset.starboard = G_MAXDOUBLE;
   priv->offset.forward = G_MAXDOUBLE;
   priv->offset.vertical = G_MAXDOUBLE;
   priv->offset.yaw = G_MAXDOUBLE;
   priv->offset.pitch = G_MAXDOUBLE;
   priv->offset.roll = G_MAXDOUBLE;

   return TRUE;
}

/**
 * hyscan_dummy_device_check_receiver_time:
 * @dummy: указатель на #HyScanDummyDevice
 * @receive_time: время приёма эхосигнала
 * @wait_time: время задержки излучения после приёма
 *
 * Функция проверяет параметры функций #hyscan_sonar_receiver_set_time.
 */
gboolean
hyscan_dummy_device_check_receiver_time (HyScanDummyDevice *dummy,
                                         gdouble            receive_time,
                                         gdouble            wait_time)
{
  HyScanDummyDevicePrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_DUMMY_DEVICE (dummy), FALSE);

  priv = dummy->priv;

  if (priv->command != HYSCAN_DUMMY_DEVICE_COMMAND_RECEIVER_SET_TIME)
    return FALSE;

  if ((priv->receiver_time != receive_time) || (priv->wait_time != wait_time))
    return FALSE;

  priv->command = HYSCAN_DUMMY_DEVICE_COMMAND_INVALID;
  priv->receiver_time = 0.0;
  priv->wait_time = 0.0;

  return TRUE;
}

/**
 * hyscan_dummy_device_check_receiver_auto:
 * @dummy: указатель на #HyScanDummyDevice
 *
 * Функция проверяет параметры функций #hyscan_sonar_receiver_set_auto.
 */
gboolean
hyscan_dummy_device_check_receiver_auto (HyScanDummyDevice *dummy)
{
  HyScanDummyDevicePrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_DUMMY_DEVICE (dummy), FALSE);

  priv = dummy->priv;

  if (priv->command != HYSCAN_DUMMY_DEVICE_COMMAND_RECEIVER_SET_AUTO)
    return FALSE;

  priv->command = HYSCAN_DUMMY_DEVICE_COMMAND_INVALID;

  return TRUE;
}

/**
 * hyscan_dummy_device_check_receiver_disable:
 * @dummy: указатель на #HyScanDummyDevice
 *
 * Функция проверяет параметры функций #hyscan_sonar_receiver_disable.
 */
gboolean
hyscan_dummy_device_check_receiver_disable (HyScanDummyDevice *dummy)
{
  HyScanDummyDevicePrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_DUMMY_DEVICE (dummy), FALSE);

  priv = dummy->priv;

  if (priv->command != HYSCAN_DUMMY_DEVICE_COMMAND_RECEIVER_DISABLE)
    return FALSE;

  priv->command = HYSCAN_DUMMY_DEVICE_COMMAND_INVALID;

  return TRUE;
}

/**
 * hyscan_dummy_device_check_generator_preset:
 * @dummy: указатель на #HyScanDummyDevice
 * @preset: идентификатор преднастройки
 *
 * Функция проверяет параметры функций #hyscan_sonar_generator_set_preset.
 */
gboolean
hyscan_dummy_device_check_generator_preset (HyScanDummyDevice *dummy,
                                            gint64             preset)
{
  HyScanDummyDevicePrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_DUMMY_DEVICE (dummy), FALSE);

  priv = dummy->priv;

  if (priv->command != HYSCAN_DUMMY_DEVICE_COMMAND_GENERATOR_SET_PRESET)
    return FALSE;

  if (priv->generator_preset != preset)
    return FALSE;

  priv->command = HYSCAN_DUMMY_DEVICE_COMMAND_INVALID;
  priv->generator_preset = 0;

  return TRUE;
}

/**
 * hyscan_dummy_device_check_generator_disable:
 * @dummy: указатель на #HyScanDummyDevice
 *
 * Функция проверяет параметры функций #hyscan_sonar_generator_disable.
 */
gboolean
hyscan_dummy_device_check_generator_disable (HyScanDummyDevice *dummy)
{
  HyScanDummyDevicePrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_DUMMY_DEVICE (dummy), FALSE);

  priv = dummy->priv;

  if (priv->command != HYSCAN_DUMMY_DEVICE_COMMAND_GENERATOR_DISABLE)
    return FALSE;

  priv->command = HYSCAN_DUMMY_DEVICE_COMMAND_INVALID;

  return TRUE;
}

/**
 * hyscan_dummy_device_check_tvg_auto:
 * @dummy: указатель на #HyScanDummyDevice
 * @level: целевой уровень сигнала
 * @sensitivity: чувствительность автомата регулировки
 *
 * Функция проверяет параметры функций #hyscan_sonar_tvg_set_auto.
 */
gboolean
hyscan_dummy_device_check_tvg_auto (HyScanDummyDevice *dummy,
                                    gdouble            level,
                                    gdouble            sensitivity)
{
  HyScanDummyDevicePrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_DUMMY_DEVICE (dummy), FALSE);

  priv = dummy->priv;

  if (priv->command != HYSCAN_DUMMY_DEVICE_COMMAND_TVG_SET_AUTO)
    return FALSE;

  if ((priv->tvg_level != level) ||
      (priv->tvg_sensitivity != sensitivity))
    {
      return FALSE;
    }

  priv->command = HYSCAN_DUMMY_DEVICE_COMMAND_INVALID;
  priv->tvg_level = 0.0;
  priv->tvg_sensitivity = 0.0;

  return TRUE;
}

/**
 * hyscan_dummy_device_check_tvg_linear_db:
 * @dummy: указатель на #HyScanDummyDevice
 * @gain: уровень усиления
 *
 * Функция проверяет параметры функций #hyscan_sonar_tvg_set_constant.
 */
gboolean
hyscan_dummy_device_check_tvg_constant (HyScanDummyDevice *dummy,
                                        gdouble            gain)
{
  HyScanDummyDevicePrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_DUMMY_DEVICE (dummy), FALSE);

  priv = dummy->priv;

  if (priv->command != HYSCAN_DUMMY_DEVICE_COMMAND_TVG_SET_CONSTANT)
    return FALSE;

  if (priv->tvg_gain0 != gain)
    return FALSE;

  priv->command = HYSCAN_DUMMY_DEVICE_COMMAND_INVALID;
  priv->tvg_gain0 = 0.0;

  return TRUE;
}

/**
 * hyscan_dummy_device_check_tvg_linear_db:
 * @dummy: указатель на #HyScanDummyDevice
 * @gain0: начальный уровень усиления
 * @gain_step: величина изменения усиления каждые 100 метров
 *
 * Функция проверяет параметры функций #hyscan_sonar_tvg_set_linear_db.
 */
gboolean
hyscan_dummy_device_check_tvg_linear_db (HyScanDummyDevice *dummy,
                                         gdouble            gain0,
                                         gdouble            gain_step)
{
  HyScanDummyDevicePrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_DUMMY_DEVICE (dummy), FALSE);

  priv = dummy->priv;

  if (priv->command != HYSCAN_DUMMY_DEVICE_COMMAND_TVG_SET_LINEAR_DB)
    return FALSE;

  if ((priv->tvg_gain0 != gain0) ||
      (priv->tvg_step != gain_step))
    {
      return FALSE;
    }

  priv->command = HYSCAN_DUMMY_DEVICE_COMMAND_INVALID;
  priv->tvg_gain0 = 0.0;
  priv->tvg_step = 0.0;

  return TRUE;
}

/**
 * hyscan_dummy_device_check_tvg_logarithmic:
 * @dummy: указатель на #HyScanDummyDevice
 * @gain0: начальный уровень усиления
 * @beta: коэффициент поглощения цели
 * @alpha: коэффициент затухания
 *
 * Функция проверяет параметры функций #hyscan_sonar_tvg_set_logaruthmic.
 */
gboolean
hyscan_dummy_device_check_tvg_logarithmic (HyScanDummyDevice *dummy,
                                           gdouble            gain0,
                                           gdouble            beta,
                                           gdouble            alpha)
{
  HyScanDummyDevicePrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_DUMMY_DEVICE (dummy), FALSE);

  priv = dummy->priv;

  if (priv->command != HYSCAN_DUMMY_DEVICE_COMMAND_TVG_SET_LOGARITHMIC)
    return FALSE;

  if ((priv->tvg_gain0 != gain0) ||
      (priv->tvg_beta != beta) ||
      (priv->tvg_alpha != alpha))
    {
      return FALSE;
    }

  priv->command = HYSCAN_DUMMY_DEVICE_COMMAND_INVALID;
  priv->tvg_gain0 = 0.0;
  priv->tvg_beta = 0.0;
  priv->tvg_alpha = 0.0;

  return TRUE;
}

/**
 * hyscan_dummy_device_check_tvg_disable:
 * @dummy: указатель на #HyScanDummyDevice
 *
 * Функция проверяет параметры функций #hyscan_sonar_tvg_disable.
 */
gboolean
hyscan_dummy_device_check_tvg_disable (HyScanDummyDevice *dummy)
{
  HyScanDummyDevicePrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_DUMMY_DEVICE (dummy), FALSE);

  priv = dummy->priv;

  if (priv->command != HYSCAN_DUMMY_DEVICE_COMMAND_TVG_DISABLE)
    return FALSE;

  priv->command = HYSCAN_DUMMY_DEVICE_COMMAND_INVALID;

  return TRUE;
}

/**
 * hyscan_dummy_device_check_start:
 * @dummy: указатель на #HyScanDummyDevice
 * @project_name: название проекта, в который записывать данные
 * @track_name: название галса, в который записывать данные
 * @track_type: тип галса
 * @track_plan: (nullable): план галса, #HyScanTrackPlan или %NULL
 *
 * Функция проверяет параметры функций #hyscan_sonar_start.
 */
gboolean
hyscan_dummy_device_check_start (HyScanDummyDevice     *dummy,
                                 const gchar           *project_name,
                                 const gchar           *track_name,
                                 HyScanTrackType        track_type,
                                 const HyScanTrackPlan *track_plan)
{
  HyScanDummyDevicePrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_DUMMY_DEVICE (dummy), FALSE);

  priv = dummy->priv;

  if (priv->command != HYSCAN_DUMMY_DEVICE_COMMAND_START)
    return FALSE;

  if ((priv->project_name != project_name) ||
      (priv->track_name != track_name) ||
      (priv->track_type != track_type) ||
      (priv->track_plan != track_plan))
    {
      return FALSE;
    }

  priv->project_name = NULL;
  priv->track_name = NULL;
  priv->track_type = 0;
  priv->track_plan = NULL;

  return TRUE;
}

/**
 * hyscan_dummy_device_check_stop:
 * @dummy: указатель на #HyScanDummyDevice
 *
 * Функция проверяет параметры функций #hyscan_sonar_stop.
 */
gboolean
hyscan_dummy_device_check_stop (HyScanDummyDevice *dummy)
{
  HyScanDummyDevicePrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_DUMMY_DEVICE (dummy), FALSE);

  priv = dummy->priv;

  if (priv->command != HYSCAN_DUMMY_DEVICE_COMMAND_STOP)
    return FALSE;

  priv->command = HYSCAN_DUMMY_DEVICE_COMMAND_INVALID;

  return TRUE;
}

/**
 * hyscan_dummy_device_check_sync:
 * @dummy: указатель на #HyScanDummyDevice
 *
 * Функция проверяет параметры функций #hyscan_sonar_sync.
 */
gboolean
hyscan_dummy_device_check_sync (HyScanDummyDevice *dummy)
{
  HyScanDummyDevicePrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_DUMMY_DEVICE (dummy), FALSE);

  priv = dummy->priv;

  if (priv->command != HYSCAN_DUMMY_DEVICE_COMMAND_SYNC)
    return FALSE;

  priv->command = HYSCAN_DUMMY_DEVICE_COMMAND_INVALID;

  return TRUE;
}

/**
 * hyscan_dummy_device_check_sensor_enable:
 * @dummy: указатель на #HyScanDummyDevice
 * @sensor: название датчика
 *
 * Функция проверяет параметры функций #hyscan_sensor_set_enable.
 */
gboolean
hyscan_dummy_device_check_sensor_enable (HyScanDummyDevice *dummy,
                                         const gchar       *sensor)
{
  HyScanDummyDevicePrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_DUMMY_DEVICE (dummy), FALSE);

  priv = dummy->priv;

  if (priv->command != HYSCAN_DUMMY_DEVICE_COMMAND_SENSOR_ENABLE)
    return FALSE;

  if (priv->sensor_name != sensor)
    return FALSE;

  priv->command = HYSCAN_DUMMY_DEVICE_COMMAND_INVALID;
  priv->sensor_name = NULL;

  return TRUE;
}

/**
 * hyscan_dummy_device_check_actuator_disable:
 * @dummy: указатель на #HyScanDummyDevice
 * @actuator: название привода
 *
 * Функция проверяет параметры функций #hyscan_actuator_disable.
 */
gboolean
hyscan_dummy_device_check_actuator_disable (HyScanDummyDevice *dummy,
                                            const gchar       *actuator)
{
  HyScanDummyDevicePrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_DUMMY_DEVICE (dummy), FALSE);

  priv = dummy->priv;

  if (priv->command != HYSCAN_DUMMY_DEVICE_COMMAND_ACTUATOR_DISABLE)
    return FALSE;

  if (priv->actuator_name != actuator)
    return FALSE;

  priv->command = HYSCAN_DUMMY_DEVICE_COMMAND_INVALID;
  priv->actuator_name = NULL;

  return TRUE;
}

/**
 * hyscan_dummy_device_check_actuator_scan:
 * @dummy: указатель на #HyScanDummyDevice
 * @actuator: название привода
 * @from: начальный угол сектора обзора
 * @to: конечный угол сектора обзора
 * @speed: скорость вращения привода
 *
 * Функция проверяет параметры функций #hyscan_actuator_scan.
 */
gboolean
hyscan_dummy_device_check_actuator_scan (HyScanDummyDevice *dummy,
                                         const gchar       *actuator,
                                         gdouble            from,
                                         gdouble            to,
                                         gdouble            speed)
{
  HyScanDummyDevicePrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_DUMMY_DEVICE (dummy), FALSE);

  priv = dummy->priv;

  if (priv->command != HYSCAN_DUMMY_DEVICE_COMMAND_ACTUATOR_SCAN)
    return FALSE;

  if ((priv->actuator_name != actuator) ||
      (priv->actuator_from != from) ||
      (priv->actuator_to != to) ||
      (priv->actuator_speed != speed))
    {
      return FALSE;
    }

  priv->command = HYSCAN_DUMMY_DEVICE_COMMAND_INVALID;
  priv->actuator_name = NULL;
  priv->actuator_from = 0.0;
  priv->actuator_to = 0.0;
  priv->actuator_speed = 0.0;

  return TRUE;
}

/**
 * hyscan_dummy_device_check_actuator_manual:
 * @dummy: указатель на #HyScanDummyDevice
 * @actuator: название привода
 * @angle: угол направления привода
 *
 * Функция проверяет параметры функций #hyscan_actuator_manual.
 */
gboolean
hyscan_dummy_device_check_actuator_manual (HyScanDummyDevice *dummy,
                                           const gchar       *actuator,
                                           gdouble            angle)
{
  HyScanDummyDevicePrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_DUMMY_DEVICE (dummy), FALSE);

  priv = dummy->priv;

  if (priv->command != HYSCAN_DUMMY_DEVICE_COMMAND_ACTUATOR_MANUAL)
    return FALSE;

  if ((priv->actuator_name != actuator) ||
      (priv->actuator_angle != angle))
    {
      return FALSE;
    }

  priv->command = HYSCAN_DUMMY_DEVICE_COMMAND_INVALID;
  priv->actuator_name = NULL;
  priv->actuator_angle = 0.0;

  return TRUE;
}

/**
 * hyscan_dummy_device_check_params:
 * @info_id: параметр ветки /info
 * @param_id: параметр ветки /param
 * @system_id: параметр ветки /system
 *
 * Функция проверяет значения параметров драйвера, изменяемые через
 * интерфейс #HyScanParam.
 *
 * Returns: %TRUE если проверка прошла успешно, иначе %FALSE.
 */
gboolean
hyscan_dummy_device_check_params (HyScanDummyDevice *dummy,
                                  gint32             info_id,
                                  gint32             param_id,
                                  gint32             system_id)
{
  HyScanDummyDevicePrivate *priv;
  gchar *key_id;

  g_return_val_if_fail (HYSCAN_IS_DUMMY_DEVICE (dummy), FALSE);

  priv = dummy->priv;

  key_id = g_strdup_printf ("/info/%s/id", priv->device_id);
  if (g_hash_table_lookup (priv->params, key_id) != GINT_TO_POINTER (info_id))
    return FALSE;
  g_free (key_id);

  key_id = g_strdup_printf ("/params/%s/id", priv->device_id);
  if (g_hash_table_lookup (priv->params, key_id) != GINT_TO_POINTER (param_id))
    return FALSE;
  g_free (key_id);

  key_id = g_strdup_printf ("/system/%s/id", priv->device_id);
  if (g_hash_table_lookup (priv->params, key_id) != GINT_TO_POINTER (system_id))
    return FALSE;
  g_free (key_id);

  return TRUE;
}

HyScanDummyDeviceType
hyscan_dummy_device_get_type_by_sensor (const gchar *sensor)
{
  if ((g_strcmp0 (sensor, "nmea-1") == 0) ||
      (g_strcmp0 (sensor, "nmea-2") == 0))
    {
      return HYSCAN_DUMMY_DEVICE_SIDE_SCAN;
    }

  if ((g_strcmp0 (sensor, "nmea-3") == 0) ||
      (g_strcmp0 (sensor, "nmea-4") == 0))
    {
      return HYSCAN_DUMMY_DEVICE_PROFILER;
    }

  return HYSCAN_DUMMY_DEVICE_INVALID;
}

HyScanDummyDeviceType
hyscan_dummy_device_get_type_by_source (HyScanSourceType source)
{
  if ((source == HYSCAN_SOURCE_SIDE_SCAN_PORT) ||
      (source == HYSCAN_SOURCE_SIDE_SCAN_STARBOARD))
    {
      return HYSCAN_DUMMY_DEVICE_SIDE_SCAN;
    }

  if ((source == HYSCAN_SOURCE_PROFILER) ||
      (source == HYSCAN_SOURCE_PROFILER_ECHO))
    {
      return HYSCAN_DUMMY_DEVICE_PROFILER;
    }

  return HYSCAN_DUMMY_DEVICE_INVALID;
}

HyScanDummyDeviceType
hyscan_dummy_device_get_type_by_actuator (const gchar *actuator)
{
  if (g_strcmp0 (actuator, "actuator-1") == 0)
    return HYSCAN_DUMMY_DEVICE_SIDE_SCAN;

  if (g_strcmp0 (actuator, "actuator-2") == 0)
    return HYSCAN_DUMMY_DEVICE_PROFILER;

  return HYSCAN_DUMMY_DEVICE_INVALID;
}

/**
 * hyscan_dummy_device_get_sensor_offset:
 * @sensor: название датчика
 *
 * Функция возвращает эталонные значения смещения антенн датчика.
 *
 * Returns: (transfer full): Смещение антенн #HyScanAntennaOffset.
 * Для удаления #hyscan_antenna_offset_free.
 */
HyScanAntennaOffset *
hyscan_dummy_device_get_sensor_offset (const gchar *sensor)
{
  HyScanAntennaOffset offset;
  guint seed = g_str_hash (sensor);

  offset.starboard = 1.0 * seed;
  offset.forward = 2.0 * seed;
  offset.vertical = 3.0 * seed;
  offset.yaw = 4.0 * seed;
  offset.pitch = 5.0 * seed;
  offset.roll = 6.0 * seed;

  return hyscan_antenna_offset_copy (&offset);
}

/**
 * hyscan_dummy_device_get_source_offset:
 * @source: тип гидролокационного источника данных
 *
 * Функция возвращает эталонные значения смещения антенн гидролокационного
 * источника данных.
 *
 * Returns: (transfer full): Смещение антенн #HyScanAntennaOffset.
 * Для удаления #hyscan_antenna_offset_free.
 */
HyScanAntennaOffset *
hyscan_dummy_device_get_source_offset (HyScanSourceType source)
{
  HyScanAntennaOffset offset;

  offset.starboard = 1.0 * source;
  offset.forward = 2.0 * source;
  offset.vertical = 3.0 * source;
  offset.yaw = 4.0 * source;
  offset.pitch = 5.0 * source;
  offset.roll = 6.0 * source;

  return hyscan_antenna_offset_copy (&offset);
}

/**
 * hyscan_dummy_device_get_sensor_info:
 * @sensor: название датчика
 *
 * Функция возвращает эталонные значения параметров датчика.
 *
 * Returns: (transfer full): Параметры датчика #HyScanSensorInfoSensor.
 * Для удаления #hyscan_sensor_info_sensor_free.
 */
HyScanSensorInfoSensor *
hyscan_dummy_device_get_sensor_info (const gchar *sensor)
{
  HyScanSensorInfoSensor *pinfo;
  HyScanSensorInfoSensor info;

  HyScanDummyDeviceType dev_type;
  const gchar *dev_id;

  /* Идентификатор устройства. */
  dev_type = hyscan_dummy_device_get_type_by_sensor (sensor);
  if (dev_type == HYSCAN_DUMMY_DEVICE_SIDE_SCAN)
    dev_id = "ss";
  else if (dev_type == HYSCAN_DUMMY_DEVICE_PROFILER)
    dev_id = "pf";
  else
    return NULL;

  info.name = sensor;
  info.dev_id = dev_id;
  info.description = g_strdup_printf ("%s description", sensor);

  if (g_strcmp0 (sensor, "nmea-3") == 0)
    info.offset = hyscan_dummy_device_get_sensor_offset (sensor);
  else
    info.offset = NULL;

  pinfo = hyscan_sensor_info_sensor_copy (&info);
  hyscan_antenna_offset_free (info.offset);
  g_free ((gchar*)info.description);

  return pinfo;
}

/**
 * hyscan_dummy_device_get_source_info:
 * @source: тип гидролокационного источника данных
 *
 * Функция возвращает эталонные значения параметров источника
 * гидролокационных данных.
 *
 * Returns: (transfer full): Параметры источника данных #HyScanSonarInfoSource.
 * Для удаления #hyscan_sonar_info_source_free.
 */
HyScanSonarInfoSource *
hyscan_dummy_device_get_source_info (HyScanSourceType source)
{
  const gchar *source_name;

  HyScanSonarInfoSource *pinfo = NULL;
  HyScanSonarInfoSource info;

  HyScanDummyDeviceType dev_type;
  const gchar *dev_id;

  HyScanSonarInfoReceiver receiver;
  GList *presets = NULL;
  HyScanSonarInfoTVG tvg;

  guint i;

  /* Название источника данных. */
  source_name = hyscan_source_get_id_by_type (source);

  /* Идентификатор устройства. */
  dev_type = hyscan_dummy_device_get_type_by_source (source);
  if (dev_type == HYSCAN_DUMMY_DEVICE_SIDE_SCAN)
    dev_id = "ss";
  else if (dev_type == HYSCAN_DUMMY_DEVICE_PROFILER)
    dev_id = "pf";
  else
    return NULL;

  /* Возможности источника данных. */
  receiver.capabilities = HYSCAN_SONAR_RECEIVER_MODE_MANUAL |
                          HYSCAN_SONAR_RECEIVER_MODE_AUTO;

  tvg.capabilities = HYSCAN_SONAR_TVG_MODE_AUTO |
                     HYSCAN_SONAR_TVG_MODE_CONSTANT |
                     HYSCAN_SONAR_TVG_MODE_LINEAR_DB |
                     HYSCAN_SONAR_TVG_MODE_LOGARITHMIC;

  /* Преднастройки генератора. */
  for (i = 0; i < N_PRESETS; i++)
    {
      HyScanDataSchemaEnumValue *preset;
      gchar *id = g_strdup_printf ("%s-preset-%d", source_name, i + 1);
      gchar *name = g_strdup_printf ("%s name %d", source_name, i + 1);
      gchar *description = g_strdup_printf ("%s description %d", source_name, i + 1);

      preset = hyscan_data_schema_enum_value_new (i, id, name, description);
      presets = g_list_append (presets, preset);

      g_free (description);
      g_free (name);
      g_free (id);
    }

  /* Описание источника данных. */
  info.source = source;
  info.dev_id = dev_id;
  info.description = source_name;
  info.actuator = (dev_type == HYSCAN_DUMMY_DEVICE_SIDE_SCAN) ? "actuator-1" : "actuator-2";
  info.offset = NULL;
  info.receiver = &receiver;
  info.presets = presets;
  info.tvg = &tvg;

  /* Уникальные параметры источников данных. */
  if (source == HYSCAN_SOURCE_SIDE_SCAN_PORT)
    {
      receiver.min_time = 0.01;
      receiver.max_time = 0.1;

      tvg.min_gain = -10.0;
      tvg.max_gain = 10.0;
      tvg.decrease = TRUE;
    }
  else if (source == HYSCAN_SOURCE_SIDE_SCAN_STARBOARD)
    {
      receiver.min_time = 0.02;
      receiver.max_time = 0.2;

      tvg.min_gain = -20.0;
      tvg.max_gain = 20.0;
      tvg.decrease = TRUE;
    }
  else if (source == HYSCAN_SOURCE_PROFILER)
    {
      receiver.min_time = 0.03;
      receiver.max_time = 0.3;

      tvg.min_gain = -30.0;
      tvg.max_gain = 30.0;
      tvg.decrease = TRUE;
    }
  else if (source == HYSCAN_SOURCE_PROFILER_ECHO)
    {
      receiver.min_time = 0.04;
      receiver.max_time = 0.4;

      tvg.min_gain = -40.0;
      tvg.max_gain = 40.0;
      tvg.decrease = TRUE;
    }
  else
    {
      goto exit;
    }

  pinfo = hyscan_sonar_info_source_copy (&info);

exit:
  g_list_free_full (presets, (GDestroyNotify)hyscan_data_schema_enum_value_free);

  return pinfo;
}

/**
 * hyscan_dummy_device_get_actuator_info:
 * @sensor: название привода
 *
 * Функция возвращает эталонные значения параметров привода.
 *
 * Returns: (transfer full): Параметры привода #HyScanActuatorInfoActuator.
 * Для удаления #hyscan_actuator_info_actuator_free.
 */
HyScanActuatorInfoActuator *
hyscan_dummy_device_get_actuator_info (const gchar *actuator)
{
  HyScanActuatorInfoActuator *pinfo;
  HyScanActuatorInfoActuator info;

  HyScanDummyDeviceType dev_type;
  const gchar *dev_id;

  /* Идентификатор устройства. */
  dev_type = hyscan_dummy_device_get_type_by_actuator (actuator);
  if (dev_type == HYSCAN_DUMMY_DEVICE_SIDE_SCAN)
    dev_id = "ss";
  else if (dev_type == HYSCAN_DUMMY_DEVICE_PROFILER)
    dev_id = "pf";
  else
    return NULL;

  info.name = actuator;
  info.dev_id = dev_id;
  info.description = g_strdup_printf ("%s description", actuator);

  info.capabilities = HYSCAN_ACTUATOR_MODE_SCAN |
                      HYSCAN_ACTUATOR_MODE_MANUAL;
  info.min_range = -G_PI;
  info.max_range = G_PI;
  info.min_speed = G_PI / 10.0;
  info.max_speed = G_PI / 2.0;

  pinfo = hyscan_actuator_info_actuator_copy (&info);
  g_free ((gchar*)info.description);

  return pinfo;
}

/**
 * hyscan_dummy_device_get_acoustic_info:
 * @source: тип гидролокационного источника данных
 *
 * Функция возвращает эталонные значения параметров акустических данных.
 *
 * Returns: Параметры акустических данных #HyScanAcousticDataInfo.
 */
HyScanAcousticDataInfo
hyscan_dummy_device_get_acoustic_info (HyScanSourceType source)
{
  HyScanAcousticDataInfo info;

  info.data_type = HYSCAN_DATA_COMPLEX_FLOAT32LE;
  info.data_rate = 2.0 * source;
  info.signal_frequency = 3.0 * source;
  info.signal_bandwidth = 4.0 * source;
  info.signal_heterodyne = 5.0 * source;
  info.antenna_voffset = 6.0 * source;
  info.antenna_hoffset = 7.0 * source;
  info.antenna_vaperture = 8.0 * source;
  info.antenna_haperture = 9.0 * source;
  info.antenna_frequency = 10.0 * source;
  info.antenna_bandwidth = 11.0 * source;
  info.antenna_group = 12 * source;
  info.adc_vref = 13.0 * source;
  info.adc_offset = 14.0 * source;

  return info;
}

/**
 * hyscan_dummy_device_get_sensor_data:
 * @sensor: название датчика
 * @time: метка времени данныx
 *
 * Функция возвращает эталонные данные датчика.
 *
 * Returns: (transfer full): Эталонные данные. Для удаления #g_free.
 */
gchar *
hyscan_dummy_device_get_sensor_data (const gchar *sensor,
                                     gint64      *time)
{
  *time = g_str_hash (sensor);

  return g_strdup (sensor);
}

/**
 * hyscan_dummy_device_get_complex_float_data:
 * @source: тип гидролокационного источника данных
 * @n_points: число точек данных
 * @time: метка времени данныx
 *
 * Функция возвращает эталонные гидролокационные данные.
 *
 * Returns: (transfer full): Эталонные данные. Для удаления #g_free.
 */
HyScanComplexFloat *
hyscan_dummy_device_get_complex_float_data (HyScanSourceType  source,
                                            guint32          *n_points,
                                            gint64           *time)
{
  HyScanComplexFloat *data;
  guint i;

  data = g_new (HyScanComplexFloat, N_POINTS);
  for (i = 0; i < N_POINTS; i++)
    data[i].re = data[i].im = source;

  *n_points = N_POINTS;
  *time = source;

  return data;
}

/**
 * hyscan_dummy_device_get_float_data:
 * @source: тип гидролокационного источника данных
 * @n_points: число точек данных
 * @time: метка времени данныx
 *
 * Функция возвращает эталонные гидролокационные данные.
 *
 * Returns: (transfer full): Эталонные данные. Для удаления #g_free.
 */
gfloat *
hyscan_dummy_device_get_float_data (HyScanSourceType  source,
                                    guint32          *n_points,
                                    gint64           *time)
{
  gfloat *data;
  guint i;

  data = g_new (gfloat, N_POINTS);
  for (i = 0; i < N_POINTS; i++)
    data[i] = source;

  *n_points = N_POINTS;
  *time = source;

  return data;
}

static void
hyscan_dummy_device_param_interface_init (HyScanParamInterface *iface)
{
  iface->schema = hyscan_dummy_device_param_schema;
  iface->set = hyscan_dummy_device_param_set;
  iface->get = hyscan_dummy_device_param_get;
}

static void
hyscan_dummy_device_device_interface_init (HyScanDeviceInterface *iface)
{
  iface->sync = hyscan_dummy_device_sync;
  iface->set_sound_velocity = hyscan_dummy_device_set_sound_velocity;
  iface->disconnect = hyscan_dummy_device_disconnect;
}

static void
hyscan_dummy_device_sonar_interface_init (HyScanSonarInterface *iface)
{
  iface->antenna_set_offset = hyscan_dummy_device_sonar_antenna_set_offset;
  iface->receiver_set_time = hyscan_dummy_device_sonar_receiver_set_time;
  iface->receiver_set_auto = hyscan_dummy_device_sonar_receiver_set_auto;
  iface->receiver_disable = hyscan_dummy_device_sonar_receiver_disable;
  iface->generator_set_preset = hyscan_dummy_device_sonar_generator_set_preset;
  iface->generator_disable = hyscan_dummy_device_sonar_generator_disable;
  iface->tvg_set_auto = hyscan_dummy_device_sonar_tvg_set_auto;
  iface->tvg_set_constant = hyscan_dummy_device_sonar_tvg_set_constant;
  iface->tvg_set_linear_db = hyscan_dummy_device_sonar_tvg_set_linear_db;
  iface->tvg_set_logarithmic = hyscan_dummy_device_sonar_tvg_set_logarithmic;
  iface->tvg_disable = hyscan_dummy_device_sonar_tvg_disable;
  iface->start = hyscan_dummy_device_sonar_start;
  iface->stop = hyscan_dummy_device_sonar_stop;
}

static void
hyscan_dummy_device_sensor_interface_init (HyScanSensorInterface *iface)
{
  iface->antenna_set_offset = hyscan_dummy_device_sensor_antenna_set_offset;
  iface->set_enable = hyscan_dummy_device_sensor_set_enable;
}

static void
hyscan_dummy_device_actuator_interface_init (HyScanActuatorInterface *iface)
{
  iface->disable = hyscan_dummy_device_actuator_disable;
  iface->scan = hyscan_dummy_device_actuator_scan;
  iface->manual = hyscan_dummy_device_actuator_manual;
}
