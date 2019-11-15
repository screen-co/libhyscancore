/* hyscan-hsx-converter.c
 *
 * Copyright 2019 Screen LLC, Maxim Pylaev <pilaev@screen-co.ru>
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
 * SECTION: hyscan-hsx-converter
 * @Short_description: класс конвертации данных из базы данных 
 * в формат HSX. 
 * @Title: HyScanHSXConverter
 *
 * Класс выполняет функцию конвертирования данных навигации и акустической
 * съемки в формат HSX.
 *
 * #hyscan_hsx_converter_new - создание класса с указанием выходной папки 
 * для сформированных классов. 
 * #hyscan_hsx_converter_set_track - установка БД, проекта и галса для текущего 
 * конвертирования. 
 * #hyscan_hsx_converter_set_max_ampl - установка максимального значения 
 * амплитудного отсчета в выходном файле. (Степень двойки 4096, 8192, до 65536)
 * #hyscan_hsx_converter_set_image_prm - установка преобразования акустических
 * данных по черной и белой точкам, и гамме. Выполняется при необходимости также перед
 * началом конвертации.
 * #hyscan_hsx_converter_set_velosity - установка скорости звука в воде для текущей конвертации
 * #hyscan_hsx_converter_init_crs -установка параметров исходной системы координат в формате proj4.
 *
 * В процессе преобразования данных класс иммитирует сигнал @exec с целочисленным 
 * значением выполнения в процентах.
 * По достижении 100% класс иммитирует сигнал @done - без передаваемых параметров.
 *
 * #hyscan_hsx_converter_run - запуск потока конвертирования данных.
 * #hyscan_hsx_converter_stop - останавливает конвертацию.
 * #hyscan_hsx_converter_is_run - проверка работы потока конвертирования.
 * 
 * Для работы с классом необходимо его создать, указать текущий галс для конвертации.
 * При необходимости установить параметры обработки акустического изображения и
 * максимальной выходной амплитуды, скорости звука. Обязательно установить параметры 
 * исходной системы координат #hyscan_hsx_converter_init_crs.
 * Подписаться на сигналы, если необходимо или периодически опрашивать класс о наличии
 * конвертирования. 
 * Затем запустить процесс конвертации.
 * Дождаться её завершения, или принудительно остановить поток.
 * Для конвертации следующего галса выполнить установку проекта и галса.
 * При завершении работы с классом освободить память %g_object_unref.
 */
#include "hyscan-hsx-converter.h"
#include "hyscan-data-player.h"
#include "hyscan-factory-amplitude.h"
#include "hyscan-nmea-parser.h"
#include <hyscan-cached.h>
#include <gio/gio.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <math.h>
#include <string.h>
#include <proj_api.h>

#define HYSCAN_HSX_CONVERTER_NMEA_PARSERS_COUNT       14        /* Количество типов NMEA данных */
#define HYSCAN_HSX_CONVERTER_DEFAULT_MAX_AMPLITUDE    8191      /* Максимальное значение амплитуды по умолчанию */
#define HYSCAN_HSX_CONVERTER_DEFAULT_MAX_RSS_SIZE     2048      /* Максимум значений на один борт в RSS строке */
#define HYSCAN_HSX_CONVERTER_DEFAULT_VELOSITY         1500.0    /* Дефолтное значение скорости звука в воде, м/с */
#define UNINIT                                        (-500.0)  /* Неинициализированное значение */


/* Добавка микросекунд в GDateTime */
GDateTime*
add_microsec (GDateTime *dt, gdouble us) 
{
  return g_date_time_add_seconds (dt, us / 1.e6);
}

/* Получение секунд из GDateTime */
gdouble
get_seconds (GDateTime *dt)
{
  return ((g_date_time_get_hour (dt) * 3600 + 
           g_date_time_get_minute (dt) * 60 +
           g_date_time_get_seconds (dt)));
}

enum
{
  PROP_O,
  PROP_RESULT_PATH                                        /* Путь выходной папки */
};

enum
{
  SIGNAL_EXEC,                                            /* Выполнение */
  SIGNAL_DONE,                                            /* Завершено */
  SIGNAL_LAST
};

enum
{
  SIGNAL_PLAYER_PROCESS,                                  /* Сигнал от плеера - process */
  SIGNAL_PLAYER_RANGE,                                    /* Сигнал от плеера - range */
  SIGNAL_PLAYER_LAST
};

typedef enum
{
  HYSCAN_AC_TYPE_PORT = 0,                               /* Индекс для данных левого борта */
  HYSCAN_AC_TYPE_STARBOARD,                              /* Индекс для данных правого борта */
  HYSCAN_AC_TYPE_LAST
} AcousticType;

typedef enum
{
  HYSCAN_NMEA_TYPE_RMC = 0,                              /* Индекс для NMEA данных типа RMC */              
  HYSCAN_NMEA_TYPE_GGA,                                  /* Индекс для NMEA данных типа GGA */
  HYSCAN_NMEA_TYPE_DPT,                                  /* Индекс для NMEA данных типа DPT */
  HYSCAN_NMEA_TYPE_HDT,                                  /* Индекс для NMEA данных типа HDT */
  HYSCAN_NMEA_TYPE_LAST
} NmeaType;

/* Для выходного файла */
typedef struct
{
  gchar                         *path;                  /* Путь до выходного файла */
  gchar                         *file_name;             /* Имя выходного файла */
  GFile                         *file;                  /* Файл */
  GOutputStream                 *out_stream;            /* Поток записи */
}HyScanHSXConverterOut;

/* структура для записи значений в файл */
typedef struct
{
  struct
  {
    gdouble                      in_time;               /* Метка времени из БД */
    gdouble                      time;                  /* Метка времени с начала суток*/
    gint                        *data;                  /* Указатель на данные */
    guint32                      size;                  /* Размер данных */
  }                              acoustic[HYSCAN_AC_TYPE_LAST];
                                                        /* Для левого и правого бортов   */

  gdouble                        cut_fs;                /* Приведенная частота семплов после сжатия */

  gdouble                        depth;                 /* Текущая глубина, м */
  gdouble                        depth_time;            /* Время DPT */
  gdouble                        sound_velosity;        /* Cкорость звука м/с */

  /* Времена фиксации сообщений */
  gdouble                        rmc_time;              /* Время RMC */
  gdouble                        gga_time;              /* Время GGA */
  gdouble                        hdt_time;              /* Время HDT */

  gdouble                        heading;               /* Курс (HDT) */
  gdouble                        tracking;              /* Курс (COG) */
  gdouble                        quality;               /* Качество */
  gdouble                        speed_knots;           /* Скорость носителя в морских узлах */
  gdouble                        hdop_gps;              /* HDOP */
  gint                           sat_count;             /* Количество спутников */
  gdouble                        altitude;              /* Высота над геоидом, м */
  gdouble                        roll;                  /* Крен, г + на правый борт */
  gdouble                        pitch;                 /* Дифферент, г. + на нос */
  gdouble                        x;                     /* Кооордината х в прямоугольной проекции */
  gdouble                        y;                     /* Кооордината y в прямоугольной проекции */
}HyScanHSXConverterOutData;

/* Для коррекции амплитуд */
typedef struct
{
  gfloat                         black;                 /* Черная точка */
  gfloat                         white;                 /* Белая точка */
  gfloat                         gamma;                 /* Гамма */
}HyScanHSXConverterImagePrm;

typedef struct
{
  projPJ                         proj_src;              /* Система координат (СК) исходная */
  projPJ                         proj_dst;              /* СК конечная */
  gchar                         *param_dst;             /* Проекция и датум конечной СК */
  gint                           zone_number;           /* Номер зоны конечной СК */
}HyScanHSXConverterProj;

struct _HyScanHSXConverterPrivate
{
  HyScanCache                   *cache;                 /* Кеш */
  HyScanDataPlayer              *player;                /* Плеер данных */
  GDateTime                     *track_time;            /* Время начала галса */

  HyScanFactoryAmplitude        *ampl_factory;          /* Фабрика амплитудных объектов */
  guint                          max_ampl_value;        /* Максимальное значение амплитуд в выходных данных */
  guint                          max_rss_size;          /* Максимальное количество элементов амплитуд */
  gint64                         zero_time;             /* Начальная точка для всех данных */
  gfloat                         sound_velosity;        /* Скорость звука в воде */

  HyScanAmplitude               *ampl[HYSCAN_AC_TYPE_LAST];
                                                        /* Инферфейсы для амплитудных данных */
  HyScanHSXConverterImagePrm     image_prm;             /* Структура для коррекции отсчетов амплитуд */

  HyScanNavData                 *nmea[HYSCAN_HSX_CONVERTER_NMEA_PARSERS_COUNT];
                                                        /* Парсеры данных NMEA */
  HyScanHSXConverterProj         transform;             /* Объекты для преобразования координат */
  struct
  {
    gint64                       min_time;              /* Минимальное время для всех источников */
    gint64                       max_time;              /* Максимальное время для всех источников */
    gdouble                      percent_koeff;         /* Процентный коэффициент */
    gint                         current_percent;       /* Текущий процент выполнения конвертации */
  }                              state;                 /* Состояние выполнения конвертации */

  GThread                       *conv_thread;           /* Поток конвертирования */
  volatile gint                  is_run;                /* Флаг работы потока */

  HyScanHSXConverterOut          out;                   /* Выходная структура для записи в файл */
  HyScanHSXConverterOutData      data;                  /* Выходные даннные для записи в файл */
};

static void       hyscan_hsx_converter_set_property             (GObject                   *object,
                                                                 guint                      prop_id,
                                                                 const GValue              *value,
                                                                 GParamSpec                *pspec);
static void       hyscan_hsx_converter_object_constructed       (GObject                   *object);
static void       hyscan_hsx_converter_object_finalize          (GObject                   *object);

/* Коррекция входного массива %data */
static void       hyscan_hsx_converter_add_image_prm            (gfloat                    *data,
                                                                 gint                       size,
                                                                 gfloat                     black,
                                                                 gfloat                     white,
                                                                 gfloat                     gamma);

/* Упаковка данных %input в размер out_size */
static gfloat*    hyscan_hsx_converter_pack                     (gfloat                    *input,
                                                                 gint                       size,
                                                                 gint                       out_size);

/* Обрезка от 0 до 1 данных %data */
static void       hyscan_hsx_converter_clamp_0_1                (gfloat                    *data,
                                                                 gint                       size);

/* Преобразование %data в целочисленный массив */
static gint*      hyscan_hsx_converter_float2int                (gfloat                    *data, 
                                                                 gint                       size, 
                                                                 gint                       mult);

/* Определение индекса данных, ближайшего по времени %time */
static guint32    hyscan_hsx_converter_nearest                  (gint64                     time,
                                                                 guint32                    lindex,
                                                                 guint32                    rindex,
                                                                 gint64                     ltime, 
                                                                 gint64                     rtime);
 
 /* Очистка источников данных */
static void       hyscan_hsx_converter_sources_clear            (HyScanHSXConverter        *self);

/* Инит источников данных */
static gboolean   hyscan_hsx_converter_sources_init             (HyScanHSXConverter        *self,
                                                                 HyScanDB                  *db,
                                                                 const gchar               *project_name,
                                                                 const gchar               *track_name);

/* Очистка выходных структур */
static void       hyscan_hsx_converter_out_clear                (HyScanHSXConverter        *self);

/* Инит выходных структур */
static gboolean   hyscan_hsx_converter_out_init                 (HyScanHSXConverter        *self,
                                                                 const gchar               *project_name,
                                                                 const gchar               *track_name);

/* Запись заголовка файла */
static gboolean   hyscan_hsx_converter_make_header              (HyScanHSXConverter        *self,
                                                                 const gchar               *project_name);

/* Поток, проходащий по данным плеера */
static gpointer   hyscan_hsx_converter_exec                     (HyScanHSXConverter        *self);

/* Посылка сигнала с процентным выполнением */
static gboolean   hyscan_hsx_converter_exec_emit                (HyScanHSXConverter        *self,
                                                                 gint64                     time);

/* Обработчик сигнала prоcess от плеера */
void              hyscan_hsx_converter_proc_cb                  (HyScanHSXConverter        *self,
                                                                 gint64                     time,
                                                                 gpointer                   user_data);

/* Обработчик сигнала range. Фиксирует изменившийся диапазон данных. */
void              hyscan_hsx_converter_range_cb                 (HyScanHSXConverter        *self,
                                                                 gint64                     min,
                                                                 gint64                     max,
                                                                 gpointer                   user_data);

/* Обработка и подготовка акустических данных */
static gboolean   hyscan_hsx_converter_process_acoustic         (HyScanHSXConverter        *self,
                                                                 gint64                     time);

/* Обработка и подготовка данных NMEA */
static gboolean   hyscan_hsx_converter_process_nmea             (HyScanHSXConverter        *self,
                                                                 gint64                     time);

/* Определение буквы зоны UTM */
static gchar*     utm_letter_designator                         (const gdouble              lat);

/* Определение номера зоны UTM */
static guint      hyscan_hsx_converter_zone_number              (const gdouble              lat,
                                                                 const gdouble              lon,
                                                                 gint                      *err);

/* Преобразование широты и долготы из долей градусов в UTM координаты */
static gint       hyscan_hsx_converter_latlon2dst_proj          (HyScanHSXConverter        *self,
                                                                 gdouble                    lat,
                                                                 gdouble                    lon,
                                                                 gdouble                   *easting,
                                                                 gdouble                   *northing,
                                                                 gint                      *zone_num,
                                                                 gchar                    **zone);

/* Запись данных в файл */
static gboolean   hyscan_hsx_converter_send_out                 (HyScanHSXConverter        *self);

/* Очистка выходного буфера */
static void       hyscan_hsx_converter_clear_out_data           (HyScanHSXConverter        *self);

static guint      hyscan_hsx_converter_signals[SIGNAL_LAST]       = { 0 };
static guint      hyscan_hsx_converter_player[SIGNAL_PLAYER_LAST] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (HyScanHSXConverter, hyscan_hsx_converter, G_TYPE_OBJECT)

static void
hyscan_hsx_converter_class_init (HyScanHSXConverterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = hyscan_hsx_converter_object_constructed;
  object_class->finalize = hyscan_hsx_converter_object_finalize;
  object_class->set_property = hyscan_hsx_converter_set_property;

  g_object_class_install_property (object_class, PROP_RESULT_PATH,
    g_param_spec_string ("result-path", "ResultPath", "ResultPath", NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

   /**
   * HyScanHSXConverter::exec:
   * @converter: указатель на #HyScanHSXConverter
   * @percent: процент выполнения конвертации
   *
   * Сигнал ::exec посылается каждый раз при обработке данных по времени time, от сигнала
   * HyScanDataPlayer::process.
   *
   */
  hyscan_hsx_converter_signals[SIGNAL_EXEC] =
    g_signal_new ("exec", HYSCAN_TYPE_HSX_CONVERTER, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__INT, G_TYPE_NONE, 1, G_TYPE_INT);

  /**
   * HyScanHSXConverter::done:
   * @converter: указатель на #HyScanHSXConverter
   *
   * Сигнал ::done посылается один раз при достижении процента выполнения 
   * конвертации в 100%. Затем поток конвертации завершается.
   *
   */
  hyscan_hsx_converter_signals[SIGNAL_DONE] =
    g_signal_new ("done", HYSCAN_TYPE_HSX_CONVERTER, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
}

static void
hyscan_hsx_converter_init (HyScanHSXConverter *hsx_converter)
{
  hsx_converter->priv = hyscan_hsx_converter_get_instance_private (hsx_converter);
}

static void
hyscan_hsx_converter_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  HyScanHSXConverter *conv = HYSCAN_HSX_CONVERTER (object);
  HyScanHSXConverterPrivate *priv = conv->priv;

  switch (prop_id)
    {
    case PROP_RESULT_PATH:
      priv->out.path = g_value_dup_string (value);
      break;
  
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_hsx_converter_object_constructed (GObject *object)
{
  HyScanHSXConverter *hsx_converter = HYSCAN_HSX_CONVERTER (object);
  HyScanHSXConverterPrivate *priv = hsx_converter->priv;

  priv->player          = hyscan_data_player_new ();
  priv->cache           = HYSCAN_CACHE (hyscan_cached_new (64));
  priv->ampl_factory    = hyscan_factory_amplitude_new (priv->cache);
  priv->max_ampl_value  = HYSCAN_HSX_CONVERTER_DEFAULT_MAX_AMPLITUDE;
  priv->max_rss_size    = HYSCAN_HSX_CONVERTER_DEFAULT_MAX_RSS_SIZE;
  priv->sound_velosity  = HYSCAN_HSX_CONVERTER_DEFAULT_VELOSITY;
  priv->image_prm.black = 0.0;
  priv->image_prm.white = 1.0;
  priv->image_prm.gamma = 1.0;

  memset (&priv->data, 0, sizeof (HyScanHSXConverterOutData));
}

static void
hyscan_hsx_converter_object_finalize (GObject *object)
{
  HyScanHSXConverter *self = HYSCAN_HSX_CONVERTER (object);
  HyScanHSXConverterPrivate *priv = self->priv;

  hyscan_data_player_shutdown (priv->player);
  hyscan_hsx_converter_sources_clear (self);
  hyscan_hsx_converter_out_clear (self);


  g_object_unref (priv->player);
  g_object_unref (priv->cache);
  g_object_unref (priv->ampl_factory);
  pj_free (priv->transform.proj_src);
  pj_free (priv->transform.proj_dst);
  g_free (priv->transform.param_dst);
  g_free (priv->out.path);

  G_OBJECT_CLASS (hyscan_hsx_converter_parent_class)->finalize (object);
}

static void
hyscan_hsx_converter_add_image_prm (gfloat *data,
                                    gint    size,
                                    gfloat  black,
                                    gfloat  white,
                                    gfloat  gamma)
{
  gint i;
  if (size <= 0)
    return;

  for (i = 0; i < size; ++i)
    {
      /* Преобразование уровней. */
      if (data[i] >= white)
        data[i] = 1.0;
      else if (data[i] <= black && data[i] >= 0.0)
        data[i] = 0.0;
      else if (data[i] >= 0)
        data[i] = pow ((data[i] - black) / (white - black), gamma);
    }
  
}

static gfloat* 
hyscan_hsx_converter_pack (gfloat *input,
                           gint    size, 
                           gint    out_size)
{
  gint i, op, koef, r_koef;
  gfloat *out = g_malloc0 (out_size * sizeof (gfloat));

  koef = (gint)(size / out_size + 0.5);

  for (i = op = 0; op < out_size; ++i, ++op)
    {
      gint a = koef * i;
      gfloat sum = 0.0;
      r_koef = 0;

      while (a < size && r_koef < koef)
        {
          sum += input[a++];
          r_koef++;
        }
      out[op] = sum / r_koef;
    }
  return out;
}

static void
hyscan_hsx_converter_clamp_0_1 (gfloat *data,
                                gint    size)
{

  gint i;
  for (i = 0; i < size; ++i)
    data[i] = CLAMP (data[i], 0.0, 1.0);
}

static gint*
hyscan_hsx_converter_float2int (gfloat *data,
                                gint    size, 
                                gint    mult)
{
  gint i;
  gint *out = g_malloc0 (size * sizeof (gint));

  for (i = 0; i < size; ++i)
    out[i] = (gint)(data[i] * (gfloat)mult);

  return out;
}

static guint32
hyscan_hsx_converter_nearest (gint64  time,
                              guint32 lindex,
                              guint32 rindex,
                              gint64  ltime,
                              gint64  rtime)
{
  if (time <= ltime)
    return lindex;
  if (time >= rtime)
    return rindex;

  if (rtime - time < time - ltime)
    return rindex;

  return lindex;
}

static void
hyscan_hsx_converter_sources_clear (HyScanHSXConverter *self)
{
  HyScanHSXConverter *hsx_converter = HYSCAN_HSX_CONVERTER (self);
  HyScanHSXConverterPrivate *priv = hsx_converter->priv;
  gint i = 0;

  hyscan_data_player_clear_channels (priv->player); 
  if (hyscan_hsx_converter_player[SIGNAL_PLAYER_RANGE] != 0)
    g_signal_handler_disconnect (priv->player, hyscan_hsx_converter_player[SIGNAL_PLAYER_RANGE]);

  while (i < HYSCAN_AC_TYPE_LAST)
    g_clear_object (&priv->ampl[i++]);
  
  priv->zero_time = 0;
  if (priv->track_time != NULL)
    g_date_time_unref (priv->track_time);

  i = 0;
  while (i < HYSCAN_HSX_CONVERTER_NMEA_PARSERS_COUNT)
    g_clear_object (&priv->nmea[i++]);

}

static gboolean
hyscan_hsx_converter_sources_init (HyScanHSXConverter *self,
                                   HyScanDB           *db,
                                   const gchar        *project_name,
                                   const gchar        *track_name)
{
  HyScanCache *cache = NULL;
  HyScanHSXConverterPrivate *priv = self->priv;
  gint32 pid = 0, tid = 0, tidp = 0;
  gchar **track_params = NULL;
  gint i = 0;
                            
  cache = priv->cache;

  hyscan_data_player_set_track (priv->player, db, project_name, track_name);

  hyscan_factory_amplitude_set_project (priv->ampl_factory, db, project_name);

  /* Создание объектов амплитуд */
/*  priv->ampl[HYSCAN_AC_TYPE_PORT] = hyscan_factory_amplitude_produce (
                                    priv->ampl_factory, HYSCAN_SOURCE_SIDE_SCAN_PORT);
  priv->ampl[HYSCAN_AC_TYPE_STARBOARD] = hyscan_factory_amplitude_produce (
                                        priv->ampl_factory, HYSCAN_SOURCE_SIDE_SCAN_STARBOARD);*/

  /* Получение времени создания галса */
  pid = hyscan_db_project_open (db, project_name);
  tid = hyscan_db_track_open (db, pid, track_name);
  priv->track_time = hyscan_db_track_get_ctime (db, tid);
  
  {
    tidp = hyscan_db_track_param_open (db, tid);
    track_params = hyscan_db_param_object_list (db, tidp);
    while (track_params[i] != NULL)
      {
        HyScanSourceType  source = HYSCAN_SOURCE_LAST;
        HyScanChannelType type = HYSCAN_CHANNEL_LAST;
        guint             channel = 0;
        hyscan_channel_get_types_by_id (track_params[i], &source, &type, &channel);
        if (type != HYSCAN_CHANNEL_DATA)
          goto skip;

        if (source == HYSCAN_SOURCE_SIDE_SCAN_PORT)
          {
            priv->ampl[HYSCAN_AC_TYPE_PORT] = hyscan_factory_amplitude_produce (
                                              priv->ampl_factory, track_name, source);
          }
        if (source == HYSCAN_SOURCE_SIDE_SCAN_STARBOARD)
          {
            priv->ampl[HYSCAN_AC_TYPE_STARBOARD] = hyscan_factory_amplitude_produce (
                                                   priv->ampl_factory, track_name, source);
          }

        if (source == HYSCAN_SOURCE_NMEA)
          { /* Разбиение по каналам мало похоже на правду */
            if (channel == 1)
              {
                priv->nmea[HYSCAN_NMEA_FIELD_LAT] = HYSCAN_NAV_DATA (
                  hyscan_nmea_parser_new (db, cache, project_name, track_name, 1, 
                  HYSCAN_NMEA_DATA_RMC, HYSCAN_NMEA_FIELD_LAT));

                priv->nmea[HYSCAN_NMEA_FIELD_LON] = HYSCAN_NAV_DATA (
                  hyscan_nmea_parser_new (db, cache, project_name, track_name, 1,
                  HYSCAN_NMEA_DATA_RMC, HYSCAN_NMEA_FIELD_LON));

                priv->nmea[HYSCAN_NMEA_FIELD_SPEED] = HYSCAN_NAV_DATA (
                  hyscan_nmea_parser_new (db, cache, project_name, track_name, 1,
                  HYSCAN_NMEA_DATA_RMC, HYSCAN_NMEA_FIELD_SPEED));
                
                priv->nmea[HYSCAN_NMEA_FIELD_TRACK] = HYSCAN_NAV_DATA (
                  hyscan_nmea_parser_new (db, cache, project_name, track_name, 1,
                  HYSCAN_NMEA_DATA_RMC, HYSCAN_NMEA_FIELD_TRACK));

                priv->nmea[HYSCAN_NMEA_FIELD_HEADING] = HYSCAN_NAV_DATA (
                  hyscan_nmea_parser_new (db, cache, project_name, track_name, 1,
                  HYSCAN_NMEA_DATA_HDT, HYSCAN_NMEA_FIELD_HEADING));

                priv->nmea[HYSCAN_NMEA_FIELD_FIX_QUAL] = HYSCAN_NAV_DATA (
                  hyscan_nmea_parser_new (db, cache, project_name, track_name, 1,
                  HYSCAN_NMEA_DATA_GGA, HYSCAN_NMEA_FIELD_FIX_QUAL));
                
                priv->nmea[HYSCAN_NMEA_FIELD_N_SATS] = HYSCAN_NAV_DATA (
                  hyscan_nmea_parser_new (db, cache, project_name, track_name, 1,
                  HYSCAN_NMEA_DATA_GGA, HYSCAN_NMEA_FIELD_N_SATS));

                priv->nmea[HYSCAN_NMEA_FIELD_HDOP] = HYSCAN_NAV_DATA (
                  hyscan_nmea_parser_new (db, cache, project_name, track_name, 1,
                  HYSCAN_NMEA_DATA_GGA, HYSCAN_NMEA_FIELD_HDOP));

                priv->nmea[HYSCAN_NMEA_FIELD_ALTITUDE] = HYSCAN_NAV_DATA (
                  hyscan_nmea_parser_new (db, cache, project_name, track_name, 1,
                  HYSCAN_NMEA_DATA_GGA, HYSCAN_NMEA_FIELD_ALTITUDE));

              }
            else if (channel == 2)
              {
                priv->nmea[HYSCAN_NMEA_FIELD_DEPTH] = HYSCAN_NAV_DATA (
                  hyscan_nmea_parser_new (db, cache, project_name, track_name, 2,
                  HYSCAN_NMEA_DATA_DPT, HYSCAN_NMEA_FIELD_DEPTH));
              }
          }

      skip:
        i++;
      }
    g_strfreev (track_params);
  }

  hyscan_db_close (db, tid);
  hyscan_db_close (db, pid);
  
  /* Если нет акустики и навигации - конвертировать нечего */
  if ((priv->ampl[HYSCAN_AC_TYPE_PORT] == NULL) &
      (priv->ampl[HYSCAN_AC_TYPE_STARBOARD] == NULL) &
      (priv->nmea[HYSCAN_NMEA_FIELD_LAT] == NULL))
    {
      return FALSE;
    }

  /* Подпишемся, чтобы не пропустить диапазоны от каналов */
  hyscan_hsx_converter_player[SIGNAL_PLAYER_RANGE] = 
    g_signal_connect_swapped (priv->player, "range", 
                              G_CALLBACK (hyscan_hsx_converter_range_cb),
                              self);

  if (hyscan_hsx_converter_player[SIGNAL_PLAYER_RANGE] <= 0)
    return FALSE;

  /* В плеер только те источники, которые есть в галсе */
  if (priv->ampl[HYSCAN_AC_TYPE_PORT] != NULL)
    {
      if (hyscan_data_player_add_channel (priv->player, HYSCAN_SOURCE_SIDE_SCAN_PORT, 1, HYSCAN_CHANNEL_DATA) < 0)
        return FALSE;
    }
  if (priv->ampl[HYSCAN_AC_TYPE_STARBOARD] != NULL)
    {
      if (hyscan_data_player_add_channel (priv->player, HYSCAN_SOURCE_SIDE_SCAN_STARBOARD, 1, HYSCAN_CHANNEL_DATA) < 0)
        return FALSE;
    }
  if (priv->nmea[HYSCAN_NMEA_FIELD_LAT] != NULL)
    {
      if (hyscan_data_player_add_channel (priv->player, HYSCAN_SOURCE_NMEA, 1, HYSCAN_CHANNEL_DATA) < 0)
        return FALSE;
    }
  if (priv->nmea[HYSCAN_NMEA_FIELD_DEPTH] != NULL)
    {
      if (hyscan_data_player_add_channel (priv->player, HYSCAN_SOURCE_NMEA, 2, HYSCAN_CHANNEL_DATA) < 0)
        return FALSE;
    }

  /* Пауза чтобы отработал range с данными пределов */
  g_usleep (G_TIME_SPAN_MILLISECOND * 500);
  return TRUE;
}

static void
hyscan_hsx_converter_out_clear (HyScanHSXConverter *self)
{
  HyScanHSXConverterPrivate *priv = self->priv;

  if (priv->out.out_stream != NULL)
    {
      g_output_stream_flush (priv->out.out_stream, NULL, NULL);
      g_output_stream_close (priv->out.out_stream, NULL, NULL);
      g_clear_object (&priv->out.out_stream);
    }

  g_clear_object (&priv->out.file);
  g_free (priv->out.file_name);
}

static gboolean
hyscan_hsx_converter_out_init (HyScanHSXConverter *self,
                               const gchar        *project_name,
                               const gchar        *track_name)
{
  HyScanHSXConverterPrivate *priv = self->priv;
  hyscan_hsx_converter_out_clear (self);
  gboolean res = FALSE;

  priv->out.file_name = g_strdup_printf ("%s%s%s_%s.HSX", priv->out.path, 
                                         G_DIR_SEPARATOR_S,
                                         project_name, track_name);

  if (g_file_test (priv->out.file_name, G_FILE_TEST_EXISTS))
    res = g_unlink (priv->out.file_name);

  if (res != 0)
    {
      g_warning ("HyScanHSXConverter: can't delete existing '%s'", priv->out.file_name);
      return FALSE;
    }

  priv->out.file = g_file_new_for_path (priv->out.file_name);

  priv->out.out_stream = G_OUTPUT_STREAM (g_file_replace (priv->out.file,
                                          NULL, FALSE, G_FILE_CREATE_NONE,
                                          NULL, NULL));
  if (priv->out.out_stream == NULL)
    {
      g_warning ("HyScanHSXConverter: can't create file %s", priv->out.file_name);
      return FALSE;
    }
  
  /* Очистка выходных данных */
  memset (&priv->data, 0, sizeof (HyScanHSXConverterOutData));
  hyscan_hsx_converter_clear_out_data (self);
  /* Очистка состояния */
  memset (&priv->state, 0, sizeof (priv->state));

  return hyscan_hsx_converter_make_header (self, project_name);
}

static gboolean
hyscan_hsx_converter_make_header (HyScanHSXConverter *self,
                                  const gchar        *project_name)

{
  HyScanHSXConverterPrivate *priv = self->priv;
  GString *data = g_string_new (NULL);
  gchar *wdata = NULL;
  gboolean res = FALSE;
  gchar *add = NULL;
  
  g_string_append (data, "FTP NEW 2\nHSX 8\nVER 12.0.0.0\n");
 
  g_string_append_printf (data, "INF \"hyscan5_user\" \"\" \"Project_%s\" \"\" 0 0 %f\n",
                          project_name, priv->sound_velosity);

  g_string_append_printf (data, "%s%s%s", "ELL WGS-84 6378137.000 298.257223563\n",
                          "DTM 0.00 0.00 0.00 0.00000 0.00000 0.00000 0.00000\n",
                          "HVU 1.0000000000 1.0000000000\n"); 

  add = g_date_time_format (priv->track_time, "%H:%M:%S %m/%d/%y");
  g_string_append_printf (data, "TND %s \n", add);
  g_free (add);

  g_string_append_printf (data, "%s%s%s%s", "DEV 0 100 \"NAV\"\n", "DV2 0 4 0 1\n",
                          "OF2 0 0 0.000000 0.000000 0.000000 0.000000 0.000000 0.000000 0.000000\n",
                          "PRI 0\n");

  g_string_append_printf (data, "%s%s%s", "DEV 1 512 \"SAS_1\"\n", "DV2 1 200 0 1\n",
                          "OF2 1 2 0.000000 0.000000 0.000000 0.000000 0.000000 0.000000 0.000000\n");

  g_string_append_printf (data, "%s%s%s", "DEV 2 32 \"SAS_2\"\n","DV2 2 20 0 1\n",
                          "OF2 2 1 0.000000 0.000000 0.000000 0.000000 0.000000 0.000000 0.000000\n");

  g_string_append_printf (data, "%s%s%s", "DEV 3 16 \"DPH\"\n","DV2 3 10 0 1\n",
                          "OF2 3 0 0.000000 0.000000 0.000000 0.000000 0.000000 0.000000 0.000000\n");

  g_string_append_printf (data, "%s%s%s%s", "DEV 4 32768 \"SSS\"\n","DV2 4 8 0 1\n",
                          "OF2 4 3 0.000000 0.000000 0.000000 0.000000 0.000000 0.000000 0.000000\n",
                          "OF2 4 4 0.000000 0.000000 0.000000 0.000000 0.000000 0.000000 0.000000\n");
  g_string_append_printf (data,"SSI 4 100 %d %d\nEOH\n", priv->max_rss_size, priv->max_rss_size);

  wdata = g_string_free (data, FALSE);
  res = g_output_stream_write_all (priv->out.out_stream, wdata, strlen (wdata), NULL, NULL, NULL);
  g_free (wdata);

  return res;
}

static gpointer
hyscan_hsx_converter_exec (HyScanHSXConverter *self)
{
  HyScanHSXConverterPrivate *priv = self->priv;

  while (g_atomic_int_get (&priv->is_run) == 1)
    {
      hyscan_data_player_seek_next (priv->player);
      g_usleep (G_TIME_SPAN_MILLISECOND * 15);
    }

  return NULL;
}

static gboolean
hyscan_hsx_converter_exec_emit (HyScanHSXConverter *self,
                                gint64              time)
{
  HyScanHSXConverterPrivate *priv = self->priv;
  priv->state.current_percent = (gint) ((time - priv->zero_time) / priv->state.percent_koeff);

  priv->state.current_percent = CLAMP (priv->state.current_percent, 0, 100);
  g_signal_emit (self, hyscan_hsx_converter_signals[SIGNAL_EXEC],
                 0 , priv->state.current_percent);

  if (priv->state.current_percent == 100)
    {
      hyscan_hsx_converter_stop (self);

      g_signal_emit (self, hyscan_hsx_converter_signals[SIGNAL_DONE],
                     0 , priv->state.current_percent);

      return FALSE;
    }
  return TRUE;
}

/* Callback на сигнал process от плеера */
void
hyscan_hsx_converter_proc_cb (HyScanHSXConverter *self,
                              gint64              time,
                              gpointer            user_data)
{

  if (!hyscan_hsx_converter_exec_emit (self, time))
    return;
  
  hyscan_hsx_converter_process_acoustic (self, time);

  hyscan_hsx_converter_process_nmea (self, time);

  hyscan_hsx_converter_send_out (self);

  hyscan_hsx_converter_clear_out_data (self);
}

/* Callback на изменение диапазона данных в плеере */
void
hyscan_hsx_converter_range_cb (HyScanHSXConverter *self,
                               gint64              min,
                               gint64              max,
                               gpointer            user_data)
{
  HyScanHSXConverterPrivate *priv = self->priv;

  priv->state.min_time = (priv->state.min_time == 0 || priv->state.min_time > min) ? 
                          min : priv->state.min_time;

  priv->zero_time = priv->state.min_time;

  priv->state.max_time = (priv->state.max_time == 0 || priv->state.max_time < max) ?
                          max : priv->state.max_time;

  priv->state.percent_koeff = (gdouble)(priv->state.max_time - priv->state.min_time) / 100.0;
}

static gboolean
hyscan_hsx_converter_process_acoustic (HyScanHSXConverter *self,
                                       gint64              time)
{
  HyScanHSXConverterPrivate *priv = self->priv;
  HyScanHSXConverterOutData *out_data = &priv->data;
  guint rss_elem_cnt = priv->max_rss_size;  
  guint i = 0;
  gboolean is_acoustic_time = FALSE;
  guint32 li = 0, ri = 0;

  typedef struct
  {
    gboolean exist;
    gboolean noise;
    guint32  points;
    const gfloat *c_ampls;
    gfloat *ampls;
    gfloat *ampl_cut;
    gint64  time;
  }InData;  /* Параметры входных данных */
  
  InData s[HYSCAN_AC_TYPE_LAST];
  memset (&s[0], 0, sizeof (InData));
  memset (&s[1], 0, sizeof (InData));

  /* Проверка на акустическое время */ 
  if (priv->ampl[HYSCAN_AC_TYPE_PORT] == NULL)
    goto next;

  if (hyscan_amplitude_find_data (priv->ampl[HYSCAN_AC_TYPE_PORT], time, &li, &ri, NULL, NULL) !=
      HYSCAN_DB_FIND_FAIL)
    {
      is_acoustic_time |= ((li == ri) ? TRUE : FALSE);
    }

next:
  if (priv->ampl[HYSCAN_AC_TYPE_STARBOARD] == NULL)
    goto next2;

  if (hyscan_amplitude_find_data (priv->ampl[HYSCAN_AC_TYPE_STARBOARD], time, &li, &ri, NULL, NULL) !=
      HYSCAN_DB_FIND_FAIL)
    {
      is_acoustic_time |= ((li == ri) ? TRUE : FALSE);
    }

next2:
  if (!is_acoustic_time)
    return FALSE;

  /* Получение данных */
  for (i = 0; i < HYSCAN_AC_TYPE_LAST; i++)
    {
      HyScanDBFindStatus find_status;
      guint32 lindex = 0, rindex = 0, index = 0;
      gint64 ltime = 0, rtime = 0;

      /* Поиск индекса данных для указанного момента времени.*/
      find_status = hyscan_amplitude_find_data (priv->ampl[i], time, &lindex, &rindex, &ltime, &rtime);

      if (find_status == HYSCAN_DB_FIND_FAIL)
        continue;

      index = hyscan_hsx_converter_nearest (time, lindex, rindex, ltime, rtime);

      s[i].c_ampls = hyscan_amplitude_get_amplitude (priv->ampl[i], NULL, index, &s[i].points, &s[i].time, &s[i].noise);

      if (s[i].c_ampls == NULL)
        continue;

      /* Уже использовали это  время */
      if (out_data->acoustic[i].in_time == s[i].time)
        continue;

      s[i].exist = !s[i].noise;
    }

  /* Минимальный размер массива, к которому нужно привести два массива */
  if (s[0].exist && s[1].exist)
    rss_elem_cnt = MIN (rss_elem_cnt, MIN (s[0].points, s[1].points));
  else if (s[0].exist && !s[1].exist)
    rss_elem_cnt = MIN (rss_elem_cnt, s[0].points);
  else if (!s[0].exist && s[1].exist)
    rss_elem_cnt = MIN (rss_elem_cnt, s[1].points);
  else
    return FALSE; /* Данные невалидны для текущей итерации */

  /* Для HYPACK - подыгрыш данных глубины со временем акустических данных */
  if (priv->nmea[HYSCAN_NMEA_FIELD_DEPTH] != NULL)
    {
      HyScanDBFindStatus find_status = HYSCAN_DB_FIND_FAIL;
      guint32 lindex = 0, rindex = 0, index = 0;
      gint64 ltime = 0, rtime = 0, got_time = 0;
      gdouble v = 0.0;
      GDateTime *added_time;

      find_status = hyscan_nav_data_find_data (priv->nmea[HYSCAN_NMEA_FIELD_DEPTH], 
                                               time, &lindex, &rindex, &ltime, &rtime);

      if (find_status == HYSCAN_DB_FIND_FAIL)
        goto skip;

      index = hyscan_hsx_converter_nearest (time, lindex, rindex, ltime, rtime);

      if (hyscan_nav_data_get (priv->nmea[HYSCAN_NMEA_FIELD_DEPTH], NULL, index, &got_time, &v))
        {
          /* Подмена полученного времени, временем от акустики */
          i = s[HYSCAN_AC_TYPE_PORT].exist ? HYSCAN_AC_TYPE_PORT : HYSCAN_AC_TYPE_STARBOARD;
          added_time = add_microsec (priv->track_time, s[i].time - priv->zero_time);
          out_data->depth_time = get_seconds (added_time);
          g_date_time_unref (added_time);
          out_data->depth = v;
        }
    }

  skip:
  /* Пересчет акустики для выходного формата */
  for (i = 0; i < HYSCAN_AC_TYPE_LAST; i++)
    { 
      gfloat fs;
      gint *i_port_amp = NULL;
      HyScanAcousticDataInfo acoustic_info;
      GDateTime *added_time;

      if (!s[i].exist)
        continue;

      acoustic_info = hyscan_amplitude_get_info (priv->ampl[i]);
      fs = acoustic_info.data_rate;

      s[i].ampls = g_memdup (s[i].c_ampls, s[i].points * sizeof (s[i].c_ampls[i]));

      /*1 - корректировка значений по black/white/gamma */
      hyscan_hsx_converter_add_image_prm (s[i].ampls, s[i].points,
                                          priv->image_prm.black,
                                          priv->image_prm.white,
                                          priv->image_prm.gamma);

      /* 2 - сжатие данных до максимального количества элементов */
      if (s[i].points > rss_elem_cnt)
        {
          s[i].ampl_cut = hyscan_hsx_converter_pack (s[i].ampls, s[i].points, rss_elem_cnt);
          g_free (s[i].ampls);
          fs = acoustic_info.data_rate * rss_elem_cnt / s[i].points;
        }
      else
        {
          s[i].ampl_cut = s[i].ampls;
        }

      /* 3 - нормировка от 0 до 1 */
      hyscan_hsx_converter_clamp_0_1 (s[i].ampl_cut, rss_elem_cnt);

      /* 4 - преобразjвание из float to int */
      i_port_amp = hyscan_hsx_converter_float2int (s[i].ampl_cut, rss_elem_cnt, priv->max_ampl_value);

      g_free (s[i].ampl_cut);
      added_time = add_microsec (priv->track_time, s[i].time - priv->zero_time);
      out_data->acoustic[i].time = get_seconds (added_time);
      g_date_time_unref (added_time);

      out_data->acoustic[i].in_time = s[i].time;
      out_data->acoustic[i].data = i_port_amp;
      out_data->acoustic[i].size = rss_elem_cnt;
      out_data->cut_fs = fs;
    }

  return TRUE;
}

static gboolean
hyscan_hsx_converter_process_nmea (HyScanHSXConverter *self,
                                   gint64              time)
{
  guint32 index = 0, rindex = 0;
  gint i = 0;
  HyScanDBFindStatus find_status;
  HyScanHSXConverterPrivate *priv = self->priv;
  HyScanHSXConverterOutData *out_data = &priv->data;
  static gint64 prev_time[HYSCAN_HSX_CONVERTER_NMEA_PARSERS_COUNT] = {0};

  typedef struct
  {
    gboolean  exist;
    gdouble   val;
    gint64    time;
  }InData;
  
  InData s[HYSCAN_HSX_CONVERTER_NMEA_PARSERS_COUNT];
  while (i < HYSCAN_HSX_CONVERTER_NMEA_PARSERS_COUNT)
    memset (&s[i++], 0, sizeof (InData));
 
  i = 0;
  while (i < HYSCAN_HSX_CONVERTER_NMEA_PARSERS_COUNT)
    {
      /* Пропуск неинициализированных парсеров */
      if (priv->nmea[i] == NULL)
        goto skip;
      /* Глубину пропустим - её подыгрывали в акустике */
      if (i == HYSCAN_NMEA_FIELD_DEPTH)
        goto skip;

      find_status = HYSCAN_DB_FIND_FAIL;

      find_status = hyscan_nav_data_find_data (priv->nmea[i], 
                                               time, &index, &rindex, NULL, NULL);

      if (find_status == HYSCAN_DB_FIND_FAIL)
        goto skip;

      if (find_status != HYSCAN_DB_FIND_OK)
        goto skip;

      s[i].exist = hyscan_nav_data_get (priv->nmea[i],
                                        NULL,
                                        index,
                                        &s[i].time,
                                        &s[i].val);

      /* Защита от повторений данных */
      if (prev_time[i] == s[i].time)
        s[i].exist = FALSE;

      prev_time[i] = s[i].time;

    skip:
      i++;
    }
    
  if (s[HYSCAN_NMEA_FIELD_LAT].exist & s[HYSCAN_NMEA_FIELD_LON].exist)
    {
      gdouble x = UNINIT, y = UNINIT;
      GDateTime *added_time;
      /* Преобразование из широты и долготы в UTM координаты */     
      if (hyscan_hsx_converter_latlon2dst_proj (self, s[HYSCAN_NMEA_FIELD_LAT].val,
                                                s[HYSCAN_NMEA_FIELD_LON].val,
                                                &x, &y, NULL, NULL) == 0)
        {
          out_data->x = x;
          out_data->y = y;
        }

      added_time = add_microsec (priv->track_time, s[HYSCAN_NMEA_FIELD_LAT].time - priv->zero_time);     
      out_data->rmc_time = get_seconds (added_time);
      g_date_time_unref (added_time);
      out_data->x = x;
      out_data->y = y;
    }

  if (s[HYSCAN_NMEA_FIELD_SPEED].exist)
    out_data->speed_knots = s[HYSCAN_NMEA_FIELD_SPEED].val;

  if (s[HYSCAN_NMEA_FIELD_TRACK].exist)
    {
      GDateTime *added_time;
      added_time = add_microsec (priv->track_time, s[HYSCAN_NMEA_FIELD_TRACK].time - priv->zero_time);
      out_data->hdt_time = get_seconds (added_time);
      g_date_time_unref (added_time);
      out_data->tracking = s[HYSCAN_NMEA_FIELD_TRACK].val;
    }

  if (s[HYSCAN_NMEA_FIELD_HEADING].exist)
    {
      GDateTime *added_time;
      added_time = add_microsec (priv->track_time, s[HYSCAN_NMEA_FIELD_HEADING].time - priv->zero_time);
      out_data->hdt_time = get_seconds (added_time);
      g_date_time_unref (added_time);

      out_data->heading = s[HYSCAN_NMEA_FIELD_HEADING].val;
    }

  if (s[HYSCAN_NMEA_FIELD_FIX_QUAL].exist)
    {
      GDateTime *added_time;
      added_time = add_microsec (priv->track_time, s[HYSCAN_NMEA_FIELD_FIX_QUAL].time - priv->zero_time);
      out_data->gga_time = get_seconds (added_time);
      g_date_time_unref (added_time);

      out_data->quality = s[HYSCAN_NMEA_FIELD_FIX_QUAL].val;
    }

  if (s[HYSCAN_NMEA_FIELD_N_SATS].exist)
    out_data->sat_count = s[HYSCAN_NMEA_FIELD_N_SATS].val;

  if (s[HYSCAN_NMEA_FIELD_HDOP].exist)
    out_data->hdop_gps = s[HYSCAN_NMEA_FIELD_HDOP].val;

  if (s[HYSCAN_NMEA_FIELD_ALTITUDE].exist)
    out_data->altitude = s[HYSCAN_NMEA_FIELD_ALTITUDE].val;

  if (s[HYSCAN_NMEA_FIELD_DEPTH].exist)
    {
      GDateTime *added_time;
      added_time = add_microsec (priv->track_time, s[HYSCAN_NMEA_FIELD_DEPTH].time - priv->zero_time);
      out_data->depth_time = get_seconds (added_time);
      g_date_time_unref (added_time);

      out_data->depth = s[HYSCAN_NMEA_FIELD_DEPTH].val;
    }

  return TRUE;
}

static gchar* 
utm_letter_designator (const gdouble lat)
{
  /* 
    This routine determines the correct UTM letter designator for the given latitude
    returns "Z" if latitude is outside the UTM limits of 84N to 80S
    Written by Chuck Gantz - chuck.gantz@globalstar.com
    rewritten for Qt/C++ by Rainer Sieger - rsieger@pangaea.de
    rewritten for Glib/c by Maxim Pylaev - maks_shv@mail.ru
  */
    if (lat > 84)
      return g_strdup ("Z");
    if (lat < -80)
       return g_strdup ("Z");

    if ((84 >= lat ) && (lat >= 72))
      return g_strdup ("X");
    if ((72 > lat ) && (lat >= 64))
      return g_strdup ("W");
    if ((64 > lat ) && (lat >= 56)) 
      return g_strdup ("V");
    if ((56 > lat ) && (lat >= 48)) 
      return g_strdup ("U");
    if ((48 > lat ) && (lat >= 40)) 
      return g_strdup ("T");
    if ((40 > lat ) && (lat >= 32)) 
      return g_strdup ("S");
    if ((32 > lat ) && (lat >= 24)) 
      return g_strdup ("R");
    if ((24 > lat ) && (lat >= 16)) 
      return g_strdup ("Q");
    if ((16 > lat ) && (lat >= 8)) 
      return g_strdup ("P");
    if ((8 > lat) && (lat >= 0)) 
      return g_strdup ("N");
    if ((0 > lat) && (lat >= -8)) 
      return g_strdup ("M");
    if ((-8 > lat) && (lat >= -16)) 
      return g_strdup ("L");
    if ((-16 > lat) && (lat >= -24)) 
      return g_strdup ("K");
    if ((-24 > lat) && (lat >= -32)) 
      return g_strdup ("J");
    if ((-32 > lat) && (lat >= -40)) 
      return g_strdup ("H");
    if ((-40 > lat) && (lat >= -48)) 
      return g_strdup ("G");
    if ((-48 > lat) && (lat >= -56)) 
      return g_strdup ("F");
    if ((-56 > lat) && (lat >= -64)) 
      return g_strdup ("E");
    if ((-64 > lat) && (lat >= -72)) 
      return g_strdup ("D");
    if ((-72 > lat) && (lat >= -80)) 
      return g_strdup ("C");

    /*This is here as an error flag to show that the Latitude is outside the UTM limits*/
    return g_strdup ("Z"); 
}

static guint
hyscan_hsx_converter_zone_number (const gdouble  lat,
                                  const gdouble  lon,
                                  gint          *err)
{
  guint zone_number = 0;
  gint er = 0;

  if ((lat < -80.) || (lat > 84.))
    er = -1;

  if ((lon < -180.) || ( lon >= 180.))
    er = -2;

  if (er != 0)
    {
      if (err != NULL)
        *err = er;
      return zone_number;
    }

  zone_number = (gint)((lon + 180.) / 6.) + 1;

  if ((lat >= 56.0) && (lat < 64.0) && ( lon >= 3.0) && ( lon < 12.0))
    zone_number = 32;

  /* Special zones for Svalbard */
  if ((lat >= 72.0) && (lat < 84.0))
    {
      if ((lon >= 0.0) && (lon <  9.0)) 
        zone_number = 31;
      else if ((lon >= 9.0) && (lon < 21.0))
        zone_number = 33;
      else if ((lon >= 21.0) && (lon < 33.0))
        zone_number = 35;
      else if ((lon >= 33.0) && (lon < 42.0))
        zone_number = 37;
    }

  if (err != NULL)
    *err = er;

  return zone_number;
}

static gint
hyscan_hsx_converter_latlon2dst_proj (HyScanHSXConverter  *self,
                                      gdouble              lat,
                                      gdouble              lon,
                                      gdouble             *easting,
                                      gdouble             *northing,
                                      gint                *zone_num,
                                      gchar              **zone)
{
  HyScanHSXConverterPrivate *priv = self->priv;
  gchar *init_str = NULL;
  gint err = 0;
  gint _zone = 0;
  gdouble x, y;

  _zone = hyscan_hsx_converter_zone_number (lat, lon, &err);
  if (err != 0)
    return err;

  if ((priv->transform.proj_dst != NULL) && (_zone == priv->transform.zone_number))
    goto calc;

  g_clear_pointer (&priv->transform.proj_dst, pj_free);

  init_str = g_strdup_printf ("%s +zone=%i", priv->transform.param_dst, _zone);
  priv->transform.proj_dst = pj_init_plus (init_str);
  g_free (init_str);
  g_return_val_if_fail (priv->transform.proj_dst != NULL, -1);

  priv->transform.zone_number = _zone;

calc:
  x = lon * DEG_TO_RAD;
  y = lat * DEG_TO_RAD;

  if ((err = pj_transform (priv->transform.proj_src,
                           priv->transform.proj_dst,
                           1, 1, &x, &y, NULL )) != 0)
    {
      g_warning ("HyScanHSXConverter: can`t convert lat/lon coord's' %f %f to UTM", lat, lon);
      return err;
    }

  if (zone_num != NULL)
    *zone_num = _zone;
  if (zone != NULL)
    *zone     = utm_letter_designator (lat);

  *easting = x;
  *northing = y;
  return err;
}

static gboolean
hyscan_hsx_converter_send_out (HyScanHSXConverter *self)
{
  HyScanHSXConverterPrivate *priv = self->priv;
  HyScanHSXConverterOutData *out_data = &priv->data;
  gsize written = 0;
  guint i = 0, j = 0;
  gboolean res = FALSE;
  gdouble depth = 0.0;
  HyScanHSXConverterOutData *od = out_data;
  
  /* Определяю индекс борта с данными или пропускаю RSS строку */ 
  if (od->acoustic[HYSCAN_AC_TYPE_PORT].size == 0 && od->acoustic[HYSCAN_AC_TYPE_STARBOARD].size == 0)
    goto next;
  else
    i = (od->acoustic[HYSCAN_AC_TYPE_PORT].size != 0) ? HYSCAN_AC_TYPE_PORT : HYSCAN_AC_TYPE_STARBOARD;

  depth = (od->depth == UNINIT) ? 0.0 : od->depth;
  res = g_output_stream_printf (priv->out.out_stream,
                                &written, NULL, NULL,
                                "RSS 4 %.3f 100 %d %d %.2f 0 %.2f %.2f 0 %d %d 0\n", 
                                od->acoustic[i].time,
                                od->acoustic[HYSCAN_AC_TYPE_PORT].size,
                                od->acoustic[HYSCAN_AC_TYPE_STARBOARD].size,
                                od->sound_velosity,
                                depth,
                                od->cut_fs,
                                priv->max_ampl_value,
                                16 - (gint) round (log2 (priv->max_ampl_value)));
  if (!res)
    return FALSE;

  for (; i < HYSCAN_AC_TYPE_LAST; i++)
    {
      GString *str = g_string_new (NULL);
      gchar *words;

      for (j = 0; j < od->acoustic[i].size; j++)
        g_string_append_printf (str, "%d ",od->acoustic[i].data[j]);

      words = g_string_free (str, FALSE);

      if (!g_output_stream_printf (priv->out.out_stream,
                                   &written, NULL, NULL,
                                   "%s\n", words))
        {
          g_free (words);
          return FALSE;
        }

      g_free (words);
    }

next:
  if (od->depth != UNINIT && od->depth_time != UNINIT)
    {
      if (!g_output_stream_printf (priv->out.out_stream,
                                   &written, NULL, NULL,
                                   "EC1 3 %.3f %.2f\n", od->depth_time, od->depth))
        {
          return FALSE;
        }
    }

  if (od->altitude != UNINIT || od->roll != UNINIT || od->pitch != UNINIT)
    {
      gdouble pitch = (od->pitch == UNINIT) ? 0.0 : od->pitch;
      gdouble roll  = (od->roll == UNINIT) ? 0.0 : od->roll;
      gdouble alt   = (od->altitude == UNINIT) ? 0.0 : od->altitude;

      if (!g_output_stream_printf (priv->out.out_stream,
                                   &written, NULL, NULL,
                                   "HCP 1 %.3f %.2f %.2f %.2f\n",
                                   od->gga_time, alt, roll, pitch))
        {
          return FALSE;
        }
    }

  if (od->gga_time != UNINIT && od->tracking != UNINIT)
    { /* Выбор времени gga, т.к. параметров в этом сообщении больше из GGA строки */
      if (!g_output_stream_printf (priv->out.out_stream,
                                   &written, NULL, NULL,
                                   "GPS 0 %.3f %.1f %.2f %.2f %.0f %.0f\n",
                                   od->gga_time, od->tracking, od->speed_knots,
                                   od->hdop_gps, od->quality, (gdouble)od->sat_count))
        {
          return FALSE;
        }
    }

  if (od->heading != UNINIT)
    {
      if (!g_output_stream_printf (priv->out.out_stream,
                                   &written, NULL, NULL,
                                   "GYR 2 %.3f %.2f\n",
                                   od->hdt_time, od->heading))
        {
          return FALSE;
        }
    }

  if (od->x != UNINIT && od->y != UNINIT)
    {
       if (!g_output_stream_printf (priv->out.out_stream,
                                    &written, NULL, NULL,
                                    "POS 0 %.3f %.2f %.2f\n",
                                    od->rmc_time, od->x, od->y))
        {
          return FALSE;
        }
    }

  return TRUE;
}

static void
hyscan_hsx_converter_clear_out_data (HyScanHSXConverter *self)
{
  gint i = 0;
  HyScanHSXConverterOutData *out_data = &self->priv->data;

  if (out_data == NULL)
    return; 

  while (i < HYSCAN_AC_TYPE_LAST)
  {
    out_data->acoustic[i].time = 0;
    out_data->acoustic[i].size = 0;
    g_clear_pointer (&out_data->acoustic[i++].data, g_free);
  }

  out_data->depth = UNINIT;
  out_data->depth_time = UNINIT;
  out_data->cut_fs = UNINIT;
  out_data->sound_velosity = HYSCAN_HSX_CONVERTER_DEFAULT_VELOSITY;
  out_data->rmc_time = UNINIT;
  out_data->gga_time = UNINIT;
  out_data->hdt_time = UNINIT;
  out_data->heading = UNINIT;
  out_data->quality = UNINIT;
  out_data->speed_knots = UNINIT;
  out_data->hdop_gps = UNINIT;
  out_data->sat_count = UNINIT;
  out_data->altitude = UNINIT;
  out_data->roll = UNINIT;
  out_data->pitch = UNINIT;
  out_data->x = UNINIT;
  out_data->y = UNINIT;
}

/**
 * hyscan_hsx_converter_new:
 * @path: путь к выходным файлам конвертера
 *
 * Функция создает объект конвертера.
 * Путь для выходного файла может быть задан %NULL или "." или "",
 * тогда путь замещается рабочей директорией.
 *  
 * Returns: %HyScanHSXConverter - указатель на объект, или %NULL при наличии ошибок.
 */
HyScanHSXConverter*
hyscan_hsx_converter_new (const gchar *path)
{
  HyScanHSXConverter *converter = NULL;
  gchar *_path;

  if (path == NULL) 
    {
      _path = g_get_current_dir ();
    }
  else if (g_strcmp0 (path, "") == 0 || g_strcmp0 (path,".") == 0)
    {
      _path = g_get_current_dir ();
    }
  else if (g_strcmp0 (path, "..") == 0)
    {
      gchar *end = NULL;
      gchar *_path_1 = NULL;

      _path = g_get_current_dir ();
      end = g_strrstr (_path, G_DIR_SEPARATOR_S);
      if (end == NULL)
        goto exit;
      _path_1 = g_strndup (_path, end - _path);
      g_free (_path);
      _path = _path_1;
    }
  else
    _path = g_strdup (path);

  if (!g_file_test (_path, G_FILE_TEST_IS_DIR))
    {
      g_warning ("HyScanHSXConverter: path '%s' - not exist", _path);
      g_free (_path);
      return converter;
    }

  converter = g_object_new (HYSCAN_TYPE_HSX_CONVERTER, "result-path", _path, NULL);

exit:  
  g_free (_path);

  return converter;
}

/**
 * hyscan_hsx_converter_set_track:
 * @self: указатель на себя
 * @db: указатель на БД
 * @project_name: имя проекта
 * @track_name: имя галса в проекте
 *
 * Функция устанавливает БД, проект и галс для конвертации.
 * Функция перустанавливает источники данных и форирует 
 * новые объекты для записи в выходной файл.
 *
 * Returns: %TRUE если установка выполнена успешно, 
 * %FALSE - если класс занят другим конвертированием или
 * не удалось инициализировать выходной поток.
 */
gboolean
hyscan_hsx_converter_set_track (HyScanHSXConverter *self,
                                HyScanDB           *db,
                                const gchar        *project_name,
                                const gchar        *track_name)
{
  g_return_val_if_fail (HYSCAN_IS_HSX_CONVERTER (self), FALSE);

  g_debug ("HyScanHSXConverter: set track %s", track_name);

  if (db == NULL || project_name == NULL || track_name == NULL)
    {
      g_warning ("HyScanHSXConverter: convert params invalid.");
      return FALSE; 
    }
  if (hyscan_hsx_converter_is_run (self))
    {
      g_warning ("HyScanHSXConverter: convert thread is execute. Stop it before set track.");
      return FALSE;
    }

  hyscan_hsx_converter_sources_clear (self);
  if (!hyscan_hsx_converter_sources_init (self, db, project_name, track_name))
    goto error;
  if (!hyscan_hsx_converter_out_init (self, project_name, track_name))
    goto error;

  return TRUE;

error:
  hyscan_hsx_converter_sources_clear (self);
  hyscan_hsx_converter_out_clear (self);
  g_warning ("HyScanHSXConverter: Can't init. Stop set track.");
  return FALSE; 
}

/**
 * hyscan_hsx_converter_set_max_ampl:
 * @self: указатель на себя
 * @ampl_val: максимальное значение амплитуды.
 *
 * Функция устанавливает максимальное значение амплитуды для 
 * выходного файла. Значение по умолчанию - 8191.
 * 
 */
void
hyscan_hsx_converter_set_max_ampl (HyScanHSXConverter *self,
                                   guint               ampl_val)
{
  HyScanHSXConverterPrivate *priv;
  g_return_if_fail (HYSCAN_IS_HSX_CONVERTER (self));

  priv = self->priv;
  priv->max_ampl_value = ampl_val;

}

/**
 * hyscan_hsx_converter_set_image_prm:
 * @self: указатель на себя
 * @black: значение черной точки от 0.0 до 1.0
 * @white: значение белой точки от 0.0 до 1.0
 * @gamma: значение гаммы точки от 0.0 до 1.0
 *
 * Функция устанавливает параметры коррекции акустических данных
 * перед их преобразованием в выходные данные.
 *
 */
void
hyscan_hsx_converter_set_image_prm (HyScanHSXConverter *self,
                                    gfloat              black,
                                    gfloat              white,
                                    gfloat              gamma)
{
  HyScanHSXConverterPrivate *priv;
  g_return_if_fail (HYSCAN_IS_HSX_CONVERTER (self));
  priv = self->priv;

  black = CLAMP (black, 0.0, 1.0);
  priv->image_prm.black = black;
  white = CLAMP (white, 0.0, 1.0);
  priv->image_prm.white = white;
  gamma = CLAMP (gamma, 0.0, 1.0);
  priv->image_prm.gamma = gamma;
}

/**
 * hyscan_hsx_converter_set_velosity:
 * @self: указатель на себя
 * @velosity: задаваемое значение скорости звука в воде, м/с
 *
 * Функция устанавливает значение скорости звука в воде
 *
 */
void
hyscan_hsx_converter_set_velosity (HyScanHSXConverter *self,
                                   gfloat              velosity)
{
  HyScanHSXConverterPrivate *priv;
  g_return_if_fail (HYSCAN_IS_HSX_CONVERTER (self));
  priv = self->priv;

  priv->sound_velosity = velosity;
}

/**
 * hyscan_hsx_converter_init_crs:
 * @self: указатель на себя
 * @src_projection_id: имя проекции исходной системы координат.
 * @src_datum_id: имя датума исходной системы координат.
 * 
 * Функция устанавливает исходную систему координат навигационных данных.
 * Для установки параметров по умолчанию (proj = latlon, datum = WGS84)
 * следует указать @src_projection_id и @src_datum_id в %NULL.
 *
 * Для параметра @src_projection_id типы проекций можно узнать вызовом 'proj -l'
 * Для параметра @src_datum_id типы проекций можно узнать вызовом 'proj -ld'
 * 
 * Returns: %TRUE если установка выполнена успешно, 
 * %FALSE - иначе.
 */
gboolean
hyscan_hsx_converter_init_crs (HyScanHSXConverter *self,
                               const gchar        *src_projection_id,
                               const gchar        *src_datum_id)
{
  HyScanHSXConverterPrivate *priv;
  gchar *init_str = NULL;

  g_return_val_if_fail (HYSCAN_IS_HSX_CONVERTER (self), FALSE);
  priv = self->priv;

  if (priv->transform.proj_src != NULL)
    pj_free (priv->transform.proj_src);

  if (priv->transform.proj_dst != NULL)
    pj_free (priv->transform.proj_dst);

  if (src_projection_id == NULL && src_datum_id == NULL)
    init_str = g_strdup ("+proj=latlon +datum=WGS84");
  else
    init_str = g_strdup_printf ("+proj=%s +datum=%s", src_projection_id, src_datum_id);

  priv->transform.proj_src = pj_init_plus (init_str);
  g_free (init_str);
  g_return_val_if_fail (priv->transform.proj_src != NULL, FALSE);

  g_free (priv->transform.param_dst);
  /* Здесь сохраним только тип проекции и датум. 
     Зона будет расчитываться динамически для текущих широты и долготы */
  priv->transform.param_dst = g_strdup ("+proj=utm +datum=WGS84");

  return TRUE;
}

/**
 * hyscan_hsx_converter_run:
 * @self: указатель на себя
 *
 * Функция запускает процес конвертации данных.
 *
 * Returns: %TRUE поток конвертации запущен, 
 * %FALSE - поток конвертации не запущен.
 */
gboolean
hyscan_hsx_converter_run (HyScanHSXConverter *self)
{
  HyScanHSXConverterPrivate *priv;
  g_return_val_if_fail (HYSCAN_IS_HSX_CONVERTER (self), FALSE);

  priv = self->priv;

  if (hyscan_hsx_converter_stop (self))
    {
      g_atomic_int_set (&priv->is_run, 1);
      priv->conv_thread = g_thread_new ("hs-hsx-conv",
                                       (GThreadFunc)hyscan_hsx_converter_exec,
                                       self);

      /* Оформляю подписку на плеер */
      hyscan_hsx_converter_player[SIGNAL_PLAYER_PROCESS] =  
        g_signal_connect_swapped (priv->player, "process", 
                                  G_CALLBACK (hyscan_hsx_converter_proc_cb),
                                  self);
      g_debug ("HyScanHSXConverter: convert thread started");
    }

  return (priv->conv_thread != NULL);
}

/**
 * hyscan_hsx_converter_stop:
 * @self: указатель на себя
 *
 * Функция останавливает поток конвертации данных.
 *
 * Returns: %TRUE поток конвертации остановлен, 
 * %FALSE - поток конвертации не остановлен.
 */
gboolean
hyscan_hsx_converter_stop (HyScanHSXConverter *self)
{
  HyScanHSXConverterPrivate *priv;
  g_return_val_if_fail (HYSCAN_IS_HSX_CONVERTER (self), FALSE);

  priv = self->priv;
 /* Остановим поток */
  if (NULL != priv->conv_thread)
    {
      /* Завершим поток */
      g_atomic_int_set (&priv->is_run, 0);
      g_thread_join (priv->conv_thread);
      priv->conv_thread = NULL;

      if (hyscan_hsx_converter_player[SIGNAL_PLAYER_PROCESS] != 0)
        g_signal_handler_disconnect (priv->player, hyscan_hsx_converter_player[SIGNAL_PLAYER_PROCESS]); 

      g_debug ("HyScanHSXConverter: convert thread joined");
    }
  return (priv->conv_thread == NULL);
}

/**
 * hyscan_hsx_converter_is_run:
 * @self: указатель на себя
 *
 * Функция проверяет работоу потока конвертации данных.
 * Returns: %TRUE - конвертация выполняется, %FALSE - конвертация не выполняется.
 */
gboolean
hyscan_hsx_converter_is_run (HyScanHSXConverter *self)
{
  return g_atomic_int_get (&self->priv->is_run) != 0;
}
