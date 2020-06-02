#include "hyscan-data-estimator.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

enum
{
  PROP_O,
  PROP_SIGNAL,
  PROP_NOISE,
  PROP_NAVIG
};

struct _HyScanDataEstimatorPrivate
{
  HyScanAcousticData *signal_data;            /* Сигнал */
  HyScanAcousticData *noise_data;             /* Шум */
  HyScanNavData      *navig_data;             /* Навигация */
  guint32             signal_row_index;       /* Индекс строки сигнала */

  guint32            *quality_val;            /* Значения качества */
  guint32            *quality_ind;            /* Номера отсчетов, после которого значение качества изменилось */

  gboolean            convolve;               /* Оценивается сигнал до или после свертки */
  gfloat             *processed_signal;       /* Сигнал с удаленной просачкой */
  gfloat             *noise_std;              /* Значения СКО шума */
  guint32             noise_std_size;              /* Значения СКО шума */
  gfloat             *snr;                    /* Значения отношения сигнал/шум */
  guint32             smooth_window;          /* Количество усредняемых отсчетов сигнала */
  guint32             samples_window;         /* Количество усредняемых отсчетов шума */
  guint32             time_window;            /* Количество усредняемых выборок шума */
  guint32             max_navig_delay;        /* Максимально допустимый интервал выдачи навигацции */
  guint32             navig_mean_window;      /* Количество усредняемых временных интервалов выдачи навигации */
  guint32             min_quality;            /* Минимальное значение качества */
  guint32             max_quality;            /* Максимальное значение качества */
  gint                prev_noise_index;       /* Номер предыдущего индекса строки шума  */
  guint32             signal_size;            /* Количество отсчетов сигнала */
  GArray *source_quality;
};

static void    hyscan_data_estimator_set_property        (GObject             *object,
                                                          guint                prop_id,
                                                          const GValue        *value,
                                                          GParamSpec          *pspec);

static void    hyscan_data_estimator_object_constructed  (GObject             *object);
static void    hyscan_data_estimator_object_finalize     (GObject             *object);

static guint32 hyscan_data_estimator_get_leak_size       (HyScanDataEstimator *self);

static gfloat* hyscan_data_estimator_get_ampl_array      (HyScanDataEstimator *self,
                                                          guint32             *signal_row_size,
                                                          gint64              *signal_time);

static guint32 hyscan_data_estimator_cut_leak            (HyScanDataEstimator *self,
                                                          gfloat              *source_signal,
                                                          guint32              source_signal_size,
                                                          guint32              leak_size);

static void    hyscan_data_estimator_smooth_signal       (HyScanDataEstimator *self,
                                                          guint32              signal_size,
                                                          guint32              leak_size);

static void    hyscan_data_estimator_account_tvg         (HyScanAcousticData  *data,
                                                          guint32              signal_row_index,
                                                          gfloat              *source_signal,
                                                          guint32              signal_row_size);

static gint    hyscan_data_estimator_count_noise_std     (HyScanDataEstimator *self,
                                                          gint                 noise_row_index,
                                                          guint32              leak_size);

static void    hyscan_data_estimator_count_snr           (gfloat              *signal,
                                                          gfloat              *noise_std,
                                                          guint32              noise_std_row_size,
                                                          guint32              signal_size,
                                                          guint32              leak_size,
                                                          gfloat              *snr);

static gfloat  hyscan_data_estimator_find_max            (gfloat              *data,
                                                          guint32              signal_size,
                                                          guint32              leak_size);

static void    hyscan_data_estimator_count_acust_quality (HyScanDataEstimator *self,
                                                          gfloat               max_snr,
                                                          guint32              signal_row_size,
                                                          guint32              leak_size,
                                                          guint32             *quality);

static void   hyscan_data_estimator_unite_quality        (HyScanDataEstimator *self,
                                                          guint32             *quality,
                                                          guint32              signal_row_size,
                                                          guint32             *united_quality_size);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanDataEstimator, hyscan_data_estimator, G_TYPE_OBJECT);


static void
hyscan_data_estimator_class_init (HyScanDataEstimatorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_data_estimator_set_property;

  object_class->constructed = hyscan_data_estimator_object_constructed;
  object_class->finalize = hyscan_data_estimator_object_finalize;

  g_object_class_install_property (object_class, PROP_SIGNAL,
          g_param_spec_object ("signal", "signal-data", "HyScanAcousticData object", HYSCAN_TYPE_ACOUSTIC_DATA,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_NOISE,
          g_param_spec_object ("noise", "noise-data", "HyScanAcousticData object", HYSCAN_TYPE_ACOUSTIC_DATA,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_NAVIG,
          g_param_spec_object ("navigation", "navigation-data", "HyScanNavData object", HYSCAN_TYPE_NAV_DATA,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_data_estimator_init (HyScanDataEstimator *estimator)
{
  estimator->priv = hyscan_data_estimator_get_instance_private (estimator);
}

static void
hyscan_data_estimator_set_property (GObject      *object,
                                    guint        prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  HyScanDataEstimator        *data_estimator = HYSCAN_DATA_ESTIMATOR (object);
  HyScanDataEstimatorPrivate *priv           = data_estimator->priv;

  switch (prop_id)
    {
    case PROP_SIGNAL:
      priv->signal_data = g_value_dup_object (value);
      break;
    case PROP_NOISE:
      priv->noise_data  = g_value_dup_object (value);
      break;
    case PROP_NAVIG:
      priv->navig_data  = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);

      break;
    }
}

static void
hyscan_data_estimator_object_constructed (GObject *object)
{
  HyScanDataEstimator *data_estimator = HYSCAN_DATA_ESTIMATOR (object);
  HyScanDataEstimatorPrivate *priv = data_estimator->priv;

  priv->convolve         = TRUE;
  priv->smooth_window    = 10;
  priv->samples_window   = 100;
  priv->time_window      = 4;
  priv->min_quality      = 0;
  priv->max_quality      = 255;
  priv->max_navig_delay  = 10;
  priv->navig_mean_window = 10;

  priv->prev_noise_index = -1;
  priv->source_quality = g_array_new (FALSE, TRUE, sizeof (guint32));

  G_OBJECT_CLASS (hyscan_data_estimator_parent_class)->constructed (object);
}

static void
hyscan_data_estimator_object_finalize (GObject *object)
{
  HyScanDataEstimator* estimator   = HYSCAN_DATA_ESTIMATOR (object);
  HyScanDataEstimatorPrivate* priv = estimator->priv;

  // g_free (priv->processed_signal);
  // g_free (priv->snr);
  g_free (priv->noise_std);
  // g_free (priv->quality_val);
  // g_free (priv->quality_ind);
  g_array_free (priv->source_quality, TRUE);
  g_clear_object (&priv->signal_data);
  g_clear_object (&priv->noise_data);
  g_clear_object (&priv->navig_data);

  G_OBJECT_CLASS (hyscan_data_estimator_parent_class)->finalize (object);
}

/**
 * hyscan_data_estimator_new:
 *
 * Функция создаёт новый объект #HyScanDataEstimator.
 *
 * Returns: #HyScanDataEstimator. Для удаления #g_object_unref.
 */
HyScanDataEstimator*
hyscan_data_estimator_new (HyScanAcousticData *signal_data,
                           HyScanAcousticData *noise_data,
                           HyScanNavData      *navigation_data)
{
  HyScanDataEstimator* estimator = NULL;

  estimator = g_object_new (HYSCAN_TYPE_DATA_ESTIMATOR,
                            "signal",     signal_data,
                            "noise",      noise_data,
                            "navigation", navigation_data,
                            NULL);

  return estimator;
}

/* Функция определяет длительность просачки */
static guint32
hyscan_data_estimator_get_leak_size (HyScanDataEstimator *self)
{
  HyScanDataEstimatorPrivate *priv = self->priv;

  guint32 refer_signal_size;
  guint64 refer_signal_time;

  if (hyscan_acoustic_data_get_signal (priv->signal_data,
                                       priv->signal_row_index,
                                       &refer_signal_size,
                                       &refer_signal_time) != NULL)
    {
      return refer_signal_size;
    }
  else
    {
      return 0;
    }
}

/* Функция возвращает строку сигнала */
static gfloat*
hyscan_data_estimator_get_ampl_array (HyScanDataEstimator *self,
                                      guint32             *signal_row_size,
                                      gint64              *signal_time)
{
  HyScanDataEstimatorPrivate *priv = self->priv;

  gfloat*           signal_ampls;
  gfloat*           source_signal_ampls;

  signal_ampls = hyscan_acoustic_data_get_amplitude (priv->signal_data,
                                                     NULL,
                                                     priv->signal_row_index,
                                                     signal_row_size,
                                                     signal_time);
  return signal_ampls;
}

/* Функция вырезает из сигнала участок, где присутствует просачка */
static guint32
hyscan_data_estimator_cut_leak (HyScanDataEstimator *self,
                                gfloat*              source_signal,
                                guint32              source_signal_size,
                                guint32              leak_size)
{
  // если длительность просачки превышает длительность сигнала,
  //  то функция возвращает -1.
  if (leak_size > source_signal_size)
    {
      return -1;
    }
  else
    {
      HyScanDataEstimatorPrivate *priv = self->priv;
      memset (source_signal, 0, sizeof (gfloat) * leak_size);
      return 0;
    }
}

/* Функция осуществляет сглаживание сигнала */
static void
hyscan_data_estimator_smooth_signal (HyScanDataEstimator *self,
                                      guint32             signal_size,
                                      guint32             leak_size)
{
  HyScanDataEstimatorPrivate *priv = self->priv;

  guint i;
  gfloat temp_sum = 0.0;
  gfloat temp_mean = 0.0;

  // сглаживание сигнала путем усреднения в окне размером priv->smooth_window
  for (i = leak_size; i < leak_size + priv->smooth_window; i++)
    {
      temp_sum += priv->processed_signal[i];
    }

  for (i = leak_size ; i < signal_size - (priv->smooth_window + 1); i++)
    {
      temp_mean = temp_sum / priv->smooth_window;
      temp_sum = temp_sum - priv->processed_signal[i] + priv->processed_signal[i + priv->smooth_window];
      priv->processed_signal[i] = temp_mean;
    }
}

/* Функция ищет максимальное значение массива */
static gfloat
hyscan_data_estimator_find_max (gfloat  *data,
                                guint32  signal_size,
                                guint32  leak_size)
{
  guint i, max_ind;
  gfloat max;

  max = data[leak_size];

  for (i = leak_size + 1; i < signal_size; i++)
    {
      if (max < data[i])
        {
          max = data[i];
          max_ind = i;
        }
    }

  return max;
}

/* Функция определяет СКО шума */
static gint
hyscan_data_estimator_count_noise_std (HyScanDataEstimator *self,
                                       gint                 noise_row_index,
                                       guint32              leak_size)
{
  HyScanDataEstimatorPrivate *priv = self->priv;

  gfloat* gained_noise;
  gfloat* noise_ampls;

  if (priv->prev_noise_index == noise_row_index)
    {
      return 0;
    }
  else
    {
      priv->prev_noise_index = noise_row_index;
      guint32 noise_row_size;

      guint i;
      guint j;
      gfloat temp_sum = 0.0;
      guint start_index = 0;

      if (priv->time_window < noise_row_index)
        {
          start_index = noise_row_index - priv->time_window + 1;
        }

      hyscan_acoustic_data_get_size_time (priv->noise_data, 0, &noise_row_size, NULL);

      g_clear_pointer (&priv->noise_std, g_free);
      priv->noise_std_size = noise_row_size;
      priv->noise_std = g_new0 (gfloat, priv->noise_std_size);

      noise_ampls = g_malloc (sizeof(gfloat) * priv->noise_std_size);

      for (j = start_index; j <= noise_row_index; j++)
        {
          guint32 amp_row_size;

          gained_noise = hyscan_acoustic_data_get_amplitude (priv->noise_data,
                                                             NULL,
                                                             j,
                                                             &amp_row_size,
                                                             NULL);

          /* Находим количество отсчётов, которые есть и в шуме, и в сигнале. */
          noise_row_size = MIN (noise_row_size, amp_row_size);
          priv->noise_std_size = noise_row_size;

          memcpy (noise_ampls, gained_noise, sizeof(gfloat) * noise_row_size);

          /* Определение исходного уровня шума (без ВАРУ) */
          hyscan_data_estimator_account_tvg (priv->noise_data,
                                             j,
                                             noise_ampls,
                                             noise_row_size);

          temp_sum = 0.0;
          for (i = leak_size; i < leak_size + priv->samples_window; i++)
            {
              temp_sum += noise_ampls[i];
            }

          for (i = leak_size; i < noise_row_size - (priv->samples_window + 1); i++)
            {
              priv->noise_std[i] += temp_sum;
              temp_sum = temp_sum - noise_ampls[i] + noise_ampls[i + priv->samples_window];
            }
        }

      /* Т.к рассматриваются амплитуды шумовой выборки, то распределение - рэлеевское, mean= std* (pi/2)^0.5  */
      for (i = leak_size; i < noise_row_size - (priv->samples_window + 1); i++)
        {
          priv->noise_std[i] = priv->noise_std[i] / (priv->samples_window * (noise_row_index - start_index + 1));
          priv->noise_std[i] /= 1.2533;
        }
    }

  g_free (noise_ampls);
  return 0;
}

/* Функция определяет отношение синал/шум */
static void
hyscan_data_estimator_count_snr (gfloat *signal,
                                 gfloat *noise_std,
                                 guint32 noise_std_row_size,
                                 guint32 signal_row_size,
                                 guint32 leak_size,
                                 gfloat *result)
{
  gint i;
  gfloat koef = 1.41421;

  /* Отношение с/ш определяется как 20*lg(A/(std* 2^0.5)), где std - ско шума, А - амплитуда сигнала */
  for (i = leak_size; i < signal_row_size; i++)
    {
      if (i < noise_std_row_size && noise_std[i] != 0.0)
        {
          result[i] =  20 * log10f (signal[i] / (koef * noise_std[i]));
        }
      else
        {
          result[i] = 0;
        }
    }
}

/* Функция определяет качество для каждого отсчета сигнала */
static void
hyscan_data_estimator_count_acust_quality (HyScanDataEstimator* self,
                                           gfloat               max_snr,
                                           guint32              signal_row_size,
                                           guint32              leak_size,
                                           guint32*             quality)
{
  HyScanDataEstimatorPrivate* priv = self->priv;

  /* Определение коэффициента пересчета с/ш в качество */
  gfloat snr_to_quality_coef = (gfloat)(priv->max_quality - priv->min_quality) / max_snr;
  gint i;
  for (i = 0; i < leak_size; i++)
    quality[i] = 0;

  for (i = leak_size; i < signal_row_size; i++)
    {
      if (priv->snr[i] <= max_snr && priv->snr[i] >= 0)
        {
          quality[i] = priv->snr[i] * snr_to_quality_coef;
        }
      if (priv->snr[i] > max_snr)
        {
          quality[i] = priv->max_quality;
        }
      if (priv->snr[i] < 0)
        {
          quality[i] = 0;
        }
    }
}

/* Функция вычисляет амплитуду исходного сигнала (без ВАРУ) */
static void
hyscan_data_estimator_account_tvg (HyScanAcousticData* data,
                                   guint32             signal_row_index,
                                   gfloat*             source_signal,
                                   guint32             signal_row_size)
{
  const gfloat* tvg_koef;
  guint32 tvg_row_size;
  gint64 tvg_time;
  guint32 i;

  tvg_koef = hyscan_acoustic_data_get_tvg (data,
                                           signal_row_index,
                                           &tvg_row_size,
                                           &tvg_time);

  signal_row_size = MIN (signal_row_size, tvg_row_size);
  for (i = 0; i < signal_row_size; i++)
    source_signal[i] = source_signal[i] / tvg_koef[i];

  for (; i < signal_row_size; i++)
    source_signal[i] = 0;
}

/* Функция объединяет отсчеты с одинаковым значением качества сигнала */
static void
hyscan_data_estimator_unite_quality (HyScanDataEstimator* self,
                                     guint32*             quality,
                                     guint32              signal_row_size,
                                     guint32*             united_quality_size)
{
  HyScanDataEstimatorPrivate* priv = self->priv;

  gint counter;
  gint i;

  counter = 0;

  for (i = 0; i < signal_row_size - 1; i++)
    {
      priv->quality_ind[counter] = i;
      priv->quality_val[counter] = quality[i];

      if (quality[i] != quality[i+1])
        {
          counter++;
          priv->quality_ind[counter] = i + 1;
          priv->quality_val[counter] = quality[i + 1];
        }
    }

  *united_quality_size = counter +1;
}

/**
 * hyscan_data_estimator_get_quality:
 * @data: указатель на #HyScanDataEstimator
 * @row_index: индекс считываемых данныx
 * @quality: (out): значения качества данныx для отсчетов дальности одной строки
 * @indexes: (out): номера отсчетов по дальности
 * @n_points: (out): число значений качества данных
 *
 * Функция определяет качество данных по отношению
 * сигнал/шум.
 *
 * Returns: Статус определения качества данных
 */
const guint32 *
hyscan_data_estimator_get_acust_quality (HyScanDataEstimator *self,
                                         guint32              row_index,
                                         guint32             *n_points)
{
  HyScanDataEstimatorPrivate* priv = self->priv;

  /*Обрабатывается амплитудный сигнал после сжатия по дальности*/
  // hyscan_acoustic_data_set_convolve (priv->signal_data, priv->convolve, 1.0);
  // hyscan_acoustic_data_set_convolve (priv->noise_data, priv->convolve, 1.0);
  priv->signal_row_index = row_index;

  /* Определение длительности просачки*/
  guint32 leak_size = hyscan_data_estimator_get_leak_size (self);

  guint32 source_signal_size;
  gint64 signal_time;
  gfloat* gained_signal = hyscan_data_estimator_get_ampl_array (self,
                                                                &source_signal_size,
                                                                &signal_time);
  priv->signal_size = source_signal_size;

  gfloat max_snr;

  guint32 left_index, right_index;
  gint64  left_time, right_time;

  gboolean data_exists;

  gfloat* smooth_snr = NULL;
  gfloat* source_signal = NULL;

  source_signal = g_malloc (sizeof(gfloat) * source_signal_size);
  memcpy (source_signal, gained_signal, sizeof(gfloat) * source_signal_size);

  // priv->quality_val = g_new0 (guint32, source_signal_size);
  // priv->quality_ind = g_new0 (guint32, source_signal_size);

  /* Если есть информация о параметрах использованной системы ВАРУ, то вычисляем исходный сигнал */
  if (hyscan_acoustic_data_has_tvg (priv->signal_data))
    {
      hyscan_data_estimator_account_tvg (priv->signal_data,
                                         priv->signal_row_index,
                                         source_signal,
                                         source_signal_size);
    }

  /* Убираем просачку */
  hyscan_data_estimator_cut_leak (self,
                                  source_signal,
                                  source_signal_size,
                                  leak_size);

  priv->processed_signal = g_malloc (sizeof (gfloat) * source_signal_size);
  memcpy (priv->processed_signal, source_signal, sizeof(gfloat) * source_signal_size);

  hyscan_data_estimator_smooth_signal (self,
                                       source_signal_size,
                                       leak_size);

  if (priv->samples_window == 0 || priv->time_window == 0)
    {
      return NULL;
    }

  /* Рассчитываем СКО шума */
  data_exists = hyscan_acoustic_data_find_data (priv->noise_data,
                                                signal_time,
                                                &left_index,
                                                &right_index,
                                                &left_time,
                                                &right_time);
  if (data_exists == HYSCAN_DB_FIND_OK)
    {
      hyscan_data_estimator_count_noise_std (self,
                                             left_index,
                                             leak_size);
    }
  else
    {
      goto exit;
    }

  priv->snr = g_new0 (gfloat, source_signal_size);
  smooth_snr = g_new0 (gfloat, source_signal_size);

  /* Определяем отношение сигнал/шум */
  hyscan_data_estimator_count_snr (source_signal,
                                   priv->noise_std,
                                   priv->noise_std_size,
                                   source_signal_size,
                                   leak_size,
                                   priv->snr);

  hyscan_data_estimator_count_snr (priv->processed_signal,
                                   priv->noise_std,
                                   priv->noise_std_size,
                                   source_signal_size,
                                   leak_size,
                                   smooth_snr);

  max_snr = hyscan_data_estimator_find_max (smooth_snr, source_signal_size, leak_size);

  g_array_set_size (priv->source_quality, source_signal_size);

  /* Определяем качество локационных данных по отношению/шум  для каждого отсчета дальности */
  hyscan_data_estimator_count_acust_quality (self, max_snr, source_signal_size, leak_size,
                                             (guint32 *) priv->source_quality->data);

  /* Объединяем метки с одинаковым качеством */
  // hyscan_data_estimator_unite_quality (self,
  //                                      source_quality,
  //                                      source_signal_size,
  //                                      n_points);

exit:
  g_free (source_signal);
  g_clear_pointer (&priv->processed_signal, g_free);
  g_clear_pointer (&priv->snr, g_free);
  g_free (smooth_snr);

  *n_points = priv->source_quality->len;

  return (guint32 *) priv->source_quality->data;
}

void
hyscan_data_estimator_set_max_quality (HyScanDataEstimator *self,
                                       guint32              max_quality)
{
  g_return_if_fail (HYSCAN_IS_DATA_ESTIMATOR (self));

  self->priv->max_quality = max_quality;
}

guint32
hyscan_data_estimator_get_max_quality (HyScanDataEstimator *self)
{
  g_return_val_if_fail (HYSCAN_IS_DATA_ESTIMATOR (self), 0);

  return self->priv->max_quality;
}

/**
 * hyscan_data_estimator_get_navig_quality:
 * @self: указатель на #HyScanDataEstimator
 * @row_index: индекс локационных данныx
 * @quality: (out): качество данныx
 *
 * Функция определяет качество локационных данных на
 * основе анализа периодичности выдачи навигационных
 * данных.
 *
 * Returns: Статус определения качества данных
 */
gint
hyscan_data_estimator_get_navig_quality (HyScanDataEstimator *self,
                                         gint64               signal_time,
                                         guint32             *quality)
{
  HyScanDataEstimatorPrivate *priv = self->priv;

  guint32 source_signal_size;

  guint32 lindex, rindex;
  gint64  ltime, rtime;
  gdouble value;

  gint64 last_interval;
  gint64 mean_interval = 0;

  gfloat delay_to_quality_coef;
  gint quality_estim;

  gint64 nav_time, prev_nav_time;

  gboolean data_exists;

  data_exists = hyscan_nav_data_find_data (priv->navig_data,
                                           signal_time,
                                           &lindex,
                                           &rindex,
                                           &ltime,
                                           &rtime);
  if (data_exists == HYSCAN_DB_FIND_OK)
    {
      if (lindex > priv->navig_mean_window)
        {          
          guint i;

          hyscan_nav_data_get (priv->navig_data, NULL, 0, &prev_nav_time, &value);

          /* Определение среденего интервала выдачи навигационных данных */
          for (i = 1; i < priv->navig_mean_window + 1; i++)
            {
              hyscan_nav_data_get (priv->navig_data, NULL, i, &nav_time, &value);
              mean_interval += nav_time - prev_nav_time;
              prev_nav_time = nav_time;
            }

          mean_interval /= priv->navig_mean_window;
          last_interval = signal_time - ltime;

          /* Определение качества данных */
          delay_to_quality_coef = (gfloat)(priv->max_quality - priv->min_quality) / (priv->max_navig_delay * mean_interval);
          quality_estim = priv->max_quality - delay_to_quality_coef * last_interval;

          if (quality_estim < 0)
            {
              *quality = 0;
            }
          else
            {
              *quality = quality_estim;
            }

          return 0;
        }
      else
        {
          *quality = 0;
          return 0;
        }
    }
  else
    {
      return -1;
    }
}
