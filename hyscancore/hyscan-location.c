#include "hyscan-location.h"
#include "hyscan-location-tools.h"
#include <glib.h>
#include <glib/gprintf.h>


#define HYSCAN_LOCATIONDATA_NAN NAN,NAN,NAN,0,0,0,NAN,NAN,0,0

/* Макрос, используемый при установке источников по-умолчанию.
 * Написан исключительно с целью уменьшения количества повторяющегося кода. */
#define SOURCE_LIST_CHECK(src_type,par) (source_info->source_type == (src_type) && source_info->parameter == (par))

enum
{
  PROP_O,
  PROP_DB,
  PROP_CACHE,
  PROP_CACHE_PREFIX,
  PROP_PROJECT,
  PROP_TRACK,
  PROP_PARAM_PREFIX,
  PROP_OVERSEER_PERIOD,
  PROP_Q
};

struct _HyScanLocationPrivate
{
  HyScanDB             *db;                             /* Интерфейс базы данных. */
  gchar                *uri;                            /* URI базы данных. */
  gchar                *project_name;                   /* Название проекта. */
  gchar                *track_name;                     /* Название галса. */
  gint32                project_id;                     /* Идентификатор проекта. */
  gint32                track_id;                       /* Идентификатор галса. */

  HyScanCache          *cache;                          /* Интерфейс системы кэширования. */
  gchar                *cache_prefix;                   /* Префикс ключа кэширования. */
  gchar                *cache_key;                      /* Ключ кэширования. */
  gsize                 cache_key_length;               /* Максимальная длина ключа. */
  gchar                *detail_key;                     /* Вспомогательная информация для кэширования. */
  gsize                 detail_key_length;              /* Максимальная длина вспомогательной информации. */

  gint32                param_id;                       /* Идентификатор группы параметров проекта. */
  gchar                *param_prefix;                   /* Префикс группы параметров. */
  GArray               *user_params;                    /* Пользовательские параметры обработки. */

  GArray               *source_list;                    /* Внутренний список источников. */

  HyScanLocationSourceIndices source_indices;           /* Индексы источников данных. */

  HyScanLocationLocalCaches local_caches;               /* Локальные кэши. */

  HyScanLocationOverseerParams overseer;                /* Параметры надзирателя. */

  GArray                soundspeedtable;                /* Таблица скорости звука. */

  guint                 mod_count;                      /* Счетчик изменеий. */
  gint32                progress;                       /* Процент выполнения. */
  gdouble               quality;                        /* Качество данных и степень обработки. */
  GMutex                lock;                           /* Блокировка многопоточного доступа. */
};

static void             hyscan_location_set_property    (GObject            *object,
                                                         guint               prop_id,
                                                         const GValue       *value,
                                                         GParamSpec         *pspec);
static void             hyscan_location_constructed     (GObject            *object);
static void             hyscan_location_finalize        (GObject            *object);

static void             hyscan_location_geo_initialize  (HyScanLocation     *location,
                                                         gboolean            uninit);

static void             hyscan_location_source_list_int (HyScanLocation     *location);
static void             hyscan_location_source_defaults (HyScanLocation     *location);
static gboolean         hyscan_location_param_load      (HyScanLocation     *location,
                                                         gint32              source);

static gpointer         hyscan_location_overseer        (gpointer            user_data);

static gboolean         hyscan_location_update_cache_key(HyScanLocation     *location,
                                                         gint64              time);
static gboolean         hyscan_location_cache_get       (HyScanLocation     *location,
                                                         gint                parameter,
                                                         HyScanLocationData *data);
static void             hyscan_location_source_free     (HyScanLocationSourcesList *data);
static void             hyscan_location_source_list_add (GArray             *source_list,
                                                         gint32              index,
                                                         HyScanLocationParameters parameter,
                                                         HyScanLocationSourceTypes source_type,
                                                         gint                sensor_channel,
                                                         gchar             **channel_list,
                                                         gint                channel_list_index);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanLocation, hyscan_location, HYSCAN_TYPE_GEO);

static void
hyscan_location_class_init (HyScanLocationClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_location_set_property;

  object_class->constructed = hyscan_location_constructed;
  object_class->finalize = hyscan_location_finalize;

  /* При создании объекта передаем ему указатели на БД, галс и канал. */
  g_object_class_install_property (object_class, PROP_DB,
      g_param_spec_object ("db", "DB", "HyScanDB interface", HYSCAN_TYPE_DB,
                           G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_CACHE,
    g_param_spec_object ("cache", "Cache", "HyScanCache interface", HYSCAN_TYPE_CACHE,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_CACHE_PREFIX,
    g_param_spec_string ("cache_prefix", "CachePrefix", "Cache key prefix", NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_PROJECT,
      g_param_spec_string ("project_name", "ProjectName", "project name", NULL,
                           G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_TRACK,
      g_param_spec_string ("track_name", "Track", "Track name", NULL,
                           G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_CACHE_PREFIX,
      g_param_spec_string ("param_prefix", "ParamPrefix", "Parameters prefix", NULL,
                           G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_OVERSEER_PERIOD,
      g_param_spec_int64 ("overseer_period", "OverseerPeriod", "Overseer sleep time", 1e2, 1e8, 1e6,
                          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));

  g_object_class_install_property (object_class, PROP_Q,
      g_param_spec_double ("quality", "Quality", "Quality of input", 0.0, 1.0, 0.5,
                           G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));
}

static void
hyscan_location_init (HyScanLocation *location)
{
  location->priv = hyscan_location_get_instance_private (location);
}

static void
hyscan_location_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  HyScanLocation *location = HYSCAN_LOCATION (object);
  HyScanLocationPrivate *priv = location->priv;

  switch (prop_id)
    {
    case PROP_DB:
      priv->db = g_value_dup_object (value);
      priv->uri = hyscan_db_get_uri (priv->db);
      break;

    case PROP_CACHE:
      priv->cache = g_value_get_object (value);
      if (priv->cache != NULL)
        g_object_ref (priv->cache);
      break;

    case PROP_CACHE_PREFIX:
      priv->cache_prefix = g_value_dup_string (value);
      break;

    case PROP_PROJECT:
      priv->project_name = g_value_dup_string (value);
      break;

    case PROP_TRACK:
      priv->track_name = g_value_dup_string (value);
      break;

    case PROP_PARAM_PREFIX:
      priv->param_prefix = g_value_dup_string (value);
      break;

    case PROP_OVERSEER_PERIOD:
      priv->overseer.overseer_period = g_value_get_int64 (value);
      break;

    case PROP_Q:
      priv->quality = g_value_get_double (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (location, prop_id, pspec);
      break;
    }

  g_atomic_int_inc (&priv->mod_count);
}

static void
hyscan_location_constructed (GObject *object)
{
  HyScanLocation *location = HYSCAN_LOCATION (object);
  HyScanLocationPrivate *priv = location->priv;

  gchar *param_name = NULL;

  /* Вызываем конструктор родительского класса. */
  G_OBJECT_CLASS (hyscan_location_parent_class)->constructed (object);

  priv->overseer.overseer_thread = NULL;
  priv->overseer.overseer_pause = 0;
  priv->overseer.overseer_paused = 0;

  priv->mod_count = 0;

  if (priv->param_prefix == NULL)
    priv->param_prefix = g_strdup_printf ("%s", "location.default");

  param_name = g_strdup_printf ("%s.%s", priv->param_prefix, priv->track_name);

  /* Открываем проект и галс. */
  if (priv->db != NULL)
    {
      priv->project_id = hyscan_db_project_open (priv->db, priv->project_name);
      priv->track_id = hyscan_db_track_open (priv->db, priv->project_id, priv->track_name);

      priv->param_id = hyscan_db_project_param_open (priv->db, priv->project_id, param_name);
      priv->user_params = g_array_new (FALSE, TRUE, sizeof(HyScanLocationUserParameters));

      priv->source_indices.latlong_source = -1;
      priv->source_indices.altitude_source = -1;
      priv->source_indices.track_source = -1;
      priv->source_indices.roll_source = -1;
      priv->source_indices.pitch_source = -1;
      priv->source_indices.speed_source = -1;
      priv->source_indices.depth_source = -1;
      priv->source_indices.datetime_source = -1;

      /* Составляем список источников. */
      hyscan_location_source_list_int (location);

      /* Устанавливаем источники по умолчанию. */
      hyscan_location_source_defaults (location);

      /* Запускаем поток надзирателя. */
      g_atomic_int_set (&(priv->overseer.overseer_stop), 0);
      priv->overseer.overseer_thread = g_thread_new ("overseer thread", hyscan_location_overseer, location);
      /* Ждем, пока поток не запустится. */
      while (g_atomic_int_get (&(priv->overseer.overseer_started)) != 1)
        g_usleep (1000);
    }

  g_free (param_name);
}

/* Функция создает новый объект обработки навигационных данных. */
HyScanLocation *
hyscan_location_new (HyScanDB    *db,
                     gchar       *project,
                     gchar       *track,
                     gchar       *param_prefix,
                     gdouble      quality)
{
  return g_object_new (HYSCAN_TYPE_LOCATION,
                       "db", db,
                       "project_name", project,
                       "track_name", track,
                       "param_prefix", param_prefix,
                       "quality", quality,
                       NULL);
}

/* Функция создает новый объект обработки навигационных данных с использованием кэша. */
HyScanLocation*
hyscan_location_new_with_cache (HyScanDB       *db,
                                HyScanCache    *cache,
                                gchar          *project,
                                gchar          *track,
                                gchar          *param_prefix,
                                gdouble         quality)
{
  return g_object_new (HYSCAN_TYPE_LOCATION,
                       "db", db,
                       "cache", cache,
                       "project_name", project,
                       "track_name", track,
                       "param_prefix", param_prefix,
                       "quality", quality,
                       NULL);
}

/* Функция создает новый объект обработки .навигационных данных с использованием кэша и префиксом. */
HyScanLocation*
hyscan_location_new_with_cache_prefix (HyScanDB       *db,
                                       HyScanCache    *cache,
                                       gchar          *cache_prefix,
                                       gchar          *project,
                                       gchar          *track,
                                       gchar          *param_prefix,
                                       gdouble         quality)
{
  return g_object_new (HYSCAN_TYPE_LOCATION,
                       "db", db,
                       "cache", cache,
                       "cache_prefix", cache_prefix,
                       "project_name", project,
                       "track_name", track,
                       "param_prefix", param_prefix,
                       "quality", quality,
                       NULL);

}

static void
hyscan_location_finalize (GObject *object)
{
  HyScanLocation *location = HYSCAN_LOCATION (object);
  HyScanLocationPrivate *priv = location->priv;
  guint i;

  HyScanLocationSourcesList *source_info;

  /* Говорим надзирателю, что пора бы и остановиться, и закрываем соответствующий поток. */
  g_atomic_int_set (&(priv->overseer.overseer_stop), 1);
  g_thread_join (priv->overseer.overseer_thread);

  /* Очищаем кэши. */
  if (priv->local_caches.latlong_cache != NULL)
    {
      g_array_free (priv->local_caches.latlong_cache, TRUE);
      priv->local_caches.latlong_cache = NULL;
    }
  if (priv->local_caches.altitude_cache != NULL)
    {
      g_array_free (priv->local_caches.altitude_cache, TRUE);
      priv->local_caches.altitude_cache = NULL;
    }
  if (priv->local_caches.track_cache != NULL)
    {
      g_array_free (priv->local_caches.track_cache, TRUE);
      priv->local_caches.track_cache = NULL;
    }
  if (priv->local_caches.roll_cache != NULL)
    {
      g_array_free (priv->local_caches.roll_cache, TRUE);
      priv->local_caches.roll_cache = NULL;
    }
  if (priv->local_caches.pitch_cache != NULL)
    {
      g_array_free (priv->local_caches.pitch_cache, TRUE);
      priv->local_caches.pitch_cache = NULL;
    }
  if (priv->local_caches.speed_cache != NULL)
    {
      g_array_free (priv->local_caches.speed_cache, TRUE);
      priv->local_caches.speed_cache = NULL;
    }
  if (priv->local_caches.depth_cache != NULL)
    {
      g_array_free (priv->local_caches.depth_cache, TRUE);
      priv->local_caches.depth_cache = NULL;
    }
  if (priv->local_caches.datetime_cache != NULL)
    {
      g_array_free (priv->local_caches.datetime_cache, TRUE);
      priv->local_caches.datetime_cache = NULL;
    }

  if (priv->user_params != NULL)
    g_array_free (priv->user_params, TRUE);

  /* Закрываем каналы данных. */
  for (i = 0; i < priv->source_list->len; i++)
    {
      source_info = &g_array_index (priv->source_list, HyScanLocationSourcesList, i);
      if (source_info->active == TRUE)
        {
          hyscan_db_close (priv->db, source_info->channel_id);
          hyscan_db_close (priv->db, source_info->param_id);
        }
    }

  g_array_free (priv->source_list, TRUE);

  /* Закрываем проект и галс. */
  hyscan_db_close (priv->db, priv->track_id);
  hyscan_db_close (priv->db, priv->project_id);

  g_free (priv->uri);
  g_free (priv->project_name);
  g_free (priv->track_name);
  g_free (priv->cache_prefix);
  g_free (priv->cache_key);
  g_free (priv->detail_key);
  g_free (priv->param_prefix);

  /* Вызываем деструктор родительского класса. */
  G_OBJECT_CLASS (hyscan_location_parent_class)->finalize (object);
}

/* Функция установки начальной точки для перевода координат в топоцентрическую СК и обратно. */
static void
hyscan_location_geo_initialize (HyScanLocation *location,
                                gboolean        uninit)
{
  HyScanLocationPrivate *priv = location->priv;
  gint32 index;
  gchar *buffer;
  gint32 buffer_size;
  gint64 db_time;

  HyScanGeoGeodetic bla0;
  HyScanGeoEllipsoidType ellipse_type = 0;

  HyScanLocationInternalData *first_point;
  HyScanLocationSourcesList *source_info;

  if (priv->local_caches.latlong_cache == NULL || priv->source_list == NULL)
    {
      return;
    }
  else
    {
      source_info = &g_array_index (priv->source_list, HyScanLocationSourcesList, priv->source_indices.latlong_source);
      first_point = &g_array_index (priv->local_caches.latlong_cache, HyScanLocationInternalData, 0);
    }


  if (uninit)
    {
      /* Сбрасываем флаг инициализации и выходим из функции.*/
      hyscan_geo_ready (HYSCAN_GEO (location), TRUE);
      return;
    }

  hyscan_location_get_range (location, &index, NULL);
  if (!hyscan_db_channel_get_data (priv->db, source_info->channel_id, index, NULL, &buffer_size, NULL))
    return;
  buffer = g_malloc0 (buffer_size * sizeof(gchar));
  hyscan_db_channel_get_data (priv->db, source_info->channel_id, index, buffer, &buffer_size, &db_time);

  /* Теперь смотрим на следующие 2 символа: GP, GN, GL. */
  if (g_pattern_match_simple ("?GP*", buffer) || g_pattern_match_simple ("?GN*", buffer))
    ellipse_type = HYSCAN_GEO_ELLIPSOID_WGS84;
  if (g_pattern_match_simple ("?GL*", buffer))
    ellipse_type = HYSCAN_GEO_ELLIPSOID_PZ90;

  if (first_point->validity == HYSCAN_LOCATION_ASSEMBLED    ||
      first_point->validity == HYSCAN_LOCATION_PREPROCESSED ||
      first_point->validity == HYSCAN_LOCATION_PROCESSED    ||
      first_point->validity == HYSCAN_LOCATION_USER_VALID   ||
      first_point->validity == HYSCAN_LOCATION_VALID)
     {
       bla0.lat = first_point->int_latitude;
       bla0.lon = first_point->int_longitude;
       bla0.h = 0;
     }
  hyscan_geo_set_origin (HYSCAN_GEO (location), bla0, ellipse_type);

  g_free (buffer);
}

/* Внутренняя функция. Составляет список источников. Вызывается один раз в конструкторе. */
static void
hyscan_location_source_list_int (HyScanLocation *location)
{
  HyScanLocationPrivate *priv = location->priv;
  gint i = 0;
  gchar **channel_list;
  gint index = 0;

  HyScanSourceType data_type = 0;
  gboolean hi_res = FALSE;
  gboolean raw = FALSE;
  guint sensor_channel = 0;
  guint number_of_channels = 0;

  /* Получаем список КД и подсчитываем их количество. */
  channel_list = hyscan_db_channel_list (priv->db, priv->track_id);

  while (channel_list != NULL && channel_list[number_of_channels] != NULL)
    number_of_channels++;

  /* Составляем таблицу тех КД, которые этот объект умеет обрабатывать. */
  priv->source_list = g_array_new (TRUE, TRUE, sizeof(HyScanLocationSourcesList));
  g_array_set_clear_func (priv->source_list, (GDestroyNotify)(hyscan_location_source_free));

  for (i = 0, index = 0; i < number_of_channels; i++)
  {
    hyscan_channel_get_types_by_name (channel_list[i], &data_type, &raw, &sensor_channel);

    /* Пропускаем этот источник, если он выдает сырые данные. */

    switch (data_type)
      {
      case HYSCAN_SOURCE_NMEA_RMC:
        hyscan_location_source_list_add (priv->source_list, index, HYSCAN_LOCATION_PARAMETER_LATLONG, HYSCAN_LOCATION_SOURCE_NMEA, sensor_channel, channel_list, i);
        index++;
        hyscan_location_source_list_add (priv->source_list, index, HYSCAN_LOCATION_PARAMETER_TRACK, HYSCAN_LOCATION_SOURCE_NMEA, sensor_channel, channel_list, i);
        index++;
        hyscan_location_source_list_add (priv->source_list, index, HYSCAN_LOCATION_PARAMETER_TRACK, HYSCAN_LOCATION_SOURCE_NMEA_COMPUTED, sensor_channel, channel_list, i);
        index++;
        hyscan_location_source_list_add (priv->source_list, index, HYSCAN_LOCATION_PARAMETER_SPEED, HYSCAN_LOCATION_SOURCE_NMEA, sensor_channel, channel_list, i);
        index++;
        hyscan_location_source_list_add (priv->source_list, index, HYSCAN_LOCATION_PARAMETER_SPEED, HYSCAN_LOCATION_SOURCE_NMEA_COMPUTED, sensor_channel, channel_list, i);
        index++;
        hyscan_location_source_list_add (priv->source_list, index, HYSCAN_LOCATION_PARAMETER_DATETIME, HYSCAN_LOCATION_SOURCE_NMEA, sensor_channel, channel_list, i);
        index++;
        break;

      case HYSCAN_SOURCE_NMEA_GGA:
        hyscan_location_source_list_add (priv->source_list, index, HYSCAN_LOCATION_PARAMETER_ALTITUDE, HYSCAN_LOCATION_SOURCE_NMEA, sensor_channel, channel_list, i);
        index++;
        break;

      case HYSCAN_SOURCE_ECHOSOUNDER:
        hyscan_location_source_list_add (priv->source_list, index, HYSCAN_LOCATION_PARAMETER_DEPTH, HYSCAN_LOCATION_SOURCE_ECHOSOUNDER, sensor_channel, channel_list, i);
        index++;
        break;

      case HYSCAN_SOURCE_SIDE_SCAN_PORT:

        if (hi_res == TRUE)
          hyscan_location_source_list_add (priv->source_list, index, HYSCAN_LOCATION_PARAMETER_DEPTH, HYSCAN_LOCATION_SOURCE_SONAR_HIRES_PORT, sensor_channel, channel_list, i);
        if (hi_res == FALSE)
          hyscan_location_source_list_add (priv->source_list, index, HYSCAN_LOCATION_PARAMETER_DEPTH, HYSCAN_LOCATION_SOURCE_SONAR_PORT, sensor_channel, channel_list, i);
        index++;
        break;

      case HYSCAN_SOURCE_SIDE_SCAN_STARBOARD:

        if (hi_res == TRUE)
          hyscan_location_source_list_add (priv->source_list, index, HYSCAN_LOCATION_PARAMETER_DEPTH, HYSCAN_LOCATION_SOURCE_SONAR_HIRES_STARBOARD, sensor_channel, channel_list, i);
        if (hi_res == FALSE)
          hyscan_location_source_list_add (priv->source_list, index, HYSCAN_LOCATION_PARAMETER_DEPTH, HYSCAN_LOCATION_SOURCE_SONAR_STARBOARD, sensor_channel, channel_list, i);
        index++;
        break;

      case HYSCAN_SOURCE_NMEA_DPT:
        hyscan_location_source_list_add (priv->source_list, index, HYSCAN_LOCATION_PARAMETER_DEPTH, HYSCAN_LOCATION_SOURCE_NMEA, sensor_channel, channel_list, i);
        index++;
        break;

      default:
        break;
      }
  }

  g_strfreev (channel_list);
}

/* Внутренняя функция. Устанавливает источники по умолчанию. Вызывается один раз в конструкторе. */
static void
hyscan_location_source_defaults (HyScanLocation *location)
{
  guint i = 0;

  HyScanLocationPrivate *priv = location->priv;
  HyScanLocationSourcesList *source_info;

  /* Выбираем КД по умолчанию.
   * Параметры скорость и курс по умолчанию высчитываются из координат, а не берутся из строк.
   */

  /* HYSCAN_LOCATION_PARAMETER_LATLONG. */
  for (i = 0; i < priv->source_list->len && priv->source_indices.latlong_source == -1; i++)
    {
      source_info = &g_array_index (priv->source_list, HyScanLocationSourcesList, i);
      if (SOURCE_LIST_CHECK (HYSCAN_LOCATION_SOURCE_NMEA,HYSCAN_LOCATION_PARAMETER_LATLONG))
        hyscan_location_source_set (location, i, TRUE);
    }

  /* HYSCAN_LOCATION_PARAMETER_ALTITUDE. */
  for (i = 0; i < priv->source_list->len && priv->source_indices.altitude_source == -1; i++)
    {
      source_info = &g_array_index (priv->source_list, HyScanLocationSourcesList, i);
      if (SOURCE_LIST_CHECK (HYSCAN_LOCATION_SOURCE_NMEA_COMPUTED, HYSCAN_LOCATION_PARAMETER_ALTITUDE))
        hyscan_location_source_set (location, i, TRUE);
    }
  for (i = 0; i < priv->source_list->len && priv->source_indices.altitude_source == -1; i++)
    {
      source_info = &g_array_index (priv->source_list, HyScanLocationSourcesList, i);
      if (SOURCE_LIST_CHECK (HYSCAN_LOCATION_SOURCE_NMEA, HYSCAN_LOCATION_PARAMETER_ALTITUDE))
        hyscan_location_source_set (location, i, TRUE);
    }
  for (i = 0; i < priv->source_list->len && priv->source_indices.altitude_source == -1; i++)
    {
      source_info = &g_array_index (priv->source_list, HyScanLocationSourcesList, i);
      if (SOURCE_LIST_CHECK (HYSCAN_LOCATION_SOURCE_ZERO, HYSCAN_LOCATION_PARAMETER_ALTITUDE))
        hyscan_location_source_set (location, i, TRUE);
    }

  /* HYSCAN_LOCATION_PARAMETER_TRACK. */
  for (i = 0; i < priv->source_list->len && priv->source_indices.track_source == -1; i++)
    {
      source_info = &g_array_index (priv->source_list, HyScanLocationSourcesList, i);
      if (SOURCE_LIST_CHECK (HYSCAN_LOCATION_SOURCE_NMEA_COMPUTED, HYSCAN_LOCATION_PARAMETER_TRACK))
        hyscan_location_source_set (location, i, TRUE);
    }
  for (i = 0; i < priv->source_list->len && priv->source_indices.track_source == -1; i++)
    {
      source_info = &g_array_index (priv->source_list, HyScanLocationSourcesList, i);
      if (SOURCE_LIST_CHECK (HYSCAN_LOCATION_SOURCE_NMEA, HYSCAN_LOCATION_PARAMETER_TRACK))
        hyscan_location_source_set (location, i, TRUE);
    }
  for (i = 0; i < priv->source_list->len && priv->source_indices.track_source == -1; i++)
    {
      source_info = &g_array_index (priv->source_list, HyScanLocationSourcesList, i);
      if (SOURCE_LIST_CHECK (HYSCAN_LOCATION_SOURCE_ZERO, HYSCAN_LOCATION_PARAMETER_TRACK))
        hyscan_location_source_set (location, i, TRUE);
    }

  /* HYSCAN_LOCATION_PARAMETER_ROLL. */
  for (i = 0; i < priv->source_list->len && priv->source_indices.roll_source == -1; i++)
    {
      source_info = &g_array_index (priv->source_list, HyScanLocationSourcesList, i);
      if (SOURCE_LIST_CHECK (HYSCAN_LOCATION_SOURCE_ZERO, HYSCAN_LOCATION_PARAMETER_ROLL))
        hyscan_location_source_set (location, i, TRUE);
    }

  /* HYSCAN_LOCATION_PARAMETER_PITCH. */
  for (i = 0; i < priv->source_list->len && priv->source_indices.pitch_source == -1; i++)
    {
      source_info = &g_array_index (priv->source_list, HyScanLocationSourcesList, i);
      if (SOURCE_LIST_CHECK (HYSCAN_LOCATION_SOURCE_ZERO, HYSCAN_LOCATION_PARAMETER_PITCH))
        hyscan_location_source_set (location, i, TRUE);
    }

  /* HYSCAN_LOCATION_PARAMETER_SPEED. */
  for (i = 0; i < priv->source_list->len && priv->source_indices.speed_source == -1; i++)
    {
      source_info = &g_array_index (priv->source_list, HyScanLocationSourcesList, i);
      if (SOURCE_LIST_CHECK (HYSCAN_LOCATION_SOURCE_NMEA_COMPUTED, HYSCAN_LOCATION_PARAMETER_SPEED))
        hyscan_location_source_set (location, i, TRUE);
    }
  for (i = 0; i < priv->source_list->len && priv->source_indices.speed_source == -1; i++)
    {
      source_info = &g_array_index (priv->source_list, HyScanLocationSourcesList, i);
      if (SOURCE_LIST_CHECK (HYSCAN_LOCATION_SOURCE_NMEA, HYSCAN_LOCATION_PARAMETER_SPEED))
        hyscan_location_source_set (location, i, TRUE);
    }
  for (i = 0; i < priv->source_list->len && priv->source_indices.speed_source == -1; i++)
    {
      source_info = &g_array_index (priv->source_list, HyScanLocationSourcesList, i);
      if (SOURCE_LIST_CHECK (HYSCAN_LOCATION_SOURCE_ZERO, HYSCAN_LOCATION_PARAMETER_SPEED))
        hyscan_location_source_set (location, i, TRUE);
    }

  /* HYSCAN_LOCATION_PARAMETER_DEPTH. */
  for (i = 0; i < priv->source_list->len && priv->source_indices.depth_source == -1; i++)
    {
      source_info = &g_array_index (priv->source_list, HyScanLocationSourcesList, i);
      if (SOURCE_LIST_CHECK (HYSCAN_LOCATION_SOURCE_ECHOSOUNDER, HYSCAN_LOCATION_PARAMETER_DEPTH))
        hyscan_location_source_set (location, i, TRUE);
    }
  for (i = 0; i < priv->source_list->len && priv->source_indices.depth_source == -1; i++)
    {
      source_info = &g_array_index (priv->source_list, HyScanLocationSourcesList, i);
      if (source_info->parameter == HYSCAN_LOCATION_PARAMETER_DEPTH             &&(
          source_info->source_type == HYSCAN_LOCATION_SOURCE_ECHOSOUNDER        ||
          source_info->source_type == HYSCAN_LOCATION_SOURCE_SONAR_PORT         ||
          source_info->source_type == HYSCAN_LOCATION_SOURCE_SONAR_STARBOARD    ||
          source_info->source_type == HYSCAN_LOCATION_SOURCE_SONAR_HIRES_PORT   ||
          source_info->source_type == HYSCAN_LOCATION_SOURCE_SONAR_HIRES_STARBOARD))
        hyscan_location_source_set (location, i, TRUE);
    }
  for (i = 0; i < priv->source_list->len && priv->source_indices.depth_source == -1; i++)
    {
      source_info = &g_array_index (priv->source_list, HyScanLocationSourcesList, i);
      if (SOURCE_LIST_CHECK (HYSCAN_LOCATION_SOURCE_ZERO, HYSCAN_LOCATION_PARAMETER_DEPTH))
        hyscan_location_source_set (location, i, TRUE);
    }

  /* HYSCAN_LOCATION_PARAMETER_DATETIME. */
  for (i = 0; i < priv->source_list->len && priv->source_indices.datetime_source == -1; i++)
    {
      source_info = &g_array_index (priv->source_list, HyScanLocationSourcesList, i);
      if (SOURCE_LIST_CHECK (HYSCAN_LOCATION_SOURCE_NMEA, HYSCAN_LOCATION_PARAMETER_DATETIME))
        hyscan_location_source_set (location, i, TRUE);
    }
}
/* Функция отдает список источников для заданного параметра. */
HyScanLocationSources **
hyscan_location_source_list (HyScanLocation *location,
                             gint            parameter)
{
  HyScanLocationPrivate *priv;
  HyScanLocationSourcesList *source_info;
  HyScanLocationSources **output_source_list;

  g_return_val_if_fail (HYSCAN_IS_LOCATION (location), NULL);

  priv = location->priv;

  guint i = 0,
        j = 0;

  if (priv->db == NULL)
    return NULL;

  for (i = 0; i < priv->source_list->len; i++)
    {
      source_info = &g_array_index (priv->source_list, HyScanLocationSourcesList, i);
      if (source_info->parameter == parameter)
        j++;
    }

  output_source_list = g_malloc0_n (j+1, sizeof(gpointer));
  for (i = 0, j = 0; i < priv->source_list->len; i++)
    {
      source_info = &g_array_index (priv->source_list, HyScanLocationSourcesList, i);
      if (source_info->parameter == parameter)
        {
          output_source_list[j] = g_malloc0 (sizeof(HyScanLocationSources));
          output_source_list[j]->index = source_info->index;
          output_source_list[j]->source_type = source_info->source_type;
          output_source_list[j]->sensor_channel = source_info->sensor_channel;
          j++;
        }
    }

  output_source_list[j] = NULL;

  return output_source_list;
}

void
hyscan_location_source_list_free (HyScanLocationSources ***data)
{
  HyScanLocationSources **d = *data;
  guint j = 0;
  while (d[j] != NULL)
    g_clear_pointer (&(d[j++]), g_free);

  g_free (*data);
  *data = NULL;
}

/* Функция возвращает индекс активного источника для заданного параметра. */
gint
hyscan_location_source_get (HyScanLocation *location,
                            gint            parameter)
{

  HyScanLocationPrivate *priv;
  HyScanLocationSourcesList *source_info;
  guint i = 0;

  g_return_val_if_fail (HYSCAN_IS_LOCATION (location), 0);
  priv = location->priv;

  if (priv->db == NULL)
    return -1;

  for (i = 0; i < priv->source_list->len; i++)
    {
      source_info = &g_array_index (priv->source_list, HyScanLocationSourcesList, i);
      if (source_info->parameter == parameter && source_info->active == TRUE)
        return source_info->index;
    }
  return -1;
}

/* Функция задает источник. */
gboolean
hyscan_location_source_set (HyScanLocation *location,
                            gint            source,
                            gboolean        turn_on)
{
  HyScanLocationPrivate *priv;

  HyScanLocationSourcesList *source_info;
  HyScanLocationParameters parameter;
  GArray *param_cache;
  gint32 *param_source;
  HyScanLocationSourceTypes source_type;
  HyScanAcousticDataInfo dc_info;
  HyScanAntennaPosition dc_position;
  gboolean status = FALSE;

  g_return_val_if_fail (HYSCAN_IS_LOCATION (location), FALSE);
  priv = location->priv;

  if (priv->db == NULL)
    return FALSE;

  /* Ставим надзирателя на паузу. */
  if (priv->overseer.overseer_thread != NULL)
    {
      g_atomic_int_set (&(priv->overseer.overseer_pause), 1);
      while (g_atomic_int_get (&(priv->overseer.overseer_paused)) != 1)
        g_usleep (priv->overseer.overseer_period);
    }

  source_info = &g_array_index (priv->source_list, HyScanLocationSourcesList, source);
  parameter = source_info->parameter;

  /* Устанавливаем внутренние указатели на кэш и источник. */
  switch (parameter)
    {
    case HYSCAN_LOCATION_PARAMETER_LATLONG:
      param_cache = priv->local_caches.latlong_cache;
      param_source = &(priv->source_indices.latlong_source);
      break;

    case HYSCAN_LOCATION_PARAMETER_ALTITUDE:
      param_cache = priv->local_caches.altitude_cache;
      param_source = &(priv->source_indices.altitude_source);
      break;

    case HYSCAN_LOCATION_PARAMETER_TRACK:
      param_cache = priv->local_caches.track_cache;
      param_source = &(priv->source_indices.track_source);
      break;

    case HYSCAN_LOCATION_PARAMETER_SPEED:
      param_cache = priv->local_caches.speed_cache;
      param_source = &(priv->source_indices.speed_source);
      break;

    case HYSCAN_LOCATION_PARAMETER_ROLL:
      param_cache = priv->local_caches.roll_cache;
      param_source = &(priv->source_indices.roll_source);
      break;

    case HYSCAN_LOCATION_PARAMETER_PITCH:
      param_cache = priv->local_caches.pitch_cache;
      param_source = &(priv->source_indices.pitch_source);
      break;

    case HYSCAN_LOCATION_PARAMETER_DEPTH:
      param_cache = priv->local_caches.depth_cache;
      param_source = &(priv->source_indices.depth_source);
      break;

    case HYSCAN_LOCATION_PARAMETER_DATETIME:
      param_cache = priv->local_caches.datetime_cache;
      param_source = &(priv->source_indices.datetime_source);
      break;

    default:
      status = FALSE;
      goto exit;
    }

  /* Закрываем предыдущий источник, если он используется. */
  if (*param_source != -1)
    {
      source_info = &g_array_index (priv->source_list, HyScanLocationSourcesList, *param_source);
      hyscan_db_close (priv->db, source_info->channel_id);
      g_array_free (param_cache, TRUE);

      source_info->channel_id = 0;
      source_info->active = FALSE;

      *param_source = -1;
    }
  /* Если нас просят исключить этот параметр из обработки, ставим соответствующий индекс источника в -1. */
  if (turn_on)
    {
      *param_source = source;
    }
  else
    {
      *param_source = -1;
      status = TRUE;
      goto exit;
    }

  source_info = &g_array_index (priv->source_list, HyScanLocationSourcesList, *param_source);

  /* Открываем КД. */
  source_type = source_info->source_type;
  switch (source_type)
    {
    case HYSCAN_LOCATION_SOURCE_NMEA:
    case HYSCAN_LOCATION_SOURCE_NMEA_COMPUTED:
      source_info->channel_id = hyscan_db_channel_open (priv->db,
                                                        priv->track_id,
                                                        source_info->channel_name);
      break;
    case HYSCAN_LOCATION_SOURCE_ECHOSOUNDER:
    case HYSCAN_LOCATION_SOURCE_SONAR_PORT:
    case HYSCAN_LOCATION_SOURCE_SONAR_STARBOARD:
    case HYSCAN_LOCATION_SOURCE_SONAR_HIRES_PORT:
    case HYSCAN_LOCATION_SOURCE_SONAR_HIRES_STARBOARD:
      source_info->dchannel = hyscan_data_channel_new (priv->db,
                                                       priv->project_name,
                                                       priv->track_name,
                                                       source_info->channel_name);
      break;
    default:
      status = FALSE;
      goto exit;
    }

  /* Инициализируем локальный кэш. */
  switch (parameter)
    {
    case HYSCAN_LOCATION_PARAMETER_LATLONG:
      priv->local_caches.latlong_cache = g_array_new (FALSE, TRUE, sizeof(HyScanLocationInternalData));
      hyscan_location_geo_initialize (location, TRUE);
      break;

    case HYSCAN_LOCATION_PARAMETER_ALTITUDE:
      priv->local_caches.altitude_cache = g_array_new (FALSE, TRUE, sizeof(HyScanLocationInternalData));

      break;

    case HYSCAN_LOCATION_PARAMETER_TRACK:
      if (source_type == HYSCAN_LOCATION_SOURCE_NMEA_COMPUTED)
        priv->local_caches.track_cache = g_array_new (FALSE, TRUE, sizeof(HyScanLocationInternalData));
      else
        priv->local_caches.track_cache = g_array_new (FALSE, TRUE, sizeof(HyScanLocationInternalData));
      break;

    case HYSCAN_LOCATION_PARAMETER_SPEED:
      if (source_type == HYSCAN_LOCATION_SOURCE_NMEA_COMPUTED)
        priv->local_caches.speed_cache = g_array_new (FALSE, TRUE, sizeof(HyScanLocationInternalData));
      else
        priv->local_caches.speed_cache = g_array_new (FALSE, TRUE, sizeof(HyScanLocationInternalData));
      break;

    case HYSCAN_LOCATION_PARAMETER_ROLL:
      priv->local_caches.roll_cache = g_array_new (FALSE, TRUE, sizeof(HyScanLocationInternalData));
      break;

    case HYSCAN_LOCATION_PARAMETER_PITCH:
      priv->local_caches.pitch_cache = g_array_new (FALSE, TRUE, sizeof(HyScanLocationInternalData));
      break;

    case HYSCAN_LOCATION_PARAMETER_DEPTH:
      priv->local_caches.depth_cache = g_array_new (FALSE, TRUE, sizeof(HyScanLocationInternalData));
      break;

    case HYSCAN_LOCATION_PARAMETER_DATETIME:
      priv->local_caches.datetime_cache = g_array_new (FALSE, TRUE, sizeof(HyScanLocationInternalTime));
      break;

    default:
      status = FALSE;
      goto exit;
    }

  /* Загружаем параметры КД. */
  switch (source_type)
    {
    case HYSCAN_LOCATION_SOURCE_NMEA:
    case HYSCAN_LOCATION_SOURCE_NMEA_COMPUTED:
      source_info->param_id = hyscan_db_channel_param_open (priv->db, source_info->channel_id);
      status = hyscan_location_param_load (location, *param_source);
      if (!status)
      {
        source_info->x = 0;
        source_info->y = 0;
        source_info->z = 0;
        source_info->psi = 0;
        source_info->gamma = 0;
        source_info->theta = 0;
        source_info->data_rate = 0;
      }
      break;
    case HYSCAN_LOCATION_SOURCE_ECHOSOUNDER:
    case HYSCAN_LOCATION_SOURCE_SONAR_PORT:
    case HYSCAN_LOCATION_SOURCE_SONAR_STARBOARD:
    case HYSCAN_LOCATION_SOURCE_SONAR_HIRES_PORT:
    case HYSCAN_LOCATION_SOURCE_SONAR_HIRES_STARBOARD:
      dc_info = hyscan_data_channel_get_info (source_info->dchannel);
      dc_position = hyscan_data_channel_get_position (source_info->dchannel);
      source_info->x = dc_position.x;
      source_info->y = dc_position.y;
      source_info->z = dc_position.z;
      source_info->psi = dc_position.psi;
      source_info->gamma = dc_position.gamma;
      source_info->theta = dc_position.theta;
      source_info->data_rate = dc_info.data.rate;
      break;
    default:
      status = FALSE;
      goto exit;
    }

  source_info->active = TRUE;
  status = TRUE;
  g_atomic_int_inc (&priv->mod_count);

  exit:
  /* Снимаем надзирателя с паузы. */
  if (priv->overseer.overseer_thread != NULL)
    {
      g_atomic_int_set (&(priv->overseer.overseer_pause), 0);
      while (g_atomic_int_get (&(priv->overseer.overseer_paused)) != 0)
        g_usleep (priv->overseer.overseer_period);
    }

  return status;
}

/* Функция загружает параметры КД. */
static gboolean
hyscan_location_param_load (HyScanLocation *location,
                            gint32          source)
{
  HyScanLocationPrivate *priv = location->priv;
  HyScanLocationSourcesList *source_info;
  source_info = &g_array_index (priv->source_list, HyScanLocationSourcesList, source);

  /* Загружаем параметры источника. */
  if (!hyscan_db_param_get_double (priv->db, source_info->param_id, NULL, "/position/x", &(source_info->x)))
    return FALSE;
  if (!hyscan_db_param_get_double (priv->db, source_info->param_id, NULL, "/position/y", &(source_info->y)))
    return FALSE;
  if (!hyscan_db_param_get_double (priv->db, source_info->param_id, NULL, "/position/z", &(source_info->z)))
    return FALSE;
  if (!hyscan_db_param_get_double (priv->db, source_info->param_id, NULL, "/position/psi", &(source_info->psi)))
    return FALSE;
  if (!hyscan_db_param_get_double (priv->db, source_info->param_id, NULL, "/position/gamma", &(source_info->gamma)))
    return FALSE;
  if (!hyscan_db_param_get_double (priv->db, source_info->param_id, NULL, "/position/theta", &(source_info->theta)))
    return FALSE;

  return TRUE;
}

/* Функция установки таблицы скорости звука. */
gboolean
hyscan_location_soundspeed_set (HyScanLocation *location,
                                GArray          soundspeedtable)
{

  HyScanLocationPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_LOCATION (location), FALSE);
  priv = location->priv;

  priv->soundspeedtable = soundspeedtable;

  if (priv->db == NULL)
    return FALSE;

  g_atomic_int_inc (&priv->mod_count);
  return TRUE;
}

/* Функция-надзиратель. */
static gpointer
hyscan_location_overseer (gpointer user_data)
{
  gint32 i;
  HyScanLocation *location = user_data;
  HyScanLocationPrivate *priv = location->priv;

  HyScanLocationSourcesList *source_info = NULL;

  guint64 param_mod_count = 0,
          new_param_mod_count = 0;

  gchar **param_list;
  HyScanLocationUserParameters param_info;

  gboolean latlong_changed = TRUE,
           track_changed = TRUE;

  /* Сигнализируем, что поток запустился. */
  g_atomic_int_set (&(priv->overseer.overseer_started), 1);
  if (priv->db == NULL)
    {
      g_atomic_int_set (&(priv->overseer.overseer_stop), 1);
      return NULL;
    }

  /* Поток работает до тех пор, пока не будет установлен флаг остановки. */
  while (g_atomic_int_get (&(priv->overseer.overseer_stop)) != 1)
    {
      /* Если надо поставить обработку на паузу. */
      if (g_atomic_int_get (&(priv->overseer.overseer_pause)) == 1)
        {
          g_atomic_int_set (&(priv->overseer.overseer_paused), 1);
          while (g_atomic_int_get (&(priv->overseer.overseer_pause)) != 0)
            g_usleep (priv->overseer.overseer_period);
          g_atomic_int_set (&(priv->overseer.overseer_paused), 0);
        }

      /* Важно: hyscan_location_overseer_datetime должен выполняться первым, т.к. все остальные методы используют его для сдвижки данных во времени. */

      /* Ставим блокировку, чтобы никто не прочитал негодные данные.*/
      g_mutex_lock (&priv->lock);
      hyscan_location_overseer_datetime (priv->db,
                                         priv->source_list,
                                         &(priv->local_caches),
                                         &(priv->source_indices),
                                         priv->quality);
      hyscan_location_overseer_latlong  (priv->db,
                                         priv->source_list,
                                         priv->user_params,
                                         &(priv->local_caches),
                                         &(priv->source_indices),
                                         priv->quality);
      hyscan_location_overseer_altitude (priv->db,
                                         priv->source_list,
                                         &(priv->local_caches),
                                         &(priv->source_indices),
                                         priv->quality);
      hyscan_location_overseer_track    (priv->db,
                                         priv->source_list,
                                         priv->user_params,
                                         &(priv->local_caches),
                                         &(priv->source_indices),
                                         priv->quality);
      hyscan_location_overseer_roll     (priv->db,
                                         priv->source_list,
                                         priv->user_params,
                                         &(priv->local_caches),
                                         &(priv->source_indices),
                                         priv->quality);
      hyscan_location_overseer_pitch    (priv->db,
                                         priv->source_list,
                                         priv->user_params,
                                         &(priv->local_caches),
                                         &(priv->source_indices),
                                         priv->quality);
      hyscan_location_overseer_speed    (priv->db,
                                         priv->source_list,
                                         priv->user_params,
                                         &(priv->local_caches),
                                         &(priv->source_indices),
                                         priv->quality);
      hyscan_location_overseer_depth    (priv->db,
                                         priv->source_list,
                                         &(priv->local_caches),
                                         &(priv->source_indices),
                                         &(priv->soundspeedtable),
                                         priv->quality);

      /* Если нужно, устанавливаем начальную точку. */
      if (hyscan_geo_ready (HYSCAN_GEO (location), FALSE) == 0)
        {
          hyscan_location_geo_initialize (location, FALSE);
        }

      /* Снимаем блокировку. */
      g_mutex_unlock (&priv->lock);

      new_param_mod_count = hyscan_db_get_mod_count (priv->db, priv->param_id);
      if (new_param_mod_count != param_mod_count)
        {

          latlong_changed = FALSE;
          track_changed = FALSE;

          /* Сейчас данные будут значительно изменяться, поэтому ставим блокировку.*/
          g_mutex_lock (&priv->lock);

          while (param_mod_count != new_param_mod_count)
            {
              param_mod_count = hyscan_db_get_mod_count (priv->db, priv->param_id);
              g_usleep (priv->overseer.overseer_period);
              new_param_mod_count = hyscan_db_get_mod_count (priv->db, priv->param_id);
            }

          param_list = hyscan_db_param_object_list (priv->db, priv->param_id);
          if (param_list == NULL)
            {
              g_mutex_unlock (&priv->lock);
              continue;
            }
          priv->user_params = g_array_set_size (priv->user_params, 0);

          /* Заполняем новый список параметров. */
          for (i = 0; param_list[i] != NULL; i++)
            {
              param_info.type = 0;
              param_info.ltime = -1;
              param_info.rtime = -1;
              param_info.value1 = NAN;
              param_info.value2 = NAN;
              param_info.value3 = NAN;

              if (g_strrstr (param_list[i], "location-edit-latlong") != NULL)
                {
                  param_info.type = HYSCAN_LOCATION_EDIT_LATLONG;
                  hyscan_db_param_get_integer (priv->db, priv->param_id, param_list[i], "/time", &param_info.ltime);
                  hyscan_db_param_get_integer (priv->db, priv->param_id, param_list[i], "/time", &param_info.rtime);
                  hyscan_db_param_get_double (priv->db, priv->param_id, param_list[i], "/new-latitude", &param_info.value1);
                  hyscan_db_param_get_double (priv->db, priv->param_id, param_list[i], "/new-longitude", &param_info.value2);

                  g_array_append_val (priv->user_params, param_info);

                  latlong_changed = TRUE;

                  source_info = &g_array_index (priv->local_caches.track_cache, HyScanLocationSourcesList, priv->source_indices.track_source);
                  if (source_info->source_type == HYSCAN_LOCATION_SOURCE_NMEA_COMPUTED)
                    track_changed = TRUE;

                  continue;
                }

              if (g_strrstr (param_list[i], "location-bulk-remove") != NULL)
                {
                  param_info.type = HYSCAN_LOCATION_BULK_REMOVE;
                  hyscan_db_param_get_integer (priv->db, priv->param_id, param_list[i], "/ltime", &param_info.ltime);
                  hyscan_db_param_get_integer (priv->db, priv->param_id, param_list[i], "/rtime", &param_info.rtime);
                  g_array_append_val (priv->user_params, param_info);

                  latlong_changed = TRUE;

                  source_info = &g_array_index (priv->local_caches.track_cache, HyScanLocationSourcesList, priv->source_indices.track_source);
                  if (source_info->source_type == HYSCAN_LOCATION_SOURCE_NMEA_COMPUTED)
                    track_changed = TRUE;

                  continue;
                }
            }

          g_strfreev (param_list);

          /* Запускаем обработку данных. Устанавливаем shift в -1, чтобы заставить overseer
           * переинициализировать все индексы, после чего вызываем функцию-надзирателя для соответствующего параметра. */
          if (latlong_changed)
            {
              source_info = &g_array_index (priv->source_list, HyScanLocationSourcesList, priv->source_indices.latlong_source);
              source_info->shift = -1;
              hyscan_location_overseer_latlong (priv->db,
                                                priv->source_list,
                                                priv->user_params,
                                                &(priv->local_caches),
                                                &(priv->source_indices),
                                                priv->quality);
            }
          if (track_changed)
            {
              source_info = &g_array_index (priv->source_list, HyScanLocationSourcesList, priv->source_indices.track_source);
              source_info->shift = -1;
              hyscan_location_overseer_track (priv->db,
                                              priv->source_list,
                                              priv->user_params,
                                              &(priv->local_caches),
                                              &(priv->source_indices),
                                              priv->quality);
            }

          g_atomic_int_inc (&priv->mod_count);
          g_mutex_unlock (&priv->lock);
        }

      g_usleep (priv->overseer.overseer_period);
    }
  return NULL;
}

/* Функция возвращает значения, определенные параметром, в заданный момент времени. */
HyScanLocationData
hyscan_location_get (HyScanLocation *location,
                     gint            parameter,
                     gint64          time,
                     gdouble         x,
                     gdouble         y,
                     gdouble         z,
                     gdouble         psi,
                     gdouble         gamma,
                     gdouble         theta,
                     gboolean        topo)
{
  HyScanLocationPrivate *priv;
  HyScanLocationData output = {NAN,NAN,NAN,0,0,0,NAN,NAN,time,HYSCAN_LOCATION_VALID};

  HyScanGeoGeodetic geod_coordinates;
  HyScanGeoCartesian3D topo_coordinates;

  gdouble cache_status;

  HyScanLocationInternalData data = {0};
  HyScanLocationInternalTime temp3 = {0};

  HyScanLocationSourcesList *source_info;
  gdouble center_x = 0,
          center_y = 0,
          center_z = 0,
          center_psi = 0,
          center_gamma = 0,
          center_theta = 0;

  g_return_val_if_fail (HYSCAN_IS_LOCATION (location), output);
  priv = location->priv;

  if (priv->db == NULL)
    return output;

  /* Сначала ищем значение в кэше. */
  if (priv->cache != NULL)
    {
      cache_status = hyscan_location_update_cache_key (location, time);
      if (cache_status)
        {
          if (hyscan_location_cache_get (location, parameter, &output))
            return output;
        }
    }

  g_mutex_lock (&priv->lock);

  if ((parameter & HYSCAN_LOCATION_PARAMETER_LATLONG) == HYSCAN_LOCATION_PARAMETER_LATLONG)
    {
      data = hyscan_location_getter_latlong (priv->db, priv->source_list, priv->local_caches.latlong_cache, priv->source_indices.latlong_source, time, priv->quality)  ;
      output.latitude = data.int_latitude;
      output.longitude = data.int_longitude;
      if (data.validity != HYSCAN_LOCATION_VALID)
        goto fail;

      source_info = &g_array_index (priv->source_list, HyScanLocationSourcesList, priv->source_indices.latlong_source);
      center_x = source_info->x;
      center_y = source_info->y;
    }
  if ((parameter & HYSCAN_LOCATION_PARAMETER_ALTITUDE) == HYSCAN_LOCATION_PARAMETER_ALTITUDE)
    {
      data = hyscan_location_getter_altitude (priv->db, priv->source_list, priv->local_caches.altitude_cache, priv->source_indices.altitude_source, time, priv->quality);
      output.altitude = data.int_value;
      if (data.validity != HYSCAN_LOCATION_VALID)
        goto fail;

      source_info = &g_array_index (priv->source_list, HyScanLocationSourcesList, priv->source_indices.altitude_source);
      center_z = source_info->z;
    }
  if ((parameter & HYSCAN_LOCATION_PARAMETER_TRACK) == HYSCAN_LOCATION_PARAMETER_TRACK)
    {
      data = hyscan_location_getter_track (priv->db, priv->source_list, priv->local_caches.track_cache, priv->source_indices.track_source, time, priv->quality);
      output.track = data.int_value;

      source_info = &g_array_index (priv->source_list, HyScanLocationSourcesList, priv->source_indices.track_source);
      center_psi = source_info->psi;
      output.track += center_psi;

      if (data.validity != HYSCAN_LOCATION_VALID)
        goto fail;
    }
  if ((parameter & HYSCAN_LOCATION_PARAMETER_ROLL) == HYSCAN_LOCATION_PARAMETER_ROLL)
    {
      data = hyscan_location_getter_roll (priv->db, priv->source_list, priv->local_caches.roll_cache, priv->source_indices.roll_source, time, priv->quality);
      output.roll = data.int_value;

      source_info = &g_array_index (priv->source_list, HyScanLocationSourcesList, priv->source_indices.roll_source);
      center_gamma = source_info->gamma;
      output.roll += center_gamma;

      if (data.validity != HYSCAN_LOCATION_VALID)
        goto fail;
    }
  if ((parameter & HYSCAN_LOCATION_PARAMETER_PITCH) == HYSCAN_LOCATION_PARAMETER_PITCH)
    {
      data = hyscan_location_getter_pitch (priv->db, priv->source_list, priv->local_caches.pitch_cache, priv->source_indices.pitch_source, time, priv->quality);
      output.pitch = data.int_value;

      source_info = &g_array_index (priv->source_list, HyScanLocationSourcesList, priv->source_indices.pitch_source);
      center_theta = source_info->theta;
      output.pitch += center_theta;

      if (data.validity != HYSCAN_LOCATION_VALID)
        goto fail;
    }
  if ((parameter & HYSCAN_LOCATION_PARAMETER_SPEED) == HYSCAN_LOCATION_PARAMETER_SPEED)
    {
      data = hyscan_location_getter_speed (priv->db, priv->source_list, priv->local_caches.speed_cache, priv->source_indices.speed_source, time, priv->quality);
      output.speed = data.int_value;
      if (data.validity != HYSCAN_LOCATION_VALID)
        goto fail;
    }
  if ((parameter & HYSCAN_LOCATION_PARAMETER_DEPTH) == HYSCAN_LOCATION_PARAMETER_DEPTH)
    {
      data = hyscan_location_getter_depth (priv->db, priv->source_list, priv->local_caches.depth_cache, priv->source_indices.depth_source, time, priv->quality);
      output.depth = data.int_value;
      if (data.validity != HYSCAN_LOCATION_VALID)
        goto fail;
    }
  if ((parameter & HYSCAN_LOCATION_PARAMETER_DATETIME) == HYSCAN_LOCATION_PARAMETER_DATETIME)
    {
      temp3 = hyscan_location_getter_datetime (priv->db, priv->source_list, priv->local_caches.datetime_cache, priv->source_indices.datetime_source, time, priv->quality);
      if (temp3.validity != HYSCAN_LOCATION_VALID)
        goto fail;
    }

  /* Сдвигаем к центру масс. */
  hyscan_location_shift (&output, -center_x, -center_y, -center_z, (output.track)*G_PI/180.0, output.roll, output.pitch);

  /* Кладем значение в кэш. */
  if (priv->cache != NULL && cache_status && output.validity)
    hyscan_cache_set (priv->cache, priv->cache_key, priv->detail_key, &output, sizeof(HyScanLocationData));

  /* Сдвигаем к запрашиваемой точке. */
  hyscan_location_shift (&output, x, y, z, (output.track)*G_PI/180.0, output.roll, output.pitch);

  /* Переводим в топоцентрические координаты. */
  if (topo)
    {
      geod_coordinates.lat = output.latitude;
      geod_coordinates.lon = output.longitude;
      if (isnan (output.altitude))
        geod_coordinates.h = 0;
      else
        geod_coordinates.h = output.altitude;

      if (!hyscan_geo_geo2topo (HYSCAN_GEO (location), &topo_coordinates, geod_coordinates))
        goto fail;

      output.latitude = topo_coordinates.x;
      output.longitude = topo_coordinates.y;
      output.altitude = topo_coordinates.z;
    }

  /* Учет углов установки. */
  output.track += psi;
  output.roll += gamma;
  output.pitch += theta;
  g_mutex_unlock (&priv->lock);
  return output;

 fail:
  output.validity = HYSCAN_LOCATION_INVALID;
  g_mutex_unlock (&priv->lock);
  return output;
}

/* Функция возвращает диапазон сырых данных. */
gboolean
hyscan_location_get_range (HyScanLocation *location,
                           gint32         *lindex,
                           gint32         *rindex)
{
  HyScanLocationPrivate *priv;
  HyScanLocationSourcesList *source_info;

  gint32 data_range_first = 0,
         data_range_last = 0,
         size;


  g_return_val_if_fail (HYSCAN_IS_LOCATION (location), FALSE);
  priv = location->priv;

  source_info = &g_array_index (priv->source_list, HyScanLocationSourcesList, priv->source_indices.latlong_source);
  if (!hyscan_db_channel_get_data_range (priv->db, source_info->channel_id, &data_range_first, &data_range_last))
    return FALSE;
  if (lindex != NULL)
    {
      if (!hyscan_db_channel_get_data (priv->db, source_info->channel_id, *lindex, NULL, &size, NULL))
        return FALSE;
    }
  if (rindex != NULL)
    {
      if (!hyscan_db_channel_get_data (priv->db, source_info->channel_id, *rindex, NULL, &size, NULL))
        return FALSE;
    }

  return TRUE;
}

/* Функция возвращает сырые данные. */
HyScanLocationData
hyscan_location_get_raw_data (HyScanLocation *location,
                              gint32          index)
{
  HyScanLocationPrivate *priv;
  HyScanLocationSourcesList *source_info;

  HyScanLocationData output = {NAN,NAN,NAN,0,0,0,NAN,NAN,0,HYSCAN_LOCATION_INVALID};
  HyScanLocationInternalData latlong;

  gchar *buffer = NULL;
  gint32 buffer_size = 0;

  gint64 db_time = 0;

  g_return_val_if_fail (HYSCAN_IS_LOCATION (location), output);
  priv = location->priv;

  source_info = &g_array_index (priv->source_list, HyScanLocationSourcesList, priv->source_indices.latlong_source);

  hyscan_db_channel_get_data (priv->db, source_info->channel_id, index, NULL, &buffer_size, NULL);
  buffer = g_malloc0 (buffer_size * sizeof(gchar));
  hyscan_db_channel_get_data (priv->db, source_info->channel_id, index, buffer, &buffer_size, &db_time);

  latlong = hyscan_location_nmea_latlong_get (buffer);

  output.latitude = latlong.int_latitude;
  output.longitude = latlong.int_longitude;
  output.validity = latlong.validity;
  output.time = db_time;

  g_free (buffer);
  return output;
}
/* Функция возвращает счетчик изменений. */
gint64
hyscan_location_get_mod_count (HyScanLocation *location)
{
  HyScanLocationPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_LOCATION (location), 0);
  priv = location->priv;

  if (priv->db == NULL)
    return -1;

  return g_atomic_int_get (&(priv->mod_count));
}

/* Функция возвращает процент готовности данных. */
gint
hyscan_location_get_progress (HyScanLocation *location)
{
  HyScanLocationPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_LOCATION (location), 0);
  priv = location->priv;

  if (priv->db == NULL)
    return -1;

  return g_atomic_int_get (&(priv->progress));
}

/* Функция обновляет ключ для системы кэширования. */
static gboolean
hyscan_location_update_cache_key (HyScanLocation *location,
                                  gint64          time)
{
  HyScanLocationPrivate *priv = location->priv;

  /* Проверяем, используется ли система кэширования. */
  if (priv->cache == NULL)
    return FALSE;

  /* Если да, то обновляем ключ кеширования. */
  if (priv->cache_key == NULL)
    {
      if (priv->cache_prefix != NULL)
        {
          priv->cache_key = g_strdup_printf ("%s.%s.%s.%s.%ld",
                                             priv->uri,
                                             priv->cache_prefix,
                                             priv->project_name,
                                             priv->track_name,
                                             G_MAXINT64);
        }
      else
        {
          priv->cache_key = g_strdup_printf ("%s.%s.%s.%ld",
                                             priv->uri,
                                             priv->project_name,
                                             priv->track_name,
                                             G_MAXINT64);
        }
      priv->cache_key_length = strlen (priv->cache_key);
    }

  if (priv->cache_prefix)
    {
      g_snprintf (priv->cache_key, priv->cache_key_length, "%s.%s.%s.%s.%ld",
                  priv->uri,
                  priv->cache_prefix,
                  priv->project_name,
                  priv->track_name,
                  time);
    }
  else
    {
      g_snprintf (priv->cache_key, priv->cache_key_length, "%s.%s.%s.%ld",
                  priv->uri,
                  priv->project_name,
                  priv->track_name,
                  time);
    }

    /* Обновляем вспомогательную информацию для кэширования. В качестве этой величины используется mod_count.
     * Это не дает забрать из кэша данные, положенные туда при предыдущей комбинации параметров обработки.
     */
    if (priv->detail_key == NULL)
      {
        priv->detail_key = g_strdup_printf ("%u", G_MAXUINT);
        priv->detail_key_length = strlen (priv->detail_key);
      }
    else
      {
        g_snprintf (priv->detail_key, priv->detail_key_length, "%u", priv->mod_count);
      }

  return TRUE;
}

/* Функция получает значение из кэша. */
static gboolean
hyscan_location_cache_get (HyScanLocation     *location,
                           gint                parameter,
                           HyScanLocationData *data)
{
  HyScanLocationPrivate *priv = location->priv;
  gint32 buffer_size = sizeof(HyScanLocationData);

  if (!hyscan_cache_get (priv->cache, priv->cache_key, priv->detail_key, data, &buffer_size))
    return FALSE;

  /* Проверяем, что для каждого требуемого параметра есть данные в кэше.
   * Это сделано на тот случай, если в процессе работы пользователь
   * станет запрашивать другой набор параметров.
   */
  if ((parameter & HYSCAN_LOCATION_PARAMETER_LATLONG) == HYSCAN_LOCATION_PARAMETER_LATLONG)
    {
      if (isnan(data->latitude) || isnan(data->longitude))
        return FALSE;
    }
  if ((parameter & HYSCAN_LOCATION_PARAMETER_ALTITUDE) == HYSCAN_LOCATION_PARAMETER_ALTITUDE)
    {
      if (isnan(data->altitude))
        return FALSE;
    }
  if ((parameter & HYSCAN_LOCATION_PARAMETER_TRACK) == HYSCAN_LOCATION_PARAMETER_TRACK)
    {
      if (isnan(data->track))
          return FALSE;
    }
  if ((parameter & HYSCAN_LOCATION_PARAMETER_ROLL) == HYSCAN_LOCATION_PARAMETER_ROLL)
    {
      if (isnan(data->roll))
        return FALSE;
    }
  if ((parameter & HYSCAN_LOCATION_PARAMETER_PITCH) == HYSCAN_LOCATION_PARAMETER_PITCH)
    {
      if (isnan(data->pitch))
        return FALSE;
    }
  if ((parameter & HYSCAN_LOCATION_PARAMETER_SPEED) == HYSCAN_LOCATION_PARAMETER_SPEED)
    {
      if (isnan(data->speed))
        return FALSE;
    }
  if ((parameter & HYSCAN_LOCATION_PARAMETER_DEPTH) == HYSCAN_LOCATION_PARAMETER_DEPTH)
    {
      if (isnan(data->depth))
        return FALSE;
    }

  return TRUE;
}

/* Функция позволяет задать период работы потока-надзирателя. */
void
hyscan_location_overseer_period_set (HyScanLocation *location,
                                     gint32          overseer_period)
{
  HyScanLocationPrivate *priv;

  g_return_if_fail (HYSCAN_IS_LOCATION (location));
  priv = location->priv;

  priv->overseer.overseer_period = overseer_period;
  g_atomic_int_inc (&priv->mod_count);
}

/* Функция позволяет задать качество данных. */
void
hyscan_location_quality_set (HyScanLocation *location,
                             gdouble         quality)
{
  HyScanLocationPrivate *priv;

  g_return_if_fail (HYSCAN_IS_LOCATION (location));
  priv = location->priv;

  priv->quality = quality;
  g_atomic_int_inc (&priv->mod_count);
}

/* Функция освобождает элемент массива HyScanLocationSourcesList. */
static void
hyscan_location_source_free (HyScanLocationSourcesList *data)
{
  g_free (data->channel_name);
}

/* Функция добавляет новый элемент в список источников. */
static void
hyscan_location_source_list_add (GArray                   *source_list,
                                 gint32                    index,
                                 HyScanLocationParameters  parameter,
                                 HyScanLocationSourceTypes source_type,
                                 gint                      sensor_channel,
                                 gchar                   **channel_list,
                                 gint                      channel_list_index)
{
  HyScanLocationSourcesList new_source;

  new_source.index = index;
  new_source.parameter = parameter;
  new_source.source_type = source_type;
  new_source.sensor_channel = sensor_channel;
  new_source.active = FALSE;
  new_source.dchannel = NULL;
  if (channel_list != NULL)
    new_source.channel_name = g_strdup (channel_list[channel_list_index]);
  else
    new_source.channel_name = NULL;

  new_source.channel_id = 0;
  new_source.param_id = 0;
  new_source.shift = -1;
  new_source.assembler_index = 0;
  new_source.preprocessing_index = 0;
  new_source.thresholder_prev_index = 0;
  new_source.thresholder_next_index = 0;
  new_source.processing_index = 0;
  new_source.x = 0.0;
  new_source.y = 0.0;
  new_source.z = 0.0;
  new_source.psi = 0.0;
  new_source.gamma = 0.0;
  new_source.theta = 0.0;
  new_source.data_rate = 0.0;
  if (source_list != NULL)
    g_array_append_val (source_list, new_source);
};
