#include "hyscan-location.h"
#include "hyscan-location-tools.h"
#include <glib.h>

#define HYSCAN_LOCATIONDATA_NAN NAN,NAN,NAN,0,0,0,NAN,NAN,0,TRUE
/* Макрос заполняет строку списка каналов данных. */
#define SOURCE_LIST_ADD(ind,par,src_typ,sens_chan,cl_ind)             \
source_list_element.index = (ind);                                    \
source_list_element.parameter = (par);                                \
source_list_element.source_type = (src_typ);                          \
source_list_element.sensor_channel = (sens_chan);                     \
source_list_element.active = FALSE;                                   \
source_list_element.dchannel = 0;                                     \
source_list_element.channel_name = g_strdup (channel_list[(cl_ind)]); \
source_list_element.channel_id = 0;                                   \
source_list_element.param_id = 0;                                     \
source_list_element.shift = -1;                                       \
source_list_element.assembler_index = 0;                              \
source_list_element.processing_index = 0;                             \
source_list_element.discretization_type = NULL;                       \
source_list_element.x = 0.0;                                          \
source_list_element.y = 0.0;                                          \
source_list_element.z = 0.0;                                          \
source_list_element.psi = 0.0;                                        \
source_list_element.gamma = 0.0;                                      \
source_list_element.theta = 0.0;                                      \
source_list_element.discretization_frequency = 0.0;                   \
g_array_append_val (priv->source_list, source_list_element);

/* Макрос, используемый при установке источников по-умолчанию.
 * Написан исключительно с целью уменьшения количества повторяющегося кода. */
#define SOURCE_LIST_CHECK(ind,par)                                \
 (                                                                \
  source_list_element->source_type == default_source &&     \
  source_list_element->sensor_channel == default_channel && \
  source_list_element->parameter == (par)                   \
)

enum
{
  PROP_O,
  PROP_DB,
  PROP_CACHE,
  PROP_CACHE_PREFIX,
  PROP_PROJECT,
  PROP_TRACK,
  PROP_OVERSEER_PERIOD,
  PROP_Q
};

struct _HyScanLocationPrivate
{
  GObject              parent_instance;
  HyScanDB             *db;                             /* Интерфейс базы данных. */
  gchar                *uri;                            /* URI базы данных. */

  gchar                *project_name;                   /* Название проекта. */
  gchar                *track_name;                     /* Название галса. */
  gint32                project_id;                     /* Идентификатор проекта. */
  gint32                track_id;                       /* Идентификатор галса. */

  HyScanCache          *cache;                          /* Интерфейс системы кэширования. */
  gchar                *cache_prefix;                   /* Префикс ключа кэширования. */
  gchar                *cache_key;                      /* Ключ кэширования. */
  gint                  cache_key_length;               /* Максимальная длина ключа. */

  GArray               *latlong_cache;                  /* Локальный кэш координат. */
  GArray               *altitude_cache;                 /* Локальный кэш высоты. */
  GArray               *track_cache;                    /* Локальный кэш курса. */
  GArray               *roll_cache;                     /* Локальный кэш крена. */
  GArray               *pitch_cache;                    /* Локальный кэш дифферента. */
  GArray               *speed_cache;                    /* Локальный кэш скорости. */
  GArray               *depth_cache;                    /* Локальный кэш глубины. */
  GArray               *datetime_cache;                 /* Локальный кэш времени. */

  gint32                latlong_source;                 /* Индекс КД координат в source_list. */
  gint32                altitude_source;                /* Индекс КД высоты в source_list. */
  gint32                track_source;                   /* Индекс КД курса в source_list. */
  gint32                roll_source;                    /* Индекс КД крена в source_list. */
  gint32                pitch_source;                   /* Индекс КД дифферента в source_list. */
  gint32                speed_source;                   /* Индекс КД скорости в source_list. */
  gint32                depth_source;                   /* Индекс КД глубины в source_list. */
  gint32                datetime_source;                /* Индекс КД времени в source_list. */

  GArray               *source_list;                    /* Внутренний список источников. */

  GThread              *overseer_thread;                /* Поток надзирателя. */
  gint                  overseer_started;               /* Сигнализирует, что надзиратель запустился. */
  gint                  overseer_pause;
  gint                  overseer_paused;
  gint                  overseer_stop;                  /* Сигнализирует, что надзирателю пора остановиться. */
  gint32                overseer_period;                /* Время ожидания надзирателя. */

  GArray                soundspeedtable;
  gfloat                discretization_frequency;

  gint32                mod_count;
  gint32                progress;
  gdouble               quality;
  GMutex                lock;                          /* Блокировка многопоточного доступа. */
};

static void             hyscan_location_set_property    (GObject            *object,
                                                         guint               prop_id,
                                                         const GValue       *value,
                                                         GParamSpec         *pspec);
static void             hyscan_location_constructed     (GObject            *object);
static void             hyscan_location_finalize        (GObject            *object);
static void             hyscan_location_source_free     (HyScanLocationSourcesList *data);

static void             hyscan_location_source_list_int (HyScanLocation     *location);
static void             hyscan_location_source_defaults (HyScanLocation     *location);
GArray                 *hyscan_location_source_list     (HyScanLocation     *location,
                                                         gint                parameter);
gint                    hyscan_location_source_get      (HyScanLocation     *location,
                                                         gint                parameter);
gboolean                hyscan_location_source_set      (HyScanLocation     *location,
                                                         gint                source,
                                                         gboolean            turn_on);
static gboolean         hyscan_location_param_load      (HyScanLocation     *location,
                                                         gint32              source);
gboolean                hyscan_location_soundspeed_set  (HyScanLocation     *location,
                                                         GArray              soundspeedtable);

static gpointer         hyscan_location_overseer        (gpointer            user_data);

HyScanLocationData      hyscan_location_get             (HyScanLocation *location,
                                                         gint            parameter,
                                                         gint64          time,
                                                         gdouble         x,
                                                         gdouble         y,
                                                         gdouble         z,
                                                         gdouble         psi,
                                                         gdouble         gamma,
                                                         gdouble         theta);
static gboolean         hyscan_location_update_cache_key(HyScanLocation     *location,
                                                         gint64              time);
static gboolean         hyscan_location_cache_set       (HyScanLocation     *location,
                                                         HyScanLocationData *data);
static gboolean         hyscan_location_cache_get       (HyScanLocation     *location,
                                                         gint                parameter,
                                                         HyScanLocationData *data);

gint32                  hyscan_location_get_mod_count   (HyScanLocation *location);
gint                    hyscan_location_get_progress    (HyScanLocation *location);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanLocation, hyscan_location, G_TYPE_OBJECT);

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

  g_object_class_install_property ( object_class, PROP_PROJECT,
      g_param_spec_string ("project_name", "ProjectName", "project name", NULL,
        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_TRACK,
      g_param_spec_string ("track_name", "Track", "Track name", NULL,
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
      priv->db = g_value_get_object (value);
      if (priv->db != NULL)
        g_object_ref (priv->db);
      else
        G_OBJECT_WARN_INVALID_PROPERTY_ID (location, prop_id, pspec);
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

    case PROP_OVERSEER_PERIOD:
      priv->overseer_period = g_value_get_int64 (value);
      break;

    case PROP_Q:
      priv->quality = g_value_get_double (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (location, prop_id, pspec);
      break;
    }
}

static void
hyscan_location_constructed (GObject *object)
{
  HyScanLocation *location = HYSCAN_LOCATION (object);
  HyScanLocationPrivate *priv = location->priv;

  priv->overseer_thread = NULL;
  priv->overseer_pause = 0;
  priv->overseer_paused = 0;
  /* Открываем проект и галс. */
  priv->project_id = hyscan_db_project_open (priv->db, priv->project_name);
  priv->track_id = hyscan_db_track_open (priv->db, priv->project_id, priv->track_name);

  priv->latlong_source = -1;
  priv->altitude_source = -1;
  priv->track_source = -1;
  priv->roll_source = -1;
  priv->pitch_source = -1;
  priv->speed_source = -1;
  priv->depth_source = -1;
  priv->datetime_source = -1;
  /* Составляем список источников. */
  hyscan_location_source_list_int (location);
  /* Устанавливаем источники по умолчанию. */
  hyscan_location_source_defaults (location);

  //priv->overseer_period = 1000000;
  /* Запускаем поток надзирателя. */
  g_atomic_int_set (&(priv->overseer_stop), 0);
  priv->overseer_thread = g_thread_new ("overseer thread", hyscan_location_overseer, location);
  /* Ждем, пока поток не запустится. */
  while (g_atomic_int_get (&(priv->overseer_started)) != 1)
    g_usleep (1000);

  G_OBJECT_CLASS (hyscan_location_parent_class)->constructed (object);

}

HyScanLocation *
hyscan_location_new (HyScanDB    *db,
                     HyScanCache *cache,
                     gchar       *cache_prefix,
                     gchar       *project,
                     gchar       *track,
                     gdouble      quality)
{
  return g_object_new (HYSCAN_TYPE_LOCATION,
                       "db", db,
                       "cache", cache,
                       "cache_prefix", cache_prefix,
                       "project_name", project,
                       "track_name", track,
                       "quality", quality,
                       NULL);
}
static void
hyscan_location_finalize (GObject *object)
{
  HyScanLocation *location = HYSCAN_LOCATION (object);
  HyScanLocationPrivate *priv = location->priv;
  gint i;

  HyScanLocationSourcesList *source_list_element;
  /* Говорим надзирателю, что пора бы и остановиться, и закрываем соответствующий поток. */
  g_atomic_int_set (&(priv->overseer_stop), 1);
  g_thread_join (priv->overseer_thread);

  /* Очищаем кэши. */
  if (priv->latlong_cache != NULL)
    g_array_free (priv->latlong_cache, TRUE);
  priv->latlong_cache = NULL;
  if (priv->altitude_cache != NULL)
    g_array_free (priv->altitude_cache, TRUE);
  priv->altitude_cache = NULL;
  if (priv->track_cache != NULL)
    g_array_free (priv->track_cache, TRUE);
  priv->track_cache = NULL;
  if (priv->roll_cache != NULL)
    g_array_free (priv->roll_cache, TRUE);
  priv->roll_cache = NULL;
  if (priv->pitch_cache != NULL)
    g_array_free (priv->pitch_cache, TRUE);
  priv->pitch_cache = NULL;
  if (priv->speed_cache != NULL)
    g_array_free (priv->speed_cache, TRUE);
  priv->speed_cache = NULL;
  if (priv->depth_cache != NULL)
    g_array_free (priv->depth_cache, TRUE);
  priv->depth_cache = NULL;
  if (priv->datetime_cache != NULL)
    g_array_free (priv->datetime_cache, TRUE);
  priv->datetime_cache = NULL;


  /* Закрываем каналы данных. */
  for (i = 0; i < priv->source_list->len; i++)
    {
      source_list_element = &g_array_index (priv->source_list, HyScanLocationSourcesList, i);
      if (source_list_element->active == TRUE)
        {
          hyscan_db_close (priv->db, source_list_element->channel_id);
          hyscan_db_close (priv->db, source_list_element->param_id);
        }
    }

  /* Очищаем список источников. */
  g_array_free (priv->source_list, TRUE);

  /* Закрываем проект и галс. */
  hyscan_db_close (priv->db, priv->track_id);
  hyscan_db_close (priv->db, priv->project_id);

  G_OBJECT_CLASS (hyscan_location_parent_class)->finalize (object);
}

/* Внутренняя функция. Составляет список источников. Вызывается один раз в конструкторе. */
static void
hyscan_location_source_list_int (HyScanLocation *location)
{
  HyScanLocationPrivate *priv = location->priv;
  gint i = 0;
  gchar **channel_list;
  gint index = 0;

  HyScanSonarDataType data_type = 0;
  gboolean hi_res = FALSE;
  gboolean raw = FALSE;
  HyScanSonarChannelIndex sensor_channel = 0;
  gint number_of_channels = 0;

  HyScanLocationSourcesList source_list_element;

  /* Получаем список КД и подсчитываем их количество. */
  channel_list = hyscan_db_channel_list (priv->db, priv->track_id);

  while (channel_list != NULL && channel_list[number_of_channels] != NULL)
    number_of_channels++;

  /* Составляем таблицу тех КД, которые этот объект умеет обрабатывать. */
  priv->source_list = g_array_new (TRUE, TRUE, sizeof(HyScanLocationSourcesList));
  g_array_set_clear_func (priv->source_list, (GDestroyNotify)(hyscan_location_source_free));

  for (i = 0, index = 0; i < number_of_channels; i++)
  {
    hyscan_channel_get_types_by_name (channel_list[i], &data_type, &hi_res, &raw, &sensor_channel);

    /* Пропускаем этот источник, если он выдает сырые данные. */
    if (raw)
      continue;

    switch (data_type)
      {
      case HYSCAN_SONAR_DATA_NMEA_RMC:
        SOURCE_LIST_ADD(index, HYSCAN_LOCATION_PARAMETER_LATLONG, HYSCAN_LOCATION_SOURCE_NMEA, sensor_channel, i);
        index++;
        SOURCE_LIST_ADD(index, HYSCAN_LOCATION_PARAMETER_TRACK, HYSCAN_LOCATION_SOURCE_NMEA, sensor_channel, i);
        index++;
        SOURCE_LIST_ADD(index, HYSCAN_LOCATION_PARAMETER_TRACK, HYSCAN_LOCATION_SOURCE_NMEA_COMPUTED, sensor_channel, i);
        index++;
        SOURCE_LIST_ADD(index, HYSCAN_LOCATION_PARAMETER_SPEED, HYSCAN_LOCATION_SOURCE_NMEA, sensor_channel, i);
        index++;
        SOURCE_LIST_ADD(index, HYSCAN_LOCATION_PARAMETER_SPEED, HYSCAN_LOCATION_SOURCE_NMEA_COMPUTED, sensor_channel, i);
        index++;
        SOURCE_LIST_ADD(index, HYSCAN_LOCATION_PARAMETER_DATETIME, HYSCAN_LOCATION_SOURCE_NMEA, sensor_channel, i);
        index++;
        break;

      case HYSCAN_SONAR_DATA_NMEA_GGA:
        SOURCE_LIST_ADD (index, HYSCAN_LOCATION_PARAMETER_ALTITUDE, HYSCAN_LOCATION_SOURCE_NMEA, sensor_channel, i);
        index++;
        break;

      case HYSCAN_SONAR_DATA_ECHOSOUNDER:
        SOURCE_LIST_ADD (index, HYSCAN_LOCATION_PARAMETER_DEPTH, HYSCAN_LOCATION_SOURCE_ECHOSOUNDER, sensor_channel, i);
        index++;
        break;

      case HYSCAN_SONAR_DATA_SS_PORT:

        if (hi_res == TRUE)
          SOURCE_LIST_ADD (index, HYSCAN_LOCATION_PARAMETER_DEPTH, HYSCAN_LOCATION_SOURCE_SONAR_HIRES_PORT, sensor_channel, i);
        if (hi_res == FALSE)
          SOURCE_LIST_ADD (index, HYSCAN_LOCATION_PARAMETER_DEPTH, HYSCAN_LOCATION_SOURCE_SONAR_PORT, sensor_channel, i);
        index++;
        break;

      case HYSCAN_SONAR_DATA_SS_STARBOARD:

        if (hi_res == TRUE)
          SOURCE_LIST_ADD (index, HYSCAN_LOCATION_PARAMETER_DEPTH, HYSCAN_LOCATION_SOURCE_SONAR_HIRES_STARBOARD, sensor_channel, i);
        if (hi_res == FALSE)
          SOURCE_LIST_ADD (index, HYSCAN_LOCATION_PARAMETER_DEPTH, HYSCAN_LOCATION_SOURCE_SONAR_STARBOARD, sensor_channel, i);
        index++;
        break;

      case HYSCAN_SONAR_DATA_NMEA_DPT:
        SOURCE_LIST_ADD (index, sensor_channel, HYSCAN_LOCATION_PARAMETER_DEPTH, HYSCAN_LOCATION_SOURCE_NMEA, i);
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
  gint i = 0, j = 0, k = 0;
  HyScanLocationPrivate *priv = location->priv;
  HyScanLocationSourcesList *source_list_element;
  /* Выбираем КД по умолчанию.
   * NMEA-каналы данных выбираются по приоритету (1..5)
   * Причем наиболее приоритетным является тот канал, с которого вычисляются координаты.
   * Параметры скорость и курс по умолчанию высчитываются из координат, а не из строк.
   */
  HyScanSonarChannelIndex channel_priority[8] =
    {
      HYSCAN_SONAR_CHANNEL_1,
      HYSCAN_SONAR_CHANNEL_2,
      HYSCAN_SONAR_CHANNEL_3,
      HYSCAN_SONAR_CHANNEL_4,
      HYSCAN_SONAR_CHANNEL_5,
      HYSCAN_SONAR_CHANNEL_6,
      HYSCAN_SONAR_CHANNEL_7,
      HYSCAN_SONAR_CHANNEL_8
    };
  HyScanSonarChannelIndex default_channel;
  HyScanLocationSourceTypes depth_priority[5] =
    {
      HYSCAN_LOCATION_SOURCE_ECHOSOUNDER,
      HYSCAN_LOCATION_SOURCE_SONAR_PORT,
      HYSCAN_LOCATION_SOURCE_SONAR_STARBOARD,
      HYSCAN_LOCATION_SOURCE_SONAR_HIRES_PORT,
      HYSCAN_LOCATION_SOURCE_SONAR_HIRES_STARBOARD
    };
  HyScanLocationSourceTypes nmea_priority[2] =
    {
      HYSCAN_LOCATION_SOURCE_NMEA_COMPUTED,
      HYSCAN_LOCATION_SOURCE_NMEA
    };
  HyScanLocationSourceTypes default_source;

  /* HYSCAN_LOCATION_PARAMETER_LATLONG */
  default_source = nmea_priority[1];
  for ( j = 0; j < 8 && priv->latlong_source == -1; j++)
    {
      default_channel = channel_priority[j];
      for (i = 0; i < priv->source_list->len && priv->latlong_source == -1; i++)
        {
          source_list_element = &g_array_index (priv->source_list, HyScanLocationSourcesList, i);
          if (SOURCE_LIST_CHECK (i,HYSCAN_LOCATION_PARAMETER_LATLONG))
            {
              hyscan_location_source_set (location, i, TRUE);
              channel_priority[j] = channel_priority[0];
              channel_priority[0] = default_channel;
            }
        }
    }
  /* HYSCAN_LOCATION_PARAMETER_ALTITUDE
   * Приоритет: NMEA 1, NMEA 2, ..., NMEA 5
   */
  default_source = nmea_priority[1];
  for ( j = 0; j < 8 && priv->altitude_source == -1; j++)
    {
      default_channel = channel_priority[j];
      for (i = 0; i < priv->source_list->len && priv->altitude_source == -1; i++)
        {
          source_list_element = &g_array_index (priv->source_list, HyScanLocationSourcesList, i);
          if (SOURCE_LIST_CHECK (i, HYSCAN_LOCATION_PARAMETER_ALTITUDE))
            {
              hyscan_location_source_set (location, i, TRUE);
            }
        }
    }
  /* HYSCAN_LOCATION_PARAMETER_TRACK
   * Приоритет: NMEA-COMPUTED 1, NMEA 1, NMEA-COMPUTED 2, NMEA 2, ...
   */
  for (k = 0; k < 2 && priv->track_source == -1; k++)
    {
      default_source = nmea_priority[k];
      for (j = 0; j < 5 && priv->track_source == -1; j++)
        {
          default_channel = channel_priority[j];
          for (i = 0; i < priv->source_list->len && priv->track_source == -1; i++)
            {
              source_list_element = &g_array_index (priv->source_list, HyScanLocationSourcesList, i);
              if (SOURCE_LIST_CHECK (i, HYSCAN_LOCATION_PARAMETER_TRACK))
                {
                  hyscan_location_source_set (location, i, TRUE);
                }
            }
        }
    }
  /* HYSCAN_LOCATION_PARAMETER_ROLL */
  /* HYSCAN_LOCATION_PARAMETER_PITCH */
  /* HYSCAN_LOCATION_PARAMETER_SPEED */
  for (k = 0; k < 2 && priv->speed_source == -1; k++)
    {
      default_source = nmea_priority[k];
      for (j = 0; j < 5 && priv->speed_source == -1; j++)
        {
          default_channel = channel_priority[j];
          for (i = 0; i < priv->source_list->len && priv->speed_source == -1; i++)
            {
              source_list_element = &g_array_index (priv->source_list, HyScanLocationSourcesList, i);
              if (SOURCE_LIST_CHECK (i, HYSCAN_LOCATION_PARAMETER_SPEED))
                {
                  hyscan_location_source_set (location, i, TRUE);
                }
            }
        }
    }
  /* HYSCAN_LOCATION_PARAMETER_DEPTH */
  default_channel = 1;
  for (k = 0; k < 8 && priv->depth_source == -1; k++)
    {
      default_source = depth_priority[k];
      for (i = 0; i < priv->source_list->len && priv->depth_source == -1; i++)
        {
          source_list_element = &g_array_index (priv->source_list, HyScanLocationSourcesList, i);
          if (SOURCE_LIST_CHECK (i, HYSCAN_LOCATION_PARAMETER_DEPTH))
            {
              hyscan_location_source_set (location, i, TRUE);
            }
        }
    }
  /* HYSCAN_LOCATION_PARAMETER_DATETIME */
  default_source = nmea_priority[1];
  for (j = 0; j < 8 && priv->datetime_source == -1; j++)
    {
      default_channel = channel_priority[j];
      for (i = 0; i < priv->source_list->len && priv->datetime_source == -1; i++)
        {
          source_list_element = &g_array_index (priv->source_list, HyScanLocationSourcesList, i);
          if (SOURCE_LIST_CHECK (i, HYSCAN_LOCATION_PARAMETER_DATETIME))
            {
              hyscan_location_source_set (location, i, TRUE);
            }
        }
    }
}

/* Функция отдает список источников для заданного параметра. */
GArray *
hyscan_location_source_list (HyScanLocation *location,
                             gint            parameter)
{
  HyScanLocationPrivate *priv = location->priv;
  HyScanLocationSourcesList *source_list_element;

  HyScanLocationSources output_source_element;
  GArray *output_source_list;
  output_source_list = g_array_new (TRUE, TRUE, sizeof(HyScanLocationSources));

  gint i = 0;

  for (i = 0; i < priv->source_list->len; i++)
    {
      source_list_element = &g_array_index (priv->source_list, HyScanLocationSourcesList, i);
      if (source_list_element->parameter == parameter)


        output_source_element.index = source_list_element->index;
        output_source_element.source_type = source_list_element->source_type;
        output_source_element.sensor_channel = source_list_element->sensor_channel;
        g_array_append_val (output_source_list, output_source_element);
    }

  return output_source_list;
}

/* Функция возвращает индекс активного источника для заданного параметра. */
gint
hyscan_location_source_get (HyScanLocation *location,
                            gint            parameter)
{
  HyScanLocationPrivate *priv = location->priv;
  HyScanLocationSourcesList *source_list_element;
  gint i = 0;
  for (i = 0; i < priv->source_list->len; i++)
    {
      source_list_element = &g_array_index (priv->source_list, HyScanLocationSourcesList, i);
      if (source_list_element->parameter == parameter && source_list_element->active == TRUE)
        return source_list_element->index;
    }
  return 0;
}

/* Функция задает источник. */
gboolean
hyscan_location_source_set (HyScanLocation *location,
                            gint            source,
                            gboolean        turn_on)
{
  HyScanLocationPrivate *priv = location->priv;

  HyScanLocationSourcesList *source_list_element;
  HyScanLocationParameters parameter;
  GArray *param_cache;
  gint32 *param_source;
  HyScanLocationSourceTypes source_type;
  HyScanDataChannelInfo *dc_info;
  gboolean status = FALSE;

  /* Ставим надзирателя на паузу. */
  if (priv->overseer_thread != NULL)
    {
      g_atomic_int_set (&(priv->overseer_pause), 1);
      while ( g_atomic_int_get (&(priv->overseer_paused)) != 1 )
        g_usleep (priv->overseer_period);
    }

  source_list_element = &g_array_index (priv->source_list, HyScanLocationSourcesList, source);
  parameter = source_list_element->parameter;

  /* Устанавливаем внутренние указатели на кэш и источник. */
  switch (parameter)
    {
    case HYSCAN_LOCATION_PARAMETER_LATLONG:
      param_cache = priv->latlong_cache;
      param_source = &(priv->latlong_source);
      break;

    case HYSCAN_LOCATION_PARAMETER_ALTITUDE:
      param_cache = priv->altitude_cache;
      param_source = &(priv->altitude_source);
      break;

    case HYSCAN_LOCATION_PARAMETER_TRACK:
      param_cache = priv->track_cache;
      param_source = &(priv->track_source);
      break;

    case HYSCAN_LOCATION_PARAMETER_SPEED:
      param_cache = priv->speed_cache;
      param_source = &(priv->speed_source);
      break;

    case HYSCAN_LOCATION_PARAMETER_ROLL:
      param_cache = priv->roll_cache;
      param_source = &(priv->roll_source);
      break;

    case HYSCAN_LOCATION_PARAMETER_PITCH:
      param_cache = priv->pitch_cache;
      param_source = &(priv->pitch_source);
      break;

    case HYSCAN_LOCATION_PARAMETER_DEPTH:
      param_cache = priv->depth_cache;
      param_source = &(priv->depth_source);
      break;

    case HYSCAN_LOCATION_PARAMETER_DATETIME:
      param_cache = priv->datetime_cache;
      param_source = &(priv->datetime_source);
      break;

    default:
      status = FALSE;
      goto exit;
    }

  /* Закрываем предыдущий источник, если он используется. */
  if (*param_source != -1)
    {
      source_list_element = &g_array_index (priv->source_list, HyScanLocationSourcesList, *param_source);
      hyscan_db_close (priv->db, source_list_element->channel_id);
      g_array_free (param_cache, TRUE);

      source_list_element->channel_id = 0;
      source_list_element->active = FALSE;

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

  source_list_element = &g_array_index (priv->source_list, HyScanLocationSourcesList, *param_source);

  /* Открываем КД. */
  source_type = source_list_element->source_type;
  switch (source_type)
    {
    case HYSCAN_LOCATION_SOURCE_NMEA:
    case HYSCAN_LOCATION_SOURCE_NMEA_COMPUTED:
      source_list_element->channel_id = hyscan_db_channel_open (priv->db,
                                                                priv->track_id,
                                                                source_list_element->channel_name);
      break;
    case HYSCAN_LOCATION_SOURCE_ECHOSOUNDER:
    case HYSCAN_LOCATION_SOURCE_SONAR_PORT:
    case HYSCAN_LOCATION_SOURCE_SONAR_STARBOARD:
    case HYSCAN_LOCATION_SOURCE_SONAR_HIRES_PORT:
    case HYSCAN_LOCATION_SOURCE_SONAR_HIRES_STARBOARD:
      source_list_element->dchannel = hyscan_data_channel_new (priv->db,
                                                               priv->project_name,
                                                               priv->track_name,
                                                               source_list_element->channel_name);
      break;
    default:
      status = FALSE;
      goto exit;
    }

  /* Инициализируем локальный кэш. */
  switch (parameter)
    {
    case HYSCAN_LOCATION_PARAMETER_LATLONG:
      priv->latlong_cache = g_array_new (FALSE, TRUE, sizeof (HyScanLocationGdouble2));
      break;

    case HYSCAN_LOCATION_PARAMETER_ALTITUDE:
      priv->altitude_cache = g_array_new (FALSE, TRUE, sizeof (HyScanLocationGdouble1));

      break;

    case HYSCAN_LOCATION_PARAMETER_TRACK:
      if (source_type == HYSCAN_LOCATION_SOURCE_NMEA_COMPUTED)
        priv->track_cache = g_array_new (FALSE, TRUE, sizeof (HyScanLocationGdouble2));
      else
        priv->track_cache = g_array_new (FALSE, TRUE, sizeof (HyScanLocationGdouble1));
      break;

    case HYSCAN_LOCATION_PARAMETER_SPEED:
      if (source_type == HYSCAN_LOCATION_SOURCE_NMEA_COMPUTED)
        priv->speed_cache = g_array_new (FALSE, TRUE, sizeof (HyScanLocationGdouble2));
      else
        priv->speed_cache = g_array_new (FALSE, TRUE, sizeof (HyScanLocationGdouble1));
      break;

    case HYSCAN_LOCATION_PARAMETER_ROLL:
      priv->roll_cache = g_array_new (FALSE, TRUE, sizeof (HyScanLocationGdouble1));
      break;

    case HYSCAN_LOCATION_PARAMETER_PITCH:
      priv->pitch_cache = g_array_new (FALSE, TRUE, sizeof (HyScanLocationGdouble1));
      break;

    case HYSCAN_LOCATION_PARAMETER_DEPTH:
      priv->depth_cache = g_array_new (FALSE, TRUE, sizeof (HyScanLocationGdouble1));
      break;

    case HYSCAN_LOCATION_PARAMETER_DATETIME:
      priv->datetime_cache = g_array_new (FALSE, TRUE, sizeof (HyScanLocationGint1));
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
      source_list_element->param_id = hyscan_db_channel_param_open (priv->db, source_list_element->channel_id);
      status = hyscan_location_param_load (location, *param_source);
      if (!status)
      {
        source_list_element->x = 0;
        source_list_element->y = 0;
        source_list_element->z = 0;
        source_list_element->psi = 0;
        source_list_element->gamma = 0;
        source_list_element->theta = 0;
        source_list_element->discretization_frequency = 0;
      }
      break;
    case HYSCAN_LOCATION_SOURCE_ECHOSOUNDER:
    case HYSCAN_LOCATION_SOURCE_SONAR_PORT:
    case HYSCAN_LOCATION_SOURCE_SONAR_STARBOARD:
    case HYSCAN_LOCATION_SOURCE_SONAR_HIRES_PORT:
    case HYSCAN_LOCATION_SOURCE_SONAR_HIRES_STARBOARD:
      //source_list_element->param_id = hyscan_db_channel_param_open (priv->db, source_list_element->channel_id);
      dc_info = hyscan_data_channel_get_info (source_list_element->dchannel);
      source_list_element->x = dc_info->x;
      source_list_element->y = dc_info->y;
      source_list_element->z = dc_info->z;
      source_list_element->psi = dc_info->psi;
      source_list_element->gamma = dc_info->gamma;
      source_list_element->theta = dc_info->theta;
      source_list_element->discretization_frequency = dc_info->discretization_frequency;
      break;
    default:
      status = FALSE;
      goto exit;
    }

  source_list_element->active = TRUE;
  status = TRUE;
  exit:
  /* Снимаем надзирателя с паузы. */
  if (priv->overseer_thread != NULL)
    {
      g_atomic_int_set (&(priv->overseer_pause), 0);
      while ( g_atomic_int_get (&(priv->overseer_paused)) != 0 )
        g_usleep (priv->overseer_period);
    }

  return status;
}

/* Функция загружает параметры КД. */
gboolean
hyscan_location_param_load (HyScanLocation *location,
                            gint32          source)
{
  HyScanLocationPrivate *priv = location->priv;
  HyScanLocationSourceTypes source_type = 0;
  HyScanLocationSourcesList *source_list_element;
  source_list_element = &g_array_index (priv->source_list, HyScanLocationSourcesList, source);
  /* Загружаем параметры источника. */
  if (!hyscan_db_param_get_double (priv->db, source_list_element->param_id, NULL, "/position/x", &(source_list_element->x)))
    return FALSE;
  if (!hyscan_db_param_get_double (priv->db, source_list_element->param_id, NULL, "/position/y", &(source_list_element->y)))
    return FALSE;
  if (!hyscan_db_param_get_double (priv->db, source_list_element->param_id, NULL, "/position/z", &(source_list_element->z)))
    return FALSE;
  if (!hyscan_db_param_get_double (priv->db, source_list_element->param_id, NULL, "/orientation/psi", &(source_list_element->psi)))
    return FALSE;
  if (!hyscan_db_param_get_double (priv->db, source_list_element->param_id, NULL, "/orientation/gamma", &(source_list_element->gamma)))
    return FALSE;
  if (!hyscan_db_param_get_double (priv->db, source_list_element->param_id, NULL, "/orientation/theta", &(source_list_element->theta)))
    return FALSE;


  source_type = source_list_element->source_type;
  if (source_type == HYSCAN_LOCATION_SOURCE_ECHOSOUNDER      ||
      source_type == HYSCAN_LOCATION_SOURCE_SONAR_PORT       ||
      source_type == HYSCAN_LOCATION_SOURCE_SONAR_STARBOARD  ||
      source_type == HYSCAN_LOCATION_SOURCE_SONAR_HIRES_PORT ||
      source_type == HYSCAN_LOCATION_SOURCE_SONAR_HIRES_STARBOARD)
    {
      if (!hyscan_db_param_get_double (priv->db, source_list_element->param_id, NULL, "/discretization/frequency", &(source_list_element->discretization_frequency)))
        return FALSE;

      source_list_element->discretization_type = hyscan_db_param_get_string (priv->db,
                                                                            source_list_element->param_id,
                                                                            NULL,
                                                                            "/discretization/type");
      if (source_list_element->discretization_type == NULL)
        return FALSE;
    }
  return TRUE;
}

/* Функция установки таблицы скорости звука. */
gboolean
hyscan_location_soundspeed_set (HyScanLocation *location,
                                GArray          soundspeedtable)
{
  HyScanLocationPrivate *priv = location->priv;
  priv->soundspeedtable = soundspeedtable;
  return TRUE;
}

/* Функция-надзиратель. */
static gpointer
hyscan_location_overseer (gpointer user_data)
{
  HyScanLocation *location = user_data;
  HyScanLocationPrivate *priv = location->priv;

  /* Сигнализируем, что поток запустился. */
  g_atomic_int_set (&(priv->overseer_started), 1);

  /* Поток работает до тех пор, пока не будет установлен флаг остановки. */
  while (g_atomic_int_get (&(priv->overseer_stop)) != 1)
    {
      /* Если надо поставить обработку на паузу. */
      if (g_atomic_int_get (&(priv->overseer_pause)) == 1)
        {
          g_atomic_int_set (&(priv->overseer_paused), 1);
          while (g_atomic_int_get (&(priv->overseer_pause)) != 0)
            g_usleep (priv->overseer_period);
          g_atomic_int_set (&(priv->overseer_paused), 0);
        }

      /* Важно: hyscan_location_overseer_datetime должен выполняться первым, т.к. все остальные методы используют его для сдвижки данных во времени. */
      hyscan_location_overseer_datetime (priv->db, priv->source_list, priv->datetime_cache, priv->datetime_source, priv->quality);

      hyscan_location_overseer_latlong  (priv->db, priv->source_list, priv->latlong_cache,  priv->latlong_source,  priv->datetime_cache, priv->datetime_source, priv->quality);
      hyscan_location_overseer_altitude (priv->db, priv->source_list, priv->altitude_cache, priv->altitude_source, priv->datetime_cache, priv->datetime_source, priv->quality);
      hyscan_location_overseer_track    (priv->db, priv->source_list, priv->track_cache,    priv->track_source,    priv->datetime_cache, priv->datetime_source, priv->quality);
      hyscan_location_overseer_roll     (priv->db, priv->source_list, priv->roll_cache,     priv->roll_source,     priv->datetime_cache, priv->datetime_source, priv->quality);
      hyscan_location_overseer_pitch    (priv->db, priv->source_list, priv->pitch_cache,    priv->pitch_source,    priv->datetime_cache, priv->datetime_source, priv->quality);
      hyscan_location_overseer_speed    (priv->db, priv->source_list, priv->speed_cache,    priv->speed_source,    priv->datetime_cache, priv->datetime_source, priv->quality);
      hyscan_location_overseer_depth    (priv->db, priv->source_list, priv->depth_cache,    priv->depth_source,    &(priv->soundspeedtable), priv->quality);

      g_printf("overseer ok\n");
      g_usleep (priv->overseer_period);
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
                     gdouble         theta)
{
  HyScanLocationPrivate *priv = location->priv;
  HyScanLocationData output = {HYSCAN_LOCATIONDATA_NAN};
  gdouble cache_status;

  HyScanLocationGdouble1 temp1 = {0};
  HyScanLocationGdouble2 temp2 = {0};
  HyScanLocationGint1    temp3 = {0};

  HyScanLocationSourcesList *source_list_element;
  gdouble center_x = 0,
          center_y = 0,
          center_z = 0,
          center_psi = 0,
          center_gamma = 0,
          center_theta = 0;


  /* Сначала ищем значение в кэше. */
  cache_status = hyscan_location_update_cache_key (location, time);
  if (cache_status)
    {
      if (hyscan_location_cache_get (location, parameter, &output))
        return output;
    }

  if ((parameter & HYSCAN_LOCATION_PARAMETER_LATLONG) == HYSCAN_LOCATION_PARAMETER_LATLONG)
    {
      temp2 = hyscan_location_getter_latlong (priv->db, priv->source_list, priv->latlong_cache, priv->latlong_source, time, priv->quality)  ;
      output.latitude = temp2.value1;
      output.longitude = temp2.value2;
      output.validity &= temp2.validity;

      source_list_element = &g_array_index (priv->source_list, HyScanLocationSourcesList, priv->latlong_source);
      center_x = source_list_element->x;
      center_y = source_list_element->y;
    }
  if ((parameter & HYSCAN_LOCATION_PARAMETER_ALTITUDE) == HYSCAN_LOCATION_PARAMETER_ALTITUDE)
    {
      temp1 = hyscan_location_getter_altitude (priv->db, priv->source_list, priv->altitude_cache, priv->altitude_source, time, priv->quality);
      output.altitude = temp1.value;
      output.validity &= temp1.validity;

      source_list_element = &g_array_index (priv->source_list, HyScanLocationSourcesList, priv->altitude_source);
      center_z = source_list_element->z;
    }
  if ((parameter & HYSCAN_LOCATION_PARAMETER_TRACK) == HYSCAN_LOCATION_PARAMETER_TRACK)
    {
      temp1 = hyscan_location_getter_track (priv->db, priv->source_list, priv->track_cache, priv->track_source, time, priv->quality);
      output.track = temp1.value;

      source_list_element = &g_array_index (priv->source_list, HyScanLocationSourcesList, priv->track_source);
      center_psi = source_list_element->psi;
      output.track += center_psi;

      output.validity &= temp1.validity;
    }
  if ((parameter & HYSCAN_LOCATION_PARAMETER_ROLL) == HYSCAN_LOCATION_PARAMETER_ROLL)
    {
      temp1 = hyscan_location_getter_roll (priv->db, priv->source_list, priv->roll_cache, priv->roll_source, time, priv->quality);
      output.roll = temp1.value;

      source_list_element = &g_array_index (priv->source_list, HyScanLocationSourcesList, priv->roll_source);
      center_gamma = source_list_element->gamma;
      output.roll += center_gamma;

      output.validity &= temp1.validity;
    }
  if ((parameter & HYSCAN_LOCATION_PARAMETER_PITCH) == HYSCAN_LOCATION_PARAMETER_PITCH)
    {
      temp1 = hyscan_location_getter_pitch (priv->db, priv->source_list, priv->pitch_cache, priv->pitch_source, time, priv->quality);
      output.pitch = temp1.value;

      source_list_element = &g_array_index (priv->source_list, HyScanLocationSourcesList, priv->pitch_source);
      center_theta = source_list_element->theta;
      output.pitch += center_theta;

      output.validity &= temp1.validity;
    }
  if ((parameter & HYSCAN_LOCATION_PARAMETER_SPEED) == HYSCAN_LOCATION_PARAMETER_SPEED)
    {
      temp1 = hyscan_location_getter_speed (priv->db, priv->source_list, priv->speed_cache, priv->speed_source, time, priv->quality);
      output.speed = temp1.value;
      output.validity &= temp1.validity;
    }
  if ((parameter & HYSCAN_LOCATION_PARAMETER_DEPTH) == HYSCAN_LOCATION_PARAMETER_DEPTH)
    {
      temp1 = hyscan_location_getter_depth (priv->db, priv->source_list, priv->depth_cache, priv->depth_source, time, priv->quality);
      output.depth = temp1.value;
      output.validity &= temp1.validity;
    }
  if ((parameter & HYSCAN_LOCATION_PARAMETER_DATETIME) == HYSCAN_LOCATION_PARAMETER_DATETIME)
    {
      temp3 = hyscan_location_getter_datetime (priv->db, priv->source_list, priv->datetime_cache, priv->datetime_source, time, priv->quality);
      //output.time = temp3.value;
      output.validity &= temp3.validity;
    }

  /* Сдвигаем к центру масс. */
  hyscan_location_shift (&output, center_x, center_y, center_z, (output.track)*G_PI/180.0, output.roll, output.pitch);
  /* Кладем значение в кэш. */
  if (cache_status && output.validity)
    hyscan_location_cache_set (location, &output);
  /* Сдвигаем к запрашиваемой точке. */
  hyscan_location_shift (&output, x, y, z, (output.track)*G_PI/180.0, output.roll, output.pitch);
  /* Учет углов установки.*/
  output.track += psi;
  output.roll += gamma;
  output.pitch += theta;
  return output;
}

/* Функция возвращает счетчик изменений. */
gint32
hyscan_location_get_mod_count (HyScanLocation *location)
{
  HyScanLocationPrivate *priv = location->priv;
  return g_atomic_int_get (&(priv->mod_count));
}

/* Функция возвращает процент готовности данных. */
gint
hyscan_location_get_progress (HyScanLocation *location)
{
  HyScanLocationPrivate *priv = location->priv;
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
  if (priv->cache != NULL && priv->cache_key == NULL)
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
  return TRUE;
}

/* Функция записывает значение в кэш. */
static gboolean
hyscan_location_cache_set (HyScanLocation     *location,
                           HyScanLocationData *data)
{
  HyScanLocationPrivate *priv = location->priv;

  /* Пишем значение в кеш. */
  return hyscan_cache_set (priv->cache, priv->cache_key, NULL, data, sizeof(HyScanLocationData));
}

/* Функция получает значение из кэша. */
static gboolean
hyscan_location_cache_get (HyScanLocation     *location,
                           gint                parameter,
                           HyScanLocationData *data)
{
  HyScanLocationPrivate *priv = location->priv;
  gint32 buffer_size = sizeof(HyScanLocationData);

  if (!hyscan_cache_get (priv->cache, priv->cache_key, NULL, data, &buffer_size))
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

static void
hyscan_location_source_free (HyScanLocationSourcesList *data)
{
  g_free(data->channel_name);
  g_free(data->discretization_type);
}
