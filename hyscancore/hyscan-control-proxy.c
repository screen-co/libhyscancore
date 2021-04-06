/* hyscan-control-proxy.c
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

/**
 * SECTION: hyscan-control-proxy
 * @Short_description: класс управления гидролокаторами и датчиками в режиме прокси
 * @Title: HyScanControlProxy
 *
 * Класс предназначен для проксирования доступа к устройствам с целью
 * выполнения обработки данных. Под обработкой подразумевается вычисление
 * амплитуды для акустических данных и прореживание по точкам и строкам.
 * Обработка данных выполняется в фоновом режиме. Если за период зондирования
 * обработка не была завершена, новые данные отбрасываются. Это позволяет
 * использовать класс для выполнения предварительного просмотра данных в
 * пониженном разрешении, без остановки записи полных данных.
 *
 * При создании объекта необходимо передать указатель на #HyScanControl, в
 * который будут отправляться все запросы.
 *
 * Управление прореживанием данных осуществляется с помощью функции
 * #hyscan_control_proxy_set_scale. Тип выходных данных задаётся функцией
 * #hyscan_control_proxy_set_data_type.
 *
 * Список датчиков и источников данных можно получить с помощью функций
 * #hyscan_control_proxy_sources_list и #hyscan_control_proxy_sensors_list.
 *
 * Информацию о датчиках и источниках данных можно получить с помощью функций
 * #hyscan_control_proxy_sensor_get_info и #hyscan_control_proxy_source_get_info.
 *
 * Управлять включением и отключением пересылки данных (и соответственно их
 * обработкой) можно с помощью функций #hyscan_control_proxy_sensor_set_sender
 * и #hyscan_control_proxy_source_set_sender.
 */

#include "hyscan-control-proxy.h"

#include <hyscan-convolution.h>
#include <hyscan-data-schema-builder.h>
#include <hyscan-param-controller.h>
#include <hyscan-param-proxy.h>
#include <hyscan-buffer.h>

#include <hyscan-device-driver.h>
#include <hyscan-sensor-driver.h>
#include <hyscan-sonar-driver.h>

#include <glib/gi18n-lib.h>
#include <math.h>

#define AQ_MAX_SCALE           32                /* Максимальный коэффициент прореживания. */

#define LOG_SRC_SZ             250               /* максимальная длина названия источника сообщения. */
#define LOG_MSG_SZ             750               /* Максимальная длина сообщения. */
#define LOG_BUF_SZ             16                /* Размер буфера сообщений. */
#define AQ_BUF_SZ              4                 /* Размер буфера акустических данных. */

#define PROXY_DATA_TYPES       "data-types"      /* ENUM идентификатор типов экспортируемых данных. */

#define PROXY_DATA_TYPE        "data-type"       /* Параметр типа экспортируемых данных. */
#define PROXY_LINE_SCALE       "line-scale"      /* Параметр прореживания строк. */
#define PROXY_POINT_SCALE      "point-scale"     /* Параметр прореживания точек. */

#define PROXY_STAT             "stat"            /* Ветка статистики. */
#define PROXY_STAT_TOTAL       "stat/total"      /* Ветка статистики принятых данных. */
#define PROXY_STAT_DROPPED     "stat/dropped"    /* Ветка статистики отброшенных данных. */

#define PROXY_PARAM_NAME(...)  hyscan_param_name_constructor (key_id, \
                                 (guint)sizeof (key_id), "params", __VA_ARGS__)

#define PROXY_SYSTEM_NAME(...) hyscan_param_name_constructor (key_id, \
                                 (guint)sizeof (key_id), "system", __VA_ARGS__)

enum
{
  PROP_O,
  PROP_CONTROL,
  PROP_DEV_ID
};

typedef enum
{
  HYSCAN_CONTROL_PROXY_EMPTY,                   /* Буфер пуст. */
  HYSCAN_CONTROL_PROXY_PROCESS                  /* Данные в процессе обработки. */
} HyScanControlProxyStatus;

typedef struct
{
  HyScanControlProxyStatus     status;           /* Статус буфера сообщения. */
  gint64                       time;             /* Метка времени сообщения. */
  HyScanLogLevel               level;            /* Тип информационного сообщения. */
  gchar                        src[LOG_SRC_SZ];  /* Источник. */
  gchar                        msg[LOG_MSG_SZ];  /* Сообщение. */
} HyScanControlProxyLog;

typedef struct
{
  gint64                       time;             /* Время начала действия сигнала. */
  HyScanBuffer                *image;            /* Образ сигнала для свёртки. */
  HyScanConvolution           *conv;             /* Объект свёртки данных. */
  GMutex                       lock;             /* Блокировка. */
} HyScanControlProxySignal;

typedef struct
{
  HyScanControlProxyStatus     status;           /* Статус буфера данных. */
  gint64                       time;             /* Метка времени данных. */
  HyScanAcousticDataInfo       info;             /* Информация о данных. */
  HyScanBuffer                *data;             /* Данные. */
} HyScanControlProxyData;

typedef struct
{
  gboolean                     enable;           /* Признак необходимости пересылки данных. */
  gchar                       *description;      /* Описание источника данных. */
  gchar                       *actuator;         /* Название привода. */
  gboolean                     start;            /* Признак начала галса. */
  HyScanControlProxyData       data[AQ_BUF_SZ];  /* Буферы обработки акустических данных. */
  HyScanBuffer                *import;           /* Временный буфер акустических данных. */
  HyScanControlProxySignal     signal;           /* Образ сигнала для свёртки. */
  gboolean                     send;             /* Признак принудительной отправки текущих данных. */
  gint64                       new_data_type;    /* Установленный тип экспортируемых данных. */
  gint64                       cur_data_type;    /* Текущий тип экспортируемых данных. */
  gint64                       new_line_scale;   /* Установленное прореживание по строкам. */
  gint64                       cur_line_scale;   /* Текущее прореживание по строкам. */
  gint64                       new_point_scale;  /* Установленное масштабирование по точкам. */
  gint64                       cur_point_scale;  /* Текущее масштабирование по точкам. */
  guint                        line_counter;     /* Счётчик прореживания строк. */
  gint64                       received;         /* Число принятых строк. */
  gint64                       dropped;          /* Число отброшенных строк из-за переполнения. */
} HyScanControlProxyAcoustic;

typedef struct
{
  gboolean                     enable;           /* Признак необходимости пересылки данных. */
  HyScanControlProxyStatus     status;           /* Статус буфера данных. */
  HyScanSourceType             source;           /* Тип источника данных. */
  gint64                       time;             /* Метка времени. */
  HyScanBuffer                *data;             /* Данные. */
  gint64                       received;         /* Число принятых пакетов. */
  gint64                       dropped;          /* Число отброшенных пакетов из-за переполнения. */
} HyScanControlProxySensor;

struct _HyScanControlProxyPrivate
{
  gchar                       *dev_id;           /* Идентификатор "прокси" устройства. */

  HyScanControl               *control;          /* Управление устройством. */
  HyScanParam                 *param;            /* Интерфейс HyScanParam устройства. */
  HyScanDevice                *device;           /* Интерфейс HyScanDevice устройства. */
  HyScanSonar                 *sonar;            /* Интерфейс HyScanSonar устройства. */
  HyScanSensor                *sensor;           /* Интерфейс HyScanSensor устройства. */
  HyScanActuator              *actuator;         /* Интерфейс HyScanActuator устройства. */

  GHashTable                  *sources;          /* Буферы данных источников. */
  GHashTable                  *sensors;          /* Буферы данных датчиков. */

  gboolean                     logs_enable;      /* Признак необходимости пересылки сообщений. */
  HyScanControlProxyLog        logs[LOG_BUF_SZ]; /* Буферы сообщений. */

  gboolean                     shutdown;         /* Признак завершения работы. */
  gboolean                     started;          /* Признак рабочего режима. */

  GThread                     *sender;           /* Поток отправки данных. */
  GMutex                       lock;             /* Блокировка. */
  GCond                        cond;             /* Сигнализатор обработки. */
};

static void      hyscan_control_proxy_param_interface_init     (HyScanParamInterface    *iface);
static void      hyscan_control_proxy_device_interface_init    (HyScanDeviceInterface   *iface);
static void      hyscan_control_proxy_sonar_interface_init     (HyScanSonarInterface    *iface);
static void      hyscan_control_proxy_sensor_interface_init    (HyScanSensorInterface   *iface);
static void      hyscan_control_proxy_actuator_interface_init  (HyScanActuatorInterface *iface);

static void      hyscan_control_proxy_set_property             (GObject                 *object,
                                                                guint                    prop_id,
                                                                const GValue            *value,
                                                                GParamSpec              *pspec);
static void      hyscan_control_proxy_object_constructed       (GObject                 *object);
static void      hyscan_control_proxy_object_finalize          (GObject                 *object);

static HyScanDataSchema *
                 hyscan_control_proxy_create_schema            (HyScanControl           *control,
                                                                const gchar             *dev_id);

static void      hyscan_control_proxy_disconnect               (HyScanControlProxy      *proxy);

static void      hyscan_control_proxy_source_free              (gpointer                 data);
static void      hyscan_control_proxy_sensor_free              (gpointer                 data);

static void      hyscan_control_proxy_device_state             (HyScanControlProxy      *proxy,
                                                                const gchar             *dev_id);

static void      hyscan_control_proxy_device_log               (HyScanControlProxy      *proxy,
                                                                const gchar             *source,
                                                                gint64                   time,
                                                                gint                     level,
                                                                const gchar             *message);

static void      hyscan_control_proxy_sonar_signal             (HyScanControlProxy      *proxy,
                                                                gint                     source,
                                                                guint                    channel,
                                                                gint64                   time,
                                                                HyScanBuffer            *image);

static void      hyscan_control_proxy_sonar_acoustic_data      (HyScanControlProxy      *proxy,
                                                                gint                     source,
                                                                guint                    channel,
                                                                gboolean                 noise,
                                                                gint64                   time,
                                                                HyScanAcousticDataInfo  *info,
                                                                HyScanBuffer            *data);

static void      hyscan_control_proxy_sensor_data              (HyScanControlProxy      *proxy,
                                                                const gchar             *sensor,
                                                                gint                     source,
                                                                gint64                   time,
                                                                HyScanBuffer            *data);

static gpointer  hyscan_control_proxy_sender                   (gpointer                 user_data);

G_DEFINE_TYPE_WITH_CODE (HyScanControlProxy, hyscan_control_proxy, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (HyScanControlProxy)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_PARAM,    hyscan_control_proxy_param_interface_init)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_DEVICE,   hyscan_control_proxy_device_interface_init)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_SENSOR,   hyscan_control_proxy_sensor_interface_init)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_SONAR,    hyscan_control_proxy_sonar_interface_init)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_ACTUATOR, hyscan_control_proxy_actuator_interface_init))

static void
hyscan_control_proxy_class_init (HyScanControlProxyClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_control_proxy_set_property;

  object_class->constructed = hyscan_control_proxy_object_constructed;
  object_class->finalize = hyscan_control_proxy_object_finalize;

  g_object_class_install_property (object_class, PROP_CONTROL,
    g_param_spec_object ("control", "Control", "Device controller", HYSCAN_TYPE_CONTROL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_DEV_ID,
    g_param_spec_string ("dev-id", "DeviceID", "Device identification", NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_control_proxy_init (HyScanControlProxy *proxy)
{
  proxy->priv = hyscan_control_proxy_get_instance_private (proxy);
}

static void
hyscan_control_proxy_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  HyScanControlProxy *proxy = HYSCAN_CONTROL_PROXY (object);
  HyScanControlProxyPrivate *priv = proxy->priv;

  switch (prop_id)
    {
    case PROP_CONTROL:
      priv->control = g_value_dup_object (value);
      break;

    case PROP_DEV_ID:
      priv->dev_id = g_value_dup_string (value);
      priv->dev_id = (priv->dev_id == NULL) ? g_strdup ("proxy") : priv->dev_id;
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_control_proxy_object_constructed (GObject *object)
{
  HyScanControlProxy *proxy = HYSCAN_CONTROL_PROXY (object);
  HyScanControlProxyPrivate *priv = proxy->priv;

  HyScanDataSchema *proxy_schema;
  HyScanParamController *proxy_config;
  HyScanParamProxy *proxy_param;
  gchar key_id[128];

  const HyScanSourceType *sources;
  const gchar * const *sensors;
  guint32 n_sources;
  guint32 i, j;

  /* Блокируем мьютекс, так как он используется только в g_cond_wait_until,
   * который разблокирует его при вызове и заблокирует при получении сигнала.
   * Сам мьютекс нам не нужен, он нужен только для g_cond_wait_until. */
  g_cond_init (&priv->cond);
  g_mutex_init (&priv->lock);
  g_mutex_lock (&priv->lock);

  /* Обязательно должен быть указан объект управления устройством. */
  if (priv->control == NULL)
    return;

  /* Схема "прокси" устройства. */
  proxy_config = hyscan_param_controller_new (NULL);
  proxy_schema = hyscan_control_proxy_create_schema (priv->control, priv->dev_id);
  hyscan_param_controller_set_schema (proxy_config, proxy_schema);

  /* Объединяем схемы "прокси" и реального устройств. */
  proxy_param = hyscan_param_proxy_new ();
  if (!hyscan_param_proxy_add (proxy_param, "/", HYSCAN_PARAM (priv->control), "/") ||
      !hyscan_param_proxy_add (proxy_param, "/", HYSCAN_PARAM (proxy_config), "/") ||
      !hyscan_param_proxy_bind (proxy_param))
    {
      return;
    }

  priv->device = HYSCAN_DEVICE (priv->control);
  priv->sonar = HYSCAN_SONAR (priv->control);
  priv->sensor = HYSCAN_SENSOR (priv->control);
  priv->actuator = HYSCAN_ACTUATOR (priv->control);
  priv->param = HYSCAN_PARAM (proxy_param);

  /* Таблицы буферов данных. */
  priv->sources = g_hash_table_new_full (NULL, NULL,
                                         NULL, hyscan_control_proxy_source_free);
  priv->sensors = g_hash_table_new_full (g_str_hash, g_str_equal,
                                         g_free, hyscan_control_proxy_sensor_free);

  /* Буферы гидролокационных источников данных. */
  sources = hyscan_control_sources_list (priv->control, &n_sources);
  if (sources != NULL)
    {
      for (i = 0; i < n_sources; i++)
        {
          HyScanControlProxyAcoustic *buffer = g_slice_new0 (HyScanControlProxyAcoustic);
          const gchar *source_id = hyscan_source_get_id_by_type (sources[i]);

          g_mutex_init (&buffer->signal.lock);
          buffer->new_data_type = HYSCAN_DATA_AMPLITUDE_INT16LE;
          buffer->new_line_scale = 1;
          buffer->new_point_scale = 1;
          buffer->signal.image = hyscan_buffer_new ();
          buffer->signal.conv = hyscan_convolution_new ();
          buffer->import = hyscan_buffer_new ();
          for (j = 0; j < AQ_BUF_SZ; j++)
            buffer->data[j].data = hyscan_buffer_new ();

          g_hash_table_insert (priv->sources, GINT_TO_POINTER (sources[i]), buffer);

          PROXY_PARAM_NAME (priv->dev_id, source_id, PROXY_DATA_TYPE, NULL);
          hyscan_param_controller_add_integer (proxy_config, key_id, &buffer->new_data_type);

          PROXY_PARAM_NAME (priv->dev_id, source_id, PROXY_LINE_SCALE, NULL);
          hyscan_param_controller_add_integer (proxy_config, key_id, &buffer->new_line_scale);

          PROXY_PARAM_NAME (priv->dev_id, source_id, PROXY_POINT_SCALE, NULL);
          hyscan_param_controller_add_integer (proxy_config, key_id, &buffer->new_point_scale);

          PROXY_SYSTEM_NAME (priv->dev_id, PROXY_STAT_TOTAL, source_id, NULL);
          hyscan_param_controller_add_integer (proxy_config, key_id, &buffer->received);

          PROXY_SYSTEM_NAME (priv->dev_id, PROXY_STAT_DROPPED, source_id, NULL);
          hyscan_param_controller_add_integer (proxy_config, key_id, &buffer->dropped);
        }
    }

  /* Буферы данных датчиков. */
  sensors = hyscan_control_sensors_list (priv->control);
  if (sensors != NULL)
    {
      for (i = 0; sensors[i] != NULL; i++)
        {
          HyScanControlProxySensor *buffer;

          buffer = g_slice_new0 (HyScanControlProxySensor);
          buffer->data = hyscan_buffer_new ();

          g_hash_table_insert (priv->sensors, g_strdup (sensors[i]), buffer);

          PROXY_SYSTEM_NAME (priv->dev_id, PROXY_STAT_TOTAL, sensors[i], NULL);
          hyscan_param_controller_add_integer (proxy_config, key_id, &buffer->received);

          PROXY_SYSTEM_NAME (priv->dev_id, PROXY_STAT_DROPPED, sensors[i], NULL);
          hyscan_param_controller_add_integer (proxy_config, key_id, &buffer->dropped);
        }
    }

  /* Поток отправки данных. */
  priv->sender = g_thread_new ("proxy-sender", hyscan_control_proxy_sender, proxy);

  /* Обработчики данных от гидролокатора и датчиков. */
  g_signal_connect_swapped (priv->device, "device-state",
                            G_CALLBACK (hyscan_control_proxy_device_state), proxy);
  g_signal_connect_swapped (priv->device, "device-log",
                            G_CALLBACK (hyscan_control_proxy_device_log), proxy);
  g_signal_connect_swapped (priv->sonar, "sonar-signal",
                            G_CALLBACK (hyscan_control_proxy_sonar_signal), proxy);
  g_signal_connect_swapped (priv->sonar, "sonar-acoustic-data",
                            G_CALLBACK (hyscan_control_proxy_sonar_acoustic_data), proxy);
  g_signal_connect_swapped (priv->sensor, "sensor-data",
                            G_CALLBACK (hyscan_control_proxy_sensor_data), proxy);

  g_object_unref (proxy_schema);
  g_object_unref (proxy_config);
}

static void
hyscan_control_proxy_object_finalize (GObject *object)
{
  HyScanControlProxy *proxy = HYSCAN_CONTROL_PROXY (object);
  HyScanControlProxyPrivate *priv = proxy->priv;

  hyscan_control_proxy_disconnect (proxy);

  g_free (priv->dev_id);
  g_clear_object (&priv->param);
  g_clear_object (&priv->control);
  g_clear_pointer (&priv->sources, g_hash_table_unref);
  g_clear_pointer (&priv->sensors, g_hash_table_unref);

  /* Не забываем разблокировать мьютекс перед его удалением, см. constructed. */
  g_mutex_unlock (&priv->lock);
  g_mutex_clear (&priv->lock);
  g_cond_clear (&priv->cond);

  G_OBJECT_CLASS (hyscan_control_proxy_parent_class)->finalize (object);
}

/* Функция создаёт схему устройства. */
static HyScanDataSchema *
hyscan_control_proxy_create_schema (HyScanControl *control,
                                    const gchar   *dev_id)
{
  HyScanDataSchemaBuilder *builder;
  HyScanDataSchema *schema;
  gchar key_id[128];

  const HyScanSourceType *sources;
  const gchar * const *sensors;
  guint32 n_sources;
  guint32 i;

  builder = hyscan_data_schema_builder_new ("proxy");

  hyscan_data_schema_builder_enum_create       (builder, PROXY_DATA_TYPES);
  hyscan_data_schema_builder_enum_value_create (builder, PROXY_DATA_TYPES,
                                                HYSCAN_DATA_AMPLITUDE_INT8,
                                                "Unsigned 8bit", _("Unsigned 8bit"), NULL);
  hyscan_data_schema_builder_enum_value_create (builder, PROXY_DATA_TYPES,
                                                HYSCAN_DATA_AMPLITUDE_INT16LE,
                                                "Unsigned 16bit LE", _("Unsigned 16bit LE"), NULL);
  hyscan_data_schema_builder_enum_value_create (builder, PROXY_DATA_TYPES,
                                                HYSCAN_DATA_AMPLITUDE_INT24LE,
                                                "Unsigned 24bit LE", _("Unsigned 24bit LE"), NULL);
  hyscan_data_schema_builder_enum_value_create (builder, PROXY_DATA_TYPES,
                                                HYSCAN_DATA_AMPLITUDE_INT32LE,
                                                "Unsigned 32bit LE", _("Unsigned 32bit LE"), NULL);
  hyscan_data_schema_builder_enum_value_create (builder, PROXY_DATA_TYPES,
                                                HYSCAN_DATA_AMPLITUDE_FLOAT16LE,
                                                "Float 16bit LE", _("Float 16bit LE"), NULL);
  hyscan_data_schema_builder_enum_value_create (builder, PROXY_DATA_TYPES,
                                                HYSCAN_DATA_AMPLITUDE_FLOAT32LE,
                                                "Float 32bit LE", _("Float 32bit LE"), NULL);

  PROXY_PARAM_NAME (dev_id, NULL);
  hyscan_data_schema_builder_node_set_name (builder, key_id, _("Proxy"), dev_id);

  PROXY_SYSTEM_NAME (dev_id, NULL);
  hyscan_data_schema_builder_node_set_name (builder, key_id, _("Proxy"), dev_id);

  PROXY_SYSTEM_NAME (dev_id, PROXY_STAT, NULL);
  hyscan_data_schema_builder_node_set_name (builder, key_id, _("Statistics"), NULL);

  PROXY_SYSTEM_NAME (dev_id, PROXY_STAT_TOTAL, NULL);
  hyscan_data_schema_builder_node_set_name (builder, key_id, _("Total received"), NULL);

  PROXY_SYSTEM_NAME (dev_id, PROXY_STAT_DROPPED, NULL);
  hyscan_data_schema_builder_node_set_name (builder, key_id, _("Dropped"), NULL);

  sources = hyscan_control_sources_list (control, &n_sources);
  if (sources != NULL)
    {
      for (i = 0; i < n_sources; i++)
        {
          const gchar *source_id = hyscan_source_get_id_by_type (sources[i]);
          const gchar *source_name = hyscan_source_get_name_by_type (sources[i]);

          PROXY_PARAM_NAME (dev_id, source_id, NULL);
          hyscan_data_schema_builder_node_set_name (builder, key_id, source_name, NULL);

          PROXY_PARAM_NAME (dev_id, source_id, PROXY_DATA_TYPE, NULL);
          hyscan_data_schema_builder_key_enum_create (builder, key_id, _("Data type"), NULL,
                                                      PROXY_DATA_TYPES,
                                                      HYSCAN_DATA_AMPLITUDE_INT16LE);

          PROXY_PARAM_NAME (dev_id, source_id, PROXY_LINE_SCALE, NULL);
          hyscan_data_schema_builder_key_integer_create (builder, key_id, _("Line scale"), NULL, 1);
          hyscan_data_schema_builder_key_integer_range  (builder, key_id, 1, AQ_MAX_SCALE, 1);

          PROXY_PARAM_NAME (dev_id, source_id, PROXY_POINT_SCALE, NULL);
          hyscan_data_schema_builder_key_integer_create (builder, key_id, _("Point scale"), NULL, 1);
          hyscan_data_schema_builder_key_integer_range  (builder, key_id, 1, AQ_MAX_SCALE, 1);

          PROXY_SYSTEM_NAME (dev_id, PROXY_STAT_TOTAL, source_id, NULL);
          hyscan_data_schema_builder_key_integer_create (builder, key_id, source_name, NULL, 0);
          hyscan_data_schema_builder_key_set_access (builder, key_id, HYSCAN_DATA_SCHEMA_ACCESS_READ);

          PROXY_SYSTEM_NAME (dev_id, PROXY_STAT_DROPPED, source_id, NULL);
          hyscan_data_schema_builder_key_integer_create (builder, key_id, source_name, NULL, 0);
          hyscan_data_schema_builder_key_set_access (builder, key_id, HYSCAN_DATA_SCHEMA_ACCESS_READ);
        }
    }

  sensors = hyscan_control_sensors_list (control);
  if (sensors != NULL)
    {
      for (i = 0; sensors[i] != NULL; i++)
        {
          const HyScanSensorInfoSensor *info;

          info = hyscan_control_sensor_get_info (control, sensors[i]);

          PROXY_SYSTEM_NAME (dev_id, PROXY_STAT_TOTAL, sensors[i], NULL);
          hyscan_data_schema_builder_key_integer_create (builder, key_id, info->description, NULL, 0);
          hyscan_data_schema_builder_key_set_access (builder, key_id, HYSCAN_DATA_SCHEMA_ACCESS_READ);

          PROXY_SYSTEM_NAME (dev_id, PROXY_STAT_DROPPED, sensors[i], NULL);
          hyscan_data_schema_builder_key_integer_create (builder, key_id, info->description, NULL, 0);
          hyscan_data_schema_builder_key_set_access (builder, key_id, HYSCAN_DATA_SCHEMA_ACCESS_READ);
        }
    }

  schema = hyscan_data_schema_builder_get_schema (builder);
  g_object_unref (builder);

  return schema;
}

/* Функция производит отключение от устройства. */
static void
hyscan_control_proxy_disconnect (HyScanControlProxy *proxy)
{
  HyScanControlProxyPrivate *priv = proxy->priv;

  g_atomic_int_set (&priv->shutdown, TRUE);
  g_clear_pointer (&priv->sender, g_thread_join);

  g_signal_handlers_disconnect_by_data (priv->device, proxy);
}

/* Функция освобождает память занятую структурой HyScanControlProxySource. */
static void
hyscan_control_proxy_source_free (gpointer data)
{
  HyScanControlProxyAcoustic *buffer = data;
  guint i;

  for (i = 0; i < AQ_BUF_SZ; i++)
    g_object_unref (buffer->data[i].data);
  g_object_unref (buffer->signal.image);
  g_object_unref (buffer->signal.conv);
  g_object_unref (buffer->import);
  g_mutex_clear (&buffer->signal.lock);
  g_free (buffer->description);
  g_free (buffer->actuator);

  g_slice_free (HyScanControlProxyAcoustic, buffer);
}

/* Функция освобождает память занятую структурой HyScanControlProxySensor. */
static void
hyscan_control_proxy_sensor_free (gpointer data)
{
  HyScanControlProxySensor *buffer = data;

  g_object_unref (buffer->data);

  g_slice_free (HyScanControlProxySensor, buffer);
}

/* Функция обрабатывает изменения состояния устройства. */
static void
hyscan_control_proxy_device_state (HyScanControlProxy *proxy,
                                   const gchar        *dev_id)
{
  hyscan_device_driver_send_state (proxy, dev_id);
}

/* Функция обрабатывает информационные сообщения от устройств. */
static void
hyscan_control_proxy_device_log (HyScanControlProxy *proxy,
                                 const gchar        *source,
                                 gint64              time,
                                 gint                level,
                                 const gchar        *message)
{
  HyScanControlProxyPrivate *priv = proxy->priv;
  HyScanControlProxyLog *log = NULL;
  gint i;

  if (!g_atomic_int_get (&priv->logs_enable))
    return;

  for (i = 0; i < LOG_BUF_SZ; i++)
    {
      log = &priv->logs[i];
      if (g_atomic_int_compare_and_exchange (&log->status,
                                             HYSCAN_CONTROL_PROXY_EMPTY,
                                             HYSCAN_CONTROL_PROXY_PROCESS))
        {
          break;
        }
      log = NULL;
    }

  log->time = time;
  log->level = level;
  g_snprintf (log->src, LOG_SRC_SZ, "%s", source);
  g_snprintf (log->msg, LOG_MSG_SZ, "%s", message);

  g_cond_signal (&priv->cond);
}

/* Функция запоминает новый образ сигнала. */
static void
hyscan_control_proxy_sonar_signal (HyScanControlProxy *proxy,
                                   gint                source,
                                   guint               channel,
                                   gint64              time,
                                   HyScanBuffer       *image)
{
  HyScanControlProxyPrivate *priv = proxy->priv;
  HyScanControlProxyAcoustic *buffer;

  if (!g_atomic_int_get (&priv->started))
    return;

  if (channel != 1)
    return;

  buffer = g_hash_table_lookup (priv->sources, GINT_TO_POINTER (source));
  if (buffer == NULL)
    return;

  /* Запоминаем новый образ сигнала. */
  g_mutex_lock (&buffer->signal.lock);

  buffer->signal.time = time;
  if (image != NULL)
    hyscan_buffer_copy (buffer->signal.image, image);
  else
    hyscan_buffer_set_complex_float (buffer->signal.image, NULL, 0);

  g_mutex_unlock (&buffer->signal.lock);

  /* Отправляем на обработку данные накапливаемые сейчас. */
  g_atomic_int_set (&buffer->send, TRUE);
}

/* Функция запоминает акустические данные. */
static void
hyscan_control_proxy_sonar_acoustic_data (HyScanControlProxy     *proxy,
                                          gint                    source,
                                          guint                   channel,
                                          gboolean                noise,
                                          gint64                  time,
                                          HyScanAcousticDataInfo *info,
                                          HyScanBuffer           *data)
{
  HyScanControlProxyPrivate *priv = proxy->priv;
  HyScanControlProxyData *acoustic = NULL;
  HyScanControlProxyAcoustic *buffer;
  HyScanDiscretizationType discretization;
  guint32 i;

  if (!g_atomic_int_get (&priv->started))
    return;

  if (source == HYSCAN_SOURCE_FORWARD_LOOK)
    return;

  if ((channel != 1) || noise)
    return;

  if (info->data_type != hyscan_buffer_get_data_type (data))
    return;

  discretization = hyscan_discretization_get_type_by_data (info->data_type);
  if ((discretization != HYSCAN_DISCRETIZATION_COMPLEX) &&
      (discretization != HYSCAN_DISCRETIZATION_AMPLITUDE))
    {
      return;
    }

  buffer = g_hash_table_lookup (priv->sources, GINT_TO_POINTER (source));
  if ((buffer == NULL) || (!g_atomic_int_get (&buffer->enable)))
    return;

  buffer->received += 1;
  buffer->line_counter += 1;
  if (buffer->line_counter < buffer->cur_line_scale)
    return;

  /* Ищем пустой буфер. */
  for (i = 0; i < AQ_BUF_SZ; i++)
    {
      if (g_atomic_int_get (&buffer->data[i].status) == HYSCAN_CONTROL_PROXY_EMPTY)
        {
          if (acoustic == NULL)
            acoustic = &buffer->data[i];
        }
    }

  g_atomic_int_set (&buffer->send, FALSE);

  /* Нет пустых буферов, строка потеряна. */
  if (acoustic == NULL)
    {
      buffer->dropped += 1;
      return;
    }

  /* Сохраняем строку. */
  if (!hyscan_buffer_import (acoustic->data, data))
    return;

  buffer->line_counter = 0;
  acoustic->info = *info;
  acoustic->time = time;

  /* Сигнализируем об обработке. */
  g_atomic_int_set (&acoustic->status, HYSCAN_CONTROL_PROXY_PROCESS);
  g_cond_signal (&priv->cond);
}

/* Функция запоминает данные датчиков. */
static void
hyscan_control_proxy_sensor_data (HyScanControlProxy *proxy,
                                  const gchar        *sensor,
                                  gint                source,
                                  gint64              time,
                                  HyScanBuffer       *data)
{
  HyScanControlProxyPrivate *priv = proxy->priv;
  HyScanControlProxySensor *buffer;

  if (!g_atomic_int_get (&priv->started))
    return;

  buffer = g_hash_table_lookup (priv->sensors, sensor);
  if ((buffer == NULL) || (!g_atomic_int_get (&buffer->enable)))
    return;

  if (g_atomic_int_get (&buffer->status) != HYSCAN_CONTROL_PROXY_EMPTY)
    {
      buffer->dropped += 1;
      return;
    }

  buffer->received += 1;

  buffer->time = time;
  buffer->source = source;
  hyscan_buffer_copy (buffer->data, data);
  g_atomic_int_set (&buffer->status, HYSCAN_CONTROL_PROXY_PROCESS);

  g_cond_signal (&priv->cond);
}

/* Поток отправки данных. */
static gpointer
hyscan_control_proxy_sender (gpointer user_data)
{
  HyScanControlProxy *proxy = user_data;
  HyScanControlProxyPrivate *priv = proxy->priv;

  HyScanBuffer *ibuffer;
  HyScanBuffer *abuffer;
  HyScanBuffer *sbuffer;

  ibuffer = hyscan_buffer_new ();
  abuffer = hyscan_buffer_new ();
  sbuffer = hyscan_buffer_new ();

  while (TRUE)
    {
      GHashTableIter iter;
      gpointer key, value;

      gint64 wait_time;

      /* Ожидаем события или завершения работы. */
      wait_time = g_get_monotonic_time () + 100 * G_TIME_SPAN_MILLISECOND;
      g_cond_wait_until (&priv->cond, &priv->lock, wait_time);

      /* Завершаем работу. */
      if (g_atomic_int_get (&priv->shutdown))
        break;

      /* Отправляем информационные сообщения. */
      while (TRUE)
        {
          HyScanControlProxyLog *log;
          guint i;

          for (i = 0, log = NULL; i < LOG_BUF_SZ; i++)
            {
              if (g_atomic_int_get (&priv->logs[i].status) != HYSCAN_CONTROL_PROXY_PROCESS)
                continue;

              if ((log == NULL) || (log->time > priv->logs[i].time))
                log = &priv->logs[i];
            }

          if (log == NULL)
            break;

          hyscan_device_driver_send_log (proxy,
                                         log->src, log->time,
                                         log->level, log->msg);

          g_atomic_int_set (&log->status, HYSCAN_CONTROL_PROXY_EMPTY);
        }

      /* Отправляем данные датчиков. */
      g_hash_table_iter_init (&iter, priv->sensors);
      while (g_hash_table_iter_next (&iter, &key, &value))
        {
          HyScanControlProxySensor *buffer = value;
          const gchar *sensor = key;

          if (g_atomic_int_get (&buffer->status) != HYSCAN_CONTROL_PROXY_PROCESS)
            continue;

          /* Отправляем данные только в рабочем режиме. */
          if (g_atomic_int_get (&priv->started))
            {
              hyscan_sensor_driver_send_data (proxy,
                                              sensor, buffer->source,
                                              buffer->time, buffer->data);
            }

          g_atomic_int_set (&buffer->status, HYSCAN_CONTROL_PROXY_EMPTY);
        }

      /* Отправляем гидролокационные данные. */
      g_hash_table_iter_init (&iter, priv->sources);
      while (g_hash_table_iter_next (&iter, &key, &value))
        {
          HyScanControlProxyAcoustic *buffer = value;
          HyScanSourceType source = GPOINTER_TO_INT (key);

          HyScanDiscretizationType discretization;
          HyScanControlProxyData *acoustic = NULL;
          gboolean update_signal = FALSE;
          gboolean send_data = FALSE;
          gdouble a_scale;
          guint p_scale;
          guint i;

          /* Выбираем буфер для обработки с самым ранним временем. */
          for (i = 0; i < AQ_BUF_SZ; i++)
            {
              if ((g_atomic_int_get (&buffer->data[i].status) == HYSCAN_CONTROL_PROXY_PROCESS) &&
                  ((acoustic == NULL) || (acoustic->time > buffer->data[i].time)))
                {
                  acoustic = &buffer->data[i];
                }
            }

          /* Нет данных для обработки. */
          if (acoustic == NULL)
            continue;

          /* Проверяем необходимость изменения образа сигнала. */
          g_mutex_lock (&buffer->signal.lock);

          if ((buffer->signal.time >= 0) && (acoustic->time >= buffer->signal.time))
            {
              update_signal = hyscan_buffer_import (ibuffer, buffer->signal.image);

              buffer->signal.time = -1;
            }

          g_mutex_unlock (&buffer->signal.lock);

          /* Обновляем текущий образ сигнала. */
          if (update_signal)
            {
              HyScanComplexFloat *image = NULL;
              guint32 n_points = 0;

              image = hyscan_buffer_get_complex_float (ibuffer, &n_points);
              if ((image != NULL) && (n_points > 1))
                hyscan_convolution_set_image_td (buffer->signal.conv, 0, image, n_points);
              else
                hyscan_convolution_set_image_td (buffer->signal.conv, 0, NULL, 0);
            }

          /* Обработка данных. */
          discretization = hyscan_discretization_get_type_by_data (acoustic->info.data_type);

          /* Коэффициент масштабирования по дальности. */
          p_scale = buffer->cur_point_scale;
          a_scale = 1.0 / p_scale;

          if (discretization == HYSCAN_DISCRETIZATION_COMPLEX)
            {
              HyScanComplexFloat *original = NULL;
              gfloat *amplitude = NULL;
              guint32 o_points = 0;
              guint32 a_points = 0;
              guint32 i, j, k;

              original = hyscan_buffer_get_complex_float (acoustic->data, &o_points);
              hyscan_convolution_convolve (buffer->signal.conv, 0, original, o_points, 10.0);

              /* Вычисление амплитуды с усреднением. */
              a_points = o_points / p_scale;
              hyscan_buffer_set_float (abuffer, NULL, a_points);
              amplitude = hyscan_buffer_get_float (abuffer, &a_points);

              for (i = 0, j = 0; (i < o_points) && (j < a_points); j++)
                {
                  amplitude[j] = 0.0;

                  for (k = 0; (i < o_points) && (k < p_scale); i++, k++)
                    {
                      gfloat re = original[i].re;
                      gfloat im = original[i].im;
                      amplitude[j] += sqrtf (re * re + im * im);
                    }

                  amplitude[j] *= a_scale;
                }

              send_data = TRUE;
            }

          else if (discretization == HYSCAN_DISCRETIZATION_AMPLITUDE)
            {
              gfloat *original = NULL;
              gfloat *amplitude = NULL;
              guint32 o_points = 0;
              guint32 a_points = 0;
              guint32 i, j, k;

              /* Усреднение амплитуды. */
              original = hyscan_buffer_get_float (acoustic->data, &o_points);
              a_points = o_points / p_scale;
              hyscan_buffer_set_float (abuffer, NULL, a_points);
              amplitude = hyscan_buffer_get_float (abuffer, &a_points);

              for (i = 0, j = 0; (i < o_points) && (j < a_points); j++)
                {
                  amplitude[j] = 0.0;

                  for (k = 0; (i < o_points) && (k < p_scale); i++, k++)
                    amplitude[j] += original[i];

                  amplitude[j] *= a_scale;
                }

              send_data = TRUE;
            }

          /* Отправляем данные только в рабочем режиме. */
          if (send_data && g_atomic_int_get (&priv->started) &&
              hyscan_buffer_export (abuffer, sbuffer, buffer->cur_data_type))
            {
              HyScanAcousticDataInfo info = acoustic->info;

              info.data_rate /= p_scale;
              info.data_type = buffer->cur_data_type;

              if (buffer->start)
                {
                  hyscan_sonar_driver_send_source_info (proxy, source, 1,
                                                        buffer->description,
                                                        buffer->actuator,
                                                        &info);

                  buffer->start = FALSE;
                }

              hyscan_sonar_driver_send_acoustic_data (proxy,
                                                      source, 1, FALSE, acoustic->time,
                                                      &info, sbuffer);
            }

          g_atomic_int_set (&acoustic->status, HYSCAN_CONTROL_PROXY_EMPTY);
        }
    }

  g_object_unref (ibuffer);
  g_object_unref (abuffer);
  g_object_unref (sbuffer);

  return NULL;
}

/* Интерфейс HyScanParam. */

static HyScanDataSchema *
hyscan_control_proxy_param_schema (HyScanParam *param)
{
  HyScanControlProxy *proxy = HYSCAN_CONTROL_PROXY (param);
  HyScanControlProxyPrivate *priv = proxy->priv;

  return hyscan_param_schema (priv->param);
}

static gboolean
hyscan_control_proxy_param_set (HyScanParam     *param,
                                HyScanParamList *list)
{
  HyScanControlProxy *proxy = HYSCAN_CONTROL_PROXY (param);
  HyScanControlProxyPrivate *priv = proxy->priv;

  return hyscan_param_set (priv->param, list);
}

static gboolean
hyscan_control_proxy_param_get (HyScanParam     *param,
                                HyScanParamList *list)
{
  HyScanControlProxy *proxy = HYSCAN_CONTROL_PROXY (param);
  HyScanControlProxyPrivate *priv = proxy->priv;

  return hyscan_param_get (priv->param, list);
}

/* Интерфейс HyScanDevice. */

static gboolean
hyscan_control_proxy_device_sync (HyScanDevice *device)
{
  HyScanControlProxy *proxy = HYSCAN_CONTROL_PROXY (device);
  HyScanControlProxyPrivate *priv = proxy->priv;

  if (priv->sonar == NULL)
    return FALSE;

  return hyscan_device_sync (priv->device);
}

static gboolean
hyscan_control_proxy_device_set_sound_velocity (HyScanDevice *device,
                                                GList        *svp)
{
  HyScanControlProxy *proxy = HYSCAN_CONTROL_PROXY (device);
  HyScanControlProxyPrivate *priv = proxy->priv;

  return hyscan_device_set_sound_velocity (priv->device, svp);
}

static gboolean
hyscan_control_proxy_device_disconnect (HyScanDevice *device)
{
  HyScanControlProxy *proxy = HYSCAN_CONTROL_PROXY (device);
  HyScanControlProxyPrivate *priv = proxy->priv;

  hyscan_control_proxy_disconnect (proxy);

  return hyscan_device_disconnect (priv->device);
}

/* Интерфейс HyScanSonar. */

static gboolean
hyscan_control_proxy_sonar_antenna_set_offset (HyScanSonar               *sonar,
                                               HyScanSourceType           source,
                                               const HyScanAntennaOffset *offset)
{
  HyScanControlProxy *proxy = HYSCAN_CONTROL_PROXY (sonar);
  HyScanControlProxyPrivate *priv = proxy->priv;

  if (priv->sonar == NULL)
    return FALSE;

  return hyscan_sonar_antenna_set_offset (priv->sonar, source, offset);
}

static gboolean
hyscan_control_proxy_sonar_receiver_set_time (HyScanSonar      *sonar,
                                              HyScanSourceType  source,
                                              gdouble           receive_time,
                                              gdouble           wait_time)
{
  HyScanControlProxy *proxy = HYSCAN_CONTROL_PROXY (sonar);
  HyScanControlProxyPrivate *priv = proxy->priv;

  if (priv->sonar == NULL)
    return FALSE;

  return hyscan_sonar_receiver_set_time (priv->sonar, source, receive_time, wait_time);
}

static gboolean
hyscan_control_proxy_sonar_receiver_set_auto (HyScanSonar      *sonar,
                                              HyScanSourceType  source)
{
  HyScanControlProxy *proxy = HYSCAN_CONTROL_PROXY (sonar);
  HyScanControlProxyPrivate *priv = proxy->priv;

  if (priv->sonar == NULL)
    return FALSE;

  return hyscan_sonar_receiver_set_auto (priv->sonar, source);
}

static gboolean
hyscan_control_proxy_sonar_receiver_disable (HyScanSonar      *sonar,
                                             HyScanSourceType  source)
{
  HyScanControlProxy *proxy = HYSCAN_CONTROL_PROXY (sonar);
  HyScanControlProxyPrivate *priv = proxy->priv;

  if (priv->sonar == NULL)
    return FALSE;

  return hyscan_sonar_receiver_disable (priv->sonar, source);
}

static gboolean
hyscan_control_proxy_sonar_generator_set_preset (HyScanSonar      *sonar,
                                                 HyScanSourceType  source,
                                                 gint64            preset)
{
  HyScanControlProxy *proxy = HYSCAN_CONTROL_PROXY (sonar);
  HyScanControlProxyPrivate *priv = proxy->priv;

  if (priv->sonar == NULL)
    return FALSE;

  return hyscan_sonar_generator_set_preset (priv->sonar, source, preset);
}

static gboolean
hyscan_control_proxy_sonar_generator_disable (HyScanSonar      *sonar,
                                              HyScanSourceType  source)
{
  HyScanControlProxy *proxy = HYSCAN_CONTROL_PROXY (sonar);
  HyScanControlProxyPrivate *priv = proxy->priv;

  if (priv->sonar == NULL)
    return FALSE;

  return hyscan_sonar_generator_disable (priv->sonar, source);
}

static gboolean
hyscan_control_proxy_sonar_tvg_set_auto (HyScanSonar      *sonar,
                                         HyScanSourceType  source,
                                         gdouble           level,
                                         gdouble           sensitivity)
{
  HyScanControlProxy *proxy = HYSCAN_CONTROL_PROXY (sonar);
  HyScanControlProxyPrivate *priv = proxy->priv;

  if (priv->sonar == NULL)
    return FALSE;

  return hyscan_sonar_tvg_set_auto (priv->sonar, source, level, sensitivity);
}

static gboolean
hyscan_control_proxy_sonar_tvg_set_constant (HyScanSonar      *sonar,
                                             HyScanSourceType  source,
                                             gdouble           gain)
{
  HyScanControlProxy *proxy = HYSCAN_CONTROL_PROXY (sonar);
  HyScanControlProxyPrivate *priv = proxy->priv;

  if (priv->sonar == NULL)
    return FALSE;

  return hyscan_sonar_tvg_set_constant (priv->sonar, source, gain);
}

static gboolean
hyscan_control_proxy_sonar_tvg_set_linear_db (HyScanSonar      *sonar,
                                              HyScanSourceType  source,
                                              gdouble           gain0,
                                              gdouble           step)
{
  HyScanControlProxy *proxy = HYSCAN_CONTROL_PROXY (sonar);
  HyScanControlProxyPrivate *priv = proxy->priv;

  if (priv->sonar == NULL)
    return FALSE;

  return hyscan_sonar_tvg_set_linear_db (priv->sonar, source, gain0, step);
}

static gboolean
hyscan_control_proxy_sonar_tvg_set_logarithmic (HyScanSonar      *sonar,
                                                HyScanSourceType  source,
                                                gdouble           gain0,
                                                gdouble           beta,
                                                gdouble           alpha)
{
  HyScanControlProxy *proxy = HYSCAN_CONTROL_PROXY (sonar);
  HyScanControlProxyPrivate *priv = proxy->priv;

  if (priv->sonar == NULL)
    return FALSE;

  return hyscan_sonar_tvg_set_logarithmic (priv->sonar, source, gain0, beta, alpha);
}

static gboolean
hyscan_control_proxy_sonar_tvg_disable (HyScanSonar      *sonar,
                                        HyScanSourceType  source)
{
  HyScanControlProxy *proxy = HYSCAN_CONTROL_PROXY (sonar);
  HyScanControlProxyPrivate *priv = proxy->priv;

  if (priv->sonar == NULL)
    return FALSE;

  return hyscan_sonar_tvg_disable (priv->sonar, source);
}

static gboolean
hyscan_control_proxy_sonar_start (HyScanSonar           *sonar,
                                  const gchar           *project_name,
                                  const gchar           *track_name,
                                  HyScanTrackType        track_type,
                                  const HyScanTrackPlan *track_plan)
{
  HyScanControlProxy *proxy = HYSCAN_CONTROL_PROXY (sonar);
  HyScanControlProxyPrivate *priv = proxy->priv;

  GHashTableIter iter;
  gpointer key, value;
  gboolean status;

  if (priv->sonar == NULL)
    return FALSE;

  /* Обновляем параметры прореживания данных. */
  g_hash_table_iter_init (&iter, priv->sources);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      HyScanControlProxyAcoustic *buffer = value;

      buffer->cur_data_type = buffer->new_data_type;
      buffer->cur_line_scale = buffer->new_line_scale;
      buffer->cur_point_scale = buffer->new_point_scale;
      buffer->start = TRUE;
    }

  /* Запускаем оборудование и если всё прошло нормально,
   * включаем приём данных. */
  status = hyscan_sonar_start (priv->sonar, project_name, track_name, track_type, track_plan);
  g_atomic_int_set (&priv->started, status);

  return status;
}

static gboolean
hyscan_control_proxy_sonar_stop (HyScanSonar *sonar)
{
  HyScanControlProxy *proxy = HYSCAN_CONTROL_PROXY (sonar);
  HyScanControlProxyPrivate *priv = proxy->priv;

  GHashTableIter iter;
  gpointer key, value;
  gboolean status;

  if (priv->sonar == NULL)
    return FALSE;

  /* Останавливаем приём данных. */
  g_atomic_int_set (&priv->started, FALSE);
  status = hyscan_sonar_stop (priv->sonar);

  /* Ждём пока освободятся все буферы. */
  while (TRUE)
    {
      guint i;

      for (i = 0; i < LOG_BUF_SZ; i++)
        {
          if (g_atomic_int_get (&priv->logs[i].status) != HYSCAN_CONTROL_PROXY_EMPTY)
            goto wait_for_empty;
        }

      g_hash_table_iter_init (&iter, priv->sensors);
      while (g_hash_table_iter_next (&iter, &key, &value))
        {
          HyScanControlProxySensor *buffer = value;
          if (g_atomic_int_get (&buffer->status) != HYSCAN_CONTROL_PROXY_EMPTY)
            goto wait_for_empty;
        }

      g_hash_table_iter_init (&iter, priv->sources);
      while (g_hash_table_iter_next (&iter, &key, &value))
        {
          HyScanControlProxyAcoustic *buffer = value;

          for (i = 0; i < AQ_BUF_SZ; i++)
            {
              if (g_atomic_int_get (&buffer->data[i].status) != HYSCAN_CONTROL_PROXY_EMPTY)
                goto wait_for_empty;
            }

          g_mutex_lock (&buffer->signal.lock);
          buffer->line_counter = 0;
          buffer->signal.time = -1;
          buffer->start = FALSE;
          g_mutex_unlock (&buffer->signal.lock);
        }

      break;

      wait_for_empty:
        g_usleep (10000);
    }

  return status;
}

/* Интерфейс HyScanSensor. */

static gboolean
hyscan_control_proxy_sensor_set_enable (HyScanSensor *sensor,
                                        const gchar  *name,
                                        gboolean      enable)
{
  HyScanControlProxy *proxy = HYSCAN_CONTROL_PROXY (sensor);
  HyScanControlProxyPrivate *priv = proxy->priv;

  if (priv->sensor == NULL)
    return FALSE;

  return hyscan_sensor_set_enable (priv->sensor, name, enable);
}

static gboolean
hyscan_control_proxy_sensor_antenna_set_offset (HyScanSensor              *sensor,
                                                const gchar               *name,
                                                const HyScanAntennaOffset *offset)
{
  HyScanControlProxy *proxy = HYSCAN_CONTROL_PROXY (sensor);
  HyScanControlProxyPrivate *priv = proxy->priv;

  if (priv->sensor == NULL)
    return FALSE;

  return hyscan_sensor_antenna_set_offset (priv->sensor, name, offset);
}

/* Интерфейс HyScanActuator. */

static gboolean
hyscan_control_proxy_actuator_disable (HyScanActuator *actuator,
                                       const gchar    *name)
{
  HyScanControlProxy *proxy = HYSCAN_CONTROL_PROXY (actuator);
  HyScanControlProxyPrivate *priv = proxy->priv;

  if (priv->actuator == NULL)
    return FALSE;

  return hyscan_actuator_disable (priv->actuator, name);
}

static gboolean
hyscan_control_proxy_actuator_scan (HyScanActuator *actuator,
                                    const gchar    *name,
                                    gdouble         from,
                                    gdouble         to,
                                    gdouble         speed)
{
  HyScanControlProxy *proxy = HYSCAN_CONTROL_PROXY (actuator);
  HyScanControlProxyPrivate *priv = proxy->priv;

  if (priv->actuator == NULL)
    return FALSE;

  return hyscan_actuator_scan (priv->actuator, name, from, to, speed);
}

static gboolean
hyscan_control_proxy_actuator_manual (HyScanActuator *actuator,
                                      const gchar    *name,
                                      gdouble         angle)
{
  HyScanControlProxy *proxy = HYSCAN_CONTROL_PROXY (actuator);
  HyScanControlProxyPrivate *priv = proxy->priv;

  if (priv->actuator == NULL)
    return FALSE;

  return hyscan_actuator_manual (priv->actuator, name, angle);
}

/**
 * hyscan_control_proxy_new:
 * @control: уазатель на #HyScanControl
 * @dev_id: (nullable): идентификатор прокси устройства
 *
 * Функция создаёт новый прокси объект управления гидролокаторами и датчиками.
 *
 * Если идентификатор прокси устройства задать равным NULL, будет
 * использоваться значение по умолчанию. Это достаточно для одиночного
 * применения #HyScanControlProxy. Если предполагается каскадное включение,
 * необходимо для каждого прокси устройства задать свой идентификатор.
 *
 * Returns: (nullable) #HyScanControlProxy или NULL.
 *                     Для удаления #g_object_unref.
 */
HyScanControlProxy *
hyscan_control_proxy_new (HyScanControl *control,
                          const gchar   *dev_id)
{
  HyScanControlProxy *proxy;

  proxy = g_object_new (HYSCAN_TYPE_CONTROL_PROXY,
                        "control", control,
                        "dev-id", dev_id,
                        NULL);

  if (proxy->priv->control == NULL)
    g_clear_object (&proxy);

  return proxy;
}

/**
 * hyscan_control_proxy_set_scale:
 * @proxy: указатель на #HyScanControlProxy
 * @source: источник гидролокационных данных
 * @line_scale: коэффициент прореживания по строкам
 * @point_scale: коэффициент прореживания по точкам в строке
 *
 * Функция задаёт коэффициенты прореживания данных по строкам и точкам.
 * Новые коэффициенты применяются после остановки и запуска устройства.
 */
void
hyscan_control_proxy_set_scale (HyScanControlProxy *proxy,
                                HyScanSourceType    source,
                                guint               line_scale,
                                guint               point_scale)
{
  HyScanControlProxyAcoustic *buffer;

  g_return_if_fail (HYSCAN_IS_CONTROL_PROXY (proxy));

  line_scale = CLAMP (line_scale, 1, AQ_MAX_SCALE);
  point_scale = CLAMP (point_scale, 1, AQ_MAX_SCALE);

  buffer = g_hash_table_lookup (proxy->priv->sources, GINT_TO_POINTER (source));
  if (buffer == NULL)
    return;

  buffer->new_line_scale = line_scale;
  buffer->new_point_scale = point_scale;
}

/**
 * hyscan_control_proxy_set_data_type:
 * @proxy: указатель на #HyScanControlProxy
 * @source: источник гидролокационных данных
 * @type: тип данных
 *
 * Функция устанавливает формат выходных данных после выполнения обработки
 * и прореживания. Тип выходных данных должен быть амплитудным. Новый формат
 * применяется после остановки и запуска устройства.
 */
void
hyscan_control_proxy_set_data_type (HyScanControlProxy *proxy,
                                    HyScanSourceType    source,
                                    HyScanDataType      type)
{
  HyScanControlProxyAcoustic *buffer;

  g_return_if_fail (HYSCAN_IS_CONTROL_PROXY (proxy));

  if (hyscan_discretization_get_type_by_data (type) != HYSCAN_DISCRETIZATION_AMPLITUDE)
    return;

  buffer = g_hash_table_lookup (proxy->priv->sources, GINT_TO_POINTER (source));
  if (buffer == NULL)
    return;

  buffer->new_data_type = type;
}

/**
 * hyscan_control_proxy_sensors_list:
 * @proxy: указатель на #HyScanControlProxy
 *
 * Функция возвращает список датчиков. Список возвращается в виде NULL
 * терминированного массива строк с названиями датчиков. Список принадлежит
 * объекту #HyScanControl и не должен изменяться пользователем.
 *
 * Returns: (transfer none): Список портов или NULL.
 */
const gchar * const *
hyscan_control_proxy_sensors_list (HyScanControlProxy *proxy)
{
  g_return_val_if_fail (HYSCAN_IS_CONTROL_PROXY (proxy), NULL);

  return hyscan_control_sensors_list (proxy->priv->control);
}

/**
 * hyscan_control_proxy_sources_list:
 * @proxy: указатель на #HyScanControlProxy
 * @n_sources: (out): число источников данных
 *
 * Функция возвращает список источников гидролокационных данных. Список
 * возвращается в виде массива элементов типа HyScanSourceType. Массив
 * принадлежит объекту #HyScanControl и не должен изменяться пользователем.
 *
 * Returns: (transfer none): Список источников данных или NULL.
 */
const HyScanSourceType *
hyscan_control_proxy_sources_list (HyScanControlProxy *proxy,
                                   guint32            *n_sources)
{
  g_return_val_if_fail (HYSCAN_IS_CONTROL_PROXY (proxy), NULL);

  return hyscan_control_sources_list (proxy->priv->control, n_sources);
}

/**
 * hyscan_control_proxy_sensor_get_info:
 * @proxy: указатель на #HyScanControlProxy
 * @sensor: название датчика
 *
 * Функция возвращает параметры датчика.
 *
 * Returns: (nullable) (transfer none): Параметры датчика или NULL.
 */
const HyScanSensorInfoSensor *
hyscan_control_proxy_sensor_get_info (HyScanControlProxy *proxy,
                                      const gchar        *sensor)
{
  g_return_val_if_fail (HYSCAN_IS_CONTROL_PROXY (proxy), NULL);

  return hyscan_control_sensor_get_info (proxy->priv->control, sensor);
}

/**
 * hyscan_control_proxy_source_get_info:
 * @proxy: указатель на #HyScanControlProxy
 * @source: источник гидролокационных данных
 *
 * Функция возвращает параметры источника гидролокационных данных.
 *
 * Returns: (nullable) (transfer none): Параметры гидролокационного источника данных или NULL.
 */
const HyScanSonarInfoSource *
hyscan_control_proxy_source_get_info (HyScanControlProxy *proxy,
                                      HyScanSourceType    source)
{
  g_return_val_if_fail (HYSCAN_IS_CONTROL_PROXY (proxy), NULL);

  return hyscan_control_source_get_info (proxy->priv->control, source);
}

/**
 * hyscan_control_proxy_sensor_set_sender:
 * @proxy: указатель на #HyScanControlProxy
 * @sensor: название датчика
 * @enable: признак включения пересылки данных
 *
 * Функция управляет включением и выключением пересылки данных датчиков.
 */
void
hyscan_control_proxy_sensor_set_sender (HyScanControlProxy *proxy,
                                        const gchar        *sensor,
                                        gboolean            enable)
{
  HyScanControlProxyPrivate *priv;
  HyScanControlProxySensor *buffer;

  g_return_if_fail (HYSCAN_IS_CONTROL_PROXY (proxy));

  priv = proxy->priv;

  buffer = g_hash_table_lookup (priv->sensors, sensor);
  if (buffer == NULL)
    return;

  g_atomic_int_set (&buffer->enable, enable);
}

/**
 * hyscan_control_proxy_source_set_sender:
 * @proxy: указатель на #HyScanControlProxy
 * @source: источник гидролокационных данных
 * @enable: признак включения пересылки данных
 *
 * Функция управляет включением и выключением пересылки данных гидролокационных
 * источников.
 */
void
hyscan_control_proxy_source_set_sender (HyScanControlProxy *proxy,
                                        HyScanSourceType    source,
                                        gboolean            enable)
{
  HyScanControlProxyPrivate *priv;
  HyScanControlProxyAcoustic *buffer;

  g_return_if_fail (HYSCAN_IS_CONTROL_PROXY (proxy));

  priv = proxy->priv;

  if (source == HYSCAN_SOURCE_LOG)
    {
      g_atomic_int_set (&priv->logs_enable, enable);
      return;
    }

  buffer = g_hash_table_lookup (priv->sources, GINT_TO_POINTER (source));
  if (buffer == NULL)
    return;

  g_atomic_int_set (&buffer->enable, enable);
}

static void
hyscan_control_proxy_param_interface_init (HyScanParamInterface *iface)
{
  iface->schema = hyscan_control_proxy_param_schema;
  iface->set = hyscan_control_proxy_param_set;
  iface->get = hyscan_control_proxy_param_get;
}

static void
hyscan_control_proxy_device_interface_init (HyScanDeviceInterface *iface)
{
  iface->sync = hyscan_control_proxy_device_sync;
  iface->set_sound_velocity = hyscan_control_proxy_device_set_sound_velocity;
  iface->disconnect = hyscan_control_proxy_device_disconnect;
}

static void
hyscan_control_proxy_sonar_interface_init (HyScanSonarInterface *iface)
{
  iface->antenna_set_offset = hyscan_control_proxy_sonar_antenna_set_offset;
  iface->receiver_set_time = hyscan_control_proxy_sonar_receiver_set_time;
  iface->receiver_set_auto = hyscan_control_proxy_sonar_receiver_set_auto;
  iface->receiver_disable = hyscan_control_proxy_sonar_receiver_disable;
  iface->generator_set_preset = hyscan_control_proxy_sonar_generator_set_preset;
  iface->generator_disable = hyscan_control_proxy_sonar_generator_disable;
  iface->tvg_set_auto = hyscan_control_proxy_sonar_tvg_set_auto;
  iface->tvg_set_constant = hyscan_control_proxy_sonar_tvg_set_constant;
  iface->tvg_set_linear_db = hyscan_control_proxy_sonar_tvg_set_linear_db;
  iface->tvg_set_logarithmic = hyscan_control_proxy_sonar_tvg_set_logarithmic;
  iface->tvg_disable = hyscan_control_proxy_sonar_tvg_disable;
  iface->start = hyscan_control_proxy_sonar_start;
  iface->stop = hyscan_control_proxy_sonar_stop;
}

static void
hyscan_control_proxy_sensor_interface_init (HyScanSensorInterface *iface)
{
  iface->antenna_set_offset = hyscan_control_proxy_sensor_antenna_set_offset;
  iface->set_enable = hyscan_control_proxy_sensor_set_enable;
}

static void
hyscan_control_proxy_actuator_interface_init (HyScanActuatorInterface *iface)
{
  iface->disable = hyscan_control_proxy_actuator_disable;
  iface->scan = hyscan_control_proxy_actuator_scan;
  iface->manual = hyscan_control_proxy_actuator_manual;
}
