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
 * Драйвер реализует интерфейсы #HyScanSonar, #HyScanSensor и #HyScanParam.
 * Он запоминает значения параметров для последней вызваной функции этих
 * интерфейсов, а также содержит функции проверяющие эти параметры. Логика
 * тестирования предполагает вызов каждой функции интерфейса и сравнение
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
  HYSCAN_DUMMY_DEVICE_COMMAND_SET_SOUND_VELOCITY,
  HYSCAN_DUMMY_DEVICE_COMMAND_RECEIVER_SET_TIME,
  HYSCAN_DUMMY_DEVICE_COMMAND_RECEIVER_SET_AUTO,
  HYSCAN_DUMMY_DEVICE_COMMAND_GENERATOR_SET_PRESET,
  HYSCAN_DUMMY_DEVICE_COMMAND_GENERATOR_SET_AUTO,
  HYSCAN_DUMMY_DEVICE_COMMAND_GENERATOR_SET_SIMPLE,
  HYSCAN_DUMMY_DEVICE_COMMAND_GENERATOR_SET_EXTENDED,
  HYSCAN_DUMMY_DEVICE_COMMAND_TVG_SET_AUTO,
  HYSCAN_DUMMY_DEVICE_COMMAND_TVG_SET_POINTS,
  HYSCAN_DUMMY_DEVICE_COMMAND_TVG_SET_CONSTANT,
  HYSCAN_DUMMY_DEVICE_COMMAND_TVG_SET_LINEAR_DB,
  HYSCAN_DUMMY_DEVICE_COMMAND_TVG_SET_LOGARITHMIC,
  HYSCAN_DUMMY_DEVICE_COMMAND_SET_SOFTWARE_PING,
  HYSCAN_DUMMY_DEVICE_COMMAND_START,
  HYSCAN_DUMMY_DEVICE_COMMAND_STOP,
  HYSCAN_DUMMY_DEVICE_COMMAND_SYNC,
  HYSCAN_DUMMY_DEVICE_COMMAND_PING,
  HYSCAN_DUMMY_DEVICE_COMMAND_DISCONNECT,
  HYSCAN_DUMMY_DEVICE_COMMAND_SENSOR_ENABLE
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

  gdouble                          receiver_time;
  gdouble                          wait_time;

  gint64                           generator_preset;
  HyScanSonarGeneratorSignalType   generator_signal;
  gdouble                          generator_power;
  gdouble                          generator_duration;

  gdouble                          tvg_level;
  gdouble                          tvg_sensitivity;
  gdouble                          tvg_gain0;
  gdouble                          tvg_step;
  gdouble                          tvg_alpha;
  gdouble                          tvg_beta;
  gdouble                          tvg_time_step;
  const gdouble                   *tvg_gains;
  guint32                          tvg_n_gains;

  const gchar                     *project_name;
  const gchar                     *track_name;
  HyScanTrackType                  track_type;

  const gchar                     *sensor_name;
};

static void        hyscan_dummy_device_param_interface_init     (HyScanParamInterface  *iface);
static void        hyscan_dummy_device_sonar_interface_init     (HyScanSonarInterface  *iface);
static void        hyscan_dummy_device_sensor_interface_init    (HyScanSensorInterface *iface);

static void        hyscan_dummy_device_set_property             (GObject               *object,
                                                                 guint                  prop_id,
                                                                 const GValue          *value,
                                                                 GParamSpec            *pspec);

static void        hyscan_dummy_device_object_constructed       (GObject               *object);
static void        hyscan_dummy_device_object_finalize          (GObject               *object);

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
  HYSCAN_SOURCE_ECHOPROFILER,
  HYSCAN_SOURCE_INVALID
};

G_DEFINE_TYPE_WITH_CODE (HyScanDummyDevice, hyscan_dummy_device, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (HyScanDummyDevice)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_PARAM, hyscan_dummy_device_param_interface_init)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_SONAR, hyscan_dummy_device_sonar_interface_init)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_SENSOR, hyscan_dummy_device_sensor_interface_init))

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

  HyScanDataSchemaBuilder *builder;
  HyScanSensorSchema *sensor_schema;
  HyScanSonarSchema *sonar_schema;

  gint32 uniq_value;
  gchar *key_id;

  builder = hyscan_data_schema_builder_new ("device");
  sensor_schema = hyscan_sensor_schema_new (builder);
  sonar_schema = hyscan_sonar_schema_new (builder);

  hyscan_sonar_schema_set_software_ping (sonar_schema);

  /* ГБО. */
  if (priv->type == HYSCAN_DUMMY_DEVICE_SIDE_SCAN)
    {
      HyScanSensorInfoSensor *sensor_info;
      HyScanSonarInfoSource *source_info;

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

      priv->device_id = "ss";
    }

  /* Профилограф. */
  else if (priv->type == HYSCAN_DUMMY_DEVICE_PROFILER)
    {
      HyScanSensorInfoSensor *sensor_info;
      HyScanSonarInfoSource *source_info;

      sensor_info = hyscan_dummy_device_get_sensor_info ("nmea-3");
      hyscan_sensor_schema_add_full (sensor_schema, sensor_info);
      hyscan_sensor_info_sensor_free (sensor_info);

      sensor_info = hyscan_dummy_device_get_sensor_info ("nmea-4");
      hyscan_sensor_schema_add_full (sensor_schema, sensor_info);
      hyscan_sensor_info_sensor_free (sensor_info);

      source_info = hyscan_dummy_device_get_source_info (HYSCAN_SOURCE_PROFILER);
      hyscan_sonar_schema_source_add_full (sonar_schema, source_info);
      hyscan_sonar_info_source_free (source_info);

      source_info = hyscan_dummy_device_get_source_info (HYSCAN_SOURCE_ECHOPROFILER);
      hyscan_sonar_schema_source_add_full (sonar_schema, source_info);
      hyscan_sonar_info_source_free (source_info);

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

  uniq_value = g_random_int_range (0, 1024);
  key_id = g_strdup_printf ("/state/%s/id", priv->device_id);
  hyscan_data_schema_builder_key_integer_create (builder, key_id, "id", NULL, 0);
  g_hash_table_insert (priv->params, key_id, GINT_TO_POINTER (uniq_value));

  key_id = g_strdup_printf ("/state/%s/status", priv->device_id);
  hyscan_data_schema_builder_key_string_create (builder, key_id, "id", NULL, NULL);
  g_hash_table_insert (priv->params, key_id, HYSCAN_DEVICE_SCHEMA_STATUS_ERROR);

  priv->schema = hyscan_data_schema_builder_get_schema (builder);

  priv->connected = TRUE;

  g_object_unref (sensor_schema);
  g_object_unref (sonar_schema);
  g_object_unref (builder);
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

      if (g_str_has_suffix (keys[i], "/status"))
        hyscan_param_list_set_string (list, keys[i], value);
      else
        hyscan_param_list_set_integer (list, keys[i], GPOINTER_TO_INT (value));
    }

  return TRUE;
}

static gboolean
hyscan_dummy_device_sonar_set_sound_velocity (HyScanSonar *sonar,
                                              GList       *svp)
{
  HyScanDummyDevice *dummy = HYSCAN_DUMMY_DEVICE (sonar);
  HyScanDummyDevicePrivate *priv = dummy->priv;

  priv->svp = svp;
  priv->command = HYSCAN_DUMMY_DEVICE_COMMAND_SET_SOUND_VELOCITY;

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
hyscan_dummy_device_sonar_generator_set_auto (HyScanSonar                    *sonar,
                                              HyScanSourceType                source,
                                              HyScanSonarGeneratorSignalType  signal)
{
  HyScanDummyDevice *dummy = HYSCAN_DUMMY_DEVICE (sonar);
  HyScanDummyDevicePrivate *priv = dummy->priv;

  priv->generator_signal = signal;
  priv->command = HYSCAN_DUMMY_DEVICE_COMMAND_GENERATOR_SET_AUTO;

  return TRUE;
}

static gboolean
hyscan_dummy_device_sonar_generator_set_simple (HyScanSonar                    *sonar,
                                                HyScanSourceType                source,
                                                HyScanSonarGeneratorSignalType  signal,
                                                gdouble                         power)
{
  HyScanDummyDevice *dummy = HYSCAN_DUMMY_DEVICE (sonar);
  HyScanDummyDevicePrivate *priv = dummy->priv;

  priv->generator_signal = signal;
  priv->generator_power = power;
  priv->command = HYSCAN_DUMMY_DEVICE_COMMAND_GENERATOR_SET_SIMPLE;

  return TRUE;
}

static gboolean
hyscan_dummy_device_sonar_generator_set_extended (HyScanSonar                    *sonar,
                                                  HyScanSourceType                source,
                                                  HyScanSonarGeneratorSignalType  signal,
                                                  gdouble                         duration,
                                                  gdouble                         power)
{
  HyScanDummyDevice *dummy = HYSCAN_DUMMY_DEVICE (sonar);
  HyScanDummyDevicePrivate *priv = dummy->priv;

  priv->generator_signal = signal;
  priv->generator_power = power;
  priv->generator_duration = duration;
  priv->command = HYSCAN_DUMMY_DEVICE_COMMAND_GENERATOR_SET_EXTENDED;

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
hyscan_dummy_device_sonar_tvg_set_points (HyScanSonar      *sonar,
                                          HyScanSourceType  source,
                                          gdouble           time_step,
                                          const gdouble    *gains,
                                          guint32           n_gains)
{
  HyScanDummyDevice *dummy = HYSCAN_DUMMY_DEVICE (sonar);
  HyScanDummyDevicePrivate *priv = dummy->priv;

  priv->tvg_time_step = time_step;
  priv->tvg_gains = gains;
  priv->tvg_n_gains = n_gains;
  priv->command = HYSCAN_DUMMY_DEVICE_COMMAND_TVG_SET_POINTS;

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
hyscan_dummy_device_sonar_set_software_ping (HyScanSonar *sonar)
{
  HyScanDummyDevice *dummy = HYSCAN_DUMMY_DEVICE (sonar);
  HyScanDummyDevicePrivate *priv = dummy->priv;

  priv->command = HYSCAN_DUMMY_DEVICE_COMMAND_SET_SOFTWARE_PING;

  return TRUE;
}

static gboolean
hyscan_dummy_device_sonar_start (HyScanSonar     *sonar,
                                 const gchar     *project_name,
                                 const gchar     *track_name,
                                 HyScanTrackType  track_type)
{
  HyScanDummyDevice *dummy = HYSCAN_DUMMY_DEVICE (sonar);
  HyScanDummyDevicePrivate *priv = dummy->priv;

  priv->project_name = project_name;
  priv->track_name = track_name;
  priv->track_type = track_type;
  priv->command = HYSCAN_DUMMY_DEVICE_COMMAND_START;

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
hyscan_dummy_device_sonar_sync (HyScanSonar *sonar)
{
  HyScanDummyDevice *dummy = HYSCAN_DUMMY_DEVICE (sonar);
  HyScanDummyDevicePrivate *priv = dummy->priv;

  priv->command = HYSCAN_DUMMY_DEVICE_COMMAND_SYNC;

  return TRUE;
}

static gboolean
hyscan_dummy_device_sonar_ping (HyScanSonar *sonar)
{
  HyScanDummyDevice *dummy = HYSCAN_DUMMY_DEVICE (sonar);
  HyScanDummyDevicePrivate *priv = dummy->priv;

  priv->command = HYSCAN_DUMMY_DEVICE_COMMAND_PING;

  return TRUE;
}

static gboolean
hyscan_dummy_device_sonar_disconnect (HyScanSonar *sonar)
{
  HyScanDummyDevice *dummy = HYSCAN_DUMMY_DEVICE (sonar);
  HyScanDummyDevicePrivate *priv = dummy->priv;

  priv->connected = FALSE;
  priv->command = HYSCAN_DUMMY_DEVICE_COMMAND_DISCONNECT;

  return TRUE;
}

static gboolean
hyscan_dummy_device_sensor_set_sound_velocity (HyScanSensor *sensor,
                                               GList        *svp)
{
  HyScanDummyDevice *dummy = HYSCAN_DUMMY_DEVICE (sensor);
  HyScanDummyDevicePrivate *priv = dummy->priv;

  priv->svp = svp;
  priv->command = HYSCAN_DUMMY_DEVICE_COMMAND_SET_SOUND_VELOCITY;

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
hyscan_dummy_device_sensor_disconnect (HyScanSensor *sensor)
{
  HyScanDummyDevice *dummy = HYSCAN_DUMMY_DEVICE (sensor);
  HyScanDummyDevicePrivate *priv = dummy->priv;

  priv->connected = FALSE;
  priv->command = HYSCAN_DUMMY_DEVICE_COMMAND_DISCONNECT;

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
  g_hash_table_replace (priv->params, g_strdup (key_id), (gpointer)HYSCAN_DEVICE_SCHEMA_STATUS_OK);

  g_signal_emit_by_name (dummy, "device-state", priv->device_id);
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
      hyscan_buffer_wrap_data (data, HYSCAN_DATA_STRING, sdata, strlen (sdata) + 1);

      g_signal_emit_by_name (dummy, "sensor-data", sensor, HYSCAN_SOURCE_NMEA_ANY, time, data);

      g_free (sdata);
    }

  for (i = 0; hyscan_dummy_device_sources[i] != HYSCAN_SOURCE_INVALID; i++)
    {
      HyScanSourceType source;
      HyScanAcousticDataInfo *info;
      HyScanComplexFloat *cdata;
      gfloat *fdata;
      guint32 n_points;
      gint64 time;

      source = hyscan_dummy_device_sources[i];
      info = hyscan_dummy_device_get_acoustic_info (source);

      cdata = hyscan_dummy_device_get_complex_float_data (source, &n_points, &time);
      hyscan_buffer_wrap_data (data, HYSCAN_DATA_COMPLEX_FLOAT32LE, cdata, n_points * sizeof (HyScanComplexFloat));
      g_signal_emit_by_name (dummy, "sonar-signal", source, 1, time, data);
      g_signal_emit_by_name (dummy, "sonar-acoustic-data", source, 1, FALSE, time, info, data);

      fdata = hyscan_dummy_device_get_float_data (source, &n_points, &time);
      hyscan_buffer_wrap_data (data, HYSCAN_DATA_FLOAT32LE, fdata, n_points * sizeof (gfloat));
      g_signal_emit_by_name (dummy, "sonar-tvg", source, 1, time, data);

      hyscan_acoustic_data_info_free (info);
      g_free (cdata);
      g_free (fdata);
    }

  if (dummy->priv->type == HYSCAN_DUMMY_DEVICE_SIDE_SCAN)
    {
      gint64 time = 0;
      g_signal_emit_by_name (dummy, "sonar-log", "side-scan", time, HYSCAN_LOG_LEVEL_INFO, "sonar-log");
      g_signal_emit_by_name (dummy, "sensor-log", "side-scan", time, HYSCAN_LOG_LEVEL_INFO, "sensor-log");
    }
  else if (dummy->priv->type == HYSCAN_DUMMY_DEVICE_PROFILER)
    {
      gint64 time = 0;
      g_signal_emit_by_name (dummy, "sonar-log", "profiler", time, HYSCAN_LOG_LEVEL_INFO, "sonar-log");
      g_signal_emit_by_name (dummy, "sensor-log", "profiler", time, HYSCAN_LOG_LEVEL_INFO, "sensor-log");
    }

  dummy->priv->command = HYSCAN_DUMMY_DEVICE_COMMAND_INVALID;

  g_object_unref (data);
}

/**
 * hyscan_dummy_device_check_sound_velocity:
 * @dummy: указатель на #HyScanDummyDevice
 * @svp: профиль скорости звука
 *
 * Функция проверяет параметры функций #hyscan_sonar_set_sound_velocity
 * и #hyscan_sensor_set_sound_velocity.
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
 * hyscan_dummy_device_check_generator_auto:
 * @dummy: указатель на #HyScanDummyDevice
 * @signal: тип сигнала
 *
 * Функция проверяет параметры функций #hyscan_sonar_generator_set_auto.
 */
gboolean
hyscan_dummy_device_check_generator_auto (HyScanDummyDevice              *dummy,
                                          HyScanSonarGeneratorSignalType  signal)
{
  HyScanDummyDevicePrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_DUMMY_DEVICE (dummy), FALSE);

  priv = dummy->priv;

  if (priv->command != HYSCAN_DUMMY_DEVICE_COMMAND_GENERATOR_SET_AUTO)
    return FALSE;

  if (priv->generator_signal != signal)
    return FALSE;

  priv->command = HYSCAN_DUMMY_DEVICE_COMMAND_INVALID;
  priv->generator_signal = 0;

  return TRUE;
}

/**
 * hyscan_dummy_device_check_generator_simple:
 * @dummy: указатель на #HyScanDummyDevice
 * @signal: тип сигнала
 * @power: энергия сигнала
 *
 * Функция проверяет параметры функций #hyscan_sonar_generator_set_simple.
 */
gboolean
hyscan_dummy_device_check_generator_simple (HyScanDummyDevice              *dummy,
                                            HyScanSonarGeneratorSignalType  signal,
                                            gdouble                         power)
{
  HyScanDummyDevicePrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_DUMMY_DEVICE (dummy), FALSE);

  priv = dummy->priv;

  if (priv->command != HYSCAN_DUMMY_DEVICE_COMMAND_GENERATOR_SET_SIMPLE)
    return FALSE;

  if ((priv->generator_signal != signal) ||
      (priv->generator_power != power))
    {
      return FALSE;
    }

  priv->command = HYSCAN_DUMMY_DEVICE_COMMAND_INVALID;
  priv->generator_signal = 0;
  priv->generator_power = 0.0;

  return TRUE;
}

/**
 * hyscan_dummy_device_check_generator_extended:
 * @dummy: указатель на #HyScanDummyDevice
 * @signal: тип сигнала
 * @duration: длительность сигнала
 * @power: энергия сигнала
 *
 * Функция проверяет параметры функций #hyscan_sonar_generator_set_extended.
 */
gboolean
hyscan_dummy_device_check_generator_extended (HyScanDummyDevice              *dummy,
                                              HyScanSonarGeneratorSignalType  signal,
                                              gdouble                         duration,
                                              gdouble                         power)
{
  HyScanDummyDevicePrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_DUMMY_DEVICE (dummy), FALSE);

  priv = dummy->priv;

  if (priv->command != HYSCAN_DUMMY_DEVICE_COMMAND_GENERATOR_SET_EXTENDED)
    return FALSE;

  if ((priv->generator_signal != signal) ||
      (priv->generator_duration != duration) ||
      (priv->generator_power != power))
    {
      return FALSE;
    }

  priv->command = HYSCAN_DUMMY_DEVICE_COMMAND_INVALID;
  priv->generator_signal = 0;
  priv->generator_duration = 0.0;
  priv->generator_power = 0.0;

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
 * hyscan_dummy_device_check_tvg_auto:
 * @dummy: указатель на #HyScanDummyDevice
 * @time_step: интервал времени между точками
 * @gains: массив коэффициентов усиления
 * @n_gains: число коэффициентов усиления
 *
 * Функция проверяет параметры функций #hyscan_sonar_tvg_set_points.
 */
gboolean
hyscan_dummy_device_check_tvg_points (HyScanDummyDevice *dummy,
                                      gdouble            time_step,
                                      const gdouble     *gains,
                                      guint32            n_gains)
{
  HyScanDummyDevicePrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_DUMMY_DEVICE (dummy), FALSE);

  priv = dummy->priv;

  if (priv->command != HYSCAN_DUMMY_DEVICE_COMMAND_TVG_SET_POINTS)
    return FALSE;

  if ((priv->tvg_time_step != time_step) ||
      (priv->tvg_gains != gains) ||
      (priv->tvg_n_gains != n_gains))
    {
      return FALSE;
    }

  priv->command = HYSCAN_DUMMY_DEVICE_COMMAND_INVALID;
  priv->tvg_time_step = 0.0;
  priv->tvg_gains = NULL;
  priv->tvg_n_gains = 0;

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
 * hyscan_dummy_device_check_software_ping:
 * @dummy: указатель на #HyScanDummyDevice
 *
 * Функция проверяет параметры функций #hyscan_sonar_set_software_ping.
 */
gboolean
hyscan_dummy_device_check_software_ping (HyScanDummyDevice *dummy)
{
  HyScanDummyDevicePrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_DUMMY_DEVICE (dummy), FALSE);

  priv = dummy->priv;

  if (priv->command != HYSCAN_DUMMY_DEVICE_COMMAND_SET_SOFTWARE_PING)
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
 *
 * Функция проверяет параметры функций #hyscan_sonar_start.
 */
gboolean
hyscan_dummy_device_check_start (HyScanDummyDevice *dummy,
                                 const gchar       *project_name,
                                 const gchar       *track_name,
                                 HyScanTrackType    track_type)
{
  HyScanDummyDevicePrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_DUMMY_DEVICE (dummy), FALSE);

  priv = dummy->priv;

  if (priv->command != HYSCAN_DUMMY_DEVICE_COMMAND_START)
    return FALSE;

  if ((priv->project_name != project_name) ||
      (priv->track_name != track_name) ||
      (priv->track_type != track_type))
    {
      return FALSE;
    }

  priv->project_name = NULL;
  priv->track_name = NULL;
  priv->track_type = 0;

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
 * hyscan_dummy_device_check_ping:
 * @dummy: указатель на #HyScanDummyDevice
 *
 * Функция проверяет параметры функций #hyscan_sonar_ping.
 */
gboolean
hyscan_dummy_device_check_ping (HyScanDummyDevice *dummy)
{
  HyScanDummyDevicePrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_DUMMY_DEVICE (dummy), FALSE);

  priv = dummy->priv;

  if (priv->command != HYSCAN_DUMMY_DEVICE_COMMAND_PING)
    return FALSE;

  priv->command = HYSCAN_DUMMY_DEVICE_COMMAND_INVALID;

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
 * hyscan_dummy_device_check_params:
 * @info_id: параметр ветки /info
 * @param_id: параметр ветки /param
 * @system_id: параметр ветки /system
 * @state_id: параметр ветки /state
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
                                  gint32             system_id,
                                  gint32             state_id)
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

  key_id = g_strdup_printf ("/state/%s/id", priv->device_id);
  if (g_hash_table_lookup (priv->params, key_id) != GINT_TO_POINTER (state_id))
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
      (source == HYSCAN_SOURCE_ECHOPROFILER))
    {
      return HYSCAN_DUMMY_DEVICE_PROFILER;
    }

  return HYSCAN_DUMMY_DEVICE_INVALID;
}

/**
 * hyscan_dummy_device_get_sensor_position:
 * @sensor: название датчика
 *
 * Функция возвращает эталонные значения установки местоположения
 * антенн датчика.
 *
 * Returns: (transfer full): Местоположение антенн #HyScanAntennaPosition.
 * Для удаления #hyscan_antenna_position_free.
 */
HyScanAntennaPosition *
hyscan_dummy_device_get_sensor_position (const gchar *sensor)
{
  HyScanAntennaPosition position;
  guint seed = g_str_hash (sensor);

  position.x = 1.0 * seed;
  position.y = 2.0 * seed;
  position.z = 3.0 * seed;
  position.psi = 4.0 * seed;
  position.gamma = 5.0 * seed;
  position.theta = 6.0 * seed;

  return hyscan_antenna_position_copy (&position);
}

/**
 * hyscan_dummy_device_get_source_position:
 * @source: тип гидролокационного источника данных
 *
 * Функция возвращает эталонные значения установки местоположения
 * антенн гидролокационного источника данных.
 *
 * Returns: (transfer full): Местоположение антенн #HyScanAntennaPosition.
 * Для удаления #hyscan_antenna_position_free.
 */
HyScanAntennaPosition *
hyscan_dummy_device_get_source_position (HyScanSourceType source)
{
  HyScanAntennaPosition position;

  position.x = 1.0 * source;
  position.y = 2.0 * source;
  position.z = 3.0 * source;
  position.psi = 4.0 * source;
  position.gamma = 5.0 * source;
  position.theta = 6.0 * source;

  return hyscan_antenna_position_copy (&position);
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
    info.position = hyscan_dummy_device_get_sensor_position (sensor);
  else
    info.position = NULL;

  pinfo = hyscan_sensor_info_sensor_copy (&info);
  hyscan_antenna_position_free (info.position);
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

  HyScanSonarInfoCapabilities capabilities;
  HyScanSonarInfoReceiver receiver;
  HyScanSonarInfoGenerator generator;
  HyScanSonarInfoTVG tvg;

  HyScanSonarInfoSignal tone;
  HyScanSonarInfoSignal lfm;
  GList *presets = NULL;

  guint i;

  /* Название источника данных. */
  source_name = hyscan_source_get_name_by_type (source);

  /* Идентификатор устройства. */
  dev_type = hyscan_dummy_device_get_type_by_source (source);
  if (dev_type == HYSCAN_DUMMY_DEVICE_SIDE_SCAN)
    dev_id = "ss";
  else if (dev_type == HYSCAN_DUMMY_DEVICE_PROFILER)
    dev_id = "pf";
  else
    return NULL;

  /* Возможности источника данных. */
  capabilities.receiver = HYSCAN_SONAR_RECEIVER_MODE_MANUAL |
                          HYSCAN_SONAR_RECEIVER_MODE_AUTO;

  capabilities.generator = HYSCAN_SONAR_GENERATOR_MODE_PRESET |
                           HYSCAN_SONAR_GENERATOR_MODE_AUTO |
                           HYSCAN_SONAR_GENERATOR_MODE_SIMPLE |
                           HYSCAN_SONAR_GENERATOR_MODE_EXTENDED;

  capabilities.tvg = HYSCAN_SONAR_TVG_MODE_AUTO |
                     HYSCAN_SONAR_TVG_MODE_POINTS |
                     HYSCAN_SONAR_TVG_MODE_CONSTANT |
                     HYSCAN_SONAR_TVG_MODE_LINEAR_DB |
                     HYSCAN_SONAR_TVG_MODE_LOGARITHMIC;

  /* Преднастройки генератора. */
  for (i = 0; i < N_PRESETS; i++)
    {
      HyScanDataSchemaEnumValue *preset;
      gchar *name = g_strdup_printf ("%s preset %d", source_name, i + 1);
      gchar *description = g_strdup_printf ("%s description %d", source_name, i + 1);

      preset = hyscan_data_schema_enum_value_new (i, name, description);
      presets = g_list_append (presets, preset);

      g_free (description);
      g_free (name);
    }

  /* Параметры генератора. */
  generator.signals = HYSCAN_SONAR_GENERATOR_SIGNAL_AUTO |
                      HYSCAN_SONAR_GENERATOR_SIGNAL_TONE |
                      HYSCAN_SONAR_GENERATOR_SIGNAL_LFM;
  generator.presets = presets;
  generator.automatic = TRUE;
  generator.tone = &tone;
  generator.lfm = &lfm;

  /* Описание источника данных. */
  info.source = source;
  info.dev_id = dev_id;
  info.description = source_name;
  info.master = HYSCAN_SOURCE_INVALID;
  info.position = NULL;
  info.capabilities = &capabilities;
  info.receiver = &receiver;
  info.generator = &generator;
  info.tvg = &tvg;

  /* Уникальные параметры источников данных. */
  if (source == HYSCAN_SOURCE_SIDE_SCAN_PORT)
    {
      receiver.min_time = 0.01;
      receiver.max_time = 0.1;

      tone.duration_name = "ssp-tone";
      tone.min_duration = 1e-6;
      tone.max_duration = 1e-4;
      tone.duration_step = 2e-6;
      tone.dirty_cycle = 10;

      lfm.duration_name = "ssp-lfm";
      lfm.min_duration = 1e-3;
      lfm.max_duration = 1e-2;
      lfm.duration_step = 2e-3;
      lfm.dirty_cycle = 100;

      tvg.min_gain = -10.0;
      tvg.max_gain = 10.0;
      tvg.decrease = TRUE;
    }
  else if (source == HYSCAN_SOURCE_SIDE_SCAN_STARBOARD)
    {
      receiver.min_time = 0.02;
      receiver.max_time = 0.2;

      tone.duration_name = "sss-tone";
      tone.min_duration = 2e-6;
      tone.max_duration = 2e-4;
      tone.duration_step = 4e-6;
      tone.dirty_cycle = 20;

      lfm.duration_name = "sss-lfm";
      lfm.min_duration = 2e-3;
      lfm.max_duration = 2e-2;
      lfm.duration_step = 4e-3;
      lfm.dirty_cycle = 200;

      tvg.min_gain = -20.0;
      tvg.max_gain = 20.0;
      tvg.decrease = TRUE;
    }
  else if (source == HYSCAN_SOURCE_PROFILER)
    {
      receiver.min_time = 0.03;
      receiver.max_time = 0.3;

      tone.duration_name = "pf-tone";
      tone.min_duration = 3e-6;
      tone.max_duration = 3e-4;
      tone.duration_step = 6e-6;
      tone.dirty_cycle = 30;

      lfm.duration_name = "pf-lfm";
      lfm.min_duration = 3e-3;
      lfm.max_duration = 3e-2;
      lfm.duration_step = 6e-3;
      lfm.dirty_cycle = 300;

      tvg.min_gain = -30.0;
      tvg.max_gain = 30.0;
      tvg.decrease = TRUE;
    }
  else if (source == HYSCAN_SOURCE_ECHOPROFILER)
    {
      receiver.min_time = 0.04;
      receiver.max_time = 0.4;

      tvg.min_gain = -40.0;
      tvg.max_gain = 40.0;
      tvg.decrease = TRUE;

      capabilities.generator = 0;

      info.master = HYSCAN_SOURCE_PROFILER;
      info.generator = NULL;
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
 * hyscan_dummy_device_get_acoustic_info:
 * @source: тип гидролокационного источника данных
 *
 * Функция возвращает эталонные значения параметров акустических данных.
 *
 * Returns: (transfer full): Параметры акустических данных #HyScanAcousticDataInfo.
 * Для удаления #hyscan_acoustic_data_info_free.
 */
HyScanAcousticDataInfo *
hyscan_dummy_device_get_acoustic_info (HyScanSourceType source)
{
  HyScanAcousticDataInfo info;

  info.data_type = HYSCAN_DATA_COMPLEX_FLOAT32LE;
  info.data_rate = 2.0 * source;
  info.signal_frequency = 3.0 * source;
  info.signal_bandwidth = 4.0 * source;
  info.antenna_voffset = 5.0 * source;
  info.antenna_hoffset = 6.0 * source;
  info.antenna_vpattern = 7.0 * source;
  info.antenna_hpattern = 8.0 * source;
  info.antenna_frequency = 9.0 * source;
  info.antenna_bandwidth = 10.0 * source;
  info.adc_vref = 11.0 * source;
  info.adc_offset = 12.0 * source;

  return hyscan_acoustic_data_info_copy (&info);
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
hyscan_dummy_device_sonar_interface_init (HyScanSonarInterface *iface)
{
  iface->set_sound_velocity = hyscan_dummy_device_sonar_set_sound_velocity;
  iface->receiver_set_time = hyscan_dummy_device_sonar_receiver_set_time;
  iface->receiver_set_auto = hyscan_dummy_device_sonar_receiver_set_auto;
  iface->generator_set_preset = hyscan_dummy_device_sonar_generator_set_preset;
  iface->generator_set_auto = hyscan_dummy_device_sonar_generator_set_auto;
  iface->generator_set_simple = hyscan_dummy_device_sonar_generator_set_simple;
  iface->generator_set_extended = hyscan_dummy_device_sonar_generator_set_extended;
  iface->tvg_set_auto = hyscan_dummy_device_sonar_tvg_set_auto;
  iface->tvg_set_points = hyscan_dummy_device_sonar_tvg_set_points;
  iface->tvg_set_constant = hyscan_dummy_device_sonar_tvg_set_constant;
  iface->tvg_set_linear_db = hyscan_dummy_device_sonar_tvg_set_linear_db;
  iface->tvg_set_logarithmic = hyscan_dummy_device_sonar_tvg_set_logarithmic;
  iface->set_software_ping = hyscan_dummy_device_sonar_set_software_ping;
  iface->start = hyscan_dummy_device_sonar_start;
  iface->stop = hyscan_dummy_device_sonar_stop;
  iface->sync = hyscan_dummy_device_sonar_sync;
  iface->ping = hyscan_dummy_device_sonar_ping;
  iface->disconnect = hyscan_dummy_device_sonar_disconnect;
}

static void
hyscan_dummy_device_sensor_interface_init (HyScanSensorInterface *iface)
{
  iface->set_sound_velocity = hyscan_dummy_device_sensor_set_sound_velocity;
  iface->set_enable = hyscan_dummy_device_sensor_set_enable;
  iface->disconnect = hyscan_dummy_device_sensor_disconnect;
}
