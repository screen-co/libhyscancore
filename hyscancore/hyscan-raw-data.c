/*
 * \file hyscan-raw-data.c
 *
 * \brief Исходный файл класса обработки акустических данных
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2015
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-raw-data.h"
#include "hyscan-core-schemas.h"
#include <hyscan-convolution.h>

#include <math.h>
#include <string.h>

enum
{
  PROP_O,
  PROP_DB,
  PROP_PROJECT_NAME,
  PROP_TRACK_NAME,
  PROP_SOURCE_TYPE,
  PROP_SOURCE_CHANNEL,
  PROP_NOISE
};

enum
{
  DATA_TYPE_AMPLITUDE,
  DATA_TYPE_QUADRATURE
};

/* Структура с образом сигнала и объектом свёртки. */
typedef struct
{
  gint64               time;                                   /* Момент времени начала действия сигнала. */
  HyScanComplexFloat  *image;                                  /* Образ сигнала для свёртки. */
  guint32              n_points;                               /* Число точек образа сигнала. */
  HyScanConvolution   *convolution;                            /* Объект свёртки. */
} HyScanRawDataSignal;

struct _HyScanRawDataPrivate
{
  HyScanDB            *db;                                     /* Интерфейс базы данных. */
  gchar               *db_uri;                                 /* URI базы данных. */

  gchar               *project_name;                           /* Название проекта. */
  gchar               *track_name;                             /* Название галса. */
  HyScanSourceType     source_type;                            /* Тип источника данных. */
  guint                source_channel;                         /* Индекс канала данных. */
  gboolean             noise;                                  /* Открывать канал с шумами. */

  HyScanCache         *cache;                                  /* Интерфейс системы кэширования. */
  gchar               *cache_prefix;                           /* Префикс ключа кэширования. */

  HyScanAntennaPosition position;                              /* Местоположение приёмной антенны. */
  HyScanRawDataInfo     info;                                  /* Параметры сырых данных. */

  gint32               channel_id;                             /* Идентификатор открытого канала данных. */
  gint32               signal_id;                              /* Идентификатор открытого канала с образами сигналов. */
  gint32               tvg_id;                                 /* Идентификатор открытого канала с коэффициентами усиления. */

  gpointer             raw_buffer;                             /* Буфер для чтения "сырых" данных канала. */
  gint32               raw_buffer_size;                        /* Размер буфера в байтах. */

  HyScanComplexFloat  *data_buffer;                            /* Буфер для обработки данных. */
  gint64               data_time;                              /* Метка времени обрабатываемых данных. */

  GArray              *signals;                                /* Массив сигналов. */
  guint32              last_signal_index;                      /* Индекс последнего загруженного сигнала. */
  guint64              signals_mod_count;                      /* Номер изменений сигналов. */
  gboolean             convolve;                               /* Выполнять или нет свёртку. */

  gchar               *cache_key;                              /* Ключ кэширования. */
  gint                 cache_key_length;                       /* Максимальная длина ключа. */
};

static void            hyscan_raw_data_set_property            (GObject                       *object,
                                                                guint                          prop_id,
                                                                const GValue                  *value,
                                                                GParamSpec                    *pspec);
static void            hyscan_raw_data_object_constructed      (GObject                       *object);
static void            hyscan_raw_data_object_finalize         (GObject                       *object);

static gboolean        hyscan_raw_data_load_position            (HyScanDB                      *db,
                                                                gint32                         param_id,
                                                                HyScanAntennaPosition         *position);
static gboolean        hyscan_raw_data_load_data_params        (HyScanDB                      *db,
                                                                gint32                         param_id,
                                                                HyScanRawDataInfo             *info);
static gboolean        hyscan_raw_data_check_addon_params      (HyScanDB                      *db,
                                                                gint32                         param_id,
                                                                gint64                         schema_id,
                                                                gdouble                        data_rate,
                                                                HyScanDataType                 data_type);

static void            hyscan_raw_data_update_cache_key        (HyScanRawDataPrivate          *priv,
                                                                gint                           data_type,
                                                                guint32                        index);
static void            hyscan_raw_data_buffer_realloc          (HyScanRawDataPrivate          *priv,
                                                                gint32                         size);
static guint32         hyscan_raw_data_read_raw_data           (HyScanRawDataPrivate          *priv,
                                                                gint32                         channel_id,
                                                                guint32                        index,
                                                                gint64                        *time);

static void            hyscan_raw_data_load_signals            (HyScanRawDataPrivate          *priv);
static HyScanRawDataSignal *hyscan_raw_data_find_signal        (HyScanRawDataPrivate          *priv,
                                                                gint64                         time);

static guint32         hyscan_raw_data_read_data               (HyScanRawDataPrivate          *priv,
                                                                guint32                        index);
static gboolean        hyscan_raw_data_check_cache             (HyScanRawDataPrivate          *priv,
                                                                gint                           data_type,
                                                                guint32                        index,
                                                                gpointer                       buffer,
                                                                guint32                       *buffer_size,
                                                                gint64                        *time);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanRawData, hyscan_raw_data, G_TYPE_OBJECT);

static void
hyscan_raw_data_class_init (HyScanRawDataClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_raw_data_set_property;

  object_class->constructed = hyscan_raw_data_object_constructed;
  object_class->finalize = hyscan_raw_data_object_finalize;

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
    g_param_spec_int ("source-channel", "SourceChannel", "Source channel", 1, 5, 1,
                      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_NOISE,
    g_param_spec_boolean ("noise", "Noise", "Use noise channel", FALSE,
                          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_raw_data_init (HyScanRawData *data)
{
  data->priv = hyscan_raw_data_get_instance_private (data);
}

static void
hyscan_raw_data_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  HyScanRawData *data = HYSCAN_RAW_DATA (object);
  HyScanRawDataPrivate *priv = data->priv;

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

    case PROP_SOURCE_CHANNEL:
      priv->source_channel = g_value_get_int (value);
      break;

    case PROP_NOISE:
      priv->noise = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (data, prop_id, pspec);
      break;
    }
}

static void
hyscan_raw_data_object_constructed (GObject *object)
{
  HyScanRawData *data = HYSCAN_RAW_DATA (object);
  HyScanRawDataPrivate *priv = data->priv;

  gint32 project_id = -1;
  gint32 track_id = -1;
  gint32 param_id = -1;

  guint32 first_index;
  guint32 last_index;

  const gchar *channel_name;
  gchar *signal_name = NULL;
  gchar *tvg_name = NULL;

  gboolean status = FALSE;

  priv->channel_id = -1;
  priv->signal_id = -1;
  priv->last_signal_index = -1;

  /* Проверяем тип источника данных. */
  if (!hyscan_source_is_raw (priv->source_type))
    goto exit;

  if (priv->db == NULL)
    goto exit;

  priv->db_uri = hyscan_db_get_uri (priv->db);

  /* Названия проекта, галса, канала данных. */
  channel_name = hyscan_channel_get_name_by_types (priv->source_type, TRUE, priv->source_channel);
  if ((priv->project_name == NULL) || (priv->track_name == NULL) || (channel_name == NULL))
    {
      if (priv->project_name == NULL)
        g_warning ("HyScanRawData: unknown project name");
      if (priv->track_name == NULL)
        g_warning ("HyScanRawData: unknown track name");
      if (channel_name == NULL)
        g_warning ("HyScanRawData: unknown channel name");
      goto exit;
    }

  project_id = hyscan_db_project_open (priv->db, priv->project_name);
  if (project_id < 0)
    {
      g_info ("HyScanRawData: can't open project '%s'", priv->project_name);
      goto exit;
    }

  track_id = hyscan_db_track_open (priv->db, project_id, priv->track_name);
  if (track_id < 0)
    {
      g_info ("HyScanRawData: can't open track '%s.%s'", priv->project_name, priv->track_name);
      goto exit;
    }

  /* Открываем канал данных. */
  if (priv->noise)
    {
      gchar *noise_channel_name = g_strdup_printf ("%s-noise", channel_name);
      priv->channel_id = hyscan_db_channel_open (priv->db, track_id, noise_channel_name);
      g_free (noise_channel_name);
    }
  else
    {
      priv->channel_id = hyscan_db_channel_open (priv->db, track_id, channel_name);
    }

  if (priv->channel_id < 0)
    {
      g_info ("HyScanRawData: can't open channel '%s.%s.%s'",
              priv->project_name, priv->track_name, channel_name);
      goto exit;
    }

  /* Проверяем наличие записей в канале. */
  if (!hyscan_db_channel_get_data_range (priv->db, priv->channel_id, &first_index, &last_index) ||
      ((first_index == 0) && (last_index == 0)))
    {
      goto exit;
    }

  /* Параметры канала данных. */
  param_id = hyscan_db_channel_param_open (priv->db, priv->channel_id);
  if (param_id < 0)
    {
      g_warning ("HyScanRawData: '%s.%s.%s': can't open parameters",
                 priv->project_name, priv->track_name, channel_name);
      goto exit;
    }

  if (!hyscan_raw_data_load_position (priv->db, param_id, &priv->position))
    {
      g_warning ("HyScanRawData: '%s.%s.%s': can't read antenna position",
                 priv->project_name, priv->track_name, channel_name);
      goto exit;
    }

  if (!hyscan_raw_data_load_data_params (priv->db, param_id, &priv->info))
    {
      g_warning ("HyScanRawData: '%s.%s.%s': can't read parameters",
                 priv->project_name, priv->track_name, channel_name);
      goto exit;
    }

  hyscan_db_close (priv->db, param_id);
  param_id = -1;

  /* Образы сигналов для свёртки. */
  signal_name = g_strdup_printf ("%s-signal", channel_name);
  if (hyscan_db_is_exist (priv->db, priv->project_name, priv->track_name, signal_name))
    priv->signal_id = hyscan_db_channel_open (priv->db, track_id, signal_name);
  g_free (signal_name);

  /* Загрузка образов сигналов. */
  if (priv->signal_id > 0)
    {
      param_id = hyscan_db_channel_param_open (priv->db, priv->signal_id);
      if (param_id < 0)
        {
          g_warning ("HyScanRawData: '%s.%s.%s-signal': can't open parameters",
                     priv->project_name, priv->track_name, channel_name);
          goto exit;
        }

      status = hyscan_raw_data_check_addon_params (priv->db, param_id, SIGNAL_CHANNEL_SCHEMA_ID,
                                                   priv->info.data.rate, HYSCAN_DATA_COMPLEX_FLOAT);
      if (!status)
        {
          g_warning ("HyScanRawData: '%s.%s.%s-signal': error in parameters",
                     priv->project_name, priv->track_name, channel_name);
          goto exit;
        }
      else
        {
          priv->signals_mod_count = hyscan_db_get_mod_count (priv->db, priv->signal_id) - 1;
          hyscan_raw_data_load_signals (priv);

          priv->convolve = TRUE;
        }
      status = FALSE;

      hyscan_db_close (priv->db, param_id);
      param_id = -1;
    }

  /* Коэффициенты усиления ВАРУ. */
  tvg_name = g_strdup_printf ("%s-tvg", channel_name);
  if (hyscan_db_is_exist (priv->db, priv->project_name, priv->track_name, tvg_name))
    priv->tvg_id = hyscan_db_channel_open (priv->db, track_id, tvg_name);
  g_free (tvg_name);

  /* Проверка параметров ВАРУ. */
  if (priv->tvg_id > 0)
    {
      param_id = hyscan_db_channel_param_open (priv->db, priv->tvg_id);
      if (param_id < 0)
        {
          g_warning ("HyScanRawData: '%s.%s.%s-tvg': can't open parameters",
                     priv->project_name, priv->track_name, channel_name);
          goto exit;
        }

      status = hyscan_raw_data_check_addon_params (priv->db, param_id, TVG_CHANNEL_SCHEMA_ID,
                                                   priv->info.data.rate, HYSCAN_DATA_FLOAT);
      if (!status)
        {
          g_warning ("HyScanRawData: '%s.%s.%s-tvg': error in parameters",
                     priv->project_name, priv->track_name, channel_name);
          goto exit;
        }
      status = FALSE;

      hyscan_db_close (priv->db, param_id);
      param_id = -1;
    }

  status = TRUE;

exit:
  if (!status)
    {
      if (priv->channel_id > 0)
        hyscan_db_close (priv->db, priv->channel_id);
      if (priv->signal_id > 0)
        hyscan_db_close (priv->db, priv->signal_id);
      if (priv->tvg_id > 0)
        hyscan_db_close (priv->db, priv->tvg_id);
      priv->channel_id = -1;
      priv->signal_id = -1;
      priv->tvg_id = -1;
    }

  if (project_id > 0)
    hyscan_db_close (priv->db, project_id);
  if (track_id > 0)
    hyscan_db_close (priv->db, track_id);
  if (param_id > 0)
    hyscan_db_close (priv->db, param_id);
}

static void
hyscan_raw_data_object_finalize (GObject *object)
{
  HyScanRawData *data = HYSCAN_RAW_DATA (object);
  HyScanRawDataPrivate *priv = data->priv;

  guint i;

  if (priv->channel_id > 0)
    hyscan_db_close (priv->db, priv->channel_id);
  if (priv->signal_id > 0)
    hyscan_db_close (priv->db, priv->signal_id);
  if (priv->tvg_id > 0)
    hyscan_db_close (priv->db, priv->tvg_id);

  if (priv->signals != NULL)
    {
      for (i = 0; i < priv->signals->len; i++)
        {
          HyScanRawDataSignal *signal = &g_array_index (priv->signals, HyScanRawDataSignal, i);
          g_clear_object (&signal->convolution);
          g_free (signal->image);
        }
      g_array_unref (priv->signals);
    }
  priv->last_signal_index = -1;

  g_free (priv->project_name);
  g_free (priv->track_name);

  g_free (priv->db_uri);
  g_free (priv->cache_key);
  g_free (priv->cache_prefix);

  g_free (priv->raw_buffer);
  g_free (priv->data_buffer);

  g_clear_object (&priv->db);
  g_clear_object (&priv->cache);

  G_OBJECT_CLASS (hyscan_raw_data_parent_class)->finalize (object);
}

/* Функция загружает местоположение приёмной антенны гидролокатора. */
static gboolean
hyscan_raw_data_load_position (HyScanDB              *db,
                               gint32                 param_id,
                               HyScanAntennaPosition *position)
{
  const gchar *param_names[9];
  GVariant *param_values[9];
  gboolean status = FALSE;

  param_names[0] = "/schema/id";
  param_names[1] = "/schema/version";
  param_names[2] = "/position/x";
  param_names[3] = "/position/y";
  param_names[4] = "/position/z";
  param_names[5] = "/position/psi";
  param_names[6] = "/position/gamma";
  param_names[7] = "/position/theta";
  param_names[8] = NULL;

  if (!hyscan_db_param_get (db, param_id, NULL, param_names, param_values))
    return FALSE;

  if ((g_variant_get_int64 (param_values[0]) != RAW_CHANNEL_SCHEMA_ID) ||
      (g_variant_get_int64 (param_values[1]) != RAW_CHANNEL_SCHEMA_VERSION) )
    {
      goto exit;
    }

  position->x = g_variant_get_double (param_values[2]);
  position->y = g_variant_get_double (param_values[3]);
  position->z = g_variant_get_double (param_values[4]);
  position->psi = g_variant_get_double (param_values[5]);
  position->gamma = g_variant_get_double (param_values[6]);
  position->theta = g_variant_get_double (param_values[7]);

  status = TRUE;

exit:
  g_variant_unref (param_values[0]);
  g_variant_unref (param_values[1]);
  g_variant_unref (param_values[2]);
  g_variant_unref (param_values[3]);
  g_variant_unref (param_values[4]);
  g_variant_unref (param_values[5]);
  g_variant_unref (param_values[6]);
  g_variant_unref (param_values[7]);

  return status;
}

/* Функция загружает параметры акустических данных. */
static gboolean
hyscan_raw_data_load_data_params (HyScanDB          *db,
                                  gint32             param_id,
                                  HyScanRawDataInfo *info)
{
  const gchar *param_names[11];
  GVariant *param_values[11];
  gboolean status = FALSE;

  param_names[0] = "/schema/id";
  param_names[1] = "/schema/version";
  param_names[2] = "/data/type";
  param_names[3] = "/data/rate";
  param_names[4] = "/antenna/offset/vertical";
  param_names[5] = "/antenna/offset/horizontal";
  param_names[6] = "/antenna/pattern/vertical";
  param_names[7] = "/antenna/pattern/horizontal";
  param_names[8] = "/adc/vref";
  param_names[9] = "/adc/offset";
  param_names[10] = NULL;

  if (!hyscan_db_param_get (db, param_id, NULL, param_names, param_values))
    return FALSE;

  if ((g_variant_get_int64 (param_values[0]) != RAW_CHANNEL_SCHEMA_ID) ||
      (g_variant_get_int64 (param_values[1]) != RAW_CHANNEL_SCHEMA_VERSION) )
    {
      goto exit;
    }

  info->data.type = hyscan_data_get_type_by_name (g_variant_get_string (param_values[2], NULL));
  info->data.rate = g_variant_get_double (param_values[3]);
  info->antenna.offset.vertical = g_variant_get_double (param_values[4]);
  info->antenna.offset.horizontal = g_variant_get_double (param_values[5]);
  info->antenna.pattern.vertical = g_variant_get_double (param_values[6]);
  info->antenna.pattern.horizontal = g_variant_get_double (param_values[7]);
  info->adc.vref = g_variant_get_double (param_values[8]);
  info->adc.offset = g_variant_get_int64 (param_values[9]);

  status = TRUE;

exit:
  g_variant_unref (param_values[0]);
  g_variant_unref (param_values[1]);
  g_variant_unref (param_values[2]);
  g_variant_unref (param_values[3]);
  g_variant_unref (param_values[4]);
  g_variant_unref (param_values[5]);
  g_variant_unref (param_values[6]);
  g_variant_unref (param_values[7]);
  g_variant_unref (param_values[8]);
  g_variant_unref (param_values[9]);

  return status;
}

/* Функция загружает параметры канала с образами сигналов. */
static gboolean
hyscan_raw_data_check_addon_params (HyScanDB       *db,
                                    gint32          param_id,
                                    gint64          schema_id,
                                    gdouble         data_rate,
                                    HyScanDataType  data_type)
{
  const gchar *param_names[5];
  GVariant *param_values[5];
  gboolean status = FALSE;

  param_names[0] = "/schema/id";
  param_names[1] = "/schema/version";
  param_names[2] = "/data/type";
  param_names[3] = "/data/rate";
  param_names[4] = NULL;

  if (!hyscan_db_param_get (db, param_id, NULL, param_names, param_values))
    return FALSE;

  if ((g_variant_get_int64 (param_values[0]) == schema_id) &&
      (g_variant_get_int64 (param_values[1]) == TRACK_SCHEMA_VERSION) &&
      (fabs (g_variant_get_double (param_values[3]) - data_rate) < 1.0) &&
      (hyscan_data_get_type_by_name (g_variant_get_string (param_values[2], NULL)) == data_type))
    {
      status = TRUE;
    }

  g_variant_unref (param_values[0]);
  g_variant_unref (param_values[1]);
  g_variant_unref (param_values[2]);
  g_variant_unref (param_values[3]);

  return status;
}

/* Функция создаёт ключ кэширования данных. */
static void
hyscan_raw_data_update_cache_key (HyScanRawDataPrivate *priv,
                                  gint                  data_type,
                                  guint32               index)
{
  const gchar *channel_name;
  const gchar *data_type_str;

  channel_name = hyscan_channel_get_name_by_types (priv->source_type, TRUE, priv->source_channel);

  if (data_type == DATA_TYPE_AMPLITUDE)
    data_type_str = "A";
  else
    data_type_str = "Q";

  if (priv->cache != NULL && priv->cache_key == NULL)
    {
      if (priv->cache_prefix != NULL)
        {
        priv->cache_key = g_strdup_printf ("%s.%s.%s.%s.%s.CV.A.0123456789",
                                           priv->db_uri,
                                           priv->cache_prefix,
                                           priv->project_name,
                                           priv->track_name,
                                           channel_name);
        }
      else
        {
        priv->cache_key = g_strdup_printf ("%s.%s.%s.%s.CV.A.0123456789",
                                           priv->db_uri,
                                           priv->project_name,
                                           priv->track_name,
                                           channel_name);
        }
      priv->cache_key_length = strlen (priv->cache_key);
    }

  if (priv->cache_prefix != NULL)
    {
      g_snprintf (priv->cache_key, priv->cache_key_length, "%s.%s.%s.%s.%s.%s.%s.%d",
                  priv->db_uri,
                  priv->cache_prefix,
                  priv->project_name,
                  priv->track_name,
                  channel_name,
                  priv->convolve ? "CV" : "NC",
                  data_type_str,
                  index);
    }
  else
    {
      g_snprintf (priv->cache_key, priv->cache_key_length, "%s.%s.%s.%s.%s.%s.%d",
                  priv->db_uri,
                  priv->project_name,
                  priv->track_name,
                  channel_name,
                  priv->convolve ? "CV" : "NC",
                  data_type_str,
                  index);
    }
}

/* Функция проверяет размер буферов и увеличивает его при необходимости. */
static void
hyscan_raw_data_buffer_realloc (HyScanRawDataPrivate *priv,
                                gint32                size)
{
  gsize data_buffer_size;

  if (priv->raw_buffer_size > size)
    return;

  priv->raw_buffer_size = size + 32;
  data_buffer_size = priv->raw_buffer_size / hyscan_data_get_point_size (priv->info.data.type);
  data_buffer_size *= sizeof (HyScanComplexFloat);

  priv->raw_buffer = g_realloc (priv->raw_buffer, priv->raw_buffer_size);
  priv->data_buffer = g_realloc (priv->data_buffer, data_buffer_size);
}

/* Функция считывает запись из канала данных в буфер. */
static guint32
hyscan_raw_data_read_raw_data (HyScanRawDataPrivate *priv,
                               gint32                channel_id,
                               guint32               index,
                               gint64               *time)
{
  guint32 io_size;
  gboolean status;

  /* Считываем данные канала. */
  io_size = priv->raw_buffer_size;
  status = hyscan_db_channel_get_data (priv->db, channel_id, index,
                                       priv->raw_buffer, &io_size, time);
  if (!status)
    return 0;

  /* Если размер считанных данных равен размеру буфера, запрашиваем реальный размер,
     увеличиваем размер буфера и считываем данные еще раз. */
  if (priv->raw_buffer_size == 0 || priv->raw_buffer_size == io_size)
    {
      status = hyscan_db_channel_get_data (priv->db, channel_id, index,
                                           NULL, &io_size, NULL);
      if (!status)
        return 0;

      hyscan_raw_data_buffer_realloc (priv, io_size);
      io_size = priv->raw_buffer_size;

      status = hyscan_db_channel_get_data (priv->db, channel_id, index,
                                           priv->raw_buffer, &io_size, time);
      if (!status)
        return 0;
    }

  return io_size;
}

/* Функция загружает образы сигналов для свёртки. */
static void
hyscan_raw_data_load_signals (HyScanRawDataPrivate *priv)
{
  guint32 first_signal_index;
  guint32 last_signal_index;
  guint64 signals_mod_count;

  gboolean status;
  guint32 i;

  /* Если нет канала с образами сигналов или сигналы не изменяются, выходим. */
  if (priv->signal_id < 0)
    return;

  /* Проверяем изменения в сигналах. */
  signals_mod_count = hyscan_db_get_mod_count (priv->db, priv->signal_id);
  if (priv->signals_mod_count == signals_mod_count)
    return;

  /* Массив образов сигналов. */
  if (priv->signals == NULL)
    priv->signals = g_array_new (TRUE, TRUE, sizeof (HyScanRawDataSignal));

  /* Проверяем индекс последнего загруженного образа сигнала. */
  status = hyscan_db_channel_get_data_range (priv->db, priv->signal_id,
                                             &first_signal_index, &last_signal_index);
  if (!status)
    return;
  if (priv->last_signal_index == last_signal_index)
    return;

  /* Загружаем "новые" образы сигналов. */
  i = priv->last_signal_index + 1;
  if (i < first_signal_index)
    i = first_signal_index;

  for (; i <= last_signal_index; i++)
    {
      gint32 io_size;
      HyScanRawDataSignal signal_info = {0};

      /* Считываем образ сигнала и проверяем его размер. */
      io_size = hyscan_raw_data_read_raw_data (priv, priv->signal_id, i, &signal_info.time);
      if (io_size == 0 || (io_size % sizeof(HyScanComplexFloat)) != 0)
        return;

      /* Объект свёртки. */
      if (io_size > sizeof (HyScanComplexFloat))
        {

          signal_info.image = g_memdup (priv->raw_buffer, io_size);
          signal_info.n_points = io_size / sizeof (HyScanComplexFloat);
          signal_info.convolution = hyscan_convolution_new ();
          hyscan_convolution_set_image (signal_info.convolution,
                                        signal_info.image,
                                        signal_info.n_points);
        }

      g_array_append_val (priv->signals, signal_info);
      priv->last_signal_index = i;
    }

  priv->signals_mod_count = signals_mod_count;

  /* Если запись в канал данных завершена, перестаём обновлять образы сигналов. */
  if (!hyscan_db_channel_is_writable (priv->db, priv->signal_id))
    {
      hyscan_db_close (priv->db, priv->signal_id);
      priv->signal_id = -1;
    }
}

/* Функция ищет образ сигнала для свёртки для указанного момента времени. */
static HyScanRawDataSignal *
hyscan_raw_data_find_signal (HyScanRawDataPrivate *priv,
                             gint64                time)
{
  gint i;

  if (priv->signals == NULL)
    return NULL;

  /* Ищем сигнал для момента времени time. */
  for (i = priv->signals->len - 1; i >= 0; i--)
    {
      HyScanRawDataSignal *signal = &g_array_index (priv->signals, HyScanRawDataSignal, i);
      if(time >= signal->time)
        return signal;
    }

  return NULL;
}

/* Функция считывает "сырые" акустические данные для указанного индекса,
   импортирует их в буфер данных и при необходимости осуществляет свёртку. */
static guint32
hyscan_raw_data_read_data (HyScanRawDataPrivate *priv,
                           guint32               index)
{
  guint32 io_size;
  guint32 n_points;
  guint32 point_size;

  /* Загружаем образы сигналов. */
  hyscan_raw_data_load_signals (priv);

  /* Считываем данные канала. */
  io_size = hyscan_raw_data_read_raw_data (priv, priv->channel_id, index, &priv->data_time);
  if (io_size == 0)
    return 0;

  /* Размер считанных данных должен быть кратен размеру точки. */
  point_size = hyscan_data_get_point_size (priv->info.data.type);
  if (io_size % point_size != 0)
    return 0;
  n_points = io_size / point_size;

  /* Импортируем данные в буфер обработки. */
  hyscan_data_import_complex_float (priv->info.data.type, priv->raw_buffer, io_size,
                                    priv->data_buffer, &n_points);

  /* Выполняем свёртку. */
  if (priv->convolve)
    {
      HyScanRawDataSignal *signal;

      signal = hyscan_raw_data_find_signal (priv, priv->data_time);
      if ((signal != NULL) && (signal->convolution != NULL))
        {
        if (!hyscan_convolution_convolve (signal->convolution, priv->data_buffer, n_points))
          return 0;
        }
    }

  return n_points;
}

/* Функция проверяет кэш на наличие данных указанного типа и если такие есть считывает их. */
static gboolean
hyscan_raw_data_check_cache (HyScanRawDataPrivate *priv,
                             gint                  data_type,
                             guint32               index,
                             gpointer              buffer,
                             guint32              *buffer_size,
                             gint64               *time)
{
  gint64 cached_time;
  gint32 time_size;
  gint32 io_size;

  gint data_type_size;

  gboolean status;

  if (priv->cache == NULL || buffer == NULL)
    return FALSE;

  if (data_type == DATA_TYPE_AMPLITUDE)
    data_type_size = sizeof (gfloat);
  else
    data_type_size = sizeof (HyScanComplexFloat);

  /* Ключ кэширования. */
  hyscan_raw_data_update_cache_key (priv, data_type, index);

  /* Ищем данные в кэше. */
  time_size = sizeof(cached_time);
  io_size = *buffer_size * data_type_size;
  status = hyscan_cache_get2 (priv->cache, priv->cache_key, NULL,
                              &cached_time, &time_size,
                              buffer, &io_size);
  if (!status)
    return FALSE;

  if (time_size != sizeof(cached_time) || io_size % data_type_size != 0)
    return FALSE;

  if (time != NULL)
    *time = cached_time;

  *buffer_size = io_size / data_type_size;

  return TRUE;
}

/* Функция создаёт новый объект обработки сырых данных. */
HyScanRawData *
hyscan_raw_data_new (HyScanDB         *db,
                     const gchar      *project_name,
                     const gchar      *track_name,
                     HyScanSourceType  source_type,
                     guint             source_channel)
{
  HyScanRawData *data;

  data = g_object_new (HYSCAN_TYPE_RAW_DATA,
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

/* Функция создаёт новый объект обработки шумов сырых данных. */
HyScanRawData *
hyscan_raw_data_noise_new (HyScanDB         *db,
                           const gchar      *project_name,
                           const gchar      *track_name,
                           HyScanSourceType  source_type,
                           guint             source_channel)
{
  HyScanRawData *data;

  data = g_object_new (HYSCAN_TYPE_RAW_DATA,
                       "db", db,
                       "project-name", project_name,
                       "track-name", track_name,
                       "source-type", source_type,
                       "source-channel", source_channel,
                       "noise", TRUE,
                       NULL);

  if (data->priv->channel_id <= 0)
    g_clear_object (&data);

  return data;
}

/* Функция задаёт используемый кэш. */
void
hyscan_raw_data_set_cache (HyScanRawData *data,
                           HyScanCache   *cache,
                           const gchar   *prefix)
{
  g_return_if_fail (HYSCAN_IS_RAW_DATA (data));

  g_clear_object (&data->priv->cache);
  g_clear_pointer (&data->priv->cache_prefix, g_free);
  g_clear_pointer (&data->priv->cache_key, g_free);

  if (cache == NULL)
    return;
  data->priv->cache = g_object_ref (cache);

  if (prefix == NULL)
    return;
  data->priv->cache_prefix = g_strdup (prefix);
}

/* Функция возвращает информацию о местоположении приёмной антенны гидролокатора. */
HyScanAntennaPosition
hyscan_raw_data_get_position (HyScanRawData *data)
{
  HyScanAntennaPosition zero;

  memset (&zero, 0, sizeof (HyScanAntennaPosition));

  g_return_val_if_fail (HYSCAN_IS_RAW_DATA (data), zero);

  return data->priv->position;
}

/* Функция возвращает параметры канала данных. */
HyScanRawDataInfo
hyscan_raw_data_get_info (HyScanRawData *data)
{
  HyScanRawDataInfo zero;

  memset (&zero, 0, sizeof (HyScanRawDataInfo));

  g_return_val_if_fail (HYSCAN_IS_RAW_DATA (data), zero);

  return data->priv->info;
}

/* Функция возвращает тип источника данных. */
HyScanSourceType
hyscan_raw_data_get_source (HyScanRawData *data)
{
  g_return_val_if_fail (HYSCAN_IS_RAW_DATA (data), HYSCAN_SOURCE_INVALID);

  return data->priv->source_type;
}

/* Функция возвращает номер канала для этого канала данных. */
guint
hyscan_raw_data_get_channel (HyScanRawData *data)
{
  g_return_val_if_fail (HYSCAN_IS_RAW_DATA (data), 0);

  return data->priv->source_channel;
}

/* Функция определяет возможность изменения акустических данных. */
gboolean
hyscan_raw_data_is_writable (HyScanRawData *data)
{
  g_return_val_if_fail (HYSCAN_IS_RAW_DATA (data), FALSE);

  return hyscan_db_channel_is_writable (data->priv->db, data->priv->channel_id);
}

/* Функция возвращает диапазон значений индексов записанных данных. */
gboolean
hyscan_raw_data_get_range (HyScanRawData *data,
                           guint32       *first_index,
                           guint32       *last_index)
{
  g_return_val_if_fail (HYSCAN_IS_RAW_DATA (data), FALSE);

  return hyscan_db_channel_get_data_range (data->priv->db, data->priv->channel_id,
                                           first_index, last_index);
}

/* Функция ищет индекс данных для указанного момента времени. */
HyScanDBFindStatus
hyscan_raw_data_find_data (HyScanRawData *data,
                           gint64         time,
                           guint32       *lindex,
                           guint32       *rindex,
                           gint64        *ltime,
                           gint64        *rtime)
{
  g_return_val_if_fail (HYSCAN_IS_RAW_DATA (data), HYSCAN_DB_FIND_FAIL);

  return hyscan_db_channel_find_data (data->priv->db, data->priv->channel_id,
                                      time, lindex, rindex, ltime, rtime);
}

/* Функция возвращает число точек данных для указанного индекса. */
guint32
hyscan_raw_data_get_values_count (HyScanRawData *data,
                                  guint32        index)
{
  guint32 dsize;

  g_return_val_if_fail (HYSCAN_IS_RAW_DATA (data), 0);

  if (hyscan_db_channel_get_data (data->priv->db, data->priv->channel_id,
                                  index, NULL, &dsize, NULL))
    {
      dsize /= hyscan_data_get_point_size (data->priv->info.data.type);
    }
  else
    {
      dsize = 0;
    }

  return dsize;
}

/* Функция возвращает размер образа сигнала для указанного индекса. */
guint32
hyscan_raw_data_get_signal_size (HyScanRawData *data,
                                 guint32        index)
{
  gint64 dtime;
  HyScanRawDataSignal *signal;

  g_return_val_if_fail (HYSCAN_IS_RAW_DATA (data), 0);

  hyscan_raw_data_load_signals (data->priv);

  dtime = hyscan_raw_data_get_time (data, index);
  signal = hyscan_raw_data_find_signal (data->priv, dtime);
  if (signal == NULL)
    return 0;

  return signal->n_points;
}

/* Функция возвращает время приёма данных для указанного индекса. */
gint64
hyscan_raw_data_get_time (HyScanRawData *data,
                          guint32        index)
{
  guint32 dsize;
  gint64 time;

  g_return_val_if_fail (HYSCAN_IS_RAW_DATA (data), -1);

  if (!hyscan_db_channel_get_data (data->priv->db, data->priv->channel_id,
                                   index, NULL, &dsize, &time))
    {
      time = -1;
    }

  return time;
}

/* Функция устанавливает необходимость выполнения свёртки акустических данных. */
void
hyscan_raw_data_set_convolve (HyScanRawData *data,
                              gboolean       convolve)
{
  g_return_if_fail (HYSCAN_IS_RAW_DATA (data));

  data->priv->convolve = convolve;
}

/* Функция возвращает образ сигнала. */
gboolean
hyscan_raw_data_get_signal_image (HyScanRawData      *data,
                                  guint32             index,
                                  HyScanComplexFloat *buffer,
                                  guint32            *buffer_size,
                                  gint64             *time)
{
  gint64 dtime;
  HyScanRawDataSignal *signal;

  g_return_val_if_fail (HYSCAN_IS_RAW_DATA (data), FALSE);

  hyscan_raw_data_load_signals (data->priv);
  dtime = hyscan_raw_data_get_time (data, index);
  signal = hyscan_raw_data_find_signal (data->priv, dtime);
  if (signal == NULL)
    return FALSE;

  *buffer_size = MIN (*buffer_size, signal->n_points);
  memcpy (buffer, signal->image, *buffer_size * sizeof(HyScanComplexFloat));

  if (time != NULL)
    *time = signal->time;

  return TRUE;
}

/* Функция возвращает значения коэффициентов усиления. */
gboolean
hyscan_raw_data_get_tvg_values (HyScanRawData *data,
                                guint32        index,
                                gfloat        *buffer,
                                guint32       *buffer_size,
                                gint64        *time)
{
  HyScanRawDataPrivate *priv;

  gint64 dtime;
  gint64 ttime;
  gint64 ltime;
  gint64 rtime;
  guint32 tindex;
  guint32 lindex;
  guint32 rindex;
  guint32 tsize;

  HyScanDBFindStatus find_status;
  gboolean status;

  g_return_val_if_fail (HYSCAN_IS_RAW_DATA (data), FALSE);

  priv = data->priv;

  /* Ищем индекс записи с нужными коэффициентами ВАРУ. */
  while (TRUE)
    {
      dtime = hyscan_raw_data_get_time (data, index);
      find_status = hyscan_db_channel_find_data (priv->db, priv->tvg_id, dtime,
                                                 &lindex, &rindex, &ltime, &rtime);

      /* Коэффициенты ВАРУ не найдены. */
      if ((find_status == HYSCAN_DB_FIND_FAIL) || (find_status == HYSCAN_DB_FIND_LESS))
        return FALSE;

      /* Используется последняя запись. Необходимо проверить, что не появились ещё
       * данные между моментом поиска и определением последнего индекса. */
      if (find_status == HYSCAN_DB_FIND_GREATER)
        {
          status = hyscan_db_channel_get_data_range (priv->db, priv->tvg_id, NULL, &tindex);
          if (!status)
            return FALSE;

          status = hyscan_db_channel_get_data (data->priv->db, data->priv->tvg_id,
                                               tindex, NULL, &tsize, &ttime);
          if (!status)
            return FALSE;

          if (dtime > ttime)
            break;
        }

      /* Нам нужна запись с меньшим временем. */
      tindex = lindex;

      status = hyscan_db_channel_get_data (data->priv->db, data->priv->tvg_id,
                                           tindex, NULL, &tsize, NULL);
      if (!status)
        return FALSE;

      break;
    }

  /* Считываем коэффициенты ВАРУ. */
  tsize = *buffer_size * sizeof (gfloat);
  status = hyscan_db_channel_get_data (data->priv->db, data->priv->tvg_id,
                                       tindex, buffer, &tsize, time);
  if (!status)
    return FALSE;

  *buffer_size = tsize / sizeof (gfloat);

  return TRUE;
}

/* Функция возвращает значения амплитуды акустического сигнала. */
gboolean
hyscan_raw_data_get_amplitude_values (HyScanRawData *data,
                                      guint32        index,
                                      gfloat        *buffer,
                                      guint32       *buffer_size,
                                      gint64        *time)
{
  HyScanRawDataPrivate *priv;

  gfloat *amplitude;
  guint32 n_points;
  guint32 i;

  g_return_val_if_fail (HYSCAN_IS_RAW_DATA (data), FALSE);

  priv = data->priv;

  /* Проверяем наличие данных в кэше. */
  if (hyscan_raw_data_check_cache (priv, DATA_TYPE_AMPLITUDE, index, buffer, buffer_size, time))
    return TRUE;

  /* Считываем данные канала. */
  n_points = hyscan_raw_data_read_data (priv, index);
  if (n_points == 0)
    return FALSE;

  /* Возвращаем амплитуду. */
  *buffer_size = MIN (*buffer_size, n_points);
  amplitude = (gpointer) priv->data_buffer;
  for (i = 0; i < n_points; i++)
    {
      gfloat re = priv->data_buffer[i].re;
      gfloat im = priv->data_buffer[i].im;
      amplitude[i] = sqrtf (re * re + im * im);
    }
  memcpy (buffer, amplitude, *buffer_size * sizeof(gfloat));

  /* Сохраняем данные в кэше. */
  if (priv->cache != NULL && buffer != NULL)
    {
      hyscan_cache_set2 (priv->cache, priv->cache_key, NULL,
                         &priv->data_time, sizeof(priv->data_time),
                         amplitude, n_points * sizeof(gfloat));
    }

  if (time != NULL)
    *time = priv->data_time;

  return TRUE;
}

/* Функция возвращает квадратурные отсчёты акустического сигнала. */
gboolean
hyscan_raw_data_get_quadrature_values (HyScanRawData      *data,
                                       guint32             index,
                                       HyScanComplexFloat *buffer,
                                       guint32            *buffer_size,
                                       gint64             *time)
{
  HyScanRawDataPrivate *priv;

  guint32 n_points;

  g_return_val_if_fail (HYSCAN_IS_RAW_DATA (data), FALSE);

  priv = data->priv;

  /* Проверяем наличие данных в кэше. */
  if (hyscan_raw_data_check_cache (priv, DATA_TYPE_QUADRATURE, index, buffer, buffer_size, time))
    return TRUE;

  /* Считываем данные канала. */
  n_points = hyscan_raw_data_read_data (priv, index);
  if (n_points == 0)
    return FALSE;

  /* Возвращаем квадратурные отсчёты. */
  *buffer_size = MIN (*buffer_size, n_points);
  memcpy (buffer, priv->data_buffer, *buffer_size * sizeof(HyScanComplexFloat));

  /* Сохраняем данные в кэше. */
  if (priv->cache != NULL && buffer != NULL)
    {
      hyscan_cache_set2 (priv->cache, priv->cache_key, NULL,
                         &priv->data_time, sizeof(priv->data_time),
                         priv->data_buffer, n_points * sizeof(HyScanComplexFloat));
    }

  if (time != NULL)
    *time = priv->data_time;

  return TRUE;
}
