/* hyscan-data-writer.h
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
 * SECTION: hyscan-data-writer
 * @Short_description: класс управления записью данных
 * @Title: HyScanDataWriter
 *
 * Класс предназначен для управления записью данных в систему хранения.
 * Он содержит функции обеспечивающие создание проектов и галсов, а также
 * записи гидролокационных данных и данных датчиков. При создании проекта
 * или галса, в него автоматически записываются все необходимые параметры.
 * Функции класса можно разделить на три условные группы:
 *
 * - установка параметров;
 * - управление записью;
 * - запись данных.
 *
 * Функции установки параметров:
 *
 * - #hyscan_data_writer_set_db - устанавливает систему хранения;
 * - #hyscan_data_writer_set_operator_name - устанавливает имя оператора гидролокатора;
 * - #hyscan_data_writer_set_sonar_info - для задания информации о гидролокаторе;
 * - #hyscan_data_writer_set_chunk_size - устанавливает максимальный размер файлов в галсе;
 * - #hyscan_data_writer_sensor_set_position - для установки местоположения приёмной антенны датчика;
 * - #hyscan_data_writer_sonar_set_position - для установки местоположения приёмной антенны гидролокатора.
 *
 * Функции установки параметров необходимо вызывать до начала записи данных.
 *
 * Функции управления записью:
 *
 * - #hyscan_data_writer_start - включает запись данных;
 * - #hyscan_data_writer_stop - останавливает запись данных.
 *
 * Функции записи данных:
 *
 * - #hyscan_data_writer_log_add_message - запись информационных сообщений;
 * - #hyscan_data_writer_sensor_add_data - запись данных от датчика;
 * - #hyscan_data_writer_acoustic_add_data -запись гидроакустических данных;
 * - #hyscan_data_writer_acoustic_add_signal - запись образов сигналов;
 * - #hyscan_data_writer_acoustic_add_tvg - запись параметров ВАРУ.
 *
 * Класс HyScanDataWriter сохраняет во внутреннем буфере текущий образ сигнала и
 * параметры ВАРУ для каждого из источников данных. В дальнейшем эта информация
 * записывается в новые галсы автоматически, до тех пор пока не будут установлены
 * новые значения.
 */

#include "hyscan-data-writer.h"
#include "hyscan-core-common.h"

#include <gio/gio.h>
#include <math.h>

typedef struct
{
  HyScanDB                    *db;                             /* Интерфейс системы хранения данных. */
  HyScanSourceType             source;                         /* Источник данных. */
  guint                        channel;                        /* Номер канала данных. */
  gint32                       data_id;                        /* Идентификатор канала данных. */
} HyScanDataWriterSensorChannel;

typedef struct
{
  HyScanDB                    *db;                             /* Интерфейс системы хранения данных. */
  HyScanSourceType             source;                         /* Источник данных. */
  guint                        channel;                        /* Номер канала данных. */
  gint32                       data_id;                        /* Идентификатор канала данных. */
  gint32                       noise_id;                       /* Идентификатор канала данных шумов. */
  gint32                       signal_id;                      /* Идентификатор канала сигналов. */
  gint32                       tvg_id;                         /* Идентификатор канала параметров ВАРУ. */
  HyScanDataType               data_type;                      /* Тип данных. */
  gdouble                      data_rate;                      /* Частота дискретизации данных, Гц. */
} HyScanDataWriterSonarChannel;

typedef struct
{
  gint64                       time;                           /* Время начала действия сигнала. */
  HyScanBuffer                *image;                          /* Образ сигнала для свёртки. */
} HyScanDataWriterSignal;

typedef struct
{
  gint64                       time;                           /* Время начала действия параметров системы ВАРУ. */
  HyScanBuffer                *gains;                          /* Коэффициенты передачи приёмного тракта, дБ. */
} HyScanDataWriterTVG;

struct _HyScanDataWriterPrivate
{
  HyScanDB                    *db;                             /* Интерфейс системы хранения данных. */
  gint32                       chunk_size;                     /* Максимальный размер файлов в галсе. */
  gchar                       *project_name;                   /* Название проекта для записи галсов.. */
  gchar                       *track_name;                     /* Название галса для записи данных. */
  gint32                       track_id;                       /* Идентификатор галса для записи данных. */

  GString                     *log_message;                    /* Информационое сообщение. */
  HyScanBuffer                *log_data;                       /* Буфер для информационного сообщения. */
  gint32                       log_id;                         /* Идентификатор канала для записи информационных сообщений. */

  gchar                       *operator_name;                  /* Имя оператора. */
  gchar                       *sonar_info;                     /* Информация о гидролокаторе. */

  GHashTable                  *sensor_positions;               /* Информация о местоположении антенн датчиков. */
  GHashTable                  *sensor_channels;                /* Список каналов для записи данных от датчиков. */

  GHashTable                  *sonar_positions;                /* Информация о местоположении гидролокационных антенн. */
  GHashTable                  *sonar_channels;                 /* Список каналов для записи гидролокационных данных. */
  GHashTable                  *signals;                        /* Список образов сигналов по источникам данных. */
  GHashTable                  *tvg;                            /* Список параметров ВАРУ. */

  GMutex                       lock;                           /* Блокировка. */
  GRand                       *rand;                           /* Генератор случайных чисел. */
};

static void      hyscan_data_writer_object_constructed         (GObject                       *object);
static void      hyscan_data_writer_object_finalize            (GObject                       *object);

static gpointer  hyscan_data_writer_uniq_channel               (HyScanSourceType               source,
                                                                guint                          channel);

static void      hyscan_data_writer_sensor_channel_free        (gpointer                       data);
static void      hyscan_data_writer_sonar_channel_free         (gpointer                       data);
static void      hyscan_data_writer_raw_signal_free            (gpointer                       data);
static void      hyscan_data_writer_raw_gain_free              (gpointer                       data);

static void      hyscan_data_writer_get_id                     (GRand                         *rand,
                                                                gchar                         *buffer,
                                                                guint                          size);

static gboolean  hyscan_data_writer_create_project             (HyScanDB                      *db,
                                                                const gchar                   *project_name,
                                                                gint64                         date_time,
                                                                GRand                         *rand);

static gint32    hyscan_data_writer_create_track               (HyScanDB                      *db,
                                                                gint32                         project_id,
                                                                const gchar                   *track_name,
                                                                HyScanTrackType                track_type,
                                                                gint64                         date_time,
                                                                const gchar                   *operator,
                                                                const gchar                   *sonar,
                                                                GRand                         *rand);

static gint32    hyscan_data_writer_create_log_channel         (HyScanDataWriterPrivate       *priv);

static HyScanDataWriterSensorChannel *
                 hyscan_data_writer_create_sensor_channel      (HyScanDataWriterPrivate       *priv,
                                                                const gchar                   *sensor,
                                                                HyScanSourceType               source,
                                                                guint                          channel);

static HyScanDataWriterSonarChannel *
                 hyscan_data_writer_create_acoustic_channel    (HyScanDataWriterPrivate       *priv,
                                                                HyScanSourceType               source,
                                                                guint                          channel,
                                                                HyScanAcousticDataInfo        *info);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanDataWriter, hyscan_data_writer, G_TYPE_OBJECT)

static void
hyscan_data_writer_class_init (HyScanDataWriterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = hyscan_data_writer_object_constructed;
  object_class->finalize = hyscan_data_writer_object_finalize;
}

static void
hyscan_data_writer_init (HyScanDataWriter *data_writer)
{
  data_writer->priv = hyscan_data_writer_get_instance_private (data_writer);
}

static void
hyscan_data_writer_object_constructed (GObject *object)
{
  HyScanDataWriter *writer = HYSCAN_DATA_WRITER (object);
  HyScanDataWriterPrivate *priv = writer->priv;

  priv->rand = g_rand_new ();
  g_mutex_init (&priv->lock);

  priv->chunk_size = -1;
  priv->track_id = -1;

  priv->log_message = g_string_new (NULL);
  priv->log_data = hyscan_buffer_new ();
  priv->log_id = -1;

  priv->sensor_positions = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                  g_free, (GDestroyNotify)hyscan_antenna_position_free);

  priv->sensor_channels = g_hash_table_new_full  (g_direct_hash, g_direct_equal,
                                                  NULL, hyscan_data_writer_sensor_channel_free);

  priv->sonar_positions = g_hash_table_new_full  (g_direct_hash, g_direct_equal,
                                                  NULL, (GDestroyNotify)hyscan_antenna_position_free);

  priv->sonar_channels = g_hash_table_new_full   (g_direct_hash, g_direct_equal,
                                                  NULL, hyscan_data_writer_sonar_channel_free);

  priv->signals = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                         NULL, hyscan_data_writer_raw_signal_free);

  priv->tvg = g_hash_table_new_full     (g_direct_hash, g_direct_equal,
                                         NULL, hyscan_data_writer_raw_gain_free);
}

static void
hyscan_data_writer_object_finalize (GObject *object)
{
  HyScanDataWriter *writer = HYSCAN_DATA_WRITER (object);
  HyScanDataWriterPrivate *priv = writer->priv;

  g_hash_table_unref (priv->sensor_positions);
  g_hash_table_unref (priv->sensor_channels);
  g_hash_table_unref (priv->sonar_positions);
  g_hash_table_unref (priv->sonar_channels);
  g_hash_table_unref (priv->signals);
  g_hash_table_unref (priv->tvg);

  if (priv->log_id > 0)
    hyscan_db_close (priv->db, priv->log_id);
  if (priv->track_id > 0)
    hyscan_db_close (priv->db, priv->track_id);

  g_string_free (priv->log_message, TRUE);
  g_object_unref (priv->log_data);

  g_free (priv->project_name);
  g_free (priv->track_name);

  g_free (priv->sonar_info);
  g_free (priv->operator_name);

  g_clear_object (&priv->db);

  g_mutex_clear (&priv->lock);
  g_rand_free (priv->rand);

  G_OBJECT_CLASS (hyscan_data_writer_parent_class)->finalize (object);
}

/* Функция формирует уникальный идентификатор для источника данных и номера канала. */
static gpointer
hyscan_data_writer_uniq_channel (HyScanSourceType source,
                                 guint            channel)
{
  gint id = (HYSCAN_SOURCE_LAST * channel) + source;

  return GINT_TO_POINTER (id);
}

/* Функция освобождает память занятую структурой HyScanDataWriterSensorChannel. */
static void
hyscan_data_writer_sensor_channel_free (gpointer data)
{
  HyScanDataWriterSensorChannel *info = data;

  hyscan_db_close (info->db, info->data_id);

  g_slice_free (HyScanDataWriterSensorChannel, info);
}

/* Функция освобождает память занятую структурой HyScanDataWriterAcousticChannel. */
static void
hyscan_data_writer_sonar_channel_free (gpointer data)
{
  HyScanDataWriterSonarChannel *info = data;

  if (info->data_id > 0)
    hyscan_db_close (info->db, info->data_id);
  if (info->noise_id > 0)
    hyscan_db_close (info->db, info->noise_id);
  if (info->signal_id > 0)
    hyscan_db_close (info->db, info->signal_id);
  if (info->tvg_id > 0)
    hyscan_db_close (info->db, info->tvg_id);

  g_slice_free (HyScanDataWriterSonarChannel, info);
}

/* Функция освобождает память занятую структурой HyScanDataWriterSignal. */
static void
hyscan_data_writer_raw_signal_free (gpointer data)
{
  HyScanDataWriterSignal *signal = data;

  g_object_unref (signal->image);

  g_slice_free (HyScanDataWriterSignal, signal);
}

/* Функция освобождает память занятую структурой HyScanDataWriterGain. */
static void
hyscan_data_writer_raw_gain_free (gpointer data)
{
  HyScanDataWriterTVG *tvg = data;

  g_object_unref (tvg->gains);

  g_slice_free (HyScanDataWriterTVG, tvg);
}

/* Функция создаёт уникальный идентификатор. */
static void
hyscan_data_writer_get_id (GRand *rand,
                           gchar *buffer,
                           guint  size)
{
  guint i;

  for (i = 0; i < (size - 1); i++)
    {
      gint rnd = g_rand_int_range (rand, 0, 62);
      if (rnd < 10)
        buffer[i] = '0' + rnd;
      else if (rnd < 36)
        buffer[i] = 'a' + rnd - 10;
      else
        buffer[i] = 'A' + rnd - 36;
    }

  buffer[i] = 0;
}

/* Функция создаёт новый проект в системе хранения. */
static gboolean
hyscan_data_writer_create_project (HyScanDB    *db,
                                   const gchar *project_name,
                                   gint64       date_time,
                                   GRand       *rand)
{
  gboolean status = FALSE;

  gchar project_ids[33] = {0};
  gint32 project_id = -1;
  gint32 param_id = -1;

  HyScanParamList *param_list = NULL;
  GBytes *project_schema;

  /* Схема проекта. */
  project_schema = g_resources_lookup_data ("/org/hyscan/schemas/project-schema.xml",
                                            G_RESOURCE_LOOKUP_FLAGS_NONE, NULL);
  if (project_schema == NULL)
    {
      g_warning ("HyScanCore: can't load project schema");
      return FALSE;
    }

  /* Создаём проект или открываем если он уже существует. */
  project_id = hyscan_db_project_create (db, project_name,
                                         g_bytes_get_data (project_schema, NULL));
  if (project_id == 0)
    {
      project_id = hyscan_db_project_open (db, project_name);
    }
  else if (project_id > 0)
    {
      param_id = hyscan_db_project_param_open (db, project_id, PROJECT_INFO_GROUP);
      if (param_id < 0)
        goto exit;

      if (!hyscan_db_param_object_create (db, param_id, PROJECT_INFO_OBJECT, PROJECT_INFO_SCHEMA))
        goto exit;

      hyscan_data_writer_get_id (rand, project_ids, sizeof (project_ids));
      param_list = hyscan_param_list_new ();

      hyscan_param_list_set_string (param_list, "/id", project_ids);
      hyscan_param_list_set_integer (param_list, "/ctime", date_time);
      hyscan_param_list_set_integer (param_list, "/mtime", date_time);

      status = hyscan_db_param_set (db, param_id, PROJECT_INFO_OBJECT, param_list);
    }
  else
    {
      goto exit;
    }

  status = TRUE;

exit:
  g_bytes_unref (project_schema);
  g_clear_object (&param_list);

  if (param_id > 0)
    hyscan_db_close (db, param_id);

  if (project_id > 0)
    hyscan_db_close (db, project_id);

  return status;
}

/* Функция создаёт галс в системе хранения. */
static gint32
hyscan_data_writer_create_track (HyScanDB        *db,
                                 gint32           project_id,
                                 const gchar     *track_name,
                                 HyScanTrackType  track_type,
                                 gint64           date_time,
                                 const gchar     *operator,
                                 const gchar     *sonar,
                                 GRand           *rand)
{
  gboolean status = FALSE;

  gchar track_ids[33] = {0};
  gint32 track_id = -1;
  gint32 param_id = -1;

  HyScanParamList *param_list = NULL;
  const gchar *track_type_name;
  GBytes *track_schema;

  /* Схема галса. */
  track_schema = g_resources_lookup_data ("/org/hyscan/schemas/track-schema.xml",
                                          G_RESOURCE_LOOKUP_FLAGS_NONE, NULL);
  if (track_schema == NULL)
    {
      g_warning ("HyScanCore: can't load track schema");
      return FALSE;
    }

  track_type_name = hyscan_track_get_name_by_type (track_type);
  if (track_type_name == NULL)
    goto exit;

  /* Создаём галс. Галс не должен существовать. */
  track_id = hyscan_db_track_create (db, project_id, track_name,
                                     g_bytes_get_data (track_schema, NULL), TRACK_SCHEMA);
  if (track_id <= 0)
    goto exit;

  /* Параметры галса. */
  param_id = hyscan_db_track_param_open (db, track_id);
  if (param_id <= 0)
    goto exit;

  hyscan_data_writer_get_id (rand, track_ids, sizeof (track_ids));
  param_list = hyscan_param_list_new ();

  hyscan_param_list_set_string (param_list, "/id", track_ids);
  hyscan_param_list_set_integer (param_list, "/ctime", date_time);
  hyscan_param_list_set_string (param_list, "/type", track_type_name);
  hyscan_param_list_set_string (param_list, "/operator", operator);
  hyscan_param_list_set_string (param_list, "/sonar", sonar);
  if (!hyscan_db_param_set (db, param_id, NULL, param_list))
    goto exit;

  /* Параметры проекта. */
  hyscan_db_close (db, param_id);
  param_id = hyscan_db_project_param_open (db, project_id, PROJECT_INFO_GROUP);
  if (param_id <= 0)
    goto exit;

  /* Обновляем дату и время изменения проекта. */
  hyscan_param_list_clear (param_list);
  hyscan_param_list_set_integer (param_list, "/mtime", date_time);
  if (!hyscan_db_param_set (db, param_id, PROJECT_INFO_OBJECT, param_list))
    goto exit;

  /* Дополнительная информация о галсе. */
  if (!hyscan_db_param_object_create (db, param_id, track_ids, TRACK_INFO_SCHEMA))
    goto exit;

  hyscan_param_list_clear (param_list);
  hyscan_param_list_set_integer (param_list, "/mtime", date_time);
  if (!hyscan_db_param_set (db, param_id, track_ids, param_list))
    goto exit;

  status = TRUE;

exit:
  g_bytes_unref (track_schema);
  g_clear_object (&param_list);

  if (param_id > 0)
    hyscan_db_close (db, param_id);

  if ((!status) && (track_id > 0))
    {
      hyscan_db_close (db, track_id);
      track_id = -1;
    }

  return track_id;
}

/* Функция создаёт канал для записи информационных сообщений. */
static gint32
hyscan_data_writer_create_log_channel (HyScanDataWriterPrivate *priv)
{
  const gchar *channel_name;
  gint32 channel_id;

  /* Канал записи сообщений. */
  channel_name =  hyscan_source_get_name_by_type (HYSCAN_SOURCE_LOG);
  channel_id = hyscan_db_channel_create (priv->db, priv->track_id, channel_name, LOG_CHANNEL_SCHEMA);
  if (channel_id < 0)
    return -1;

  if (priv->chunk_size > 0)
    hyscan_db_channel_set_chunk_size (priv->db, channel_id, priv->chunk_size);

  return channel_id;
}

/* Функция создаёт канал для записи данных от датчиков. */
static HyScanDataWriterSensorChannel *
hyscan_data_writer_create_sensor_channel (HyScanDataWriterPrivate *priv,
                                          const gchar             *sensor,
                                          HyScanSourceType         source,
                                          guint                    channel)
{
  HyScanDataWriterSensorChannel *channel_info = NULL;
  HyScanAntennaPosition *position;
  const gchar *channel_name;
  gint32 channel_id;

  /* Канал записи данных. */
  channel_name = hyscan_channel_get_name_by_types (source, HYSCAN_CHANNEL_DATA, channel);
  channel_id = hyscan_db_channel_create (priv->db, priv->track_id,
                                         channel_name, SENSOR_CHANNEL_SCHEMA);
  if (channel_id < 0)
    goto exit;

  /* Местоположение приёмных антенн. */
  position = g_hash_table_lookup (priv->sensor_positions, sensor);
  if (position != NULL)
    {
      if (!hyscan_core_params_set_antenna_position (priv->db, channel_id, position))
        goto exit;
    }
  else
    {
      g_info ("HyScanDataWriter: unspecified antenna position for sensor %s", sensor);
    }

  channel_info = g_slice_new0 (HyScanDataWriterSensorChannel);
  channel_info->db = priv->db;
  channel_info->source = source;
  channel_info->channel = channel;
  channel_info->data_id = channel_id;

  g_hash_table_insert (priv->sensor_channels,
                       hyscan_data_writer_uniq_channel (source, channel),
                       channel_info);

  if (priv->chunk_size > 0)
    hyscan_db_channel_set_chunk_size (priv->db, channel_info->data_id, priv->chunk_size);

exit:
  if ((channel_info == NULL) && (channel_id > 0))
    hyscan_db_close (priv->db, channel_id);

  return channel_info;
}

/* Функция создаёт канал для записи гидроакустических данных. */
static HyScanDataWriterSonarChannel *
hyscan_data_writer_create_acoustic_channel (HyScanDataWriterPrivate *priv,
                                            HyScanSourceType         source,
                                            guint                    channel,
                                            HyScanAcousticDataInfo  *info)
{
  HyScanDataWriterSonarChannel *channel_info = NULL;
  HyScanAntennaPosition *position;
  HyScanDataWriterSignal *signal;
  HyScanDataWriterTVG *tvg;

  const gchar *data_channel_name = NULL;
  const gchar *noise_channel_name = NULL;
  const gchar *signal_channel_name = NULL;
  const gchar *tvg_channel_name = NULL;
  gint32 data_id = -1;
  gint32 noise_id = -1;
  gint32 signal_id = -1;
  gint32 tvg_id = -1;

  /* Название канала для данных. */
  data_channel_name = hyscan_channel_get_name_by_types (source, HYSCAN_CHANNEL_DATA, channel);
  noise_channel_name = hyscan_channel_get_name_by_types (source, HYSCAN_CHANNEL_NOISE, channel);
  signal_channel_name = hyscan_channel_get_name_by_types (source, HYSCAN_CHANNEL_SIGNAL, channel);
  tvg_channel_name = hyscan_channel_get_name_by_types (source, HYSCAN_CHANNEL_TVG, channel);

  /* Каналы образов сигналов. */
  signal_id = hyscan_db_channel_create (priv->db, priv->track_id,
                                        signal_channel_name, SIGNAL_CHANNEL_SCHEMA);
  if (signal_id < 0)
    {
      g_warning ("HyScanDataWriter: %s.%s.%s: can't create signal channel",
                 priv->project_name, priv->track_name, data_channel_name);
      goto exit;
    }

  if (!hyscan_core_params_set_signal_info (priv->db, signal_id, info->data_rate))
    {
      g_warning ("HyScanDataWriter: %s.%s.%s: can't set signal parameters",
                 priv->project_name, priv->track_name, data_channel_name);
      goto exit;
    }

  /* Каналы параметров ВАРУ. */
  tvg_id = hyscan_db_channel_create (priv->db, priv->track_id,
                                     tvg_channel_name, TVG_CHANNEL_SCHEMA);
  if (tvg_id < 0)
    {
      g_warning ("HyScanDataWriter: %s.%s.%s: can't create tvg channel",
                 priv->project_name, priv->track_name, data_channel_name);
      goto exit;
    }

  if (!hyscan_core_params_set_tvg_info (priv->db, tvg_id, info->data_rate))
    {
      g_warning ("HyScanDataWriter: %s.%s.%s: can't set tvg parameters",
                 priv->project_name, priv->track_name, data_channel_name);
      goto exit;
    }

  /* Канал шумовой картины. */
  noise_id = hyscan_db_channel_create (priv->db, priv->track_id,
                                       noise_channel_name, ACOUSTIC_CHANNEL_SCHEMA);
  if (noise_id < 0)
    {
      g_warning ("HyScanDataWriter: %s.%s.%s: can't create noise channel",
                 priv->project_name, priv->track_name, data_channel_name);
      goto exit;
    }

  if (!hyscan_core_params_set_acoustic_data_info (priv->db, noise_id, info))
    {
      g_warning ("HyScanDataWriter: %s.%s.%s: can't set noise parameters",
                 priv->project_name, priv->track_name, data_channel_name);
      goto exit;
    }

  /* Канал записи данных. */
  data_id = hyscan_db_channel_create (priv->db, priv->track_id,
                                      data_channel_name, ACOUSTIC_CHANNEL_SCHEMA);
  if (data_id < 0)
    {
      g_warning ("HyScanDataWriter: %s.%s.%s: can't create channel",
                 priv->project_name, priv->track_name, data_channel_name);
      goto exit;
    }

  if (!hyscan_core_params_set_acoustic_data_info (priv->db, data_id, info))
    {
      g_warning ("HyScanDataWriter: %s.%s.%s: can't set data parameters",
                 priv->project_name, priv->track_name, data_channel_name);
      goto exit;
    }

  /* Местоположение приёмных антенн. */
  position = g_hash_table_lookup (priv->sonar_positions, GINT_TO_POINTER (source));
  if (position != NULL)
    {
      if (!hyscan_core_params_set_antenna_position (priv->db, data_id, position) ||
          !hyscan_core_params_set_antenna_position (priv->db, noise_id, position))
        {
          goto exit;
        }
    }
  else
    {
      g_info ("HyScanDataWriter: %s.%s.%s: unspecified antenna position",
              priv->project_name, priv->track_name, data_channel_name);
    }

  /* Записываем текущий сигнал. */
  signal = g_hash_table_lookup (priv->signals,
                                hyscan_data_writer_uniq_channel (source, channel));
  if ((signal != NULL) &&
      (hyscan_buffer_get_size (signal->image) >= 2 * sizeof (HyScanComplexFloat)))
    {
      if (!hyscan_db_channel_add_data (priv->db, signal_id, signal->time, signal->image, NULL))
        {
          g_warning ("HyScanDataWriter: %s.%s.%s: can't add signal",
                     priv->project_name, priv->track_name, data_channel_name);

          goto exit;
        }
    }

  /* Записываем параметры ВАРУ. */
  tvg = g_hash_table_lookup (priv->tvg,
                             hyscan_data_writer_uniq_channel (source, channel));
  if ((tvg != NULL) &&
      (hyscan_buffer_get_size (tvg->gains) >= sizeof (gfloat)))
    {
      if (!hyscan_db_channel_add_data (priv->db, tvg_id, tvg->time, tvg->gains, NULL))
        {
          g_warning ("HyScanDataWriter: %s.%s.%s: can't add tvg",
                     priv->project_name, priv->track_name, data_channel_name);

          goto exit;
        }
    }

  /* Информация о канале записи гидроакустических данных. */
  channel_info = g_slice_new0 (HyScanDataWriterSonarChannel);
  channel_info->db = priv->db;
  channel_info->source = source;
  channel_info->channel = channel;
  channel_info->data_id = data_id;
  channel_info->noise_id = noise_id;
  channel_info->signal_id = signal_id;
  channel_info->tvg_id = tvg_id;
  channel_info->data_type = info->data_type;
  channel_info->data_rate = info->data_rate;

  g_hash_table_insert (priv->sonar_channels,
                       hyscan_data_writer_uniq_channel (source, channel),
                       channel_info);

  if (priv->chunk_size > 0)
    {
      hyscan_db_channel_set_chunk_size (priv->db, channel_info->data_id, priv->chunk_size);
      hyscan_db_channel_set_chunk_size (priv->db, channel_info->noise_id, priv->chunk_size);
      hyscan_db_channel_set_chunk_size (priv->db, channel_info->signal_id, priv->chunk_size);
      hyscan_db_channel_set_chunk_size (priv->db, channel_info->tvg_id, priv->chunk_size);
    }

exit:
  if (channel_info == NULL)
    {
      if (data_id > 0)
        hyscan_db_close (priv->db, data_id);
      if (noise_id > 0)
        hyscan_db_close (priv->db, noise_id);
      if (signal_id > 0)
        hyscan_db_close (priv->db, signal_id);
      if (tvg_id > 0)
        hyscan_db_close (priv->db, tvg_id);
    }

  return channel_info;
}

/**
 * hyscan_data_writer_new:
 *
 * Функция создаёт новый объект #HyScanDataWriter.
 *
 * Returns: #HyScanDataWriter. Для удаления #g_object_unref.
 */
HyScanDataWriter *
hyscan_data_writer_new (void)
{
  return g_object_new (HYSCAN_TYPE_DATA_WRITER, NULL);
}

/**
 * hyscan_data_writer_set_db:
 * @writer: указатель на #HyScanDataWriter
 * @db: указатель на #HyScanDB
 *
 * Функция устанавливает систему хранения данных. Система хранения может быть
 * установлена только при отключенной записи.
 */
void
hyscan_data_writer_set_db (HyScanDataWriter *writer,
                           HyScanDB         *db)
{
  HyScanDataWriterPrivate *priv;

  g_return_if_fail (HYSCAN_IS_DATA_WRITER (writer));

  priv = writer->priv;

  g_mutex_lock (&priv->lock);
  if (priv->track_id < 0)
    {
      g_clear_object (&priv->db);
      priv->db = g_object_ref (db);
    }
  g_mutex_unlock (&priv->lock);
}

/**
 * hyscan_data_writer_set_operator_name:
 * @writer: указатель на #HyScanDataWriter
 * @name: имя оператора
 *
 * Функция устанавливает имя оператора, которое будет записываться в каждый галс при
 * его создании.
 */
void
hyscan_data_writer_set_operator_name (HyScanDataWriter *writer,
                                      const gchar      *name)
{
  HyScanDataWriterPrivate *priv;

  g_return_if_fail (HYSCAN_IS_DATA_WRITER (writer));

  priv = writer->priv;

  g_mutex_lock (&priv->lock);
  g_free (priv->operator_name);
  priv->operator_name = g_strdup (name);
  g_mutex_unlock (&priv->lock);
}

/**
 * hyscan_data_writer_set_sonar_info:
 * @writer: указатель на #HyScanDataWriter
 * @info: информация о гидролокаторе
 *
 * Функция устанавливает информацию о гидролокаторе, которая будет записываться
 * в каждый галс при его создании. Информация о гидролокаторе задаётся в виде
 * XML описания схемы данных #HyScanDataSchema.
 */
void
hyscan_data_writer_set_sonar_info (HyScanDataWriter *writer,
                                   const gchar      *info)
{
  HyScanDataWriterPrivate *priv;

  g_return_if_fail (HYSCAN_IS_DATA_WRITER (writer));

  priv = writer->priv;

  g_mutex_lock (&priv->lock);
  g_free (priv->sonar_info);
  priv->sonar_info = g_strdup (info);
  g_mutex_unlock (&priv->lock);
}

/**
 * hyscan_data_writer_set_chunk_size:
 * @writer: указатель на #HyScanDataWriter
 * @chunk_size: максимальный размер файлов в байтах или отрицательное число
 *
 * Функция устанавливает максимальный размер файлов в галсе. Подробнее
 * об этом можно прочитать в описании интерфейса #HyScanDB.
 */
void
hyscan_data_writer_set_chunk_size (HyScanDataWriter *writer,
                                   gint32            chunk_size)
{
  HyScanDataWriterPrivate *priv;

  GHashTableIter iter;
  gpointer data;

  g_return_if_fail (HYSCAN_IS_DATA_WRITER (writer));

  priv = writer->priv;

  g_mutex_lock (&priv->lock);

  if ((priv->db != NULL) && (chunk_size > 0))
    {
      g_hash_table_iter_init (&iter, priv->sensor_channels);
      while (g_hash_table_iter_next (&iter, NULL, &data))
        {
          HyScanDataWriterSensorChannel *channel = data;
          hyscan_db_channel_set_chunk_size (priv->db, channel->data_id, chunk_size);
        }

      g_hash_table_iter_init (&iter, priv->sonar_channels);
      while (g_hash_table_iter_next (&iter, NULL, &data))
        {
          HyScanDataWriterSonarChannel *channel = data;
          hyscan_db_channel_set_chunk_size (priv->db, channel->data_id, chunk_size);
          hyscan_db_channel_set_chunk_size (priv->db, channel->noise_id, chunk_size);
          hyscan_db_channel_set_chunk_size (priv->db, channel->signal_id, chunk_size);
          hyscan_db_channel_set_chunk_size (priv->db, channel->tvg_id, chunk_size);
        }
    }

  priv->chunk_size = chunk_size;

  g_mutex_unlock (&priv->lock);
}

/**
 * hyscan_data_writer_sensor_set_position:
 * @writer: указатель на #HyScanDataWriter
 * @sensor: название датчика
 * @position: информация о местоположении приёмной антенны
 *
 * Функция устанавливает информацию о местоположении приёмной антенны датчика.
 */
void
hyscan_data_writer_sensor_set_position (HyScanDataWriter            *writer,
                                        const gchar                 *sensor,
                                        const HyScanAntennaPosition *position)
{
  g_return_if_fail (HYSCAN_IS_DATA_WRITER (writer));

  g_mutex_lock (&writer->priv->lock);
  g_hash_table_insert (writer->priv->sensor_positions,
                       g_strdup (sensor),
                       hyscan_antenna_position_copy (position));
  g_mutex_unlock (&writer->priv->lock);
}

/**
 * hyscan_data_writer_sonar_set_position:
 * @writer: указатель на #HyScanDataWriter
 * @source: тип источника данных
 * @position: информация о местоположении приёмной антенны
 *
 * Функция устанавливает информацию о местоположении приёмной антенны
 * гидролокатора.
 */
void
hyscan_data_writer_sonar_set_position (HyScanDataWriter            *writer,
                                       HyScanSourceType             source,
                                       const HyScanAntennaPosition *position)
{
  g_return_if_fail (HYSCAN_IS_DATA_WRITER (writer));

  g_mutex_lock (&writer->priv->lock);
  g_hash_table_insert (writer->priv->sonar_positions,
                       GINT_TO_POINTER (source),
                       hyscan_antenna_position_copy (position));
  g_mutex_unlock (&writer->priv->lock);
}

/**
 * hyscan_data_writer_start:
 * @writer: указатель на #HyScanDataWriter
 * @project_name: название проекта для записи данных
 * @track_name: название галса для записи данных
 * @track_type: тип галса
 * @date_time: дата и время создания проека и галса или -1
 *
 * Функция включает запись данных. Пользователь может указать дату и время
 * создания проекта и галса отличные от текущих. Дата и время указываются
 * в микросекундах прошедших с 1 января 1970 для временной зоны UTC. Если
 * пользователь передаст отрицательное значение, будут использованы текущие
 * дата и время.
 *
 * Returns: %TRUE если запись включена, иначе %FALSE.
 */
gboolean
hyscan_data_writer_start (HyScanDataWriter *writer,
                          const gchar      *project_name,
                          const gchar      *track_name,
                          HyScanTrackType   track_type,
                          gint64            date_time)
{
  HyScanDataWriterPrivate *priv;
  gint32 project_id;

  gboolean status = FALSE;

  g_return_val_if_fail (HYSCAN_IS_DATA_WRITER (writer), FALSE);

  priv = writer->priv;

  g_mutex_lock (&priv->lock);

  /* Работа без системы хранения. */
  if (priv->db == NULL)
    {
      status = TRUE;
      goto exit;
    }

  /* Попросили начать запись в тот же галс. */
  if ((g_strcmp0 (project_name, priv->project_name) == 0) &&
      (g_strcmp0 (track_name, priv->track_name) == 0))
    {
      status = TRUE;
      goto exit;
    }

  /* Закрываем все открытые каналы. */
  g_hash_table_remove_all (priv->sensor_channels);
  g_hash_table_remove_all (priv->sonar_channels);

  /* Закрываем текущий галс. */
  if (priv->log_id > 0)
    hyscan_db_close (priv->db, priv->log_id);
  if (priv->track_id > 0)
    hyscan_db_close (priv->db, priv->track_id);

  priv->log_id = -1;
  priv->track_id = -1;

  g_clear_pointer (&priv->track_name, g_free);
  g_clear_pointer (&priv->project_name, g_free);

  /* Текущие дата и время. */
  if (date_time < 0)
    date_time = g_get_real_time ();

  /* Создаём проект, если он еще не создан. */
  if (!hyscan_data_writer_create_project (priv->db, project_name, date_time, priv->rand))
    goto exit;

  /* Открываем проект. */
  project_id = hyscan_db_project_open (priv->db, project_name);
  if (project_id < 0)
    goto exit;

  /* Создаём новый галс. */
  priv->track_id = hyscan_data_writer_create_track (priv->db, project_id,
                                                    track_name, track_type, date_time,
                                                    priv->operator_name, priv->sonar_info,
                                                    priv->rand);
  hyscan_db_close (priv->db, project_id);
  if (priv->track_id <= 0)
    {
      priv->track_id = -1;
      goto exit;
    }

  priv->project_name = g_strdup (project_name);
  priv->track_name = g_strdup (track_name);

  status = TRUE;

exit:
  g_mutex_unlock (&priv->lock);

  return status;
}

/**
 * hyscan_data_writer_stop:
 * @writer: указатель на #HyScanDataWriter
 *
 * Функция отключает запись данных.
 */
void
hyscan_data_writer_stop (HyScanDataWriter *writer)
{
  HyScanDataWriterPrivate *priv;
  GHashTableIter iter;
  gpointer data;

  g_return_if_fail (HYSCAN_IS_DATA_WRITER (writer));

  priv = writer->priv;

  g_mutex_lock (&priv->lock);

  /* Закрываем все открытые каналы. */
  g_hash_table_remove_all (priv->sensor_channels);
  g_hash_table_remove_all (priv->sonar_channels);

  /* Обнуляем текущие параметры ВАРУ и сигналы. */
  g_hash_table_iter_init (&iter, priv->signals);
  while (g_hash_table_iter_next (&iter, NULL, &data))
    {
      HyScanDataWriterSignal *signal = data;
      hyscan_buffer_set_size (signal->image, 0);
      signal->time = 0;
    }

  g_hash_table_iter_init (&iter, priv->tvg);
  while (g_hash_table_iter_next (&iter, NULL, &data))
    {
      HyScanDataWriterTVG *tvg = data;
      hyscan_buffer_set_size (tvg->gains, 0);
      tvg->time = 0;
    }

  /* Закрываем текущий галс. */
  if (priv->db != NULL)
    {
      if (priv->log_id > 0)
        hyscan_db_close (priv->db, priv->log_id);
      if (priv->track_id > 0)
        hyscan_db_close (priv->db, priv->track_id);

      priv->log_id = -1;
      priv->track_id = -1;
    }

  g_clear_pointer (&priv->project_name, g_free);
  g_clear_pointer (&priv->track_name, g_free);

  g_mutex_unlock (&priv->lock);
}

/**
 * hyscan_data_writer_log_add_message:
 * @writer: указатель на #HyScanDataWriter
 * @source: название источника сообщения
 * @time: метка времени, мкс
 * @level: тип сообщения
 * @message: сообщение
 *
 * Функция записывает информационные сообщения.
 *
 * Returns: %TRUE если запись выполнена, иначе %FALSE.
 */
gboolean
hyscan_data_writer_log_add_message (HyScanDataWriter *writer,
                                    const gchar      *source,
                                    gint64            time,
                                    HyScanLogLevel    level,
                                    const gchar      *message)
{
  HyScanDataWriterPrivate *priv;
  gboolean status = FALSE;

  g_return_val_if_fail (HYSCAN_IS_DATA_WRITER (writer), FALSE);

  priv = writer->priv;

  g_mutex_lock (&priv->lock);

  /* Работа без системы хранения. */
  if (priv->db == NULL)
    {
      status = TRUE;
      goto exit;
    }

  /* Текущий галс. */
  if (priv->track_id < 0)
    goto exit;

  /* Канал для записи сообщений или открываем новый. */
  if (priv->log_id < 0)
    {
      priv->log_id = hyscan_data_writer_create_log_channel (priv);
      if (priv->log_id < 0)
        goto exit;
    }

  /* Формируем сообщение. */
  g_string_printf (priv->log_message, "%s\t%s\t%s",
                   source,
                   hyscan_log_level_get_name_by_type (level),
                   message);

  hyscan_buffer_wrap_data (priv->log_data, HYSCAN_DATA_STRING,
                           priv->log_message->str, priv->log_message->len + 1);

  /* Записываем данные. */
  status = hyscan_db_channel_add_data (priv->db, priv->log_id, time, priv->log_data, NULL);
  if (!status)
    {
      g_warning ("HyScanDataWriter: %s.%s: can't add log message",
                 priv->project_name, priv->track_name);
    }

exit:
  g_mutex_unlock (&priv->lock);

  return status;
}

/**
 * hyscan_data_writer_sensor_add_data:
 * @writer: указатель на #HyScanDataWriter
 * @sensor: название датчика
 * @source: тип источника данных
 * @channel: индекс канала данных
 * @time: метка времени, мкс
 * @data: данные датчика
 *
 * Функция записывает данные от датчиков.
 *
 * Returns: %TRUE если запись выполнена, иначе %FALSE.
 */
gboolean
hyscan_data_writer_sensor_add_data (HyScanDataWriter *writer,
                                    const gchar      *sensor,
                                    HyScanSourceType  source,
                                    guint             channel,
                                    gint64            time,
                                    HyScanBuffer     *data)
{
  HyScanDataWriterPrivate *priv;
  HyScanDataWriterSensorChannel *channel_info;
  gboolean status = FALSE;

  g_return_val_if_fail (HYSCAN_IS_DATA_WRITER (writer), FALSE);

  priv = writer->priv;

  /* Проверяем тип данных на соответствие данным датчиков. */
  if (!hyscan_source_is_sensor (source))
    return FALSE;

  g_mutex_lock (&priv->lock);

  /* Работа без системы хранения. */
  if (priv->db == NULL)
    {
      status = TRUE;
      goto exit;
    }

  /* Текущий галс. */
  if (priv->track_id < 0)
    goto exit;

  /* Ищем канал для записи данных или открываем новый. */
  channel_info = g_hash_table_lookup (priv->sensor_channels,
                                      hyscan_data_writer_uniq_channel (source, channel));
  if (channel_info == NULL)
    {
      channel_info = hyscan_data_writer_create_sensor_channel (priv, sensor, source, channel);
      if (channel_info == NULL)
        goto exit;
    }

  /* Записываем данные. */
  status = hyscan_db_channel_add_data (priv->db, channel_info->data_id, time, data, NULL);
  if (!status)
    {
      g_warning ("HyScanDataWriter: %s.%s.%s: can't add data",
                 priv->project_name, priv->track_name,
                 hyscan_source_get_name_by_type (channel_info->source));
    }

exit:
  g_mutex_unlock (&priv->lock);

  return status;
}

/**
 * hyscan_data_writer_acoustic_add_data:
 * @writer: указатель на #HyScanDataWriter
 * @source: тип источника данных
 * @channel: индекс канала данных
 * @noise: признак шумовых данных
 * @time: метка времени, мкс
 * @info: параметры гидроакустических данных
 * @data: гидролокационные данные
 *
 * Функция записывает гидроакустические данные.
 *
 * Returns: %TRUE если запись выполнена, иначе %FALSE.
 */
gboolean
hyscan_data_writer_acoustic_add_data (HyScanDataWriter       *writer,
                                      HyScanSourceType        source,
                                      guint                   channel,
                                      gboolean                noise,
                                      gint64                  time,
                                      HyScanAcousticDataInfo *info,
                                      HyScanBuffer           *data)
{
  HyScanDataWriterPrivate *priv;
  HyScanDataWriterSonarChannel *channel_info;
  gboolean status = FALSE;

  g_return_val_if_fail (HYSCAN_IS_DATA_WRITER (writer), FALSE);

  priv = writer->priv;

  /* Проверяем тип данных на соответствие гидролокационным данным. */
  if (!hyscan_source_is_sonar (source))
    return FALSE;

  /* Проверяем совпадение типа данных. */
  if (hyscan_buffer_get_data_type (data) != info->data_type)
    return FALSE;

  g_mutex_lock (&priv->lock);

  /* Работа без системы хранения. */
  if (priv->db == NULL)
    {
      status = TRUE;
      goto exit;
    }

  /* Текущий галс. */
  if (priv->track_id < 0)
    goto exit;

  /* Ищем канал для записи данных или открываем новый. */
  channel_info = g_hash_table_lookup (priv->sonar_channels,
                                      hyscan_data_writer_uniq_channel (source, channel));
  if (channel_info == NULL)
    {
      channel_info = hyscan_data_writer_create_acoustic_channel (priv, source, channel, info);
      if (channel_info == NULL)
        goto exit;
    }

  /* Проверяем тип данных и частоту дискретизации. */
  if ((channel_info->data_type == info->data_type) &&
      (channel_info->data_rate == info->data_rate))
    {
      gint32 channel_id = noise ? channel_info->noise_id : channel_info->data_id;
      status = hyscan_db_channel_add_data (priv->db, channel_id, time, data, NULL);

      if (!status)
        {
          g_warning ("HyScanDataWriter: %s.%s.%s: can't add data",
                     priv->project_name, priv->track_name,
                     hyscan_source_get_name_by_type (channel_info->source));
        }
    }

exit:
  g_mutex_unlock (&priv->lock);

  return status;
}

/**
 * hyscan_data_writer_acoustic_add_signal:
 * @writer: указатель на #HyScanDataWriter
 * @source: тип источника данных
 * @channel: индекс канала данных
 * @time: метка времени, мкс
 * @image: (nullable): образ сигнала
 *
 * Функция записывает образ сигнала для свёртки для указанного канала
 * данных источника. Если образ сигнала = NULL, свёртка с данного момента
 * времени отменяется.
 *
 * Returns: %TRUE если запись выполнена, иначе %FALSE.
 */
gboolean
hyscan_data_writer_acoustic_add_signal (HyScanDataWriter *writer,
                                        HyScanSourceType  source,
                                        guint             channel,
                                        gint64            time,
                                        HyScanBuffer     *image)
{
  HyScanDataWriterPrivate *priv;
  HyScanDataWriterSonarChannel *channel_info;
  HyScanDataWriterSignal *signal;
  gboolean status = FALSE;

  g_return_val_if_fail (HYSCAN_IS_DATA_WRITER (writer), FALSE);

  priv = writer->priv;

  /* Проверяем тип данных в буфере. */
  if ((image != NULL) && (hyscan_buffer_get_data_type (image) != HYSCAN_DATA_COMPLEX_FLOAT32LE))
    return FALSE;

  /* Проверяем тип данных на соответствие гидролокационным данным. */
  if (!hyscan_source_is_sonar (source))
    return FALSE;

  g_mutex_lock (&priv->lock);

  /* Работа без системы хранения. */
  if (priv->db == NULL)
    {
      status = TRUE;
      goto exit;
    }

  /* Текущий галс. */
  if (priv->track_id < 0)
    goto exit;

  /* Ищем текущий образ или создаём новую запись. */
  signal = g_hash_table_lookup (priv->signals,
                                    hyscan_data_writer_uniq_channel (source, channel));
  if (signal == NULL)
    {
      signal = g_slice_new0 (HyScanDataWriterSignal);
      signal->image = hyscan_buffer_new ();

      g_hash_table_insert (priv->signals,
                           hyscan_data_writer_uniq_channel (source, channel),
                           signal);
    }

  /* Запоминаем текущий образ сигнала. */
  if (image != NULL)
    {
      hyscan_buffer_copy_data (signal->image, image);
    }
  else
    {
      guint32 zero[2] = {0, 0};
      hyscan_buffer_set_data (signal->image, HYSCAN_DATA_COMPLEX_FLOAT32LE, &zero, sizeof (zero));
    }
  signal->time = time;

  /* Записываем образ сигнала. */
  channel_info = g_hash_table_lookup (priv->sonar_channels,
                                      hyscan_data_writer_uniq_channel (source, channel));
  if (channel_info != NULL)
    {
      status = hyscan_db_channel_add_data (priv->db, channel_info->signal_id,
                                           signal->time, signal->image, NULL);
      if (!status)
        {
          g_warning ("HyScanDataWriter: %s.%s.%s: can't add signal",
                     priv->project_name, priv->track_name,
                     hyscan_source_get_name_by_type (channel_info->source));
        }
    }
  else
    {
      status = TRUE;
    }

exit:
  g_mutex_unlock (&priv->lock);

  return status;
}

/**
 * hyscan_data_writer_acoustic_add_tvg:
 * @writer: указатель на #HyScanDataWriter
 * @source: тип источника данных
 * @channel: индекс канала данных
 * @time: метка времени, мкс
 * @gains: коэффициенты передачи ВАРУ
 *
 * Функция записывает значения коэффициенты передачи ВАРУ для указанного
 * канала данных источника.
 *
 * Returns: %TRUE если запись выполнена, иначе %FALSE.
 */
gboolean
hyscan_data_writer_acoustic_add_tvg (HyScanDataWriter *writer,
                                     HyScanSourceType  source,
                                     guint             channel,
                                     gint64            time,
                                     HyScanBuffer     *gains)
{
  HyScanDataWriterPrivate *priv;
  HyScanDataWriterSonarChannel *channel_info;
  HyScanDataWriterTVG *cur_tvg;
  gboolean status = FALSE;

  g_return_val_if_fail (HYSCAN_IS_DATA_WRITER (writer), FALSE);

  priv = writer->priv;

  /* Проверяем тип данных в буфере. */
  if (hyscan_buffer_get_data_type (gains) != HYSCAN_DATA_FLOAT32LE)
    return FALSE;

  /* Проверяем тип данных на соответствие гидролокационным данным. */
  if (!hyscan_source_is_sonar (source))
    return FALSE;

  g_mutex_lock (&priv->lock);

  /* Работа без системы хранения. */
  if (priv->db == NULL)
    {
      status = TRUE;
      goto exit;
    }

  /* Текущий галс. */
  if (priv->track_id < 0)
    goto exit;

  /* Ищем текущие параметры ВАРУ или создаём новую запись. */
  cur_tvg = g_hash_table_lookup (priv->tvg,
                                 hyscan_data_writer_uniq_channel (source, channel));
  if (cur_tvg == NULL)
    {
      cur_tvg = g_slice_new0 (HyScanDataWriterTVG);
      cur_tvg->gains = hyscan_buffer_new ();

      g_hash_table_insert (priv->tvg,
                           hyscan_data_writer_uniq_channel (source, channel),
                           cur_tvg);
    }

  /* Запоминаем текущие параметры ВАРУ. */
  hyscan_buffer_copy_data (cur_tvg->gains, gains);
  cur_tvg->time = time;

  /* Записываем параметры ВАРУ. */
  channel_info = g_hash_table_lookup (priv->sonar_channels,
                                      hyscan_data_writer_uniq_channel (source, channel));
  if (channel_info != NULL)
    {
      status = hyscan_db_channel_add_data (priv->db, channel_info->tvg_id,
                                           cur_tvg->time, cur_tvg->gains, NULL);
      if (!status)
        {
          g_warning ("HyScanDataWriter: %s.%s.%s: can't add tvg",
                     priv->project_name, priv->track_name,
                     hyscan_source_get_name_by_type (channel_info->source));
        }
    }
  else
    {
      status = TRUE;
    }

exit:
  g_mutex_unlock (&priv->lock);

  return status;
}
