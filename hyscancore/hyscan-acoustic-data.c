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

#include <string.h>

enum
{
  PROP_O,
  PROP_DB,
  PROP_PROJECT_NAME,
  PROP_TRACK_NAME,
  PROP_SOURCE_TYPE,
  PROP_RAW
};

struct _HyScanAcousticDataPrivate
{
  HyScanDB            *db;                                     /* Интерфейс базы данных. */
  gchar               *db_uri;                                 /* URI базы данных. */

  gchar               *project_name;                           /* Название проекта. */
  gchar               *track_name;                             /* Название галса. */
  HyScanSourceType     source_type;                            /* Тип источника данных. */
  gboolean             raw;                                    /* Использовать сырые данные. */

  HyScanRawData       *raw_data;                               /* Обработчик сырых данных. */

  HyScanCache         *cache;                                  /* Интерфейс системы кэширования. */
  gchar               *cache_prefix;                           /* Префикс ключа кэширования. */
  gchar               *cache_key;                              /* Ключ кэширования. */
  gsize                cache_key_length;                       /* Максимальная длина ключа. */

  HyScanAntennaPosition  position;                             /* Местоположение приёмной антенны. */
  HyScanAcousticDataInfo info;                                 /* Параметры сырых данных. */

  gint32               channel_id;                             /* Идентификатор открытого канала данных. */

  GArray              *raw_buffer;                             /* Буфер для чтения данных канала. */
  GArray              *amp_buffer;                             /* Буфер для амплитуд данных канала. */
};

static void    hyscan_acoustic_data_set_property       (GObject                       *object,
                                                        guint                          prop_id,
                                                        const GValue                  *value,
                                                        GParamSpec                    *pspec);
static void    hyscan_acoustic_data_object_constructed (GObject                       *object);
static void    hyscan_acoustic_data_object_finalize    (GObject                       *object);

static void    hyscan_acoustic_data_buffer_realloc     (HyScanAcousticDataPrivate     *priv,
                                                        guint32                        max_size);

static void    hyscan_acoustic_data_update_cache_key   (HyScanAcousticDataPrivate     *priv,
                                                        guint32                        index);

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

  priv->raw_buffer = g_array_new (FALSE, FALSE, sizeof (guint8));
  priv->amp_buffer = g_array_new (FALSE, FALSE, sizeof (gfloat));

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

  g_array_unref (priv->raw_buffer);
  g_array_unref (priv->amp_buffer);

  g_clear_object (&priv->db);
  g_clear_object (&priv->cache);
  g_clear_object (&priv->raw_data);

  G_OBJECT_CLASS (hyscan_acoustic_data_parent_class)->finalize (object);
}

/* Функция проверяет размер буферов и увеличивает его при необходимости. */
static void hyscan_acoustic_data_buffer_realloc (HyScanAcousticDataPrivate *priv,
                                                 guint32                    max_size)
{
  guint32 point_size;

  if (max_size + 16 <= priv->raw_buffer->len)
    return;

  point_size = hyscan_data_get_point_size (priv->info.data_type);

  g_array_set_size (priv->raw_buffer, max_size + 16);
  g_array_set_size (priv->amp_buffer, max_size / point_size);
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
  gint64 cached_time;
  guint32 io_size;
  gfloat *amp;

  gboolean status;

  g_return_val_if_fail (HYSCAN_IS_ACOUSTIC_DATA (data), NULL);

  priv = data->priv;

  if ((priv->channel_id <= 0) && (priv->raw_data == NULL))
    return NULL;

  if (priv->raw_data != NULL)
    return hyscan_raw_data_get_amplitude_values (priv->raw_data, index, n_points, time);

  /* Ищем данные в кэше. */
  if (priv->cache != NULL)
    {
      /* Ключ кэширования. */
      hyscan_acoustic_data_update_cache_key (priv, index);

      /* Определяем размер данных в кэше, если они есть. */
      if (hyscan_cache_get (priv->cache, priv->cache_key, NULL, NULL, &io_size))
        {
          guint32 time_size = sizeof (cached_time);

          /* При необходимости корректируем размер буферов. */
          io_size -= time_size;
          hyscan_acoustic_data_buffer_realloc (priv, io_size);

          /* Считываем данные из кэша. */
          status = hyscan_cache_get2 (priv->cache, priv->cache_key, NULL,
                                      &cached_time, &time_size,
                                      priv->amp_buffer->data, &io_size);
          if ((status) &&
              (time_size == sizeof (cached_time)) &&
              ((io_size % sizeof (gfloat)) == 0))
            {
              (time != NULL) ? *time = cached_time : 0;
              *n_points = io_size / sizeof (gfloat);
              return (gfloat*)priv->amp_buffer->data;
            }
        }
    }

  /* Считываем данные канала. */
  io_size = priv->raw_buffer->len;
  status = hyscan_db_channel_get_data (priv->db, priv->channel_id, index,
                                       priv->raw_buffer->data, &io_size, &cached_time);
  if (!status)
    return NULL;

  /* Если размер считанных данных равен размеру буфера, запрашиваем реальный размер,
     увеличиваем размер буфера и считываем данные еще раз. */
  if ((priv->raw_buffer->len == 0) || (priv->raw_buffer->len == io_size))
    {
      status = hyscan_db_channel_get_data (priv->db, priv->channel_id, index,
                                           NULL, &io_size, NULL);
      if (!status)
        return NULL;

      hyscan_acoustic_data_buffer_realloc (priv, io_size);

      io_size = priv->raw_buffer->len;
      status = hyscan_db_channel_get_data (priv->db, priv->channel_id, index,
                                           priv->raw_buffer->data, &io_size, &cached_time);
      if (!status)
        return NULL;
    }

  /* Если данные уже типа FLOAT, возвращаем их как есть. */
  if (priv->info.data_type == HYSCAN_DATA_FLOAT)
    {
      *n_points = io_size / sizeof (gfloat);

      amp = (gfloat*)priv->raw_buffer->data;
    }
  else
    {
      *n_points = priv->amp_buffer->len;

      status = hyscan_data_import_float (priv->info.data_type,
                                         priv->raw_buffer->data, io_size,
                                         (gfloat*)priv->amp_buffer->data, n_points);

      if (!status)
        return NULL;

      amp = (gfloat*)priv->amp_buffer->data;
    }

  /* Сохраняем данные в кэше. */
  if (priv->cache != NULL)
    {
      hyscan_cache_set2 (priv->cache, priv->cache_key, NULL,
                         &cached_time, sizeof (cached_time),
                         amp, *n_points * sizeof (gfloat));
    }

  (time != NULL) ? *time = cached_time : 0;

  return amp;
}
