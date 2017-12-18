/*
 * \file hyscan-forward-look-data.c
 *
 * \brief Исходный файл класса обработки данных вперёдсмотрящего локатора
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-forward-look-data.h"
#include "hyscan-core-schemas.h"
#include "hyscan-raw-data.h"

#include <string.h>
#include <math.h>

#define DETAIL_PRECISION       1000.0                          /* Точность округления параметров. */
#define CACHE_HEADER_MAGIC     0x8a09be31                      /* Идентификатор заголовка кэша. */

enum
{
  PROP_O,
  PROP_DB,
  PROP_PROJECT_NAME,
  PROP_TRACK_NAME,
  PROP_RAW
};

/* Структруа заголовка кэша. */
typedef struct
{
  guint32                      magic;                          /* Идентификатор заголовка. */
  guint32                      n_points;                       /* Число точек данных. */
  gint64                       time;                           /* Метка времени. */
} HyScanForwardLookDataCacheHeader;

struct _HyScanForwardLookDataPrivate
{
  HyScanDB                    *db;                             /* Интерфейс базы данных. */
  gchar                       *db_uri;                         /* URI базы данных. */

  gchar                       *project_name;                   /* Название проекта. */
  gchar                       *track_name;                     /* Название галса. */
  gboolean                     raw;                            /* Использовать сырые данные. */

  HyScanCache                 *cache;                          /* Интерфейс системы кэширования. */
  gchar                       *cache_prefix;                   /* Префикс ключа кэширования. */
  gchar                       *cache_key;                      /* Ключ кэширования. */
  gsize                        cache_key_length;               /* Максимальная длина ключа кэширования. */
  gchar                       *detail_key;                     /* Ключ детализации. */
  gsize                        detail_key_length;              /* Максимальная длина ключа детализации. */
  HyScanBuffer                *cache_buffer;                   /* Буфер заголовка кэша данных. */

  gdouble                      data_rate;                      /* Частота дискретизации. */
  gdouble                      sound_velocity;                 /* Скорость звука, м/с. */
  gdouble                      antenna_base;                   /* Расстояние между антеннами, м. */
  gdouble                      frequency;                      /* Центральная рабочая частота, Гц. */
  gdouble                      wave_length;                    /* Длина волны, м. */
  gdouble                      alpha;                          /* Максимальный/минимальный угол по азимуту, рад. */

  HyScanRawData               *channel1;                       /* Сырые данные канала 1. */
  HyScanRawData               *channel2;                       /* Сырые данные канала 2. */

  HyScanBuffer                *doa;                            /* Буфер обработанных данных. */
};

static void    hyscan_forward_look_data_set_property           (GObject                       *object,
                                                                guint                          prop_id,
                                                                const GValue                  *value,
                                                                GParamSpec                    *pspec);
static void    hyscan_forward_look_data_object_constructed     (GObject                       *object);
static void    hyscan_forward_look_data_object_finalize        (GObject                       *object);

static void    hyscan_forward_look_data_update_cache_key       (HyScanForwardLookDataPrivate  *priv,
                                                                guint32                        index);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanForwardLookData, hyscan_forward_look_data, G_TYPE_OBJECT)

static void
hyscan_forward_look_data_class_init (HyScanForwardLookDataClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_forward_look_data_set_property;

  object_class->constructed = hyscan_forward_look_data_object_constructed;
  object_class->finalize = hyscan_forward_look_data_object_finalize;

  g_object_class_install_property (object_class, PROP_DB,
    g_param_spec_object ("db", "DB", "HyScanDB interface", HYSCAN_TYPE_DB,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_PROJECT_NAME,
    g_param_spec_string ("project-name", "ProjectName", "Project name", NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_TRACK_NAME,
    g_param_spec_string ("track-name", "TrackName", "Track name", NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_RAW,
    g_param_spec_boolean ("raw", "Raw", "Use raw data type", FALSE,
                          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_forward_look_data_init (HyScanForwardLookData *data)
{
  data->priv = hyscan_forward_look_data_get_instance_private (data);
}

static void
hyscan_forward_look_data_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  HyScanForwardLookData *data = HYSCAN_FORWARD_LOOK_DATA (object);
  HyScanForwardLookDataPrivate *priv = data->priv;

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

    case PROP_RAW:
      priv->raw = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_forward_look_data_object_constructed (GObject *object)
{
  HyScanForwardLookData *data = HYSCAN_FORWARD_LOOK_DATA (object);
  HyScanForwardLookDataPrivate *priv = data->priv;

  HyScanRawDataInfo channel_info1;
  HyScanRawDataInfo channel_info2;

  if (priv->db == NULL)
    {
      g_warning ("HyScanForwardLookData: db is not specified");
      return;
    }
  if (priv->project_name == NULL)
    {
      g_warning ("HyScanForwardLookData: unknown project name");
      return;
    }
  if (priv->track_name == NULL)
    {
      g_warning ("HyScanForwardLookData: unknown track name");
      return;
    }

  priv->db_uri = hyscan_db_get_uri (priv->db);

  /* Каналы данных. */
  priv->channel1 = hyscan_raw_data_new (priv->db, priv->project_name, priv->track_name, HYSCAN_SOURCE_FORWARD_LOOK, 1);
  priv->channel2 = hyscan_raw_data_new (priv->db, priv->project_name, priv->track_name, HYSCAN_SOURCE_FORWARD_LOOK, 2);

  if ((priv->channel1 == NULL) || (priv->channel2 == NULL))
    {
      g_clear_object (&priv->channel1);
      g_clear_object (&priv->channel2);
      return;
    }

  /* Параметры каналов данных. */
  channel_info1 = hyscan_raw_data_get_info (priv->channel1);
  channel_info2 = hyscan_raw_data_get_info (priv->channel2);

  /* Проверяем параметры каналов данных.
   * Должны быть указаны рабочая частота и база антенны, а также должны
   * совпадать рабочие частоты каналов и частоты оцифровки данных.*/
  if ((channel_info1.antenna_frequency < 1.0) ||
      (fabs (channel_info1.antenna_hoffset - channel_info2.antenna_hoffset) < 1e-4) ||
      (fabs (channel_info1.data_rate - channel_info2.data_rate) > 0.1) ||
      (fabs (channel_info1.antenna_frequency - channel_info2.antenna_frequency) > 0.1))
    {
      g_warning ("HyScanForwardLookData: error in channels parameters");
      g_clear_object (&priv->channel1);
      g_clear_object (&priv->channel2);

      return;
    }

  /* Буферы для обработки. */
  priv->cache_buffer = hyscan_buffer_new ();
  priv->doa = hyscan_buffer_new ();

  /* Параметры обработки. */
  priv->data_rate = channel_info1.data_rate;
  priv->antenna_base = channel_info2.antenna_hoffset - channel_info1.antenna_hoffset;
  priv->frequency = channel_info1.antenna_frequency;
  hyscan_forward_look_data_set_sound_velocity (data, 1500.0);
}

static void
hyscan_forward_look_data_object_finalize (GObject *object)
{
  HyScanForwardLookData *data = HYSCAN_FORWARD_LOOK_DATA (object);
  HyScanForwardLookDataPrivate *priv = data->priv;

  g_clear_object (&priv->cache_buffer);
  g_clear_object (&priv->doa);

  g_clear_object (&priv->cache);
  g_clear_object (&priv->channel1);
  g_clear_object (&priv->channel2);
  g_clear_object (&priv->db);

  g_free (priv->project_name);
  g_free (priv->track_name);
  g_free (priv->db_uri);

  g_free (priv->cache_prefix);
  g_free (priv->cache_key);
  g_free (priv->detail_key);

  G_OBJECT_CLASS (hyscan_forward_look_data_parent_class)->finalize (object);
}

/* Функция создаёт ключ кэширования данных. */
static void
hyscan_forward_look_data_update_cache_key (HyScanForwardLookDataPrivate *priv,
                                           guint32                       index)
{
  guint32 sound_velocity = 1000 * priv->sound_velocity;

  if (priv->cache_key == NULL)
    {
      priv->cache_key = g_strdup_printf ("%s.%s.%s.%s.FL.%ux",
                                         priv->cache_prefix,
                                         priv->db_uri,
                                         priv->project_name,
                                         priv->track_name,
                                         G_MAXUINT32);

      priv->cache_key_length = strlen (priv->cache_key);
    }

  g_snprintf (priv->cache_key, priv->cache_key_length, "%s.%s.%s.%s.FL.%u",
              priv->cache_prefix,
              priv->db_uri,
              priv->project_name,
              priv->track_name,
              index);

  if (priv->detail_key == NULL)
    {
      priv->detail_key = g_strdup_printf ("%ux", G_MAXUINT32);

      priv->detail_key_length = strlen (priv->detail_key);
    }

  g_snprintf (priv->detail_key, priv->detail_key_length, "%u", sound_velocity);
}

/* Функция создаёт новый объект обработки данных вперёдсмотрящего локатора. */
HyScanForwardLookData *
hyscan_forward_look_data_new (HyScanDB    *db,
                              const gchar *project_name,
                              const gchar *track_name,
                              gboolean     raw)
{
  HyScanForwardLookData *data;

  data = g_object_new (HYSCAN_TYPE_FORWARD_LOOK_DATA,
                       "db", db,
                       "project-name", project_name,
                       "track-name", track_name,
                       "raw", raw,
                       NULL);

  if ((data->priv->channel1 == NULL) || (data->priv->channel2 == NULL))
    g_clear_object (&data);

  return data;
}

/* Функция задаёт используемый кэш. */
void
hyscan_forward_look_data_set_cache (HyScanForwardLookData *data,
                                    HyScanCache           *cache,
                                    const gchar           *prefix)
{
  g_return_if_fail (HYSCAN_IS_FORWARD_LOOK_DATA (data));

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
hyscan_forward_look_data_get_position (HyScanForwardLookData *data)
{
  HyScanAntennaPosition zero = {0};

  g_return_val_if_fail (HYSCAN_IS_FORWARD_LOOK_DATA (data), zero);

  if (data->priv->channel1 == NULL)
    return zero;

  return hyscan_raw_data_get_position (data->priv->channel1);
}

/* Функция определяет возможность изменения данных. */
gboolean
hyscan_forward_look_data_is_writable (HyScanForwardLookData *data)
{
  gboolean is_writable1;
  gboolean is_writable2;

  g_return_val_if_fail (HYSCAN_IS_FORWARD_LOOK_DATA (data), FALSE);

  if (data->priv->channel1 == NULL)
    return FALSE;

  is_writable1 = hyscan_raw_data_is_writable (data->priv->channel1);
  is_writable2 = hyscan_raw_data_is_writable (data->priv->channel2);

  return (is_writable1 | is_writable2);
}

/* Функция задаёт скорость звука в воде, используемую для обработки данных. */
void
hyscan_forward_look_data_set_sound_velocity (HyScanForwardLookData *data,
                                             gdouble                sound_velocity)
{
  g_return_if_fail (HYSCAN_IS_FORWARD_LOOK_DATA (data));

  if (sound_velocity <= 0.0)
    return;

  sound_velocity *= DETAIL_PRECISION;
  sound_velocity = round (sound_velocity);
  sound_velocity /= DETAIL_PRECISION;

  data->priv->sound_velocity = sound_velocity;
  data->priv->wave_length = sound_velocity / data->priv->frequency;
  data->priv->alpha = asin (sound_velocity / (2.0 * data->priv->antenna_base * data->priv->frequency));
  data->priv->alpha = ABS (data->priv->alpha);
}

/* Функция возвращает угол, определяющий сектор обзора локатора в горизонтальной плоскости. */
gdouble
hyscan_forward_look_data_get_alpha (HyScanForwardLookData *data)
{
  g_return_val_if_fail (HYSCAN_IS_FORWARD_LOOK_DATA (data), 0.0);

  return data->priv->alpha;
}

/* Функция возвращает диапазон значений индексов записанных данных. */
gboolean
hyscan_forward_look_data_get_range (HyScanForwardLookData *data,
                                    guint32               *first_index,
                                    guint32               *last_index)
{
  g_return_val_if_fail (HYSCAN_IS_FORWARD_LOOK_DATA (data), FALSE);

  if (data->priv->channel1 == NULL)
    return FALSE;

  return hyscan_raw_data_get_range (data->priv->channel1, first_index, last_index);
}

/* Функция возвращает номер изменения в данных. */
guint32
hyscan_forward_look_data_get_mod_count (HyScanForwardLookData *data)
{
  g_return_val_if_fail (HYSCAN_IS_FORWARD_LOOK_DATA (data), 0);

  if (data->priv->channel1 == NULL)
    return 0;

  return hyscan_raw_data_get_mod_count (data->priv->channel1);
}

/* Функция ищет индекс данных для указанного момента времени. */
HyScanDBFindStatus
hyscan_forward_look_data_find_data (HyScanForwardLookData *data,
                                    gint64                 time,
                                    guint32               *lindex,
                                    guint32               *rindex,
                                    gint64                *ltime,
                                    gint64                *rtime)
{
  g_return_val_if_fail (HYSCAN_IS_FORWARD_LOOK_DATA (data), HYSCAN_DB_FIND_FAIL);

  if (data->priv->channel1 == NULL)
    return HYSCAN_DB_FIND_FAIL;

  return hyscan_raw_data_find_data (data->priv->channel1, time, lindex, rindex, ltime, rtime);
}

/* Функция возвращает данные вперёдсмотрящего локатора. */
const HyScanForwardLookDOA *
hyscan_forward_look_data_get_doa_values (HyScanForwardLookData *data,
                                         guint32                index,
                                         guint32               *n_points,
                                         gint64                *time)

{
  HyScanForwardLookDataPrivate *priv;
  HyScanDBFindStatus find_status;
  const HyScanComplexFloat *raw1, *raw2;
  HyScanForwardLookDOA *doa;

  guint32 n_points1, n_points2;
  guint32 index1, index2;
  gint64 time1, time2;

  guint32 i;

  g_return_val_if_fail (HYSCAN_IS_FORWARD_LOOK_DATA (data), NULL);

  priv = data->priv;

  if (priv->channel1 == NULL)
    return NULL;

  /* Ищем данные в кэше. */
  if (priv->cache != NULL)
    {
      HyScanForwardLookDataCacheHeader header;

      /* Ключ кэширования. */
      hyscan_forward_look_data_update_cache_key (priv, index);

      /* Ищем данные в кэше. */
      hyscan_buffer_wrap_data (priv->cache_buffer, HYSCAN_DATA_BLOB, &header, sizeof (header));
      if (hyscan_cache_get2 (priv->cache, priv->cache_key, priv->detail_key,
                             sizeof (header), priv->cache_buffer, priv->doa))
        {
          guint32 cached_n_points;

          cached_n_points  = hyscan_buffer_get_size (priv->doa);
          cached_n_points /= sizeof (HyScanForwardLookDOA);

          /* Верификация данных. */
          if ((header.magic == CACHE_HEADER_MAGIC) &&
              (header.n_points == cached_n_points))
            {
              (time != NULL) ? *time = header.time : 0;
              *n_points = cached_n_points;

              return hyscan_buffer_get_data (priv->doa, &cached_n_points);
            }
        }
    }

  /* Считываем данные первого канала. */
  index1 = index;
  raw1 = hyscan_raw_data_get_quadrature_values (priv->channel1, index1, &n_points1, &time1);
  if (raw1 == NULL)
    return NULL;

  /* Ищем парную строку для указанного индекса. */
  find_status = hyscan_raw_data_find_data (priv->channel2, time1, &index2, NULL, &time2, NULL);
  if ((find_status != HYSCAN_DB_FIND_OK) || (time1 != time2))
    return NULL;

  /* Считываем данные второго канала. */
  raw2 = hyscan_raw_data_get_quadrature_values (priv->channel2, index2, &n_points2, NULL);
  if (raw2 == NULL)
    return NULL;

  if (n_points1 != n_points2)
    {
      g_warning ("HyScanForwardLookData: data size mismatch in '%s.%s' for index %d",
                 priv->project_name, priv->track_name, index);
    }

  /* Корректируем размер буфера данных. */
  *n_points = n_points1 = n_points2 = MIN (n_points1, n_points2);
  hyscan_buffer_set_size (priv->doa, n_points1 * sizeof (HyScanForwardLookDOA));

  /* Расчитываем углы прихода и интенсивности. */
  doa = hyscan_buffer_get_data (priv->doa, &n_points1);
  n_points1 /= sizeof (HyScanForwardLookDOA);
  for (i = 0; i < n_points1; i++)
    {
      gfloat re1 = raw1[i].re;
      gfloat im1 = raw1[i].im;
      gfloat re2 = raw2[i].re;
      gfloat im2 = raw2[i].im;

      gfloat re = re1 * re2 - im1 * -im2;
      gfloat im = re1 * -im2 + im1 * re2;
      gfloat phase = atan2f (im, re);

      doa[i].angle = asinf (phase * priv->wave_length / (2.0 * G_PI * priv->antenna_base));
      doa[i].distance = i * priv->sound_velocity / (2 * priv->data_rate);
      doa[i].amplitude = sqrtf (re1 * re1 + im1 * im1) * sqrtf (re2 * re2 + im2 * im2);
    }

  /* Сохраняем данные в кэше. */
  if (priv->cache != NULL)
    {
      HyScanForwardLookDataCacheHeader header;

      header.magic = CACHE_HEADER_MAGIC;
      header.n_points = n_points1;
      header.time = time1;
      hyscan_buffer_wrap_data (priv->cache_buffer, HYSCAN_DATA_BLOB, &header, sizeof (header));

      hyscan_cache_set2 (priv->cache, priv->cache_key, priv->detail_key, priv->cache_buffer, priv->doa);
    }

  (time != NULL) ? *time = time1 : 0;

  return doa;
}
