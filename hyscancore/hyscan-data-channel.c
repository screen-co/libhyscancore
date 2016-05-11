/*
 * \file hyscan-data-channel.c
 *
 * \brief Исходный файл класса обработки акустических данных
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2015
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-data-channel.h"
#include "hyscan-convolution.h"

#include <math.h>
#include <string.h>

/* Версия API канала гидролокационных данных. */
#define DATA_CHANNEL_API                       20160500

/* Постфикс названия канала с образцами сигналов. */
#define SIGNALS_CHANNEL_POSTFIX                "signals"

enum
{
  PROP_O,
  PROP_DB,
  PROP_PROJECT_NAME,
  PROP_TRACK_NAME,
  PROP_CHANNEL_NAME,
  PROP_CACHE,
  PROP_CACHE_PREFIX
};

enum
{
  DATA_TYPE_AMPLITUDE,
  DATA_TYPE_QUADRATURE
};

/* Структура с описанием параметров образца сигнала для свёртки. */
typedef struct
{
  gint64               time;                                   /* Момент времени начала действия сигнала. */
  HyScanConvolution   *convolution;                            /* Образец сигнала для свёртки. */
} HyScanDataChannelSignal;

struct _HyScanDataChannelPrivate
{
  HyScanDB            *db;                                     /* Интерфейс базы данных. */
  gchar               *db_uri;                                 /* URI базы данных. */

  gchar               *project_name;                           /* Название проекта. */
  gchar               *track_name;                             /* Название галса. */
  gchar               *channel_name;                           /* Название канала данных. */

  HyScanCache         *cache;                                  /* Интерфейс системы кэширования. */
  gchar               *cache_prefix;                           /* Префикс ключа кэширования. */

  HyScanDataChannelInfo info;                                  /* Параметры канала данных. */

  gint32               channel_id;                             /* Идентификатор открытого канала данных. */
  gint32               signal_id;                              /* Идентификатор открытого канала с образцами сигналов. */

  gpointer             raw_buffer;                             /* Буфер для чтения "сырых" данных канала. */
  gint32               raw_buffer_size;                        /* Размер буфера в байтах. */

  HyScanComplexFloat  *data_buffer;                            /* Буфер для обработки данных. */
  gint64               data_time;                              /* Метка времени обрабатываемых данных. */

  GArray              *signals;                                /* Массив объектов свёртки. */
  gint32               last_signal_index;                      /* Индекс последнего загруженного сигнала. */
  guint64              signals_mod_count;                      /* Номер изменений сигналов. */
  gint                 convolve;                               /* Выполнять или нет свёртку. */

  gchar               *cache_key;                              /* Ключ кэширования. */
  gint                 cache_key_length;                       /* Максимальная длина ключа. */
};

static void            hyscan_data_channel_set_property        (GObject                       *object,
                                                                guint                          prop_id,
                                                                const GValue                  *value,
                                                                GParamSpec                    *pspec);
static void            hyscan_data_channel_object_constructed  (GObject                       *object);
static void            hyscan_data_channel_object_finalize     (GObject                       *object);

static gboolean        hyscan_data_channel_load_data_params    (HyScanDB                      *db,
                                                                gint32                         param_id,
                                                                HyScanDataChannelInfo         *info);
static gboolean        hyscan_data_channel_load_signals_params (HyScanDB                      *db,
                                                                gint32                         param_id,
                                                                gdouble                       *discretization_frequency);

static void            hyscan_data_channel_update_cache_key    (HyScanDataChannelPrivate      *priv,
                                                                gint                           data_type,
                                                                gint32                         index);
static void            hyscan_data_channel_buffer_realloc      (HyScanDataChannelPrivate      *priv,
                                                                gint32                         size);
static gint32          hyscan_data_channel_read_raw_data       (HyScanDataChannelPrivate      *priv,
                                                                gint32                         channel_id,
                                                                gint32                         index,
                                                                gint64                        *time);

static void            hyscan_data_channel_load_signals        (HyScanDataChannelPrivate      *priv);
static HyScanConvolution *hyscan_data_channel_find_signal      (HyScanDataChannelPrivate      *priv,
                                                                gint64                         time);

static gint32          hyscan_data_channel_read_data           (HyScanDataChannelPrivate      *priv,
                                                                gint32                         index);
static gboolean        hyscan_data_channel_check_cache         (HyScanDataChannelPrivate      *priv,
                                                                gint32                         index,
                                                                gint                           data_type,
                                                                gpointer                       buffer,
                                                                gint32                        *buffer_size,
                                                                gint64                        *time);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanDataChannel, hyscan_data_channel, G_TYPE_OBJECT);

static void
hyscan_data_channel_class_init (HyScanDataChannelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_data_channel_set_property;

  object_class->constructed = hyscan_data_channel_object_constructed;
  object_class->finalize = hyscan_data_channel_object_finalize;

  g_object_class_install_property (object_class, PROP_DB,
    g_param_spec_object ("db", "DB", "HyScanDB interface", HYSCAN_TYPE_DB,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_PROJECT_NAME,
    g_param_spec_string ("project-name", "ProjectName", "Project name", NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_TRACK_NAME,
    g_param_spec_string ("track-name", "TrackName", "Track name", NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_CHANNEL_NAME,
    g_param_spec_string ("channel-name", "ChannelName", "Channel name", NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_CACHE,
    g_param_spec_object ("cache", "Cache", "HyScanCache interface", HYSCAN_TYPE_CACHE,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_CACHE_PREFIX,
    g_param_spec_string ("cache-prefix", "CachePrefix", "Cache key prefix", NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_data_channel_init (HyScanDataChannel *dchannel)
{
  dchannel->priv = hyscan_data_channel_get_instance_private (dchannel);
}

static void
hyscan_data_channel_set_property (GObject *object,
                                  guint prop_id,
                                  const GValue *value,
                                  GParamSpec *pspec)
{
  HyScanDataChannel *dchannel = HYSCAN_DATA_CHANNEL (object);
  HyScanDataChannelPrivate *priv = dchannel->priv;

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

    case PROP_CHANNEL_NAME:
      priv->channel_name = g_value_dup_string (value);
      break;

    case PROP_CACHE:
      priv->cache = g_value_dup_object (value);
      break;

    case PROP_CACHE_PREFIX:
      priv->cache_prefix = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (dchannel, prop_id, pspec);
      break;
    }
}

static void
hyscan_data_channel_object_constructed (GObject *object)
{
  HyScanDataChannel *dchannel = HYSCAN_DATA_CHANNEL (object);
  HyScanDataChannelPrivate *priv = dchannel->priv;

  gint32 project_id = -1;
  gint32 track_id = -1;
  gint32 param_id = -1;

  gchar *signals_name = NULL;
  gdouble signal_discretization_frequency;

  gboolean status = FALSE;

  priv->channel_id = -1;
  priv->signal_id = -1;
  priv->last_signal_index = -1;

  if (priv->db == NULL)
    goto exit;

  priv->db_uri = hyscan_db_get_uri (priv->db);

  /* Названия проекта, галса, канала данных. */
  if (priv->project_name == NULL || priv->track_name == NULL|| priv->channel_name == NULL)
    {
      if (priv->project_name == NULL)
        g_warning ("HyScanDataChannel: unknown project name");
      if (priv->track_name == NULL)
        g_warning ("HyScanDataChannel: unknown track name");
      if (priv->channel_name == NULL)
        g_warning ("HyScanDataChannel: unknown channel name");
      goto exit;
    }

  project_id = hyscan_db_project_open (priv->db, priv->project_name);
  if (project_id < 0)
    {
      g_warning ("HyScanDataChannel: can't open project '%s'",
                 priv->project_name);
      goto exit;
    }

  track_id = hyscan_db_track_open (priv->db, project_id, priv->track_name);
  if (track_id < 0)
    {
      g_warning ("HyScanDataChannel: can't open track '%s.%s'",
                 priv->project_name, priv->track_name);
      goto exit;
    }

  priv->channel_id = hyscan_db_channel_open (priv->db, track_id, priv->channel_name);
  if (priv->channel_id < 0)
    {
      g_warning ("HyScanDataChannel: can't open channel '%s.%s.%s'",
                 priv->project_name, priv->track_name, priv->channel_name);
      goto exit;
    }

  /* Параметры канала данных. */
  param_id = hyscan_db_channel_param_open (priv->db, priv->channel_id);
  if (param_id < 0)
    {
      g_warning ("HyScanDataChannel: can't open channel '%s.%s.%s' parameters",
                 priv->project_name, priv->track_name, priv->channel_name);
      goto exit;
    }

  if (!hyscan_data_channel_load_data_params (priv->db, param_id, &priv->info))
    {
      g_warning ("HyScanDataChannel: error in channel '%s.%s.%s' parameters",
                 priv->project_name, priv->track_name, priv->channel_name);
      goto exit;
    }

  hyscan_db_close (priv->db, param_id);
  param_id = -1;

  /* Образцы сигналов для свёртки. */
  signals_name = g_strdup_printf ("%s.%s", priv->channel_name, SIGNALS_CHANNEL_POSTFIX);
  if (hyscan_db_is_exist (priv->db, priv->project_name, priv->track_name, signals_name))
    priv->signal_id = hyscan_db_channel_open (priv->db, track_id, signals_name);
  g_free (signals_name);

  /* Свёртка не нужна. */
  if (priv->signal_id < 0)
    {
      priv->convolve = FALSE;
      status = TRUE;
      goto exit;
    }

  /* Параметры канала с образцами сигналов. */
  param_id = hyscan_db_channel_param_open (priv->db, priv->signal_id);
  if (param_id < 0)
    {
      g_warning ("HyScanDataChannel: can't open channel '%s.%s.%s.%s' parameters",
                 priv->project_name, priv->track_name, priv->channel_name,
                 SIGNALS_CHANNEL_POSTFIX);
      goto exit;
    }

  if (!hyscan_data_channel_load_signals_params (priv->db, param_id, &signal_discretization_frequency))
    {
      g_warning ("HyScanDataChannel: error in signals '%s.%s.%s' parameters",
                 priv->project_name, priv->track_name, priv->channel_name);
      goto exit;
    }
  else
    {
      if (fabs (priv->info.discretization_frequency - signal_discretization_frequency) > 0.001)
        {
          g_warning ("HyScanDataChannel: '%s.%s.%s.%s': discretization frequency mismatch",
                     priv->project_name, priv->track_name, priv->channel_name,
                     SIGNALS_CHANNEL_POSTFIX);
          goto exit;
        }

      priv->signals_mod_count = hyscan_db_get_mod_count (priv->db, priv->signal_id) - 1;
      hyscan_data_channel_load_signals (priv);

      priv->convolve = TRUE;
    }

  hyscan_db_close (priv->db, param_id);
  param_id = -1;

  status = TRUE;

exit:
  if (!status)
    {
      if (priv->channel_id > 0)
        hyscan_db_close (priv->db, priv->channel_id);
      if (priv->signal_id > 0)
        hyscan_db_close (priv->db, priv->signal_id);
      priv->channel_id = -1;
      priv->signal_id = -1;
    }

  if (project_id > 0)
    hyscan_db_close (priv->db, project_id);
  if (track_id > 0)
    hyscan_db_close (priv->db, track_id);
  if (param_id > 0)
    hyscan_db_close (priv->db, param_id);
}

static void
hyscan_data_channel_object_finalize (GObject *object)
{
  HyScanDataChannel *dchannel = HYSCAN_DATA_CHANNEL (object);
  HyScanDataChannelPrivate *priv = dchannel->priv;

  guint i;

  if (priv->channel_id > 0)
    hyscan_db_close (priv->db, priv->channel_id);
  if (priv->signal_id > 0)
    hyscan_db_close (priv->db, priv->signal_id);

  if (priv->signals != NULL)
    {
      for (i = 0; i < priv->signals->len; i++)
        {
          HyScanDataChannelSignal *signal = &g_array_index (priv->signals,
                                                            HyScanDataChannelSignal, i);
          g_clear_object (&signal->convolution);
        }
      g_array_unref (priv->signals);
    }
  priv->last_signal_index = -1;

  g_free (priv->project_name);
  g_free (priv->track_name);
  g_free (priv->channel_name);

  g_free (priv->db_uri);
  g_free (priv->cache_key);
  g_free (priv->cache_prefix);

  g_free (priv->raw_buffer);
  g_free (priv->data_buffer);

  g_clear_object (&priv->db);
  g_clear_object (&priv->cache);

  G_OBJECT_CLASS (hyscan_data_channel_parent_class)->finalize (object);
}

/* Функция загружает параметры канала данных. */
static gboolean
hyscan_data_channel_load_data_params (HyScanDB              *db,
                                      gint32                 param_id,
                                      HyScanDataChannelInfo *info)
{
  gchar *discretization_type;
  gint64 api_version;
  gboolean status;

  status = hyscan_db_param_get_integer (db, param_id, NULL, "/data-version", &api_version);
  if (!status || ((api_version / 100) != (DATA_CHANNEL_API / 100)))
    return FALSE;

  discretization_type = hyscan_db_param_get_string (db, param_id, NULL, "/discretization/type");
  info->discretization_type = hyscan_data_get_type_by_name (discretization_type);
  g_free (discretization_type);
  if (info->discretization_type == HYSCAN_DATA_INVALID)
    return FALSE;

  if (!hyscan_db_param_get_double (db, param_id, NULL, "/discretization/frequency", &info->discretization_frequency))
    return FALSE;

  if (!hyscan_db_param_get_double (db, param_id, NULL, "/pattern/vertical", &info->vertical_pattern))
    return FALSE;

  if (!hyscan_db_param_get_double (db, param_id, NULL, "/pattern/horizontal", &info->horizontal_pattern))
    return FALSE;

  if (!hyscan_db_param_get_double (db, param_id, NULL, "/position/x", &info->x))
    return FALSE;

  if (!hyscan_db_param_get_double (db, param_id, NULL, "/position/y", &info->y))
    return FALSE;

  if (!hyscan_db_param_get_double (db, param_id, NULL, "/position/z", &info->z))
    return FALSE;

  if (!hyscan_db_param_get_double (db, param_id, NULL, "/orientation/psi", &info->psi))
    return FALSE;

  if (!hyscan_db_param_get_double (db, param_id, NULL, "/orientation/gamma", &info->gamma))
    return FALSE;

  if (!hyscan_db_param_get_double (db, param_id, NULL, "/orientation/theta", &info->theta))
    return FALSE;

  return TRUE;
}

/* Функция загружает параметры канала с образцами сигналов. */
static gboolean
hyscan_data_channel_load_signals_params (HyScanDB *db,
                                         gint32    param_id,
                                         gdouble  *discretization_frequency)
{
  HyScanDataType discretization_type;
  gchar *discretization_type_str;
  gint64 api_version;
  gboolean status;

  status = hyscan_db_param_get_integer (db, param_id, NULL, "/signal-version", &api_version);
  if (!status || ((api_version / 100) != (DATA_CHANNEL_API / 100)))
    return FALSE;

  discretization_type_str = hyscan_db_param_get_string (db, param_id, NULL, "/discretization/type");
  discretization_type = hyscan_data_get_type_by_name (discretization_type_str);
  g_free (discretization_type_str);
  if (discretization_type != HYSCAN_DATA_COMPLEX_FLOAT)
    return FALSE;

  status = hyscan_db_param_get_double (db, param_id, NULL,
                                       "/discretization/frequency", discretization_frequency);
  if (!status)
    return FALSE;

  return TRUE;
}

/* Функция создаёт ключ кэширования данных. */
static void
hyscan_data_channel_update_cache_key (HyScanDataChannelPrivate *priv,
                                      gint                      data_type,
                                      gint32                    index)
{
  gchar *data_type_str;

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
                                           priv->channel_name);
        }
      else
        {
        priv->cache_key = g_strdup_printf ("%s.%s.%s.%s.CV.A.0123456789",
                                           priv->db_uri,
                                           priv->project_name,
                                           priv->track_name,
                                           priv->channel_name);
        }
      priv->cache_key_length = strlen (priv->cache_key);
    }

  if (priv->cache_prefix)
    {
      g_snprintf (priv->cache_key, priv->cache_key_length, "%s.%s.%s.%s.%s.%s.%s.%d",
                  priv->db_uri,
                  priv->cache_prefix,
                  priv->project_name,
                  priv->track_name,
                  priv->channel_name,
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
                  priv->channel_name,
                  priv->convolve ? "CV" : "NC",
                  data_type_str,
                  index);
    }
}

/* Функция проверяет размер буферов и увеличивает его при необходимости. */
static void
hyscan_data_channel_buffer_realloc (HyScanDataChannelPrivate *priv,
                                    gint32                    size)
{
  gsize data_buffer_size;

  if (priv->raw_buffer_size > size)
    return;

  priv->raw_buffer_size = size + 32;
  data_buffer_size = priv->raw_buffer_size / hyscan_data_get_point_size (priv->info.discretization_type);
  data_buffer_size *= sizeof (HyScanComplexFloat);

  priv->raw_buffer = g_realloc (priv->raw_buffer, priv->raw_buffer_size);
  priv->data_buffer = g_realloc (priv->data_buffer, data_buffer_size);
}

/* Функция считывает запись из канала данных в буфер. */
static gint32
hyscan_data_channel_read_raw_data (HyScanDataChannelPrivate *priv,
                                   gint32                    channel_id,
                                   gint32                    index,
                                   gint64                   *time)
{
  gint32 io_size;
  gboolean status;

  /* Считываем данные канала. */
  io_size = priv->raw_buffer_size;
  status = hyscan_db_channel_get_data (priv->db, channel_id, index,
                                       priv->raw_buffer, &io_size, time);
  if (!status)
    return -1;

  /* Если размер считанных данных равен размеру буфера, запрашиваем реальный размер,
     увеличиваем размер буфера и считываем данные еще раз. */
  if (priv->raw_buffer_size == 0 || priv->raw_buffer_size == io_size)
    {
      status = hyscan_db_channel_get_data (priv->db, channel_id, index,
                                           NULL, &io_size, NULL);
      if (!status)
        return -1;

      hyscan_data_channel_buffer_realloc (priv, io_size);
      io_size = priv->raw_buffer_size;

      status = hyscan_db_channel_get_data (priv->db, channel_id, index,
                                           priv->raw_buffer, &io_size, time);
      if (!status)
        return -1;
    }

  return io_size;
}

/* Функция загружает образцы сигналов для свёртки. */
#warning "Don't check signals in read only mode"
static void
hyscan_data_channel_load_signals (HyScanDataChannelPrivate *priv)
{
  gint32 first_signal_index;
  gint32 last_signal_index;
  guint64 signals_mod_count;

  gboolean status;
  gint i;
  /* Если нет канала с образцами сигналов, выходим. */
  if (priv->signal_id < 0)
    return;

  /* Проверяем изменения в сигналах. */
  signals_mod_count = hyscan_db_get_mod_count (priv->db, priv->signal_id);
  if (priv->signals_mod_count == signals_mod_count)
    return;

  /* Массив образцов сигналов. */
  if (priv->signals == NULL)
    priv->signals = g_array_new (TRUE, TRUE, sizeof (HyScanDataChannelSignal));

  /* Проверяем индекс последнего загруженного образца сигнала. */
  status = hyscan_db_channel_get_data_range (priv->db, priv->signal_id,
                                             &first_signal_index, &last_signal_index);
  if (!status)
    return;
  if (priv->last_signal_index == last_signal_index)
    return;

  /* Загружаем "новые" образцы сигналов. */
  i = priv->last_signal_index + 1;
  if (i < first_signal_index)
    i = first_signal_index;

  for (; i <= last_signal_index; i++)
    {
      gint32 io_size;
      HyScanComplexFloat *signal;
      HyScanDataChannelSignal signal_info;

      /* Считываем образец сигнала и проверяем его размер. */
      io_size = hyscan_data_channel_read_raw_data (priv, priv->signal_id, i, &signal_info.time);
      if (io_size < 0 || (io_size % sizeof(HyScanComplexFloat)) != 0)
        return;

      signal = priv->raw_buffer;

      /* Признак отключенной свёртки - тональный сигнал. */
      if (io_size == sizeof (HyScanComplexFloat) &&
          ABS (signal[0].re) < 1e-07 &&
          ABS (signal[0].im) < 1e-07)
        {
          signal_info.convolution = NULL;
        }

      /* Объект свёртки. */
      else
        {
          signal_info.convolution = hyscan_convolution_new ();
          hyscan_convolution_set_image (signal_info.convolution,
                                        priv->raw_buffer,
                                        io_size / sizeof (HyScanComplexFloat));
        }

      g_array_append_val (priv->signals, signal_info);
      priv->last_signal_index = i;
    }

  priv->signals_mod_count = signals_mod_count;
}

/* Функция ищет образец сигнала для свёртки для указанного момента времени. */
static HyScanConvolution *
hyscan_data_channel_find_signal (HyScanDataChannelPrivate *priv,
                                 gint64                    time)
{
  gint i;

  if (priv->signals == NULL)
    return NULL;

  /* Ищем сигнал для момента времени time. */
  for (i = priv->signals->len - 1; i >= 0; i--)
    {
      HyScanDataChannelSignal *signal = &g_array_index (priv->signals, HyScanDataChannelSignal, i);
      if(time >= signal->time)
        return signal->convolution;
    }

  return NULL;
}

/* Функция считывает "сырые" акустические данные для указанного индекса,
   импортирует их в буфер данных и при необходимости осуществляет свёртку. */
static gint32
hyscan_data_channel_read_data (HyScanDataChannelPrivate *priv,
                               gint32                    index)
{
  gint32 io_size;
  gint32 n_points;
  gint32 point_size;

  HyScanConvolution *convolution;

  /* Проверка состояния объекта. */
  if (priv->channel_id < 0)
    return -1;

  /* Загружаем образцы сигналов. */
  hyscan_data_channel_load_signals (priv);

  /* Считываем данные канала. */
  io_size = hyscan_data_channel_read_raw_data (priv, priv->channel_id, index, &priv->data_time);
  if (io_size < 0)
    return -1;

  /* Размер считанных данных должен быть кратен размеру точки. */
  point_size = hyscan_data_get_point_size (priv->info.discretization_type);
  if (io_size % point_size != 0)
    return -1;
  n_points = io_size / point_size;

  /* Импортируем данные в буфер обработки. */
  hyscan_data_import_complex_float (priv->info.discretization_type, priv->raw_buffer, io_size,
                                    priv->data_buffer, &n_points);

  /* Выполняем свёртку. */
  if (priv->convolve)
    {
      convolution = hyscan_data_channel_find_signal (priv, priv->data_time);
      if (convolution != NULL)
        {
        if (!hyscan_convolution_convolve (convolution, priv->data_buffer, n_points))
          return -1;
        }
    }

  return n_points;
}

/* Функция проверяет кэш на наличие данных указанного типа и если такие есть считывает их. */
static gboolean
hyscan_data_channel_check_cache (HyScanDataChannelPrivate *priv,
                                 gint                      data_type,
                                 gint32                    index,
                                 gpointer                  buffer,
                                 gint32                   *buffer_size,
                                 gint64                   *time)
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
  hyscan_data_channel_update_cache_key (priv, data_type, index);

  /* Ищем данные в кэше. */
  time_size = sizeof(cached_time);
  io_size = *buffer_size * sizeof(gfloat);
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

/* Функция создаёт новый объект обработки акустических данных без использования кэша. */
HyScanDataChannel *
hyscan_data_channel_new (HyScanDB    *db,
                         const gchar *project_name,
                         const gchar *track_name,
                         const gchar *channel_name)
{
  return g_object_new (HYSCAN_TYPE_DATA_CHANNEL,
                       "db", db,
                       "project-name", project_name,
                       "track-name", track_name,
                       "channel-name", channel_name,
                       NULL);
}

/* Функция создаёт новый объект обработки акустических данных с использованием кэша. */
HyScanDataChannel *
hyscan_data_channel_new_with_cache (HyScanDB    *db,
                                    const gchar *project_name,
                                    const gchar *track_name,
                                    const gchar *channel_name,
                                    HyScanCache *cache)
{
  return g_object_new (HYSCAN_TYPE_DATA_CHANNEL,
                       "db", db,
                       "project-name", project_name,
                       "track-name", track_name,
                       "channel-name", channel_name,
                       "cache", cache,
                       NULL);
}

/* Функция создаёт новый объект обработки акустических данных с использованием кэша и префикса. */
HyScanDataChannel *
hyscan_data_channel_new_with_cache_prefix (HyScanDB    *db,
                                           const gchar *project_name,
                                           const gchar *track_name,
                                           const gchar *channel_name,
                                           HyScanCache *cache,
                                           const gchar *cache_prefix)
{
  return g_object_new (HYSCAN_TYPE_DATA_CHANNEL,
                       "db", db,
                       "project-name", project_name,
                       "track-name", track_name,
                       "channel-name", channel_name,
                       "cache", cache,
                       "cache-prefix", cache_prefix,
                       NULL);
}

/* Функция возвращает параметры канала данных. */
HyScanDataChannelInfo *
hyscan_data_channel_get_info (HyScanDataChannel *dchannel)
{
  HyScanDataChannelInfo *info;

  g_return_val_if_fail (HYSCAN_IS_DATA_CHANNEL (dchannel), HYSCAN_DATA_INVALID);

  if (dchannel->priv->channel_id < 0)
    return NULL;

  info = g_new (HyScanDataChannelInfo, 1);
  memcpy (info, &dchannel->priv->info, sizeof (HyScanDataChannelInfo));

  return info;
}

/* Функция возвращает диапазон значений индексов записанных данных. */
gboolean
hyscan_data_channel_get_range (HyScanDataChannel *dchannel,
                               gint32            *first_index,
                               gint32            *last_index)
{
  g_return_val_if_fail (HYSCAN_IS_DATA_CHANNEL (dchannel), FALSE);

  if (dchannel->priv->channel_id < 0)
    return FALSE;

  return hyscan_db_channel_get_data_range (dchannel->priv->db, dchannel->priv->channel_id,
                                           first_index, last_index);
}

/* Функция возвращает число точек данных для указанного индекса. */
gint32
hyscan_data_channel_get_values_count (HyScanDataChannel *dchannel,
                                      gint32             index)
{
  gint32 dsize = -1;

  g_return_val_if_fail (HYSCAN_IS_DATA_CHANNEL (dchannel), FALSE);

  if (dchannel->priv->channel_id < 0)
    return 0;

  if (hyscan_db_channel_get_data (dchannel->priv->db, dchannel->priv->channel_id,
                                  index, NULL, &dsize, NULL))
    {
      dsize /= hyscan_data_get_point_size (dchannel->priv->info.discretization_type);
    }

  return dsize;
}

/* Функция ищет индекс данных для указанного момента времени. */
gboolean
hyscan_data_channel_find_data (HyScanDataChannel *dchannel,
                               gint64             time,
                               gint32            *lindex,
                               gint32            *rindex,
                               gint64            *ltime,
                               gint64            *rtime)
{
  g_return_val_if_fail (HYSCAN_IS_DATA_CHANNEL (dchannel), FALSE);

  if (dchannel->priv->channel_id < 0)
    return FALSE;

  return hyscan_db_channel_find_data (dchannel->priv->db, dchannel->priv->channel_id,
                                      time, lindex, rindex, ltime, rtime);
}

/* Функция устанавливает необходимость выполнения свёртки акустических данных. */
void
hyscan_data_channel_set_convolve (HyScanDataChannel *dchannel,
                                  gboolean           convolve)
{
  g_return_if_fail (HYSCAN_IS_DATA_CHANNEL (dchannel));

  if (dchannel->priv->channel_id < 0)
    return;

  g_atomic_int_set (&dchannel->priv->convolve, convolve ? 1 : 0);
}

/* Функция возвращает значения амплитуды акустического сигнала. */
gboolean
hyscan_data_channel_get_amplitude_values (HyScanDataChannel *dchannel,
                                          gint32             index,
                                          gfloat            *buffer,
                                          gint32            *buffer_size,
                                          gint64            *time)
{
  HyScanDataChannelPrivate *priv;

  gfloat *amplitude;
  gint32 n_points;
  gint32 i;

  g_return_val_if_fail (HYSCAN_IS_DATA_CHANNEL (dchannel), FALSE);

  priv = dchannel->priv;

  if (priv->channel_id < 0)
    return FALSE;

  /* Проверяем наличие данных в кэше. */
  if (hyscan_data_channel_check_cache (priv, DATA_TYPE_AMPLITUDE, index, buffer, buffer_size, time))
    return TRUE;

  /* Считываем данные канала. */
  n_points = hyscan_data_channel_read_data (priv, index);
  if (n_points <= 0)
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
hyscan_data_channel_get_quadrature_values (HyScanDataChannel  *dchannel,
                                           gint32              index,
                                           HyScanComplexFloat *buffer,
                                           gint32             *buffer_size,
                                           gint64             *time)
{
  HyScanDataChannelPrivate *priv;

  gint32 n_points;

  g_return_val_if_fail (HYSCAN_IS_DATA_CHANNEL (dchannel), FALSE);

  priv = dchannel->priv;

  if (priv->channel_id < 0)
    return FALSE;

  /* Проверяем наличие данных в кэше. */
  if (hyscan_data_channel_check_cache (priv, DATA_TYPE_QUADRATURE, index, buffer, buffer_size, time))
    return TRUE;

  /* Считываем данные канала. */
  n_points = hyscan_data_channel_read_data (priv, index);
  if (n_points <= 0)
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
