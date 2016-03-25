/**
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
#define DATA_CHANNEL_API       20150700

/* Имена параметров канала данных. */
#define PARAM_VERSION                   "channel.version"
#define PARAM_DISCRETIZATION_TYPE       "discretization.type"
#define PARAM_DISCRETIZATION_FREQUENCY  "discretization.frequency"

#define SIGNALS_CHANNEL_POSTFIX         "signals"

enum
{
  PROP_O,
  PROP_DB,
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
  gint64               time;                           /* Момент времени начала действия сигнала. */
  HyScanConvolution   *convolution;                    /* Образец сигнала для свёртки. */
} HyScanDataChannelSignal;

/* Внутренние данные объекта. */
struct _HyScanDataChannel
{
  GObject              parent_instance;

  HyScanDB            *db;                             /* Интерфейс базы данных. */
  gchar               *uri;                            /* URI базы данных. */

  HyScanCache         *cache;                          /* Интерфейс системы кэширования. */
  gchar               *cache_prefix;                   /* Префикс ключа кэширования. */
  gchar               *cache_key;                      /* Ключ кэширования. */
  gint                 cache_key_length;               /* Максимальная длина ключа. */

  gchar               *project_name;                   /* Название проекта. */
  gchar               *track_name;                     /* Название галса. */
  gchar               *channel_name;                   /* Название канала данных. */
  gint32               channel_id;                     /* Идентификатор открытого канала данных. */

  HyScanDataType       discretization_type;            /* Тип дискретизации данных. */
  gfloat               discretization_frequency;       /* Частота дискретизации данных. */

  gpointer             raw_buffer;                     /* Буфер для чтения "сырых" данных канала. */
  gint32               raw_buffer_size;                /* Размер буфера в байтах. */

  HyScanComplexFloat  *data_buffer;                    /* Буфер для обработки данных. */
  gint64               data_time;                      /* Метка времени обрабатываемых данных. */
  gint32               next_data_index;                /* Предполагаемый индекс следующей записи. */

  gint32               signal_id;                      /* Идентификатор открытого канала с образцами сигналов. */
  GArray              *signals;                        /* Массив объектов свёртки. */
  gint32               last_signal_index;              /* Индекс последнего загруженного сигнала. */
  gboolean             convolve;                       /* Выполнять или нет свёртку. */

  GMutex               lock;                           /* Блокировка многопоточного доступа. */

  gboolean             readonly;                       /* Режим доступа: только чтение или чтение/запись. */
};

static void     hyscan_data_channel_set_property          (GObject               *object,
                                                           guint                  prop_id,
                                                           const GValue          *value,
                                                           GParamSpec            *pspec);
static void     hyscan_data_channel_object_finalize       (GObject               *object);

static void     hyscan_data_channel_update_cache_key      (HyScanDataChannel     *dchannel,
                                                           gint                   data_type,
                                                           gint32                 index);
static void     hyscan_data_channel_buffer_realloc        (HyScanDataChannel     *dchannel,
                                                           gint32                 size);
static gint32   hyscan_data_channel_read_raw_data         (HyScanDataChannel     *dchannel,
                                                           gint32                 channel_id,
                                                           gint32                 index,
                                                           gint64                *time);

static void     hyscan_data_channel_load_signals          (HyScanDataChannel *dchannel);
static HyScanConvolution *hyscan_data_channel_find_signal (HyScanDataChannel *dchannel,
                                                           gint64             time);

static gint32   hyscan_data_channel_read_data             (HyScanDataChannel     *dchannel,
                                                           gint32                 index );
static gboolean hyscan_data_channel_check_cache           (HyScanDataChannel     *dchannel,
                                                           gint32                 index,
                                                           gint                   data_type,
                                                           gpointer               buffer,
                                                           gint32                *buffer_size,
                                                           gint64                *time );

static gboolean hyscan_data_channel_create_int            (HyScanDataChannel     *dchannel,
                                                           const gchar           *project_name,
                                                           const gchar           *track_name,
                                                           const gchar           *channel_name,
                                                           HyScanDataType         discretization_type,
                                                           gfloat                 discretization_frequency);
static gboolean hyscan_data_channel_open_int              (HyScanDataChannel     *dchannel,
                                                           const gchar           *project_name,
                                                           const gchar           *track_name,
                                                           const gchar           *channel_name);
static void     hyscan_data_channel_close_int             (HyScanDataChannel     *dchannel);

G_DEFINE_TYPE( HyScanDataChannel, hyscan_data_channel, G_TYPE_OBJECT );

static void
hyscan_data_channel_class_init (HyScanDataChannelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_data_channel_set_property;

  object_class->finalize = hyscan_data_channel_object_finalize;

  g_object_class_install_property (object_class, PROP_DB,
    g_param_spec_object ("db", "DB", "HyScanDB interface", HYSCAN_TYPE_DB,
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
  dchannel->channel_id = -1;
  dchannel->signal_id = -1;
  dchannel->last_signal_index = -1;
  g_mutex_init( &dchannel->lock );
}

static void
hyscan_data_channel_set_property (GObject *object,
                                  guint prop_id,
                                  const GValue *value,
                                  GParamSpec *pspec )
{
  HyScanDataChannel *dchannel = HYSCAN_DATA_CHANNEL (object);

  switch (prop_id)
    {
    case PROP_DB:
      dchannel->db = g_value_get_object (value);
      if (dchannel->db != NULL )
        g_object_ref (dchannel->db);
      else
        G_OBJECT_WARN_INVALID_PROPERTY_ID (dchannel, prop_id, pspec);
      dchannel->uri = hyscan_db_get_uri (dchannel->db);
      break;

    case PROP_CACHE:
      dchannel->cache = g_value_get_object (value);
      if (dchannel->cache != NULL)
        g_object_ref (dchannel->cache);
      break;

    case PROP_CACHE_PREFIX:
      dchannel->cache_prefix = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (dchannel, prop_id, pspec);
      break;
    }
}

static void
hyscan_data_channel_object_finalize (GObject *object)
{
  HyScanDataChannel *dchannel = HYSCAN_DATA_CHANNEL (object);

  hyscan_data_channel_close (dchannel);

  g_free (dchannel->uri);
  g_free (dchannel->raw_buffer);
  g_free (dchannel->data_buffer);

  g_clear_object (&dchannel->db);
  g_clear_object (&dchannel->cache);

  g_mutex_clear (&dchannel->lock);

  G_OBJECT_CLASS (hyscan_data_channel_parent_class)->finalize (object);
}

/* Функция создаёт ключ кэширования данных. */
static void
hyscan_data_channel_update_cache_key (HyScanDataChannel *dchannel,
                                      gint               data_type,
                                      gint32             index)
{
  gchar *data_type_str;

  if (data_type == DATA_TYPE_AMPLITUDE)
    data_type_str = "A";
  else
    data_type_str = "Q";

  if (dchannel->cache != NULL && dchannel->cache_key == NULL)
    {
      if (dchannel->cache_prefix != NULL)
        {
        dchannel->cache_key = g_strdup_printf ("%s.%s.%s.%s.%s.CV.A.0123456789",
                                               dchannel->uri,
                                               dchannel->cache_prefix,
                                               dchannel->project_name,
                                               dchannel->track_name,
                                               dchannel->channel_name);
        }
      else
        {
        dchannel->cache_key = g_strdup_printf ("%s.%s.%s.%s.CV.A.0123456789",
                                               dchannel->uri,
                                               dchannel->project_name,
                                               dchannel->track_name,
                                               dchannel->channel_name);
        }
      dchannel->cache_key_length = strlen (dchannel->cache_key);
    }

  if (dchannel->cache_prefix)
    {
      g_snprintf (dchannel->cache_key, dchannel->cache_key_length, "%s.%s.%s.%s.%s.%s.%s.%d",
                  dchannel->uri,
                  dchannel->cache_prefix,
                  dchannel->project_name,
                  dchannel->track_name,
                  dchannel->channel_name,
                  dchannel->convolve ? "CV" : "NC",
                  data_type_str,
                  index);
    }
  else
    {
      g_snprintf (dchannel->cache_key, dchannel->cache_key_length, "%s.%s.%s.%s.%s.%s.%d",
                  dchannel->uri,
                  dchannel->project_name,
                  dchannel->track_name,
                  dchannel->channel_name,
                  dchannel->convolve ? "CV" : "NC",
                  data_type_str,
                  index);
    }
}

/* Функция проверяет размер буферов и увеличивает его при необходимости. */
static void
hyscan_data_channel_buffer_realloc (HyScanDataChannel *dchannel,
                                    gint32             size)
{
  gsize data_buffer_size;

  if (dchannel->raw_buffer_size > size)
    return;

  dchannel->raw_buffer_size = size + 32;
  data_buffer_size = dchannel->raw_buffer_size / hyscan_get_data_point_size (dchannel->discretization_type);
  data_buffer_size *= sizeof (HyScanComplexFloat);

  dchannel->raw_buffer = g_realloc (dchannel->raw_buffer, dchannel->raw_buffer_size);
  dchannel->data_buffer = g_realloc (dchannel->data_buffer, data_buffer_size);
}

/* Функция считывает запись из канала данных в буфер. */
static gint32
hyscan_data_channel_read_raw_data (HyScanDataChannel *dchannel,
                                   gint32             channel_id,
                                   gint32             index,
                                   gint64            *time)
{
  gint32 io_size;
  gboolean status;

  /* Считываем данные канала. */
  io_size = dchannel->raw_buffer_size;
  status = hyscan_db_get_channel_data (dchannel->db, channel_id, index,
                                       dchannel->raw_buffer, &io_size, time);
  if (!status)
    return -1;

  /* Если размер считанных данных равен размеру буфера, запрашиваем реальный размер,
     увеличиваем размер буфера и считываем данные еще раз. */
  if (dchannel->raw_buffer_size == 0 || dchannel->raw_buffer_size == io_size)
    {
      status = hyscan_db_get_channel_data (dchannel->db, channel_id, index,
                                           NULL, &io_size, NULL);
      if (!status)
        return -1;

      hyscan_data_channel_buffer_realloc (dchannel, io_size);
      io_size = dchannel->raw_buffer_size;

      status = hyscan_db_get_channel_data (dchannel->db, channel_id, index,
                                           dchannel->raw_buffer, &io_size, time);
      if (!status)
        return -1;
    }

  return io_size;
}

/* Функция загружает образцы сигналов для свёртки. */
static void
hyscan_data_channel_load_signals (HyScanDataChannel *dchannel)
{
  gint32 first_signal_index;
  gint32 last_signal_index;

  gboolean status;
  gint i;

  /* Если нет канала с образцами сигналов, выходим. */
  if (dchannel->signal_id < 0)
    return;

  /* Массив образцов сигналов. */
  if (dchannel->signals == NULL)
    dchannel->signals = g_array_new (TRUE, TRUE, sizeof (HyScanDataChannelSignal));

  /* Проверяем индекс последнего загруженного образца сигнала. */
  status = hyscan_db_get_channel_data_range (dchannel->db, dchannel->signal_id,
                                             &first_signal_index, &last_signal_index);
  if (!status)
    return;
  if (dchannel->last_signal_index == last_signal_index)
    return;

  /* Загружаем "новые" образцы сигналов. */
  i = dchannel->last_signal_index + 1;
  if (i < first_signal_index)
    i = first_signal_index;

  for (; i <= last_signal_index; i++)
    {
      gint32 io_size;
      HyScanComplexFloat *signal;
      HyScanDataChannelSignal signal_info;

      /* Считываем образец сигнала и проверяем его размер. */
      io_size = hyscan_data_channel_read_raw_data (dchannel, dchannel->signal_id, i, &signal_info.time);
      if (io_size < 0 || (io_size % sizeof(HyScanComplexFloat)) != 0)
        return;

      signal = dchannel->raw_buffer;

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
                                        dchannel->raw_buffer,
                                        io_size / sizeof (HyScanComplexFloat));
        }

      g_array_append_val (dchannel->signals, signal_info);
      dchannel->last_signal_index = i;
    }
}

/* Функция ищет образец сигнала для свёртки для указанного момента времени. */
static HyScanConvolution *
hyscan_data_channel_find_signal (HyScanDataChannel *dchannel,
                                 gint64             time)
{
  gint i;

  if (dchannel->signals == NULL)
    return NULL;

  /* Ищем сигнал для момента времени time. */
  for (i = dchannel->signals->len - 1; i >= 0; i--)
    {
      HyScanDataChannelSignal *signal = &g_array_index (dchannel->signals, HyScanDataChannelSignal, i);
      if(time >= signal->time)
        return signal->convolution;
    }

  return NULL;
}

/* Функция считывает "сырые" акустические данные для указанного индекса,
   импортирует их в буфер данных и при необходимости осуществляет свёртку. */
static gint32
hyscan_data_channel_read_data (HyScanDataChannel *dchannel,
                               gint32             index)
{
  gint32 io_size;
  gint32 n_points;
  gint32 point_size;

  HyScanConvolution *convolution;

  /* Проверка состояния объекта. */
  if (dchannel->channel_id < 0)
    return -1;

  /* Загружаем образцы сигналов. */
  hyscan_data_channel_load_signals (dchannel);

  /* Считываем данные канала. */
  io_size = hyscan_data_channel_read_raw_data (dchannel, dchannel->channel_id, index, &dchannel->data_time);
  if (io_size < 0)
    return -1;

  /* Размер считанных данных должен быть кратен размеру точки. */
  point_size = hyscan_get_data_point_size (dchannel->discretization_type);
  if (io_size % point_size != 0)
    return -1;
  n_points = io_size / point_size;

  /* Импортируем данные в буфер обработки. */
  hyscan_data_import_complex_float (dchannel->discretization_type, dchannel->raw_buffer, io_size,
                                    dchannel->data_buffer, &n_points);

  /* Выполняем свёртку. */
  if (dchannel->convolve )
    {
      convolution = hyscan_data_channel_find_signal (dchannel, dchannel->data_time);
      if (convolution != NULL)
        {
        if (!hyscan_convolution_convolve (convolution, dchannel->data_buffer, n_points))
          return -1;
        }
    }

  return n_points;
}

/* Функция проверяет кэш на наличие данных указанного типа и если такие есть считывает их. */
static gboolean
hyscan_data_channel_check_cache (HyScanDataChannel *dchannel,
                                 gint               data_type,
                                 gint32             index,
                                 gpointer           buffer,
                                 gint32            *buffer_size,
                                 gint64            *time )
{
  gint64 cached_time;
  gint32 time_size;
  gint32 io_size;

  gint data_type_size;

  gboolean status;

  if (dchannel->cache == NULL || buffer == NULL)
    return FALSE;

  if (data_type == DATA_TYPE_AMPLITUDE)
    data_type_size = sizeof (gfloat);
  else
    data_type_size = sizeof (HyScanComplexFloat);

  /* Ключ кэширования. */
  hyscan_data_channel_update_cache_key (dchannel, data_type, index);

  /* Ищем данные в кэше. */
  time_size = sizeof(cached_time);
  io_size = *buffer_size * sizeof(gfloat);
  status = hyscan_cache_get2 (dchannel->cache, dchannel->cache_key, NULL,
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

/* Функция создаёт новый канал хранения акустических данных и открывает его для обработки. */
static gboolean
hyscan_data_channel_create_int (HyScanDataChannel *dchannel,
                                const gchar       *project_name,
                                const gchar       *track_name,
                                const gchar       *channel_name,
                                HyScanDataType     discretization_type,
                                gfloat             discretization_frequency)
{
  gboolean status = FALSE;

  gchar *signal_name;

  gint32 project_id = -1;
  gint32 track_id = -1;
  gint32 param_id = -1;

  /* Проверяем, что передан указатель на базу данных. */
  if (dchannel->db == NULL)
    goto exit;

  /* Проверяем, что заданы имя проекта, галса и канала данных. */
  if (project_name == NULL || track_name == NULL|| channel_name == NULL)
    {
      if (project_name == NULL)
        g_critical ("hyscan_data_channel_create: unknown project name");
      if (track_name == NULL)
        g_critical ("hyscan_data_channel_create: unknown track name");
      if (channel_name == NULL)
        g_critical ("hyscan_data_channel_create: unknown channel name");
      goto exit;
    }

  /* Сохраняем имена проекта, галса и канала данных. */
  dchannel->project_name = g_strdup (project_name);
  dchannel->track_name = g_strdup (track_name);
  dchannel->channel_name = g_strdup (channel_name);

  /* Открываем проект. */
  project_id = hyscan_db_open_project (dchannel->db, dchannel->project_name);
  if (project_id < 0)
    {
      g_critical ("hyscan_data_channel_create: can't open project '%s'",
                  dchannel->project_name);
      goto exit;
    }

  /* Открываем галс. */
  track_id = hyscan_db_open_track (dchannel->db, project_id, dchannel->track_name);
  if (track_id < 0)
    {
      g_critical ("hyscan_data_channel_create: can't open track '%s.%s'",
                  dchannel->project_name, dchannel->track_name);
      goto exit;
    }

  /* Создаём канал данных. */
  dchannel->channel_id = hyscan_db_create_channel (dchannel->db, track_id, channel_name);
  if (dchannel->channel_id < 0)
    {
      g_critical ("hyscan_data_channel_create: can't create channel '%s.%s.%s'",
                  project_name, track_name, channel_name);
      goto exit;
    }

  // Параметры канала данных.
  param_id = hyscan_db_open_channel_param (dchannel->db, dchannel->channel_id);
  if (param_id < 0)
    {
      g_critical ("hyscan_data_channel_create: can't create channel '%s.%s.%s' parameters",
                  project_name, track_name, channel_name);
      goto exit;
    }

  status = hyscan_db_set_integer_param (dchannel->db, param_id,
                                        PARAM_VERSION,
                                        DATA_CHANNEL_API);
  if (!status)
    goto exit;

  status = hyscan_db_set_string_param  (dchannel->db, param_id,
                                        PARAM_DISCRETIZATION_TYPE,
                                        hyscan_get_data_type_name (discretization_type));
  if (!status)
    goto exit;

  status = hyscan_db_set_double_param  (dchannel->db, param_id,
                                        PARAM_DISCRETIZATION_FREQUENCY,
                                        discretization_frequency);
  if (!status)
    goto exit;

  hyscan_db_close_param (dchannel->db, param_id);
  param_id = -1;

  status = FALSE;

  /* Создаём канал с образцами сигналов. */
  signal_name = g_strdup_printf ("%s.%s", dchannel->channel_name, SIGNALS_CHANNEL_POSTFIX);
  dchannel->signal_id = hyscan_db_create_channel (dchannel->db, track_id, signal_name);
  g_free (signal_name);

  if (dchannel->signal_id < 0)
    {
      g_critical ("hyscan_data_channel_create: can't create channel '%s.%s.%s.%s'",
                  project_name, track_name, channel_name, SIGNALS_CHANNEL_POSTFIX);
      goto exit;
    }

  // Параметры канала данных.
  param_id = hyscan_db_open_channel_param (dchannel->db, dchannel->signal_id);
  if (param_id < 0)
    {
      g_critical ("hyscan_data_channel_create: can't create channel '%s.%s.%s.%s' parameters",
                  project_name, track_name, channel_name, SIGNALS_CHANNEL_POSTFIX);
      goto exit;
    }

  status = hyscan_db_set_integer_param (dchannel->db, param_id,
                                        PARAM_VERSION, DATA_CHANNEL_API);
  if (!status)
    goto exit;

  status = hyscan_db_set_string_param  (dchannel->db, param_id,
                                        PARAM_DISCRETIZATION_TYPE,
                                        hyscan_get_data_type_name (HYSCAN_DATA_TYPE_COMPLEX_FLOAT));
  if (!status)
    goto exit;

  status = hyscan_db_set_double_param  (dchannel->db, param_id,
                                        PARAM_DISCRETIZATION_FREQUENCY,
                                        discretization_frequency);
  if (!status)
    goto exit;

  dchannel->discretization_type = discretization_type;
  dchannel->discretization_frequency = discretization_frequency;
  dchannel->next_data_index = 0;
  dchannel->convolve = TRUE;
  dchannel->readonly = FALSE;

  status = TRUE;

exit:
  if (!status)
    hyscan_data_channel_close_int (dchannel);
  if (project_id > 0)
    hyscan_db_close_project (dchannel->db, project_id);
  if (track_id > 0)
    hyscan_db_close_track (dchannel->db, track_id);
  if (param_id > 0)
    hyscan_db_close_param (dchannel->db, param_id);

  return status;
}


/* Функция открывает существующий канал хранения акустических данных для обработки. */
static gboolean
hyscan_data_channel_open_int (HyScanDataChannel *dchannel,
                              const gchar       *project_name,
                              const gchar       *track_name,
                              const gchar       *channel_name)
{
  gboolean status = FALSE;

  gchar *signal_name;
  gchar *type;

  gint32 project_id = -1;
  gint32 track_id = -1;
  gint32 param_id = -1;

  gint64 api_version;

  /* Проверяем, что передан указатель на базу данных. */
  if (dchannel->db == NULL)
    goto exit;

  /* Проверяем, что заданы имя проекта, галса и канала данных. */
  if (project_name == NULL || track_name == NULL|| channel_name == NULL)
    {
      if (project_name == NULL)
        g_critical ("hyscan_data_channel_open: unknown project name");
      if (track_name == NULL)
        g_critical ("hyscan_data_channel_open: unknown track name");
      if (channel_name == NULL)
        g_critical ("hyscan_data_channel_open: unknown channel name");
      goto exit;
    }

  /* Сохраняем имена проекта, галса и канала данных. */
  dchannel->project_name = g_strdup (project_name);
  dchannel->track_name = g_strdup (track_name);
  dchannel->channel_name = g_strdup (channel_name);

  /* Открываем проект. */
  project_id = hyscan_db_open_project (dchannel->db, dchannel->project_name);
  if (project_id < 0)
    {
      g_critical ("hyscan_data_channel_open: can't open project '%s'",
                  dchannel->project_name);
      goto exit;
    }

  /* Открываем галс. */
  track_id = hyscan_db_open_track (dchannel->db, project_id, dchannel->track_name);
  if (track_id < 0)
    {
      g_critical ("hyscan_data_channel_open: can't open track '%s.%s'",
                  dchannel->project_name, dchannel->track_name);
      goto exit;
    }

  /* Открываем канал данных. */
  dchannel->channel_id = hyscan_db_open_channel (dchannel->db, track_id, dchannel->channel_name);
  if (dchannel->channel_id < 0)
    {
      g_critical ("hyscan_data_channel_open: can't open channel '%s.%s.%s'",
                  dchannel->project_name, dchannel->track_name, dchannel->channel_name);
      goto exit;
    }

  /* Открываем группу параметров канала данных. */
  param_id = hyscan_db_open_channel_param (dchannel->db, dchannel->channel_id);
  if (param_id < 0)
    {
      g_critical ("hyscan_data_channel_open: can't open channel '%s.%s.%s' parameters",
                  dchannel->project_name, dchannel->track_name, dchannel->channel_name);
      goto exit;
    }

  /* Проверяем версию API канала данных. */
  api_version = hyscan_db_get_integer_param (dchannel->db, param_id, PARAM_VERSION);
  if ((api_version / 100) != (DATA_CHANNEL_API / 100))
    {
      g_critical ("hyscan_data_channel_open: '%s.%s.%s': unsupported api version (%" G_GINT64_FORMAT ")",
                  dchannel->project_name, dchannel->track_name, dchannel->channel_name,
                  api_version);
      goto exit;
    }

  /* Тип дискретизации данных в канале. */
  type = hyscan_db_get_string_param (dchannel->db, param_id, PARAM_DISCRETIZATION_TYPE);
  dchannel->discretization_type = hyscan_get_data_type_by_name (type);
  g_free (type);

  if (dchannel->discretization_type == HYSCAN_DATA_TYPE_INVALID)
    {
      g_critical ("hyscan_data_channel_open: '%s.%s.%s': unsupported discretization type",
                  dchannel->project_name, dchannel->track_name, dchannel->channel_name);
      goto exit;
    }

  /* Частота дискретизации данных. */
  dchannel->discretization_frequency =
      hyscan_db_get_double_param (dchannel->db, param_id, PARAM_DISCRETIZATION_FREQUENCY);

  if (dchannel->discretization_frequency < 1.0)
    {
      g_critical ("hyscan_data_channel_open: '%s.%s.%s': unsupported discretization frequency %.3fHz",
                  dchannel->project_name, dchannel->track_name, dchannel->channel_name,
                  dchannel->discretization_frequency);
      goto exit;
    }

  /* Открываем канал данных с образцами сигналов. */
  signal_name = g_strdup_printf ("%s.%s", dchannel->channel_name, SIGNALS_CHANNEL_POSTFIX);
  dchannel->signal_id = hyscan_db_open_channel (dchannel->db, track_id, signal_name);
  g_free (signal_name);

  if (dchannel->signal_id > 0)
    {
      HyScanDataType discretization_type;
      gfloat discretization_frequency;

      hyscan_db_close_param (dchannel->db, param_id);

      /* Открываем группу параметров канала с образцами сигналов. */
      param_id = hyscan_db_open_channel_param (dchannel->db, dchannel->signal_id);
      if (param_id < 0)
        {
          g_critical ("hyscan_data_channel_open: can't open channel '%s.%s.%s.%s' parameters",
                      dchannel->project_name, dchannel->track_name, dchannel->channel_name,
                      SIGNALS_CHANNEL_POSTFIX);
          goto exit;
        }

      /* Проверяем версию API канала с образцами сигналов. */
      api_version = hyscan_db_get_integer_param (dchannel->db, param_id, PARAM_VERSION);
      if ((api_version / 100) != (DATA_CHANNEL_API / 100))
        {
          g_critical ("hyscan_data_channel_open: '%s.%s.%s.%s': unsupported api version (%" G_GINT64_FORMAT ")",
                      dchannel->project_name, dchannel->track_name, dchannel->channel_name,
                      SIGNALS_CHANNEL_POSTFIX, api_version);
          goto exit;
        }

      /* Проверяем тип дискретизации данных в канале. */
      type = hyscan_db_get_string_param (dchannel->db, param_id, PARAM_DISCRETIZATION_TYPE);
      discretization_type = hyscan_get_data_type_by_name (type);
      g_free (type);

      if (discretization_type != HYSCAN_DATA_TYPE_COMPLEX_FLOAT)
        {
          g_critical ("hyscan_data_channel_open: '%s.%s.%s.%s': unsupported discretization type",
                      dchannel->project_name, dchannel->track_name, dchannel->channel_name,
                      SIGNALS_CHANNEL_POSTFIX);
          goto exit;
        }

      /* Частота дискретизации данных. */
      discretization_frequency =
          hyscan_db_get_double_param (dchannel->db, param_id, PARAM_DISCRETIZATION_FREQUENCY);

      if (discretization_frequency != dchannel->discretization_frequency)
        {
          g_critical ("hyscan_data_channel_open: '%s.%s.%s.%s': signal discretization frequency mismatch with data",
                      dchannel->project_name, dchannel->track_name, dchannel->channel_name,
                      SIGNALS_CHANNEL_POSTFIX);
          goto exit;
        }

      hyscan_data_channel_load_signals (dchannel);
      dchannel->convolve = TRUE;
    }

  dchannel->readonly = TRUE;

  status = TRUE;

exit:
  if (!status)
    hyscan_data_channel_close_int (dchannel);
  if (project_id > 0)
    hyscan_db_close_project (dchannel->db, project_id);
  if (track_id > 0)
    hyscan_db_close_track (dchannel->db, track_id);
  if (param_id > 0)
    hyscan_db_close_param (dchannel->db, param_id);

  return status;
}

/* Функция закрывает текущий обрабатываемый канал данных. */
static void
hyscan_data_channel_close_int (HyScanDataChannel *dchannel)
{
  guint i;

  /* Закрываем канал данных. */
  if (dchannel->channel_id > 0)
    hyscan_db_close_channel (dchannel->db, dchannel->channel_id);
  dchannel->channel_id = -1;

  /* Обнуляем имена проекта, галса и канала данных. */
  g_clear_pointer (&dchannel->project_name, g_free);
  g_clear_pointer (&dchannel->track_name, g_free);
  g_clear_pointer (&dchannel->channel_name, g_free);

  /* Обнуляем буфер ключа кэширования. */
  g_clear_pointer (&dchannel->cache_key, g_free);

  /* Удаляем образцы сигналов для свёртки. */
  if (dchannel->signals != NULL)
    {
      for (i = 0; i < dchannel->signals->len; i++)
        {
          HyScanDataChannelSignal *signal = &g_array_index (dchannel->signals,
                                                            HyScanDataChannelSignal, i);
          g_clear_object (&signal->convolution);
        }
      g_clear_pointer (&dchannel->signals, g_array_unref);
    }
  dchannel->last_signal_index = -1;

  /* Закрываем канал с образцами сигналов. */
  if (dchannel->signal_id > 0)
    hyscan_db_close_channel (dchannel->db, dchannel->signal_id);
  dchannel->signal_id = -1;
}

/* Функция создаёт новый объект первичной обработки акустических данных. */
HyScanDataChannel *
hyscan_data_channel_new (HyScanDB    *db,
                         HyScanCache *cache,
                         const gchar *cache_prefix)
{
  return g_object_new (HYSCAN_TYPE_DATA_CHANNEL,
                       "db", db,
                       "cache", cache,
                       "cache-prefix", cache_prefix,
                       NULL);
}

/* Функция создаёт новый канал хранения акустических данных и открывает его для обработки. */
gboolean
hyscan_data_channel_create (HyScanDataChannel     *dchannel,
                            const gchar           *project_name,
                            const gchar           *track_name,
                            const gchar           *channel_name,
                            HyScanDataType         discretization_type,
                            gfloat                 discretization_frequency)
{
  gboolean status;

  g_mutex_lock (&dchannel->lock);
  hyscan_data_channel_close_int (dchannel);
  status = hyscan_data_channel_create_int (dchannel, project_name, track_name, channel_name,
                                           discretization_type, discretization_frequency);
  g_mutex_unlock (&dchannel->lock);

  return status;
}

/* Функция открывает существующий канал хранения акустических данных для обработки. */
gboolean
hyscan_data_channel_open (HyScanDataChannel *dchannel,
                          const gchar       *project_name,
                          const gchar       *track_name,
                          const gchar       *channel_name)
{
  gboolean status;

  g_mutex_lock (&dchannel->lock);
  hyscan_data_channel_close_int (dchannel);
  status = hyscan_data_channel_open_int (dchannel, project_name, track_name, channel_name);
  g_mutex_unlock (&dchannel->lock);

  return status;
}

/* Функция закрывает текущий обрабатываемый канал данных. */
void
hyscan_data_channel_close (HyScanDataChannel *dchannel)
{
  g_mutex_lock (&dchannel->lock);
  hyscan_data_channel_close_int (dchannel);
  g_mutex_unlock (&dchannel->lock);
}

/* Функция возвращает тип дискретизации данных. */
HyScanDataType
hyscan_data_channel_get_discretization_type (HyScanDataChannel *dchannel)
{
  HyScanDataType discretization_type = HYSCAN_DATA_TYPE_INVALID;

  g_mutex_lock (&dchannel->lock);
  if (dchannel->channel_id > 0)
    discretization_type = dchannel->discretization_type;
   g_mutex_unlock (&dchannel->lock);

  return discretization_type;
}

/* Функция возвращает значение частоты дискретизации данных. */
gfloat
hyscan_data_channel_get_discretization_frequency (HyScanDataChannel *dchannel)
{
  gfloat discretization_frequency = 0.0;

  g_mutex_lock (&dchannel->lock);
  if (dchannel->channel_id > 0)
    discretization_frequency = dchannel->discretization_frequency;
  g_mutex_unlock (&dchannel->lock);

  return discretization_frequency;
}

/* Функция задаёт образец сигнала для свёртки. */
gboolean
hyscan_data_channel_add_signal_image (HyScanDataChannel  *dchannel,
                                      gint64              time,
                                      HyScanComplexFloat *signal,
                                      gint32              n_signal_points)
{
  gboolean status;

  gint32 signal_size;
  HyScanComplexFloat zero = {0.0, 0.0};

  g_mutex_lock (&dchannel->lock);

  /* Если канал не открыт или открыт только для чтения, ошибка. */
  if (dchannel->signal_id < 0 || dchannel->readonly)
    {
      g_mutex_unlock (&dchannel->lock);
      return FALSE;
    }

  /* Признак отключения свёртки с указанного момента времени. */
  if (signal == NULL || n_signal_points == 0)
    {
      signal = &zero;
      signal_size = sizeof (zero);
    }
  else
    {
      signal_size = n_signal_points * sizeof (HyScanComplexFloat);
    }

  /* Записываем образец сигнала. */
  status = hyscan_db_add_channel_data (dchannel->db, dchannel->signal_id, time,
                                       signal, signal_size, NULL);

  g_mutex_unlock (&dchannel->lock);

  return status;
}

/* Функция устанавливает необходимость выполнения свёртки акустических данных. */
void
hyscan_data_channel_set_convolve (HyScanDataChannel *dchannel,
                                  gboolean           convolve)
{
  g_mutex_lock (&dchannel->lock);
  dchannel->convolve = convolve;
  g_mutex_unlock (&dchannel->lock);
}

/* Функция возвращает диапазон значений индексов записанных данных. */
gboolean
hyscan_data_channel_get_range (HyScanDataChannel *dchannel,
                               gint32            *first_index,
                               gint32            *last_index )
{
  gboolean status = FALSE;

  g_mutex_lock (&dchannel->lock);
  if (dchannel->channel_id > 0)
    status = hyscan_db_get_channel_data_range (dchannel->db, dchannel->channel_id,
                                               first_index, last_index);
  g_mutex_unlock (&dchannel->lock);

  return status;
}

/* Функция возвращает число точек данных для указанного индекса. */
gint32
hyscan_data_channel_get_values_count (HyScanDataChannel *dchannel,
                                      gint32             index)
{
  gint32 dsize = -1;
  gboolean status;

  g_mutex_lock (&dchannel->lock);
  if (dchannel->channel_id > 0)
    {
      status = hyscan_db_get_channel_data (dchannel->db, dchannel->channel_id, index,
                                           NULL, &dsize, NULL);
      if (status)
        dsize /= hyscan_get_data_point_size (dchannel->discretization_type);
    }
  g_mutex_unlock (&dchannel->lock);

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
  gboolean status = FALSE;

  g_mutex_lock (&dchannel->lock);
  if (dchannel->channel_id > 0)
    status = hyscan_db_find_channel_data (dchannel->db, dchannel->channel_id, time,
                                          lindex, rindex,
                                          ltime, rtime);
  g_mutex_unlock (&dchannel->lock);

  return status;
}

/* Функция записывает новые данные в канал. */
gboolean
hyscan_data_channel_add_data (HyScanDataChannel *dchannel,
                              gint64             time,
                              gpointer           data,
                              gint32             size,
                              gint32            *index)
{
  gboolean status = FALSE;

  HyScanConvolution *convolution;

  gint32 data_index = -1;
  gint32 n_points;
  gfloat *buffer;

  gint32 i;

  g_mutex_lock (&dchannel->lock);

  /* Если канал не открыт или открыт только для чтения, ошибка. */
  if (dchannel->channel_id < 0 || dchannel->readonly)
    {
      g_mutex_unlock (&dchannel->lock);
      return FALSE;
    }

  if (dchannel->cache != NULL )
    {

      /* Проверяем размер буферов. */
      if (dchannel->raw_buffer_size < size)
        hyscan_data_channel_buffer_realloc (dchannel, size);

      /* Импортируем данные в буфер обработки. */
      n_points = size / hyscan_get_data_point_size (dchannel->discretization_type);
      hyscan_data_import_complex_float (dchannel->discretization_type, data, size,
                                        dchannel->data_buffer, &n_points);
      buffer = (gpointer)dchannel->data_buffer;

      /* Загружаем образцы сигналов. */
      hyscan_data_channel_load_signals (dchannel);

      /* Выполняем свёртку. */
      if (dchannel->convolve )
        {
          convolution = hyscan_data_channel_find_signal (dchannel, time);
          if (convolution != NULL)
            {
              if (!hyscan_convolution_convolve (convolution, dchannel->data_buffer, n_points))
                return -1;
            }
        }

      /* Рассчитываем амплитуду. */
      for( i = 0; i < n_points; i++ )
        {
        gfloat re = dchannel->data_buffer[i].re;
        gfloat im = dchannel->data_buffer[i].im;
        buffer[i] = sqrtf( re*re + im*im );
        }

      /* Запоминаем амплитуду в кэше. */
      hyscan_data_channel_update_cache_key (dchannel, DATA_TYPE_AMPLITUDE, dchannel->next_data_index);
      hyscan_cache_set2 (dchannel->cache, dchannel->cache_key, NULL,
                         &time, sizeof (time),
                         buffer, n_points * sizeof (gfloat));
    }

  /* Записываем данные в канал. */
  status = hyscan_db_add_channel_data (dchannel->db, dchannel->channel_id, time, data, size, &data_index);

  /* Сверяем индекс данных в кэше и в базе данных.
     Если индексы не совпадают очищаем кэш. */
  if (dchannel->cache != NULL)
    {
      if (dchannel->next_data_index == data_index)
        dchannel->next_data_index = data_index + 1;
      else
        hyscan_cache_set (dchannel->cache, dchannel->cache_key, NULL, NULL, 0);
    }

  g_mutex_unlock (&dchannel->lock);

  return status;
}

/* Функция возвращает необработанные акустические данные в формате их записи. */
gboolean
hyscan_data_channel_get_raw_values (HyScanDataChannel *dchannel,
                                    gint32             index,
                                    gpointer           buffer,
                                    gint32            *buffer_size,
                                    gint64            *time)
{
  gboolean status = FALSE;

  g_mutex_lock (&dchannel->lock);
  if (dchannel->channel_id > 0)
    status = hyscan_db_get_channel_data (dchannel->db, dchannel->channel_id, index,
                                         buffer, buffer_size, time);
  g_mutex_unlock (&dchannel->lock);

  return status;
}

/* Функция возвращает значения амплитуды акустического сигнала. */
gboolean
hyscan_data_channel_get_amplitude_values (HyScanDataChannel *dchannel,
                                          gint32             index,
                                          gfloat            *buffer,
                                          gint32            *buffer_size,
                                          gint64            *time)
{
  gboolean status = FALSE;
  gfloat *amplitude;
  gint32 n_points;
  gint32 i;

  g_mutex_lock (&dchannel->lock);

  if (dchannel->channel_id < 0)
    goto exit;

  // Проверяем наличие данных в кэше.
  status = hyscan_data_channel_check_cache (dchannel, DATA_TYPE_AMPLITUDE, index,
                                            buffer, buffer_size, time);
  if (status)
    goto exit;

  // Считываем данные канала.
  n_points = hyscan_data_channel_read_data (dchannel, index);
  if (n_points <= 0)
    goto exit;

  // Возвращаем амплитуду.
  *buffer_size = MIN( *buffer_size, n_points );
  amplitude = (gpointer) dchannel->data_buffer;
  for (i = 0; i < n_points; i++)
    {
      gfloat re = dchannel->data_buffer[i].re;
      gfloat im = dchannel->data_buffer[i].im;
      amplitude[i] = sqrtf (re * re + im * im);
    }
  memcpy (buffer, amplitude, *buffer_size * sizeof(gfloat));

  // Сохраняем данные в кэше.
  if (dchannel->cache != NULL && buffer != NULL)
    hyscan_cache_set2 (dchannel->cache, dchannel->cache_key, NULL, &dchannel->data_time,
                       sizeof(dchannel->data_time), amplitude, n_points * sizeof(gfloat));

  if (time != NULL)
    *time = dchannel->data_time;

  status = TRUE;

exit:
  g_mutex_unlock (&dchannel->lock);

  return status;
}

/* Функция возвращает квадратурные отсчёты акустического сигнала. */
gboolean
hyscan_data_channel_get_quadrature_values (HyScanDataChannel  *dchannel,
                                           gint32              index,
                                           HyScanComplexFloat *buffer,
                                           gint32             *buffer_size,
                                           gint64             *time)
{
  gboolean status = FALSE;
  gint32 n_points;

  g_mutex_lock (&dchannel->lock);

  if (dchannel->channel_id < 0)
    goto exit;

  // Проверяем наличие данных в кэше.
  status = hyscan_data_channel_check_cache (dchannel, DATA_TYPE_QUADRATURE, index,
                                            buffer, buffer_size, time);
  if (status)
    {
      status = TRUE;
      goto exit;
    }

  // Считываем данные канала.
  n_points = hyscan_data_channel_read_data (dchannel, index);
  if (n_points <= 0)
    goto exit;

  // Возвращаем квадратурные отсчёты.
  *buffer_size = MIN( *buffer_size, n_points );
  memcpy (buffer, dchannel->data_buffer, *buffer_size * sizeof(HyScanComplexFloat));

  // Сохраняем данные в кэше.
  if (dchannel->cache && buffer)
    hyscan_cache_set2 (dchannel->cache, dchannel->cache_key, NULL, &dchannel->data_time,
                       sizeof(dchannel->data_time), dchannel->data_buffer,
                       n_points * sizeof(HyScanComplexFloat));

  if (time)
    *time = dchannel->data_time;
  status = TRUE;

  exit:

  g_mutex_unlock (&dchannel->lock);

  return status;
}
