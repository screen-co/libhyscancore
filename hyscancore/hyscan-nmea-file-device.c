/* hyscan-nmea-file-device.c
 *
 * Copyright 2019 Screen LLC, Alexey Sakhnov <alexsakhnov@gmail.com>
 *
 * This file is part of HyScanGui library.
 *
 * HyScanGui is dual-licensed: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * HyScanGui is distributed in the hope that it will be useful,
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

/* HyScanGui имеет двойную лицензию.
 *
 * Во-первых, вы можете распространять HyScanGui на условиях Стандартной
 * Общественной Лицензии GNU версии 3, либо по любой более поздней версии
 * лицензии (по вашему выбору). Полные положения лицензии GNU приведены в
 * <http://www.gnu.org/licenses/>.
 *
 * Во-вторых, этот программный код можно использовать по коммерческой
 * лицензии. Для этого свяжитесь с ООО Экран - <info@screen-co.ru>.
 */

/**
 * SECTION: hyscan-nmea-file-device
 * @Short_description: класс имитирует работу навигационного оборудования, читая NMEA-строки из файла
 * @Title: HyScanNmeaFileDevice
 *
 * Класс реализует интерфейсы #HyScanDevice и #HyScanSensor и может быть использован
 * для имитации работы GPS-приёмника, проигрывая NMEA-строки из файла.
 *
 * Созданный объект класса читает из файла NMEA-строки, парсит время их фиксации,
 * и эмитирует сигналы #HyScanSensor::sensor-data с этими строками каждую секунду.
 * Если данных для отправки за текущий промежуток нет, то сигнал не отправляется.
 *
 * Как только объект достигнет конца файла, будет отправлен сигнал #HyScanNmeaFileDevice::finish.
 *
 * - Проигрывание файла начинается сразу после создания объекта с помощью функции
 *   hyscan_nmea_file_device_new().
 *
 * - Отправка сигналов "sensor-data" и "finish" по умолчанию отключена и включается
 *   функцией hyscan_sensor_set_enable()
 *
 * - Для завершения проигрывания необходимо вызвать hyscan_device_disconnect().
 */

#include "hyscan-nmea-file-device.h"
#include "hyscan-nmea-parser.h"
#include <hyscan-buffer.h>
#include <glib/gstdio.h>
#include <math.h>

enum
{
  PROP_O,
  PROP_NAME,
  PROP_FILENAME
};

enum
{
  SIGNAL_FINISH,
  SIGNAL_LAST,
};

struct _HyScanNmeaFileDevicePrivate
{
  gchar                *name;               /* Имя датчика. */
  gchar                *filename;           /* Имя файла, откуда читать NMEA-строки. */

  gboolean              enable;             /* Статус датчика: включен или выключен. */
  gboolean              shutdown;           /* Флаг о завершении работы. */

  GThread              *thread;             /* Поток, в котором читаются NMEA-строки. */

  GTimer               *timer;              /* Таймер. */
  gdouble               timer_offset;       /* Момент времени в файле, соответсвующий запуску таймера. */
  gdouble               tick_step;          /* Период времени, в течение которого строки отправляются одновременно. */
  gdouble               next_tick;          /* Время следующего чтения файла. */

  FILE                 *fp;                 /* Дескриптор файла с NMEA-строками. */
  GString              *line;               /* Строка NMEA. */
  gdouble               line_time;          /* Время из строки line. */
  GString              *sensor_data;        /* Буфер для сбора NMEA-строк в рамках одного сигнала "sensor-data". */
  HyScanBuffer         *data_buffer;        /* Буфер, отправляемый сигналом "sensor-data". */
  HyScanNMEAParser     *parser_time;        /* Парсер времени из NMEA-строк. */
  HyScanNMEAParser     *parser_date;        /* Парсер даты из NMEA-строк. */
};

static void     hyscan_nmea_file_device_sensor_interface_init    (HyScanSensorInterface  *iface);
static void     hyscan_nmea_file_device_device_interface_init    (HyScanDeviceInterface  *iface);
static void     hyscan_nmea_file_device_set_property             (GObject                *object,
                                                                  guint                   prop_id,
                                                                  const GValue           *value,
                                                                  GParamSpec             *pspec);
static void     hyscan_nmea_file_device_object_constructed       (GObject                *object);
static void     hyscan_nmea_file_device_object_finalize          (GObject                *object);
static gpointer hyscan_nmea_file_device_process                  (HyScanNmeaFileDevice   *device);
static gboolean hyscan_nmea_file_device_parse_time               (HyScanNmeaFileDevice   *device,
                                                                  const gchar            *sentence,
                                                                  gdouble                *_val);
static void     hyscan_nmea_file_device_read                     (HyScanNmeaFileDevice   *device);
static gboolean hyscan_nmea_read_line                            (HyScanNmeaFileDevice   *device);

static guint hyscan_nmea_file_device_signals[SIGNAL_LAST] = {0};

G_DEFINE_TYPE_WITH_CODE (HyScanNmeaFileDevice, hyscan_nmea_file_device, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (HyScanNmeaFileDevice)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_DEVICE, hyscan_nmea_file_device_device_interface_init)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_SENSOR, hyscan_nmea_file_device_sensor_interface_init))

static void
hyscan_nmea_file_device_class_init (HyScanNmeaFileDeviceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_nmea_file_device_set_property;

  object_class->constructed = hyscan_nmea_file_device_object_constructed;
  object_class->finalize = hyscan_nmea_file_device_object_finalize;

  g_object_class_install_property (object_class, PROP_NAME,
    g_param_spec_string ("name", "Device Name", "Device name", NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_FILENAME,
    g_param_spec_string ("filename", "File Name", "Path to file with NMEA-lines", NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  /**
   * HyScanNmeaFileDevice::finish:
   * @device: указатель на #HyScanNmeaFileDevice
   *
   * Сигнал отправляется, когда @device достиг конца файла
   */
  hyscan_nmea_file_device_signals[SIGNAL_FINISH] =
    g_signal_new ("finish", HYSCAN_TYPE_NMEA_FILE_DEVICE, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
}

static void
hyscan_nmea_file_device_init (HyScanNmeaFileDevice *nmea_file_device)
{
  nmea_file_device->priv = hyscan_nmea_file_device_get_instance_private (nmea_file_device);
}

static void
hyscan_nmea_file_device_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  HyScanNmeaFileDevice *nmea_file_device = HYSCAN_NMEA_FILE_DEVICE (object);
  HyScanNmeaFileDevicePrivate *priv = nmea_file_device->priv;

  switch (prop_id)
    {
    case PROP_NAME:
      priv->name = g_value_dup_string (value);
      break;

    case PROP_FILENAME:
      priv->filename = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_nmea_file_device_object_constructed (GObject *object)
{
  HyScanNmeaFileDevice *device = HYSCAN_NMEA_FILE_DEVICE (object);
  HyScanNmeaFileDevicePrivate *priv = device->priv;

  G_OBJECT_CLASS (hyscan_nmea_file_device_parent_class)->constructed (object);

  priv->parser_time = hyscan_nmea_parser_new_empty (HYSCAN_NMEA_DATA_RMC, HYSCAN_NMEA_FIELD_TIME);
  priv->parser_date = hyscan_nmea_parser_new_empty (HYSCAN_NMEA_DATA_RMC, HYSCAN_NMEA_FIELD_DATE);
  priv->tick_step = 0.01;
  priv->thread = g_thread_new ("nmea-file-device", (GThreadFunc) hyscan_nmea_file_device_process, device);
}

static void
hyscan_nmea_file_device_object_finalize (GObject *object)
{
  HyScanNmeaFileDevice *nf_device = HYSCAN_NMEA_FILE_DEVICE (object);
  HyScanNmeaFileDevicePrivate *priv= nf_device->priv;

  if (g_atomic_int_compare_and_exchange (&priv->shutdown, FALSE, TRUE))
    {
      g_warning ("HyScanNmeaFileDevice: hyscan_device_disconnect() MUST BE called before finalize");
      g_thread_join (priv->thread);
    }

  g_free (priv->filename);
  g_free (priv->name);
  g_object_unref (priv->parser_date);
  g_object_unref (priv->parser_time);

  G_OBJECT_CLASS (hyscan_nmea_file_device_parent_class)->finalize (object);
}

static gboolean
hyscan_nmea_file_device_parse_time (HyScanNmeaFileDevice *device,
                                    const gchar          *sentence,
                                    gdouble              *_val)
{
  HyScanNmeaFileDevicePrivate *priv = device->priv;
  gdouble date_val, time_val;

  if (!hyscan_nmea_parser_parse_string (priv->parser_time, sentence, &time_val) ||
      !hyscan_nmea_parser_parse_string (priv->parser_date, sentence, &date_val))
    return FALSE;

  *_val = date_val + time_val;

  return TRUE;
}

/* Считывает из файла следующую строку.
 * start содержит оффсет начала NMEA-сообщения внутри считанной строки. */
static gboolean
hyscan_nmea_read_line (HyScanNmeaFileDevice *device)
{
  HyScanNmeaFileDevicePrivate *priv = device->priv;
  gchar c;

  /* Очищаем буфер. */
  g_string_erase (priv->line, 0, -1);

  /* Посимвольно добавляем данные из файла в буфер до конца строки / файла. */
  while ((c = fgetc (priv->fp)) != EOF)
    {
      g_string_append_c (priv->line, c);

      if (c == '\n')
        break;
    }

  return priv->line->len > 0;
}

/* Читает из файла строки до текущего момента времени. */
static void
hyscan_nmea_file_device_read (HyScanNmeaFileDevice *device)
{
  HyScanNmeaFileDevicePrivate *priv = device->priv;

  gdouble time;

  time = priv->timer_offset + g_timer_elapsed (priv->timer, NULL);

  /* Фиксируем приём сигнала, только если достигли следующего тика. */
  if (time < priv->next_tick)
    return;

  /* Добавляем последнюю считанную строку: line_time <= (next_tick = ceil(line_time)) <= time. */
  g_assert (priv->line_time <= time);
  g_string_append_len (priv->sensor_data, priv->line->str, priv->line->len);

  /* Считываем следующие строки, пока они проходят по времени: line_time < next_tick + priv->tick_step. */
  while (hyscan_nmea_read_line (device))
    {
      /* Проверяем, что данные подходят по времени, т.е. не из будущего. */
      hyscan_nmea_file_device_parse_time (device, priv->line->str, &priv->line_time);
      if (priv->line_time >= priv->next_tick + priv->tick_step)
        break;

      g_string_append_len (priv->sensor_data, priv->line->str, priv->line->len);
    }

  /* Отправляем сигнал с данными. */
  hyscan_buffer_wrap (priv->data_buffer, HYSCAN_DATA_STRING, priv->sensor_data->str, priv->sensor_data->len);
  g_signal_emit_by_name (device, "sensor-data", priv->name, HYSCAN_SOURCE_NMEA, (gint64) (time * 1e6), priv->data_buffer);

  /* Если дошли до конца файла, то завершаем свою работу. */
  if (feof (priv->fp))
    {
      g_signal_emit (device, hyscan_nmea_file_device_signals[SIGNAL_FINISH], 0);
      g_atomic_int_set (&priv->shutdown, TRUE);
    }

  g_string_truncate (priv->sensor_data, 0);
  priv->next_tick = priv->line_time;
}

static gpointer
hyscan_nmea_file_device_process (HyScanNmeaFileDevice *device)
{
  HyScanNmeaFileDevicePrivate *priv = device->priv;

  /* Инициализируем вспомогательные переменные. */
  priv->line = g_string_new (NULL);
  priv->sensor_data = g_string_new (NULL);
  priv->data_buffer = hyscan_buffer_new ();
  priv->timer = g_timer_new ();

  /* Открываем файл. */
  priv->fp = g_fopen (priv->filename, "r");
  if (priv->fp == NULL)
    goto exit;

  /* Определяем начальную метку времени. */
  priv->line_time = G_MINDOUBLE;
  while (hyscan_nmea_read_line (device))
    {
      if (hyscan_nmea_file_device_parse_time (device, priv->line->str, &priv->line_time))
        break;
    }

  if (priv->line_time == G_MINDOUBLE)
    goto exit;

  priv->next_tick = ceil (priv->line_time);
  priv->timer_offset = priv->line_time;

  /* Запускаем проигрывание NMEA-строк. */
  while (!g_atomic_int_get (&priv->shutdown))
    {
      if (g_atomic_int_get (&priv->enable))
        hyscan_nmea_file_device_read (device);

      g_usleep (G_USEC_PER_SEC * priv->tick_step / 20);
    }

exit:
  g_string_free (priv->sensor_data, TRUE);
  g_string_free (priv->line, TRUE);
  g_clear_pointer (&priv->timer, g_timer_destroy);
  g_clear_pointer (&priv->fp, fclose);
  g_clear_object (&priv->data_buffer);

  return NULL;
}

static gboolean
hyscan_nmea_file_device_set_enable (HyScanSensor *sensor,
                                    const gchar  *name,
                                    gboolean      enable)
{
  HyScanNmeaFileDevice *device = HYSCAN_NMEA_FILE_DEVICE (sensor);
  HyScanNmeaFileDevicePrivate *priv = device->priv;

  if (g_strcmp0 (name, priv->name) != 0)
    return FALSE;

  if (enable && priv->filename == NULL)
    return FALSE;

  g_atomic_int_set (&priv->enable, enable);

  return TRUE;
}

static gboolean
hyscan_nmea_file_device_disconnect (HyScanDevice *device)
{
  HyScanNmeaFileDevice *nf_device = HYSCAN_NMEA_FILE_DEVICE (device);
  HyScanNmeaFileDevicePrivate *priv = nf_device->priv;

  if (g_atomic_int_compare_and_exchange (&priv->shutdown, FALSE, TRUE))
    g_thread_join (priv->thread);

  return TRUE;
}

static void
hyscan_nmea_file_device_sensor_interface_init (HyScanSensorInterface *iface)
{
  iface->set_enable = hyscan_nmea_file_device_set_enable;
}

static void
hyscan_nmea_file_device_device_interface_init (HyScanDeviceInterface *iface)
{
  iface->set_sound_velocity = NULL;
  iface->disconnect = hyscan_nmea_file_device_disconnect;
}

/**
 * hyscan_nmea_file_device_new:
 * @name: имя датчика
 * @filename: путь к файлу с NMEA-строками
 *
 * Returns: указатель на новый #HyScanNmeaFileDevice
 */
HyScanNmeaFileDevice *
hyscan_nmea_file_device_new (const gchar *name,
                             const gchar *filename)
{
  return g_object_new (HYSCAN_TYPE_NMEA_FILE_DEVICE,
                       "name", name,
                       "filename", filename,
                       NULL);
}
