/*
 * \file hyscan-data-writer.c
 *
 * \brief Исходный файл класса управления записью данных
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-data-writer.h"
#include "hyscan-core-params.h"
#include <gio/gio.h>
#include <math.h>

enum
{
  PROP_O,
  PROP_DB
};

typedef struct
{
  HyScanDB                    *db;                             /* Интерфейс системы хранения данных. */
  gint32                       data_id;                        /* Идентификатор канала данных. */
} HyScanDataWriterSensorChannel;

typedef struct
{
  HyScanDB                    *db;                             /* Интерфейс системы хранения данных. */
  const gchar                 *name;                           /* Название канала данных. */
  gint32                       data_id;                        /* Идентификатор канала данных. */
  gint32                       noise_id;                       /* Идентификатор канала данных шумов. */
  gint32                       signal_id;                      /* Идентификатор канала сигналов. */
  gint32                       tvg_id;                         /* Идентификатор канала параметров ВАРУ. */
  HyScanSourceType             raw_source;                     /* Тип "сырых" данных. */
  HyScanDataType               data_type;                      /* Тип данных. */
  gdouble                      data_rate;                      /* Частота дискретизации данных, Гц. */
} HyScanDataWriterSonarChannel;

struct _HyScanDataWriterPrivate
{
  HyScanDB                    *db;                             /* Интерфейс системы хранения данных. */
  gchar                       *project_name;                   /* Название проекта для записи галсов.. */
  gchar                       *track_name;                     /* Название галса для записи данных. */
  gint32                       project_id;                     /* Идентификатор проекта для записи галсов. */
  gint32                       track_id;                       /* Идентификатор галса для записи данных. */

  gchar                       *operator_name;                  /* Имя оператора. */
  gchar                       *sonar_info;                     /* Информация о гидролокаторе. */

  GHashTable                  *sensor_positions;               /* Информация о местоположении антенн датчиков. */
  GHashTable                  *sensor_channels;                /* Список каналов для записи данных от датчиков. */

  GHashTable                  *sonar_positions;                /* Информация о местоположении гидролокационных антенн. */
  GHashTable                  *sonar_channels;                 /* Список каналов для записи гидролокационных данных. */
  GHashTable                  *signal;                         /* Список образов сигналов. */
  GHashTable                  *tvg;                            /* Список параметров ВАРУ. */

  HyScanDataWriterModeType     mode;                           /* Режим записи данных. */
  gint32                       chunk_size;                     /* Максимальный размер файлов в галсе. */
  gint64                       save_time;                      /* Интервал времени хранения данных. */
  gint64                       save_size;                      /* Максимальный объём данных в канале. */

  GMutex                       lock;                           /* Блокировка. */
  GRand                       *rand;                           /* Генератор случайных чисел. */
};

static void      hyscan_data_writer_set_property               (GObject                       *object,
                                                                guint                          prop_id,
                                                                const GValue                  *value,
                                                                GParamSpec                    *pspec);
static void      hyscan_data_writer_object_constructed         (GObject                       *object);
static void      hyscan_data_writer_object_finalize            (GObject                       *object);

static gpointer  hyscan_data_writer_uniq_channel               (HyScanSourceType               type,
                                                                gboolean                       raw,
                                                                guint                          channel);

static void      hyscan_data_writer_sensor_channel_free        (gpointer                       data);
static void      hyscan_data_writer_sonar_channel_free         (gpointer                       data);

static void      hyscan_data_writer_raw_signal_free            (gpointer                       data);
static void      hyscan_data_writer_raw_gain_free              (gpointer                       data);

static gboolean  hyscan_data_writer_create_project             (HyScanDB                      *db,
                                                                const gchar                   *project_name);

static gint32    hyscan_data_writer_create_track               (HyScanDB                      *db,
                                                                gint32                         project_id,
                                                                const gchar                   *track_name,
                                                                HyScanTrackType                track_type,
                                                                const gchar                   *operator,
                                                                const gchar                   *sonar,
                                                                GRand                         *rand);

static HyScanDataWriterSensorChannel *
                 hyscan_data_writer_create_sensor_channel      (HyScanDataWriterPrivate       *priv,
                                                                const gchar                   *sensor,
                                                                HyScanSourceType               source,
                                                                guint                          channel);

static HyScanDataWriterSonarChannel *
                 hyscan_data_writer_create_raw_channel         (HyScanDataWriterPrivate       *priv,
                                                                HyScanSourceType               source,
                                                                guint                          channel,
                                                                HyScanRawDataInfo             *info);

static HyScanDataWriterSonarChannel *
                 hyscan_data_writer_create_acoustic_channel    (HyScanDataWriterPrivate       *priv,
                                                                HyScanSourceType               source,
                                                                HyScanAcousticDataInfo        *info);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanDataWriter, hyscan_data_writer, G_TYPE_OBJECT)

static void
hyscan_data_writer_class_init (HyScanDataWriterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_data_writer_set_property;

  object_class->constructed = hyscan_data_writer_object_constructed;
  object_class->finalize = hyscan_data_writer_object_finalize;

  g_object_class_install_property (object_class, PROP_DB,
    g_param_spec_object ("db", "DB", "HyScanDB interface", HYSCAN_TYPE_DB,
                      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_data_writer_init (HyScanDataWriter *data_writer)
{
  data_writer->priv = hyscan_data_writer_get_instance_private (data_writer);
}

static void
hyscan_data_writer_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  HyScanDataWriter *writer = HYSCAN_DATA_WRITER (object);
  HyScanDataWriterPrivate *priv = writer->priv;

  switch (prop_id)
    {
    case PROP_DB:
      priv->db = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_data_writer_object_constructed (GObject *object)
{
  HyScanDataWriter *writer = HYSCAN_DATA_WRITER (object);
  HyScanDataWriterPrivate *priv = writer->priv;

  priv->rand = g_rand_new ();
  g_mutex_init (&priv->lock);

  priv->project_id = -1;
  priv->track_id = -1;
  priv->mode = HYSCAN_DATA_WRITER_MODE_BOTH;
  priv->chunk_size = -1;
  priv->save_time = -1;
  priv->save_size = -1;

  priv->sensor_positions = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
  priv->sensor_channels = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                                 NULL, hyscan_data_writer_sensor_channel_free);

  priv->sonar_positions = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);
  priv->sonar_channels = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                                NULL, hyscan_data_writer_sonar_channel_free);

  priv->signal = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                         NULL, hyscan_data_writer_raw_signal_free);
  priv->tvg = g_hash_table_new_full (g_direct_hash, g_direct_equal,
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
  g_hash_table_unref (priv->signal);
  g_hash_table_unref (priv->tvg);

  if (priv->track_id > 0)
    hyscan_db_close (priv->db, priv->track_id);
  if (priv->project_id > 0)
    hyscan_db_close (priv->db, priv->project_id);

  g_free (priv->project_name);
  g_free (priv->track_name);

  g_clear_object (&priv->db);

  g_free (priv->sonar_info);
  g_free (priv->operator_name);

  g_mutex_clear (&priv->lock);
  g_rand_free (priv->rand);

  G_OBJECT_CLASS (hyscan_data_writer_parent_class)->finalize (object);
}

/* Функция возвращает уникальный идентификатор для триплета: источник данных, признак сырых данных, индекс канала. */
static gpointer
hyscan_data_writer_uniq_channel (HyScanSourceType type,
                                 gboolean         raw,
                                 guint            channel)
{
  gint uniq = 1000 * type + (raw ? 0 : 500) + channel;
  return GINT_TO_POINTER (uniq);
}

/* Функция освобождает память занятую структурой HyScanDataWriterSensorChannel. */
static void
hyscan_data_writer_sensor_channel_free (gpointer data)
{
  HyScanDataWriterSensorChannel *info = data;

  hyscan_db_close (info->db, info->data_id);

  g_free (info);
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

  g_free (info);
}

/* Функция освобождает память занятую структурой HyScanDataWriterSignal. */
static void
hyscan_data_writer_raw_signal_free (gpointer data)
{
  HyScanDataWriterSignal *signal = data;

  g_free ((gpointer)signal->points);
  g_free (signal);
}

/* Функция освобождает память занятую структурой HyScanDataWriterGain. */
static void
hyscan_data_writer_raw_gain_free (gpointer data)
{
  HyScanDataWriterTVG *tvg = data;

  g_free ((gpointer)tvg->gains);
  g_free (tvg);
}

/* Функция создаёт новый проект в системе хранения. */
static gboolean
hyscan_data_writer_create_project (HyScanDB    *db,
                                   const gchar *project_name)
{
  gboolean status = FALSE;
  GBytes *project_schema;
  gint32 project_id;

  /* Схема проекта. */
  project_schema = g_resources_lookup_data ("/org/hyscan/schemas/project-schema.xml",
                                            G_RESOURCE_LOOKUP_FLAGS_NONE, NULL);
  if (project_schema == NULL)
    {
      g_warning ("HyScanCore: can't load project schema");
      return FALSE;
    }

  project_id = hyscan_db_project_create (db, project_name, g_bytes_get_data (project_schema, NULL));
  if (project_id >= 0)
    status = TRUE;

  g_clear_pointer (&project_schema, g_bytes_unref);
  hyscan_db_close (db, project_id);

  return status;
}

/* Функция создаёт галс в системе хранения. */
static gint32
hyscan_data_writer_create_track (HyScanDB        *db,
                                 gint32           project_id,
                                 const gchar     *track_name,
                                 HyScanTrackType  track_type,
                                 const gchar     *operator,
                                 const gchar     *sonar,
                                 GRand           *rand)
{
  gboolean status = FALSE;

  gint32 track_id = -1;
  gint32 param_id = -1;

  const gchar *track_type_name;
  GBytes *track_schema;

  gchar id[33] = {0};
  guint i;

  /* Схема галса. */
  track_schema = g_resources_lookup_data ("/org/hyscan/schemas/track-schema.xml",
                                          G_RESOURCE_LOOKUP_FLAGS_NONE, NULL);
  if (track_schema == NULL)
    {
      g_warning ("HyScanCore: can't load track schema");
      return FALSE;
    }

  /* Создаём галс. Галс не должен существовать. */
  track_id = hyscan_db_track_create (db, project_id, track_name,
                                     g_bytes_get_data (track_schema, NULL), TRACK_SCHEMA);
  if (track_id <= 0)
    goto exit;

  /* Параметры галса. */
  param_id = hyscan_db_track_param_open (db, track_id);
  if (param_id <= 0)
    goto exit;

  /* Уникальный идентификатор галса. */
  for (i = 0; i < sizeof (id) - 1; i++)
    {
      gint rnd = g_rand_int_range (rand, 0, 62);
      if (rnd < 10)
        id[i] = '0' + rnd;
      else if (rnd < 36)
        id[i] = 'a' + rnd - 10;
      else
        id[i] = 'A' + rnd - 36;
    }

  if (!hyscan_db_param_set_string (db, param_id, NULL, "/id", id))
    goto exit;

  track_type_name = hyscan_track_get_name_by_type (track_type);
  if (track_type_name != NULL)
    if (!hyscan_db_param_set_string (db, param_id, NULL, "/type", track_type_name))
      goto exit;

  if (operator != NULL)
    if (!hyscan_db_param_set_string (db, param_id, NULL, "/operator", operator))
      goto exit;

  if (sonar != NULL)
    if (!hyscan_db_param_set_string (db, param_id, NULL, "/sonar", sonar))
      goto exit;

  status = TRUE;

exit:
  g_clear_pointer (&track_schema, g_bytes_unref);

  if (param_id > 0)
    hyscan_db_close (db, param_id);

  if ((!status) && (track_id > 0))
    {
      hyscan_db_close (db, track_id);
      track_id = -1;
    }

  return track_id;
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
  channel_name =  hyscan_channel_get_name_by_types (source, TRUE, channel);
  channel_id = hyscan_db_channel_create (priv->db, priv->track_id, channel_name, SENSOR_CHANNEL_SCHEMA);
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

  channel_info = g_new0 (HyScanDataWriterSensorChannel, 1);
  channel_info->db = priv->db;
  channel_info->data_id = channel_id;

  g_hash_table_insert (priv->sensor_channels,
                       hyscan_data_writer_uniq_channel (source, TRUE, channel),
                       channel_info);

  if (priv->chunk_size > 0)
    hyscan_db_channel_set_chunk_size (priv->db, channel_info->data_id, priv->chunk_size);
  if (priv->save_time > 0)
    hyscan_db_channel_set_save_time (priv->db, channel_info->data_id, priv->save_time);
  if (priv->save_size > 0)
    hyscan_db_channel_set_save_size (priv->db, channel_info->data_id, priv->save_size);

exit:
  if (channel_info == NULL && channel_id > 0)
    hyscan_db_close (priv->db, channel_id);

  return channel_info;
}

/* Функция создаёт канал для записи "сырых" гидролокационных данных. */
static HyScanDataWriterSonarChannel *
hyscan_data_writer_create_raw_channel (HyScanDataWriterPrivate *priv,
                                       HyScanSourceType         source,
                                       guint                    channel,
                                       HyScanRawDataInfo       *info)
{
  HyScanDataWriterSonarChannel *channel_info = NULL;
  HyScanAntennaPosition *position;
  HyScanDataWriterSignal *signal;
  HyScanDataWriterTVG *tvg;

  const gchar *channel_name;
  gchar *name;
  gint32 data_id = -1;
  gint32 noise_id = -1;
  gint32 signal_id = -1;
  gint32 tvg_id = -1;

  gboolean status;

  /* Название канала для данных. */
  channel_name = hyscan_channel_get_name_by_types (source, TRUE, channel);
  if (channel_name == NULL)
    goto exit;

  /* Каналы образов сигналов. */
  name = g_strdup_printf ("%s-signal", channel_name);
  signal_id = hyscan_db_channel_create (priv->db, priv->track_id, name, SIGNAL_CHANNEL_SCHEMA);
  g_free (name);

  if (signal_id < 0)
    {
      g_warning ("HyScanDataWriter: %s.%s.%s: can't create signal channel",
                 priv->project_name, priv->track_name, channel_name);
      goto exit;
    }

  if (!hyscan_core_params_set_signal_info (priv->db, signal_id, info->data.rate))
    {
      g_warning ("HyScanDataWriter: %s.%s.%s: can't set signal parameters",
                 priv->project_name, priv->track_name, channel_name);
      goto exit;
    }

  /* Каналы параметров ВАРУ. */
  name = g_strdup_printf ("%s-tvg", channel_name);
  tvg_id = hyscan_db_channel_create (priv->db, priv->track_id, name, TVG_CHANNEL_SCHEMA);
  g_free (name);

  if (tvg_id < 0)
    {
      g_warning ("HyScanDataWriter: %s.%s.%s: can't create tvg channel",
                 priv->project_name, priv->track_name, channel_name);
      goto exit;
    }

  if (!hyscan_core_params_set_tvg_info (priv->db, tvg_id, info->data.rate))
    {
      g_warning ("HyScanDataWriter: %s.%s.%s: can't set tvg parameters",
                 priv->project_name, priv->track_name, channel_name);
      goto exit;
    }

  /* Канал шумовой картины. */
  name = g_strdup_printf ("%s-noise", channel_name);
  noise_id = hyscan_db_channel_create (priv->db, priv->track_id, name, RAW_CHANNEL_SCHEMA);
  g_free (name);

  if (noise_id < 0)
    {
      g_warning ("HyScanDataWriter: %s.%s.%s: can't create noise channel",
                 priv->project_name, priv->track_name, channel_name);
      goto exit;
    }

  if (!hyscan_core_params_set_raw_data_info (priv->db, noise_id, info))
    {
      g_warning ("HyScanDataWriter: %s.%s.%s: can't set data parameters",
                 priv->project_name, priv->track_name, channel_name);
      goto exit;
    }

  /* Канал записи данных. */
  data_id = hyscan_db_channel_create (priv->db, priv->track_id, channel_name, RAW_CHANNEL_SCHEMA);
  if (data_id < 0)
    {
      g_warning ("HyScanDataWriter: %s.%s.%s: can't create channel",
                 priv->project_name, priv->track_name, channel_name);
      goto exit;
    }

  if (!hyscan_core_params_set_raw_data_info (priv->db, data_id, info))
    {
      g_warning ("HyScanDataWriter: %s.%s.%s: can't set data parameters",
                 priv->project_name, priv->track_name, channel_name);
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
              priv->project_name, priv->track_name, channel_name);
    }

  channel_info = g_new0 (HyScanDataWriterSonarChannel, 1);
  channel_info->db = priv->db;
  channel_info->name = channel_name;
  channel_info->data_id = data_id;
  channel_info->noise_id = noise_id;
  channel_info->signal_id = signal_id;
  channel_info->tvg_id = tvg_id;
  channel_info->raw_source = source;
  channel_info->data_type = info->data.type;
  channel_info->data_rate = info->data.rate;

  g_hash_table_insert (priv->sonar_channels,
                       hyscan_data_writer_uniq_channel (source, TRUE, channel),
                       channel_info);

  if (priv->chunk_size > 0)
    hyscan_db_channel_set_chunk_size (priv->db, channel_info->data_id, priv->chunk_size);
  if (priv->save_time > 0)
    hyscan_db_channel_set_save_time (priv->db, channel_info->data_id, priv->save_time);
  if (priv->save_size > 0)
    hyscan_db_channel_set_save_size (priv->db, channel_info->data_id, priv->save_size);

  /* Записываем текущий сигнал. */
  signal = g_hash_table_lookup (priv->signal, GINT_TO_POINTER (source));
  if ((signal != NULL) && (signal->n_points > 0) && (signal->rate))
    {
      status = hyscan_db_channel_add_data (priv->db, channel_info->signal_id,
                                           signal->time, signal->points,
                                           signal->n_points * sizeof (HyScanComplexFloat),
                                           NULL);

      if (!status)
        {
          g_warning ("HyScanDataWriter: %s.%s.%s: can't add signal",
                     priv->project_name, priv->track_name, channel_info->name);
          goto exit;
        }
    }

  /* Записываем параметры ВАРУ. */
  tvg = g_hash_table_lookup (priv->tvg, hyscan_data_writer_uniq_channel (source, TRUE, channel));
  if (tvg != NULL && tvg->n_gains > 0)
    {
      status = hyscan_db_channel_add_data (priv->db, channel_info->tvg_id,
                                           tvg->time, tvg->gains,
                                           tvg->n_gains * sizeof (gfloat), NULL);

      if (!status)
        {
          g_warning ("HyScanDataWriter: %s.%s.%s: can't add tvg",
                     priv->project_name, priv->track_name, channel_info->name);
          goto exit;
        }
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

/* Функция создаёт канал для записи акустических данных. */
static HyScanDataWriterSonarChannel *
hyscan_data_writer_create_acoustic_channel (HyScanDataWriterPrivate *priv,
                                            HyScanSourceType         source,
                                            HyScanAcousticDataInfo  *info)
{
  HyScanDataWriterSonarChannel *channel_info = NULL;
  HyScanAntennaPosition *position;
  const gchar *channel_name;
  gint32 channel_id = -1;

  /* Название канала для данных. */
  channel_name = hyscan_channel_get_name_by_types (source, FALSE, 1);
  if (channel_name == NULL)
    goto exit;

  /* Канал записи данных. */
  channel_id = hyscan_db_channel_create (priv->db, priv->track_id, channel_name, ACOUSTIC_CHANNEL_SCHEMA);
  if (channel_id < 0)
    goto exit;

  /* Местоположение приёмных антенн. */
  position = g_hash_table_lookup (priv->sonar_positions, GINT_TO_POINTER (source));
  if (position != NULL)
    {
      if (!hyscan_core_params_set_antenna_position (priv->db, channel_id, position))
        goto exit;
    }
  else
    {
      g_info ("HyScanDataWriter: %s.%s.%s: unspecified antenna position",
              priv->project_name, priv->track_name, channel_name);
    }

  /* Параметры акустических данных */
  if (!hyscan_core_params_set_acoustic_data_info (priv->db, channel_id, info))
    goto exit;

  channel_info = g_new0 (HyScanDataWriterSonarChannel, 1);
  channel_info->db = priv->db;
  channel_info->name = channel_name;
  channel_info->data_id = channel_id;
  channel_info->signal_id = -1;
  channel_info->tvg_id = -1;
  channel_info->raw_source = 0;
  channel_info->data_type = info->data.type;
  channel_info->data_rate = info->data.rate;

  g_hash_table_insert (priv->sonar_channels,
                       hyscan_data_writer_uniq_channel (source, FALSE, 1),
                       channel_info);

  if (priv->chunk_size > 0)
    hyscan_db_channel_set_chunk_size (priv->db, channel_info->data_id, priv->chunk_size);
  if (priv->save_time > 0)
    hyscan_db_channel_set_save_time (priv->db, channel_info->data_id, priv->save_time);
  if (priv->save_size > 0)
    hyscan_db_channel_set_save_size (priv->db, channel_info->data_id, priv->save_size);

exit:
  if (channel_info == NULL && channel_id > 0)
    hyscan_db_close (priv->db, channel_id);

  return  channel_info;
}

/* Функция создаёт новый объект HyScanDataWriter. */
HyScanDataWriter *
hyscan_data_writer_new (HyScanDB *db)
{
  return g_object_new (HYSCAN_TYPE_DATA_WRITER,
                       "db", db,
                       NULL);
}

/* Функция устанавливает название проекта, в который будет вестись запись галсов. */
gboolean
hyscan_data_writer_set_project (HyScanDataWriter *writer,
                                const gchar      *project_name)
{
  HyScanDataWriterPrivate *priv;

  gboolean status = FALSE;

  g_return_val_if_fail (HYSCAN_IS_DATA_WRITER (writer), FALSE);

  priv = writer->priv;

  if (priv->db == NULL)
    return TRUE;

  g_mutex_lock (&priv->lock);

  /* Закрываем текущий проект. */
  if (priv->project_id > 0)
    {
      g_clear_pointer (&priv->project_name, g_free);
      hyscan_db_close (priv->db, priv->project_id);
      priv->project_id = -1;
    }

  /* Создаём проект, если он еще не создан. */
  if (!hyscan_data_writer_create_project (priv->db, project_name))
    goto exit;

  /* Открываем проект. */
  priv->project_id = hyscan_db_project_open (priv->db, project_name);
  if (priv->project_id < 0)
    goto exit;

  priv->project_name = g_strdup (project_name);

  status = TRUE;

exit:
  g_mutex_unlock (&priv->lock);

  return status;
}

/* Функция устанавливает имя оператора. */
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

/* Функция устанавливает информацию о гидролокаторе. */
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

/* Функция устанавливает режим записи данных от гидролокатора. */
void
hyscan_data_writer_set_mode (HyScanDataWriter         *writer,
                             HyScanDataWriterModeType  mode)
{
  g_return_if_fail (HYSCAN_IS_DATA_WRITER (writer));

  if ((mode == HYSCAN_DATA_WRITER_MODE_NONE) ||
      (mode == HYSCAN_DATA_WRITER_MODE_RAW) ||
      (mode == HYSCAN_DATA_WRITER_MODE_COMPUTED) ||
      (mode == HYSCAN_DATA_WRITER_MODE_BOTH))
    {
      g_atomic_int_set (&writer->priv->mode, mode);
    }
}

/* Функция устанавливает максимальный размер файлов в галсе. */
gboolean
hyscan_data_writer_set_chunk_size (HyScanDataWriter *writer,
                                   gint32            chunk_size)
{
  HyScanDataWriterPrivate *priv;

  GHashTableIter iter;
  gpointer data;
  gboolean status = FALSE;

  g_return_val_if_fail (HYSCAN_IS_DATA_WRITER (writer), FALSE);

  priv = writer->priv;

  if (priv->db == NULL)
    return TRUE;

  g_mutex_lock (&priv->lock);

  if (chunk_size > 0)
    {
      g_hash_table_iter_init (&iter, priv->sensor_channels);
      while (g_hash_table_iter_next (&iter, NULL, &data))
        {
          HyScanDataWriterSensorChannel *channel = data;
          if (!hyscan_db_channel_set_chunk_size (priv->db, channel->data_id, chunk_size))
            goto exit;
        }

      g_hash_table_iter_init (&iter, priv->sonar_channels);
      while (g_hash_table_iter_next (&iter, NULL, &data))
        {
          HyScanDataWriterSonarChannel *channel = data;
          if (!hyscan_db_channel_set_chunk_size (priv->db, channel->data_id, chunk_size))
            goto exit;
        }
    }

  priv->chunk_size = chunk_size;

  status = TRUE;

exit:
  g_mutex_unlock (&priv->lock);

  return status;
}

/* Функция задаёт интервал времени, для которого сохраняются записываемые данные. */
gboolean
hyscan_data_writer_set_save_time (HyScanDataWriter *writer,
                                  gint64            save_time)
{
  HyScanDataWriterPrivate *priv;

  GHashTableIter iter;
  gpointer data;
  gboolean status = FALSE;

  g_return_val_if_fail (HYSCAN_IS_DATA_WRITER (writer), FALSE);

  priv = writer->priv;

  if (priv->db == NULL)
    return TRUE;

  g_mutex_lock (&priv->lock);

  if (save_time > 0)
    {
      g_hash_table_iter_init (&iter, priv->sensor_channels);
      while (g_hash_table_iter_next (&iter, NULL, &data))
        {
          HyScanDataWriterSensorChannel *channel = data;
          if (!hyscan_db_channel_set_save_time (priv->db, channel->data_id, save_time))
            goto exit;
        }

      g_hash_table_iter_init (&iter, priv->sonar_channels);
      while (g_hash_table_iter_next (&iter, NULL, &data))
        {
          HyScanDataWriterSonarChannel *channel = data;
          if (!hyscan_db_channel_set_save_time (priv->db, channel->data_id, save_time))
            goto exit;
        }
    }

  priv->save_time = save_time;

  status = TRUE;

exit:
  g_mutex_unlock (&priv->lock);

  return status;
}

/* Функция задаёт объём сохраняемых данных в канале. */
gboolean
hyscan_data_writer_set_save_size (HyScanDataWriter *writer,
                                  gint64            save_size)
{
  HyScanDataWriterPrivate *priv;

  GHashTableIter iter;
  gpointer data;
  gboolean status = FALSE;

  g_return_val_if_fail (HYSCAN_IS_DATA_WRITER (writer), FALSE);

  priv = writer->priv;

  if (priv->db == NULL)
    return TRUE;

  g_mutex_lock (&priv->lock);

  if (save_size)
    {
      g_hash_table_iter_init (&iter, priv->sensor_channels);
      while (g_hash_table_iter_next (&iter, NULL, &data))
        {
          HyScanDataWriterSensorChannel *channel = data;
          if (!hyscan_db_channel_set_save_size (priv->db, channel->data_id, save_size))
            goto exit;
        }

      g_hash_table_iter_init (&iter, priv->sonar_channels);
      while (g_hash_table_iter_next (&iter, NULL, &data))
        {
          HyScanDataWriterSonarChannel *channel = data;
          if (!hyscan_db_channel_set_save_size (priv->db, channel->data_id, save_size))
            goto exit;
        }
    }

  priv->save_size = save_size;

  status = TRUE;

exit:
  g_mutex_unlock (&priv->lock);

  return status;
}

/* Функция устанавливает информацию о местоположении приёмной антенны датчика. */
void
hyscan_data_writer_sensor_set_position (HyScanDataWriter      *writer,
                                        const gchar           *sensor,
                                        HyScanAntennaPosition *position)
{
  HyScanDataWriterPrivate *priv;
  HyScanAntennaPosition *cur_position;

  g_return_if_fail (HYSCAN_IS_DATA_WRITER (writer));

  priv = writer->priv;

  g_mutex_lock (&priv->lock);

  /* Ищем текущее положение антенны или создаём новую запись. */
  cur_position = g_hash_table_lookup (priv->sensor_positions, sensor);
  if (cur_position == NULL)
    {
      cur_position = g_new0 (HyScanAntennaPosition, 1);
      g_hash_table_insert (priv->sensor_positions, g_strdup (sensor), cur_position);
    }

  /* Сохраняем текущее местоположение. */
  *cur_position = *position;

  g_mutex_unlock (&priv->lock);
}

/* Функция устанавливает информацию о местоположении приёмной антенны гидролокатора. */
void
hyscan_data_writer_sonar_set_position (HyScanDataWriter      *writer,
                                       HyScanSourceType       source,
                                       HyScanAntennaPosition *position)
{
  HyScanDataWriterPrivate *priv;
  HyScanAntennaPosition *cur_position;

  g_return_if_fail (HYSCAN_IS_DATA_WRITER (writer));

  priv = writer->priv;

  g_mutex_lock (&priv->lock);

  /* Ищем текущее положение антенны или создаём новую запись. */
  cur_position = g_hash_table_lookup (priv->sonar_positions, GINT_TO_POINTER (source));
  if (cur_position == NULL)
    {
      cur_position = g_new0 (HyScanAntennaPosition, 1);
      g_hash_table_insert (priv->sonar_positions, GINT_TO_POINTER (source), cur_position);
    }

  /* Сохраняем текущее местоположение. */
  *cur_position = *position;

  g_mutex_unlock (&priv->lock);
}

/* Функция включает запись данных. */
gboolean
hyscan_data_writer_start (HyScanDataWriter *writer,
                          const gchar      *track_name,
                          HyScanTrackType   track_type)
{
  HyScanDataWriterPrivate *priv;

  gboolean status = FALSE;

  g_return_val_if_fail (HYSCAN_IS_DATA_WRITER (writer), FALSE);

  priv = writer->priv;

  if (priv->db == NULL)
    return TRUE;

  g_mutex_lock (&priv->lock);

  /* Закрываем все открытые каналы. */
  g_hash_table_remove_all (priv->sensor_channels);
  g_hash_table_remove_all (priv->sonar_channels);

  /* Закрываем текущий галс. */
  if (priv->track_id > 0)
    {
      g_clear_pointer (&priv->track_name, g_free);
      hyscan_db_close (priv->db, priv->track_id);
      priv->track_id = -1;
    }

  /* Проект должен быть открыт. */
  if (priv->project_id < 0)
    goto exit;

  /* Создаём новый галс. */
  priv->track_id = hyscan_data_writer_create_track (priv->db, priv->project_id,
                                                    track_name, track_type,
                                                    priv->operator_name, priv->sonar_info,
                                                    priv->rand);
  if (priv->track_id <= 0)
    {
      priv->track_id = -1;
      goto exit;
    }

  priv->track_name = g_strdup (track_name);

  status = TRUE;

exit:
  g_mutex_unlock (&priv->lock);

  return status;
}

/* Функция отключает запись данных. */
void
hyscan_data_writer_stop (HyScanDataWriter *writer)
{
  HyScanDataWriterPrivate *priv;

  g_return_if_fail (HYSCAN_IS_DATA_WRITER (writer));

  priv = writer->priv;

  if (priv->db == NULL)
    return;

  g_mutex_lock (&priv->lock);

  /* Закрываем все открытые каналы. */
  g_hash_table_remove_all (priv->sensor_channels);
  g_hash_table_remove_all (priv->sonar_channels);

  /* Закрываем текущий галс. */
  if (priv->track_id > 0)
    {
      g_clear_pointer (&priv->track_name, g_free);
      hyscan_db_close (priv->db, priv->track_id);
      priv->track_id = -1;
    }

  g_mutex_unlock (&priv->lock);
}

/* Функция записывает данные от датчиков. */
gboolean hyscan_data_writer_sensor_add_data (HyScanDataWriter     *writer,
                                             const gchar          *sensor,
                                             HyScanSourceType      source,
                                             guint                 channel,
                                             HyScanDataWriterData *data)
{
  HyScanDataWriterPrivate *priv;
  HyScanDataWriterSensorChannel *channel_info;
  HyScanDataWriterModeType mode;
  gboolean status = FALSE;

  g_return_val_if_fail (HYSCAN_IS_DATA_WRITER (writer), FALSE);

  priv = writer->priv;

  /* Проверяем тип данных на соответствие данным датчиков. */
  if (!hyscan_source_is_sensor (source))
    {
      g_warning ("HyScanDataWriter: incorrect sensor source %s",
                 hyscan_channel_get_name_by_types (source, FALSE,1));
      return FALSE;
    }

  if (priv->db == NULL)
    return TRUE;

  mode = g_atomic_int_get (&writer->priv->mode);
  if (mode == HYSCAN_DATA_WRITER_MODE_NONE)
    return TRUE;

  g_mutex_lock (&priv->lock);

  /* Текущий галс. */
  if (priv->track_id < 0)
    goto exit;

  /* Ищем канал для записи данных или открываем новый. */
  channel_info = g_hash_table_lookup (priv->sensor_channels,
                                      hyscan_data_writer_uniq_channel (source, TRUE, channel));
  if (channel_info == NULL)
    {
      channel_info = hyscan_data_writer_create_sensor_channel (priv, sensor, source, channel);
      if (channel_info == NULL)
        goto exit;
    }

  /* Записываем данные. */
  status = hyscan_db_channel_add_data (priv->db, channel_info->data_id, data->time, data->data, data->size, NULL);

exit:
  g_mutex_unlock (&priv->lock);

  return status;
}

/* Функция записывает акустические данные. */
gboolean
hyscan_data_writer_raw_add_data (HyScanDataWriter     *writer,
                                 HyScanSourceType      source,
                                 guint                 channel,
                                 HyScanRawDataInfo    *info,
                                 HyScanDataWriterData *data)
{
  HyScanDataWriterPrivate *priv;
  HyScanDataWriterSonarChannel *channel_info;
  HyScanDataWriterModeType mode;
  gboolean status = FALSE;

  g_return_val_if_fail (HYSCAN_IS_DATA_WRITER (writer), FALSE);

  priv = writer->priv;

  /* Проверяем тип данных на соответствие "сырым". */
  if (!hyscan_source_is_raw (source))
    {
      g_warning ("HyScanDataWriter: incorrect raw source %s",
                 hyscan_channel_get_name_by_types (source, FALSE,1));
      return FALSE;
    }

  if (priv->db == NULL)
    return TRUE;

  mode = g_atomic_int_get (&writer->priv->mode);
  if (mode == HYSCAN_DATA_WRITER_MODE_NONE)
    return TRUE;
  if ((mode != HYSCAN_DATA_WRITER_MODE_RAW) && (mode != HYSCAN_DATA_WRITER_MODE_BOTH))
    return TRUE;

  g_mutex_lock (&priv->lock);

  /* Текущий галс. */
  if (priv->track_id < 0)
    {
      status = TRUE;
      goto exit;
    }

  /* Ищем канал для записи данных или открываем новый. */
  channel_info = g_hash_table_lookup (priv->sonar_channels,
                                      hyscan_data_writer_uniq_channel (source, TRUE, channel));
  if (channel_info == NULL)
    {
      channel_info = hyscan_data_writer_create_raw_channel (priv, source, channel, info);
      if (channel_info == NULL)
        goto exit;
    }

  /* Проверяем тип данных и частоту дисретизации. */
  if ((channel_info->data_type == info->data.type) &&
      (fabs (channel_info->data_rate - info->data.rate) < 1.0))
    {
      status = hyscan_db_channel_add_data (priv->db, channel_info->data_id, data->time, data->data, data->size, NULL);
    }

exit:
  g_mutex_unlock (&priv->lock);

  return status;
}

/* Функция записывает "сырые" гидролокационные данные без излучения, шум среды. */
gboolean
hyscan_data_writer_raw_add_noise (HyScanDataWriter     *writer,
                                  HyScanSourceType      source,
                                  guint                 channel,
                                  HyScanRawDataInfo    *info,
                                  HyScanDataWriterData *data)
{
  HyScanDataWriterPrivate *priv;
  HyScanDataWriterSonarChannel *channel_info;
  HyScanDataWriterModeType mode;
  gboolean status = FALSE;

  g_return_val_if_fail (HYSCAN_IS_DATA_WRITER (writer), FALSE);

  priv = writer->priv;

  /* Проверяем тип данных на соответствие "сырым". */
  if (!hyscan_source_is_raw (source))
    {
      g_warning ("HyScanDataWriter: incorrect raw source %s",
                 hyscan_channel_get_name_by_types (source, FALSE,1));
      return FALSE;
    }

  if (priv->db == NULL)
    return TRUE;

  mode = g_atomic_int_get (&writer->priv->mode);
  if (mode == HYSCAN_DATA_WRITER_MODE_NONE)
    return TRUE;
  if ((mode != HYSCAN_DATA_WRITER_MODE_RAW) && (mode != HYSCAN_DATA_WRITER_MODE_BOTH))
    return TRUE;

  g_mutex_lock (&priv->lock);

  /* Текущий галс. */
  if (priv->track_id < 0)
    {
      status = TRUE;
      goto exit;
    }

  /* Ищем канал для записи данных или открываем новый. */
  channel_info = g_hash_table_lookup (priv->sonar_channels,
                                      hyscan_data_writer_uniq_channel (source, TRUE, channel));
  if (channel_info == NULL)
    {
      channel_info = hyscan_data_writer_create_raw_channel (priv, source, channel, info);
      if (channel_info == NULL)
        goto exit;
    }

  /* Проверяем тип данных и частоту дискретизации. */
  if ((channel_info->data_type == info->data.type) &&
      (fabs (channel_info->data_rate - info->data.rate) < 1.0))
    {
      status = hyscan_db_channel_add_data (priv->db, channel_info->noise_id, data->time, data->data, data->size, NULL);
    }

exit:
  g_mutex_unlock (&priv->lock);

  return status;
}

/* Функция устанавливает образ сигнала для свёртки для указанного источника данных. */
gboolean
hyscan_data_writer_raw_add_signal (HyScanDataWriter       *writer,
                                   HyScanSourceType        source,
                                   HyScanDataWriterSignal *signal)
{
  HyScanDataWriterPrivate *priv;
  HyScanDataWriterSignal *cur_signal;
  HyScanDataWriterModeType mode;

  GHashTableIter iter;
  gpointer data;

  gboolean status = TRUE;

  g_return_val_if_fail (HYSCAN_IS_DATA_WRITER (writer), FALSE);

  priv = writer->priv;

  /* Проверяем тип данных на соответствие "сырым". */
  if (!hyscan_source_is_raw (source))
    {
      g_warning ("HyScanDataWriter: incorrect raw source %s",
                 hyscan_channel_get_name_by_types (source, FALSE,1));
      return FALSE;
    }

  if (priv->db == NULL)
    return TRUE;

  mode = g_atomic_int_get (&writer->priv->mode);
  if (mode == HYSCAN_DATA_WRITER_MODE_NONE)
    return TRUE;
  if ((mode != HYSCAN_DATA_WRITER_MODE_RAW) && (mode != HYSCAN_DATA_WRITER_MODE_BOTH))
    return TRUE;

  g_mutex_lock (&priv->lock);

  /* Ищем текущий сигнал или создаём новую запись. */
  cur_signal = g_hash_table_lookup (priv->signal, GINT_TO_POINTER (source));
  if (cur_signal == NULL)
    {
      cur_signal = g_new0 (HyScanDataWriterSignal, 1);
      g_hash_table_insert (priv->signal, GINT_TO_POINTER (source), cur_signal);
    }

  /* Освобождаем память занятую предыдущим сигналом. */
  if (cur_signal->points != NULL)
    g_free ((gpointer)cur_signal->points);

  /* Сохраняем текущий сигнал. */
  cur_signal->time = signal->time;
  cur_signal->rate = signal->rate;
  cur_signal->n_points = signal->n_points;
  cur_signal->points = g_memdup (signal->points, signal->n_points * sizeof (HyScanComplexFloat));

  /* Записываем сигнал в каналы с "сырыми" данными от текущего источника. */
  g_hash_table_iter_init (&iter, priv->sonar_channels);
  while (g_hash_table_iter_next (&iter, NULL, &data))
    {
      HyScanDataWriterSonarChannel *channel_info = data;
      HyScanComplexFloat zero = {0.0, 0.0};
      gconstpointer points;
      gint32 size;

      /* Проверяем тип источника. */
      if (channel_info->raw_source != source)
        continue;

      /* Частота дискретизации должна совпадать. */
      if ((fabs (channel_info->data_rate - cur_signal->rate) < 1.0))
        {
          if(cur_signal->n_points == 0)
            {
              size = sizeof (HyScanComplexFloat);
              points = &zero;
            }
          else
            {
              size = cur_signal->n_points * sizeof (HyScanComplexFloat);
              points = cur_signal->points;
            }

          status = hyscan_db_channel_add_data (priv->db, channel_info->signal_id,
                                               cur_signal->time, points, size, NULL);
          if (!status)
            {
              g_warning ("HyScanDataWriter: %s.%s.%s: can't add signal",
                         priv->project_name, priv->track_name, channel_info->name);

              break;
            }
        }
      else
        {
          g_warning ("HyScanDataWriter: %s.%s.%s: signal rate mismatch",
                     priv->project_name, priv->track_name, channel_info->name);

          status = FALSE;
          break;
        }
    }

  g_mutex_unlock (&priv->lock);

  return TRUE;
}

/* Функция устанавливает значения параметров усиления ВАРУ для указанного источника данных. */
gboolean
hyscan_data_writer_raw_add_tvg (HyScanDataWriter     *writer,
                                HyScanSourceType      source,
                                guint                 channel,
                                HyScanDataWriterTVG  *tvg)
{
  HyScanDataWriterPrivate *priv;
  HyScanDataWriterSonarChannel *channel_info;
  HyScanDataWriterTVG *cur_tvg;
  HyScanDataWriterModeType mode;
  gboolean status = TRUE;

  g_return_val_if_fail (HYSCAN_IS_DATA_WRITER (writer), FALSE);

  priv = writer->priv;

  /* Проверяем тип данных на соответствие "сырым". */
  if (!hyscan_source_is_raw (source))
    {
      g_warning ("HyScanDataWriter: incorrect raw source %s",
                 hyscan_channel_get_name_by_types (source, FALSE,1));
      return FALSE;
    }

  if (priv->db == NULL)
    return TRUE;

  mode = g_atomic_int_get (&writer->priv->mode);
  if (mode == HYSCAN_DATA_WRITER_MODE_NONE)
    return TRUE;
  if ((mode != HYSCAN_DATA_WRITER_MODE_RAW) && (mode != HYSCAN_DATA_WRITER_MODE_BOTH))
    return TRUE;

  g_mutex_lock (&priv->lock);

  /* Ищем текущие параметры ВАРУ или создаём новую запись. */
  cur_tvg = g_hash_table_lookup (priv->tvg, hyscan_data_writer_uniq_channel (source, TRUE, channel));
  if (cur_tvg == NULL)
    {
      cur_tvg = g_new0 (HyScanDataWriterTVG, 1);
      g_hash_table_insert (priv->tvg, hyscan_data_writer_uniq_channel (source, TRUE, channel), cur_tvg);
    }

  /* Освобождаем память занятую предыдущими параметрами ВАРУ. */
  if (cur_tvg->gains != NULL)
    g_free ((gpointer)cur_tvg->gains);

  /* Сохраняем текущие параметры ВАРУ. */
  cur_tvg->time = tvg->time;
  cur_tvg->rate = tvg->rate;
  cur_tvg->n_gains = tvg->n_gains;
  cur_tvg->gains = g_memdup (tvg->gains, tvg->n_gains * sizeof (gfloat));

  /* Записываем параметры ВАРУ в канал с "сырыми" данными от текущего источника. */
  channel_info = g_hash_table_lookup (priv->sonar_channels,
                                      hyscan_data_writer_uniq_channel (source, TRUE, channel));
  if ((channel_info != NULL) && (channel_info->raw_source == source))
    {
      /* Частота дискретизации должна совпадать. */
      if ((fabs (channel_info->data_rate - cur_tvg->rate) < 1.0))
        {
          status = hyscan_db_channel_add_data (priv->db, channel_info->tvg_id,
                                               cur_tvg->time, cur_tvg->gains,
                                               cur_tvg->n_gains * sizeof (gfloat), NULL);

          if (!status)
            {
              g_warning ("HyScanDataWriter: %s.%s.%s: can't add tvg",
                         priv->project_name, priv->track_name, channel_info->name);
            }
        }
      else
        {
          g_warning ("HyScanDataWriter: %s.%s.%s: tvg rate mismatch",
                     priv->project_name, priv->track_name, channel_info->name);

          status = FALSE;
        }
    }

  g_mutex_unlock (&priv->lock);

  return status;
}

gboolean
hyscan_data_writer_acoustic_add_data (HyScanDataWriter       *writer,
                                      HyScanSourceType        source,
                                      HyScanAcousticDataInfo *info,
                                      HyScanDataWriterData   *data)
{
  HyScanDataWriterPrivate *priv;
  HyScanDataWriterSonarChannel *channel_info;
  HyScanDataWriterModeType mode;
  gboolean status = FALSE;

  g_return_val_if_fail (HYSCAN_IS_DATA_WRITER (writer), FALSE);

  priv = writer->priv;

  /* Проверяем тип данных на соответствие акустическим. */
  if (!hyscan_source_is_acoustic (source))
    {
      g_warning ("HyScanDataWriter: incorrect acoustic source %s",
                 hyscan_channel_get_name_by_types (source, FALSE,1));
      return FALSE;
    }

  if (priv->db == NULL)
    return TRUE;

  mode = g_atomic_int_get (&writer->priv->mode);
  if (mode == HYSCAN_DATA_WRITER_MODE_NONE)
    return TRUE;
  if ((mode != HYSCAN_DATA_WRITER_MODE_COMPUTED) && (mode != HYSCAN_DATA_WRITER_MODE_BOTH))
    return TRUE;

  g_mutex_lock (&priv->lock);

  /* Текущий галс. */
  if (priv->track_id < 0)
    {
      status = TRUE;
      goto exit;
    }

  /* Ищем канал для записи данных или открываем новый. */
  channel_info = g_hash_table_lookup (priv->sonar_channels,
                                      hyscan_data_writer_uniq_channel (source, FALSE, 1));
  if (channel_info == NULL)
    {
      channel_info = hyscan_data_writer_create_acoustic_channel (priv, source, info);
      if (channel_info == NULL)
        goto exit;
    }

  /* Проверяем тип данных и частоту дисретизации. */
  if ((channel_info->data_type == info->data.type) &&
      (fabs (channel_info->data_rate - info->data.rate) < 1.0))
    {
      status = hyscan_db_channel_add_data (priv->db, channel_info->data_id, data->time, data->data, data->size, NULL);
    }

exit:
  g_mutex_unlock (&priv->lock);

  return status;
}
