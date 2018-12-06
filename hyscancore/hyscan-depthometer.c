/* hyscan-depthometer.c
 *
 * Copyright 2018 Screen LLC, Alexander Dmitriev <m1n7@yandex.ru>
 *
 * This file is part of HyScanCore library.
 *
 * HyScanCore is dual-licensed: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * HyScanCore is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Alternatively, you can license this code under a commercial license.
 * Contact the Screen LLC in this case - <info@screen-co.ru>.
 */

/* HyScanCore имеет двойную лицензию.
 *
 * Во-первых, вы можете распространять HyScanCore на условиях Стандартной
 * Общественной Лицензии GNU версии 3, либо по любой более поздней версии
 * лицензии (по вашему выбору). Полные положения лицензии GNU приведены в
 * <http://www.gnu.org/licenses/>.
 *
 * Во-вторых, этот программный код можно использовать по коммерческой
 * лицензии. Для этого свяжитесь с ООО Экран - <info@screen-co.ru>.
 */

/**
 *
 * SECTION: hyscan-depthometer
 * @Short_description: класс определения и аппроксимации глубины по времени
 * @Title: HyScanDepthometer
 *
 * HyScanDepthometer находится на более высоком уровне, чем #HyScanNavData.
 * Если HyScanNavData работает непосредственно с каналами данных, то HyScanDepthometer ничего
 * не знает о конкретных источниках данных. Однако он занимается определением глубины не для
 * индекса, а для произвольного времени.
 *
 * Класс не является потокобезопасным. Для работы из разных потоков следует
 * создавать как объект HyScanDepthometer, так и интерфейс HyScanNavData.
 */

#include "hyscan-depthometer.h"
#include <math.h>
#include <string.h>

enum
{
  PROP_O,
  PROP_SOURCE,
  PROP_CACHE
};

struct _HyScanDepthometerPrivate
{
  HyScanNavData *source;           /* Источник. */

  HyScanCache   *cache;            /* Кэш. */
  gchar         *key;              /* Ключ кэширования. */
  gint           key_length;       /* Длина ключа. */
  HyScanBuffer  *cache_buffer;     /* Буфер данных кэша. */

  guint32       *indexes;          /* Массив индексов. */
  gint           real_size;        /* Размер массива. */
  gint           size;             /* Количество точек для аппроксимации. */

  gint64         valid;            /* Окно валидности в мкс. */
  gint64         half_valid;       /* Половина окна валидности. */
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
    g_param_spec_object ("source", "DepthSource", "Depth interface", HYSCAN_TYPE_NAV_DATA,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_CACHE,
    g_param_spec_object ("cache", "Cache", "HyScanCache interface", HYSCAN_TYPE_CACHE,
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

    case PROP_CACHE:
      priv->cache = g_value_dup_object (value);
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

  if (!HYSCAN_IS_NAV_DATA (priv->source))
    g_clear_object (&priv->source);

  /* По умолчанию считаем, что окно валидности - 1 мс. */
  priv->valid = 1 * G_TIME_SPAN_MILLISECOND;
  priv->half_valid = priv->valid / 2;
  priv->size = 2;
  priv->real_size = 2;
  priv->indexes = g_malloc0 (2 * priv->size * sizeof (guint32));

  priv->cache_buffer = hyscan_buffer_new ();
}

static void
hyscan_depthometer_object_finalize (GObject *object)
{
  HyScanDepthometer *meter = HYSCAN_DEPTHOMETER (object);
  HyScanDepthometerPrivate *priv = meter->priv;

  g_clear_object (&priv->cache_buffer);
  g_clear_object (&priv->source);
  g_clear_object (&priv->cache);

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

  token = hyscan_nav_data_get_token (priv->source);

  if (priv->key == NULL)
    {
      priv->key = g_strdup_printf ("depthometer.%s.%i.%"G_GINT64_FORMAT".%"G_GINT64_FORMAT,
                                    token, G_MAXINT, G_MAXINT64, G_MAXINT64);
      priv->key_length = strlen (priv->key);
    }

  g_snprintf (priv->key, priv->key_length, "depthometer.%s.%i.%"G_GINT64_FORMAT".%"G_GINT64_FORMAT,
              token, priv->size, priv->valid, time);
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

/**
 * hyscan_depthometer_new:
 * @ndata: интерфейс #HyScanNavData
 * @cache: (nullable): интерфейс #HyScanCache
 *
 * Функция создает новый объект получения глубины.
 *
 * Returns: объект HyScanDepthometer.
 */
HyScanDepthometer*
hyscan_depthometer_new (HyScanNavData *ndata,
                        HyScanCache   *cache)
{
  if (!HYSCAN_IS_NAV_DATA (ndata))
    return NULL;

  return g_object_new (HYSCAN_TYPE_DEPTHOMETER,
                       "source", ndata,
                       "cache", cache, NULL);
}

/**
 * hyscan_depthometer_set_filter_size:
 * @depthometer: #HyScanDepthometer
 * @size: число точек, используемых для определения глубины
 *
 * Функция устанавливает число точек для аппроксимации.
 * Число точек должно быть больше нуля. Так как классу требуется четное число
 * точек, то при передаче нечетного значения оно будет автоматически инкрементировано.
 *
 * Returns: TRUE, если удалось установить новое значение.
 */
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

/**
 * hyscan_depthometer_set_validity_time:
 * @depthometer: #HyScanDepthometer
 * @microseconds: время в микросекундах, в течение которого считается, чт
 * глубина не изменяется.
 *
 * Функция устанавливает окно валидности данных.
 *
 * Данные могут быть запрошены не для того момента, для которого они реально
 * есть, а чуть раньше или позже. В этом случае есть два варианта:
 * либо интерполировать данные, либо "нарезать" временную шкалу так,
 * так что на каждом отрезке глубина считается постоянной. Функция задает
 * длину этого отрезка в микросекундах.
 */
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

/**
 * hyscan_depthometer_get:
 * @depthometer: #HyScanDepthometer
 * @time: время
 *
 * Функция возвращает глубину.
 * Если значение не найдено в кэше или кэш не установлен, функция
 * произведет все необходимые вычисления.
 *
 * Returns: глубина в запрошенный момент времени или -1.0 в случае ошибки.
 */
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
      hyscan_buffer_wrap_data (priv->cache_buffer, HYSCAN_DATA_BLOB, &retval, sizeof (retval));
      if (hyscan_cache_get (priv->cache, priv->key, NULL, priv->cache_buffer))
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
  if (hyscan_nav_data_get_range (priv->source, &first, &last))
    found = hyscan_nav_data_find_data (priv->source, time, &lindex, &rindex, &ltime, &rtime);
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
    {
      gdouble current = 0;
      hyscan_nav_data_get (priv->source, priv->indexes[i], NULL, &current);
      retval += current;
    }

  /* Сейчас используется среднее арифметическое. */
  retval /= size;

  /* Кладем получившееся значение в кэш. */
  if (priv->cache != NULL)
    {
      hyscan_buffer_wrap_data (priv->cache_buffer, HYSCAN_DATA_BLOB, &retval, sizeof (retval));
      hyscan_cache_set (priv->cache, priv->key, NULL, priv->cache_buffer);
    }

  return retval;
}

/**
 * hyscan_depthometer_check:
 * @depthometer: #HyScanDepthometer
 * @time: время
 *
 * Функция возвращает глубину.
 * В отличие от #hyscan_depthometer_get эта функция только ищет
 * значение в кэше. Если кэш не задан или значение не присутствует в кэше,
 * будет возвращено значение -1.0.
 *
 * Returns: глубина в запрошенный момент времени или -1.0 в случае ошибки.
 */
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

  hyscan_buffer_wrap_data (priv->cache_buffer, HYSCAN_DATA_BLOB, &retval, sizeof (retval));
  if (hyscan_cache_get (priv->cache, priv->key, NULL, priv->cache_buffer))
    if (retval_size == sizeof (retval))
      return retval;

  return -1.0;
}
