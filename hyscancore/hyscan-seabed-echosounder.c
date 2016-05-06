/*
 * \file hyscan-seabed-echosounder.c
 *
 * \brief Исходный файл класса определения глубины по эхолоту
 * \author Alexander Dmitriev (m1n7@yandex.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 */
#define MAXPEAKS 10
#include "hyscan-seabed-echosounder.h"
#include "hyscan-data-channel.h"
#include <hyscan-data.h>
#include <math.h>
#include <string.h>

enum
{
  PROP_O,
  PROP_DB,
  PROP_CACHE,
  PROP_CACHE_PREFIX,
  PROP_PROJECT,
  PROP_TRACK,
  PROP_CHANNEL,
  PROP_Q,
};

struct _HyScanSeabedEchosounderPrivate
{
  HyScanDB              *db;                            /* Интерфейс базы данных. */
  gchar                 *uri;                           /* URI базы данных. */

  HyScanCache           *cache;                         /* Интерфейс системы кэширования. */
  gchar                 *cache_prefix;                  /* Префикс ключа кэширования. */
  gchar                 *cache_key;                     /* Ключ кэширования. */
  gint                   cache_key_length;              /* Максимальная длина ключа. */

  gchar                 *project;                       /* Название проекта. */
  gchar                 *track;                         /* Название галса. */
  gchar                 *channel;                       /* Название канала данных. */

  HyScanDataChannel     *data;                          /* Указатель на канал данных. */
  gfloat                *data_buffer0;                  /* Буффер для входных данных. */
  gfloat                *data_buffer1;                  /* Буффер для обработки данных. */
  gint32                 data_buffer_size;              /* Размер обрабатываемых данных (фактически, количество точек). */

  gfloat                 discretization_frequency;      /* Частота дискретизации данных. */
  gdouble                quality;                       /* Качество входных данных. */
  GArray                *soundspeed;                    /* Таблично заданная скорость звука. */
  gint32                 soundspeed_size;               /* Размер таблицы скоростей звука. */

  gboolean               status;                        /* Для проверки результатов выполнения. */
  GMutex                 lock;                          /* Блокировка многопоточного доступа. */
};

static void     hyscan_seabed_echosounder_set_property          (GObject               *object,
                                                                 guint                  prop_id,
                                                                 const GValue          *value,
                                                                 GParamSpec            *pspec);

static void     hyscan_seabed_echosounder_object_constructed    (GObject *object);

static void     hyscan_seabed_echosounder_object_finalize       (GObject *object);

gdouble         hyscan_seabed_echosounder_get_depth_by_index    (HyScanSeabed            *seabed,
                                                                 gint32                   index);

static gdouble  hyscan_seabed_echosounder_get_depth_int         (HyScanSeabedEchosounder *seabed,
                                                                 gint32                   index);

static gboolean hyscan_seabed_echosounder_update_cache_key      (HyScanSeabedEchosounder *seabed,
                                                                 gint32                   index);

static gboolean hyscan_seabed_echosounder_set_cache             (HyScanSeabedEchosounder *seabed,
                                                                 gint32                   index,
                                                                 gdouble                 *depth);

static gboolean hyscan_seabed_echosounder_get_cache             (HyScanSeabedEchosounder *seabed,
                                                                 gint32                   index,
                                                                 gdouble                 *depth);

void            hyscan_seabed_echosounder_set_soundspeed        (HyScanSeabed            *seabed,
                                                                 GArray                  *soundspeed);

static void     hyscan_seabed_echosounder_interface_init        (HyScanSeabedInterface *iface);

G_DEFINE_TYPE_WITH_CODE (HyScanSeabedEchosounder, hyscan_seabed_echosounder, G_TYPE_OBJECT,
    G_ADD_PRIVATE (HyScanSeabedEchosounder)
    G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_SEABED, hyscan_seabed_echosounder_interface_init));

static void
hyscan_seabed_echosounder_class_init (HyScanSeabedEchosounderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = hyscan_seabed_echosounder_object_constructed;
  object_class->finalize = hyscan_seabed_echosounder_object_finalize;
  object_class->set_property = hyscan_seabed_echosounder_set_property;

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
      g_param_spec_string ("project", "ProjectName", "project name", NULL,
        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_TRACK,
      g_param_spec_string ("track", "Track", "Track name", NULL,
        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_CHANNEL,
      g_param_spec_string ("channel", "Channel", "Channel name", NULL,
        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_Q,
      g_param_spec_double ("quality", "Quality", "Quality of input", 0.0, 1.0, 0.5,
        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));
}

/* Первый этап создания объекта. */
static void
hyscan_seabed_echosounder_init (HyScanSeabedEchosounder *seabed_echosounder)
{
  seabed_echosounder->priv = hyscan_seabed_echosounder_get_instance_private (
      seabed_echosounder);
}

/* Второй этап создания объекта. */
static void
hyscan_seabed_echosounder_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  HyScanSeabedEchosounder *seabed_echosounder = HYSCAN_SEABED_ECHOSOUNDER(object);
  HyScanSeabedEchosounderPrivate *priv = seabed_echosounder->priv;

  switch (prop_id)
    {
    case PROP_DB:
      priv->db = g_value_dup_object (value);
      break;

    case PROP_CACHE:
      priv->cache = g_value_dup_object (value);
      break;

    case PROP_CACHE_PREFIX:
      priv->cache_prefix = g_value_dup_string (value);
      break;

    case PROP_PROJECT:
      priv->project = g_value_dup_string (value);
      break;

    case PROP_TRACK:
      priv->track = g_value_dup_string (value);
      break;

    case PROP_CHANNEL:
      priv->channel = g_value_dup_string (value);
      break;

    case PROP_Q:
      priv->quality = g_value_get_double (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (seabed_echosounder, prop_id, pspec);
      break;
    }
}

/*  Функция установки профиля скорости звука*/
void
hyscan_seabed_echosounder_set_soundspeed (HyScanSeabed *seabed,
                                          GArray       *soundspeed)
{
  HyScanSeabedEchosounder *seabedecho = HYSCAN_SEABED_ECHOSOUNDER (seabed);
  HyScanSeabedEchosounderPrivate *priv = seabedecho->priv;
  int i;
  gdouble sum = 0;
  SoundSpeedTable sst;

  /* Очищаем таблицу профиля скорости звука*/
  g_array_free(priv->soundspeed, TRUE);
  priv->soundspeed = g_array_new (FALSE, FALSE, sizeof(SoundSpeedTable));

  /* Наполняем таблицу новыми данными и переводим глубину из метров в дискреты*/
  for (i = 0; i < soundspeed->len; i++)
    {
      sst = g_array_index (soundspeed, SoundSpeedTable, i);
      sst.depth = sst.depth * (priv->discretization_frequency * 2 / sst.soundspeed) + sum;
      sum += sst.depth;
      g_array_append_val (priv->soundspeed, sst);
    }

  priv->soundspeed_size = priv->soundspeed->len;
}

/* третий этап создания объекта, после которого публичные методы обязаны работать*/
static void
hyscan_seabed_echosounder_object_constructed (GObject *object)
{
  HyScanSeabedEchosounder *seabed = HYSCAN_SEABED_ECHOSOUNDER(object);
  HyScanSeabedEchosounderPrivate *priv = seabed->priv;

  HyScanDataChannelInfo *info;
  SoundSpeedTable sst;

  if (priv->db == NULL)
    return;

  priv->uri = hyscan_db_get_uri (priv->db);

  sst.depth = 0;
  sst.soundspeed = 1500; /* по умолчанию считаем скорость звука 1500 м/с*/

  priv->data = hyscan_data_channel_new_with_cache (priv->db, priv->project, priv->track, priv->channel, priv->cache);
  info = hyscan_data_channel_get_info (priv->data);
  if (info != NULL)
    {
      priv->discretization_frequency = info->discretization_frequency;
      priv->status = TRUE;
      g_free (info);
    }

  priv->soundspeed = g_array_new (FALSE, FALSE, sizeof(SoundSpeedTable));
  g_array_append_val (priv->soundspeed, sst);

  G_OBJECT_CLASS (hyscan_seabed_echosounder_parent_class)->constructed (object);
}

HyScanSeabed *
hyscan_seabed_echosounder_new (HyScanDB    *db,
                               HyScanCache *cache,
                               gchar       *cache_prefix,
                               gchar       *project,
                               gchar       *track,
                               gchar       *channel,
                               gdouble     quality)
{
  return g_object_new (HYSCAN_TYPE_SEABED_ECHOSOUNDER,
                       "db", db,
                       "cache", cache,
                       "cache_prefix", cache_prefix,
                       "project", project,
                       "track", track,
                       "channel", channel,
                       "quality", quality,
                       NULL);
}

static void
hyscan_seabed_echosounder_object_finalize (GObject *object)
{
  HyScanSeabedEchosounder *seabed = HYSCAN_SEABED_ECHOSOUNDER(object);
  HyScanSeabedEchosounderPrivate *priv = seabed->priv;

  g_free (priv->data_buffer0);
  g_free (priv->data_buffer1);

  g_clear_object (&priv->db);
  g_clear_object (&priv->data);
  g_clear_object (&priv->cache);

  g_array_free(priv->soundspeed, TRUE);

  g_mutex_clear (&priv->lock);

  G_OBJECT_CLASS (hyscan_seabed_echosounder_parent_class)->finalize (object);
}

/* Функция определения глубины по индексу записи. Представляет собой обертку над #hyscan_seabed_echosounder_get_depth_int.*/
gdouble
hyscan_seabed_echosounder_get_depth_by_index (HyScanSeabed *seabed,
                                              gint32        index)
{
  HyScanSeabedEchosounder *seabedecho = HYSCAN_SEABED_ECHOSOUNDER (seabed);
  HyScanSeabedEchosounderPrivate *priv = seabedecho->priv;
  gdouble depth = -1;
  gboolean status;

  g_mutex_lock (&priv->lock);

  status = hyscan_seabed_echosounder_get_cache (seabedecho, index, &depth);
  if (status)
    {
      g_mutex_unlock (&priv->lock);
      return depth;
    }

  depth = hyscan_seabed_echosounder_get_depth_int (seabedecho, index);
  hyscan_seabed_echosounder_set_cache (seabedecho, index, &depth);

  g_mutex_unlock (&priv->lock);
  return depth;
}

/* Внутренняя функция определения глубины. */
static gdouble
hyscan_seabed_echosounder_get_depth_int (HyScanSeabedEchosounder *seabed,
                                         gint32                   index)
{
  HyScanSeabedEchosounderPrivate *priv = seabed->priv;

  int i, j, k;
  gfloat average_value = 0;             /* потребуется для усреднения. */
  gfloat stdev = 0;                     /* среднеквадратичное отклонение .*/
  gint peaks[2][MAXPEAKS] = {{0}, {0}}; /* координаты пиков, при этом peaks[0] - это начала, peaks[1] - концы .*/
  gint peakcounter = 0;                 /* Счетчик пиков .*/
  gint widest_peak_size = 0;            /* Размер самого широкого пика.*/
  gint widest_peak_begin = 0;           /* Координата начала самого широкого пика .*/
  gint64 *time = 0;
  gfloat discretization_frequency = priv->discretization_frequency;
  gdouble depth = -1;                   /* Глубина. Принимает значение -1, если что-то не так. */
  gboolean status;

  gint32 soundspeed_max = 0;                     /* Индекс наибольшего элемента таблицы скорости звука, меньшего определенного номера дискреты*/
  SoundSpeedTable *soundspeed0, *soundspeed1;    /* Таблица скоростей звука в зависимости от глубины*/
  gint32 soundspeed_size = priv->soundspeed_size;/* Размер таблицы. */

  gfloat *data_buffer0 = priv->data_buffer0;
  gfloat *data_buffer1 = priv->data_buffer1;
  gint32 data_buffer_size = priv->data_buffer_size;

  /* Узнаем количество точек в строке данных, выделяем память и заполняем её. */
  data_buffer_size = hyscan_data_channel_get_values_count (priv->data, index);
  if (data_buffer_size == -1)
    return -1;

  data_buffer0 = g_realloc (data_buffer0, data_buffer_size * sizeof(gfloat));
  data_buffer1 = g_realloc (data_buffer1, data_buffer_size * sizeof(gfloat));

  status = hyscan_data_channel_get_amplitude_values (priv->data, index, data_buffer0, &data_buffer_size, time);
  if (status == FALSE)
    return -1;

  /* TODO: обсудить,сколько точек можно занулять. */
  /* Фильтруем вход. Сейчас просто берется среднее с окном 3. */
  data_buffer1[0] = data_buffer0[0];
  data_buffer1[data_buffer_size - 1] = data_buffer0[data_buffer_size - 1];
  average_value = data_buffer1[0] + data_buffer0[data_buffer_size - 1];
  for (i = 1; i < data_buffer_size - 1; i++)
    {
      data_buffer1[i] = (data_buffer0[i-1] + data_buffer0[i] + data_buffer0[i+1]) / 3.0;
      average_value += data_buffer1[i];
    }
  average_value /= data_buffer_size;

  /* Вычисляем СКО. */
  for (i = 0; i < data_buffer_size; i++)
    stdev += pow ((data_buffer1[i] - average_value), 2);

  stdev /= data_buffer_size;
  stdev += average_value; /* - это наш порог бинаризации */

  for (i = 0; i < data_buffer_size; i++)
    {
      if (data_buffer1[i] > stdev)
        data_buffer1[i] = 1;
      else
        data_buffer1[i] = 0;
    }

  /* Ищем первые MAXPEAKS пиков. */
  i = i + 0;
  for (i = 0; i < data_buffer_size && peakcounter < MAXPEAKS; i++)
    {
      /* тут есть проблема в логике. Если d_b1[i=0] == 1, то peaks[0][0]= i=0 и этот шаг пропускается.
       * Но нас это волновать не должно, т.к. мало того, что в 0 дна быть не должно,
       * так ещё и будем занулять эти точки.
       */
      if (peaks[0][peakcounter] == 0 && data_buffer1[i] > 0)
        peaks[0][peakcounter] = i;
      if (peaks[0][peakcounter] != 0 && (data_buffer1[i] == 0 || i == data_buffer_size - 1))
        {
          peaks[1][peakcounter] = i - 1;
          peakcounter++;
        }
    }

  /* Теперь объединяем пики, если расстояние от конца первого до начала второго не более
   * 1/4 расстояния от начала первого до конца второго.
   */
  for (i = 0; i < peakcounter && peakcounter > 1; i++)
    {
      for (j = i + 1; j < peakcounter; j++)
        {
          if ((float) (peaks[0][j] - peaks[1][i]) / (float) (peaks[1][j] - peaks[0][i]) <= 0.25)
            {
              for (k = peaks[1][i]; k < peaks[0][j]; k++)
                data_buffer1[k] = 1;
              peaks[1][i] = peaks[1][j];
            }
        }
    }

  /* Ищем наиболее широкий пик. */
  widest_peak_begin = peaks[0][0];
  widest_peak_size = peaks[1][0] - peaks[0][0];
  if (peakcounter > 1)
    {
      for (i = 1; i < peakcounter; i++)
        {
          if ((peaks[1][i] - peaks[0][i]) > widest_peak_size)
            {
              widest_peak_size = peaks[1][i] - peaks[0][i];
              widest_peak_begin = peaks[0][i];
            }
        }
    }

  /* Вычисляем глубину по таблице глубина/скорость звука.
   * Для этого сначала проводим численное интегрирование,
   * а потом делим на 2*Fдискр.
   */
  depth = 0;

  for (i = 0; i < soundspeed_size; i++)
    {
      soundspeed0 = &g_array_index (priv->soundspeed, SoundSpeedTable, i);

      if (widest_peak_begin > soundspeed0->depth)
        {
          soundspeed_max = i;
          if (i > 0)
            {
              soundspeed1 = &g_array_index (priv->soundspeed, SoundSpeedTable, i-1);
              depth += (soundspeed0->depth - soundspeed1->depth) * soundspeed1->soundspeed;
            }
        }
      if (widest_peak_begin <= soundspeed0->depth)
        break;
    }
  soundspeed0 = &g_array_index (priv->soundspeed, SoundSpeedTable, soundspeed_max);
  depth += (widest_peak_begin - soundspeed0->depth) * soundspeed0->soundspeed;

  depth /= (discretization_frequency * 2);

  return depth;
}

static gboolean
hyscan_seabed_echosounder_update_cache_key (HyScanSeabedEchosounder *seabed,
                                            gint32                   index)
{
  HyScanSeabedEchosounderPrivate *priv = seabed->priv;

  if (priv->cache == NULL)
    return FALSE;

  /* Обновляем ключ кеширования. */
  if (priv->cache != NULL && priv->cache_key == NULL)
    {
      if (priv->cache_prefix != NULL)
        {
          priv->cache_key = g_strdup_printf ("%s.%s.%s.%s.%s.0123456789", priv->uri,
                                             priv->cache_prefix,
                                             priv->project,
                                             priv->track,
                                             priv->channel);
        }
      else
        {
          priv->cache_key = g_strdup_printf ("%s.%s.%s.%s.0123456789",
                                             priv->uri,
                                             priv->project,
                                             priv->track,
                                             priv->channel);
        }
      priv->cache_key_length = strlen (priv->cache_key);
    }

  if (priv->cache_prefix)
    {
      g_snprintf (priv->cache_key, priv->cache_key_length, "%s.%s.%s.%s.%s.%d",
                  priv->uri,
                  priv->cache_prefix,
                  priv->project,
                  priv->track,
                  priv->channel,
                  index);
    }
  else
    {
      g_snprintf (priv->cache_key, priv->cache_key_length, "%s.%s.%s.%s.%d",
                  priv->uri,
                  priv->project,
                  priv->track,
                  priv->channel,
                  index);
    }
  return TRUE;
}

static gboolean
hyscan_seabed_echosounder_set_cache (HyScanSeabedEchosounder *seabed,
                                     gint32                   index,
                                     gdouble                 *depth)
{
  HyScanSeabedEchosounderPrivate *priv = seabed->priv;
  gboolean status;

  if (priv->cache == NULL)
    return FALSE;

  /* Обновляем ключ кеширования. */
  status = hyscan_seabed_echosounder_update_cache_key (seabed, index);

  if (!status)
    return FALSE;
  /* Если успешно, то пишем значение в кеш. */
  status = hyscan_cache_set (priv->cache, priv->cache_key, NULL, depth, sizeof(gdouble));

  return status;
}

static gboolean
hyscan_seabed_echosounder_get_cache (HyScanSeabedEchosounder *seabed,
                                     gint32                   index,
                                     gdouble                 *depth)
{
  HyScanSeabedEchosounderPrivate *priv = seabed->priv;
  gboolean status;
  gint32 buffer_size;

  buffer_size = sizeof(gdouble);

  if (priv->cache == NULL)
    return FALSE;

  /* Обновляем ключ кеширования. */
  status = hyscan_seabed_echosounder_update_cache_key (seabed, index);

  if (!status)
    return FALSE;

  /* Если успешно, ищем данные в кеше. */
  status = hyscan_cache_get (priv->cache, priv->cache_key, NULL, depth, &buffer_size);

  return status;
}

static void
hyscan_seabed_echosounder_interface_init (HyScanSeabedInterface *iface)
{
  iface->get_depth_by_index = hyscan_seabed_echosounder_get_depth_by_index;
  iface->set_soundspeed = hyscan_seabed_echosounder_set_soundspeed;
}
