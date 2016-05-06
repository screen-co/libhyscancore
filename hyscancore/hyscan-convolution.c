/*
 * \file hyscan-convolution.c
 *
 * \brief Исходный файл класса свёртки данных
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2015
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-convolution.h"

#include <math.h>
#include <string.h>
#include "pffft.h"

#ifdef HYSCAN_CORE_USE_OPENMP
#include <omp.h>
#endif

/* Внутренние данные объекта. */
struct _HyScanConvolutionPrivate
{
  HyScanComplexFloat          *ibuff;          /* Буфер для обработки данных. */
  HyScanComplexFloat          *obuff;          /* Буфер для обработки данных. */
  gint32                       max_points;     /* Максимальное число точек помещающихся в буферах. */

  PFFFT_Setup                 *fft;            /* Коэффициенты преобразования Фурье. */
  gint32                       fft_size;       /* Размер преобразования Фурье. */
  gfloat                       fft_scale;      /* Коэффициент масштабирования свёртки. */
  HyScanComplexFloat          *fft_image;      /* Образец сигнала для свёртки. */
};

static void    hyscan_convolution_object_finalize      (GObject       *object);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanConvolution, hyscan_convolution, G_TYPE_OBJECT);

static void
hyscan_convolution_class_init (HyScanConvolutionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS( klass );

  object_class->finalize = hyscan_convolution_object_finalize;
}

static void
hyscan_convolution_init (HyScanConvolution *convolution)
{
  convolution->priv = hyscan_convolution_get_instance_private (convolution);
}

static void
hyscan_convolution_object_finalize (GObject *object)
{
  HyScanConvolution *convolution = HYSCAN_CONVOLUTION (object);
  HyScanConvolutionPrivate *priv = convolution->priv;

  pffft_aligned_free (priv->ibuff);
  pffft_aligned_free (priv->obuff);

  pffft_aligned_free (priv->fft_image);
  pffft_destroy_setup (priv->fft);

  G_OBJECT_CLASS (hyscan_convolution_parent_class)->finalize (object);
}

/* Функция создаёт новый объект HyScanConvolution. */
HyScanConvolution *
hyscan_convolution_new (void)
{
  return g_object_new (HYSCAN_TYPE_CONVOLUTION, NULL);
}

/* Функция задаёт образец сигнала для свёртки. */
gboolean
hyscan_convolution_set_image (HyScanConvolution  *convolution,
                              HyScanComplexFloat *image,
                              gint32              n_points)
{
  HyScanConvolutionPrivate *priv;

  HyScanComplexFloat *image_buff;

  gint32 conv_size = 2 * n_points;
  gint32 opt_size = 0;

  gint32 i, j, k;

  g_return_val_if_fail (HYSCAN_IS_CONVOLUTION (convolution), FALSE);

  priv = convolution->priv;

  /* Отменяем свёртку с текущим сигналом. */
  g_clear_pointer (&priv->fft, pffft_destroy_setup);
  g_clear_pointer (&priv->fft_image, pffft_aligned_free);
  priv->fft_size = 2;

  /* Пользователь отменил свёртку. */
  if (image == NULL)
    return TRUE;

  /* Ищем оптимальный размер свёртки для библиотеки pffft_new_setup (см. pffft.h). */
  opt_size = G_MAXINT32;
  for (i = 32; i <= 512; i *= 2)
    for (j = 1; j <= 243; j *= 3)
      for (k = 1; k <= 5; k *= 5)
        {
          if (i * j * k < conv_size)
            continue;
          if (ABS (i * j * k - conv_size) < ABS (opt_size - conv_size))
            opt_size = i * j * k;
        }

  if (opt_size == G_MAXINT32)
    {
      g_critical ("hyscan_convolution_set_image: can't find optimal fft size");
      return FALSE;
    }

  priv->fft_size = opt_size;

  /* Коэффициент масштабирования свёртки. */
  priv->fft_scale = 1.0 / ((gfloat) priv->fft_size * (gfloat) n_points);

  /* Параметры преобразования Фурье. */
  priv->fft = pffft_new_setup (priv->fft_size, PFFFT_COMPLEX);
  if (!priv->fft)
    {
      g_critical ("hyscan_convolution_set_image: can't setup fft");
      return FALSE;
    }

  /* Копируем образец сигнала. */
  priv->fft_image = pffft_aligned_malloc (priv->fft_size * sizeof(HyScanComplexFloat));
  memset (priv->fft_image, 0, priv->fft_size * sizeof(HyScanComplexFloat));
  memcpy (priv->fft_image, image, n_points * sizeof(HyScanComplexFloat));

  /* Подготавливаем образец к свёртке и делаем его комплексно сопряжённым. */
  image_buff = pffft_aligned_malloc (priv->fft_size * sizeof(HyScanComplexFloat));

  pffft_transform_ordered (priv->fft,
                           (const gfloat*) priv->fft_image,
                           (gfloat*) image_buff,
                           NULL,
                           PFFFT_FORWARD);

  for (i = 0; i < priv->fft_size; i++)
    image_buff[i].im = -image_buff[i].im;

  pffft_zreorder (priv->fft,
                  (const gfloat*) image_buff,
                  (gfloat*) priv->fft_image,
                  PFFFT_BACKWARD);

  pffft_aligned_free (image_buff);

  return TRUE;
}

/* Функция выполняет свертку данных.
 * Свертка выполняется блоками по fft_size элементов, при этом каждый следующий блок смещен относительно
 * предыдущего на (fft_size / 2) элементов. Входные данные находятся в ibuff, где над ними производится
 * прямое преобразование Фурье с сохранением результата в obuff, но уже без перекрытия, т.е. с шагом fft_size.
 * Затем производится перемножение ("свертка") с нужным образцом сигнала и обратное преобразование Фурье
 * в ibuff. Таким образом в ibuff оказываются необходимые данные, разбитые на некоторое число блоков,
 * в каждом из которых нам нужны только первые (fft_size / 2) элементов. Так как операции над блоками
 * происходят независимо друг от друга это процесс можно выполнять параллельно, что и производится
 * за счет использования библиотеки OpenMP. */
gboolean
hyscan_convolution_convolve (HyScanConvolution  *convolution,
                             HyScanComplexFloat *data,
                             gint32              n_points)
{
  HyScanConvolutionPrivate *priv;

  gint32 i;
  gint32 n_fft;
  gint32 full_size;
  gint32 half_size;

  g_return_val_if_fail (HYSCAN_IS_CONVOLUTION (convolution), FALSE);

  priv = convolution->priv;

  /* Свёртка невозможна. */
  if (priv->fft == NULL || priv->fft_image == NULL )
    return FALSE;

  full_size = priv->fft_size;
  half_size = priv->fft_size / 2;

  /* Число блоков преобразования Фурье над одной строкой. */
  n_fft = (n_points / half_size);
  if (n_points % half_size)
    n_fft += 1;

  /* Определяем размер буферов и изменяем их размер при необходимости. */
  if (n_fft * full_size > priv->max_points)
    {
      priv->max_points = n_fft * full_size;
      pffft_aligned_free (priv->ibuff);
      pffft_aligned_free (priv->obuff);
      priv->ibuff = pffft_aligned_malloc (priv->max_points * sizeof(HyScanComplexFloat));
      priv->obuff = pffft_aligned_malloc (priv->max_points * sizeof(HyScanComplexFloat));
    }

  /* Копируем данные во входной буфер. */
  memcpy (priv->ibuff,
          data,
          n_points * sizeof(HyScanComplexFloat));

  /* Зануляем конец буфера по границе half_size. */
  memset (priv->ibuff + n_points,
          0,
          ((n_fft + 1) * half_size - n_points) * sizeof(HyScanComplexFloat));

  /* Прямое преобразование Фурье. */
#ifdef HYSCAN_CORE_USE_OPENMP
#pragma omp parallel for
#endif
  for (i = 0; i < n_fft; i++)
    {
      pffft_transform (priv->fft,
                       (const gfloat*) (priv->ibuff + (i * half_size)),
                       (gfloat*) (priv->obuff + (i * full_size)),
                       NULL,
                       PFFFT_FORWARD);
    }

  /* Свёртка и обратное преобразование Фурье. */
#ifdef HYSCAN_CORE_USE_OPENMP
#pragma omp parallel for
#endif
  for (i = 0; i < n_fft; i++)
    {
      gint32 offset = i * full_size;
      gint32 used_size = MIN( ( n_points - i * half_size ), half_size );

      /* Обнуляем выходной буфер, т.к. функция zconvolve_accumulate добавляет полученный результат
         к значениям в этом буфере (нам это не нужно) ...*/
      memset (priv->ibuff + offset,
              0,
              full_size * sizeof(HyScanComplexFloat));

      /* ... и выполняем свёртку. */
      pffft_zconvolve_accumulate (priv->fft,
                                  (const gfloat*) (priv->obuff + offset),
                                  (const gfloat*) priv->fft_image,
                                  (gfloat*) (priv->ibuff + offset),
                                  priv->fft_scale);

      /* Выполняем обратное преобразование Фурье. */
      pffft_zreorder (priv->fft,
                      (gfloat*) (priv->ibuff + offset),
                      (gfloat*) (priv->obuff + offset),
                      PFFFT_FORWARD);

      pffft_transform_ordered (priv->fft,
                               (gfloat*) (priv->obuff + offset),
                               (gfloat*) (priv->ibuff + offset),
                               NULL,
                               PFFFT_BACKWARD);

      /* Копируем результат обратно в буфер пользователя. */
      memcpy (data + offset / 2,
              priv->ibuff + offset,
              used_size * sizeof (HyScanComplexFloat));
    }

  return TRUE;
}
