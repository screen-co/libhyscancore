/* hyscan-quality.c
 *
 * Copyright 2020 Screen LLC, Alexey Sakhnov <alexsakhnov@gmail.com>
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
 * SECTION: hyscan-quality
 * @Short_description: определение качества акустических данных
 * @Title: HyScanQuality
 *
 * Класс является заглушкой и представляет простую реализацию определения качества.
 *
 */

#include "hyscan-quality.h"
#include "hyscan-projector.h"
#include <hyscan-stats.h>
#include <math.h>

#define WINDOW_TIME_SPAN    (4 * G_TIME_SPAN_SECOND)
#define MAX_DEVIATION_ALONG 1.5

enum
{
  PROP_O,
  PROP_AMPLITUDE,
  PROP_NAV_DATA,
  PROP_ESTIMATOR,
};

struct _HyScanQualityPrivate
{
  HyScanAmplitude     *amplitude;
  HyScanNavData       *nav_data;
  HyScanProjector     *projector;
  HyScanDataEstimator *estimator;
  GArray              *buffer;
};

static void    hyscan_quality_set_property             (GObject               *object,
                                                        guint                  prop_id,
                                                        const GValue          *value,
                                                        GParamSpec            *pspec);
static void    hyscan_quality_object_constructed       (GObject               *object);
static void    hyscan_quality_object_finalize          (GObject               *object);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanQuality, hyscan_quality, G_TYPE_OBJECT)

static void
hyscan_quality_class_init (HyScanQualityClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_quality_set_property;

  object_class->constructed = hyscan_quality_object_constructed;
  object_class->finalize = hyscan_quality_object_finalize;

  g_object_class_install_property (object_class, PROP_AMPLITUDE,
    g_param_spec_object ("amplitude", "Amplitude", "HyScanAmplitude to estimate quality", HYSCAN_TYPE_AMPLITUDE,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_NAV_DATA,
    g_param_spec_object ("nav-data", "Navigation", "HyScanNavData with track value", HYSCAN_TYPE_NAV_DATA,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_ESTIMATOR,
    g_param_spec_object ("estimator", "Estimator", "HyScanDataEstimator", HYSCAN_TYPE_DATA_ESTIMATOR,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_quality_init (HyScanQuality *quality)
{
  quality->priv = hyscan_quality_get_instance_private (quality);
}

static void
hyscan_quality_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  HyScanQuality *quality = HYSCAN_QUALITY (object);
  HyScanQualityPrivate *priv = quality->priv;

  switch (prop_id)
    {
    case PROP_NAV_DATA:
      priv->nav_data = g_value_dup_object (value);
      break;
      
    case PROP_AMPLITUDE:
      priv->amplitude = g_value_dup_object (value);
      break;
      
    case PROP_ESTIMATOR:
      priv->estimator = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_quality_object_constructed (GObject *object)
{
  HyScanQuality *quality = HYSCAN_QUALITY (object);
  HyScanQualityPrivate *priv = quality->priv;

  G_OBJECT_CLASS (hyscan_quality_parent_class)->constructed (object);

  priv->projector = hyscan_projector_new (priv->amplitude);
  priv->buffer = g_array_new (FALSE, FALSE, sizeof (gdouble));
}

static void
hyscan_quality_object_finalize (GObject *object)
{
  HyScanQuality *quality = HYSCAN_QUALITY (object);
  HyScanQualityPrivate *priv = quality->priv;
  
  g_clear_object (&priv->amplitude);
  g_clear_object (&priv->nav_data);
  g_clear_object (&priv->projector);
  g_clear_object (&priv->estimator);
  g_array_free (priv->buffer, TRUE);

  G_OBJECT_CLASS (hyscan_quality_parent_class)->finalize (object);
}

/* Вариация курса в окрестности указанной точки. */
static gdouble
hyscan_quality_get_track_var (HyScanQuality *quality,
                              guint32        index)
{
  HyScanQualityPrivate *priv = quality->priv;
  GArray *buffer = priv->buffer;
  gint64 idx_time;
  guint32 n_points;
  guint32 first, last, lindex, rindex, j;
  gdouble angle_avg;
  HyScanDBFindStatus status;

  if (priv->amplitude == NULL)
    return -1.0;

  if (!hyscan_amplitude_get_size_time (priv->amplitude, index, &n_points, &idx_time))
    return -1.0;

  if (!hyscan_nav_data_get_range (priv->nav_data, &first, &last))
    return -1.0;

  status = hyscan_nav_data_find_data (priv->nav_data, idx_time - WINDOW_TIME_SPAN, &lindex, NULL, NULL, NULL);
  if (status == HYSCAN_DB_FIND_LESS)
    lindex = first;
  else if (status != HYSCAN_DB_FIND_OK)
    return -1.0;

  status = hyscan_nav_data_find_data (priv->nav_data, idx_time + WINDOW_TIME_SPAN, &rindex, NULL, NULL, NULL);
  if (status == HYSCAN_DB_FIND_GREATER)
    rindex = last;
  else if (status != HYSCAN_DB_FIND_OK)
    return -1.0;

  g_array_set_size (buffer, 0);
  for (j = lindex; j <= rindex; j++)
    {
      gdouble angle_j;
      gint64 time_j;

      if (!hyscan_nav_data_get (priv->nav_data, NULL, j, &time_j, &angle_j))
        continue;

      g_array_append_val (buffer, angle_j);
    }

  if (buffer->len < 3)
    return -1.0;

  angle_avg = hyscan_stats_avg_circular ((const gdouble *) buffer->data, buffer->len);

  return hyscan_stats_var_circular (angle_avg, (const gdouble *) buffer->data, buffer->len) / 180 * G_PI;
}

static gboolean
hyscan_quality_get_values_estimator (HyScanQuality *quality,
                                     guint32        index,
                                     const guint32 *counts,
                                     gdouble       *values,
                                     guint          n_values)
{
  HyScanQualityPrivate *priv = quality->priv;
  const guint32 *quality_values;
  guint32 n_points, i, j;
  guint32 max_quality;

  quality_values = hyscan_data_estimator_get_acust_quality (priv->estimator, index, &n_points);
  if (quality_values == NULL)
    return FALSE;

  max_quality = hyscan_data_estimator_get_max_quality (priv->estimator);
  for (i = 0; i < n_values; i++)
    {
      guint32 section_quality;
      guint32 first, last;

      first = i == 0 ? 0 : (counts[i - 1] + 1);
      last = counts[i];

      section_quality = 0;
      for (j = first; j <= last; j++)
        section_quality += quality_values[j];

      values[i] = (gdouble) section_quality / max_quality / (last - first);
    }

  return TRUE;
}

/**
 * hyscan_quality_new:
 * @amplitude: амплитудные данные
 * @ndata: навигационные данные с курсом
 *
 * Функция создаёт объект оценки качества
 *
 * Returns: (transfer full): объект оценки качества, для удаления g_object_unref().
 *
 */
HyScanQuality *
hyscan_quality_new (HyScanAmplitude *amplitude,
                    HyScanNavData   *nav_data)
{
  return g_object_new (HYSCAN_TYPE_QUALITY,
                       "amplitude", amplitude,
                       "nav-data", nav_data,
                       NULL);
}
/**
 * hyscan_quality_new_estimator:
 * @amplitude: амплитудные данные
 * @ndata: навигационные данные с курсом
 *
 * Функция создаёт объект оценки качества
 *
 * Returns: (transfer full): объект оценки качества, для удаления g_object_unref().
 *
 */
HyScanQuality *
hyscan_quality_new_estimator (HyScanDataEstimator *estimator)
{
  return g_object_new (HYSCAN_TYPE_QUALITY,
                       "estimator", estimator,
                       NULL);
}

/**
 * hyscan_quality_get_values:
 * @quality: объект оценки качества
 * @index: индекс считываемых амлитудных данных
 * @counts: (array length=n_values): средние значения качества, соответствующие каждому отрезку
 * @values: (out) (array length=n_values): номера отсчётов, соответствующие верхней границе каждого отрезка
 * @n_values: количество отрезков
 *
 * Функция определяет среднее значение качества на каждом из отрезков, ограниченных номерами отсчетов @values:
 * - values[0] - среднее значение для отсчётов c: c <= counts[0],
 * - values[i] - среднее значение для отсчётов c: counts[i-1] < c <= counts[i],
 *
 * Returns: %TRUE в случае успеха
 */
gboolean
hyscan_quality_get_values (HyScanQuality         *quality,
                           guint32                index,
                           const guint32         *counts,
                           gdouble               *values,
                           guint                  n_values)
{
  HyScanQualityPrivate *priv;
  guint i;
  gdouble angle_diff;
  gdouble mean_length;

  g_return_val_if_fail (HYSCAN_IS_QUALITY (quality), FALSE);

  priv = quality->priv;
  
  if (priv->estimator != NULL)
    return hyscan_quality_get_values_estimator (quality, index, counts, values, n_values);

  /* Вариация курса. */
  angle_diff = hyscan_quality_get_track_var (quality, index);
  if (angle_diff < 0)
    return FALSE;

  /* Дальность, в пределах которой считаем качество максимальным. */
  mean_length = fabs (MAX_DEVIATION_ALONG / tan (angle_diff));

  for (i = 0; i < n_values; i++)
    {
      gdouble across;

      hyscan_projector_count_to_coord (priv->projector, counts[i], &across, 0);
      values[i] = MIN (mean_length / across, 1.0);
    }

  return TRUE;
}

