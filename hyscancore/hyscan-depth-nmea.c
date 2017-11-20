/*
 * \file hyscan-depth-nmea.c
 *
 * \brief Исходный файл класса извлечения глубины из NMEA-строк.
 * \author Dmitriev Alexander (m1n7@yandex.ru)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-depth-nmea.h"
#include <hyscan-nmea-data.h>
#include "hyscan-core-schemas.h"
#include <string.h>

typedef struct
{
  gdouble depth;
  gint64  time;
} HyScanDepthTime;

enum
{
  PROP_O,
  PROP_DB,
  PROP_PROJECT,
  PROP_TRACK,
  PROP_SOURCE_CHANNEL
};

struct _HyScanDepthNMEAPrivate
{
  /* Переменные, передающиеся на этапе создания объекта. */
  HyScanDB              *db;            /* База данных. */
  gchar                 *project;       /* Имя проекта. */
  gchar                 *track;         /* Имя галса. */
  HyScanSourceType       source_type;   /* Тип данных. */
  guint                  channeln;      /* Номер канала для используемого типа данных. */

  /* Переменные, определяющиеся на этапе конструирования. */
  HyScanNMEAData        *dc;            /* Канал данных. */
  gchar                 *token;         /* Токен. */

  /* Система кэширования. */
  HyScanCache           *cache;         /* Кэш. */
  gchar                 *prefix;        /* Префикс системы кэширования. */
  gchar                 *key;           /* Ключ кэша. */
  gint                   key_length;    /* Длина ключа. */

  HyScanAntennaPosition  position;      /* Местоположение приемника. */
};

static void     hyscan_depth_nmea_interface_init        (HyScanDepthInterface   *iface);
static void     hyscan_depth_nmea_set_property          (GObject                *object,
                                                         guint                   prop_id,
                                                         const GValue           *value,
                                                         GParamSpec             *pspec);
static void     hyscan_depth_nmea_object_constructed    (GObject                *object);
static void     hyscan_depth_nmea_object_finalize       (GObject                *object);
static void     hyscan_depth_nmea_update_cache_key      (HyScanDepthNMEAPrivate *priv,
                                                         guint32                 index);

static gdouble  hyscan_depth_nmea_parse_sentence        (const gchar            *sentence);

G_DEFINE_TYPE_WITH_CODE (HyScanDepthNMEA, hyscan_depth_nmea, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (HyScanDepthNMEA)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_DEPTH, hyscan_depth_nmea_interface_init));

static void
hyscan_depth_nmea_class_init (HyScanDepthNMEAClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_depth_nmea_set_property;
  object_class->constructed = hyscan_depth_nmea_object_constructed;
  object_class->finalize = hyscan_depth_nmea_object_finalize;

  g_object_class_install_property (object_class, PROP_DB,
      g_param_spec_object ("db", "DB", "HyScanDB interface", HYSCAN_TYPE_DB,
                           G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_PROJECT,
      g_param_spec_string ("project", "ProjectName", "project name", NULL,
                           G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_TRACK,
      g_param_spec_string ("track", "TrackName", "track name", NULL,
                           G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_SOURCE_CHANNEL,
      g_param_spec_int ("source-channel", "SourceChannel", "Source channel", 1, 5, 1,
                       G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_depth_nmea_init (HyScanDepthNMEA *depth_nmea)
{
  depth_nmea->priv = hyscan_depth_nmea_get_instance_private (depth_nmea);
}

static void
hyscan_depth_nmea_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  HyScanDepthNMEA *depth = HYSCAN_DEPTH_NMEA (object);
  HyScanDepthNMEAPrivate *priv = depth->priv;

  switch (prop_id)
    {
    case PROP_DB:
      priv->db = g_value_dup_object (value);
      break;

    case PROP_PROJECT:
      priv->project = g_value_dup_string (value);
      break;

    case PROP_TRACK:
      priv->track = g_value_dup_string (value);
      break;

    case PROP_SOURCE_CHANNEL:
      priv->channeln = g_value_get_int (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (depth, prop_id, pspec);
      break;
    }
}

static void
hyscan_depth_nmea_object_constructed (GObject *object)
{
  gchar *db_uri;
  HyScanDepthNMEA *dnmea = HYSCAN_DEPTH_NMEA (object);
  HyScanDepthNMEAPrivate *priv = dnmea->priv;

  /* Открываем КД. */
  priv->dc = hyscan_nmea_data_new (priv->db, priv->project, priv->track,
                                     HYSCAN_SOURCE_NMEA_DPT, priv->channeln);

  if (priv->dc == NULL)
    return;

  /* Генерируем токен. */
  db_uri = hyscan_db_get_uri (priv->db);
  priv->token = g_strdup_printf ("depth_nmea.%s.%s.%s.%i",
                                 db_uri, priv->project,
                                 priv->track, priv->channeln);

  priv->position = hyscan_nmea_data_get_position (priv->dc);

  g_free (db_uri);
}

static void
hyscan_depth_nmea_object_finalize (GObject *object)
{
  HyScanDepthNMEA *dnmea = HYSCAN_DEPTH_NMEA (object);
  HyScanDepthNMEAPrivate *priv = dnmea->priv;

  g_free (priv->project);
  g_free (priv->track);

  g_clear_object (&priv->dc);

  g_clear_object (&priv->db);
  g_free (priv->token);

  g_clear_object (&priv->cache);
  g_free (priv->prefix);
  g_free (priv->key);

  G_OBJECT_CLASS (hyscan_depth_nmea_parent_class)->finalize (object);
}

/* Функция генерации и обновления ключа кэширования. */
static void
hyscan_depth_nmea_update_cache_key (HyScanDepthNMEAPrivate *priv,
                                    guint32                 index)
{
  if (priv->key == NULL)
    {
      priv->key = g_strdup_printf ("DEPTH_NMEA.%s.%s.%u",
                                    priv->prefix, priv->token,
                                    G_MAXUINT32);
      priv->key_length = strlen (priv->key);
    }

  g_snprintf (priv->key, priv->key_length,
              "DEPTH_NMEA.%s.%s.%u",
              priv->prefix, priv->token, index);
}

/* Функция извлекает глубину из NMEA-строки. */
static gdouble
hyscan_depth_nmea_parse_sentence (const gchar *sentence)
{
  gchar *sub;

  /* Проверяем тип строки. */
  if (HYSCAN_SOURCE_NMEA_DPT != hyscan_nmea_data_check_sentence (sentence))
    return -1.0;

  /* Глубина - это значение в 1 поле DPT. */
  sub = strchr (sentence, ',') + 1;

  return g_ascii_strtod (sub, NULL);
}

/* Функция установки кэша. */
static void
hyscan_depth_nmea_set_cache (HyScanDepth  *depth,
                             HyScanCache  *cache,
                             const gchar  *prefix)
{
  HyScanDepthNMEA *dnmea = HYSCAN_DEPTH_NMEA (depth);
  HyScanDepthNMEAPrivate *priv = dnmea->priv;;

  g_free (priv->prefix);
  g_clear_object (&priv->cache);

  if (cache == NULL)
    return;

  priv->cache = g_object_ref (cache);

  if (prefix != NULL)
    priv->prefix = g_strdup (prefix);
  else
    priv->prefix = g_strdup ("none");

  hyscan_nmea_data_set_cache (priv->dc, cache, prefix);
}

/* Функция получения глубины. */
static gdouble
hyscan_depth_nmea_get (HyScanDepth *depth,
                       guint32      index,
                       gint64      *time)
{
  HyScanDepthNMEA *dnmea = HYSCAN_DEPTH_NMEA (depth);
  HyScanDepthNMEAPrivate *priv = dnmea->priv;

  const gchar *nmea_sentence;
  gdouble nmea_depth;
  gint64 nmea_time;

  /* Ищем уже посчитанное значение в кэше. */
  if (priv->cache != NULL)
    {
      HyScanDepthTime dt[2];
      guint32 dt_size;
      gboolean found;

      dt_size = sizeof (dt);
      hyscan_depth_nmea_update_cache_key (priv, index);

      /* Проверяем наличие и совпадение размера. */
      found = hyscan_cache_get (priv->cache, priv->key, NULL, &dt, &dt_size);
      if (found && dt_size == sizeof (HyScanDepthTime))
        {
          nmea_depth = dt[0].depth;
          nmea_time = dt[0].time;
          goto exit;
        }
    }

  /* Если в кэше значения нет, то придется считать.
   * Забираем строку из БД и парсим её. */
  nmea_sentence = hyscan_nmea_data_get_sentence (priv->dc, index, NULL, &nmea_time);
  nmea_depth = hyscan_depth_nmea_parse_sentence (nmea_sentence);

  /* Кэшируем. */
  if (priv->cache != NULL)
    {
      HyScanDepthTime dt;
      guint32 dt_size;

      dt.time = nmea_time;
      dt.depth = nmea_depth;
      dt_size = sizeof (HyScanDepthTime);

      hyscan_cache_set (priv->cache, priv->key, NULL, &dt, dt_size);
    }

exit:
  if (time != NULL)
    *time = nmea_time;

  return nmea_depth;
}

/* Функция поиска данных. */
static HyScanDBFindStatus
hyscan_depth_nmea_find_data (HyScanDepth *depth,
                             gint64       time,
                             guint32     *lindex,
                             guint32     *rindex,
                             gint64      *ltime,
                             gint64      *rtime)
{
  HyScanDepthNMEA *dnmea = HYSCAN_DEPTH_NMEA (depth);
  return hyscan_nmea_data_find_data (dnmea->priv->dc, time, lindex, rindex, ltime, rtime);
}

/* Функция определения диапазона. */
static gboolean
hyscan_depth_nmea_get_range (HyScanDepth *depth,
                             guint32     *first,
                             guint32     *last)
{
  HyScanDepthNMEA *dnmea = HYSCAN_DEPTH_NMEA (depth);
  return hyscan_nmea_data_get_range (dnmea->priv->dc, first, last);
}

/* Функция получения местоположения антенны. */
static HyScanAntennaPosition
hyscan_depth_nmea_get_position (HyScanDepth *depth)
{
  HyScanDepthNMEA *dnmea = HYSCAN_DEPTH_NMEA (depth);
  return dnmea->priv->position;
}

/* Функция определяет, возможна ли дозапись в канал данных. */
static gboolean
hyscan_depth_nmea_is_writable (HyScanDepth *depth)
{
  HyScanDepthNMEA *dnmea = HYSCAN_DEPTH_NMEA (depth);
  return hyscan_nmea_data_is_writable (dnmea->priv->dc);
}

/* Функция возвращает токен объекта. */
static const gchar*
hyscan_depth_nmea_get_token (HyScanDepth *depth)
{
  HyScanDepthNMEA *dnmea = HYSCAN_DEPTH_NMEA (depth);
  return dnmea->priv->token;
}

/* Функция возвращает значение счётчика изменений. */
guint32
hyscan_depth_nmea_get_mod_count (HyScanDepth *depth)
{
  HyScanDepthNMEA *dnmea = HYSCAN_DEPTH_NMEA (depth);
  return hyscan_nmea_data_get_mod_count (dnmea->priv->dc);
}

/* Функция создает новый объект. */
HyScanDepthNMEA*
hyscan_depth_nmea_new (HyScanDB        *db,
                       const gchar     *project,
                       const gchar     *track,
                       guint            source_channel)
{
  HyScanDepthNMEA *dnmea;

  dnmea = g_object_new (HYSCAN_TYPE_DEPTH_NMEA,
                       "db", db,
                       "project", project,
                       "track", track,
                       "source-channel", source_channel,
                       NULL);

  if (dnmea->priv->dc == NULL)
    g_clear_object (&dnmea);

  return dnmea;
}

static void
hyscan_depth_nmea_interface_init (HyScanDepthInterface *iface)
{
  iface->set_sound_velocity = NULL;
  iface->set_cache          = hyscan_depth_nmea_set_cache;
  iface->get                = hyscan_depth_nmea_get;
  iface->find_data          = hyscan_depth_nmea_find_data;
  iface->get_range          = hyscan_depth_nmea_get_range;
  iface->get_position       = hyscan_depth_nmea_get_position;
  iface->get_token          = hyscan_depth_nmea_get_token;
  iface->is_writable        = hyscan_depth_nmea_is_writable;
  iface->get_mod_count      = hyscan_depth_nmea_get_mod_count;
}