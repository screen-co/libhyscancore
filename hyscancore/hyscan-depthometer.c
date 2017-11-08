/*
 * \file hyscan-depthometer.c
 *
 * \brief Исходный файл класса определения и аппроксимации
 * \author Alexander Dmitriev (m1n7@yandex.ru)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-depthometer.h"
#include <math.h>
#include <string.h>

enum
{
  PROP_O,
  PROP_SOURCE
};

struct _HyScanDepthometerPrivate
{
  HyScanDepth  *source;           /* Источник. */

  HyScanCache  *cache;            /* Кэш. */
  gchar        *prefix;           /* Префикс системы кэширования. */
  gchar        *key;              /* Ключ кэширования. */
  gint          key_length;       /* Длина ключа. */

  guint32      *indexes;          /* Массив индексов. */
  gint          real_size;        /* Размер массива. */
  gint          size;             /* Количество точек для аппроксимации. */

  gint64        valid;            /* Окно валидности в мкс. */
  gint64        half_valid;       /* Половина окна валидности. */
};

static void    hyscan_depthometer_set_property             (GObject               *object,
                                                            guint                  prop_id,
                                                            const GValue          *value,
                                                            GParamSpec            *pspec);
static void    hyscan_depthometer_object_constructed       (GObject               *object);
static void    hyscan_depthometer_object_finalize          (GObject               *object);
static gint64  hyscan_depthometer_time_round               (gint64                 time,
                                                            gint64                 valid,
                                                            gint64                 half);
static void    hyscan_depthometer_update_cache_key         (HyScanDepthometer     *depthometer,
                                                            gint64                 time);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanDepthometer, hyscan_depthometer, G_TYPE_OBJECT);

static void
hyscan_depthometer_class_init (HyScanDepthometerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_depthometer_set_property;

  object_class->constructed = hyscan_depthometer_object_constructed;
  object_class->finalize = hyscan_depthometer_object_finalize;

  g_object_class_install_property (object_class, PROP_SOURCE,
    g_param_spec_object ("source", "DepthSource", "Depth interface", HYSCAN_TYPE_DEPTH,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_depthometer_init (HyScanDepthometer *depthometer)
{
  depthometer->priv = hyscan_depthometer_get_instance_private (depthometer);
}

static void
hyscan_depthometer_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  HyScanDepthometer *meter = HYSCAN_DEPTHOMETER (object);
  HyScanDepthometerPrivate *priv = meter->priv;

  switch (prop_id)
    {
    case PROP_SOURCE:
      priv->source = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_depthometer_object_constructed (GObject *object)
{
  HyScanDepthometer *meter = HYSCAN_DEPTHOMETER (object);
  HyScanDepthometerPrivate *priv = meter->priv;

  if (!HYSCAN_IS_DEPTH (priv->source))
    g_clear_object (&priv->source);

  /* По умолчанию считаем, что окно валидности - 1 мс. */
  priv->valid = 1 * G_TIME_SPAN_MILLISECOND;
  priv->half_valid = priv->valid / 2;
  priv->size = 2;
  priv->real_size = 2;
  priv->indexes = g_malloc0 (2 * priv->size * sizeof (guint32));
}

static void
hyscan_depthometer_object_finalize (GObject *object)
{
  HyScanDepthometer *meter = HYSCAN_DEPTHOMETER (object);
  HyScanDepthometerPrivate *priv = meter->priv;

  g_clear_object (&priv->source);
  g_clear_object (&priv->cache);
  g_free (priv->prefix);

  g_free (priv->indexes);

  g_free (priv->key);

  G_OBJECT_CLASS (hyscan_depthometer_parent_class)->finalize (object);
}

/* Функция генерирует и обновляет ключ кэширования. */
static void
hyscan_depthometer_update_cache_key (HyScanDepthometer *meter,
                                     gint64             time)
{
  const gchar *token;
  HyScanDepthometerPrivate *priv = meter->priv;

  token = hyscan_depth_get_token (priv->source);

  if (priv->key == NULL)
    {
      priv->key = g_strdup_printf ("depthometer.%s.%s.%i.%"G_GINT64_FORMAT".%"G_GINT64_FORMAT,
                                    priv->prefix, token,
                                    G_MAXINT, G_MAXINT64, G_MAXINT64);
      priv->key_length = strlen (priv->key);
    }

  g_snprintf (priv->key, priv->key_length, "depthometer.%s.%s.%i.%"G_GINT64_FORMAT".%"G_GINT64_FORMAT,
              priv->prefix, token, priv->size, priv->valid, time);
}

/* Функция выравнивания времени по окну валидности. */
static inline gint64
hyscan_depthometer_time_round (gint64 time,
                               gint64 valid,
                               gint64 half)
{
  gint64 out = time / valid;

  if (time % valid >= half)
    out = (out + 1) * valid;
  else
    out = out * valid;

  return out;
}

/* Функция создает новый объект. */
HyScanDepthometer*
hyscan_depthometer_new (HyScanDepth *depth)
{
  if (!HYSCAN_IS_DEPTH (depth))
    return NULL;

  return g_object_new (HYSCAN_TYPE_DEPTHOMETER,
                       "source", depth, NULL);
}

/* Функция устанавливает кэш. */
void
hyscan_depthometer_set_cache (HyScanDepthometer *meter,
                              HyScanCache       *cache,
                              gchar             *prefix)
{
  HyScanDepthometerPrivate *priv;

  g_return_if_fail (HYSCAN_IS_DEPTHOMETER (meter));
  priv = meter->priv;

  g_clear_object (&priv->cache);
  g_free (priv->prefix);

  if (cache == NULL)
    return;

  priv->cache = g_object_ref (cache);
  priv->prefix = g_strdup ((prefix == NULL) ? "none" : prefix);
}

/* Функция устанавливает число точек для аппроксимации. */
gboolean
hyscan_depthometer_set_filter_size (HyScanDepthometer *meter,
                                    guint              size)
{
  g_return_val_if_fail (HYSCAN_IS_DEPTHOMETER (meter), FALSE);

  if (size == 0)
    return FALSE;

  if (size % 2 != 0)
    {
      size++;
      g_warning ("HyScanDepthometer: size of filter should be even. Setting to %i", size);
    }

  meter->priv->size = size;
  return TRUE;
}

/* Функция устанавливает окно валидности данных. */
void
hyscan_depthometer_set_validity_time (HyScanDepthometer *meter,
                                      gint64             microseconds)
{
  g_return_if_fail (HYSCAN_IS_DEPTHOMETER (meter));

  /* Минимальное значение - 1, чтобы не делить на ноль. */
  if (microseconds == 0)
    microseconds = 1;

  meter->priv->valid = microseconds;
  meter->priv->half_valid = microseconds / 2;
}

/* Функция возвращает глубину. */
gdouble
hyscan_depthometer_get (HyScanDepthometer *meter,
                        gint64             time)
{
  HyScanDepthometerPrivate *priv;
  HyScanDBFindStatus found = HYSCAN_DB_FIND_FAIL;
  guint32 first, last;
  guint32 lindex, rindex;
  gint64 ltime, rtime;
  gint i, size, half;
  gdouble retval = 0;
  guint32 retval_size = sizeof (retval);

  g_return_val_if_fail (HYSCAN_IS_DEPTHOMETER (meter), -1.0);
  priv = meter->priv;

  if (priv->source == NULL)
    return -1.0;

  /* Для начала преобразовываем запрошенное время. */
  time = hyscan_depthometer_time_round (time, priv->valid, priv->half_valid);
  size = priv->size;

  /* Сначала ищем уже посчитанное значение в кэше. */
  if (priv->cache != NULL)
    {
      hyscan_depthometer_update_cache_key (meter, time);
      if (hyscan_cache_get (priv->cache, priv->key, NULL, &retval, &retval_size))
        if (retval_size == sizeof (retval))
          return retval;
    }

  /* Если не нашли, вычисляем. */
  /* Если нужно, реаллоцируем память под массивы индексов и значений. */
  size = priv->size;
  half = size / 2;
  if (size != priv->real_size)
    {
      priv->indexes = g_realloc (priv->indexes, size * sizeof (guint32));
      priv->real_size = size;
    }

  /* Определяем диапазон данных для запрошенной временной метки. */
  if (hyscan_depth_get_range (priv->source, &first, &last))
    found = hyscan_depth_find_data (priv->source, time, &lindex, &rindex, &ltime, &rtime);
  else
    found = HYSCAN_DB_FIND_FAIL;

  /* Если не получилось найти, возвращаем 0. */
  if (found != HYSCAN_DB_FIND_OK)
    return -1.0;

  /* Заполняем массив индексами. */
  for (i = 0; i < half; i++)
    {
      priv->indexes[half - 1 - i] = MAX (lindex - i, first);
      priv->indexes[half + i] = MIN (rindex + i, last);
    }

  /* Получаем глубину для каждого индекса.*/
  for (i = 0; i < size; i++)
    retval += hyscan_depth_get (priv->source, priv->indexes[i], NULL);

  /* Сейчас используется среднее арифметическое. */
  retval /= size;

  /* Кладем получившееся значение в кэш. */
  if (priv->cache != NULL)
    hyscan_cache_set (priv->cache, priv->key, NULL, &retval, retval_size);

  return retval;
}

gdouble
hyscan_depthometer_check (HyScanDepthometer *meter,
                          gint64             time)
{
  gdouble retval;
  guint32 retval_size = sizeof (retval);
  HyScanDepthometerPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_DEPTHOMETER (meter), -1.0);
  priv = meter->priv;

  if (priv->source == NULL)
    return -1.0;

  if (priv->cache == NULL)
    return -1.0;

  time = hyscan_depthometer_time_round (time, priv->valid, priv->half_valid);

  hyscan_depthometer_update_cache_key (meter, time);

  if (hyscan_cache_get (priv->cache, priv->key, NULL, &retval, &retval_size))
    if (retval_size == sizeof (retval))
      return retval;

  return -1.0;
}
