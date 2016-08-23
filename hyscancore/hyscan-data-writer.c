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
#include "hyscan-core-schemas.h"
#include <gio/gio.h>

enum
{
  PROP_O,
  PROP_DB
};

typedef struct
{
  HyScanDB                    *db;                             /* Интерфейс системы хранения данных. */
  gint32                       channel_id;                     /* Идентификатор канала данных. */
} HyScanDataWriterSensorChannel;

typedef struct
{
  HyScanDB                    *db;                             /* Интерфейс системы хранения данных. */
  const gchar                 *channel_name;                   /* Название канала данных. */
  gint32                       channel_id;                     /* Идентификатор канала данных. */
  gint32                       signal_id;                      /* Идентификатор канала сигналов. */
  gint32                       tvg_id;                         /* Идентификатор канала параметров ВАРУ. */
  HyScanSourceType             raw_source;                     /* Тип "сырых" данных. */
  HyScanDataType               data_type;                      /* Тип данных. */
  gdouble                      data_rate;                      /* Частота дискретизации данных, Гц. */
  gdouble                      tvg_rate;                       /* Частота дискретизации параметров ВАРУ, Гц. */
} HyScanDataWriterAcousticChannel;

struct _HyScanDataWriterPrivate
{
  HyScanDB                    *db;                             /* Интерфейс системы хранения данных. */
  gint32                       track_id;                       /* Идентификатор галса для записи данных. */

  GHashTable                  *sensor_positions;               /* Информация о местоположении антенн датчиков. */
  GHashTable                  *sensor_channels;                /* Список каналов для записи данных от датчиков. */

  GHashTable                  *acoustic_positions;             /* Информация о местоположении гидроакустических антенн. */
  GHashTable                  *acoustic_channels;              /* Список каналов для записи гидроакустических данных. */
  GHashTable                  *signal;                         /* Список образов сигналов. */
  GHashTable                  *tvg;                            /* Список параметров ВАРУ. */

  gint32                       chunk_size;                     /* Максимальный размер файлов в галсе. */
  gint64                       save_time;                      /* Интервал времени хранения данных. */
  gint64                       save_size;                      /* Максимальный объём данных в канале. */

  GMutex                       lock;                           /* Блокировка. */
};

static void      hyscan_data_writer_set_property               (GObject                       *object,
                                                                guint                          prop_id,
                                                                const GValue                  *value,
                                                                GParamSpec                    *pspec);
static void      hyscan_data_writer_object_constructed         (GObject                       *object);
static void      hyscan_data_writer_object_finalize            (GObject                       *object);

static void      hyscan_data_writer_sensor_channel_free        (gpointer                       data);
static void      hyscan_data_writer_acoustic_channel_free      (gpointer                       data);

static void      hyscan_data_writer_acoustic_signal_free       (gpointer                       data);
static void      hyscan_data_writer_acoustic_gain_free         (gpointer                       data);

static gboolean  hyscan_data_writer_channel_set_position       (HyScanDB                      *db,
                                                                gint32                         channel_id,
                                                                HyScanAntennaPosition         *position);

static gboolean  hyscan_data_writer_channel_set_acoustic_info  (HyScanDB                      *db,
                                                                gint32                         channel_id,
                                                                HyScanAcousticDataInfo        *info);

static gboolean  hyscan_data_writer_channel_set_signal_info    (HyScanDB                      *db,
                                                                gint32                         channel_id,
                                                                gdouble                        rate);

static gboolean  hyscan_data_writer_channel_set_tvg_info       (HyScanDB                      *db,
                                                                gint32                         channel_id,
                                                                gdouble                        rate);

static gint32    hyscan_data_writer_track_create               (HyScanDB                      *db,
                                                                const gchar                   *project_name,
                                                                const gchar                   *track_name,
                                                                HyScanTrackType                track_type);

static HyScanDataWriterSensorChannel *hyscan_data_writer_sensor_channel_create (HyScanDB      *db,
                                                                gint32                         track_id,
                                                                const gchar                   *channel_name,
                                                                HyScanAntennaPosition         *position);

static HyScanDataWriterAcousticChannel *hyscan_data_writer_acoustic_channel_create (HyScanDB  *db,
                                                                gint32                         track_id,
                                                                HyScanSourceType               source,
                                                                gboolean                       raw,
                                                                guint                          channel,
                                                                HyScanAntennaPosition         *position,
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

  g_mutex_init (&priv->lock);

  priv->track_id = -1;
  priv->chunk_size = -1;
  priv->save_time = -1;
  priv->save_size = -1;

  priv->sensor_positions = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
  priv->sensor_channels = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                 g_free, hyscan_data_writer_sensor_channel_free);

  priv->acoustic_positions = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);
  priv->acoustic_channels = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                   g_free, hyscan_data_writer_acoustic_channel_free);

  priv->signal = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                         NULL, hyscan_data_writer_acoustic_signal_free);
  priv->tvg = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                      NULL, hyscan_data_writer_acoustic_gain_free);
}

static void
hyscan_data_writer_object_finalize (GObject *object)
{
  HyScanDataWriter *writer = HYSCAN_DATA_WRITER (object);
  HyScanDataWriterPrivate *priv = writer->priv;

  g_hash_table_unref (priv->sensor_positions);
  g_hash_table_unref (priv->sensor_channels);
  g_hash_table_unref (priv->acoustic_positions);
  g_hash_table_unref (priv->acoustic_channels);
  g_hash_table_unref (priv->signal);
  g_hash_table_unref (priv->tvg);

  if (priv->track_id > 0)
    hyscan_db_close (priv->db, priv->track_id);

  g_clear_object (&priv->db);

  g_mutex_clear (&priv->lock);

  G_OBJECT_CLASS (hyscan_data_writer_parent_class)->finalize (object);
}

/* Функция освобождает память занятую структурой HyScanDataWriterSensorChannel. */
static void
hyscan_data_writer_sensor_channel_free (gpointer data)
{
  HyScanDataWriterSensorChannel *info = data;

  hyscan_db_close (info->db, info->channel_id);

  g_free (info);
}

/* Функция освобождает память занятую структурой HyScanDataWriterAcousticChannel. */
static void
hyscan_data_writer_acoustic_channel_free (gpointer data)
{
  HyScanDataWriterAcousticChannel *info = data;

  hyscan_db_close (info->db, info->channel_id);
  if (info->signal_id > 0)
    hyscan_db_close (info->db, info->signal_id);
  if (info->tvg_id > 0)
    hyscan_db_close (info->db, info->tvg_id);

  g_free (info);
}

/* Функция освобождает память занятую структурой HyScanDataWriterSignal. */
static void
hyscan_data_writer_acoustic_signal_free (gpointer data)
{
  HyScanDataWriterSignal *signal = data;

  g_free ((gpointer)signal->points);
  g_free (signal);
}

/* Функция освобождает память занятую структурой HyScanDataWriterGain. */
static void
hyscan_data_writer_acoustic_gain_free (gpointer data)
{
  HyScanDataWriterTVG *tvg = data;

  g_free ((gpointer)tvg->gains);
  g_free (tvg);
}

/* Функция устанавливает параметры местоположения приёмной антенны. */
static gboolean
hyscan_data_writer_channel_set_position (HyScanDB              *db,
                                         gint32                 channel_id,
                                         HyScanAntennaPosition *position)
{
  gint32 param_id;
  gboolean status = TRUE;

  if (position == NULL)
    return TRUE;

  param_id = hyscan_db_channel_param_open (db, channel_id);
  if (param_id < 0)
    return FALSE;

  if (!hyscan_db_param_set_double (db, param_id, NULL, "/position/x", position->x))
    status = FALSE;
  if (!hyscan_db_param_set_double (db, param_id, NULL, "/position/y", position->y))
    status = FALSE;
  if (!hyscan_db_param_set_double (db, param_id, NULL, "/position/z", position->z))
    status = FALSE;
  if (!hyscan_db_param_set_double (db, param_id, NULL, "/position/psi", position->psi))
    status = FALSE;
  if (!hyscan_db_param_set_double (db, param_id, NULL, "/position/gamma", position->gamma))
    status = FALSE;
  if (!hyscan_db_param_set_double (db, param_id, NULL, "/position/theta", position->theta))
    status = FALSE;

  hyscan_db_close (db, param_id);

  return status;
}

/* Функция устанавливает параметры канала с акустическими данными. */
static gboolean
hyscan_data_writer_channel_set_acoustic_info (HyScanDB               *db,
                                              gint32                  channel_id,
                                              HyScanAcousticDataInfo *info)
{
  gint32 param_id;
  const gchar *data_type = hyscan_data_get_type_name (info->data.type);
  gboolean status = TRUE;

  if (info == NULL)
    return FALSE;

  param_id = hyscan_db_channel_param_open (db, channel_id);
  if (param_id < 0)
    return FALSE;

  if (!hyscan_db_param_set_string  (db, param_id, NULL, "/data/type", data_type))
    status = FALSE;
  if (!hyscan_db_param_set_double  (db, param_id, NULL, "/data/rate", info->data.rate))
    status = FALSE;

  if (!hyscan_db_param_set_double  (db, param_id, NULL, "/antenna/offset", info->antenna.offset))
    status = FALSE;
  if (!hyscan_db_param_set_double  (db, param_id, NULL, "/antenna/vertical-pattern", info->antenna.vertical_pattern))
    status = FALSE;
  if (!hyscan_db_param_set_double  (db, param_id, NULL, "/antenna/horizontal-pattern", info->antenna.horizontal_pattern))
    status = FALSE;

  if (!hyscan_db_param_set_double  (db, param_id, NULL, "/adc/vref", info->adc.vref))
    status = FALSE;
  if (!hyscan_db_param_set_integer (db, param_id, NULL, "/adc/offset", info->adc.offset))
    status = FALSE;

  hyscan_db_close (db, param_id);

  return status;
}

/* Функция устанавливает параметры канала с образами сигналов. */
static gboolean
hyscan_data_writer_channel_set_signal_info (HyScanDB *db,
                                            gint32    channel_id,
                                            gdouble   rate)
{
  gint32 param_id;
  const gchar *data_type = hyscan_data_get_type_name (HYSCAN_DATA_COMPLEX_FLOAT);
  gboolean status = TRUE;

  param_id = hyscan_db_channel_param_open (db, channel_id);
  if (param_id < 0)
    return FALSE;

  if (!hyscan_db_param_set_string (db, param_id, NULL, "/data/type", data_type))
    status = FALSE;
  if (!hyscan_db_param_set_double (db, param_id, NULL, "/data/rate", rate))
    status = FALSE;

  hyscan_db_close (db, param_id);

  return status;
}

/* Функция устанавливает параметры канала с параметрами ВАРУ. */
static gboolean
hyscan_data_writer_channel_set_tvg_info (HyScanDB *db,
                                         gint32    channel_id,
                                         gdouble   rate)
{
  gint32 param_id;
  const gchar *data_type = hyscan_data_get_type_name (HYSCAN_DATA_FLOAT);
  gboolean status = TRUE;

  param_id = hyscan_db_channel_param_open (db, channel_id);
  if (param_id < 0)
    return FALSE;

  if (!hyscan_db_param_set_string (db, param_id, NULL, "/data/type", data_type))
    status = FALSE;
  if (!hyscan_db_param_set_double (db, param_id, NULL, "/data/rate", rate))
    status = FALSE;

  hyscan_db_close (db, param_id);

  return status;
}

/* ункция создаёт галс в системе хранения. */
static gint32
hyscan_data_writer_track_create (HyScanDB        *db,
                                 const gchar     *project_name,
                                 const gchar     *track_name,
                                 HyScanTrackType  track_type)
{
  gboolean status = FALSE;

  gint32 project_id = -1;
  gint32 track_id = -1;
  gint32 param_id = -1;

  const gchar *track_type_name;
  GBytes *schema;

  schema = g_resources_lookup_data ("/org/hyscan/schemas/track-schema.xml", G_RESOURCE_LOOKUP_FLAGS_NONE, NULL);
  if (schema == NULL)
    {
      g_warning ("HyScanCore: can't find track schema");
      return FALSE;
    }

  project_id = hyscan_db_project_open (db, project_name);
  if (project_id <= 0)
    goto exit;

  track_id = hyscan_db_track_create (db, project_id, track_name, g_bytes_get_data (schema, NULL), TRACK_SCHEMA);
  if (track_id <= 0)
    goto exit;

  param_id = hyscan_db_track_param_open (db, track_id);
  if (param_id <= 0)
    goto exit;

  track_type_name = hyscan_track_get_name_by_type (track_type);
  if (track_type_name != NULL)
    if (!hyscan_db_param_set_string (db, param_id, NULL, "/type", track_type_name))
      goto exit;

  status = TRUE;

exit:
  g_clear_pointer (&schema, g_bytes_unref);
  if (project_id > 0)
    hyscan_db_close (db, project_id);
  if (param_id > 0)
    hyscan_db_close (db, param_id);
  if ((!status) && (track_id > 0))
    {
      hyscan_db_close (db, track_id);
      return -1;
    }

  return track_id;
}

/* Функция создаёт канал для записи данных от датчиков. */
static HyScanDataWriterSensorChannel *
hyscan_data_writer_sensor_channel_create (HyScanDB              *db,
                                          gint32                 track_id,
                                          const gchar           *channel_name,
                                          HyScanAntennaPosition *position)
{
  HyScanDataWriterSensorChannel *channel_info;
  gint32 channel_id;

  /* Канал записи данных. */
  channel_id = hyscan_db_channel_create (db, track_id, channel_name, SENSOR_CHANNEL_SCHEMA);
  if (channel_id < 0)
    goto exit;

  /* Местоположение приёмных антенн. */
  if (!hyscan_data_writer_channel_set_position (db, channel_id, position))
    goto exit;

  channel_info = g_new0 (HyScanDataWriterSensorChannel, 1);
  channel_info->db = db;
  channel_info->channel_id = channel_id;

exit:
  if (channel_info == NULL && channel_id > 0)
    hyscan_db_close (db, channel_id);

  return channel_info;
}

/* Функция создаёт канал для записи акустических данных. */
static HyScanDataWriterAcousticChannel *
hyscan_data_writer_acoustic_channel_create (HyScanDB               *db,
                                            gint32                  track_id,
                                            HyScanSourceType        source,
                                            gboolean                raw,
                                            guint                   channel,
                                            HyScanAntennaPosition  *position,
                                            HyScanAcousticDataInfo *info)
{
  HyScanDataWriterAcousticChannel *channel_info = NULL;

  const gchar *channel_name;
  gint32 channel_id = -1;
  gint32 signal_id = -1;
  gint32 tvg_id = -1;

  /* Название канала для данных. */
  channel_name = hyscan_channel_get_name_by_types (source, raw, channel);
  if (channel_name == NULL)
    goto exit;

  /* Для "сырых" данных создаём каналы записи образов сигналов и параметров ВАРУ. */
  if (raw)
    {
      gchar *signal_name = g_strdup_printf ("%s-signal", channel_name);
      gchar *tvg_name = g_strdup_printf ("%s-tvg", channel_name);

      signal_id = hyscan_db_channel_create (db, track_id, signal_name, SIGNAL_CHANNEL_SCHEMA);
      tvg_id = hyscan_db_channel_create (db, track_id, tvg_name, TVG_CHANNEL_SCHEMA);

      g_free (signal_name);
      g_free (tvg_name);

      if (signal_id < 0 || tvg_id < 0)
        goto exit;

      if (!hyscan_data_writer_channel_set_signal_info (db, signal_id, info->data.rate))
        goto exit;
    }

  /* Канал записи данных. */
  channel_id = hyscan_db_channel_create (db, track_id, channel_name, ACOUSTIC_CHANNEL_SCHEMA);
  if (channel_id < 0)
    goto exit;

  /* Местоположение приёмных антенн и параметры акустических данных. */
  if (!hyscan_data_writer_channel_set_position (db, channel_id, position) ||
      !hyscan_data_writer_channel_set_acoustic_info (db, channel_id, info))
    goto exit;

  channel_info = g_new0 (HyScanDataWriterAcousticChannel, 1);
  channel_info->db = db;
  channel_info->channel_name = channel_name;
  channel_info->channel_id = channel_id;
  channel_info->signal_id = signal_id;
  channel_info->tvg_id = tvg_id;
  channel_info->raw_source = raw ? source : 0;
  channel_info->data_type = info->data.type;
  channel_info->data_rate = info->data.rate;
  channel_info->tvg_rate = 0.0;

exit:
  if (channel_info == NULL)
    {
      if (channel_id > 0)
        hyscan_db_close (db, channel_id);
      if (signal_id > 0)
        hyscan_db_close (db, signal_id);
      if (tvg_id > 0)
        hyscan_db_close (db, tvg_id);
    }

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

/* Функция включает запись данных. */
gboolean
hyscan_data_writer_start (HyScanDataWriter *writer,
                          const gchar      *project_name,
                          const gchar      *track_name,
                          HyScanTrackType   track_type)
{
  HyScanDataWriterPrivate *priv;

  gboolean status = FALSE;

  g_return_val_if_fail (HYSCAN_IS_DATA_WRITER (writer), FALSE);

  priv = writer->priv;

  if (priv->db == NULL)
    return FALSE;

  g_mutex_lock (&priv->lock);

  /* Закрываем все открытые каналы. */
  g_hash_table_remove_all (priv->sensor_channels);
  g_hash_table_remove_all (priv->acoustic_channels);

  /* Создаём новый галс. */
  priv->track_id = hyscan_data_writer_track_create (priv->db, project_name, track_name, track_type);
  if (priv->track_id < 0)
    goto exit;

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
  g_hash_table_remove_all (priv->acoustic_channels);

  priv->track_id = -1;

  g_mutex_unlock (&priv->lock);
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
    return FALSE;

  g_mutex_lock (&priv->lock);

  if (chunk_size > 0)
    {
      g_hash_table_iter_init (&iter, priv->sensor_channels);
      while (g_hash_table_iter_next (&iter, NULL, &data))
        {
          HyScanDataWriterSensorChannel *channel = data;
          if (!hyscan_db_channel_set_chunk_size (priv->db, channel->channel_id, chunk_size))
            goto exit;
        }

      g_hash_table_iter_init (&iter, priv->acoustic_channels);
      while (g_hash_table_iter_next (&iter, NULL, &data))
        {
          HyScanDataWriterAcousticChannel *channel = data;
          if (!hyscan_db_channel_set_chunk_size (priv->db, channel->channel_id, chunk_size))
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
    return FALSE;

  g_mutex_lock (&priv->lock);

  if (save_time > 0)
    {
      g_hash_table_iter_init (&iter, priv->sensor_channels);
      while (g_hash_table_iter_next (&iter, NULL, &data))
        {
          HyScanDataWriterSensorChannel *channel = data;
          if (!hyscan_db_channel_set_save_time (priv->db, channel->channel_id, save_time))
            goto exit;
        }

      g_hash_table_iter_init (&iter, priv->acoustic_channels);
      while (g_hash_table_iter_next (&iter, NULL, &data))
        {
          HyScanDataWriterAcousticChannel *channel = data;
          if (!hyscan_db_channel_set_save_time (priv->db, channel->channel_id, save_time))
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
    return FALSE;

  g_mutex_lock (&priv->lock);

  if (save_size)
    {
      g_hash_table_iter_init (&iter, priv->sensor_channels);
      while (g_hash_table_iter_next (&iter, NULL, &data))
        {
          HyScanDataWriterSensorChannel *channel = data;
          if (!hyscan_db_channel_set_save_size (priv->db, channel->channel_id, save_size))
            goto exit;
        }

      g_hash_table_iter_init (&iter, priv->acoustic_channels);
      while (g_hash_table_iter_next (&iter, NULL, &data))
        {
          HyScanDataWriterAcousticChannel *channel = data;
          if (!hyscan_db_channel_set_save_size (priv->db, channel->channel_id, save_size))
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
gboolean
hyscan_data_writer_sensor_set_position (HyScanDataWriter      *writer,
                                        const gchar           *sensor,
                                        HyScanAntennaPosition *position)
{
  HyScanDataWriterPrivate *priv;
  HyScanAntennaPosition *cur_position;

  g_return_val_if_fail (HYSCAN_IS_DATA_WRITER (writer), FALSE);

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

  return TRUE;
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
  HyScanAntennaPosition *position;
  const gchar *channel_name;
  gboolean status = FALSE;

  g_return_val_if_fail (HYSCAN_IS_DATA_WRITER (writer), FALSE);

  priv = writer->priv;

  if (priv->db == NULL)
    return FALSE;

  /* Проверяем тип данных на соответствие данным датчиков. */
  if (!hyscan_source_is_sensor (source))
    {
      g_warning ("HyScanDataWriter: incorrect sensor source '%d'", source);
      return FALSE;
    }

  /* Название канала для записи данных. */
  channel_name =  hyscan_channel_get_name_by_types (source, TRUE, channel);
  if (channel_name == NULL)
    {
      g_warning ("HyScanDataWriter: unknown source '%d'", source);
      return FALSE;
    }

  priv = writer->priv;

  g_mutex_lock (&priv->lock);

  /* Текущий галс. */
  if (priv->track_id < 0)
    goto exit;

  /* Ищем канал для записи данных или открываем новый. */
  channel_info = g_hash_table_lookup (priv->sensor_channels, channel_name);
  if (channel_info == NULL)
    {
      position = g_hash_table_lookup (priv->sensor_positions, sensor);
      if (position == NULL)
        g_warning ("HyScanDataWriter: unspecified antenna position for sensor %s", sensor);

      channel_info = hyscan_data_writer_sensor_channel_create (priv->db, priv->track_id, channel_name, position);
      if (channel_info == NULL)
        goto exit;

      g_hash_table_insert (priv->sensor_channels, g_strdup (channel_name), channel_info);

      if (priv->chunk_size > 0)
        hyscan_db_channel_set_chunk_size (priv->db, channel_info->channel_id, priv->chunk_size);
      if (priv->save_time > 0)
        hyscan_db_channel_set_save_time (priv->db, channel_info->channel_id, priv->save_time);
      if (priv->save_size > 0)
        hyscan_db_channel_set_save_size (priv->db, channel_info->channel_id, priv->save_size);
    }

  /* Записываем данные. */
  if (hyscan_db_channel_add_data (priv->db, channel_info->channel_id, data->time, data->data, data->size, NULL))
    status = TRUE;

exit:
  g_mutex_unlock (&priv->lock);

  return status;
}

/* Функция устанавливает информацию о местоположении приёмной антенны гидролокатора. */
gboolean
hyscan_data_writer_acoustic_set_position (HyScanDataWriter      *writer,
                                          HyScanSourceType       source,
                                          HyScanAntennaPosition *position)
{
  HyScanDataWriterPrivate *priv;
  HyScanAntennaPosition *cur_position;

  g_return_val_if_fail (HYSCAN_IS_DATA_WRITER (writer), FALSE);

  priv = writer->priv;

  g_mutex_lock (&priv->lock);

  /* Ищем текущее положение антенны или создаём новую запись. */
  cur_position = g_hash_table_lookup (priv->acoustic_positions, GINT_TO_POINTER (source));
  if (cur_position == NULL)
    {
      cur_position = g_new0 (HyScanAntennaPosition, 1);
      g_hash_table_insert (priv->acoustic_positions, GINT_TO_POINTER (source), cur_position);
    }

  /* Сохраняем текущее местоположение. */
  *cur_position = *position;

  g_mutex_unlock (&priv->lock);

  return TRUE;
}

/* Функция записывает акустические данные. */
gboolean
hyscan_data_writer_acoustic_add_data (HyScanDataWriter       *writer,
                                      HyScanSourceType        source,
                                      gboolean                raw,
                                      guint                   channel,
                                      HyScanAcousticDataInfo *info,
                                      HyScanDataWriterData   *data)
{
  HyScanDataWriterPrivate *priv;

  HyScanDataWriterAcousticChannel *channel_info;
  const gchar *channel_name;
  gboolean status = FALSE;

  g_return_val_if_fail (HYSCAN_IS_DATA_WRITER (writer), FALSE);

  priv = writer->priv;

  if (priv->db == NULL)
    return FALSE;

  /* Проверяем тип данных на соответствие акустическим. */
  if (!hyscan_source_is_acoustic (source, raw))
    {
      g_warning ("HyScanDataWriter: incorrect acoustic source '%d'", source);
      return FALSE;
    }

  /* Название канала для записи данных. */
  channel_name = hyscan_channel_get_name_by_types (source, raw, channel);
  if (channel_name == NULL)
    {
      g_warning ("HyScanDataWriter: unknown source '%d'", source);
      return FALSE;
    }

  priv = writer->priv;

  g_mutex_lock (&priv->lock);

  /* Текущий галс. */
  if (priv->track_id < 0)
    goto exit;

  /* Ищем канал для записи данных или открываем новый. */
  channel_info = g_hash_table_lookup (priv->acoustic_channels, channel_name);
  if (channel_info == NULL)
    {
      HyScanAntennaPosition *position;
      HyScanDataWriterSignal *signal;
      HyScanDataWriterTVG *tvg;

      position = g_hash_table_lookup (priv->acoustic_positions, GINT_TO_POINTER (source));
      if (position == NULL)
        g_warning ("HyScanDataWriter: unspecified antenna position for channel %s", channel_name);

      channel_info = hyscan_data_writer_acoustic_channel_create (priv->db, priv->track_id,
                                                                 source, raw, channel,
                                                                 position, info);
      if (channel_info == NULL)
        goto exit;

      g_hash_table_insert (priv->acoustic_channels, g_strdup (channel_name), channel_info);

      if (priv->chunk_size > 0)
        hyscan_db_channel_set_chunk_size (priv->db, channel_info->channel_id, priv->chunk_size);
      if (priv->save_time > 0)
        hyscan_db_channel_set_save_time (priv->db, channel_info->channel_id, priv->save_time);
      if (priv->save_size > 0)
        hyscan_db_channel_set_save_size (priv->db, channel_info->channel_id, priv->save_size);

      /* Для "сырых" данных записываем текущий сигнал и параметры ВАРУ. */
      if (raw)
        {
          signal = g_hash_table_lookup (priv->signal, GINT_TO_POINTER (source));
          if (signal != NULL && signal->n_points > 0)
            {
              status = hyscan_db_channel_add_data (priv->db, channel_info->signal_id,
                                                   signal->time, signal->points,
                                                   signal->n_points * sizeof (HyScanComplexFloat),
                                                   NULL);

              if (!status)
                {
                  g_warning ("HyScanDataWriter: can't add signal in channel '%s'", channel_info->channel_name);
                  goto exit;
                }
            }

          tvg = g_hash_table_lookup (priv->tvg, GINT_TO_POINTER (source));
          if (tvg != NULL && tvg->n_gains > 0)
            {
              if (!hyscan_data_writer_channel_set_tvg_info (priv->db, channel_info->tvg_id, tvg->rate))
                {
                  g_warning ("HyScanDataWriter: can't set tvg parameters in channel '%s'", channel_info->channel_name);
                  channel_info->tvg_rate = -1.0;
                  goto exit;
                }

              status = hyscan_db_channel_add_data (priv->db, channel_info->tvg_id,
                                                   tvg->time, tvg->gains,
                                                   tvg->n_gains * sizeof (gfloat), NULL);

              if (!status)
                {
                  g_warning ("HyScanDataWriter: can't add tvg in channel '%s'", channel_info->channel_name);
                  goto exit;
                }
            }
        }
    }

  /* Проверяем тип данных и частоту дисретизации. */
  if ((channel_info->data_type != info->data.type) ||
      (channel_info->data_rate != info->data.rate))
    {
      goto exit;
    }

  /* Записываем данные. */
  if (hyscan_db_channel_add_data (priv->db, channel_info->channel_id, data->time, data->data, data->size, NULL))
    status = TRUE;

exit:
  g_mutex_unlock (&priv->lock);

  return status;
}

/* Функция устанавливает образ сигнала для свёртки для указанного источника данных. */
gboolean
hyscan_data_writer_acoustic_add_signal (HyScanDataWriter       *writer,
                                        HyScanSourceType        source,
                                        HyScanDataWriterSignal *signal)
{
  HyScanDataWriterPrivate *priv;
  HyScanDataWriterSignal *cur_signal;

  GHashTableIter iter;
  gpointer data;

  g_return_val_if_fail (HYSCAN_IS_DATA_WRITER (writer), FALSE);

  priv = writer->priv;

  if (priv->db == NULL)
    return FALSE;

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
  g_hash_table_iter_init (&iter, priv->acoustic_channels);
  while (g_hash_table_iter_next (&iter, NULL, &data))
    {
      HyScanDataWriterAcousticChannel *channel_info = data;
      HyScanComplexFloat zero = {0.0, 0.0};
      gconstpointer points;
      gint32 size;

      /* Проверяем тип источника. */
      if (channel_info->raw_source != source)
        continue;

      /* Частота дискретизации должна совпадать. */
      if (channel_info->data_rate == cur_signal->rate)
        {
          gboolean status;

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
            g_warning ("HyScanDataWriter: can't add signal in channel '%s'", channel_info->channel_name);
        }
      else
        {
          g_warning ("HyScanDataWriter: signal rate mismatch in channel '%s'", channel_info->channel_name);
        }
    }

  g_mutex_unlock (&priv->lock);

  return TRUE;
}

/* Функция устанавливает значения параметров усиления ВАРУ для указанного источника данных. */
gboolean
hyscan_data_writer_acoustic_add_tvg (HyScanDataWriter     *writer,
                                     HyScanSourceType      source,
                                     HyScanDataWriterTVG  *tvg)
{
  HyScanDataWriterPrivate *priv;
  HyScanDataWriterTVG *cur_tvg;

  GHashTableIter iter;
  gpointer data;

  g_return_val_if_fail (HYSCAN_IS_DATA_WRITER (writer), FALSE);

  priv = writer->priv;

  if (priv->db == NULL)
    return FALSE;

  g_mutex_lock (&priv->lock);

  /* Ищем текущие параметры ВАРУ или создаём новую запись. */
  cur_tvg = g_hash_table_lookup (priv->tvg, GINT_TO_POINTER (source));
  if (cur_tvg == NULL)
    {
      cur_tvg = g_new0 (HyScanDataWriterTVG, 1);
      g_hash_table_insert (priv->tvg, GINT_TO_POINTER (source), cur_tvg);
    }

  /* Освобождаем память занятую предыдущими параметрами ВАРУ. */
  if (cur_tvg->gains != NULL)
    g_free ((gpointer)cur_tvg->gains);

  /* Сохраняем текущие параметры ВАРУ. */
  cur_tvg->time = tvg->time;
  cur_tvg->rate = tvg->rate;
  cur_tvg->n_gains = tvg->n_gains;
  cur_tvg->gains = g_memdup (tvg->gains, tvg->n_gains * sizeof (gfloat));

  /* Записываем параметры ВАРУ в каналы с "сырыми" данными от текущего источника. */
  g_hash_table_iter_init (&iter, priv->acoustic_channels);
  while (g_hash_table_iter_next (&iter, NULL, &data))
    {
      HyScanDataWriterAcousticChannel *channel_info = data;

      /* Проверяем тип источника. */
      if (channel_info->raw_source != source)
        continue;

      /* Частота дискретизации должна совпадать. */
      if (channel_info->tvg_rate == 0.0 ||
          channel_info->tvg_rate == cur_tvg->rate)
        {
          gboolean status;

          /* Первая запись параметров ВАРУ - устанавливаем значения параметров дискретизации. */
          if (channel_info->tvg_rate == 0.0)
            if (!hyscan_data_writer_channel_set_tvg_info (priv->db, channel_info->tvg_id, cur_tvg->rate))
              {
                channel_info->tvg_rate = -1.0;
                continue;
              }

          status = hyscan_db_channel_add_data (priv->db, channel_info->tvg_id,
                                               cur_tvg->time, cur_tvg->gains,
                                               cur_tvg->n_gains * sizeof (gfloat), NULL);

          if (!status)
            g_warning ("HyScanDataWriter: can't add tvg in channel '%s'", channel_info->channel_name);
        }
      else
        {
          g_warning ("HyScanDataWriter: tvg rate mismatch in channel '%s'", channel_info->channel_name);
        }
    }

  g_mutex_unlock (&priv->lock);

  return TRUE;
}
