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
 * Создание объекта управления производится с помощью функции #hyscan_control_new.
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
 * Список источников гидролокационных данных можно получить с помощью функции
 * #hyscan_control_get_sources, а список датчиков #hyscan_control_get_sensors.
 *
 * Информацию о датчиках и источниках данных можно получить с помощью функций
 * #hyscan_control_sensor_get_info и #hyscan_control_source_get_info.
 *
 * В процессе работы, с устройством может быть потеряна связь или возникнет
 * иная не штатная ситуация. В этом случае датчик или источник данных окажется
 * не доступным. При изменении состояния устройства посылается сигнал
 * "device-state". Само состояние датчика или источника данных можно узнать с
 * помощью функций #hyscan_control_sensor_get_enable и
 * #hyscan_control_source_get_enable.
 *
 * Задать местоположение приёмных антенн датчика или источника данных можно
 * с помощью функций #hyscan_control_sensor_set_position и
 * #hyscan_control_source_set_position. Если для датчика или источника данных
 * задано местоположение антенн по умолчанию, изменить его нельзя.
 *
 * В системе могут использоваться несколько однотипных датчиков, например
 * два и более датчиков систем позиционирования ГЛОНАСС или GPS. Для того,
 * чтобы различать информацию от этих датчиков, имеется возможность добавить
 * метку к данным каждого из датчиков. Такой меткой является номер канала.
 * Для задания номера канала предназначена функция
 * #hyscan_control_sensor_set_channel.
 *
 * Функция #hyscan_control_writer_set_db устанавливает систему хранения.
 *
 * Функции #hyscan_control_writer_set_operator_name,
 * #hyscan_control_writer_set_mode, #hyscan_control_writer_set_chunk_size,
 * #hyscan_control_writer_set_save_time и #hyscan_control_writer_set_save_size
 * аналогичны подобным функциям #HyScanDataWriter.
 *
 * Класс HyScanControl поддерживает работу в многопоточном режиме.
 */

#include "hyscan-control.h"

#include <hyscan-sensor-schema.h>
#include <hyscan-sonar-schema.h>

#include <string.h>

typedef struct
{
  HyScanDevice                *device;                         /* Указатель на подключенное устройство. */
  HyScanSensorInfoSensor      *info;                           /* Информация о датчике. */
  HyScanAntennaPosition       *position;                       /* Местоположение антенны. */
  guint                        channel;                        /* Номер приёмного канала. */
} HyScanControlSensorInfo;

typedef struct
{
  HyScanDevice                *device;                         /* Указатель на подключенное устройство. */
  HyScanSonarInfoSource       *info;                           /* Информация об источнике данных. */
  HyScanAntennaPosition       *position;                       /* Местоположение антенны. */
} HyScanControlSourceInfo;

struct _HyScanControlPrivate
{
  gboolean                     binded;                         /* Признак завершения конфигурирования. */
  GHashTable                  *devices;                        /* Список подключенных устройств. */

  GHashTable                  *sensors;                        /* Список датчиков. */
  GHashTable                  *sources;                        /* Список источников данных. */

  GHashTable                  *params;                         /* Параметры. */
  HyScanParamList             *list;                           /* Список параметров. */

  gchar                      **sensors_list;                   /* Список датчиков. */
  HyScanSourceType            *sources_list;                   /* Список источников данных. */
  guint32                      n_sources;                      /* Число источников данных. */

  gboolean                     software_ping;                  /* Программное управление излучением. */
  HyScanDataSchema            *schema;                         /* Схема управления устройствами. */

  HyScanDataWriter            *writer;                         /* Объект записи данных. */

  gint64                       log_time;                       /* Метка времени последней записи информационного сообщения. */

  GMutex                       lock;                           /* Блокировка доступа. */
};

static void        hyscan_control_param_interface_init         (HyScanParamInterface           *iface);
static void        hyscan_control_sonar_interface_init         (HyScanSonarInterface           *iface);
static void        hyscan_control_sensor_interface_init        (HyScanSensorInterface          *iface);

static void        hyscan_control_object_constructed           (GObject                        *object);
static void        hyscan_control_object_finalize              (GObject                        *object);

static void        hyscan_control_free_sensor_info             (gpointer                        data);
static void        hyscan_control_free_source_info             (gpointer                        data);

static void        hyscan_control_create_device_schema         (HyScanControlPrivate           *priv);

static void        hyscan_control_sensor_data                  (HyScanDevice                   *device,
                                                                const gchar                    *sensor,
                                                                HyScanSourceType                source,
                                                                gint64                          time,
                                                                HyScanBuffer                   *data,
                                                                HyScanControl                  *control);

static void        hyscan_control_sensor_log                   (HyScanDevice                   *device,
                                                                const gchar                    *source,
                                                                gint64                          time,
                                                                gint                            level,
                                                                const gchar                    *message,
                                                                HyScanControl                  *control);

static void        hyscan_control_sonar_signal                 (HyScanDevice                   *device,
                                                                gint                            source,
                                                                gint64                          time,
                                                                HyScanBuffer                   *points,
                                                                HyScanControl                  *control);

static void        hyscan_control_sonar_tvg                    (HyScanDevice                   *device,
                                                                gint                            source,
                                                                guint                           channel,
                                                                gint64                          time,
                                                                HyScanBuffer                   *gains,
                                                                HyScanControl                  *control);

static void        hyscan_control_sonar_raw_data               (HyScanDevice                   *device,
                                                                gint                            source,
                                                                guint                           channel,
                                                                gint64                          time,
                                                                HyScanRawDataInfo              *info,
                                                                HyScanBuffer                   *data,
                                                                HyScanControl                  *control);

static void        hyscan_control_sonar_noise_data             (HyScanDevice                   *device,
                                                                gint                            source,
                                                                guint                           channel,
                                                                gint64                          time,
                                                                HyScanRawDataInfo              *info,
                                                                HyScanBuffer                   *data,
                                                                HyScanControl                  *control);

static void        hyscan_control_sonar_acoustic_data          (HyScanDevice                   *device,
                                                                gint                            source,
                                                                gint64                          time,
                                                                HyScanAcousticDataInfo         *info,
                                                                HyScanBuffer                   *data,
                                                                HyScanControl                  *control);

static void        hyscan_control_sonar_log                    (HyScanDevice                   *device,
                                                                const gchar                    *source,
                                                                gint64                          time,
                                                                gint                            level,
                                                                const gchar                    *message,
                                                                HyScanControl                  *control);

static void        hyscan_control_device_state                 (HyScanDevice                   *device,
                                                                const gchar                    *dev_id,
                                                                HyScanControl                  *control);

static HyScanDataSchema *hyscan_control_param_schema           (HyScanParam                    *param);

static gboolean    hyscan_control_param_set                    (HyScanParam                    *param,
                                                                HyScanParamList                *list);

static gboolean    hyscan_control_param_get                    (HyScanParam                    *param,
                                                                HyScanParamList                *list);

static gboolean    hyscan_control_sonar_set_sound_velocity     (HyScanSonar                    *sonar,
                                                                GList                          *svp);

static gboolean    hyscan_control_sonar_receiver_set_time      (HyScanSonar                    *sonar,
                                                                HyScanSourceType                source,
                                                                gdouble                         receive_time);

static gboolean    hyscan_control_sonar_receiver_set_auto      (HyScanSonar                    *sonar,
                                                                HyScanSourceType                source);

static gboolean    hyscan_control_sonar_generator_set_preset   (HyScanSonar                    *sonar,
                                                                HyScanSourceType                source,
                                                                gint64                          preset);

static gboolean    hyscan_control_sonar_generator_set_auto     (HyScanSonar                    *sonar,
                                                                HyScanSourceType                source,
                                                                HyScanSonarGeneratorSignalType  signal);

static gboolean    hyscan_control_sonar_generator_set_simple   (HyScanSonar                    *sonar,
                                                                HyScanSourceType                source,
                                                                HyScanSonarGeneratorSignalType  signal,
                                                                gdouble                         power);

static gboolean    hyscan_control_sonar_generator_set_extended (HyScanSonar                    *sonar,
                                                                HyScanSourceType                source,
                                                                HyScanSonarGeneratorSignalType  signal,
                                                                gdouble                         duration,
                                                                gdouble                         power);

static gboolean    hyscan_control_sonar_tvg_set_auto           (HyScanSonar                    *sonar,
                                                                HyScanSourceType                source,
                                                                gdouble                         level,
                                                                gdouble                         sensitivity);

static gboolean    hyscan_control_sonar_tvg_set_points         (HyScanSonar                    *sonar,
                                                                HyScanSourceType                source,
                                                                gdouble                         time_step,
                                                                const gdouble                  *gains,
                                                                guint32                         n_gains);

static gboolean    hyscan_control_sonar_tvg_set_linear_db      (HyScanSonar                    *sonar,
                                                                HyScanSourceType                source,
                                                                gdouble                         gain0,
                                                                gdouble                         gain_step);

static gboolean    hyscan_control_sonar_tvg_set_logarithmic    (HyScanSonar                    *sonar,
                                                                HyScanSourceType                source,
                                                                gdouble                         gain0,
                                                                gdouble                         beta,
                                                                gdouble                         alpha);

static gboolean    hyscan_control_sonar_set_software_ping      (HyScanSonar                    *sonar);

static gboolean    hyscan_control_sonar_start                  (HyScanSonar                    *sonar,
                                                                const gchar                    *project_name,
                                                                const gchar                    *track_name,
                                                                HyScanTrackType                 track_type);

static gboolean    hyscan_control_sonar_stop                   (HyScanSonar                    *sonar);

static gboolean    hyscan_control_sonar_sync                   (HyScanSonar                    *sonar);

static gboolean    hyscan_control_sonar_ping                   (HyScanSonar                    *sonar);

static gboolean    hyscan_control_sensor_set_sound_velocity    (HyScanSensor                   *sensor,
                                                                GList                          *svp);

static gboolean    hyscan_control_sensor_set_enable            (HyScanSensor                   *sensor,
                                                                const gchar                    *name,
                                                                gboolean                        enable);

G_DEFINE_TYPE_WITH_CODE (HyScanControl, hyscan_control, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (HyScanControl)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_PARAM, hyscan_control_param_interface_init)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_SONAR, hyscan_control_sonar_interface_init)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_SENSOR, hyscan_control_sensor_interface_init))

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

  g_mutex_init (&priv->lock);

  priv->devices = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                         g_object_unref);
  priv->sensors = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                         hyscan_control_free_sensor_info);
  priv->sources = g_hash_table_new_full (NULL, NULL, NULL,
                                         hyscan_control_free_source_info);
  priv->params  = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  priv->list = hyscan_param_list_new ();

  priv->writer = hyscan_data_writer_new ();

  priv->software_ping = TRUE;
}

static void
hyscan_control_object_finalize (GObject *object)
{
  HyScanControl *control = HYSCAN_CONTROL (object);
  HyScanControlPrivate *priv = control->priv;

  GHashTableIter iter;
  gpointer key, value;

  g_hash_table_iter_init (&iter, priv->devices);
  while (g_hash_table_iter_next (&iter, &key, &value))
    g_signal_handlers_disconnect_by_data (value, control);

  g_free (priv->sources_list);
  g_free (priv->sensors_list);

  g_hash_table_unref (priv->params);
  g_hash_table_unref (priv->sources);
  g_hash_table_unref (priv->sensors);
  g_hash_table_unref (priv->devices);

  g_clear_object (&priv->schema);
  g_object_unref (priv->writer);
  g_object_unref (priv->list);

  g_mutex_clear (&priv->lock);

  G_OBJECT_CLASS (hyscan_control_parent_class)->finalize (object);
}

/* Функция освобождает память занятую структурой HyScanControlSensorInfo */
static void
hyscan_control_free_sensor_info (gpointer data)
{
  HyScanControlSensorInfo *info = data;

  hyscan_sensor_info_sensor_free (info->info);
  hyscan_antenna_position_free (info->position);
  g_free (info);
}

/* Функция освобождает память занятую структурой HyScanControlSourceInfo */
static void
hyscan_control_free_source_info (gpointer data)
{
  HyScanControlSourceInfo *info = data;

  hyscan_sonar_info_source_free (info->info);
  hyscan_antenna_position_free (info->position);
  g_free (info);
}

/* функция создаёт схему устройства. */
static void
hyscan_control_create_device_schema (HyScanControlPrivate *priv)
{
  HyScanDataSchemaBuilder *builder;
  HyScanSensorSchema *sensor;
  HyScanSonarSchema *sonar;
  gchar *data;

  GHashTableIter iter;
  gpointer key, value;

  if (priv->binded)
    return;

  builder = hyscan_data_schema_builder_new ("control");
  sensor = hyscan_sensor_schema_new (builder);
  sonar = hyscan_sonar_schema_new (builder);

  /* Программое управление излучением. */
  if (priv->software_ping)
    hyscan_sonar_schema_set_software_ping (sonar);

  /* Добавляем датчики. */
  g_hash_table_iter_init (&iter, priv->sensors);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      HyScanControlSensorInfo *info = value;

      if (!hyscan_sensor_schema_add_full (sensor, info->info))
        goto exit;

      /* Местоположение по умолчанию. */
      if ((info->info->position == NULL) && (info->position != NULL))
        {
          if (!hyscan_sensor_schema_set_position (sensor, info->info->name, info->position))
            goto exit;
        }
    }

  /* Добавляем ведущие источники. */
  g_hash_table_iter_init (&iter, priv->sources);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      HyScanControlSourceInfo *info = value;

      if (info->info->master == HYSCAN_SOURCE_INVALID)
        {
          if (!hyscan_sonar_schema_source_add_full (sonar, info->info))
            goto exit;
        }
    }

  /* Добавляем ведомые источники и задаём местоположение по умолчанию. */
  g_hash_table_iter_init (&iter, priv->sources);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      HyScanControlSourceInfo *info = value;

      if (info->info->master != HYSCAN_SOURCE_INVALID)
        {
          if (!hyscan_sonar_schema_source_add_full (sonar, info->info))
            goto exit;
        }

      /* Местоположение по умолчанию. */
      if ((info->info->position == NULL) && (info->position != NULL))
        {
          if (!hyscan_sonar_schema_source_set_position (sonar, info->info->source, info->position))
           goto exit;
        }
    }

  /* Параметры устройств. */
  g_hash_table_iter_init (&iter, priv->devices);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      HyScanDataSchema *schema;
      gchar **keys;
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
          if (g_str_has_prefix (keys[i], "/params/") ||
              g_str_has_prefix (keys[i], "/system/") ||
              g_str_has_prefix (keys[i], "/state/"))
            {
              g_hash_table_insert (priv->params, g_strdup (keys[i]), value);
            }
        }

      g_object_unref (schema);
      g_strfreev (keys);
    }

  /* Создаём схему. */
  data = hyscan_data_schema_builder_get_data (builder);
  priv->schema = hyscan_data_schema_new_from_string (data, "control");
  g_free (data);

  /* Информация об устройствах для записи в галс. */
  g_object_unref (builder);
  builder = hyscan_data_schema_builder_new ("info");
  hyscan_data_schema_builder_schema_join (builder, "/info", priv->schema, "/info");
  hyscan_data_schema_builder_schema_join (builder, "/sources", priv->schema, "/sources");
  hyscan_data_schema_builder_schema_join (builder, "/sensors", priv->schema, "/sensors");
  data = hyscan_data_schema_builder_get_data (builder);
  hyscan_data_writer_set_sonar_info (priv->writer, data);
  g_free (data);

exit:
  g_object_unref (sonar);
  g_object_unref (sensor);
  g_object_unref (builder);
}

/* Обработчик сигнала sensor-data. */
static void
hyscan_control_sensor_data (HyScanDevice     *device,
                            const gchar      *sensor,
                            HyScanSourceType  source,
                            gint64            time,
                            HyScanBuffer     *data,
                            HyScanControl    *control)
{
  HyScanControlPrivate *priv = control->priv;
  HyScanControlSensorInfo *sensor_info;
  guint channel;

  if (!g_atomic_int_get (&priv->binded))
    return;

  sensor_info = g_hash_table_lookup (priv->sensors, sensor);
  if ((sensor_info == NULL) || (sensor_info->device != device))
    return;

  channel = g_atomic_int_get (&sensor_info->channel);
  hyscan_data_writer_sensor_add_data (priv->writer, sensor, source, channel, time, data);

  g_signal_emit_by_name (control, "sensor-data", sensor, source, time, data);
}

/* Обработчик сигнала sensor-log. */
static void
hyscan_control_sensor_log (HyScanDevice  *device,
                           const gchar   *source,
                           gint64         time,
                           gint           level,
                           const gchar   *message,
                           HyScanControl *control)
{
  HyScanControlPrivate *priv = control->priv;

  if (!g_atomic_int_get (&priv->binded))
    return;

  g_mutex_lock (&priv->lock);

  /* Если несколько сообщений приходит одновременно, разносим их на 1 мкс. */
  if (time <= priv->log_time)
    time = priv->log_time + 1;

  priv->log_time = time;

  g_mutex_unlock (&priv->lock);

  hyscan_data_writer_log_add_message (priv->writer, source, time, level, message);

  g_signal_emit_by_name (control, "sensor-log", source, time, level, message);
}

/* Обработчик сигнала sonar-signal. */
static void
hyscan_control_sonar_signal (HyScanDevice  *device,
                             gint           source,
                             gint64         time,
                             HyScanBuffer  *points,
                             HyScanControl *control)
{
  HyScanControlPrivate *priv = control->priv;
  HyScanControlSourceInfo *source_info;

  if (!g_atomic_int_get (&priv->binded))
    return;

  source_info = g_hash_table_lookup (priv->sources, GINT_TO_POINTER (source));
  if ((source_info == NULL) || (source_info->device != device))
    return;

  if (!hyscan_data_writer_raw_add_signal (priv->writer, source, time, points))
    {
      g_warning ("HyScanControl: can't set signal image for %s",
                 hyscan_source_get_name_by_type (source));
    }

  g_signal_emit_by_name (control, "sonar-signal", source, time, points);
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

  if (!hyscan_data_writer_raw_add_tvg (priv->writer, source, channel, time, gains))
    {
      g_warning ("HyScanControl: can't set tvg for %s",
                 hyscan_source_get_name_by_type (source));
    }

  g_signal_emit_by_name (control, "sonar-tvg", source, channel, time, gains);
}

/* Обработчик сигнала sonar-raw-data. */
static void
hyscan_control_sonar_raw_data (HyScanDevice      *device,
                               gint               source,
                               guint              channel,
                               gint64             time,
                               HyScanRawDataInfo *info,
                               HyScanBuffer      *data,
                               HyScanControl     *control)
{
  HyScanControlPrivate *priv = control->priv;
  HyScanControlSourceInfo *source_info;

  if (!g_atomic_int_get (&priv->binded))
    return;

  source_info = g_hash_table_lookup (priv->sources, GINT_TO_POINTER (source));
  if ((source_info == NULL) || (source_info->device != device))
    return;

  if (!hyscan_data_writer_raw_add_data (priv->writer, source, channel, time, info, data))
    {
      g_warning ("HyScanControl: can't add raw data for %s",
                 hyscan_source_get_name_by_type (source));
    }

  g_signal_emit_by_name (control, "sonar-raw-data", source, channel, time, info, data);
}

/* Обработчик сигнала sonar-noise-data. */
static void
hyscan_control_sonar_noise_data (HyScanDevice      *device,
                                 gint               source,
                                 guint              channel,
                                 gint64             time,
                                 HyScanRawDataInfo *info,
                                 HyScanBuffer      *data,
                                 HyScanControl     *control)
{
  HyScanControlPrivate *priv = control->priv;
  HyScanControlSourceInfo *source_info;

  if (!g_atomic_int_get (&priv->binded))
    return;

  source_info = g_hash_table_lookup (priv->sources, GINT_TO_POINTER (source));
  if ((source_info == NULL) || (source_info->device != device))
    return;

  if (!hyscan_data_writer_raw_add_noise (priv->writer, source, channel, time, info, data))
    {
      g_warning ("HyScanControl: can't add noise data for %s",
                 hyscan_source_get_name_by_type (source));
    }

  g_signal_emit_by_name (control, "sonar-noise-data", source, channel, time, info, data);
}

/* Обработчик сигнала sonar-acoustic-data. */
static void
hyscan_control_sonar_acoustic_data (HyScanDevice           *device,
                                    gint                    source,
                                    gint64                  time,
                                    HyScanAcousticDataInfo *info,
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

  if (!hyscan_data_writer_acoustic_add_data (control->priv->writer, source, time, info, data))
    {
      g_warning ("HyScanControl: can't add acoustic data for %s",
                 hyscan_source_get_name_by_type (source));
    }

  g_signal_emit_by_name (control, "sonar-acoustic-data", source, time, info, data);
}

/* Обработчик сигнала sonar-log. */
static void
hyscan_control_sonar_log (HyScanDevice  *device,
                          const gchar   *source,
                          gint64         time,
                          gint           level,
                          const gchar   *message,
                          HyScanControl *control)
{
  HyScanControlPrivate *priv = control->priv;

  if (!g_atomic_int_get (&priv->binded))
    return;

  g_mutex_lock (&priv->lock);

  /* Если несколько сообщений приходит одновременно, разносим их на 1 мкс. */
  if (time <= priv->log_time)
    time = priv->log_time + 1;

  priv->log_time = time;

  g_mutex_unlock (&priv->lock);

  hyscan_data_writer_log_add_message (priv->writer, source, time, level, message);

  g_signal_emit_by_name (control, "sonar-log", source, time, level, message);
}

/* Обработчик сигнала device-state. */
static void
hyscan_control_device_state (HyScanDevice     *device,
                             const gchar      *dev_id,
                             HyScanControl    *control)
{
  g_signal_emit_by_name (control, "device-state", dev_id);
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

  g_mutex_lock (&priv->lock);

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

  g_mutex_unlock (&priv->lock);

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

  g_mutex_lock (&priv->lock);

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

  g_mutex_unlock (&priv->lock);

  return status;
}

/* Метод HyScanSonar->set_sound_velocity. */
static gboolean
hyscan_control_sonar_set_sound_velocity (HyScanSonar *sonar,
                                         GList       *svp)
{
  HyScanControl *control = HYSCAN_CONTROL (sonar);
  HyScanControlPrivate *priv = control->priv;

  gboolean status = TRUE;

  GHashTableIter iter;
  gpointer device;

  if (!g_atomic_int_get (&priv->binded))
    return FALSE;

  g_hash_table_iter_init (&iter, priv->devices);
  while (g_hash_table_iter_next (&iter, NULL, &device))
    {
      if (!HYSCAN_IS_SONAR (device))
        continue;

      if (!hyscan_sonar_set_sound_velocity (device, svp))
        status = FALSE;
    }

  return status;
}

/* Метод HyScanSonar->receiver_set_time. */
static gboolean
hyscan_control_sonar_receiver_set_time (HyScanSonar      *sonar,
                                        HyScanSourceType  source,
                                        gdouble           receive_time)
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
                                         source, receive_time);
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

  return hyscan_sonar_receiver_set_auto (HYSCAN_SONAR (source_info->device),
                                         source);
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

/* Метод HyScanSonar->generator_set_auto. */
static gboolean
hyscan_control_sonar_generator_set_auto (HyScanSonar                    *sonar,
                                         HyScanSourceType                source,
                                         HyScanSonarGeneratorSignalType  signal)
{
  HyScanControl *control = HYSCAN_CONTROL (sonar);
  HyScanControlPrivate *priv = control->priv;

  HyScanControlSourceInfo *source_info;

  if (!g_atomic_int_get (&priv->binded))
    return FALSE;

  source_info = g_hash_table_lookup (priv->sources, GINT_TO_POINTER (source));
  if (source_info == NULL)
    return FALSE;

  return hyscan_sonar_generator_set_auto (HYSCAN_SONAR (source_info->device),
                                          source, signal);
}

/* Метод HyScanSonar->generator_set_simple. */
static gboolean
hyscan_control_sonar_generator_set_simple (HyScanSonar                    *sonar,
                                           HyScanSourceType                source,
                                           HyScanSonarGeneratorSignalType  signal,
                                           gdouble                         power)
{
  HyScanControl *control = HYSCAN_CONTROL (sonar);
  HyScanControlPrivate *priv = control->priv;

  HyScanControlSourceInfo *source_info;

  if (!g_atomic_int_get (&priv->binded))
    return FALSE;

  source_info = g_hash_table_lookup (priv->sources, GINT_TO_POINTER (source));
  if (source_info == NULL)
    return FALSE;

  return hyscan_sonar_generator_set_simple (HYSCAN_SONAR (source_info->device),
                                            source, signal, power);
}

/* Метод HyScanSonar->generator_set_extended. */
static gboolean
hyscan_control_sonar_generator_set_extended (HyScanSonar                    *sonar,
                                             HyScanSourceType                source,
                                             HyScanSonarGeneratorSignalType  signal,
                                             gdouble                         duration,
                                             gdouble                         power)
{
  HyScanControl *control = HYSCAN_CONTROL (sonar);
  HyScanControlPrivate *priv = control->priv;

  HyScanControlSourceInfo *source_info;

  if (!g_atomic_int_get (&priv->binded))
    return FALSE;

  source_info = g_hash_table_lookup (priv->sources, GINT_TO_POINTER (source));
  if (source_info == NULL)
    return FALSE;

  return hyscan_sonar_generator_set_extended (HYSCAN_SONAR (source_info->device),
                                              source, signal, duration, power);
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

/* Метод HyScanSonar->tvg_set_points. */
static gboolean
hyscan_control_sonar_tvg_set_points (HyScanSonar      *sonar,
                                     HyScanSourceType  source,
                                     gdouble           time_step,
                                     const gdouble    *gains,
                                     guint32           n_gains)
{
  HyScanControl *control = HYSCAN_CONTROL (sonar);
  HyScanControlPrivate *priv = control->priv;

  HyScanControlSourceInfo *source_info;

  if (!g_atomic_int_get (&priv->binded))
    return FALSE;

  source_info = g_hash_table_lookup (priv->sources, GINT_TO_POINTER (source));
  if (source_info == NULL)
    return FALSE;

  return hyscan_sonar_tvg_set_points (HYSCAN_SONAR (source_info->device),
                                      source, time_step, gains, n_gains);
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

/* Метод HyScanSonar->set_software_ping. */
static gboolean
hyscan_control_sonar_set_software_ping (HyScanSonar *sonar)
{
  HyScanControl *control = HYSCAN_CONTROL (sonar);
  HyScanControlPrivate *priv = control->priv;

  gboolean status = TRUE;

  GHashTableIter iter;
  gpointer device;

  if (!g_atomic_int_get (&priv->binded))
    return FALSE;

  g_hash_table_iter_init (&iter, priv->devices);
  while (g_hash_table_iter_next (&iter, NULL, &device))
    {
      if (!HYSCAN_IS_SONAR (device))
        continue;

      if (!hyscan_sonar_set_software_ping (device))
        status = FALSE;
    }

  return status;
}

/* Метод HyScanSonar->start. */
static gboolean
hyscan_control_sonar_start (HyScanSonar     *sonar,
                            const gchar     *project_name,
                            const gchar     *track_name,
                            HyScanTrackType  track_type)
{
  HyScanControl *control = HYSCAN_CONTROL (sonar);
  HyScanControlPrivate *priv = control->priv;

  gboolean status = TRUE;

  GHashTableIter iter;
  gpointer device;

  if (!g_atomic_int_get (&priv->binded))
    return FALSE;

  g_hash_table_iter_init (&iter, priv->devices);
  while (g_hash_table_iter_next (&iter, NULL, &device))
    {
      if (!HYSCAN_IS_SONAR (device))
        continue;

      if (!hyscan_sonar_start (device, project_name, track_name, track_type))
        status = FALSE;
    }

  if (status)
    status = hyscan_data_writer_start (priv->writer, project_name, track_name, track_type);
  if (!status)
    hyscan_control_sonar_stop (sonar);

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
  gpointer device;

  if (!g_atomic_int_get (&priv->binded))
    return FALSE;

  g_hash_table_iter_init (&iter, priv->devices);
  while (g_hash_table_iter_next (&iter, NULL, &device))
    {
      if (!HYSCAN_IS_SONAR (device))
        continue;

      if (!hyscan_sonar_stop (device))
        status = FALSE;
    }

  hyscan_data_writer_stop (priv->writer);

  return status;
}

/* Метод HyScanSonar->sync. */
static gboolean
hyscan_control_sonar_sync (HyScanSonar *sonar)
{
  HyScanControl *control = HYSCAN_CONTROL (sonar);
  HyScanControlPrivate *priv = control->priv;

  gboolean status = TRUE;

  GHashTableIter iter;
  gpointer device;

  if (!g_atomic_int_get (&priv->binded))
    return FALSE;

  g_hash_table_iter_init (&iter, priv->devices);
  while (g_hash_table_iter_next (&iter, NULL, &device))
    {
      if (!HYSCAN_IS_SONAR (device))
        continue;

      if (!hyscan_sonar_sync (device))
        status = FALSE;
    }

  return status;
}

/* Метод HyScanSonar->ping. */
static gboolean
hyscan_control_sonar_ping (HyScanSonar *sonar)
{
  HyScanControl *control = HYSCAN_CONTROL (sonar);
  HyScanControlPrivate *priv = control->priv;

  gboolean status = TRUE;

  GHashTableIter iter;
  gpointer device;

  if (!g_atomic_int_get (&priv->binded))
    return FALSE;

  g_hash_table_iter_init (&iter, priv->devices);
  while (g_hash_table_iter_next (&iter, NULL, &device))
    {
      if (!HYSCAN_IS_SONAR (device))
        continue;

      if (!hyscan_sonar_ping (device))
        status = FALSE;
    }

  return status;
}

/* Метод HyScanSensor->set_sound_velocity. */
static gboolean
hyscan_control_sensor_set_sound_velocity (HyScanSensor *sensor,
                                          GList        *svp)
{
  HyScanControl *control = HYSCAN_CONTROL (sensor);
  HyScanControlPrivate *priv = control->priv;

  gboolean status = TRUE;

  GHashTableIter iter;
  gpointer device;

  if (!g_atomic_int_get (&priv->binded))
    return FALSE;

  g_hash_table_iter_init (&iter, priv->devices);
  while (g_hash_table_iter_next (&iter, NULL, &device))
    {
      if (!HYSCAN_IS_SENSOR (device))
        continue;

      if (!hyscan_sensor_set_sound_velocity (device, svp))
        status = FALSE;
    }

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

  HyScanControlSensorInfo *sensor_info;

  if (!g_atomic_int_get (&priv->binded))
    return FALSE;

  sensor_info = g_hash_table_lookup (priv->sensors, name);
  if (sensor_info == NULL)
    return FALSE;

  return hyscan_sensor_set_enable (HYSCAN_SENSOR (sensor_info->device),
                                   name, enable);
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
  HyScanSensorInfo *sensor_info = NULL;
  HyScanSonarInfo *sonar_info = NULL;

  const HyScanSourceType *sources;
  const gchar * const *sensors;
  guint32 n_sources;
  guint32 n_sensors;
  guint i;

  gboolean status = FALSE;

  g_return_val_if_fail (HYSCAN_IS_CONTROL (control), FALSE);

  priv = control->priv;

  if (g_atomic_int_get (&control->priv->binded))
    return FALSE;

  /* Проверяем, что устройство реализует интерфейс HyScanParam и
   * минимум один из интерфейсов HyScanSensor или HyScanSonar. */
  if (!HYSCAN_IS_PARAM (device))
    return FALSE;
  if (!HYSCAN_IS_SENSOR (device) && !HYSCAN_IS_SONAR (device))
    return FALSE;

  /* Схема устройства и информация о нём. */
  device_schema = hyscan_param_schema (HYSCAN_PARAM (device));
  sonar_info = hyscan_sonar_info_new (device_schema);
  sensor_info = hyscan_sensor_info_new (device_schema);

  /* Список датчиков и устройств. */
  sensors = hyscan_sensor_info_list_sensors (sensor_info);
  sources = hyscan_sonar_info_list_sources (sonar_info, &n_sources);

  if (sensors != NULL)
    n_sensors = g_strv_length ((gchar**)sensors);
  else
    n_sensors = 0;

  g_mutex_lock (&priv->lock);

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

  /* Добавляем датчики. */
  if (sensors != NULL)
    {
      for (i = 0; i < n_sensors; i++)
        {
          HyScanControlSensorInfo *info = g_new0 (HyScanControlSensorInfo, 1);

          info->device = device;
          info->info = hyscan_sensor_info_sensor_copy (hyscan_sensor_info_get_sensor (sensor_info, sensors[i]));
          info->channel = 1;

          g_hash_table_insert (priv->sensors, g_strdup (sensors[i]), info);
        }
    }

  /* Добавляем гидролокационные источники данных. */
  if (sources != NULL)
    {
      /* Если устройство не поддерживает программное управление излучением,
       * отключаем эту возможность для всех устройств. */
      if (!hyscan_sonar_info_get_software_ping (sonar_info))
        priv->software_ping = FALSE;

      for (i = 0; i < n_sources; i++)
        {
          HyScanControlSourceInfo *info = g_new0 (HyScanControlSourceInfo, 1);

          info->device = device;
          info->info = hyscan_sonar_info_source_copy (hyscan_sonar_info_get_source (sonar_info, sources[i]));

          g_hash_table_insert (priv->sources, GINT_TO_POINTER (sources[i]), info);
        }
    }

  g_hash_table_insert (priv->devices, device_name, g_object_ref (device));
  device_name = NULL;

  /* Обработчики сигналов от датчиков. */
  if (HYSCAN_IS_SENSOR (device) && (n_sensors > 0))
    {
      g_signal_connect (device, "sensor-data",
                        G_CALLBACK (hyscan_control_sensor_data), control);
      g_signal_connect (device, "sensor-log",
                        G_CALLBACK (hyscan_control_sensor_log), control);
    }

  /* Обработчики сигналов от гидролокаторов. */
  if (HYSCAN_IS_SONAR (device) && (n_sources > 0))
    {
      g_signal_connect (device, "sonar-signal",
                        G_CALLBACK (hyscan_control_sonar_signal), control);
      g_signal_connect (device, "sonar-tvg",
                        G_CALLBACK (hyscan_control_sonar_tvg), control);
      g_signal_connect (device, "sonar-raw-data",
                        G_CALLBACK (hyscan_control_sonar_raw_data), control);
      g_signal_connect (device, "sonar-noise-data",
                        G_CALLBACK (hyscan_control_sonar_noise_data), control);
      g_signal_connect (device, "sonar-acoustic-data",
                        G_CALLBACK (hyscan_control_sonar_acoustic_data), control);
      g_signal_connect (device, "sonar-log",
                        G_CALLBACK (hyscan_control_sonar_log), control);
    }

  /* Обработчик сигнала device-state. */
  g_signal_connect (device, "device-state",
                    G_CALLBACK (hyscan_control_device_state), control);

  status = TRUE;

exit:
  g_mutex_unlock (&priv->lock);

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
 * Функция завершает конфигурирование объекта управления и переводит все
 * устройства в рабочий режим. После вызова этой функции, добавить новые
 * устройства нельзя.
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

  if (g_atomic_int_get (&priv->binded))
    return FALSE;

  g_mutex_lock (&priv->lock);

  /* Создаём новую схему конфигурации устройств. */
  hyscan_control_create_device_schema (priv);

  if (priv->schema != NULL)
    {
      GHashTableIter iter;
      gpointer key, value;

      guint n_sources;
      guint n_sensors;

      /* Список источников данных. */
      n_sources = g_hash_table_size (priv->sources);
      priv->sources_list = g_new0 (HyScanSourceType, n_sources);
      priv->n_sources = n_sources;

      n_sources = 0;
      g_hash_table_iter_init (&iter, priv->sources);
      while (g_hash_table_iter_next (&iter, &key, &value))
        {
          HyScanSourceType source = GPOINTER_TO_INT (key);
          HyScanControlSourceInfo *info = value;
          HyScanAntennaPosition *position;

          position = (info->position != NULL) ? info->position : info->info->position;
          hyscan_data_writer_sonar_set_position (priv->writer, source, position);

          priv->sources_list[n_sources++] = source;
        }

      /* Список датчиков. */
      n_sensors = g_hash_table_size (priv->sensors);
      priv->sensors_list = g_new0 (gchar*, n_sensors + 1);

      n_sensors = 0;
      g_hash_table_iter_init (&iter, priv->sensors);
      while (g_hash_table_iter_next (&iter, &key, &value))
        {
          const gchar *sensor = key;
          HyScanControlSensorInfo *info = value;
          HyScanAntennaPosition *position;

          position = (info->position != NULL) ? info->position : info->info->position;
          hyscan_data_writer_sensor_set_position (priv->writer, sensor, position);

          priv->sensors_list[n_sensors++] = key;
        }

      priv->binded = TRUE;
      status = TRUE;
    }

  g_mutex_unlock (&priv->lock);

  return status;
}

/**
 * hyscan_control_get_software_ping:
 * @control: указатель на #HyScanControl
 *
 * Функция возвращает признак программного управления синхронизацией излучения.
 * Программное управление излучением возможно только если все зарегистрированные
 * гидролокаторы поддерживают такую возможность.
 *
 * Returns: %TRUE если программное управление излучением возможно, иначе %FALSE.
 */
gboolean
hyscan_control_get_software_ping (HyScanControl *control)
{
  g_return_val_if_fail (HYSCAN_IS_CONTROL (control), FALSE);

  return g_atomic_int_get (&control->priv->software_ping);
}

/**
 * hyscan_control_list_sensors:
 * @control: указатель на #HyScanControl
 *
 * Функция возвращает список датчиков. Список возвращается в виде NULL
 * терминированного массива строк с названиями датчиков. Список принадлежит
 * объекту #HyScanControl и не должен изменяться пользователем.
 *
 * Returns: (transfer none): Список портов или NULL.
 */
const gchar * const *
hyscan_control_sensors_list (HyScanControl *control)
{
  g_return_val_if_fail (HYSCAN_IS_CONTROL (control), NULL);

  if (!g_atomic_int_get (&control->priv->binded))
    return NULL;

  return (const gchar **)control->priv->sensors_list;
}

/**
 * hyscan_control_list_sources:
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

  if (!g_atomic_int_get (&control->priv->binded))
    return NULL;

  *n_sources = control->priv->n_sources;

  return control->priv->sources_list;
}

/**
 * hyscan_control_get_sensor_info:
 * @control: указатель на #HyScanControl
 * @sensor: название датчика
 *
 * Функция возвращает параметры датчика.
 *
 * Returns: (transfer none): Параметры датчика или NULL.
 */
const HyScanSensorInfoSensor *
hyscan_control_sensor_get_info (HyScanControl *control,
                                const gchar   *sensor)
{
  HyScanControlSensorInfo *info;

  g_return_val_if_fail (HYSCAN_IS_CONTROL (control), NULL);

  if (!g_atomic_int_get (&control->priv->binded))
    return NULL;

  info = g_hash_table_lookup (control->priv->sensors, sensor);

  return (info != NULL) ? info->info : NULL;
}

/**
 * hyscan_control_get_source_info:
 * @control: указатель на #HyScanControl
 * @source: источник гидролокационных данных
 *
 * Функция возвращает параметры источника гидролокационных данных.
 *
 * Returns: (transfer none): Параметры гидролокационного источника данных или NULL.
 */
const HyScanSonarInfoSource *
hyscan_control_source_get_info (HyScanControl    *control,
                                HyScanSourceType  source)
{
  HyScanControlSourceInfo *info;

  g_return_val_if_fail (HYSCAN_IS_CONTROL (control), NULL);

  if (!g_atomic_int_get (&control->priv->binded))
    return NULL;

  info = g_hash_table_lookup (control->priv->sources, GINT_TO_POINTER (source));

  return (info != NULL) ? info->info : NULL;
}

/**
 * hyscan_control_get_sensor_info:
 * @control: указатель на #HyScanControl
 * @sensor: название датчика
 *
 * Функция возвращает признак доступности датчика.
 *
 * Returns: %TRUE если датчик доступен для работы, иначе %FALSE.
 */
gboolean
hyscan_control_sensor_get_enable (HyScanControl *control,
                                  const gchar   *sensor)
{
  HyScanControlPrivate *priv;
  HyScanControlSensorInfo *info;

  gpointer device;
  gchar key_id[128];
  gboolean enable;

  g_return_val_if_fail (HYSCAN_IS_CONTROL (control), FALSE);

  priv = control->priv;

  if (!g_atomic_int_get (&priv->binded))
    return FALSE;

  info = g_hash_table_lookup (priv->sensors, sensor);
  if (info == NULL)
    return FALSE;

  g_snprintf (key_id, sizeof (key_id), "/state/%s/enable", info->info->dev_id);
  device = g_hash_table_lookup (priv->params, key_id);
  if (device == NULL)
    return FALSE;

  g_mutex_lock (&priv->lock);

  hyscan_param_list_clear (priv->list);
  hyscan_param_list_add (priv->list, key_id);
  if (hyscan_param_get (device, priv->list))
    enable = hyscan_param_list_get_boolean (priv->list, key_id);
  else
    enable = FALSE;

  g_mutex_unlock (&priv->lock);

  return enable;
}

/**
 * hyscan_control_get_source_info:
 * @control: указатель на #HyScanControl
 * @source: источник гидролокационных данных
 *
 * Функция возвращает признак доступности источника данных.
 *
 * Returns: %TRUE если источника данных доступен для работы, иначе %FALSE.
 */
gboolean
hyscan_control_source_get_enable (HyScanControl    *control,
                                  HyScanSourceType  source)
{
  HyScanControlPrivate *priv;
  HyScanControlSensorInfo *info;

  gpointer device;
  gchar key_id[128];
  gboolean enable;

  g_return_val_if_fail (HYSCAN_IS_CONTROL (control), FALSE);

  priv = control->priv;

  if (!g_atomic_int_get (&priv->binded))
    return FALSE;

  info = g_hash_table_lookup (priv->sources, GINT_TO_POINTER (source));
  if (info == NULL)
    return FALSE;

  g_snprintf (key_id, sizeof (key_id), "/state/%s/enable", info->info->dev_id);
  device = g_hash_table_lookup (priv->params, key_id);
  if (device == NULL)
    return FALSE;

  g_mutex_lock (&priv->lock);

  hyscan_param_list_clear (priv->list);
  hyscan_param_list_add (priv->list, key_id);
  if (hyscan_param_get (device, priv->list))
    enable = hyscan_param_list_get_boolean (priv->list, key_id);
  else
    enable = FALSE;

  g_mutex_unlock (&priv->lock);

  return enable;
}

/**
 * hyscan_control_source_set_position:
 * @control: указатель на #HyScanControl
 * @sensor: название датчика
 * @position: (nullable): местоположение приёмной антенны
 *
 * Функция устанавливает информацию о местоположении приёмных антенн
 * датчика. Подробное описание параметров приводится в #HyScanTypes.
 *
 * Если для датчика задано местоположение по умолчанию, изменить его нельзя.
 *
 * Returns: %TRUE если команда выполнена успешно, иначе %FALSE.
 */
gboolean
hyscan_control_sensor_set_position (HyScanControl               *control,
                                    const gchar                 *sensor,
                                    const HyScanAntennaPosition *position)
{
  HyScanControlPrivate *priv;
  HyScanControlSensorInfo *info;
  gboolean status = FALSE;

  g_return_val_if_fail (HYSCAN_IS_CONTROL (control), FALSE);

  priv = control->priv;

  g_mutex_lock (&priv->lock);

  /* Если нет датчика или задано местоположение по умолчанию - выходим. */
  info = g_hash_table_lookup (priv->sensors, sensor);
  if ((info == NULL) || (info->info->position != NULL))
    goto exit;

  /* Запоминаем новое местоположение. */
  hyscan_antenna_position_free (info->position);
  info->position = hyscan_antenna_position_copy (position);
  hyscan_data_writer_sensor_set_position (priv->writer, sensor, position);

  status = TRUE;

exit:
  g_mutex_unlock (&priv->lock);

  return status;
}

/**
 * hyscan_control_source_set_position:
 * @control: указатель на #HyScanControl
 * @source: источник гидролокационных данных
 * @position: (nullable): местоположение приёмной антенны
 *
 * Функция устанавливает информацию о местоположении приёмных антенн
 * гидролокатора. Подробное описание параметров приводится в #HyScanTypes.
 *
 * Если для источника задано местоположение по умолчанию, изменить его нельзя.
 *
 * Returns: %TRUE если команда выполнена успешно, иначе %FALSE.
 */
gboolean
hyscan_control_source_set_position (HyScanControl               *control,
                                    HyScanSourceType             source,
                                    const HyScanAntennaPosition *position)
{
  HyScanControlPrivate *priv;
  HyScanControlSourceInfo *info;
  gboolean status = FALSE;

  g_return_val_if_fail (HYSCAN_IS_CONTROL (control), FALSE);

  priv = control->priv;

  g_mutex_lock (&priv->lock);

  /* Если нет источника или задано местоположение по умолчанию - выходим. */
  info = g_hash_table_lookup (priv->sources, GINT_TO_POINTER (source));
  if ((info == NULL) || (info->info->position != NULL))
    goto exit;

  /* Запоминаем новое местоположение. */
  hyscan_antenna_position_free (info->position);
  info->position = hyscan_antenna_position_copy (position);
  hyscan_data_writer_sonar_set_position (priv->writer, source, position);

  status = TRUE;

exit:
  g_mutex_unlock (&priv->lock);

  return status;
}

/**
 * hyscan_control_source_set_channel:
 * @control: указатель на #HyScanControl
 * @sensor: название датчика
 * @channel: номер приёмного канала
 *
 * Функция устанавливает номер приёмного канала для датчика.
 *
 * Returns: %TRUE если команда выполнена успешно, иначе %FALSE.
 */
gboolean
hyscan_control_sensor_set_channel (HyScanControl *control,
                                   const gchar   *sensor,
                                   guint          channel)
{
  HyScanControlPrivate *priv;
  HyScanControlSensorInfo *info;

  g_return_val_if_fail (HYSCAN_IS_CONTROL (control), FALSE);

  priv = control->priv;

  if (!g_atomic_int_get (&priv->binded))
    return FALSE;

  info = g_hash_table_lookup (priv->sensors, sensor);
  if (info == NULL)
    return FALSE;

  g_atomic_int_set (&info->channel, channel);

  return TRUE;
}

/**
 * hyscan_control_writer_set_db:
 * @control: указатель на #HyScanControl
 * @db: указатель на #HyScanDB
 *
 * Функция устанавливает систему хранения данных. Система хранения может быть
 * установлена только в режиме ожидания.
 */
gboolean
hyscan_control_writer_set_db (HyScanControl *control,
                              HyScanDB      *db)
{
  g_return_val_if_fail (HYSCAN_IS_CONTROL (control), FALSE);

  return hyscan_data_writer_set_db (control->priv->writer, db);
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
  g_return_if_fail (HYSCAN_IS_CONTROL (control));

  hyscan_data_writer_set_operator_name (control->priv->writer, name);
}

/**
 * hyscan_control_writer_set_mode:
 * @control: указатель на #HyScanControl
 * @mode: режим записи данных #HyScanDataWriterModeType
 *
 * Функция устанавливает режим записи данных от гидролокатора.
 *
 * Returns: %TRUE если команда выполнена успешно, иначе %FALSE.
 */
gboolean
hyscan_control_writer_set_mode (HyScanControl            *control,
                                HyScanDataWriterModeType  mode)
{
  g_return_val_if_fail (HYSCAN_IS_CONTROL (control), FALSE);

  return hyscan_data_writer_set_mode (control->priv->writer, mode);
}

/**
 * hyscan_control_writer_set_chunk_size:
 * @control: указатель на #HyScanControl
 * @chunk_size: максимальный размер файлов в байтах или отрицательное число
 *
 * Функция устанавливает максимальный размер файлов в галсе.
 *
 * Подробнее об этом можно прочитать в описании интерфейса #HyScanDB.
 *
 * Returns: %TRUE если команда выполнена успешно, иначе %FALSE.
 */
gboolean
hyscan_control_writer_set_chunk_size (HyScanControl *control,
                                      gint32         chunk_size)
{
  g_return_val_if_fail (HYSCAN_IS_CONTROL (control), FALSE);

  return hyscan_data_writer_set_chunk_size (control->priv->writer, chunk_size);
}

/**
 * hyscan_control_writer_set_save_time:
 * @control: указатель на #HyScanControl
 * @save_time: время хранения данных в микросекундах или отрицательное число
 *
 * Функция задаёт интервал времени, для которого сохраняются записываемые
 * данные. Если данные были записаны ранее "текущего времени" - "интервал
 * хранения" они удаляются.
 *
 * Подробнее об этом можно прочитать в описании интерфейса #HyScanDB.
 *
 * Returns: %TRUE если команда выполнена успешно, иначе %FALSE.
 */
gboolean
hyscan_control_writer_set_save_time (HyScanControl *control,
                                     gint64         save_time)
{
  g_return_val_if_fail (HYSCAN_IS_CONTROL (control), FALSE);

  return hyscan_data_writer_set_save_time (control->priv->writer, save_time);
}

/**
 * hyscan_control_writer_set_save_size:
 * @control: указатель на #HyScanControl
 * @save_size: объём сохраняемых данных в байтах или отрицательное число
 *
 * Функция задаёт объём сохраняемых данных в канале. Если объём данных
 * превышает этот предел, старые данные удаляются.
 *
 * Подробнее об этом можно прочитать в описании интерфейса #HyScanDB.
 *
 * Returns: %TRUE если команда выполнена успешно, иначе %FALSE.
 */
gboolean
hyscan_control_writer_set_save_size (HyScanControl *control,
                                     gint64         save_size)
{
  g_return_val_if_fail (HYSCAN_IS_CONTROL (control), FALSE);

  return hyscan_data_writer_set_save_size (control->priv->writer, save_size);
}

static void
hyscan_control_param_interface_init (HyScanParamInterface *iface)
{
  iface->schema = hyscan_control_param_schema;
  iface->set = hyscan_control_param_set;
  iface->get = hyscan_control_param_get;
}

static void
hyscan_control_sonar_interface_init (HyScanSonarInterface *iface)
{
  iface->set_sound_velocity = hyscan_control_sonar_set_sound_velocity;
  iface->receiver_set_time = hyscan_control_sonar_receiver_set_time;
  iface->receiver_set_auto = hyscan_control_sonar_receiver_set_auto;
  iface->generator_set_preset = hyscan_control_sonar_generator_set_preset;
  iface->generator_set_auto = hyscan_control_sonar_generator_set_auto;
  iface->generator_set_simple = hyscan_control_sonar_generator_set_simple;
  iface->generator_set_extended = hyscan_control_sonar_generator_set_extended;
  iface->tvg_set_auto = hyscan_control_sonar_tvg_set_auto;
  iface->tvg_set_points = hyscan_control_sonar_tvg_set_points;
  iface->tvg_set_linear_db = hyscan_control_sonar_tvg_set_linear_db;
  iface->tvg_set_logarithmic = hyscan_control_sonar_tvg_set_logarithmic;
  iface->set_software_ping = hyscan_control_sonar_set_software_ping;
  iface->start = hyscan_control_sonar_start;
  iface->stop = hyscan_control_sonar_stop;
  iface->sync = hyscan_control_sonar_sync;
  iface->ping = hyscan_control_sonar_ping;
}

static void
hyscan_control_sensor_interface_init (HyScanSensorInterface *iface)
{
  iface->set_sound_velocity = hyscan_control_sensor_set_sound_velocity;
  iface->set_enable = hyscan_control_sensor_set_enable;
}
