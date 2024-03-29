/* hyscan-control.c
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
 * SECTION: hyscan-control
 * @Short_description: класс управления гидролокаторами и датчиками
 * @Title: HyScanControl
 *
 * Класс реализует управление гидролокаторами и датчиками, а также приём и
 * запись данных от них. Класс реализует интерфейсы #HyScanParam, #HyScanSensor
 * и #HyScanSonar. Таким образом он сам является драйвером устройства и может
 * быть зарегистрирован в другом экземпляре класса управления HyScanControl.
 *
 * Создание объекта управления производится с помощью функции
 * #hyscan_control_new. При удалении объекта производится автоматическое
 * отключение от всех устройств с помощью функций #hyscan_sonar_disconnect
 * и #hyscan_sensor_disconnect.
 *
 * Класс имеет возможность управлять несколькими датчиками и гидролокаторами
 * одновременно. Для этого необходимо загрузить драйвер (#HyScanDriver) и
 * выполнить подключение к устройству (#HyScanDiscover#), затем зарегистрировать
 * подключенное устройство с помощью функции #hyscan_control_device_add. При
 * добавлении устройства, необходимо следить чтобы в разных устройствах не было
 * одинаковых датчиков и источников данных. После того, как все устройства
 * добавлены, необходимо вызвать функцию #hyscan_control_device_bind. После
 * её вызова, добавлять новые устройства нельзя. Работать с HyScanControl
 * можно только после вызова #hyscan_control_device_bind.
 *
 * До завершения конфигурирования и если для датчика или гидролокационного
 * источника данных не задано смещение приёмных антенн по умолчанию, их
 * можно задать с помощью функций #hyscan_control_sensor_set_default_offset и
 * #hyscan_control_source_set_default_offset.
 *
 * Через интерфейс #HyScanParam имеется возможность управлять внутренними
 * параметрами устройств и поведением объекта HyScanControl. Эти параметры
 * находятся в ветке "/params/control" схемы данных. Кроме этого, HyScanControl
 * ретранслирует все параметры добавленых устройств из веток "/info", "/params",
 * "/system" и "/state". Подробнее про эти ветки параметров можно прочитать в
 * #HyScanDeviceSchema.
 *
 * Непосредственное управление устройствами осуществляется через интерфейсы
 * #HyScanSensor и #HyScaSonar, реализуемые HyScanControl. Сам HyScanControl
 * содержит вспомогательные функции для определения параметров устройств и
 * задания системы хранения данных.
 *
 * Список устройств, управляемых #HyScanControl, можно получить с помощью
 * функции #hyscan_control_devices_list. В процессе работы, с устройством
 * может быть потеряна связь или возникнет иная не штатная ситуация. При
 * изменении состояния устройства посылается сигнал "device-state". Состояние
 * устройства можно узнать с помощью функции #hyscan_control_device_get_status.
 *
 * Список источников гидролокационных данных можно получить с помощью функции
 * #hyscan_control_sources_list, список датчиков #hyscan_control_sensors_list,
 * а список приводов #hyscan_control_actuators_list.
 *
 * Информацию об источниках данных,датчиках и приводах  можно получить с
 * помощью функций #hyscan_control_source_get_info,
 * #hyscan_control_sensor_get_info и #hyscan_control_actuator_get_info.
 *
 * Функция #hyscan_control_writer_set_db устанавливает систему хранения.
 *
 * Функции #hyscan_control_writer_set_operator_name и
 * #hyscan_control_writer_set_chunk_size аналогичны подобным функциям
 * #HyScanDataWriter.
 *
 * Класс HyScanControl поддерживает работу в многопоточном режиме.
 */

#include "hyscan-control.h"

#include <hyscan-actuator-schema.h>
#include <hyscan-device-driver.h>
#include <hyscan-sensor-schema.h>
#include <hyscan-sensor-driver.h>
#include <hyscan-sonar-schema.h>
#include <hyscan-sonar-driver.h>
#include <hyscan-data-writer.h>

#include <string.h>

typedef struct
{
  HyScanDevice                *device;                         /* Указатель на подключенное устройство. */
  HyScanSensorInfoSensor      *info;                           /* Информация о датчике. */
  HyScanAntennaOffset         *offset;                         /* Смещение антенны. */
  guint                        channel;                        /* Номер приёмного канала. */
} HyScanControlSensorInfo;

typedef struct
{
  HyScanDevice                *device;                         /* Указатель на подключенное устройство. */
  HyScanSonarInfoSource       *info;                           /* Информация об источнике данных. */
  HyScanAntennaOffset         *offset;                         /* Смещение антенны. */
  GHashTable                  *channels;                       /* Список записываемых каналов. */
} HyScanControlSourceInfo;

typedef struct
{
  gchar                       *description;                    /* Описание источника данных. */
  gchar                       *actuator;                       /* Название привода. */
  HyScanAcousticDataInfo       info;                           /* Параметры акустических данных. */
} HyScanControlChannelInfo;

typedef struct
{
  HyScanDevice                *device;                         /* Указатель на подключенное устройство. */
  HyScanActuatorInfoActuator  *info;                           /* Информация о приводе. */
} HyScanControlActuatorInfo;

struct _HyScanControlPrivate
{
  gboolean                     binded;                         /* Признак завершения конфигурирования. */
  GHashTable                  *devices;                        /* Список подключенных устройств. */

  GHashTable                  *sensors;                        /* Список датчиков. */
  GHashTable                  *sources;                        /* Список источников данных. */
  GHashTable                  *actuators;                      /* Список приводов. */

  GHashTable                  *params;                         /* Параметры. */
  HyScanParamList             *list;                           /* Список параметров. */

  GArray                      *devices_list;                   /* Список устройств. */
  GArray                      *sensors_list;                   /* Список датчиков. */
  GArray                      *sources_list;                   /* Список источников данных. */
  GArray                      *actuators_list;                 /* Список приводов. */

  HyScanDataSchema            *schema;                         /* Схема управления устройствами. */
  HyScanDataWriter            *writer;                         /* Объект записи данных. */
  gint64                       log_time;                       /* Метка времени последней записи информационного сообщения. */
  gboolean                     started;                        /* Признак запуска локатора в работу. */

  GMutex                       writer_lock;                    /* Блокировка доступа при записи данных. */
  GMutex                       list_lock;                      /* Блокировка доступа к списку параметров. */
};

static void        hyscan_control_param_interface_init         (HyScanParamInterface           *iface);
static void        hyscan_control_device_interface_init        (HyScanDeviceInterface          *iface);
static void        hyscan_control_sonar_interface_init         (HyScanSonarInterface           *iface);
static void        hyscan_control_sensor_interface_init        (HyScanSensorInterface          *iface);
static void        hyscan_control_actuator_interface_init      (HyScanActuatorInterface        *iface);

static void        hyscan_control_object_constructed           (GObject                        *object);
static void        hyscan_control_object_finalize              (GObject                        *object);

static void        hyscan_control_free_sensor_info             (gpointer                        data);
static void        hyscan_control_free_source_info             (gpointer                        data);
static void        hyscan_control_free_channel_info            (gpointer                        data);
static void        hyscan_control_free_actuator_info           (gpointer                        data);

static void        hyscan_control_create_device_schema         (HyScanControlPrivate           *priv);

static gboolean    hyscan_control_check_acoustic_channel       (HyScanDataWriter               *writer,
                                                                HyScanSourceType                source,
                                                                guint                           channel,
                                                                HyScanControlSourceInfo        *info);

static void        hyscan_control_sensor_data                  (HyScanDevice                   *device,
                                                                const gchar                    *sensor,
                                                                gint                            source,
                                                                gint64                          time,
                                                                HyScanBuffer                   *data,
                                                                HyScanControl                  *control);

static void        hyscan_control_sonar_source_info            (HyScanDevice                   *device,
                                                                gint                            source,
                                                                guint                           channel,
                                                                const gchar                    *description,
                                                                const gchar                    *actuator,
                                                                HyScanAcousticDataInfo         *info,
                                                                HyScanControl                  *control);

static void        hyscan_control_sonar_signal                 (HyScanDevice                   *device,
                                                                gint                            source,
                                                                guint                           channel,
                                                                gint64                          time,
                                                                HyScanBuffer                   *image,
                                                                HyScanControl                  *control);

static void        hyscan_control_sonar_tvg                    (HyScanDevice                   *device,
                                                                gint                            source,
                                                                guint                           channel,
                                                                gint64                          time,
                                                                HyScanBuffer                   *gains,
                                                                HyScanControl                  *control);

static void        hyscan_control_sonar_acoustic_data          (HyScanDevice                   *device,
                                                                gint                            source,
                                                                guint                           channel,
                                                                gboolean                        noise,
                                                                gint64                          time,
                                                                HyScanBuffer                   *data,
                                                                HyScanControl                  *control);

static void        hyscan_control_device_state                 (HyScanDevice                   *device,
                                                                const gchar                    *dev_id,
                                                                HyScanControl                  *control);

static void        hyscan_control_device_log                   (HyScanDevice                   *device,
                                                                const gchar                    *source,
                                                                gint64                          time,
                                                                gint                            level,
                                                                const gchar                    *message,
                                                                HyScanControl                  *control);

static HyScanDataSchema *hyscan_control_param_schema           (HyScanParam                    *param);

static gboolean    hyscan_control_param_set                    (HyScanParam                    *param,
                                                                HyScanParamList                *list);

static gboolean    hyscan_control_param_get                    (HyScanParam                    *param,
                                                                HyScanParamList                *list);

static gboolean    hyscan_control_device_sync                  (HyScanDevice                   *device);

static gboolean    hyscan_control_device_set_sound_velocity    (HyScanDevice                   *device,
                                                                GList                          *svp);

static gboolean    hyscan_control_device_disconnect            (HyScanDevice                   *device);

static gboolean    hyscan_control_sonar_antenna_set_offset     (HyScanSonar                    *sonar,
                                                                HyScanSourceType                source,
                                                                const HyScanAntennaOffset      *offset);

static gboolean    hyscan_control_sonar_receiver_set_time      (HyScanSonar                    *sonar,
                                                                HyScanSourceType                source,
                                                                gdouble                         receive_time,
                                                                gdouble                         wait_time);

static gboolean    hyscan_control_sonar_receiver_set_auto      (HyScanSonar                    *sonar,
                                                                HyScanSourceType                source);

static gboolean    hyscan_control_sonar_generator_set_preset   (HyScanSonar                    *sonar,
                                                                HyScanSourceType                source,
                                                                gint64                          preset);

static gboolean    hyscan_control_sonar_tvg_set_auto           (HyScanSonar                    *sonar,
                                                                HyScanSourceType                source,
                                                                gdouble                         level,
                                                                gdouble                         sensitivity);

static gboolean    hyscan_control_sonar_tvg_set_linear_db      (HyScanSonar                    *sonar,
                                                                HyScanSourceType                source,
                                                                gdouble                         gain0,
                                                                gdouble                         gain_step);

static gboolean    hyscan_control_sonar_tvg_set_logarithmic    (HyScanSonar                    *sonar,
                                                                HyScanSourceType                source,
                                                                gdouble                         gain0,
                                                                gdouble                         beta,
                                                                gdouble                         alpha);

static gboolean    hyscan_control_sonar_start                  (HyScanSonar                    *sonar,
                                                                const gchar                    *project_name,
                                                                const gchar                    *track_name,
                                                                HyScanTrackType                 track_type,
                                                                const HyScanTrackPlan          *track_plan);

static gboolean    hyscan_control_sonar_stop                   (HyScanSonar                    *sonar);

static gboolean    hyscan_control_sensor_antenna_set_offset    (HyScanSensor                   *sensor,
                                                                const gchar                    *name,
                                                                const HyScanAntennaOffset      *offset);

static gboolean    hyscan_control_sensor_set_enable            (HyScanSensor                   *sensor,
                                                                const gchar                    *name,
                                                                gboolean                        enable);

static gboolean    hyscan_control_actuator_disable             (HyScanActuator                 *actuator,
                                                                const gchar                    *name);

static gboolean    hyscan_control_actuator_scan                (HyScanActuator                 *actuator,
                                                                const gchar                    *name,
                                                                gdouble                         from,
                                                                gdouble                         to,
                                                                gdouble                         speed);

static gboolean    hyscan_control_actuator_manual              (HyScanActuator                 *actuator,
                                                                const gchar                    *name,
                                                                gdouble                         angle);

G_DEFINE_TYPE_WITH_CODE (HyScanControl, hyscan_control, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (HyScanControl)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_PARAM,    hyscan_control_param_interface_init)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_DEVICE,   hyscan_control_device_interface_init)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_SONAR,    hyscan_control_sonar_interface_init)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_SENSOR,   hyscan_control_sensor_interface_init)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_ACTUATOR, hyscan_control_actuator_interface_init))

static void
hyscan_control_class_init (HyScanControlClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = hyscan_control_object_constructed;
  object_class->finalize = hyscan_control_object_finalize;
}

static void
hyscan_control_init (HyScanControl *control)
{
  control->priv = hyscan_control_get_instance_private (control);
}

static void
hyscan_control_object_constructed (GObject *object)
{
  HyScanControl *control = HYSCAN_CONTROL (object);
  HyScanControlPrivate *priv = control->priv;

  G_OBJECT_CLASS (hyscan_control_parent_class)->constructed (object);

  g_mutex_init (&priv->writer_lock);
  g_mutex_init (&priv->list_lock);

  priv->devices   = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                           g_object_unref);
  priv->sensors   = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
                                           hyscan_control_free_sensor_info);
  priv->sources   = g_hash_table_new_full (NULL, NULL, NULL,
                                           hyscan_control_free_source_info);
  priv->actuators = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
                                           hyscan_control_free_actuator_info);
  priv->params    = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  priv->devices_list = g_array_new (TRUE, TRUE, sizeof (gchar*));
  priv->sources_list = g_array_new (TRUE, TRUE, sizeof (HyScanSourceType));
  priv->sensors_list = g_array_new (TRUE, TRUE, sizeof (gchar*));
  priv->actuators_list = g_array_new (TRUE, TRUE, sizeof (gchar*));

  priv->list = hyscan_param_list_new ();

  priv->writer = hyscan_data_writer_new ();
}

static void
hyscan_control_object_finalize (GObject *object)
{
  HyScanControl *control = HYSCAN_CONTROL (object);
  HyScanControlPrivate *priv = control->priv;

  GHashTableIter iter;
  gpointer key, value;

  hyscan_control_device_disconnect (HYSCAN_DEVICE (control));

  g_hash_table_iter_init (&iter, priv->devices);
  while (g_hash_table_iter_next (&iter, &key, &value))
    g_signal_handlers_disconnect_by_data (value, control);

  g_array_unref (priv->devices_list);
  g_array_unref (priv->sources_list);
  g_array_unref (priv->sensors_list);
  g_array_unref (priv->actuators_list);

  g_hash_table_unref (priv->params);
  g_hash_table_unref (priv->sources);
  g_hash_table_unref (priv->sensors);
  g_hash_table_unref (priv->actuators);
  g_hash_table_unref (priv->devices);

  g_clear_object (&priv->schema);
  g_object_unref (priv->writer);
  g_object_unref (priv->list);

  g_mutex_clear (&priv->writer_lock);
  g_mutex_clear (&priv->list_lock);

  G_OBJECT_CLASS (hyscan_control_parent_class)->finalize (object);
}

/* Функция освобождает память занятую структурой HyScanControlSensorInfo */
static void
hyscan_control_free_sensor_info (gpointer data)
{
  HyScanControlSensorInfo *info = data;

  hyscan_sensor_info_sensor_free (info->info);
  hyscan_antenna_offset_free (info->offset);
  g_slice_free (HyScanControlSensorInfo, info);
}

/* Функция освобождает память занятую структурой HyScanControlSourceInfo */
static void
hyscan_control_free_source_info (gpointer data)
{
  HyScanControlSourceInfo *info = data;

  hyscan_sonar_info_source_free (info->info);
  hyscan_antenna_offset_free (info->offset);
  g_hash_table_unref (info->channels);
  g_slice_free (HyScanControlSourceInfo, info);
}

/* Функция освобождает память занятую структурой HyScanControlChannelInfo */
static void
hyscan_control_free_channel_info (gpointer data)
{
  HyScanControlChannelInfo *info = data;

  g_free (info->description);
  g_free (info->actuator);
  g_slice_free (HyScanControlChannelInfo, info);
}

/* Функция освобождает память занятую структурой HyScanControlActuatorInfo */
static void
hyscan_control_free_actuator_info (gpointer data)
{
  HyScanControlActuatorInfo *info = data;

  hyscan_actuator_info_actuator_free (info->info);
  g_slice_free (HyScanControlActuatorInfo, info);
}

/* функция создаёт схему устройства. */
static void
hyscan_control_create_device_schema (HyScanControlPrivate *priv)
{
  HyScanDataSchemaBuilder *builder;
  HyScanDeviceSchema *device;
  HyScanSensorSchema *sensor;
  HyScanSonarSchema *sonar;
  HyScanActuatorSchema *actuator;
  gchar *data;

  GHashTableIter iter;
  gpointer key, value;

  if ((priv->schema != NULL) || priv->binded)
    return;

  device = hyscan_device_schema_new (HYSCAN_DEVICE_SCHEMA_VERSION);
  sensor = hyscan_sensor_schema_new (device);
  sonar = hyscan_sonar_schema_new (device);
  actuator = hyscan_actuator_schema_new (device);
  builder = HYSCAN_DATA_SCHEMA_BUILDER (device);

  /* Добавляем датчики. */
  g_hash_table_iter_init (&iter, priv->sensors);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      HyScanControlSensorInfo *info = value;

      if (!hyscan_sensor_schema_add_full (sensor, info->info))
        goto exit;

      if (info->info->offset != NULL)
        hyscan_data_writer_sensor_set_offset (priv->writer, info->info->name, info->info->offset);
    }

  /* Добавляем источники данных. */
  g_hash_table_iter_init (&iter, priv->sources);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      HyScanControlSourceInfo *info = value;

      if (!hyscan_sonar_schema_source_add_full (sonar, info->info))
        goto exit;

      if (info->info->offset != NULL)
        hyscan_data_writer_sonar_set_offset (priv->writer, info->info->source, info->info->offset);
    }

  /* Добавляем приводы. */
  g_hash_table_iter_init (&iter, priv->actuators);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      HyScanControlActuatorInfo *info = value;

      if (!hyscan_actuator_schema_add_full (actuator, info->info))
        goto exit;
    }

  /* Параметры устройств. */
  g_hash_table_iter_init (&iter, priv->devices);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      HyScanDataSchema *schema;
      const gchar * const *keys;
      guint i;

      schema = hyscan_param_schema (HYSCAN_PARAM (value));
      keys = hyscan_data_schema_list_keys (schema);

      /* Переносим ветки /info, /params, /system и /state в нашу новую схему. */
      hyscan_data_schema_builder_schema_join (builder, "/info", schema, "/info");
      hyscan_data_schema_builder_schema_join (builder, "/params", schema, "/params");
      hyscan_data_schema_builder_schema_join (builder, "/system", schema, "/system");
      hyscan_data_schema_builder_schema_join (builder, "/state", schema, "/state");

      /* Запоминаем какой драйвер обрабатывает эти параметры. */
      for (i = 0; keys[i] != NULL; i++)
        {
          if (g_str_has_prefix (keys[i], "/info/") ||
              g_str_has_prefix (keys[i], "/params/") ||
              g_str_has_prefix (keys[i], "/system/") ||
              g_str_has_prefix (keys[i], "/state/"))
            {
              g_hash_table_insert (priv->params, g_strdup (keys[i]), value);
            }
        }

      g_object_unref (schema);
    }

  /* Создаём схему. */
  priv->schema = hyscan_data_schema_builder_get_schema (builder);
  g_object_unref (builder);

  /* Информация об устройствах для записи в галс. */
  builder = hyscan_data_schema_builder_new ("info");
  hyscan_data_schema_builder_schema_join (builder, "/info", priv->schema, "/info");
  hyscan_data_schema_builder_schema_join (builder, "/sources", priv->schema, "/sources");
  hyscan_data_schema_builder_schema_join (builder, "/sensors", priv->schema, "/sensors");
  hyscan_data_schema_builder_schema_join (builder, "/actuators", priv->schema, "/actuators");
  data = hyscan_data_schema_builder_get_data (builder);
  hyscan_data_writer_set_sonar_info (priv->writer, data);
  g_free (data);

exit:
  g_object_unref (sonar);
  g_object_unref (sensor);
  g_object_unref (builder);
}

/* Функция, при необходимости, создаёт канал для записи акустических данных. */
static gboolean
hyscan_control_check_acoustic_channel (HyScanDataWriter        *writer,
                                       HyScanSourceType         source,
                                       guint                    channel,
                                       HyScanControlSourceInfo *info)
{
  HyScanControlChannelInfo *channel_info;

  channel_info = g_hash_table_lookup (info->channels, GINT_TO_POINTER (channel));
  if (channel_info == NULL)
    return FALSE;

  if (!hyscan_data_writer_acoustic_is_created (writer, source, channel))
    {
      if (!hyscan_data_writer_acoustic_create (writer, source, channel,
                                               channel_info->description,
                                               channel_info->actuator,
                                               &channel_info->info))
        {
          g_warning ("HyScanControl: can't create channel for %s",
                     hyscan_source_get_id_by_type (source));

          return FALSE;
        }
    }

  return TRUE;
}

/* Обработчик сигнала sensor-data. */
static void
hyscan_control_sensor_data (HyScanDevice  *device,
                            const gchar   *sensor,
                            gint           source,
                            gint64         time,
                            HyScanBuffer  *data,
                            HyScanControl *control)
{
  HyScanControlPrivate *priv = control->priv;
  HyScanControlSensorInfo *sensor_info;

  if (!g_atomic_int_get (&priv->binded))
    return;

  sensor_info = g_hash_table_lookup (priv->sensors, sensor);
  if ((sensor_info == NULL) || (sensor_info->device != device))
    return;

  g_mutex_lock (&priv->writer_lock);
  if (priv->started)
    {
      gboolean status;

      status = hyscan_data_writer_sensor_add_data (priv->writer,
                                                   sensor, source, sensor_info->channel,
                                                   time, data);
      if (!status)
        g_warning ("HyScanControl: can't add sensor data for %s", sensor);
    }
  g_mutex_unlock (&priv->writer_lock);

  hyscan_sensor_driver_send_data (control, sensor, source, time, data);
}

/* Обработчик сигнала sonar-source-info. */
static void
hyscan_control_sonar_source_info (HyScanDevice                   *device,
                                  gint                            source,
                                  guint                           channel,
                                  const gchar                    *description,
                                  const gchar                    *actuator,
                                  HyScanAcousticDataInfo         *info,
                                  HyScanControl                  *control)
{
  HyScanControlPrivate *priv = control->priv;
  HyScanControlSourceInfo *source_info;
  HyScanControlChannelInfo *channel_info;

  if (!g_atomic_int_get (&priv->binded))
    return;

  source_info = g_hash_table_lookup (priv->sources, GINT_TO_POINTER (source));
  if ((source_info == NULL) || (source_info->device != device))
    return;

  g_mutex_lock (&priv->writer_lock);

  channel_info = g_hash_table_lookup (source_info->channels, GINT_TO_POINTER (channel));
  if (channel_info == NULL)
    {
      channel_info = g_slice_new0 (HyScanControlChannelInfo);
      g_hash_table_insert (source_info->channels, GINT_TO_POINTER (channel), channel_info);
    }
  else
    {
      g_free (channel_info->description);
      g_free (channel_info->actuator);
    }

  channel_info->description = g_strdup (description);
  channel_info->actuator = g_strdup (actuator);
  channel_info->info = *info;

  g_mutex_unlock (&priv->writer_lock);

  hyscan_sonar_driver_send_source_info (control, source, channel, description, actuator, info);
}

/* Обработчик сигнала sonar-signal. */
static void
hyscan_control_sonar_signal (HyScanDevice  *device,
                             gint           source,
                             guint          channel,
                             gint64         time,
                             HyScanBuffer  *image,
                             HyScanControl *control)
{
  HyScanControlPrivate *priv = control->priv;
  HyScanControlSourceInfo *source_info;

  if (!g_atomic_int_get (&priv->binded))
    return;

  source_info = g_hash_table_lookup (priv->sources, GINT_TO_POINTER (source));
  if ((source_info == NULL) || (source_info->device != device))
    return;

  g_mutex_lock (&priv->writer_lock);

  if (priv->started)
    {
      gboolean status;

      status = hyscan_control_check_acoustic_channel (priv->writer,
                                                      source, channel,
                                                      source_info);
      if (!status)
        goto exit;

      status = hyscan_data_writer_acoustic_add_signal (priv->writer,
                                                       source, channel,
                                                       time, image);
      if (!status)
        {
          g_warning ("HyScanControl: can't set signal image for %s",
                     hyscan_source_get_id_by_type (source));
        }
    }

exit:
  g_mutex_unlock (&priv->writer_lock);

  hyscan_sonar_driver_send_signal (control, source, channel, time, image);
}

/* Обработчик сигнала sonar-tvg. */
static void
hyscan_control_sonar_tvg (HyScanDevice  *device,
                          gint           source,
                          guint          channel,
                          gint64         time,
                          HyScanBuffer  *gains,
                          HyScanControl *control)
{
  HyScanControlPrivate *priv = control->priv;
  HyScanControlSourceInfo *source_info;

  if (!g_atomic_int_get (&priv->binded))
    return;

  source_info = g_hash_table_lookup (priv->sources, GINT_TO_POINTER (source));
  if ((source_info == NULL) || (source_info->device != device))
    return;

  g_mutex_lock (&priv->writer_lock);

  if (priv->started)
    {
      gboolean status;

      status = hyscan_control_check_acoustic_channel (priv->writer,
                                                      source, channel,
                                                      source_info);
      if (!status)
        goto exit;


      status = hyscan_data_writer_acoustic_add_tvg (priv->writer,
                                                    source, channel,
                                                    time, gains);
      if (!status)
        {
          g_warning ("HyScanControl: can't set tvg for %s",
                     hyscan_source_get_id_by_type (source));
        }
    }

exit:
  g_mutex_unlock (&priv->writer_lock);

  hyscan_sonar_driver_send_tvg (control, source, channel, time, gains);
}

/* Обработчик сигнала sonar-acoustic-data. */
static void
hyscan_control_sonar_acoustic_data (HyScanDevice           *device,
                                    gint                    source,
                                    guint                   channel,
                                    gboolean                noise,
                                    gint64                  time,
                                    HyScanBuffer           *data,
                                    HyScanControl          *control)
{
  HyScanControlPrivate *priv = control->priv;
  HyScanControlSourceInfo *source_info;

  if (!g_atomic_int_get (&priv->binded))
    return;

  source_info = g_hash_table_lookup (priv->sources, GINT_TO_POINTER (source));
  if ((source_info == NULL) || (source_info->device != device))
    return;

  g_mutex_lock (&priv->writer_lock);

  if (priv->started)
    {
      gboolean status;

      status = hyscan_control_check_acoustic_channel (priv->writer,
                                                      source, channel,
                                                      source_info);
      if (!status)
        goto exit;

      status = hyscan_data_writer_acoustic_add_data (priv->writer,
                                                     source, channel, noise,
                                                     time, data);
      if (!status)
        {
          g_warning ("HyScanControl: can't add acoustic data for %s",
                     hyscan_source_get_id_by_type (source));
        }
    }

exit:
  g_mutex_unlock (&priv->writer_lock);

  hyscan_sonar_driver_send_acoustic_data (control, source, channel, noise, time, data);
}

/* Обработчик сигнала device-state. */
static void
hyscan_control_device_state (HyScanDevice     *device,
                             const gchar      *dev_id,
                             HyScanControl    *control)
{
  hyscan_device_driver_send_state (control, dev_id);
}

/* Обработчик сигнала device-log. */
static void
hyscan_control_device_log (HyScanDevice  *device,
                           const gchar   *source,
                           gint64         time,
                           gint           level,
                           const gchar   *message,
                           HyScanControl *control)
{
  HyScanControlPrivate *priv = control->priv;

  if (!g_atomic_int_get (&priv->binded))
    return;

  g_mutex_lock (&priv->writer_lock);

  /* Если несколько сообщений приходит одновременно, разносим их на 1 мкс. */
  if (time <= priv->log_time)
    time = priv->log_time + 1;

  priv->log_time = time;

  hyscan_data_writer_log_add_message (priv->writer, source, time, level, message);

  g_mutex_unlock (&priv->writer_lock);

  hyscan_device_driver_send_log (control, source, time, level, message);
}

/* Метод HyScanParam->schema. */
static HyScanDataSchema *
hyscan_control_param_schema (HyScanParam *param)
{
  HyScanControl *control = HYSCAN_CONTROL (param);
  HyScanControlPrivate *priv = control->priv;

  if (!g_atomic_int_get (&priv->binded))
    return NULL;

  return g_object_ref (priv->schema);
}

/* Метод HyScanParam->set. */
static gboolean
hyscan_control_param_set (HyScanParam     *param,
                          HyScanParamList *list)
{
  HyScanControl *control = HYSCAN_CONTROL (param);
  HyScanControlPrivate *priv = control->priv;

  const gchar * const *params_in;
  gboolean status = TRUE;

  GHashTableIter iter;
  gpointer device;
  guint i;

  if (!g_atomic_int_get (&priv->binded))
    return FALSE;

  /* Список параметров. */
  params_in = hyscan_param_list_params (list);
  if (params_in == NULL)
    return FALSE;

  /* Проверяем что нам известны все параметры. */
  for (i = 0; params_in[i] != NULL; i++)
    {
      if (!g_hash_table_contains (priv->params, params_in[i]))
        return FALSE;
    }

  g_mutex_lock (&priv->list_lock);

  /* Просматриваем все драйверы. */
  g_hash_table_iter_init (&iter, priv->devices);
  while (g_hash_table_iter_next (&iter, NULL, &device))
    {
      gboolean is_params = FALSE;

      /* Ищем какие параметры относятся к этому драйверу. */
      hyscan_param_list_clear (priv->list);
      for (i = 0; params_in[i] != NULL; i++)
        {
          if (g_hash_table_lookup (priv->params, params_in[i]) == device)
            {
              GVariant *value = hyscan_param_list_get (list, params_in[i]);
              hyscan_param_list_set (priv->list, params_in[i], value);
              g_clear_pointer (&value, g_variant_unref);
              is_params = TRUE;
            }
        }

      /* Нет параметров обрабатываемых этим драйвером. */
      if (!is_params)
        continue;

      /* Передаём параметры в устройство. */
      if (!hyscan_param_set (device, priv->list))
        status = FALSE;
    }

  hyscan_param_list_clear (priv->list);

  g_mutex_unlock (&priv->list_lock);

  return status;
}

/* Метод HyScanParam->get. */
static gboolean
hyscan_control_param_get (HyScanParam     *param,
                          HyScanParamList *list)
{
  HyScanControl *control = HYSCAN_CONTROL (param);
  HyScanControlPrivate *priv = control->priv;

  const gchar * const *params_in;
  gboolean status = TRUE;

  GHashTableIter iter;
  gpointer device;
  guint i;

  if (!g_atomic_int_get (&priv->binded))
    return FALSE;

  /* Список параметров. */
  params_in = hyscan_param_list_params (list);
  if (params_in == NULL)
    return FALSE;

  /* Проверяем что нам известны все параметры. */
  for (i = 0; params_in[i] != NULL; i++)
    {
      if (!g_hash_table_contains (priv->params, params_in[i]))
        return FALSE;
    }

  g_mutex_lock (&priv->list_lock);

  /* Просматриваем все драйверы. */
  g_hash_table_iter_init (&iter, priv->devices);
  while (g_hash_table_iter_next (&iter, NULL, &device))
    {
      const gchar * const *params_out;
      gboolean is_params = FALSE;

      /* Ищем какие параметры относятся к этому драйверу. */
      hyscan_param_list_clear (priv->list);
      for (i = 0; params_in[i] != NULL; i++)
        {
          if (g_hash_table_lookup (priv->params, params_in[i]) == device)
            {
              hyscan_param_list_add (priv->list, params_in[i]);
              is_params = TRUE;
            }
        }

      /* Нет параметров обрабатываемых этим драйвером. */
      if (!is_params)
        continue;

      /* Запрашиваем параметры из устройства. */
      if (!hyscan_param_get (device, priv->list))
        status = FALSE;

      /* Передаём результат обратно. */
      params_out = hyscan_param_list_params (priv->list);
      for (i = 0; params_out[i] != NULL; i++)
        {
          GVariant *value = hyscan_param_list_get (priv->list, params_out[i]);
          hyscan_param_list_set (list, params_out[i], value);
          g_clear_pointer (&value, g_variant_unref);
        }
    }

  g_mutex_unlock (&priv->list_lock);

  return status;
}


/* Метод HyScanDevice->sync. */
static gboolean
hyscan_control_device_sync (HyScanDevice *device)
{
  HyScanControl *control = HYSCAN_CONTROL (device);
  HyScanControlPrivate *priv = control->priv;
  gboolean status = TRUE;

  if (g_atomic_int_get (&priv->binded))
    {
      GHashTableIter iter;
      gpointer device;

      g_hash_table_iter_init (&iter, priv->devices);
      while (g_hash_table_iter_next (&iter, NULL, &device))
        {
          if (!hyscan_device_sync (device))
            status = FALSE;
        }
    }

  return status;
}

/* Метод HyScanDevice->set_sound_velocity. */
static gboolean
hyscan_control_device_set_sound_velocity (HyScanDevice *device,
                                          GList        *svp)
{
  HyScanControl *control = HYSCAN_CONTROL (device);
  HyScanControlPrivate *priv = control->priv;
  gboolean status = TRUE;

  if (g_atomic_int_get (&priv->binded))
    {
      GHashTableIter iter;
      gpointer device;

      g_hash_table_iter_init (&iter, priv->devices);
      while (g_hash_table_iter_next (&iter, NULL, &device))
        {
          if (!hyscan_device_set_sound_velocity (device, svp))
            status = FALSE;
        }
    }

  return status;
}

/* Метод HyScanDevice->disconnect. */
static gboolean
hyscan_control_device_disconnect (HyScanDevice *device)
{
  HyScanControl *control = HYSCAN_CONTROL (device);
  HyScanControlPrivate *priv = control->priv;
  gboolean status = TRUE;

  if (g_atomic_int_compare_and_exchange (&priv->binded, TRUE, FALSE))
    {
      GHashTableIter iter;
      gpointer device;

      g_hash_table_iter_init (&iter, priv->devices);
      while (g_hash_table_iter_next (&iter, NULL, &device))
        {
          if (!hyscan_device_disconnect (device))
            status = FALSE;
        }
    }

  return status;
}

/* Метод HyScanSonar->antenna_set_offset. */
static gboolean
hyscan_control_sonar_antenna_set_offset (HyScanSonar               *sonar,
                                         HyScanSourceType           source,
                                         const HyScanAntennaOffset *offset)
{
  HyScanControl *control = HYSCAN_CONTROL (sonar);
  HyScanControlPrivate *priv = control->priv;
  HyScanControlSourceInfo *source_info;
  gboolean status = FALSE;

  g_return_val_if_fail (HYSCAN_IS_CONTROL (control), FALSE);

  priv = control->priv;

  if (!g_atomic_int_get (&priv->binded))
    return FALSE;

  g_mutex_lock (&priv->writer_lock);

  /* Если нет источника или задано смещение по умолчанию - выходим. */
  source_info = g_hash_table_lookup (priv->sources, GINT_TO_POINTER (source));
  if ((source_info == NULL) || (source_info->info->offset != NULL))
    goto exit;

  /* Запоминаем новое смещение. */
  hyscan_antenna_offset_free (source_info->offset);
  source_info->offset = hyscan_antenna_offset_copy (offset);
  hyscan_data_writer_sonar_set_offset (priv->writer, source, offset);

  /* Передаём новые значения в драйвер. */
  status = hyscan_sonar_antenna_set_offset (HYSCAN_SONAR (source_info->device),
                                            source, offset);

exit:
  g_mutex_unlock (&priv->writer_lock);

  return status;
}

/* Метод HyScanSonar->receiver_set_time. */
static gboolean
hyscan_control_sonar_receiver_set_time (HyScanSonar      *sonar,
                                        HyScanSourceType  source,
                                        gdouble           receive_time,
                                        gdouble           wait_time)
{
  HyScanControl *control = HYSCAN_CONTROL (sonar);
  HyScanControlPrivate *priv = control->priv;
  HyScanControlSourceInfo *source_info;

  if (!g_atomic_int_get (&priv->binded))
    return FALSE;

  source_info = g_hash_table_lookup (priv->sources, GINT_TO_POINTER (source));
  if (source_info == NULL)
    return FALSE;

  return hyscan_sonar_receiver_set_time (HYSCAN_SONAR (source_info->device),
                                         source, receive_time, wait_time);
}

/* Метод HyScanSonar->receiver_set_auto. */
static gboolean
hyscan_control_sonar_receiver_set_auto (HyScanSonar      *sonar,
                                        HyScanSourceType  source)
{
  HyScanControl *control = HYSCAN_CONTROL (sonar);
  HyScanControlPrivate *priv = control->priv;
  HyScanControlSourceInfo *source_info;

  if (!g_atomic_int_get (&priv->binded))
    return FALSE;

  source_info = g_hash_table_lookup (priv->sources, GINT_TO_POINTER (source));
  if (source_info == NULL)
    return FALSE;

  return hyscan_sonar_receiver_set_auto (HYSCAN_SONAR (source_info->device), source);
}

/* Метод HyScanSonar->receiver_disable. */
static gboolean
hyscan_control_sonar_receiver_disable (HyScanSonar      *sonar,
                                       HyScanSourceType  source)
{
  HyScanControl *control = HYSCAN_CONTROL (sonar);
  HyScanControlPrivate *priv = control->priv;
  HyScanControlSourceInfo *source_info;

  if (!g_atomic_int_get (&priv->binded))
    return FALSE;

  source_info = g_hash_table_lookup (priv->sources, GINT_TO_POINTER (source));
  if (source_info == NULL)
    return FALSE;

  return hyscan_sonar_receiver_disable (HYSCAN_SONAR (source_info->device), source);
}

/* Метод HyScanSonar->generator_set_preset. */
static gboolean
hyscan_control_sonar_generator_set_preset (HyScanSonar      *sonar,
                                           HyScanSourceType  source,
                                           gint64            preset)
{
  HyScanControl *control = HYSCAN_CONTROL (sonar);
  HyScanControlPrivate *priv = control->priv;
  HyScanControlSourceInfo *source_info;

  if (!g_atomic_int_get (&priv->binded))
    return FALSE;

  source_info = g_hash_table_lookup (priv->sources, GINT_TO_POINTER (source));
  if (source_info == NULL)
    return FALSE;

  return hyscan_sonar_generator_set_preset (HYSCAN_SONAR (source_info->device),
                                            source, preset);
}

/* Метод HyScanSonar->generator_disable. */
static gboolean
hyscan_control_sonar_generator_disable (HyScanSonar      *sonar,
                                        HyScanSourceType  source)
{
  HyScanControl *control = HYSCAN_CONTROL (sonar);
  HyScanControlPrivate *priv = control->priv;
  HyScanControlSourceInfo *source_info;

  if (!g_atomic_int_get (&priv->binded))
    return FALSE;

  source_info = g_hash_table_lookup (priv->sources, GINT_TO_POINTER (source));
  if (source_info == NULL)
    return FALSE;

  return hyscan_sonar_generator_disable (HYSCAN_SONAR (source_info->device), source);
}

/* Метод HyScanSonar->tvg_set_auto. */
static gboolean
hyscan_control_sonar_tvg_set_auto (HyScanSonar      *sonar,
                                   HyScanSourceType  source,
                                   gdouble           level,
                                   gdouble           sensitivity)
{
  HyScanControl *control = HYSCAN_CONTROL (sonar);
  HyScanControlPrivate *priv = control->priv;
  HyScanControlSourceInfo *source_info;

  if (!g_atomic_int_get (&priv->binded))
    return FALSE;

  source_info = g_hash_table_lookup (priv->sources, GINT_TO_POINTER (source));
  if (source_info == NULL)
    return FALSE;

  return hyscan_sonar_tvg_set_auto (HYSCAN_SONAR (source_info->device),
                                    source, level, sensitivity);
}

/* Метод HyScanSonar->tvg_set_constant. */
static gboolean
hyscan_control_sonar_tvg_set_constant (HyScanSonar      *sonar,
                                       HyScanSourceType  source,
                                       gdouble           gain)
{
  HyScanControl *control = HYSCAN_CONTROL (sonar);
  HyScanControlPrivate *priv = control->priv;
  HyScanControlSourceInfo *source_info;

  if (!g_atomic_int_get (&priv->binded))
    return FALSE;

  source_info = g_hash_table_lookup (priv->sources, GINT_TO_POINTER (source));
  if (source_info == NULL)
    return FALSE;

  return hyscan_sonar_tvg_set_constant (HYSCAN_SONAR (source_info->device),
                                        source, gain);
}

/* Метод HyScanSonar->tvg_set_linear_db. */
static gboolean
hyscan_control_sonar_tvg_set_linear_db (HyScanSonar      *sonar,
                                        HyScanSourceType  source,
                                        gdouble           gain0,
                                        gdouble           gain_step)
{
  HyScanControl *control = HYSCAN_CONTROL (sonar);
  HyScanControlPrivate *priv = control->priv;
  HyScanControlSourceInfo *source_info;

  if (!g_atomic_int_get (&priv->binded))
    return FALSE;

  source_info = g_hash_table_lookup (priv->sources, GINT_TO_POINTER (source));
  if (source_info == NULL)
    return FALSE;

  return hyscan_sonar_tvg_set_linear_db (HYSCAN_SONAR (source_info->device),
                                         source, gain0, gain_step);
}

/* Метод HyScanSonar->tvg_set_logarithmic. */
static gboolean
hyscan_control_sonar_tvg_set_logarithmic (HyScanSonar      *sonar,
                                          HyScanSourceType  source,
                                          gdouble           gain0,
                                          gdouble           beta,
                                          gdouble           alpha)
{
  HyScanControl *control = HYSCAN_CONTROL (sonar);
  HyScanControlPrivate *priv = control->priv;
  HyScanControlSourceInfo *source_info;

  if (!g_atomic_int_get (&priv->binded))
    return FALSE;

  source_info = g_hash_table_lookup (priv->sources, GINT_TO_POINTER (source));
  if (source_info == NULL)
    return FALSE;

  return hyscan_sonar_tvg_set_logarithmic (HYSCAN_SONAR (source_info->device),
                                           source, gain0, beta, alpha);
}

/* Метод HyScanSonar->tvg_disable. */
static gboolean
hyscan_control_sonar_tvg_disable (HyScanSonar      *sonar,
                                  HyScanSourceType  source)
{
  HyScanControl *control = HYSCAN_CONTROL (sonar);
  HyScanControlPrivate *priv = control->priv;
  HyScanControlSourceInfo *source_info;

  if (!g_atomic_int_get (&priv->binded))
    return FALSE;

  source_info = g_hash_table_lookup (priv->sources, GINT_TO_POINTER (source));
  if (source_info == NULL)
    return FALSE;

  return hyscan_sonar_tvg_disable (HYSCAN_SONAR (source_info->device), source);
}

/* Метод HyScanSonar->start. */
static gboolean
hyscan_control_sonar_start (HyScanSonar           *sonar,
                            const gchar           *project_name,
                            const gchar           *track_name,
                            HyScanTrackType        track_type,
                            const HyScanTrackPlan *track_plan)
{
  HyScanControl *control = HYSCAN_CONTROL (sonar);
  HyScanControlPrivate *priv = control->priv;

  gboolean status = TRUE;

  GHashTableIter iter;
  gpointer value;

  if (!g_atomic_int_get (&priv->binded))
    return FALSE;

  g_mutex_lock (&priv->writer_lock);

  if (!hyscan_data_writer_start (priv->writer, project_name, track_name, track_type, track_plan, -1))
    {
      status = FALSE;
      goto exit;
    }

  priv->started = TRUE;

  g_mutex_unlock (&priv->writer_lock);

  g_hash_table_iter_init (&iter, priv->devices);
  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      HyScanSonar *sonar = value;

      if (!HYSCAN_IS_SONAR (sonar))
        continue;

      if (!hyscan_sonar_start (sonar, project_name, track_name, track_type, track_plan))
        status = FALSE;
    }

exit:
  return status;
}

/* Метод HyScanSonar->stop. */
static gboolean
hyscan_control_sonar_stop (HyScanSonar *sonar)
{
  HyScanControl *control = HYSCAN_CONTROL (sonar);
  HyScanControlPrivate *priv = control->priv;

  gboolean status = TRUE;

  GHashTableIter iter;
  gpointer value;

  if (!g_atomic_int_get (&priv->binded))
    return FALSE;

  g_hash_table_iter_init (&iter, priv->devices);
  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      HyScanSonar *sonar = value;

      if (!HYSCAN_IS_SONAR (sonar))
        continue;

      if (!hyscan_sonar_stop (sonar))
        status = FALSE;
    }

  g_mutex_lock (&priv->writer_lock);

  priv->started = FALSE;
  hyscan_data_writer_stop (priv->writer);

  g_hash_table_iter_init (&iter, priv->sources);
  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      HyScanControlSourceInfo *source_info = value;

      g_hash_table_remove_all (source_info->channels);
    }

  g_mutex_unlock (&priv->writer_lock);

  return status;
}

/* Метод HyScanSensor->antenna_set_offset. */
static gboolean
hyscan_control_sensor_antenna_set_offset (HyScanSensor              *sensor,
                                          const gchar               *name,
                                          const HyScanAntennaOffset *offset)
{
  HyScanControl *control = HYSCAN_CONTROL (sensor);
  HyScanControlPrivate *priv = control->priv;
  HyScanControlSensorInfo *sensor_info;
  gboolean status = FALSE;

  g_return_val_if_fail (HYSCAN_IS_CONTROL (control), FALSE);

  priv = control->priv;

  if (!g_atomic_int_get (&priv->binded))
    return FALSE;

  g_mutex_lock (&priv->writer_lock);

  /* Если нет датчика или задано смещение по умолчанию - выходим. */
  sensor_info = g_hash_table_lookup (priv->sensors, name);
  if ((sensor_info == NULL) || (sensor_info->info->offset != NULL))
    goto exit;

  /* Запоминаем новое смещение. */
  hyscan_antenna_offset_free (sensor_info->offset);
  sensor_info->offset = hyscan_antenna_offset_copy (offset);
  hyscan_data_writer_sensor_set_offset (priv->writer, name, offset);

  /* Передаём новые значения в драйвер. */
  status = hyscan_sensor_antenna_set_offset (HYSCAN_SENSOR (sensor_info->device),
                                             name, offset);

exit:
  g_mutex_unlock (&priv->writer_lock);

  return status;
}

/* Метод HyScanSensor->set_enable. */
static gboolean
hyscan_control_sensor_set_enable (HyScanSensor *sensor,
                                  const gchar  *name,
                                  gboolean      enable)
{
  HyScanControl *control = HYSCAN_CONTROL (sensor);
  HyScanControlPrivate *priv = control->priv;
  gboolean status;

  HyScanControlSensorInfo *sensor_info;

  if (!g_atomic_int_get (&priv->binded))
    return FALSE;

  sensor_info = g_hash_table_lookup (priv->sensors, name);
  if (sensor_info == NULL)
    return FALSE;

  status = hyscan_sensor_set_enable (HYSCAN_SENSOR (sensor_info->device),
                                     name, enable);

  return status;
}

/* Метод HyScanActuator->disable. */
static gboolean
hyscan_control_actuator_disable (HyScanActuator *actuator,
                                 const gchar    *name)
{
  HyScanControl *control = HYSCAN_CONTROL (actuator);
  HyScanControlPrivate *priv = control->priv;
  gboolean status;

  HyScanControlActuatorInfo *actuator_info;

  if (!g_atomic_int_get (&priv->binded))
    return FALSE;

  actuator_info = g_hash_table_lookup (priv->actuators, name);
  if (actuator_info == NULL)
    return FALSE;

  status = hyscan_actuator_disable (HYSCAN_ACTUATOR (actuator_info->device), name);

  return status;
}

/* Метод HyScanActuator->scan. */
static gboolean
hyscan_control_actuator_scan (HyScanActuator *actuator,
                              const gchar    *name,
                              gdouble         from,
                              gdouble         to,
                              gdouble         speed)
{
  HyScanControl *control = HYSCAN_CONTROL (actuator);
  HyScanControlPrivate *priv = control->priv;
  gboolean status;

  HyScanControlActuatorInfo *actuator_info;

  if (!g_atomic_int_get (&priv->binded))
    return FALSE;

  actuator_info = g_hash_table_lookup (priv->actuators, name);
  if (actuator_info == NULL)
    return FALSE;

  status = hyscan_actuator_scan (HYSCAN_ACTUATOR (actuator_info->device), name, from, to, speed);

  return status;
}

/* Метод HyScanActuator->manual. */
static gboolean
hyscan_control_actuator_manual (HyScanActuator *actuator,
                                const gchar    *name,
                                gdouble         angle)
{
  HyScanControl *control = HYSCAN_CONTROL (actuator);
  HyScanControlPrivate *priv = control->priv;
  gboolean status;

  HyScanControlActuatorInfo *actuator_info;

  if (!g_atomic_int_get (&priv->binded))
    return FALSE;

  actuator_info = g_hash_table_lookup (priv->actuators, name);
  if (actuator_info == NULL)
    return FALSE;

  status = hyscan_actuator_manual (HYSCAN_ACTUATOR (actuator_info->device), name, angle);

  return status;
}

/**
 * hyscan_control_new:
 *
 * Функция создаёт новый объект управления гидролокаторами и датчиками.
 *
 * Returns: #HyScanControl. Для удаления #g_object_unref.
 */
HyScanControl *
hyscan_control_new (void)
{
  return g_object_new (HYSCAN_TYPE_CONTROL, NULL);
}

/**
 * hyscan_control_device_add:
 * @control: указатель на #HyScanControl
 * @device: указатель на подключенное устройство
 *
 * Функция добавляет новое устройство в объект управления. Устройство может
 * быть добавлено только до вызова функции #hyscan_control_device_bind.
 *
 * Returns: %TRUE если команда выполнена успешно, иначе %FALSE.
 */
gboolean
hyscan_control_device_add (HyScanControl *control,
                           HyScanDevice  *device)
{
  HyScanControlPrivate *priv;

  gchar *device_name = NULL;
  HyScanDataSchema *device_schema = NULL;
  HyScanActuatorInfo *actuator_info = NULL;
  HyScanSensorInfo *sensor_info = NULL;
  HyScanSonarInfo *sonar_info = NULL;

  const HyScanSourceType *sources;
  const gchar * const *sensors;
  const gchar * const *actuators;
  guint32 n_sources;
  guint32 n_sensors;
  guint32 n_actuators;
  guint i, j;

  gboolean status = FALSE;

  g_return_val_if_fail (HYSCAN_IS_CONTROL (control), FALSE);

  priv = control->priv;

  if (g_atomic_int_get (&control->priv->binded))
    return FALSE;

  /* Проверяем, что устройство реализует интерфейсы HyScanDevice,
   * HyScanParam и минимум один из интерфейсов HyScanSensor,
   * HyScanSonar или HyScanActuator. */
  if (!HYSCAN_IS_DEVICE (device) || !HYSCAN_IS_PARAM (device))
    return FALSE;
  if (!HYSCAN_IS_SENSOR (device) && !HYSCAN_IS_SONAR (device) && !HYSCAN_IS_ACTUATOR (device))
    return FALSE;

  /* Схема устройства и информация о нём. */
  device_schema = hyscan_param_schema (HYSCAN_PARAM (device));
  sonar_info = hyscan_sonar_info_new (device_schema);
  sensor_info = hyscan_sensor_info_new (device_schema);
  actuator_info = hyscan_actuator_info_new (device_schema);

  /* Список датчиков и устройств. */
  sensors = hyscan_sensor_info_list_sensors (sensor_info);
  sources = hyscan_sonar_info_list_sources (sonar_info, &n_sources);
  actuators = hyscan_actuator_info_list_actuators (actuator_info);

  if (sensors != NULL)
    n_sensors = g_strv_length ((gchar**)sensors);
  else
    n_sensors = 0;

  if (actuators != NULL)
    n_actuators = g_strv_length ((gchar**)actuators);
  else
    n_actuators = 0;

  g_mutex_lock (&priv->writer_lock);

  /* Название устройства. */
  device_name = g_strdup_printf ("device%d", g_hash_table_size (priv->devices) + 1);

  /* Проверяем, что устройство ещё не добавлено. */
  {
    GHashTableIter iter;
    gpointer key, value;

    g_hash_table_iter_init (&iter, priv->devices);
    while (g_hash_table_iter_next (&iter, &key, &value))
      if (device == value)
        goto exit;
  }

  /* Проверяем, что нет пересечений по датчикам. */
  if (sensors != NULL)
    {
      for (i = 0; i < n_sensors; i++)
        {
          if (g_hash_table_contains (priv->sensors, sensors[i]))
            goto exit;
        }
    }

  /* Проверяем, что нет пересечений по источникам данных. */
  if (sources != NULL)
    {
      for (i = 0; i < n_sources; i++)
        {
          if (g_hash_table_contains (priv->sources, GINT_TO_POINTER (sources[i])))
            goto exit;
        }
    }

  /* Проверяем, что нет пересечений по приводам. */
  if (actuators != NULL)
    {
      for (i = 0; i < n_actuators; i++)
        {
          if (g_hash_table_contains (priv->actuators, actuators[i]))
            goto exit;
        }
    }

  /* Добавляем датчики. */
  if (sensors != NULL)
    {
      for (i = 0; i < n_sensors; i++)
        {
          HyScanControlSensorInfo *info = g_slice_new0 (HyScanControlSensorInfo);
          gboolean add_device = TRUE;

          info->device = device;
          info->info = hyscan_sensor_info_sensor_copy (hyscan_sensor_info_get_sensor (sensor_info, sensors[i]));
          info->channel = g_hash_table_size (priv->sensors) + 1;

          g_hash_table_insert (priv->sensors, (gchar*)info->info->name, info);
          g_array_append_val (priv->sensors_list, info->info->name);

          for (j = 0; j < priv->devices_list->len; j++)
            {
              const gchar *dev_id = g_array_index (priv->devices_list, const gchar *, j);
              if (g_strcmp0 (dev_id, info->info->dev_id) == 0)
                add_device = FALSE;
            }
          if (add_device)
            g_array_append_val (priv->devices_list, info->info->dev_id);
        }
    }

  /* Добавляем гидролокационные источники данных. */
  if (sources != NULL)
    {
      for (i = 0; i < n_sources; i++)
        {
          HyScanControlSourceInfo *info = g_slice_new0 (HyScanControlSourceInfo);
          gboolean add_device = TRUE;

          info->device = device;
          info->info = hyscan_sonar_info_source_copy (hyscan_sonar_info_get_source (sonar_info, sources[i]));
          info->channels = g_hash_table_new_full (NULL, NULL,
                                                  NULL,
                                                  hyscan_control_free_channel_info);

          g_hash_table_insert (priv->sources, GINT_TO_POINTER (sources[i]), info);
          g_array_append_val (priv->sources_list, sources[i]);

          for (j = 0; j < priv->devices_list->len; j++)
            {
              const gchar *dev_id = g_array_index (priv->devices_list, const gchar *, j);
              if (g_strcmp0 (dev_id, info->info->dev_id) == 0)
                add_device = FALSE;
            }
          if (add_device)
            g_array_append_val (priv->devices_list, info->info->dev_id);
        }
    }

  /* Добавляем приводы. */
  if (actuators != NULL)
    {
      for (i = 0; i < n_actuators; i++)
        {
          HyScanControlActuatorInfo *info = g_slice_new0 (HyScanControlActuatorInfo);
          gboolean add_device = TRUE;

          info->device = device;
          info->info = hyscan_actuator_info_actuator_copy (hyscan_actuator_info_get_actuator (actuator_info, actuators[i]));

          g_hash_table_insert (priv->actuators, (gchar*)info->info->name, info);
          g_array_append_val (priv->actuators_list, info->info->name);

          for (j = 0; j < priv->devices_list->len; j++)
            {
              const gchar *dev_id = g_array_index (priv->devices_list, const gchar *, j);
              if (g_strcmp0 (dev_id, info->info->dev_id) == 0)
                add_device = FALSE;
            }
          if (add_device)
            g_array_append_val (priv->devices_list, info->info->dev_id);
        }
    }

  g_hash_table_insert (priv->devices, device_name, g_object_ref (device));
  device_name = NULL;

  /* Обработчик сигналов устройства. */
  g_signal_connect (device, "device-state",
                    G_CALLBACK (hyscan_control_device_state), control);
  g_signal_connect (device, "device-log",
                    G_CALLBACK (hyscan_control_device_log), control);

  /* Обработчики сигналов от датчиков. */
  if (HYSCAN_IS_SENSOR (device) && (n_sensors > 0))
    {
      g_signal_connect (device, "sensor-data",
                        G_CALLBACK (hyscan_control_sensor_data), control);
    }

  /* Обработчики сигналов от гидролокаторов. */
  if (HYSCAN_IS_SONAR (device) && (n_sources > 0))
    {
      g_signal_connect (device, "sonar-source-info",
                        G_CALLBACK (hyscan_control_sonar_source_info), control);
      g_signal_connect (device, "sonar-signal",
                        G_CALLBACK (hyscan_control_sonar_signal), control);
      g_signal_connect (device, "sonar-tvg",
                        G_CALLBACK (hyscan_control_sonar_tvg), control);
      g_signal_connect (device, "sonar-acoustic-data",
                        G_CALLBACK (hyscan_control_sonar_acoustic_data), control);
    }

  status = TRUE;

exit:
  g_mutex_unlock (&priv->writer_lock);

  g_clear_object (&device_schema);
  g_clear_object (&sensor_info);
  g_clear_object (&sonar_info);
  g_free (device_name);

  return status;
}

/**
 * hyscan_control_device_bind:
 * @control: указатель на #HyScanControl
 *
 * Функция завершает конфигурирование объекта управления. После вызова этой
 * функции, добавить новые устройства нельзя.
 *
 * Returns: %TRUE если команда выполнена успешно, иначе %FALSE.
 */
gboolean
hyscan_control_device_bind (HyScanControl *control)
{
  HyScanControlPrivate *priv;
  gboolean status = FALSE;

  g_return_val_if_fail (HYSCAN_IS_CONTROL (control), FALSE);

  priv = control->priv;

  g_mutex_lock (&priv->writer_lock);

  if ((priv->schema != NULL) || priv->binded)
    goto exit;

  /* Создаём новую схему конфигурации устройств. */
  hyscan_control_create_device_schema (priv);
  if (priv->schema == NULL)
    goto exit;

  priv->binded = TRUE;
  status = TRUE;

exit:
  g_mutex_unlock (&priv->writer_lock);

  return status;
}

/**
 * hyscan_control_devices_list:
 * @control: указатель на #HyScanControl
 *
 * Функция возвращает список устройств, управляемых #HyScanControl.
 *
 * Returns: (transfer none): Список устройств или NULL.
 */
const gchar * const *
hyscan_control_devices_list (HyScanControl *control)
{
  g_return_val_if_fail (HYSCAN_IS_CONTROL (control), NULL);

  return (const gchar **)control->priv->devices_list->data;
}

/**
 * hyscan_control_device_get_status:
 * @control: указатель на #HyScanControl
 * @dev_id: идентификатор устройства
 *
 * Функция возвращает состояние устройства.
 *
 * Returns: Состояние устройства.
 */
HyScanDeviceStatusType
hyscan_control_device_get_status (HyScanControl *control,
                                  const gchar   *dev_id)
{
  HyScanControlPrivate *priv;
  HyScanDeviceStatusType status = HYSCAN_DEVICE_STATUS_ERROR;
  gchar key_id[128];
  gpointer device;

  g_return_val_if_fail (HYSCAN_IS_CONTROL (control), HYSCAN_DEVICE_STATUS_ERROR);

  priv = control->priv;

  if (!g_atomic_int_get (&priv->binded))
    return FALSE;

  g_snprintf (key_id, sizeof (key_id), "/state/%s/status", dev_id);
  device = g_hash_table_lookup (priv->params, key_id);
  if (device == NULL)
    return HYSCAN_DEVICE_STATUS_ERROR;

  g_mutex_lock (&priv->list_lock);

  hyscan_param_list_clear (priv->list);
  hyscan_param_list_add (priv->list, key_id);
  if (hyscan_param_get (device, priv->list))
    status = hyscan_param_list_get_enum (priv->list, key_id);

  g_mutex_unlock (&priv->list_lock);

  return status;
}

/**
 * hyscan_control_sensors_list:
 * @control: указатель на #HyScanControl
 *
 * Функция возвращает список датчиков. Список возвращается в виде NULL
 * терминированного массива строк с названиями датчиков. Список принадлежит
 * объекту #HyScanControl и не должен изменяться пользователем.
 *
 * Returns: (transfer none): Список датчиков или NULL.
 */
const gchar * const *
hyscan_control_sensors_list (HyScanControl *control)
{
  g_return_val_if_fail (HYSCAN_IS_CONTROL (control), NULL);

  return (const gchar **)control->priv->sensors_list->data;
}

/**
 * hyscan_control_sources_list:
 * @control: указатель на #HyScanControl
 * @n_sources: (out): число источников данных
 *
 * Функция возвращает список источников гидролокационных данных. Список
 * возвращается в виде массива элементов типа HyScanSourceType. Массив
 * принадлежит объекту #HyScanControl и не должен изменяться пользователем.
 *
 * Returns: (transfer none): Список источников данных или NULL.
 */
const HyScanSourceType *
hyscan_control_sources_list (HyScanControl *control,
                             guint32       *n_sources)
{
  g_return_val_if_fail (HYSCAN_IS_CONTROL (control), NULL);

  *n_sources = control->priv->sources_list->len;

  return (HyScanSourceType *)control->priv->sources_list->data;
}

/**
 * hyscan_control_actuators_list:
 * @control: указатель на #HyScanControl
 *
 * Функция возвращает список приводов. Список возвращается в виде NULL
 * терминированного массива строк с названиями приводов. Список принадлежит
 * объекту #HyScanControl и не должен изменяться пользователем.
 *
 * Returns: (transfer none): Список приводов или NULL.
 */
const gchar * const *
hyscan_control_actuators_list (HyScanControl *control)
{
  g_return_val_if_fail (HYSCAN_IS_CONTROL (control), NULL);

  return (const gchar **)control->priv->actuators_list->data;
}

/**
 * hyscan_control_sensor_get_info:
 * @control: указатель на #HyScanControl
 * @sensor: название датчика
 *
 * Функция возвращает параметры датчика.
 *
 * Returns: (nullable) (transfer none): Параметры датчика или NULL.
 */
const HyScanSensorInfoSensor *
hyscan_control_sensor_get_info (HyScanControl *control,
                                const gchar   *sensor)
{
  HyScanControlSensorInfo *sensor_info;

  g_return_val_if_fail (HYSCAN_IS_CONTROL (control), NULL);

  if (sensor == NULL)
    return NULL;

  sensor_info = g_hash_table_lookup (control->priv->sensors, sensor);

  return (sensor_info != NULL) ? sensor_info->info : NULL;
}

/**
 * hyscan_control_actuator_get_info:
 * @control: указатель на #HyScanControl
 * @actuator: название привода
 *
 * Функция возвращает параметры привода.
 *
 * Returns: (nullable) (transfer none): Параметры привода или NULL.
 */
const HyScanActuatorInfoActuator *
hyscan_control_actuator_get_info (HyScanControl *control,
                                  const gchar   *actuator)
{
  HyScanControlActuatorInfo *actuator_info;

  g_return_val_if_fail (HYSCAN_IS_CONTROL (control), NULL);

  if (actuator == NULL)
    return NULL;

  actuator_info = g_hash_table_lookup (control->priv->actuators, actuator);

  return (actuator_info != NULL) ? actuator_info->info : NULL;
}

/**
 * hyscan_control_source_get_info:
 * @control: указатель на #HyScanControl
 * @source: источник гидролокационных данных
 *
 * Функция возвращает параметры источника гидролокационных данных.
 *
 * Returns: (nullable) (transfer none): Параметры гидролокационного источника данных или NULL.
 */
const HyScanSonarInfoSource *
hyscan_control_source_get_info (HyScanControl    *control,
                                HyScanSourceType  source)
{
  HyScanControlSourceInfo *source_info;

  g_return_val_if_fail (HYSCAN_IS_CONTROL (control), NULL);

  source_info = g_hash_table_lookup (control->priv->sources, GINT_TO_POINTER (source));

  return (source_info != NULL) ? source_info->info : NULL;
}

/**
 * hyscan_control_sensor_set_default_offset:
 * @control: указатель на #HyScanControl
 * @sensor: название датчика
 * @offset: смещение приёмной антенны
 *
 * Функция задаёт смещение приёмных антенн датчика по умолчанию.
 * Данная функция может быть вызвана только если смещение антенн по
 * умолчанию ещё не было задано устройством или этой же функцией.
 *
 * Подробное описание параметров приводится в #HyScanTypes.
 *
 * Returns: %TRUE если команда выполнена успешно, иначе %FALSE.
 */
gboolean
hyscan_control_sensor_set_default_offset (HyScanControl             *control,
                                          const gchar               *sensor,
                                          const HyScanAntennaOffset *offset)
{
  HyScanControlPrivate *priv;
  HyScanControlSensorInfo *sensor_info;

  g_return_val_if_fail (HYSCAN_IS_CONTROL (control), FALSE);

  priv = control->priv;

  if (g_atomic_int_get (&priv->binded))
    return FALSE;

  /* Если нет датчика или задано смещение по умолчанию - выходим. */
  sensor_info = g_hash_table_lookup (priv->sensors, sensor);
  if ((sensor_info == NULL) || (sensor_info->info->offset != NULL))
    return FALSE;

  /* Запоминаем новое смещение по умолчанию. */
  sensor_info->info->offset = hyscan_antenna_offset_copy (offset);

  return hyscan_sensor_antenna_set_offset (HYSCAN_SENSOR (sensor_info->device), sensor, offset);
}

/**
 * hyscan_control_source_set_default_offset:
 * @control: указатель на #HyScanControl
 * @source: источник гидролокационных данных
 * @offset: смещение приёмной антенны
 *
 * Функция задаёт смещение приёмных антенн гидролокатора по умолчанию.
 * Данная функция может быть вызвана только если смещение антенн по
 * умолчанию ещё не было задано устройством или этой же функцией.
 *
 * Подробное описание параметров приводится в #HyScanTypes.
 *
 * Returns: %TRUE если команда выполнена успешно, иначе %FALSE.
 */
gboolean
hyscan_control_source_set_default_offset (HyScanControl             *control,
                                          HyScanSourceType           source,
                                          const HyScanAntennaOffset *offset)
{
  HyScanControlPrivate *priv;
  HyScanControlSourceInfo *source_info;

  g_return_val_if_fail (HYSCAN_IS_CONTROL (control), FALSE);

  priv = control->priv;

  if (g_atomic_int_get (&priv->binded))
    return FALSE;

  source_info = g_hash_table_lookup (priv->sources, GINT_TO_POINTER (source));
  if ((source_info == NULL) || (source_info->info->offset != NULL))
    return FALSE;

  /* Запоминаем новое смещение по умолчанию. */
  source_info->info->offset = hyscan_antenna_offset_copy (offset);

  return hyscan_sonar_antenna_set_offset (HYSCAN_SONAR (source_info->device), source, offset);
}

/**
 * hyscan_control_writer_set_db:
 * @control: указатель на #HyScanControl
 * @db: указатель на #HyScanDB
 *
 * Функция устанавливает систему хранения данных. Система хранения может быть
 * установлена только в режиме ожидания.
 */
void
hyscan_control_writer_set_db (HyScanControl *control,
                              HyScanDB      *db)
{
  HyScanControlPrivate *priv;

  g_return_if_fail (HYSCAN_IS_CONTROL (control));

  priv = control->priv;

  g_mutex_lock (&priv->writer_lock);
  hyscan_data_writer_set_db (control->priv->writer, db);
  g_mutex_unlock (&priv->writer_lock);
}

/**
 * hyscan_control_writer_set_operator_name:
 * @control: указатель на #HyScanControl
 * @name: имя оператора
 *
 * Функция устанавливает имя оператора, которое будет записываться в каждый
 * галс при его создании.
 */
void
hyscan_control_writer_set_operator_name (HyScanControl *control,
                                         const gchar   *name)
{
  HyScanControlPrivate *priv;

  g_return_if_fail (HYSCAN_IS_CONTROL (control));

  priv = control->priv;

  g_mutex_lock (&priv->writer_lock);
  hyscan_data_writer_set_operator_name (control->priv->writer, name);
  g_mutex_unlock (&priv->writer_lock);
}

/**
 * hyscan_control_writer_set_chunk_size:
 * @control: указатель на #HyScanControl
 * @chunk_size: максимальный размер файлов в байтах или отрицательное число
 *
 * Функция устанавливает максимальный размер файлов в галсе.
 *
 * Подробнее об этом можно прочитать в описании интерфейса #HyScanDB.
 */
void
hyscan_control_writer_set_chunk_size (HyScanControl *control,
                                      gint32         chunk_size)
{
  HyScanControlPrivate *priv;

  g_return_if_fail (HYSCAN_IS_CONTROL (control));

  priv = control->priv;

  g_mutex_lock (&priv->writer_lock);
  hyscan_data_writer_set_chunk_size (control->priv->writer, chunk_size);
  g_mutex_unlock (&priv->writer_lock);
}

static void
hyscan_control_param_interface_init (HyScanParamInterface *iface)
{
  iface->schema = hyscan_control_param_schema;
  iface->set = hyscan_control_param_set;
  iface->get = hyscan_control_param_get;
}

static void
hyscan_control_device_interface_init (HyScanDeviceInterface *iface)
{
  iface->sync = hyscan_control_device_sync;
  iface->set_sound_velocity = hyscan_control_device_set_sound_velocity;
  iface->disconnect = hyscan_control_device_disconnect;
}

static void
hyscan_control_sonar_interface_init (HyScanSonarInterface *iface)
{
  iface->antenna_set_offset = hyscan_control_sonar_antenna_set_offset;
  iface->receiver_set_time = hyscan_control_sonar_receiver_set_time;
  iface->receiver_set_auto = hyscan_control_sonar_receiver_set_auto;
  iface->receiver_disable = hyscan_control_sonar_receiver_disable;
  iface->generator_set_preset = hyscan_control_sonar_generator_set_preset;
  iface->generator_disable = hyscan_control_sonar_generator_disable;
  iface->tvg_set_auto = hyscan_control_sonar_tvg_set_auto;
  iface->tvg_set_constant = hyscan_control_sonar_tvg_set_constant;
  iface->tvg_set_linear_db = hyscan_control_sonar_tvg_set_linear_db;
  iface->tvg_set_logarithmic = hyscan_control_sonar_tvg_set_logarithmic;
  iface->tvg_disable = hyscan_control_sonar_tvg_disable;
  iface->start = hyscan_control_sonar_start;
  iface->stop = hyscan_control_sonar_stop;
}

static void
hyscan_control_sensor_interface_init (HyScanSensorInterface *iface)
{
  iface->antenna_set_offset = hyscan_control_sensor_antenna_set_offset;
  iface->set_enable = hyscan_control_sensor_set_enable;
}

static void
hyscan_control_actuator_interface_init (HyScanActuatorInterface *iface)
{
  iface->disable = hyscan_control_actuator_disable;
  iface->scan = hyscan_control_actuator_scan;
  iface->manual = hyscan_control_actuator_manual;
}
