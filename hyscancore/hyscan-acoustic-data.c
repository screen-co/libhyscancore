/*
 * \file hyscan-acoustic-data.c
 *
 * \brief Исходный файл класса обработки акустических данных
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-acoustic-data.h"
#include "hyscan-core-params.h"
#include "hyscan-raw-data.h"
#include <hyscan-buffer.h>

#include <string.h>

#define CACHE_HEADER_MAGIC     0xe6da3411                      /* Идентификатор заголовка кэша. */

enum
{
  PROP_O,
  PROP_DB,
  PROP_PROJECT_NAME,
  PROP_TRACK_NAME,
  PROP_SOURCE_TYPE,
  PROP_RAW
};

/* Структруа заголовка кэша. */
typedef struct
{
  guint32                      magic;                          /* Идентификатор заголовка. */
  guint32                      n_points;                       /* Число точек данных. */
  gint64                       time;                           /* Метка времени. */
} HyScanAcousticDataCacheHeader;

struct _HyScanAcousticDataPrivate
{
  HyScanDB                    *db;                             /* Интерфейс базы данных. */
  gchar                       *db_uri;                         /* URI базы данных. */

  gchar                       *project_name;                   /* Название проекта. */
  gchar                       *track_name;                     /* Название галса. */
  HyScanSourceType             source_type;                    /* Тип источника данных. */
  gboolean                     raw;                            /* Использовать сырые данные. */

  HyScanRawData               *raw_data;                       /* Обработчик сырых данных. */

  HyScanCache                 *cache;                          /* Интерфейс системы кэширования. */
  gchar                       *cache_prefix;                   /* Префикс ключа кэширования. */
  gchar                       *cache_key;                      /* Ключ кэширования. */
  gsize                        cache_key_length;               /* Максимальная длина ключа. */
  HyScanBuffer                *cache_buffer;                   /* Буфер заголовка кэша данных. */

  HyScanAntennaPosition        position;                       /* Местоположение приёмной антенны. */
  HyScanAcousticDataInfo       info;                           /* Параметры акустических данных. */

  gint32                       channel_id;                     /* Идентификатор открытого канала данных. */

  HyScanBuffer                *channel_buffer;                 /* Буфер канальных данных. */
  HyScanBuffer                *amp_buffer;                     /* Буфер амплитудных данных. */
};

static void        hyscan_acoustic_data_set_property           (GObject                       *object,
                                                                guint                          prop_id,
                                                                const GValue                  *value,
                                                                GParamSpec                    *pspec);
static void        hyscan_acoustic_data_object_constructed     (GObject                       *object);
static void        hyscan_acoustic_data_object_finalize        (GObject                       *object);

static void        hyscan_acoustic_data_update_cache_key       (HyScanAcousticDataPrivate     *priv,
                                                                guint32                        index);

static gboolean    hyscan_acoustic_data_check_cache            (HyScanAcousticDataPrivate     *priv,
                                                                guint32                        index,
                                                                gint64                        *time);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanAcousticData, hyscan_acoustic_data, G_TYPE_OBJECT)

static void
hyscan_acoustic_data_class_init (HyScanAcousticDataClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_acoustic_data_set_property;

  object_class->constructed = hyscan_acoustic_data_object_constructed;
  object_class->finalize = hyscan_acoustic_data_object_finalize;

  g_object_class_install_property (object_class, PROP_DB,
    g_param_spec_object ("db", "DB", "HyScanDB interface", HYSCAN_TYPE_DB,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_PROJECT_NAME,
    g_param_spec_string ("project-name", "ProjectName", "Project name", NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_TRACK_NAME,
    g_param_spec_string ("track-name", "TrackName", "Track name", NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_SOURCE_TYPE,
    g_param_spec_int ("source-type", "SourceType", "Source type", 0, G_MAXINT, 0,
                      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_RAW,
    g_param_spec_boolean ("raw", "Raw", "Use raw data type", FALSE,
                          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_acoustic_data_init (HyScanAcousticData *data)
{
  data->priv = hyscan_acoustic_data_get_instance_private (data);
}

static void
hyscan_acoustic_data_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  HyScanAcousticData *data = HYSCAN_ACOUSTIC_DATA (object);
  HyScanAcousticDataPrivate *priv = data->priv;

  switch (prop_id)
    {
    case PROP_DB:
      priv->db = g_value_dup_object (value);
      break;

    case PROP_PROJECT_NAME:
      priv->project_name = g_value_dup_string (value);
      break;

    case PROP_TRACK_NAME:
      priv->track_name = g_value_dup_string (value);
      break;

    case PROP_SOURCE_TYPE:
      priv->source_type = g_value_get_int (value);
      break;

    case PROP_RAW:
      priv->raw = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_acoustic_data_object_constructed (GObject *object)
{
  HyScanAcousticData *data = HYSCAN_ACOUSTIC_DATA (object);
  HyScanAcousticDataPrivate *priv = data->priv;

  gint32 project_id = -1;
  gint32 track_id = -1;
  gint32 param_id = -1;

  const gchar *channel_name;

  gboolean status = FALSE;

  priv->cache_prefix = g_strdup ("none");

  priv->channel_id = -1;

  priv->cache_buffer = hyscan_buffer_new ();
  priv->channel_buffer = hyscan_buffer_new ();
  priv->amp_buffer = hyscan_buffer_new ();

  /* Использовать сырые данные. */
  if (priv->raw)
    {
      HyScanRawDataInfo raw_info;

      priv->raw_data = hyscan_raw_data_new (priv->db, priv->project_name, priv->track_name,
                                            priv->source_type, 1);
      if (priv->raw_data == NULL)
        return;

      priv->position = hyscan_raw_data_get_position (priv->raw_data);

      raw_info = hyscan_raw_data_get_info (priv->raw_data);
      priv->info.data_type = raw_info.data_type;
      priv->info.data_rate = raw_info.data_rate;
      priv->info.antenna_vpattern = raw_info.antenna_vpattern;
      priv->info.antenna_hpattern = raw_info.antenna_hpattern;

      return;
    }

  /* Проверяем тип источника данных. */
  if (!hyscan_source_is_raw (priv->source_type))
    {
      g_warning ("HyScanAcousticData: unsupported source type %s",
                 hyscan_channel_get_name_by_types (priv->source_type, FALSE, 1));
      goto exit;
    }

  if (priv->db == NULL)
    {
      g_warning ("HyScanAcousticData: db is not specified");
      goto exit;
    }

  priv->db_uri = hyscan_db_get_uri (priv->db);

  /* Названия проекта, галса, канала данных. */
  channel_name = hyscan_channel_get_name_by_types (priv->source_type, FALSE, 1);
  if ((priv->project_name == NULL) || (priv->track_name == NULL) || (channel_name == NULL))
    {
      if (priv->project_name == NULL)
        g_warning ("HyScanAcousticData: unknown project name");
      if (priv->track_name == NULL)
        g_warning ("HyScanAcousticData: unknown track name");
      if (channel_name == NULL)
        g_warning ("HyScanAcousticData: unknown channel name");
      goto exit;
    }

  project_id = hyscan_db_project_open (priv->db, priv->project_name);
  if (project_id < 0)
    {
      g_info ("HyScanAcousticData: can't open project '%s'", priv->project_name);
      goto exit;
    }

  track_id = hyscan_db_track_open (priv->db, project_id, priv->track_name);
  if (track_id < 0)
    {
      g_info ("HyScanAcousticData: can't open track '%s.%s'", priv->project_name, priv->track_name);
      goto exit;
    }

  /* Открываем канал данных. */
  priv->channel_id = hyscan_db_channel_open (priv->db, track_id, channel_name);
  if (priv->channel_id < 0)
    {
      g_info ("HyScanAcousticData: can't open channel '%s.%s.%s'",
              priv->project_name, priv->track_name, channel_name);
      goto exit;
    }

  /* Проверяем, что в канале есть хотя бы одна запись. */
  if (!hyscan_db_channel_get_data_range (priv->db, priv->channel_id, NULL, NULL))
    goto exit;

  /* Параметры канала данных. */
  param_id = hyscan_db_channel_param_open (priv->db, priv->channel_id);
  if (param_id < 0)
    {
      g_warning ("HyScanAcousticData: '%s.%s.%s': can't open parameters",
                 priv->project_name, priv->track_name, channel_name);
      goto exit;
    }

  status = hyscan_core_params_load_antenna_position (priv->db, param_id,
                                                     ACOUSTIC_CHANNEL_SCHEMA_ID,
                                                     ACOUSTIC_CHANNEL_SCHEMA_VERSION,
                                                     &priv->position);
  if (!status)
    {
      g_warning ("HyScanAcousticData: '%s.%s.%s': can't read antenna position",
                 priv->project_name, priv->track_name, channel_name);
      goto exit;
    }

  if (!hyscan_core_params_load_acoustic_data_info (priv->db, param_id, &priv->info))
    {
      g_warning ("HyScanAcousticData: '%s.%s.%s': can't read parameters",
                 priv->project_name, priv->track_name, channel_name);
      goto exit;
    }

  hyscan_db_close (priv->db, param_id);
  param_id = -1;

  /* Тип данных в буфере. */
  hyscan_buffer_set_data_type (priv->channel_buffer, priv->info.data_type);

  status = TRUE;

exit:
  if (!status)
    {
      if (priv->channel_id > 0)
        hyscan_db_close (priv->db, priv->channel_id);
      priv->channel_id = -1;
    }

  if (project_id > 0)
    hyscan_db_close (priv->db, project_id);
  if (track_id > 0)
    hyscan_db_close (priv->db, track_id);
  if (param_id > 0)
    hyscan_db_close (priv->db, param_id);
}

static void
hyscan_acoustic_data_object_finalize (GObject *object)
{
  HyScanAcousticData *data = HYSCAN_ACOUSTIC_DATA (object);
  HyScanAcousticDataPrivate *priv = data->priv;

  if (priv->channel_id > 0)
    hyscan_db_close (priv->db, priv->channel_id);

  g_free (priv->project_name);
  g_free (priv->track_name);

  g_free (priv->db_uri);
  g_free (priv->cache_key);
  g_free (priv->cache_prefix);

  g_object_unref (priv->cache_buffer);
  g_object_unref (priv->channel_buffer);
  g_object_unref (priv->amp_buffer);

  g_clear_object (&priv->db);
  g_clear_object (&priv->cache);
  g_clear_object (&priv->raw_data);

  G_OBJECT_CLASS (hyscan_acoustic_data_parent_class)->finalize (object);
}

/* Функция создаёт ключ кэширования данных. */
static void
hyscan_acoustic_data_update_cache_key (HyScanAcousticDataPrivate *priv,
                                       guint32                    index)
{
  if (priv->cache_key == NULL)
    {
      priv->cache_key = g_strdup_printf ("%s.%s.%s.%s.ACOUSTIC.%d.%u",
                                         priv->cache_prefix,
                                         priv->db_uri,
                                         priv->project_name,
                                         priv->track_name,
                                         priv->source_type,
                                         G_MAXUINT32);

      priv->cache_key_length = strlen (priv->cache_key);
    }

  g_snprintf (priv->cache_key, priv->cache_key_length, "%s.%s.%s.%s.ACOUSTIC.%d.%u",
              priv->cache_prefix,
              priv->db_uri,
              priv->project_name,
              priv->track_name,
              priv->source_type,
              index);
}

/* Функция проверяет кэш на наличие акустических данных и
 * если такие есть, считывает их. */
static gboolean
hyscan_acoustic_data_check_cache (HyScanAcousticDataPrivate *priv,
                                  guint32                    index,
                                  gint64                    *time)
{
  HyScanAcousticDataCacheHeader header;
  guint32 cached_n_points;

  if (priv->cache == NULL)
    return FALSE;

  /* Ключ кэширования. */
  hyscan_acoustic_data_update_cache_key (priv, index);

  /* Ищем данные в кэше. */
  hyscan_buffer_wrap_data (priv->cache_buffer, HYSCAN_DATA_BLOB, &header, sizeof (header));
  if (!hyscan_cache_get2 (priv->cache, priv->cache_key, NULL, sizeof (header), priv->cache_buffer, priv->amp_buffer))
    return FALSE;

  /* Верификация данных. */
  cached_n_points  = hyscan_buffer_get_size (priv->amp_buffer);
  cached_n_points /= sizeof (gfloat);
  if ((header.magic != CACHE_HEADER_MAGIC) ||
      (header.n_points != cached_n_points))
    {
      return FALSE;
    }

  (time != NULL) ? *time = header.time : 0;

  return TRUE;
}

/* Функция создаёт новый объект обработки акустических данных. */
HyScanAcousticData *
hyscan_acoustic_data_new (HyScanDB         *db,
                          const gchar      *project_name,
                          const gchar      *track_name,
                          HyScanSourceType  source_type,
                          gboolean          raw)
{
  HyScanAcousticData *data;

  data = g_object_new (HYSCAN_TYPE_ACOUSTIC_DATA,
                       "db", db,
                       "project-name", project_name,
                       "track-name", track_name,
                       "source-type", source_type,
                       "raw", raw,
                       NULL);

  if ((data->priv->raw_data == NULL) && (data->priv->channel_id <= 0))
    g_clear_object (&data);

  return data;
}

/* Функция задаёт используемый кэш. */
void
hyscan_acoustic_data_set_cache (HyScanAcousticData *data,
                                HyScanCache        *cache,
                                const gchar        *prefix)

{
  HyScanAcousticDataPrivate *priv;

  g_return_if_fail (HYSCAN_IS_ACOUSTIC_DATA (data));

  priv = data->priv;

  if ((priv->channel_id <= 0) && (priv->raw_data == NULL))
    return;

  if (priv->raw)
    {
      hyscan_raw_data_set_cache (priv->raw_data, cache, prefix);
      return;
    }

  g_clear_object (&priv->cache);
  g_clear_pointer (&priv->cache_prefix, g_free);
  g_clear_pointer (&priv->cache_key, g_free);

  if (cache == NULL)
    return;

  if (prefix == NULL)
    priv->cache_prefix = g_strdup ("none");
  else
    priv->cache_prefix = g_strdup (prefix);

  priv->cache = g_object_ref (cache);
}

/* Функция возвращает информацию о местоположении приёмной антенны гидролокатора. */
HyScanAntennaPosition
hyscan_acoustic_data_get_position (HyScanAcousticData *data)
{
  HyScanAcousticDataPrivate *priv;
  HyScanAntennaPosition zero;

  memset (&zero, 0, sizeof (HyScanAntennaPosition));

  g_return_val_if_fail (HYSCAN_IS_ACOUSTIC_DATA (data), zero);

  priv = data->priv;

  if ((priv->channel_id <= 0) && (priv->raw_data == NULL))
    return zero;

  return priv->position;
}

/* Функция возвращает параметры канала акустических данных. */
HyScanAcousticDataInfo
hyscan_acoustic_data_get_info (HyScanAcousticData *data)
{
  HyScanAcousticDataPrivate *priv;
  HyScanAcousticDataInfo zero;

  memset (&zero, 0, sizeof (HyScanAcousticDataInfo));

  g_return_val_if_fail (HYSCAN_IS_ACOUSTIC_DATA (data), zero);

  priv = data->priv;

  if ((priv->channel_id <= 0) && (priv->raw_data == NULL))
    return zero;

  return priv->info;
}

/* Функция возвращает тип источника данных. */
HyScanSourceType
hyscan_acoustic_data_get_source (HyScanAcousticData *data)
{
  g_return_val_if_fail (HYSCAN_IS_ACOUSTIC_DATA (data), HYSCAN_SOURCE_INVALID);

  return data->priv->source_type;
}

/* Функция определяет возможность изменения данных. */
gboolean
hyscan_acoustic_data_is_writable (HyScanAcousticData *data)
{
  HyScanAcousticDataPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_ACOUSTIC_DATA (data), FALSE);

  priv = data->priv;

  if ((priv->channel_id <= 0) && (priv->raw_data == NULL))
    return FALSE;

  if (priv->raw_data != NULL)
    return hyscan_raw_data_is_writable (priv->raw_data);

  return hyscan_db_channel_is_writable (priv->db, priv->channel_id);
}

/* Функция возвращает диапазон значений индексов записанных данных. */
gboolean
hyscan_acoustic_data_get_range (HyScanAcousticData *data,
                                guint32            *first_index,
                                guint32            *last_index)
{
  HyScanAcousticDataPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_ACOUSTIC_DATA (data), FALSE);

  priv = data->priv;

  if ((priv->channel_id <= 0) && (priv->raw_data == NULL))
    return FALSE;

  if (priv->raw_data != NULL)
    return hyscan_raw_data_get_range (priv->raw_data, first_index, last_index);

  return hyscan_db_channel_get_data_range (priv->db, priv->channel_id, first_index, last_index);
}

/* Функция возвращает номер изменения в данных. */
guint32
hyscan_acoustic_data_get_mod_count (HyScanAcousticData *data)
{
  HyScanAcousticDataPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_ACOUSTIC_DATA (data), 0);

  priv = data->priv;

  if ((priv->channel_id <= 0) && (priv->raw_data == NULL))
    return 0;

  if (priv->raw_data != NULL)
    return hyscan_raw_data_get_mod_count (priv->raw_data);

  return hyscan_db_get_mod_count (priv->db, priv->channel_id);
}

/* Функция ищет индекс данных для указанного момента времени. */
HyScanDBFindStatus
hyscan_acoustic_data_find_data (HyScanAcousticData *data,
                                gint64              time,
                                guint32            *lindex,
                                guint32            *rindex,
                                gint64             *ltime,
                                gint64             *rtime)
{
  HyScanAcousticDataPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_ACOUSTIC_DATA (data), HYSCAN_DB_FIND_FAIL);

  priv = data->priv;

  if ((priv->channel_id <= 0) && (priv->raw_data == NULL))
    return FALSE;

  if (priv->raw_data != NULL)
    return hyscan_raw_data_find_data (priv->raw_data, time, lindex, rindex, ltime, rtime);

  return hyscan_db_channel_find_data (priv->db, priv->channel_id, time, lindex, rindex, ltime, rtime);
}

/* Функция возвращает значения амплитуды сигнала. */
const gfloat *
hyscan_acoustic_data_get_values (HyScanAcousticData *data,
                                 guint32             index,
                                 guint32            *n_points,
                                 gint64             *time)
{
  HyScanAcousticDataPrivate *priv;

  gint64 data_time;

  g_return_val_if_fail (HYSCAN_IS_ACOUSTIC_DATA (data), NULL);

  priv = data->priv;

  if ((priv->channel_id <= 0) && (priv->raw_data == NULL))
    return NULL;

  if (priv->raw_data != NULL)
    return hyscan_raw_data_get_amplitude_values (priv->raw_data, index, n_points, time);

  /* Проверяем наличие данных в кэше. */
  if (hyscan_acoustic_data_check_cache (priv, index, time))
    return hyscan_buffer_get_float (priv->amp_buffer, n_points);

  /* Считываем данные канала. */
  if (!hyscan_db_channel_get_data (priv->db, priv->channel_id, index, priv->channel_buffer, &data_time))
    return NULL;

  /* Импортируем данные в буфер обработки. */
  if (!hyscan_buffer_import_data (priv->amp_buffer, priv->channel_buffer))
    return FALSE;

  /* Сохраняем данные в кэше. */
  if (priv->cache != NULL)
    {
      HyScanAcousticDataCacheHeader header;

      header.magic = CACHE_HEADER_MAGIC;
      header.n_points = hyscan_buffer_get_size (priv->amp_buffer);
      header.n_points /= sizeof (gfloat);
      header.time = data_time;
      hyscan_buffer_wrap_data (priv->cache_buffer, HYSCAN_DATA_BLOB, &header, sizeof (header));

      hyscan_cache_set2 (priv->cache, priv->cache_key, NULL, priv->cache_buffer, priv->amp_buffer);
    }

  (time != NULL) ? *time = data_time : 0;

  return hyscan_buffer_get_float (priv->amp_buffer, n_points);
}
