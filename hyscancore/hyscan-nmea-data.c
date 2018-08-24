/*
 * \file hyscan-nmea-data.c
 *
 * \brief Исходный файл класса работы с NMEA-строками.
 * \author Dmitriev Alexander (m1n7@yandex.ru)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-nmea-data.h"
#include "hyscan-core-schemas.h"
#include "hyscan-core-common.h"
#include <string.h>

#define CACHE_HEADER_MAGIC     0x3f0a4b87          /* Идентификатор заголовка кэша. */

enum
{
  PROP_O,
  PROP_DB,
  PROP_PROJECT_NAME,
  PROP_TRACK_NAME,
  PROP_SOURCE_TYPE,
  PROP_SOURCE_CHANNEL,
};

/* Структруа заголовка кэша. */
typedef struct
{
  guint32               magic;                     /* Идентификатор заголовка. */
  guint32               length;                    /* Длина NMEA строки. */
  gint64                time;                      /* Метка времени. */
} HyScanNMEADataCacheHeader;

struct _HyScanNMEADataPrivate
{
  HyScanDB             *db;                        /* Интерфейс базы данных. */

  gchar                *project;                   /* Название проекта. */
  gchar                *track;                     /* Название галса. */
  HyScanSourceType      source_type;               /* Тип источника данных. */
  guint                 source_channel;            /* Индекс канала данных. */

  HyScanCache          *cache;                     /* Интерфейс системы кэширования. */
  gchar                *path;                      /* Путь к БД, проекту, галсу и каналу. */
  HyScanBuffer         *cache_buffer;              /* Буфер заголовка кэша данных. */

  gchar                *key;                       /* Ключ кэширования. */
  gsize                 key_length;                /* Максимальная длина ключа. */

  HyScanAntennaPosition position;                  /* Местоположение приёмной антенны. */
  gint32                channel_id;                /* Идентификатор открытого канала данных. */

  HyScanBuffer         *nmea_buffer;               /* Буфер данных. */

};

static void      hyscan_nmea_data_set_property          (GObject               *object,
                                                         guint                  prop_id,
                                                         const GValue          *value,
                                                         GParamSpec            *pspec);
static void      hyscan_nmea_data_object_constructed    (GObject               *object);
static void      hyscan_nmea_data_object_finalize       (GObject               *object);

static void      hyscan_nmea_data_update_cache_key      (HyScanNMEADataPrivate *priv,
                                                         guint32                index);
static gboolean  hyscan_nmea_data_check_cache           (HyScanNMEADataPrivate *priv,
                                                         guint32                index,
                                                         gint64                *time);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanNMEAData, hyscan_nmea_data, G_TYPE_OBJECT);

static void
hyscan_nmea_data_class_init (HyScanNMEADataClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_nmea_data_set_property;

  object_class->constructed = hyscan_nmea_data_object_constructed;
  object_class->finalize = hyscan_nmea_data_object_finalize;

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

  g_object_class_install_property (object_class, PROP_SOURCE_CHANNEL,
    g_param_spec_uint ("source-channel", "SourceChannel", "Source channel", 1, 5, 1,
                       G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_nmea_data_init (HyScanNMEAData *data)
{
  data->priv = hyscan_nmea_data_get_instance_private (data);
}

static void
hyscan_nmea_data_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  HyScanNMEAData *data = HYSCAN_NMEA_DATA (object);
  HyScanNMEADataPrivate *priv = data->priv;

  switch (prop_id)
    {
    case PROP_DB:
      priv->db = g_value_dup_object (value);
      break;

    case PROP_PROJECT_NAME:
      priv->project = g_value_dup_string (value);
      break;

    case PROP_TRACK_NAME:
      priv->track = g_value_dup_string (value);
      break;

    case PROP_SOURCE_TYPE:
      priv->source_type = g_value_get_int (value);
      break;

    case PROP_SOURCE_CHANNEL:
      priv->source_channel = g_value_get_uint (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_nmea_data_object_constructed (GObject *object)
{
  HyScanNMEAData *data = HYSCAN_NMEA_DATA (object);
  HyScanNMEADataPrivate *priv = data->priv;

  const gchar *channel_name;

  gint32 project_id = -1;
  gint32 track_id = -1;
  gint32 param_id = -1;

  gchar *db_uri = NULL;
  gboolean status = FALSE;

  priv->channel_id = -1;

  priv->cache_buffer = hyscan_buffer_new ();
  priv->nmea_buffer = hyscan_buffer_new ();

  channel_name = hyscan_core_get_channel_name (priv->source_type,
                                               priv->source_channel,
                                               HYSCAN_CHANNEL_DATA);

  /* Проверяем БД, проект, галс и название канала. */
  if ((priv->db == NULL) || (priv->project == NULL) ||
      (priv->track == NULL) || (channel_name == NULL))
    {
      if (priv->db == NULL)
        g_warning ("HyScanNMEAData: db is not specified");
      if (priv->project == NULL)
        g_warning ("HyScanNMEAData: project name is not specified");
      if (priv->track == NULL)
        g_warning ("HyScanNMEAData: track name is not specified");
      if (channel_name == NULL)
        g_warning ("HyScanNMEAData: unknown channel name");
      goto exit;
    }

  if (!hyscan_source_is_sensor (priv->source_type))
    {
      g_warning ("HyScanNMEAData: unsupported source type %s", channel_name);
      goto exit;
    }

  /* Путь к БД. */
  db_uri = hyscan_db_get_uri (priv->db);

  priv->path = g_strdup_printf ("%s.%s.%s.%s", db_uri, priv->project, priv->track, channel_name);

  project_id = hyscan_db_project_open (priv->db, priv->project);
  if (project_id < 0)
    {
      g_warning ("HyScanNMEAData: can't open project '%s'", priv->project);
      goto exit;
    }

  track_id = hyscan_db_track_open (priv->db, project_id, priv->track);
  if (track_id < 0)
    {
      g_warning ("HyScanNMEAData: can't open track '%s.%s'", priv->project, priv->track);
      goto exit;
    }

  priv->channel_id = hyscan_db_channel_open (priv->db, track_id, channel_name);
  if (priv->channel_id < 0)
    {
      g_warning ("HyScanNMEAData: can't open channel '%s.%s.%s'",
                 priv->project, priv->track, channel_name);
      goto exit;
    }

  /* Проверяем, что в канале есть хотя бы одна запись. */
  if (!hyscan_db_channel_get_data_range (priv->db, priv->channel_id, NULL, NULL))
    goto exit;

  /* Параметры канала данных. */
  param_id = hyscan_db_channel_param_open (priv->db, priv->channel_id);
  if (param_id < 0)
    {
      g_warning ("HyScanNMEAData: '%s.%s.%s': can't open parameters",
                 priv->project, priv->track, channel_name);
      goto exit;
    }

  status = hyscan_core_params_load_antenna_position (priv->db, param_id,
                                                     SENSOR_CHANNEL_SCHEMA_ID,
                                                     SENSOR_CHANNEL_SCHEMA_VERSION,
                                                     &priv->position);
  if (!status)
    {
      g_warning ("HyScanNMEAData: '%s.%s.%s': can't read antenna position",
                 priv->project, priv->track, channel_name);
      goto exit;
    }

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

  g_free (db_uri);
}

static void
hyscan_nmea_data_object_finalize (GObject *object)
{
  HyScanNMEAData *data = HYSCAN_NMEA_DATA (object);
  HyScanNMEADataPrivate *priv = data->priv;

  if (priv->channel_id > 0)
    hyscan_db_close (priv->db, priv->channel_id);

  g_free (priv->project);
  g_free (priv->track);
  g_free (priv->path);
  g_free (priv->key);

  g_object_unref (priv->cache_buffer);
  g_object_unref (priv->nmea_buffer);

  g_clear_object (&priv->db);
  g_clear_object (&priv->cache);

  G_OBJECT_CLASS (hyscan_nmea_data_parent_class)->finalize (object);
}

/* Функция обновляет ключ кэширования. */
static void
hyscan_nmea_data_update_cache_key (HyScanNMEADataPrivate *priv,
                                   guint32                index)
{
  if (priv->key == NULL)
    {
      priv->key = g_strdup_printf ("NMEA.%s.%u",
                                    priv->path,
                                    G_MAXUINT32);

      priv->key_length = strlen (priv->key);
    }

  g_snprintf (priv->key, priv->key_length,
              "NMEA.%s.%u",
              priv->path,
              index);
}

/* Функция проверяет кэш на наличие данных и считывает их. */
static gboolean
hyscan_nmea_data_check_cache (HyScanNMEADataPrivate *priv,
                              guint32                index,
                              gint64                *time)
{
  HyScanNMEADataCacheHeader header;

  if (priv->cache == NULL)
    return FALSE;

  /* Обновляем ключ кэширования. */
  hyscan_nmea_data_update_cache_key (priv, index);

  /* Ищем данные в кэше. */
  hyscan_buffer_wrap_data (priv->cache_buffer, HYSCAN_DATA_BLOB, &header, sizeof (header));
  if (!hyscan_cache_get2 (priv->cache, priv->key, NULL, sizeof (header), priv->cache_buffer, priv->nmea_buffer))
    return FALSE;

  /* Верификация данных. */
  if ((header.magic != CACHE_HEADER_MAGIC) ||
      (header.length != hyscan_buffer_get_size (priv->nmea_buffer)))
    {
      return FALSE;
    }

  (time != NULL) ? *time = header.time : 0;

  return TRUE;
}

/* Функция создаёт новый объект обработки NMEA строк. */
HyScanNMEAData*
hyscan_nmea_data_new (HyScanDB         *db,
                      const gchar      *project_name,
                      const gchar      *track_name,
                      HyScanSourceType  source_type,
                      guint             source_channel)
{
  HyScanNMEAData *data;

  data = g_object_new (HYSCAN_TYPE_NMEA_DATA,
                       "db", db,
                       "project-name", project_name,
                       "track-name", track_name,
                       "source-type", source_type,
                       "source-channel", source_channel,
                       NULL);

  if (data->priv->channel_id <= 0)
    g_clear_object (&data);

  return data;
}

/* Функция задаёт используемый кэш. */
void
hyscan_nmea_data_set_cache (HyScanNMEAData *data,
                            HyScanCache    *cache)
{
  g_return_if_fail (HYSCAN_IS_NMEA_DATA (data));

  g_clear_object (&data->priv->cache);
  if (cache != NULL)
    data->priv->cache = g_object_ref (cache);
}

/* Функция возвращает информацию о местоположении приёмной антенны. */
HyScanAntennaPosition
hyscan_nmea_data_get_position (HyScanNMEAData *data)
{
  HyScanAntennaPosition zero = {0};

  g_return_val_if_fail (HYSCAN_IS_NMEA_DATA (data), zero);

  if (data->priv->channel_id <= 0)
    return zero;

  return data->priv->position;
}

/* Функция возвращает тип источника данных. */
HyScanSourceType
hyscan_nmea_data_get_source (HyScanNMEAData *data)
{
  g_return_val_if_fail (HYSCAN_IS_NMEA_DATA (data), HYSCAN_SOURCE_INVALID);

  if (data->priv->channel_id <= 0)
    return HYSCAN_SOURCE_INVALID;

  return data->priv->source_type;
}

/* Функция возвращает номер канала для этого канала данных. */
guint
hyscan_nmea_data_get_channel (HyScanNMEAData *data)
{
  g_return_val_if_fail (HYSCAN_IS_NMEA_DATA (data), 0);

  if (data->priv->channel_id <= 0)
    return 0;

  return data->priv->source_channel;
}

/* Функция определяет возможность изменения данных. */
gboolean
hyscan_nmea_data_is_writable (HyScanNMEAData *data)
{
  g_return_val_if_fail (HYSCAN_IS_NMEA_DATA (data), FALSE);

  if (data->priv->channel_id <= 0)
    return FALSE;

  return hyscan_db_channel_is_writable (data->priv->db, data->priv->channel_id);
}

/* Функция возвращает диапазон значений индексов записанных данных. */
gboolean
hyscan_nmea_data_get_range (HyScanNMEAData *data,
                            guint32        *first,
                            guint32        *last)
{
  g_return_val_if_fail (HYSCAN_IS_NMEA_DATA (data), FALSE);

  if (data->priv->channel_id <= 0)
    return FALSE;

  return hyscan_db_channel_get_data_range (data->priv->db, data->priv->channel_id,
                                           first, last);
}

/* Функция ищет индекс данных для указанного момента времени. */
HyScanDBFindStatus
hyscan_nmea_data_find_data (HyScanNMEAData *data,
                            gint64          time,
                            guint32        *lindex,
                            guint32        *rindex,
                            gint64         *ltime,
                            gint64         *rtime)
{
  g_return_val_if_fail (HYSCAN_IS_NMEA_DATA (data), HYSCAN_DB_FIND_FAIL);

  if (data->priv->channel_id <= 0)
    return HYSCAN_DB_FIND_FAIL;

  return hyscan_db_channel_find_data (data->priv->db, data->priv->channel_id,
                                      time, lindex, rindex, ltime, rtime);
}

/* Функция возвращает строку для указанного индекса. */
const gchar*
hyscan_nmea_data_get_sentence (HyScanNMEAData *data,
                               guint32         index,
                               gint64         *time)
{
  HyScanNMEADataPrivate *priv;

  gint64 nmea_time;
  guint32 size;
  gchar *nmea;

  g_return_val_if_fail (HYSCAN_IS_NMEA_DATA (data), FALSE);
  priv = data->priv;

  if (priv->channel_id <= 0)
    return NULL;

  if (hyscan_nmea_data_check_cache (priv, index, time))
    return hyscan_buffer_get_data (priv->nmea_buffer, &size);

  /* Считываем всю строку во внутренний буфер. */
  if (!hyscan_db_channel_get_data (priv->db, priv->channel_id, index, priv->nmea_buffer, &nmea_time))
    return NULL;

  /* Принудительно пишем '\0' в конец строки.*/
  size = hyscan_buffer_get_size (priv->nmea_buffer);
  hyscan_buffer_set_size (priv->nmea_buffer, size + 1);
  nmea = hyscan_buffer_get_data (priv->nmea_buffer, &size);
  nmea[size - 1] = '\0';

  /* Сохраняем данные в кэше. Сначала время, потом строка. */
  if (priv->cache != NULL)
    {
      HyScanNMEADataCacheHeader header;

      header.magic = CACHE_HEADER_MAGIC;
      header.length = size;
      header.time = nmea_time;
      hyscan_buffer_wrap_data (priv->cache_buffer, HYSCAN_DATA_BLOB, &header, sizeof (header));

      hyscan_cache_set2 (priv->cache, priv->key, NULL, priv->cache_buffer, priv->nmea_buffer);
    }

  (time != NULL) ? *time = nmea_time : 0;

  return nmea;
}

/* Функция возвращает номер изменения в данных. */
guint32
hyscan_nmea_data_get_mod_count (HyScanNMEAData *data)
{
  g_return_val_if_fail (HYSCAN_IS_NMEA_DATA (data), 0);

  if (data->priv->channel_id <= 0)
    return 0;

  return hyscan_db_get_mod_count (data->priv->db, data->priv->channel_id);
}

/* Функция верифицирует nmea-сообщение. */
HyScanSourceType
hyscan_nmea_data_check_sentence (const gchar *sentence)
{
  const gchar *ch = sentence;
  gint checksum = 0;
  gint parsed = 0;
  gsize len = strlen (ch);

  if (sentence == NULL)
    return HYSCAN_SOURCE_INVALID;
  /* Контрольная сумма считается как XOR всех элементов между $ и *.
   * В самой строке контрольная сумма располагается после символа *
   */
  if (*ch != '$')
    return HYSCAN_SOURCE_INVALID;

  /* Подсчитываем контрольную сумму. */
  for (ch = sentence + 1, len--; *ch != '*' && len > 0; ch++, len--)
    checksum ^= *ch;

  /* Если количество оставшихся меньше длины контрольной суммы, строка не валидна. */
  if (len < strlen("*xx"))
    return HYSCAN_SOURCE_INVALID;

  /* Считываем контрольную сумму из строки. */
  parsed = g_ascii_xdigit_value (*++ch) * 16; /*< Первый. */
  parsed += g_ascii_xdigit_value (*++ch);     /*< Второй. */

  /* Если контрольная сумма не совпала, строка не валидна. */
  if (parsed != checksum)
    return HYSCAN_SOURCE_INVALID;

  /* Определяем тип строки. Пропускаем первые 3 символа "$XXyyy". */
  ch = sentence + 3;

  if (g_str_has_prefix (ch, "DPT"))
    return HYSCAN_SOURCE_NMEA_DPT;
  if (g_str_has_prefix (ch, "GGA"))
    return HYSCAN_SOURCE_NMEA_GGA;
  if (g_str_has_prefix (ch, "RMC"))
    return HYSCAN_SOURCE_NMEA_RMC;

  return HYSCAN_SOURCE_NMEA_ANY;
}

/* Функция разделяет строку на подстроки. */
gchar**
hyscan_nmea_data_split_sentence (const gchar *sentence,
                                 guint32      length)
{
  guint num = 0;
  guint32 i, j, k;
  gchar **output = NULL;

  if (sentence == NULL)
    return NULL;

  /* Я предполагаю, что строки могут оказаться разделены абсолютно
   * любыми символами. Однако стандарт весьма однозначно гласит,
   * что nmea-сообщение начинается с символа '$', а заканчивается
   * символами '*xy', где xy - контрольная сумма.
   *
   * Таким образом, я буду искать знак $, за ним искать знак *, добавлять
   * ещё 2 символа, копировать полученное в выходной вектор и принудительно
   * нуль-терминировать.
   */

  /* Подсчет общего числа сообщений. */
  for (i = 0; i < length; i++)
    {
      if (sentence[i] == '$')
        num++;
    }

  /* Выделяем память под вектор строк (и не забываем про терминирующий нуль). */
  output = g_malloc0 ((num + 1) * sizeof (gchar*));

  /* Начинаем вычленять подстроки. */
  k = 0;
  for (i = 0; i < length; i++)
    {
      guint32 size;

      /* Ищем вхождение знака '$'. */
      if (sentence[i] != '$')
        continue;

      /* Ищем конец текущей подстроки. */
      for (j = i + 1; j < length; j++)
        {
          if (sentence[j] == '*')
            {
              /* Накидываем ещё 2 символа. */
              j += 2;
              break;
            }
        }

      if (G_UNLIKELY (j >= length))
        j = length - 1;

      size = j - i + 1;

      /* Выделяем память, не забывая про \0. */
      output[k] = g_malloc0 ((size + 1) * sizeof (gchar));
      /* Копируем. */
      memcpy (output[k], sentence + i, size * sizeof (gchar));
      k++;
    }

  return output;
}
