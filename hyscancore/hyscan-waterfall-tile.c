 /*
  * \file hyscan-waterfall-tile.c
  *
  * \brief Исходный файл класса генерации тайлов.
  * \author Dmitriev Alexander (m1n7@yandex.ru)
  * \date 2017
  * \license Проприетарная лицензия ООО "Экран"
  *
  * Данный класс реализует генерацию тайлов в режиме водопад.
  *
  * Предполагается, что функция генерации будет вызываться неоднократно.
  * При этом можно подобрать масштабы таким образом, чтобы размер тайла в пикселях
  * оставался постоянным. Это позволяет избежать лишних реаллокаций.
  *
  * 1. Из БД данные записываются в data0/data_add0. При каждой записи в ячейку data0/data_add0
  *    инкрементируется соответствующий элемент weight/weight_add. Если в текущей строке
  *    оказались данные, то это отражается в mark (записывается индекс текущей строки).
  *    Если в текущей строке данные как бы есть, но они находятся ЗА пределами акустической строки,
  *    то записывается отрицательный индекс текущей строки.
  * 2. Строки интерполируются. Для оптимизации используется mark, по которому можно определить,
  *    в каких строках данных нет.
  * 3. Строки фильтруются и складываются в data1/data_add1.
  * 4. Производится интерполяция методом ближайшего соседа по кадру. При этом для каждой интерполированной
  *    строки заносится информация в mark с индексом строки, из которой она получена.
  * 5. Производится фильтрация усреднением. Результат складывается в data2. Для оптимизации опять же
  *    используется mark, по которому можно определить абсолютно идентичные строки.
  *
  * data0 и data1 хранят строки, попавшие в немного расширенные границы тайла.
  * data_add0 и data_add1 хранят строки, находящиеся сразу же за границами расширенного тайла.
  * Такая структура нужна для сшивания на крупных масштабах.
  *
  * 1. hyscan_waterfall_tile_prepare
  *    В этой функции выполняется предварительная подготовка.
  *      - Если вертикальные координаты отрицательны, то будет сразу сгенерирован пустой тайл
  *      - Если в КД есть только одна строка данных, будет сразу
  *        сгенерирован пустой тайл, но перегенерирован в дальнейшем.
  *    Для корректного сшивания используются сразу 2 техники:
  *      - К кадру добавляется по 5% по оси движения.
  *      - Берется по 1 строке за пределами кадра. Это нужно для сшивания на крупных масштабах.
  *    Если число строк, попавших в кадр, окажется меньше числа строк выходного кадра, величина
  *    апсемплинга будет принудительно выставлена в 1, чтобы снизить нагрузку и время генерации.
  *
  * 2. hyscan_waterfall_tile_reset
  *    В этой функции очищаются буферы.
  *
  * 3. hyscan_waterfall_tile_fill
  *    В этой функции заполняются строки массива и две дополнительные строки. Если в одну строку
  *    попадает больше, чем одна строка, они просто усредняются.
  *    На основании количества и среднего шага между строками вычисляется оптимальный размер фильтра.
  *
  * 4. hyscan_waterfall_tile_string_helper
  *    Внутри строк проводится интерполяция методом ближайшего соседа. Строки фильтруются.
  *
  * 5. hyscan_waterfall_tile_interpolate_frame
  *    Интерполяция внутри кадра методом ближайшего соседа.
  *
  * 6. hyscan_waterfall_tile_filter
  *    Фильтрация кадра производится простым усреднением. Однако если оптимальный размер фильтра
  *    превышает величину апсемплинга, то фактический размер фильтра будет меньше. Конкретный закон
  *    этого уменьшения находится перед вызовом функции hyscan_waterfall_tile_filter.
  *
  * 7. hyscan_waterfall_tile_compose_frame составляет выходной кадр и никакой сложной логики не
  *    реализует. Если функция видит отрицательные горизонтальные координаты, она отражает кадр по
  *    горизонтали.
  */

#include "hyscan-waterfall-tile.h"
#include <string.h>
#include <math.h>

#define SWAP(x, y, type) do { type tmp; tmp = x; x = y; y = tmp; } while (FALSE)
#define FILTER_THRESHOLD (4)
#define TRANSPARENT (-1.0)

typedef struct
{
  guint32 dc_lindex;           /* Первый индекс КД. */
  guint32 dc_rindex;           /* Последний индекс КД. */
  gint64  dc_ltime;            /* Время первого индекса. */

  guint32 frame_lindex;        /* Первый индекс, попавший в кадр. */
  guint32 frame_rindex;        /* Последний индекс, попавший в кадр. */
  guint32 frame_lindex2;       /* Первый дополнительный индекс (перед кадром). */
  guint32 frame_rindex2;       /* Второй дополнительный индекс (после кадра). */
  gint64  frame_lindex2_time;  /* Время 1го доп. индекса. */
  gint64  frame_rindex2_time;  /* Время 2го доп. индекса. */

  gfloat  *data0;              /* Основной кадр: строки из БД и интерполяция. */
  gfloat  *data1;              /* Основной кадр: фильтрованные строки и интерполяция по кадру. */
  gfloat  *data2;              /* Основной кадр: фильтрованный кадр. */
  gfloat  *weight;             /* Основной кадр: веса (количество точек, попавших в пиксель). */
  gfloat  *mark;               /* Основной кадр: индексы акустических строк, попавших в эту строку кадра. */

  gfloat  *data_add0;          /* Доп. строки: из БД и интерполяция. */
  gfloat  *data_add1;          /* Доп. строки: фильтрованные. */
  gfloat  *weight_add;         /* Доп. строки: веса. */

  gint     w;                 /* Ширина (требуемая). */
  gint     h;                 /* Высота (требуемая). */
  gint     real_w;            /* Ширина (фактическая). */
  gint     real_h;            /* Высота (фактическая). */

  gfloat   step;               /* Шаг между строками кадра. */
  gint32   start_dist;         /* Дистанция первой строки кадра. */
  gint     filter_opt;         /* Оптимальный размер фильтра. */
} HyScanWaterfallTileParams;

struct _HyScanWaterfallTilePrivate
{
  HyScanAmplitude          *dc;                 /* Канал данных. */
  HyScanDepthometer        *depth;              /* Объект обработки навигационных данных. */
  gfloat                    ship_speed;         /* Скорость движения. */
  gfloat                    sound_speed;        /* Скорость звука. */

  /* Параметры генерации*/
  HyScanWaterfallTileParams params;             /* Внутренние переменные для генерации тайла. */
  HyScanTile                requested_tile;     /* Параметры запрошенного тайла в чистом виде. */
  HyScanTile                tile;               /* Параметры запрошенного тайла с учетом знаков. */

  /* Управляющие переменые. */
  gint                      generator_busy;     /* Идет генерация тайла. */
  gint                      generator_term;     /* Прекратить генерацию тайла. */
};

static void             hyscan_waterfall_tile_object_constructed        (GObject                    *object);
static void             hyscan_waterfall_tile_object_finalize           (GObject                    *object);

static gboolean         hyscan_waterfall_tile_prepare                   (HyScanWaterfallTilePrivate *priv,
                                                                         gint                       *upsample,
                                                                         gfloat                      step,
                                                                         gboolean                   *regenerate);
static void             hyscan_waterfall_tile_reset                     (HyScanWaterfallTileParams  *params);
static gboolean         hyscan_waterfall_tile_fill                      (HyScanWaterfallTilePrivate *priv,
                                                                         gboolean                   *have_data);
static gboolean         hyscan_waterfall_tile_make_string               (HyScanWaterfallTilePrivate *priv,
                                                                         gfloat                     *data,
                                                                         guint32                     data_size,
                                                                         gfloat                     *weight,
                                                                         const gfloat               *input,
                                                                         guint32                     input_size,
                                                                         gfloat                      step,
                                                                         HyScanTileFlags             flags,
                                                                         gfloat                      dfreq,
                                                                         gfloat                      depth);
static void             hyscan_waterfall_tile_string_helper             (HyScanWaterfallTilePrivate *params,
                                                                         gint                        filter);
static void             hyscan_waterfall_tile_interpolate_string        (gfloat                     *string,
                                                                         gfloat                     *weight,
                                                                         gint                        size);
static void             hyscan_waterfall_tile_interpolate_2             (HyScanWaterfallTileParams  *params,
                                                                         gfloat                     *left,
                                                                         gfloat                     *right,
                                                                         gint                        ileft,
                                                                         gint                        iright,
                                                                         gfloat                      lmark,
                                                                         gfloat                      rmark,
                                                                         gint32                      cleft,
                                                                         gint32                      cright);
static gboolean         hyscan_waterfall_tile_interpolate_frame         (HyScanWaterfallTilePrivate *priv);
static gboolean         hyscan_waterfall_tile_filter_string             (gint                        size,
                                                                         gfloat                     *source,
                                                                         gfloat                     *dest,
                                                                         gint                        width);
static gboolean         hyscan_waterfall_tile_filter_frame              (HyScanWaterfallTilePrivate *priv,
                                                                         gint                        filter_size,
                                                                         gfloat                     *source,
                                                                         gfloat                     *mark,
                                                                         gfloat                     *dest,
                                                                         gint                        width,
                                                                         gint                        height);
static gfloat          *hyscan_waterfall_tile_compose_frame             (HyScanWaterfallTilePrivate *priv,
                                                                         gfloat                     *data,
                                                                         gint                        data_w,
                                                                         gint                        data_h,
                                                                         gint                        frame_height,
                                                                         gint                        frame_width,
                                                                         gfloat                      step,
                                                                         gint                        upsample,
                                                                         gfloat                      up_start,
                                                                         gfloat                      up_step);

static void derivativate (gfloat*, guint32);
static gfloat * pf_get_values
(HyScanAmplitude *, guint32 i, guint32 l, guint32 r, guint32* n_vals, gint64 *time);


G_DEFINE_TYPE_WITH_PRIVATE (HyScanWaterfallTile, hyscan_waterfall_tile, G_TYPE_OBJECT);

static void
hyscan_waterfall_tile_class_init (HyScanWaterfallTileClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = hyscan_waterfall_tile_object_constructed;
  object_class->finalize = hyscan_waterfall_tile_object_finalize;
}

static void
hyscan_waterfall_tile_init (HyScanWaterfallTile *waterfall)
{
  waterfall->priv = hyscan_waterfall_tile_get_instance_private (waterfall);
}

static void
hyscan_waterfall_tile_object_constructed (GObject *object)
{
  HyScanWaterfallTile *waterfall = HYSCAN_WATERFALL_TILE (object);
  HyScanWaterfallTilePrivate *priv = waterfall->priv;

  /* По умолчанию звук 1500 м/с, судно 1 м/с. И в миллиметры! */
  priv->ship_speed = 1500.0 * 1000.0;
  priv->sound_speed = 1.0 * 1000.0;
}

static void
hyscan_waterfall_tile_object_finalize (GObject *object)
{
  HyScanWaterfallTile *waterfall = HYSCAN_WATERFALL_TILE (object);
  HyScanWaterfallTilePrivate *priv = waterfall->priv;

  while (g_atomic_int_get (&priv->generator_busy) != 0)
    g_atomic_int_set (&priv->generator_term, 1);

  /* Освобождаем память. */
  g_free (priv->params.mark);
  g_free (priv->params.data0);
  g_free (priv->params.data1);
  g_free (priv->params.data2);
  g_free (priv->params.data_add0);
  g_free (priv->params.data_add1);
  g_free (priv->params.weight);
  g_free (priv->params.weight_add);

  g_clear_object (&priv->depth);

  /* Вызываем деструктор родительского класса. */
  G_OBJECT_CLASS (hyscan_waterfall_tile_parent_class)->finalize (object);
}

/* Подготовка к генерации: определение диапазона строк, величины апсемпла и т.д. */
static gboolean
hyscan_waterfall_tile_prepare (HyScanWaterfallTilePrivate *priv,
                               gint                       *upsample,
                               gfloat                      step,
                               gboolean                   *regenerate)
{
  HyScanWaterfallTileParams *params = &priv->params;
  HyScanAmplitude    *dc;
  HyScanDBFindStatus  dc_find;
  gboolean            dc_status, dc_writeable;
  guint32             dc_lindex = 0, dc_rindex = 0;
  gint64              dc_ltime;
  gint32              h_start;
  gint32              h_end;
  gint32              v_start;
  gint32              v_end;
  gint64              t;
  guint32             lindex, rindex;
  guint32             lindex_next, rindex_next;
  gint64              lindex_next_time = -1;
  gint64              rindex_next_time = -1;
  gint32              w, h;
  gfloat              upsampled_step;
  gboolean            regen;
  gboolean            have_data;
  gint32              extension;
  guint32              n_vals;

  dc = priv->dc;
  h_start   = priv->tile.across_start;
  h_end     = priv->tile.across_end;
  v_start   = priv->tile.along_start;
  v_end     = priv->tile.along_end;
  regen = FALSE;
  have_data = TRUE;

  /* Проверяем отрицательность координат вдоль оси движения. */
  if (priv->tile.along_start < 0 || priv->tile.along_end < 0)
    {
      have_data = FALSE; /* Кадр пустой, */
      regen = FALSE;     /* перегенерация не требуется. */
      goto exit;
    }

  /* Проверяем диапазон данных и отказываемся генерировать кадр, если есть только одна строка. */
  dc_status = hyscan_amplitude_get_range (dc, &dc_lindex, &dc_rindex);
  if (dc_rindex == dc_lindex)
    {
      have_data = FALSE; /* Сейчас кадр пустой, */
      regen = TRUE;      /* Но в дальнейшем это изменится. */
      goto exit;
    }

  /* Получаем параметры КД. */
  dc_writeable = hyscan_amplitude_is_writable (dc);
  hyscan_amplitude_get_amplitude (dc, dc_lindex, &n_vals, &dc_ltime, NULL);

  /* Горизонтальные координаты могут быть отрицательными, но генератор понимает только положительные. */
  h_start = ABS (h_start);
  h_end = ABS (h_end);

  if (h_start > h_end)
    SWAP (h_start, h_end, gint32);
  if (v_start > v_end)
    SWAP (v_start, v_end, gint32);

  /* Добавим по 5% сверху и снизу для сшивания и рассчитаем размер кадра. */
  //extension = MIN (ABS ((h_end - h_start) * 0.05), 2500);
  //h_start = (h_start - extension < 0) ? 0 : h_start - extension;
  //h_end += extension;

  /* Добавим по 5% сверху и снизу для сшивания и рассчитаем размер кадра. */
  extension = MIN (ABS ((v_end - v_start) * 0.05), 2500);
  v_start -= extension;
  v_end += extension;

  /* Ищем диапазон строк, которые гарантированно лежат внутри кадра. */
  dc_status = TRUE;
  t = dc_ltime + (gint64)((v_start / priv->ship_speed) * 1e6);
  dc_find = hyscan_amplitude_find_data (dc, t, NULL, &lindex, NULL, NULL);
  if (G_LIKELY (dc_find == HYSCAN_DB_FIND_OK)); /* Тут ошибки нет. */
  else if (dc_find == HYSCAN_DB_FIND_LESS)
    lindex = dc_lindex;
  else
    dc_status = FALSE;

  t = dc_ltime + (gint64)((v_end / priv->ship_speed) * 1e6);
  dc_find = hyscan_amplitude_find_data (dc, t, &rindex, NULL, NULL, NULL);
  if (G_LIKELY (dc_find == HYSCAN_DB_FIND_OK)); /* Тут ошибки нет. */
  else if (dc_find == HYSCAN_DB_FIND_GREATER)
    rindex = dc_rindex;
  else
    dc_status = FALSE;

  if (!dc_status) /* Если произошла какая-то ошибка, то */
    {
      regen = dc_writeable; /* Кадр надо перегенерировать, если КД в режиме дозаписи. */
      have_data = FALSE;    /* Кадр пустой. */
      goto exit;
    }

  lindex_next = rindex_next = G_MAXUINT32;

  if (lindex > dc_lindex)
    {
      lindex_next = lindex - 1;
      hyscan_amplitude_get_amplitude (dc, lindex_next, &n_vals, &lindex_next_time, NULL);
    }
  if (rindex < dc_rindex)
    {
      rindex_next = rindex + 1;
      hyscan_amplitude_get_amplitude (dc, rindex_next, &n_vals, &rindex_next_time, NULL);
    }

  /* Если доп. строка есть только справа или слева, а внутри ничего, то пустой тайл. */
  // if ((lindex_next == G_MAXUINT32 || rindex_next == G_MAXUINT32) && !(rindex > lindex))
  //   {
  //     have_data = FALSE;
  //     have_data = TRUE;
  //   }

  regen = dc_writeable && (rindex == dc_rindex);

  h = ceil ((v_end - v_start) / step) * *upsample + 1;
  w = ceil ((h_end - h_start) / step) * *upsample + 1;
  upsampled_step = step / *upsample;

  params->dc_lindex = dc_lindex;
  params->dc_rindex = dc_rindex;
  params->dc_ltime = dc_ltime;
  params->frame_lindex = lindex;
  params->frame_rindex = rindex;
  params->frame_lindex2 = lindex_next;
  params->frame_rindex2 = rindex_next;
  params->frame_lindex2_time = lindex_next_time;
  params->frame_rindex2_time = rindex_next_time;
  params->w = w;
  params->h = h;
  params->step = upsampled_step;
  params->start_dist = v_start;
exit:
  *regenerate = regen;

  return have_data;
}

/* Функция реаллоцирует память под массивы и очищает их. */
static void
hyscan_waterfall_tile_reset (HyScanWaterfallTileParams  *params)
{
  gint width = params->w;
  gint height = params->h;

  /* Если требуемая ширина и высота не совпадает с фактической,
   * перевыделяем память. */
  if (width != params->real_w || height != params->real_h)
    {
      g_free (params->mark);
      g_free (params->data0);
      g_free (params->data1);
      g_free (params->data2);
      g_free (params->weight);
      g_free (params->data_add0);
      g_free (params->data_add1);
      g_free (params->weight_add);

      params->real_w = width;
      params->real_h = height;

      params->mark       = g_malloc (height * sizeof (gfloat));
      params->data0      = g_malloc (width * height * sizeof (gfloat));
      params->data1      = g_malloc (width * height * sizeof (gfloat));
      params->data2      = g_malloc (width * height * sizeof (gfloat));
      params->weight     = g_malloc (width * height * sizeof (gfloat));
      params->data_add0  = g_malloc (width * 2 * sizeof (gfloat));
      params->data_add1  = g_malloc (width * 2 * sizeof (gfloat));
      params->weight_add = g_malloc (width * 2 * sizeof (gfloat));
    }

  /* Очищаем массивы. */
  memset (params->mark,       0, height * sizeof (gfloat));
  memset (params->data0,      0, width * height * sizeof (gfloat));
  memset (params->data1,      0, width * height * sizeof (gfloat));
  memset (params->data2,      0, width * height * sizeof (gfloat));
  memset (params->weight,     0, width * height * sizeof (gfloat));
  memset (params->data_add0,  0, width * 2 * sizeof (gfloat));
  memset (params->data_add1,  0, width * 2 * sizeof (gfloat));
  memset (params->weight_add, 0, width * 2 * sizeof (gfloat));

}

/* Наполнение апсемплированного массива данными. */
static gboolean
hyscan_waterfall_tile_fill (HyScanWaterfallTilePrivate *priv,
                            gboolean                   *have_data)
{
  HyScanWaterfallTileParams *params = &priv->params;
  HyScanAmplitude *dc = priv->dc;
  HyScanAcousticDataInfo dc_info = hyscan_amplitude_get_info (dc);

  gfloat depth = 0;

  guint w = params->w;
  guint h = params->h;

  gfloat *data0 = params->data0;
  gfloat *data_add = params->data_add0;
  gfloat *mark = params->mark;
  gfloat *weight = params->weight;
  gfloat *weight_add = params->weight_add;

  gint32 start_dist = params->start_dist;
  gfloat step = params->step;
  guint32 lindex = params->frame_lindex;
  guint32 rindex = params->frame_rindex;
  guint32 lindex2 = params->frame_lindex2;
  guint32 rindex2 = params->frame_rindex2;
  guint32 lend, rend;
  gint64 dc_ltime = params->dc_ltime;

  guint32 i, jsum, jtot, jprev;
  gint64 j, time;
  gfloat s;
  gboolean have_data_int, make_empty, status;
  const gfloat *vals;
  guint32 n_vals;
  gboolean is_ground_distance;
  gboolean is_profiler;


  have_data_int = FALSE;
  vals          = NULL;
  n_vals        = 0;
  jsum          = jtot = jprev = 0;

  is_ground_distance = priv->tile.flags & HYSCAN_TILE_GROUND;
  is_profiler = priv->tile.flags & HYSCAN_TILE_PROFILER;

  // if (HYSCAN_SOURCE_PROFILER != hyscan_acoustic_data_get_source (HYSCAN_ACOUSTIC_DATA (dc)))
    // is_profiler = FALSE;

  /* Узнаем время первой записи в КД. */
  if (hyscan_amplitude_get_range (dc, &lend, &rend))
    hyscan_amplitude_get_amplitude (dc, lend, &n_vals, &dc_ltime, NULL);

  for (i = lindex; i <= rindex; i++)
    {
      /* Проверяем досрочное завершение. */
      if (g_atomic_int_get (&priv->generator_term) == 1)
        goto exit;

      /* Забираем строку из БД. */
      if (is_profiler)
        vals = pf_get_values (dc, i, lend, rend, &n_vals, &time);
      else
        vals = hyscan_amplitude_get_amplitude (dc, i, &n_vals, &time, NULL);
      if (vals == NULL)
        continue;

      /* Определяем индекс. */

      s = (time - dc_ltime) * priv->ship_speed / 1e6; /* мкс * мм/с / 1е6 */
      j = round ((s - start_dist) / step);

      if (j < h && j >= 0)
        {
          jsum += j - jprev;
          jtot++;
          jprev = j;

          if (is_ground_distance)
            depth = hyscan_depthometer_get (priv->depth, time);

          /* Crutch Driven Development. */
          if (is_profiler)
            derivativate ((gfloat*)vals, n_vals);

          status = hyscan_waterfall_tile_make_string (priv, data0 + j * w, w, weight + j * w,
                                                      vals, n_vals, step, priv->tile.flags,
                                                      dc_info.data_rate, depth);
          if (is_profiler)
            g_clear_pointer (&vals, g_free);

          if (status)
            {
              have_data_int = TRUE;
              mark[j] = j + 1;
            }
          else
            {
              mark[j] = - (j + 1);
            }
        }
    }

  /* Дополнительные строки. */
  for (j = 0; j < 2; j++)
    {
      /* Выбираем строку. */
      i = (j == 0) ? lindex2 : rindex2;
      make_empty = FALSE;
      /* Если строка найдена, забираем из неё данные. */
      if (i != G_MAXUINT32)
        {
          vals = hyscan_amplitude_get_amplitude (dc, i, &n_vals, &time, NULL);
          if (vals != NULL)
            {
              if (priv->tile.flags & HYSCAN_TILE_GROUND)
                depth = hyscan_depthometer_get (priv->depth, time);

              status = hyscan_waterfall_tile_make_string (priv, data_add + j * w, w,
                                                          weight_add + j * w,
                                                          vals, n_vals, step, priv->tile.flags,
                                                          dc_info.data_rate, depth);
              if (status)
                have_data_int = TRUE;
              else
                make_empty = TRUE;
            }
        }
      if (make_empty)
        {
          for (i = 0; i < w; i++)
            {
              data_add[j * w + i] = TRANSPARENT;
              weight_add[j * w + i] = 1.0;
            }
        }
    }

  params->filter_opt = jtot > 0 ? jsum / jtot : G_MAXINT;

  *have_data = have_data_int;
  return TRUE;

 exit:
  return FALSE;
}

/* Функция отображает строку в НД или ГД. */
static gboolean
hyscan_waterfall_tile_make_string (HyScanWaterfallTilePrivate *priv,
                                   gfloat                     *data0,
                                   guint32                     data_size,
                                   gfloat                     *weight,
                                   const gfloat               *input,
                                   guint32                     input_size,
                                   gfloat                      step,
                                   HyScanTileFlags             flags,
                                   gfloat                      dfreq,
                                   gfloat                      depth)
{
  gint32 start       = priv->tile.across_start;
  gint32 end         = priv->tile.across_end;
  gfloat speed       = priv->sound_speed / 2.0;
  gboolean have_data = TRUE;
  gint64 k           = 0;
  guint32 j, istart, iend;
  gfloat real_index;
  gboolean slant = !(flags & HYSCAN_TILE_GROUND);

  depth *= 1000.0; /* Из м в мм. */

  /* Горизонтальные координаты могут быть отрицательными, но генератор понимает только положительные. */
  start = ABS (start);
  end = ABS (end);
  if (start > end)
    SWAP (start, end, gint32);

  if (slant)
    {
      istart = dfreq * start / speed; /* Начальный индекс входной строки. */
      iend = dfreq * end / speed;     /* Конечный индекс входной строки. */
    }
  else
    {
      istart = sqrt (pow ((gfloat)start, 2) + depth * depth) * dfreq / speed; /* Начальный индекс входной строки. */
      iend = sqrt (pow ((gfloat)end, 2) + depth * depth) * dfreq / speed; /* Конечный индекс входной строки. */
    }

  istart = MAX (istart, 0);

  /* Проходим по всем индексам. */
  for (j = istart; j < iend; j++)
    {
      /* Если вышли за пределы входной строки, заполняем "прозрачностью". */
      if (j >= input_size)
        {
          for ( ; k < data_size; k++)
            {
              /* Будем делать прозрачную точку, только если ещё ничего нет. */
              if (weight[k] == 0.0)
                {
                  data0[k] = TRANSPARENT;
                  weight[k] = 1.0;
                }
            }

          /* При этом если вышли прям на первом индексе, то строка полностью пустая. */
          if (istart >= input_size)
            have_data = FALSE;

          goto exit;
        }

      /* Если же не вышли, стягиваем строчку. */
      if (slant)
        real_index = ((j * speed / dfreq) - start) / step;
      else
        real_index = (sqrt (pow (j * speed / dfreq, 2) - pow (depth, 2)) - start) / step;

      k = round (real_index);

      if (k >= 0 && k < data_size)
        {
          data0[k] += input[j];
          weight[k] += 1.0;
        }
      else if (k >= data_size)
        {
          goto exit;
        }
    }

 exit:
  return have_data;
}

/* Обертка для интерполяции и фильтрации внутри строк. */
static void
hyscan_waterfall_tile_string_helper (HyScanWaterfallTilePrivate *priv,
                                     gint                        filter)
{
  gint i;
  gint width;
  gfloat *src;
  gfloat *dest;
  gfloat *mark;
  gfloat *weight;

  HyScanWaterfallTileParams *params = &priv->params;

  if (filter % 2 == 0)
    filter++;

  src = params->data0;
  dest = params->data1;
  weight = params->weight;
  mark = params->mark;
  width = params->w;

  for (i = 0; i < params->h; i++)
    {
      /* Проверяем досрочное завершение. */
      if (g_atomic_int_get (&priv->generator_term) == 1)
        return;

      if (mark[i] > 0.0)
        {
          hyscan_waterfall_tile_interpolate_string (src + i * width, weight + i * width, width);
          hyscan_waterfall_tile_filter_string (filter, src + i * width, dest + i * width, width);
        }
      else if (mark[i] < 0.0)
        {
          memcpy (dest + i * width, src + i * width, width * sizeof (gfloat));
        }
    }

  src = params->data_add0;
  dest = params->data_add1;
  weight = params->weight_add;

  for (i = 0; i < 2; i++)
    {
      /* Проверяем досрочное завершение. */
      if (g_atomic_int_get (&priv->generator_term) == 1)
        return;

      hyscan_waterfall_tile_interpolate_string (src + i * width, weight + i * width, width);
      hyscan_waterfall_tile_filter_string (filter, src + i * width, dest + i * width, width);
    }
}

/* Интерполяция внутри одной строки. */
static void
hyscan_waterfall_tile_interpolate_string (gfloat *string,
                                          gfloat *weight,
                                          gint    size)
{
  gint i, j;

  for (i = 0; i < size; i++)
    if (weight[i] != 0.0 && weight[i] != 1.0)
      string[i] /= weight[i];

  for (i = 0; i < size; i++)
    {
      if (weight[i] != 0.0)
        continue;

      for (j = 1; ABS (j) < size; j = (j > 0) ? -j : -j + 1)
        {
          if (i + j >= 0 && i + j < size && weight[i + j] != 0.0)
            {
              string[i] = string[i + j];
              weight[i] = 1.0;
              break;
            }
        }
    }
}

/* Интерполяция внутри кадра (вспомогательная функция). */
static void
hyscan_waterfall_tile_interpolate_2 (HyScanWaterfallTileParams  *params,
                                     gfloat                     *left,
                                     gfloat                     *right,
                                     gint                        ileft,
                                     gint                        iright,
                                     gfloat                      lmark,
                                     gfloat                      rmark,
                                     gint32                      cleft,
                                     gint32                      cright)
{
  gfloat *data = params->data1;
  gfloat *mark = params->mark;
  gint    w    = params->w;
  gfloat  step = params->step;
  gint32  half = (cright + cleft) / 2.0;
  gint32 coord;
  gfloat *src;
  gint i;

  if (left == NULL || right == NULL)
    {
      /* Фактически заполняем только первую строку. */
      for (i = ileft * w; i < ileft * w + w; i++)
        data[i] = TRANSPARENT;

      /* А остальные копируем. */
      src = data + ileft * w;
      for (i = ileft + 1; i < iright; i++)
        memcpy (data + i * w, src, w * sizeof (gfloat));

      return;
    }

  for (i = ileft; i <= iright; i++)
    {
      coord = i * step + params->start_dist;

      if (coord > half)
        {
          src = right;
          mark[i] = rmark;
        }
      else
        {
          src = left;
          mark[i] = lmark;
        }
      memcpy (data + i * w, src, w * sizeof (gfloat));
    }
}

/* Интерполяция кадра. */
static gboolean
hyscan_waterfall_tile_interpolate_frame (HyScanWaterfallTilePrivate *priv)
{
  HyScanWaterfallTileParams *params = &priv->params;
  gint64   ltime, rtime;
  gfloat  *data, *data_add, *mark;
  gint     w, h, i;
  gfloat *left_src, *right_src;
  gint32 left_src_dist, right_src_dist;
  gint ileft, iright;
  gboolean lfound, rfound;
  gfloat lmark, rmark;

  ltime  = params->frame_lindex2_time;
  rtime  = params->frame_rindex2_time;
  data   = params->data1;
  data_add  = params->data_add1;
  mark   = params->mark;
  w      = params->w;
  h      = params->h;
  lfound = params->frame_lindex2 != G_MAXUINT32;
  rfound = params->frame_rindex2 != G_MAXUINT32;

  left_src = lfound ? data_add : NULL;
  left_src_dist = lfound ? (ltime - params->dc_ltime) * priv->ship_speed / 1e6 : 0;
  lmark = -G_MAXFLOAT;

  for (i = 0; i < h; i++)
    {
      if (mark[i] != 0.0)
        {
          left_src = data + i * w;
          left_src_dist = params->start_dist + i * params->step;
          lmark = mark[i];
        }
      else
        {
          ileft = i;
          right_src = NULL;

          for (i = i+1 ; i < h; i++)
            {
              if (mark[i] != 0.0)
                {
                  iright = i - 1;
                  right_src = data + i * w;
                  right_src_dist = params->start_dist + i * params->step;
                  rmark = mark[i];
                  break;
                }
            }
          if (right_src == NULL)
            {
              iright = h - 1;
              right_src = rfound ? data_add + w : NULL;
              right_src_dist = rfound ? (rtime - params->dc_ltime) * priv->ship_speed / 1e6 : 0;
              rmark = G_MAXFLOAT;
            }

          hyscan_waterfall_tile_interpolate_2 (params, left_src, right_src,
                                               ileft, iright,
                                               lmark, rmark,
                                               left_src_dist, right_src_dist);
          left_src = right_src;
          left_src_dist = right_src_dist;
          lmark = rmark;
        }
    }

  return TRUE;
}

/* Функция фильтрует по горизонтальной оси. */
static gboolean
hyscan_waterfall_tile_filter_string (gint    filter_size,
                                     gfloat *src,
                                     gfloat *dest,
                                     gint    width)
{
  gint i, j, half;

  half = (filter_size - 1) / 2;

  for (i = 0; i < width; i++)
    {
      gfloat val = 0;
      gint num = 0;

      for (j = - half; j <= half; j++)
        {
          if (i + j >= 0 && i + j < width)
            {
              val += src[i + j];
              num++;
            }
        }

      dest[i] = (num > 0) ? val / num : src[i];
    }

  return TRUE;
}

/* Функция фильтрует по вертикальной оси. */
static gboolean
hyscan_waterfall_tile_filter_frame (HyScanWaterfallTilePrivate *priv,
                                    gint                        filter_size,
                                    gfloat                     *src,
                                    gfloat                     *mark,
                                    gfloat                     *dest,
                                    gint                        width,
                                    gint                        height)
{
  gint i, j, k;
  gint half;

  gboolean skip;

  if (filter_size % 2 == 0)
    filter_size++;

  half = (filter_size - 1) / 2;

  for (i = 0; i < height; i++)
    {
      /* Проверяем досрочное завершение. */
      if (g_atomic_int_get (&priv->generator_term) == 1)
        return FALSE;

      /* Сначала проанализируем, нужно ли фильтровать эту строку. */
      /* Строку не нужно фильтровать тогда, когда в фильтр попадут одни и те же строки. */
      skip = TRUE;
      for (j = -half + 1; j <= half; j++)
        {
          k = i + j;
          if (k > 0 && k < height && mark[k] != mark[k - 1])
            {
              skip = FALSE;
              break;
            }
        }

      /* Если строку можно не фильтровать, то просто скопируем её в dest. */
      if (skip)
        {
          memcpy (dest + i * width, src + i * width, width * sizeof (gfloat));
          continue;
        }

      /* Иначе строку надо честно фильтровать. */
      for (j = 0; j < width; j++)
        {
          gfloat val = 0;
          gint num = 0;

          for (k = -half; k <= half; k++)
            {
              gint i2 = i + k;

              if (i2 > 0 && i2 < height)
                {
                  val += src [i2 * width + j];
                  num++;
                }
            }

          dest[i * width + j] = (num > 0) ? val / num : src[i * width + j];
        }
    }

  return TRUE;
}


/* Составление выходного кадра. */
static gfloat*
hyscan_waterfall_tile_compose_frame (HyScanWaterfallTilePrivate *priv,
                                     gfloat                     *src,
                                     gint                        data_w,
                                     gint                        data_h,
                                     gint                        frame_height,
                                     gint                        frame_width,
                                     gfloat                      step,
                                     gint                        upsample,
                                     gfloat                      up_start,
                                     gfloat                      up_step)
{
  gfloat  *dest;
  gboolean mirror, rotate;
  gint32   start;
  gint32   end;
  gint     i, j;

  mirror = (priv->requested_tile.across_start < 0) ? TRUE : FALSE;
  rotate = priv->tile.rotate;
  start  = ABS (priv->tile.along_start);
  end    = ABS (priv->tile.along_end);
  dest = g_malloc0 (frame_width * frame_height * sizeof(gfloat));

  if (start > end)
    SWAP (start, end, gint32);

  /* Составляем выходной тайл. */
  gint out_i, out_j;
  gint in_i, in_j;

  /* Если поворот не требуется. */
  if (!rotate)
    {
      for (i = 0; i < frame_height; i++)
        {
          in_i = round ((start + i * step - up_start) / up_step);
          out_i = frame_height - 1 - i;

          for (j = 0; j < frame_width; j++)
            {
              in_j = j * upsample;
              out_j = (mirror) ? (frame_width - 1 - j) : (j);

              dest[out_i * frame_width + out_j] = src [in_i * data_w + in_j];
            }
        }
    }
  /* Если поворот требуется. */
  else
    {
      for (i = 0; i < frame_height; i++)
        {
          in_j = round ((start + i * step - up_start) / up_step);
          out_i = i;

          for (j = 0; j < frame_width; j++)
            {
              in_i = j * upsample;
              out_j = (mirror) ? j : frame_width - 1 - j;

              dest[out_j * frame_width + out_i] = src [in_j * data_w + in_i];
            }
        }
    }

  return dest;
}

/* Функция создает объект генерации тайлов. */
HyScanWaterfallTile*
hyscan_waterfall_tile_new (void)
{
  return g_object_new (HYSCAN_TYPE_WATERFALL_TILE, NULL);
}

/* Функция устанавливает кэш. */
void
hyscan_waterfall_tile_set_cache (HyScanWaterfallTile *wfall,
                                 HyScanCache         *cache,
                                 const gchar         *prefix)
{
  return;
}

/* Функция устанавливает путь к БД, проекту и галсу. */
void
hyscan_waterfall_tile_set_path (HyScanWaterfallTile *wfall,
                                const gchar         *path)
{
  return;
}

/* Функция передает генератору объект с навигационными данными. */
gboolean
hyscan_waterfall_tile_set_depth (HyScanWaterfallTile *wfall,
                                 HyScanDepthometer   *depth)
{
  g_return_val_if_fail (HYSCAN_IS_WATERFALL_TILE (wfall), FALSE);

  /* Проверяем, не идет ли сейчас генерация тайла. */
  if (g_atomic_int_get (&wfall->priv->generator_busy) == 1)
    return FALSE;

  /* Очищаем старый объект и запихиваем новый. */
  g_clear_object (&wfall->priv->depth);

  if (depth != NULL)
    wfall->priv->depth = g_object_ref (depth);

  return TRUE;
}

/* Функция устанавливает скорость судна и звука. */
gboolean
hyscan_waterfall_tile_set_speeds (HyScanWaterfallTile *wfall,
                                  gfloat               ship_speed,
                                  gfloat               sound_speed)
{
  g_return_val_if_fail (HYSCAN_IS_WATERFALL_TILE (wfall), FALSE);

  /* Проверяем, не идет ли сейчас генерация тайла. */
  if (g_atomic_int_get (&wfall->priv->generator_busy) == 1)
    return FALSE;

  /* Проверяем, что нам не передали нулевые или отрицательные скорости. */
  if (sound_speed <= 0.0 || ship_speed <= 0.0)
    return FALSE;

  /* Записываем новые значения и переводим их в мм/с. */
  wfall->priv->ship_speed = ship_speed * 1000.0;
  wfall->priv->sound_speed = sound_speed * 1000.0;

  return TRUE;
}

/* Установка КД и параметров тайла. */
gboolean
hyscan_waterfall_tile_set_tile (HyScanWaterfallTile *wfall,
                                HyScanAmplitude     *dc,
                                HyScanTile           tile)
{
  HyScanWaterfallTilePrivate *priv;
  g_return_val_if_fail (HYSCAN_IS_WATERFALL_TILE (wfall), FALSE);
  priv = wfall->priv;

  if (tile.along_start == tile.along_end || tile.across_start == tile.across_end)
    return FALSE;

  if (!HYSCAN_IS_AMPLITUDE (dc))
    return FALSE;

  /* Проверяем, не идет ли сейчас генерация тайла. */
  if (g_atomic_int_get (&priv->generator_busy) == 1)
    return FALSE;

  priv->tile = priv->requested_tile = tile;
  priv->dc = dc;

  return TRUE;
}

/* Генерация тайла. */
gfloat*
hyscan_waterfall_tile_generate (HyScanWaterfallTile *wfall,
                                HyScanTile          *tile,
                                guint32             *size)
{
  HyScanWaterfallTilePrivate *priv;

  gint      i;
  gfloat    step;
  gint      upsample, vfilt;
  gint      frame_height, frame_width;
  gboolean  have_data;
  gboolean  regenerate;
  gboolean  completed;
  HyScanWaterfallTileParams *params;

  gfloat   *output = NULL;

  g_return_val_if_fail (HYSCAN_IS_WATERFALL_TILE (wfall), FALSE);
  priv = wfall->priv;
  params = &priv->params;

  /* Устанавливаем флаг "генерируется". */
  if (!g_atomic_int_compare_and_exchange (&priv->generator_busy, 0, 1))
    return NULL;

  /* По умолчанию считаем, что тайл перегенерировать не надо. Если всё же будет надо,
   * поменяем это значение позже. */
  upsample = priv->requested_tile.upsample;

  /* Вычисляем шаг, то есть сколько миллиметров в одном пикселе. А потом размер кадра на выходе. */
  step = hyscan_tile_common_mm_per_pixel (priv->tile.scale, priv->tile.ppi);
  frame_height = hyscan_tile_common_tile_size (priv->tile.along_start, priv->tile.along_end, step);
  frame_width = hyscan_tile_common_tile_size (priv->tile.across_start, priv->tile.across_end, step);

  /* Предварительные рассчеты. */
  have_data = hyscan_waterfall_tile_prepare (priv, &upsample, step, &regenerate);

  if (!have_data)
    goto empty_frame;

  /* Подготавливаем и очищаем массивы. */
  hyscan_waterfall_tile_reset (params);

  /* Наполняем массив data0 данными. */
  completed = hyscan_waterfall_tile_fill (priv, &have_data);
  if (!completed)
    goto user_terminate;

  if (!have_data)
    goto empty_frame;

  /* Теперь проходим по строкам. Интерполируем и фильтруем. Результат будет в data1. */
  hyscan_waterfall_tile_string_helper (priv, upsample);

  /* Интерполяция по кадру. Результат остается в data1. */
  completed = hyscan_waterfall_tile_interpolate_frame (priv);
  if (!completed)
    goto user_terminate;

  /* Оптимизируем размер фильра. */
  vfilt = 2 * MAX (params->filter_opt, upsample);
  if (upsample > 1)
    {
      if (vfilt > upsample * upsample * upsample)
        vfilt = upsample * upsample * upsample;
    }
  else
    {
      if (vfilt > FILTER_THRESHOLD * FILTER_THRESHOLD)
        vfilt = FILTER_THRESHOLD * 2;
    }

  /* Фильтруем. Результат будет в data2. */
  completed = hyscan_waterfall_tile_filter_frame (priv, vfilt,
                                                  params->data1,
                                                  params->mark,
                                                  params->data2,
                                                  params->w,
                                                  params->h);
  if (!completed)
    goto user_terminate;

  /* Составляем кадр. */
  output = hyscan_waterfall_tile_compose_frame (priv, params->data2, params->w, params->h,
                                                frame_height, frame_width, step, upsample,
                                                params->start_dist, params->step);
  empty_frame:
  if (!have_data)
    {
      output = g_malloc0 (frame_width * frame_height * sizeof (gfloat));
      for (i = 0; i < frame_width * frame_height; i++)
        output[i] = TRANSPARENT;
    }

  /* Обновляем параметры тайла. */
  priv->requested_tile.w = frame_width;
  priv->requested_tile.h = frame_height;
  priv->requested_tile.finalized = regenerate ? FALSE : TRUE;

 user_terminate:

  if (size != NULL)
    *size = frame_width * frame_height * sizeof (gfloat);
  if (tile != NULL)
    *tile = priv->requested_tile;

  g_atomic_int_set (&priv->generator_busy, 0);
  g_atomic_int_set (&priv->generator_term, 0);

  return output;
}

/* Функция завершает генерацию тайла. */
gboolean
hyscan_waterfall_tile_terminate (HyScanWaterfallTile *wfall)
{
  g_return_val_if_fail (HYSCAN_IS_WATERFALL_TILE (wfall), FALSE);

  /* Ставим флаг и ждем остановки. */
  while (g_atomic_int_get (&wfall->priv->generator_busy) != 0)
    g_atomic_int_set (&wfall->priv->generator_term, 1);

  return TRUE;
}

#define NAN_F(x) if ((x) < 0) g_error ("FUCK");

static void derivativate (gfloat* data, guint32 size)
{
  guint32 i;
  for (i = 0; i < size - 1; ++i)
    {
      data[i] = MAX (0, data[i+1] - data[i]);
    }
  data[size - 1] = data[size - 2];
}

static gfloat * pf_get_values
(HyScanAmplitude *dc, guint32 i, guint32 l, guint32 r, guint32* n_vals, gint64 *time)
{
  gint k0, k1, k2;
  const gfloat *d1;
  gfloat *d0;
  guint32 s0, s1, s2, j;

  k0 = (i >= l + 1) ? i - 1 : l;
  k1 = i;
  k2 = (i <= r - 1) ? i + 1 : r;

  hyscan_amplitude_get_amplitude (dc, k0, &s0, time, NULL);
  hyscan_amplitude_get_amplitude (dc, k1, &s1, time, NULL);
  d1 = hyscan_amplitude_get_amplitude (dc, k2, &s2, time, NULL);
  // for (k0 = 0; k0 < s1; ++k0)
    // d1[k0] = 1.0;

  //derivativate (d1, s1);
  //return d1;

  /* минимальный размер строки. */
  s0 = MIN (MIN (s0, s1), s2);
  d0 = g_malloc0 (sizeof (gfloat) * s0);

  /* Копируем к0. */
  // d1 = hyscan_amplitude_get_amplitude (dc, k0, &s1, time);
  memcpy (d0, d1, s0 * sizeof (gfloat));

  /* Копируем к1*/
  d1 = hyscan_amplitude_get_amplitude (dc, k1, &s1, time, NULL);

  for (j = 0; j < s0; ++j)
    {
      d0[j] += d1[j];
      NAN_F (d0[j]);
    }

  /* Копируем к2*/
  d1 = hyscan_amplitude_get_amplitude (dc, k2, &s1, time, NULL);

  for (j = 0; j < s0; ++j)
    {
      d0[j] += d1[j];
      d0[j] /= 3;
      NAN_F (d0[j]);
    }

  if (n_vals != NULL)
    *n_vals = s0;
  derivativate (d0, s0);

  return d0;
}
