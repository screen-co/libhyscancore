/*
 * \file hyscan-raw-data.c
 *
 * \brief Исходный файл класса обработки сырых данных
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2015
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-raw-data.h"
#include "hyscan-core-params.h"
#include <hyscan-convolution.h>
#include <hyscan-buffer.h>

#include <math.h>
#include <string.h>

#define CACHE_HEADER_MAGIC     0xf97603e8                      /* Идентификатор заголовка кэша. */

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
  DATA_TYPE_QUADRATURE,
  DATA_TYPE_TVG
};

/* Структура с образом сигнала и объектом свёртки. */
typedef struct
{
  gint64                       time;                           /* Время начала действия сигнала. */
  guint32                      index;                          /* Индекс начала действия сигнала. */
  HyScanComplexFloat          *image;                          /* Образ сигнала для свёртки. */
  guint32                      n_points;                       /* Число точек образа сигнала. */
  HyScanConvolution           *convolution;                    /* Объект свёртки. */
} HyScanRawDataSignal;

/* Структруа заголовка кэша. */
typedef struct
{
  guint32                      magic;                          /* Идентификатор заголовка. */
  guint32                      n_points;                       /* Число точек данных. */
  gint64                       time;                           /* Метка времени. */
} HyScanRawDataCacheHeader;

struct _HyScanRawDataPrivate
{
  HyScanDB                    *db;                             /* Интерфейс базы данных. */
  gchar                       *db_uri;                         /* URI базы данных. */

  gchar                       *project_name;                   /* Название проекта. */
  gchar                       *track_name;                     /* Название галса. */
  HyScanSourceType             source_type;                    /* Тип источника данных. */
  guint                        source_channel;                 /* Индекс канала данных. */
  gboolean                     noise;                          /* Открывать канал с шумами. */

  HyScanCache                 *cache;                          /* Интерфейс системы кэширования. */
  gchar                       *cache_prefix;                   /* Префикс ключа кэширования. */
  gchar                       *cache_key;                      /* Ключ кэширования. */
  gsize                        cache_key_length;               /* Максимальная длина ключа. */
  HyScanBuffer                *cache_buffer;                   /* Буфер заголовка кэша данных. */

  HyScanAntennaPosition        position;                       /* Местоположение приёмной антенны. */
  HyScanRawDataInfo            info;                           /* Параметры сырых данных. */

  gint32                       channel_id;                     /* Идентификатор открытого канала данных. */
  gint32                       signal_id;                      /* Идентификатор открытого канала с образами сигналов. */
  gint32                       tvg_id;                         /* Идентификатор открытого канала с коэффициентами усиления. */

  HyScanBuffer                *channel_buffer;                 /* Буфер канальных данных. */
  HyScanBuffer                *cplx_buffer;                    /* Буфер комплексных данных. */
  HyScanBuffer                *amp_buffer;                     /* Буфер амплитудных данных. */
  gint64                       data_time;                      /* Метка времени обрабатываемых данных. */

  GArray                      *signals;                        /* Массив сигналов. */
  guint32                      last_signal_index;              /* Индекс последнего загруженного сигнала. */
  guint64                      signals_mod_count;              /* Номер изменений сигналов. */
  gboolean                     convolve;                       /* Выполнять или нет свёртку. */
};

static void                    hyscan_raw_data_set_property            (GObject                       *object,
                                                                        guint                          prop_id,
                                                                        const GValue                  *value,
                                                                        GParamSpec                    *pspec);
static void                    hyscan_raw_data_object_constructed      (GObject                       *object);
static void                    hyscan_raw_data_object_finalize         (GObject                       *object);

static void                    hyscan_raw_data_update_cache_key        (HyScanRawDataPrivate          *priv,
                                                                        gint                           data_type,
                                                                        guint32                        index);

static void                    hyscan_raw_data_load_signals            (HyScanRawDataPrivate          *priv);
static HyScanRawDataSignal    *hyscan_raw_data_find_signal             (HyScanRawDataPrivate          *priv,
                                                                        guint32                        index);

static gboolean                hyscan_raw_data_read_raw_data           (HyScanRawDataPrivate          *priv,
                                                                        guint32                        index);

static gboolean                hyscan_raw_data_calc_amplitude          (HyScanRawDataPrivate          *priv);

static gboolean                hyscan_raw_data_check_cache             (HyScanRawDataPrivate          *priv,
                                                                        gint                           data_type,
                                                                        guint32                        index,
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

  const gchar *channel_name;
  gchar *signal_name = NULL;
  gchar *tvg_name = NULL;

  gboolean status = FALSE;

  priv->channel_id = -1;
  priv->signal_id = -1;
  priv->last_signal_index = -1;

  priv->cache_buffer = hyscan_buffer_new ();
  priv->channel_buffer = hyscan_buffer_new ();
  priv->cplx_buffer = hyscan_buffer_new ();
  priv->amp_buffer = hyscan_buffer_new ();

  /* Проверяем тип источника данных. */
  if (!hyscan_source_is_raw (priv->source_type))
    {
      g_warning ("HyScanRawData: unsupported source type %s",
                 hyscan_channel_get_name_by_types (priv->source_type, FALSE, 1));
      goto exit;
    }

  if (priv->db == NULL)
    {
      g_warning ("HyScanRawData: db is not specified");
      goto exit;
    }

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

  /* Проверяем, что в канале есть хотя бы одна запись. */
  if (!hyscan_db_channel_get_data_range (priv->db, priv->channel_id, NULL, NULL))
    goto exit;

  /* Параметры канала данных. */
  param_id = hyscan_db_channel_param_open (priv->db, priv->channel_id);
  if (param_id < 0)
    {
      g_warning ("HyScanRawData: '%s.%s.%s': can't open parameters",
                 priv->project_name, priv->track_name, channel_name);
      goto exit;
    }

  status = hyscan_core_params_load_antenna_position (priv->db, param_id,
                                                     RAW_CHANNEL_SCHEMA_ID,
                                                     RAW_CHANNEL_SCHEMA_VERSION,
                                                     &priv->position);
  if (!status)
    {
      g_warning ("HyScanRawData: '%s.%s.%s': can't read antenna position",
                 priv->project_name, priv->track_name, channel_name);
      goto exit;
    }

  if (!hyscan_core_params_load_raw_data_info (priv->db, param_id, &priv->info))
    {
      g_warning ("HyScanRawData: '%s.%s.%s': can't read parameters",
                 priv->project_name, priv->track_name, channel_name);
      goto exit;
    }

  hyscan_db_close (priv->db, param_id);
  param_id = -1;

  /* Образы сигналов для свёртки. */
  signal_name = g_strdup_printf ("%s-signal", channel_name);
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

      status = hyscan_core_params_check_signal_info (priv->db, param_id, priv->info.data_rate);
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

      status = hyscan_core_params_check_tvg_info (priv->db, param_id, priv->info.data_rate);
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

  /* Типы данных в буферах. */
  hyscan_buffer_set_data_type (priv->channel_buffer, priv->info.data_type);
  hyscan_buffer_set_data_type (priv->amp_buffer, HYSCAN_DATA_FLOAT);

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

  if (priv->channel_id > 0)
    hyscan_db_close (priv->db, priv->channel_id);
  if (priv->signal_id > 0)
    hyscan_db_close (priv->db, priv->signal_id);
  if (priv->tvg_id > 0)
    hyscan_db_close (priv->db, priv->tvg_id);

  if (priv->signals != NULL)
    {
      guint i;

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

  g_object_unref (priv->cache_buffer);
  g_object_unref (priv->channel_buffer);
  g_object_unref (priv->cplx_buffer);
  g_object_unref (priv->amp_buffer);

  g_clear_object (&priv->db);
  g_clear_object (&priv->cache);

  G_OBJECT_CLASS (hyscan_raw_data_parent_class)->finalize (object);
}

/* Функция создаёт ключ кэширования данных. */
static void
hyscan_raw_data_update_cache_key (HyScanRawDataPrivate *priv,
                                  gint                  data_type,
                                  guint32               index)
{
  if (priv->cache_key == NULL)
    {
      priv->cache_key = g_strdup_printf ("%s.%s.%s.%s.RAW.%d.%d.%2s.%d.%u",
                                         priv->cache_prefix,
                                         priv->db_uri,
                                         priv->project_name,
                                         priv->track_name,
                                         priv->source_type,
                                         priv->source_channel,
                                         "XX",
                                         G_MININT32,
                                         G_MAXUINT32);

      priv->cache_key_length = strlen (priv->cache_key);
    }

  g_snprintf (priv->cache_key, priv->cache_key_length, "%s.%s.%s.%s.RAW.%d.%d.%2s.%d.%u",
              priv->cache_prefix,
              priv->db_uri,
              priv->project_name,
              priv->track_name,
              priv->source_type,
              priv->source_channel,
              (data_type == DATA_TYPE_TVG) ? "TV" : priv->convolve ? "CV" : "NC",
              data_type,
              index);
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
      HyScanRawDataSignal signal_info = {0};
      HyScanDBFindStatus find_status;
      guint32 lindex, rindex;
      gint64 ltime, rtime;
      gpointer image;
      guint32 size;
      gint64 time;

      /* Считываем образ сигнала и проверяем его размер. */
      status = hyscan_db_channel_get_data (priv->db, priv->signal_id, i,
                                           priv->channel_buffer, &time);
      if (!status)
        return;

      image = hyscan_buffer_get_data (priv->channel_buffer, &size);
      if ((image == NULL) || (size == 0) || ((size % sizeof (HyScanComplexFloat)) != 0))
        return;

      /* Ищем индекс данных с которого начинает действовать сигнал. */
      find_status = hyscan_db_channel_find_data (priv->db, priv->channel_id, time,
                                                 &lindex, &rindex, &ltime, &rtime);
      if (find_status == HYSCAN_DB_FIND_OK)
        signal_info.index = rindex;
      else if (find_status == HYSCAN_DB_FIND_LESS)
        signal_info.index = first_signal_index;
      else
        return;

      /* Объект свёртки. */
      if (size > sizeof (HyScanComplexFloat))
        {
          signal_info.time = time;
          signal_info.image = g_memdup (image, size);
          signal_info.n_points = size / sizeof (HyScanComplexFloat);
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
                             guint32               index)
{
  gint i;

  if (priv->signals == NULL)
    return NULL;

  /* Ищем сигнал для момента времени time. */
  for (i = priv->signals->len - 1; i >= 0; i--)
    {
      HyScanRawDataSignal *signal = &g_array_index (priv->signals, HyScanRawDataSignal, i);
      if(index >= signal->index)
        return signal;
    }

  return NULL;
}

/* Функция считывает сырые данные для указанного индекса,
 * преобразовывает их и при необходимости осуществляет свёртку. */
static gboolean
hyscan_raw_data_read_raw_data (HyScanRawDataPrivate *priv,
                               guint32               index)
{
  gpointer data;
  guint32 size;

  /* Загружаем образы сигналов. */
  hyscan_raw_data_load_signals (priv);

  /* Считываем данные канала. */
  if (!hyscan_db_channel_get_data (priv->db, priv->channel_id, index, priv->channel_buffer, &priv->data_time))
    return FALSE;

  data = hyscan_buffer_get_data (priv->channel_buffer, &size);
  if ((data == NULL) || (size == 0))
    return FALSE;

  /* Размер считанных данных должен быть кратен размеру точки. */
  if ((size % hyscan_data_get_point_size (priv->info.data_type)) != 0)
    return FALSE;

  /* Импортируем данные в буфер обработки. */
  if (!hyscan_buffer_import_data (priv->cplx_buffer, priv->channel_buffer))
    return FALSE;

  /* Выполняем свёртку. */
  if (priv->convolve)
    {
      HyScanRawDataSignal *signal;
      HyScanComplexFloat *cplx;
      guint32 n_points;

      signal = hyscan_raw_data_find_signal (priv, index);
      cplx = hyscan_buffer_get_complex_float (priv->cplx_buffer, &n_points);
      if ((signal != NULL) && (cplx != NULL) && (signal->convolution != NULL))
        if (!hyscan_convolution_convolve (signal->convolution, cplx, n_points))
          return FALSE;
    }

  return TRUE;
}

/* Функция расчитывает амплитуду сигнала из комплексных данных. */
static gboolean
hyscan_raw_data_calc_amplitude (HyScanRawDataPrivate *priv)
{
  HyScanComplexFloat *cplx;
  gfloat *amplitude;
  guint32 n_points;
  guint32 i;

  /* Сырые данные. */
  cplx = hyscan_buffer_get_complex_float (priv->cplx_buffer, &n_points);
  if ((cplx == NULL) || (n_points == 0))
    return FALSE;

  /* Возвращаем амплитуду. */
  hyscan_buffer_set_size (priv->amp_buffer, n_points * sizeof (gfloat));
  amplitude = hyscan_buffer_get_float (priv->amp_buffer, &n_points);
  for (i = 0; i < n_points; i++)
    {
      gfloat re = cplx[i].re;
      gfloat im = cplx[i].im;
      amplitude[i] = sqrtf (re * re + im * im);
    }

  return TRUE;
}

/* Функция проверяет кэш на наличие данных указанного типа и
 * если такие есть, считывает их. */
static gboolean
hyscan_raw_data_check_cache (HyScanRawDataPrivate *priv,
                             gint                  data_type,
                             guint32               index,
                             gint64               *time)
{
  HyScanRawDataCacheHeader header;
  HyScanBuffer *data_buffer;
  guint32 cached_n_points;

  if (priv->cache == NULL)
    return FALSE;

  /* Ключ кэширования. */
  hyscan_raw_data_update_cache_key (priv, data_type, index);

  /* Буфер для чтения данных. */
  hyscan_buffer_wrap_data (priv->cache_buffer, HYSCAN_DATA_BLOB, &header, sizeof (header));
  if (data_type == DATA_TYPE_QUADRATURE)
    data_buffer = priv->cplx_buffer;
  else
    data_buffer = priv->amp_buffer;

  /* Ищем данные в кэше. */
  if (!hyscan_cache_get2 (priv->cache, priv->cache_key, NULL, sizeof (header), priv->cache_buffer, data_buffer))
    return FALSE;

  /* Верификация данных. */
  cached_n_points  = hyscan_buffer_get_size (data_buffer);
  cached_n_points /= hyscan_data_get_point_size (hyscan_buffer_get_data_type (data_buffer));
  if ((header.magic != CACHE_HEADER_MAGIC) ||
      (header.n_points != cached_n_points))
    {
      return FALSE;
    }

  (time != NULL) ? *time = header.time : 0;

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

  if (prefix == NULL)
    data->priv->cache_prefix = g_strdup ("none");
  else
    data->priv->cache_prefix = g_strdup (prefix);

  data->priv->cache = g_object_ref (cache);
}

/* Функция возвращает информацию о местоположении приёмной антенны гидролокатора. */
HyScanAntennaPosition
hyscan_raw_data_get_position (HyScanRawData *data)
{
  HyScanAntennaPosition zero;

  memset (&zero, 0, sizeof (HyScanAntennaPosition));

  g_return_val_if_fail (HYSCAN_IS_RAW_DATA (data), zero);

  if (data->priv->channel_id <= 0)
    return zero;

  return data->priv->position;
}

/* Функция возвращает параметры канала данных. */
HyScanRawDataInfo
hyscan_raw_data_get_info (HyScanRawData *data)
{
  HyScanRawDataInfo zero;

  memset (&zero, 0, sizeof (HyScanRawDataInfo));

  g_return_val_if_fail (HYSCAN_IS_RAW_DATA (data), zero);

  if (data->priv->channel_id <= 0)
    return zero;

  return data->priv->info;
}

/* Функция возвращает тип источника данных. */
HyScanSourceType
hyscan_raw_data_get_source (HyScanRawData *data)
{
  g_return_val_if_fail (HYSCAN_IS_RAW_DATA (data), HYSCAN_SOURCE_INVALID);

  if (data->priv->channel_id <= 0)
    return HYSCAN_SOURCE_INVALID;

  return data->priv->source_type;
}

/* Функция возвращает номер канала для этого канала данных. */
guint
hyscan_raw_data_get_channel (HyScanRawData *data)
{
  g_return_val_if_fail (HYSCAN_IS_RAW_DATA (data), 0);

  if (data->priv->channel_id <= 0)
    return 0;

  return data->priv->source_channel;
}

/* Функция определяет возможность изменения данных. */
gboolean
hyscan_raw_data_is_writable (HyScanRawData *data)
{
  g_return_val_if_fail (HYSCAN_IS_RAW_DATA (data), FALSE);

  if (data->priv->channel_id <= 0)
    return FALSE;

  return hyscan_db_channel_is_writable (data->priv->db, data->priv->channel_id);
}

/* Функция возвращает диапазон значений индексов записанных данных. */
gboolean
hyscan_raw_data_get_range (HyScanRawData *data,
                           guint32       *first_index,
                           guint32       *last_index)
{
  g_return_val_if_fail (HYSCAN_IS_RAW_DATA (data), FALSE);

  if (data->priv->channel_id <= 0)
    return FALSE;

  return hyscan_db_channel_get_data_range (data->priv->db, data->priv->channel_id,
                                           first_index, last_index);
}

/* Функция возвращает номер изменения в данных. */
guint32
hyscan_raw_data_get_mod_count (HyScanRawData *data)
{
  g_return_val_if_fail (HYSCAN_IS_RAW_DATA (data), 0);

  if (data->priv->channel_id <= 0)
    return 0;

  return hyscan_db_get_mod_count (data->priv->db, data->priv->channel_id);
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

  if (data->priv->channel_id <= 0)
    return HYSCAN_DB_FIND_FAIL;

  return hyscan_db_channel_find_data (data->priv->db, data->priv->channel_id,
                                      time, lindex, rindex, ltime, rtime);
}

/* Функция устанавливает необходимость выполнения свёртки данных. */
void
hyscan_raw_data_set_convolve (HyScanRawData *data,
                              gboolean       convolve)
{
  g_return_if_fail (HYSCAN_IS_RAW_DATA (data));

  data->priv->convolve = convolve;
}

/* Функция возвращает образ сигнала. */
const HyScanComplexFloat*
hyscan_raw_data_get_signal_image (HyScanRawData      *data,
                                  guint32             index,
                                  guint32            *n_points,
                                  gint64             *time)
{
  HyScanRawDataSignal *signal;

  g_return_val_if_fail (HYSCAN_IS_RAW_DATA (data), NULL);

  hyscan_raw_data_load_signals (data->priv);
  signal = hyscan_raw_data_find_signal (data->priv, index);
  if ((signal == NULL) || (signal->image == NULL))
    return NULL;

  (time != NULL) ? *time = signal->time : 0;
  *n_points = signal->n_points;

  return signal->image;
}

/* Функция возвращает значения коэффициентов усиления. */
const gfloat *
hyscan_raw_data_get_tvg_values (HyScanRawData *data,
                                guint32        index,
                                guint32       *n_points,
                                gint64        *time)
{
  HyScanRawDataPrivate *priv;

  HyScanDBFindStatus find_status;
  guint32 last_index;
  guint32 left_index;

  guint32 tvg_index;
  gint64 tvg_time;

  g_return_val_if_fail (HYSCAN_IS_RAW_DATA (data), NULL);

  priv = data->priv;

  if ((priv->channel_id <= 0) || (priv->tvg_id <= 0))
    return NULL;

  /* Проверяем наличие данных в кэше. */
  if (hyscan_raw_data_check_cache (priv, DATA_TYPE_TVG, index, time))
    return hyscan_buffer_get_float (priv->amp_buffer, n_points);

  /* Ищем индекс записи с нужными коэффициентами ВАРУ. */
  tvg_time = hyscan_db_channel_get_data_time (priv->db, priv->channel_id, index);
  if (tvg_time < 0)
    return NULL;

  /* Последний индекс в канале ВАРУ. */
  if (!hyscan_db_channel_get_data_range (priv->db, priv->tvg_id, NULL, &last_index))
    return NULL;

  find_status = hyscan_db_channel_find_data (priv->db, priv->tvg_id, tvg_time,
                                             &left_index, NULL, NULL, NULL);

  /* Нашли точный индекс данных или попали между двумя изменениями ВАРУ.
   * Нам нужен меньший индекс. */
  if (find_status == HYSCAN_DB_FIND_OK)
    tvg_index = left_index;

  /* Данные записаны после крайнего изменения коэффициентов ВАРУ.
   * Используем последний индекс. */
  else if (find_status == HYSCAN_DB_FIND_GREATER)
    tvg_index = last_index;

  /* Ошибка поиска или данные записаны раньше чем существующие коэффициенты ВАРУ. */
  else
    return NULL;

  /* Считываем коэффициенты ВАРУ. */
  if (!hyscan_db_channel_get_data (priv->db, priv->tvg_id, tvg_index, priv->amp_buffer, &tvg_time))
    return NULL;

  /* Сохраняем данные в кэше. */
  if (priv->cache != NULL)
    {
      HyScanRawDataCacheHeader header;

      header.magic = CACHE_HEADER_MAGIC;
      header.n_points = hyscan_buffer_get_size (priv->amp_buffer);
      header.n_points /= sizeof (gfloat);
      header.time = tvg_time;
      hyscan_buffer_wrap_data (priv->cache_buffer, HYSCAN_DATA_BLOB, &header, sizeof (header));

      hyscan_cache_set2 (priv->cache, priv->cache_key, NULL, priv->cache_buffer, priv->amp_buffer);
    }

  (time != NULL) ? *time = tvg_time : 0;

  return hyscan_buffer_get_float (priv->amp_buffer, n_points);
}

/* Функция возвращает значения амплитуды сигнала. */
const gfloat *
hyscan_raw_data_get_amplitude_values (HyScanRawData *data,
                                      guint32        index,
                                      guint32       *n_points,
                                      gint64        *time)
{
  HyScanRawDataPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_RAW_DATA (data), NULL);

  priv = data->priv;

  if (priv->channel_id <= 0)
    return NULL;

  /* Проверяем наличие амплитудных данных в кэше. */
  if (hyscan_raw_data_check_cache (priv, DATA_TYPE_AMPLITUDE, index, time))
    return hyscan_buffer_get_float (priv->amp_buffer, n_points);

  /* Проверяем наличие квадратурных данных в кэше или считываем их из базы. */
  if (!hyscan_raw_data_check_cache (priv, DATA_TYPE_QUADRATURE, index, time))
    if (!hyscan_raw_data_read_raw_data (priv, index))
      return NULL;

  /* Расчёт амлитуды. */
  if (!hyscan_raw_data_calc_amplitude (priv))
    return NULL;

  /* Сохраняем данные в кэше. */
  if (priv->cache != NULL)
    {
      HyScanRawDataCacheHeader header;

      header.magic = CACHE_HEADER_MAGIC;
      header.n_points = hyscan_buffer_get_size (priv->amp_buffer);
      header.n_points /= sizeof (gfloat);
      header.time = priv->data_time;
      hyscan_buffer_wrap_data (priv->cache_buffer, HYSCAN_DATA_BLOB, &header, sizeof (header));

      hyscan_raw_data_update_cache_key (priv, DATA_TYPE_AMPLITUDE, index);
      hyscan_cache_set2 (priv->cache, priv->cache_key, NULL, priv->cache_buffer, priv->amp_buffer);
    }

  (time != NULL) ? *time = priv->data_time : 0;

  return hyscan_buffer_get_float (priv->amp_buffer, n_points);
}

/* Функция возвращает квадратурные отсчёты сигнала. */
const HyScanComplexFloat *
hyscan_raw_data_get_quadrature_values (HyScanRawData      *data,
                                       guint32             index,
                                       guint32            *n_points,
                                       gint64             *time)
{
  HyScanRawDataPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_RAW_DATA (data), NULL);

  priv = data->priv;

  if (priv->channel_id <= 0)
    return NULL;

  /* Проверяем наличие данных в кэше. */
  if (hyscan_raw_data_check_cache (priv, DATA_TYPE_QUADRATURE, index, time))
    return hyscan_buffer_get_complex_float (priv->cplx_buffer, n_points);

  /* Считываем данные канала. */
  if (!hyscan_raw_data_read_raw_data (priv, index))
    return NULL;

  /* Сохраняем данные в кэше. */
  if (priv->cache != NULL)
    {
      HyScanRawDataCacheHeader header;

      header.magic = CACHE_HEADER_MAGIC;
      header.n_points = hyscan_buffer_get_size (priv->cplx_buffer);
      header.n_points /= sizeof (HyScanComplexFloat);
      header.time = priv->data_time;
      hyscan_buffer_wrap_data (priv->cache_buffer, HYSCAN_DATA_BLOB, &header, sizeof (header));

      hyscan_cache_set2 (priv->cache, priv->cache_key, NULL, priv->cache_buffer, priv->cplx_buffer);
    }

  (time != NULL) ? *time = priv->data_time : 0;

  return hyscan_buffer_get_complex_float (priv->cplx_buffer, n_points);
}
