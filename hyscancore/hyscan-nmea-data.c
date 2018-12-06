/* hyscan-nmea-data.c
 *
 * Copyright 2018 Screen LLC, Alexander Dmitriev <m1n7@yandex.ru>
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
 * SECTION: hyscan-nmea-data
 * @Short_description: класс работы с NMEA-строками
 * @Title: HyScanNMEAData
 *
 * Класс предназначен для получения NMEA-строк из БД. Класс не осуществляет
 * верификацию строк, однако это можно сделать вспомогательной функцией
 * #hyscan_nmea_data_check_sentence.
 *
 * В канале HYSCAN_SOURCE_NMEA_ANY по одному индексу может оказаться несколько
 * строк. В таком случае класс вернет всю строку целиком. Для разбиения этой
 * строки на отдельные сообщения можно использовать функцию
 * #hyscan_nmea_data_split_sentence, которая вернет нуль-терминированный список
 * строк.
 *
 */

#include "hyscan-nmea-data.h"
#include "hyscan-core-schemas.h"
#include "hyscan-core-common.h"
#include <string.h>

#define CACHE_HEADER_MAGIC     0x3f0a4b87    /* Идентификатор заголовка кэша. */

enum
{
  PROP_O,
  PROP_CACHE,
  PROP_DB,
  PROP_PROJECT_NAME,
  PROP_TRACK_NAME,
  PROP_SOURCE_TYPE,
  PROP_SOURCE_CHANNEL,
};

/* Структруа заголовка кэша. */
typedef struct
{
  guint32               magic;          /* Идентификатор заголовка. */
  guint32               length;         /* Длина NMEA строки. */
  gint64                time;           /* Метка времени. */
} HyScanNMEADataCacheHeader;

struct _HyScanNMEADataPrivate
{
  HyScanDB             *db;             /* Интерфейс базы данных. */

  gchar                *project;        /* Название проекта. */
  gchar                *track;          /* Название галса. */
  HyScanSourceType      source_type;    /* Тип источника данных. */
  guint                 source_channel; /* Индекс канала данных. */

  HyScanCache          *cache;          /* Интерфейс системы кэширования. */
  gchar                *path;           /* Путь к БД, проекту, галсу и каналу. */
  HyScanBuffer         *cache_buffer;   /* Буфер заголовка кэша данных. */

  gchar                *key;            /* Ключ кэширования. */
  gsize                 key_length;     /* Максимальная длина ключа. */

  HyScanAntennaPosition position;       /* Местоположение приёмной антенны. */
  gint32                channel_id;     /* Идентификатор открытого канала данных. */

  HyScanBuffer         *nmea_buffer;    /* Буфер данных. */

};

static void      hyscan_nmea_data_set_property          (GObject               *object,
                                                         guint                  prop_id,
                                                         const GValue          *value,
                                                         GParamSpec            *pspec);
static void      hyscan_nmea_data_get_property          (GObject               *object,
                                                         guint                  property_id,
                                                         GValue                *value,
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
  object_class->get_property = hyscan_nmea_data_get_property;

  object_class->constructed = hyscan_nmea_data_object_constructed;
  object_class->finalize = hyscan_nmea_data_object_finalize;

  g_object_class_install_property (object_class, PROP_CACHE,
    g_param_spec_object ("cache", "Cache", "HyScanCache interface", HYSCAN_TYPE_CACHE,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_DB,
    g_param_spec_object ("db", "DB", "HyScanDB interface", HYSCAN_TYPE_DB,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_PROJECT_NAME,
    g_param_spec_string ("project-name", "ProjectName", "Project name", NULL,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_TRACK_NAME,
    g_param_spec_string ("track-name", "TrackName", "Track name", NULL,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_SOURCE_TYPE,
    g_param_spec_int ("source-type", "SourceType", "Source type", 0, G_MAXINT, 0,
                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_SOURCE_CHANNEL,
    g_param_spec_uint ("source-channel", "SourceChannel", "Source channel", 1, G_MAXUINT, 1,
                       G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
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
    case PROP_CACHE:
      priv->cache = g_value_dup_object (value);
      break;

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
hyscan_nmea_data_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  HyScanNMEAData *data = HYSCAN_NMEA_DATA (object);
  HyScanNMEADataPrivate *priv = data->priv;

  switch (prop_id)
    {
    case PROP_CACHE:
      g_value_set_object (value, priv->cache);
      break;

    case PROP_DB:
      g_value_set_object (value, priv->db);
      break;

    case PROP_PROJECT_NAME:
      g_value_set_string (value, priv->project);
      break;

    case PROP_TRACK_NAME:
      g_value_set_string (value, priv->track);
      break;

    case PROP_SOURCE_TYPE:
      g_value_set_int (value, priv->source_type);
      break;

    case PROP_SOURCE_CHANNEL:
      g_value_set_uint (value, priv->source_channel);
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

  channel_name = hyscan_channel_get_name_by_types (priv->source_type,
                                                   HYSCAN_CHANNEL_DATA,
                                                   priv->source_channel);

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

/**
 * hyscan_nmea_data_new:
 * @db указатель на #HyScanDB
 * @cache указатель #HyScanCache
 * @project_name название проекта
 * @track_name название галса
 * @source_type тип источника данных
 * @source_channel индекс канала данных
 *
 * Функция создаёт новый объект обработки NMEA строк.
 * В требуемом канале данных должна быть хотя бы одна запись,
 * иначе будет возращен NULL.
 *
 * Returns: Указатель на объект #HyScanNMEAData или NULL.
 */
HyScanNMEAData*
hyscan_nmea_data_new (HyScanDB         *db,
                      HyScanCache      *cache,
                      const gchar      *project_name,
                      const gchar      *track_name,
                      HyScanSourceType  source_type,
                      guint             source_channel)
{
  HyScanNMEAData *data;

  data = g_object_new (HYSCAN_TYPE_NMEA_DATA,
                       "db", db,
                       "cache", cache,
                       "project-name", project_name,
                       "track-name", track_name,
                       "source-type", source_type,
                       "source-channel", source_channel,
                       NULL);

  if (data->priv->channel_id <= 0)
    g_clear_object (&data);

  return data;
}

/**
 * hyscan_nmea_data_get_position:
 * @data указатель на #HyScanNMEAData
 *
 * Функция возвращает информацию о местоположении приёмной антенны
 *
 * \return Местоположение приёмной антенны.
 */
HyScanAntennaPosition
hyscan_nmea_data_get_position (HyScanNMEAData *data)
{
  HyScanAntennaPosition zero = {0};

  g_return_val_if_fail (HYSCAN_IS_NMEA_DATA (data), zero);

  if (data->priv->channel_id <= 0)
    return zero;

  return data->priv->position;
}

/**
 * hyscan_nmea_data_get_source:
 * @data указатель на #HyScanNMEAData
 *
 * Функция возвращает тип источника данных.
 *
 * Returns: Тип источника данных из #HyScanSourceType.
 */
HyScanSourceType
hyscan_nmea_data_get_source (HyScanNMEAData *data)
{
  g_return_val_if_fail (HYSCAN_IS_NMEA_DATA (data), HYSCAN_SOURCE_INVALID);

  if (data->priv->channel_id <= 0)
    return HYSCAN_SOURCE_INVALID;

  return data->priv->source_type;
}

/**
 * hyscan_nmea_data_get_channel:
 * @data: указатель на #HyScanNMEAData
 *
 * Функция возвращает номер канала для используемого источника данных.
 *
 * Returns: Номер канала.
 */
guint
hyscan_nmea_data_get_channel (HyScanNMEAData *data)
{
  g_return_val_if_fail (HYSCAN_IS_NMEA_DATA (data), 0);

  if (data->priv->channel_id <= 0)
    return 0;

  return data->priv->source_channel;
}

/**
 * hyscan_nmea_data_is_writable:
 * @data: указатель на #HyScanNMEAData
 *
 * Функция определяет возможность изменения данных. Если функция вернёт значение %TRUE,
 * возможна ситуация когда могут появиться новые данные или исчезнуть уже записанные.
 *
 * Returns: %TRUE - если возможна запись данных, %FALSE - если данные в режиме только чтения.
 */
gboolean
hyscan_nmea_data_is_writable (HyScanNMEAData *data)
{
  g_return_val_if_fail (HYSCAN_IS_NMEA_DATA (data), FALSE);

  if (data->priv->channel_id <= 0)
    return FALSE;

  return hyscan_db_channel_is_writable (data->priv->db, data->priv->channel_id);
}

/**
 * hyscan_nmea_data_get_range:
 * @data: указатель на #HyScanNMEAData
 * @first: (out) (nullable): указатель на переменную для начального индекса
 * @last: (out) (nullable): указатель на переменную для конечного индекса
 *
 * Функция возвращает диапазон значений индексов записанных данных. Функция вернёт значения
 * начального и конечного индекса записей.
 *
 * Returns: %TRUE - если границы записей определены, %FALSE - в случае ошибки.
 */
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

/**
 * hyscan_nmea_data_find_data:
 * @data: указатель на #HyScanNMEAData
 * @time: искомый момент времени
 * @lindex: (out) (nullable): "левый" индекс данных
 * @rindex: (out) (nullable): "правый" индекс данных
 * @ltime: (out) (nullable): "левая" метка времени данных
 * @rtime: (out) (nullable): "правая" метка времени данных
 *
 * Функция ищет индекс данных для указанного момента времени.
 *
 * Returns: %TRUE - если данные найдены, %FALSE - в случае ошибки.
 */
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

/**
 * hyscan_nmea_data_get_sentence:
 * @data: указатель на #HyScanNMEAData
 * @index: индекс считываемых данных
 * @time: (out) (nullable): указатель на переменную для сохранения метки времени считанных данных
 *
 * Функция возвращает указатель на NMEA-строку.
 * Строка будет гарантированно нуль-терминирована.
 * Владельцем строки является класс, запрещено освобождать эту память.
 *
 * Returns: (transfer none): указатель на константную нуль-терминированную строку.
 */
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

/**
 * hyscan_nmea_data_get_mod_count:
 * @data: указатель на #HyScanNMEAData
 *
 * Функция возвращает номер изменения в данных. Нельзя полагаться на значение
 * номера изменения, важен только факт смены номера по сравнению с предыдущим
 * запросом.
 *
 * Returns: Номер изменения.
 */
guint32
hyscan_nmea_data_get_mod_count (HyScanNMEAData *data)
{
  g_return_val_if_fail (HYSCAN_IS_NMEA_DATA (data), 0);

  if (data->priv->channel_id <= 0)
    return 0;

  return hyscan_db_get_mod_count (data->priv->db, data->priv->channel_id);
}

/**
 * hyscan_nmea_data_check_sentence:
 * @sentence: указатель на строку
 *
 * Функция верифицирует NMEA-строку: проверяет контрольную сумму и тип строки.
 * Если тип сообщения совпадает с DPT, RMC или GGA, будут возвращены
 * соответственно HYSCAN_SOURCE_NMEA_DPT, HYSCAN_SOURCE_NMEA_GGA,
 * HYSCAN_SOURCE_NMEA_RMC. Если тип сообщения не соотвествует перечисленным
 * выше, но при этом контрольная сумма совпала, будет возвращено
 * HYSCAN_SOURCE_NMEA_ANY. Если контрольная сумма не совпала, будет возвращено
 * HYSCAN_SOURCE_INVALID.
 *
 * Returns: тип строки.
 */
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

/**
 * hyscan_nmea_data_split_sentence:
 * @sentence: указатель на строку
 * @length: длина строки

 * Функция разбивает строку, содержащую несколько NMEA-сообщений, на отдельные
 * строки. В канале HYSCAN_SOURCE_NMEA_ANY по одному индексу может находиться
 * сразу несколько сообщений. Эта функция генерирует нуль-терминированный вектор
 * строк. Владельцем строки становится пользователь и после использования её нужно освободить
 * функцией g_strfreev.
 *
 *
 * Returns: указатель на нуль-терминированный список NMEA-строк. Используйте
 * g_strfreev() чтобы освободить эту память.
 */
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
