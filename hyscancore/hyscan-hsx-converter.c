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
 * #hyscan_hsx_converter_set_max_ampl - установка максимального занчения 
 * амплитудного отсчета в выходном файле. (Обычно это степень двойки 4096, 8192)
 * #hyscan_hsx_converter_set_image_prm - установка преобразования акустических
 * данных по черной и белой точкам, и гамме. Выполняется при необходимости также перед
 * началом конвертации.
 * #hyscan_hsx_converter_run - запуск потока конвертирования данных.
 * В процессе преобразования данных класс иммитирует сигнал @exec с целочисленным 
 * значением выполнения в процентах.
 * По достижении 100% класс иммитирует сигнал @done - без перезаваемых параметров.
 * #hyscan_hsx_converter_stop - останавливает конвертацию.
 * #hyscan_hsx_converter_is_run - проверка работы потока конвертирования.
 * 
 * Для работы с классом необходимо его создать, указать текущий галс для конвертации.
 * При необходимости установить параметры обработки акустического изображения и 
 * максимальной выходной амплитуды.
 * Подписаться на сигналы, если необходимо или периодически опрашивать класс о наличии
 * конвертирования. 
 * Затем запустить конвертацию.
 * Дождаться её завершения, или принудительно остановить поток.
 * Для конвертации следующего галса выполнить установку проекта и галса.
 * При завершении работы с классом освободить память %g_object_unref.
 */

#include "hyscan-hsx-converter.h"
#include "hyscan-data-player.h"
#include "hyscan-amplitude-factory.h"
#include "hyscan-nmea-parser.h"
#include <hyscan-cached.h>
#include <gio/gio.h>
#include <glib.h>
#include <math.h>
#include <string.h>

#define HYSCAN_HSX_CONVERTER_NMEA_PARSERS_COUNT       14      /* HyScanNMEAField types count */
#define HYSCAN_HSX_CONVERTER_DEFAULT_MAX_AMPLITUDE    8191    /* Максимальное значение амплитуды */
#define HYSCAN_HSX_CONVERTER_DEFAULT_MAX_RSS_SIZE     4096    /* Максимум значений на один борт в RSS строке */
#define HYSCAN_HSX_CONVERTER_DEFAULT_VELOSITY         1500.0  /* Дефолтное значение скорости звука в воде, м/с */
#define UNINIT                                        (-500.0)  /* Неинициализированное значение */


/* Добавка микросекунд в GDateTime */
GDateTime*
add_microsec (GDateTime *dt, gdouble us) 
{
  return g_date_time_add_seconds (dt, us / 1000000.0);
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
  SIGNAL_PLAYER_PROCESS,
  SIGNAL_PLAYER_RANGE,
  SIGNAL_PLAYER_LAST
};

typedef enum
{
  HYSCAN_AC_TYPE_PORT = 0,
  HYSCAN_AC_TYPE_STARBOARD,
  HYSCAN_AC_TYPE_LAST
} AcousticType;

typedef enum
{
  HYSCAN_NMEA_TYPE_RMC = 0,
  HYSCAN_NMEA_TYPE_GGA,
  HYSCAN_NMEA_TYPE_DPT,
  HYSCAN_NMEA_TYPE_HDT,
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

typedef struct 
{

    gchar       *name;
    gdouble      equatorial_radius;
    gdouble      eccentricity_squared;
}Ellipsoid;

static Ellipsoid ellipsoid[24] = 
{
    { "Airy",                   6377563., 0.006670540 },   // 0
    { "Australian National",    6378160., 0.006694542 },   // 1
    { "Bessel 1841",            6377397., 0.006674372 },   // 2
    { "Bessel 1841 (Nambia)",   6377484., 0.006674372 },   // 3
    { "Clarke 1866",            6378206., 0.006768658 },   // 4
    { "Clarke 1880",            6378249., 0.006803511 },   // 5
    { "Everest",                6377276., 0.006637847 },   // 6
    { "Fischer 1960 (Mercury)", 6378166., 0.006693422 },   // 7
    { "Fischer 1968",           6378150., 0.006693422 },   // 8
    { "GRS-67",                 6378160., 0.006694605 },   // 9
    { "GRS-80",                 6378137., 0.00669438002 }, // 10  a = 6378137 m, f = 1/298.257222101, e2 = 0.00669438002
    { "Helmert 1906",           6378200., 0.006693422 },   // 11
    { "Hough",                  6378270., 0.006722670 },   // 12
    { "International",          6378388., 0.006722670 },   // 13
    { "Krassovsky",             6378245., 0.006693422 },   // 14
    { "Modified Airy",          6377340., 0.006670540 },   // 15
    { "Modified Everest",       6377304., 0.006637847 },   // 16
    { "Modified Fischer 1960",  6378155., 0.006693422 },   // 17
    { "South American 1969",    6378160., 0.006694542 },   // 18
    { "WGS-60",                 6378165., 0.006693422 },   // 19
    { "WGS-66",                 6378145., 0.006694542 },   // 20
    { "WGS-72",                 6378135., 0.006694318 },   // 21
    { "WGS-84",                 6378137., 0.00669437999 }, // 22  a = 6378137 m, f = 1/298.257223563, e2 = f(2-f) = 0.00669437999
    { "ETRS-89",                6378137., 0.00669438002 }  // 23  a = 6378137 m, f = 1/298.257222101, e2 = f(2-f) = 0.00669438002
};

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
  gdouble                        sound_velosity;        /* --||-- скорость звука м/с */

  /* Времена фиксации сообщений */
  gdouble                        rmc_time;              /* Время RMC */
  gdouble                        gga_time;              /* Время GGA */
  gdouble                        hdt_time;              /* Время HDT */

  gdouble                        heading;               /* Курс (HDT) */
  gdouble                        tracking;              /* Курс (COG) */
  gdouble                        quality;               /* Качество */
  gdouble                        speed_knots;           /* Скорость носителя в морских узлах */
  gdouble                        hdop_gps;              /* ссс */
  gint                           sat_count;             /* Количество спутников */
  gdouble                        altitude;              /* Высота над геоидом */
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

struct _HyScanHSXConverterPrivate
{
  HyScanCache                   *cache;                 /* Кеш */
  HyScanDataPlayer              *player;                /* Плеер данных */
  GDateTime                     *track_time;            /* Время начала галса */

  HyScanAmplitudeFactory        *ampl_factory;          /* Фабрика амплитудных объектов */
  guint                          max_ampl_value;        /* Максимальное значение амплитуд в выходных данных */
  guint                          max_rss_size;          /* Максимальное количество элементов амплитуд */
  gint64                         zero_time;
  gfloat                         sound_velosity;        /* Скорость звука в воде */
                                                        /* Начальная точка для всех данных */
  HyScanAmplitude               *ampl[HYSCAN_AC_TYPE_LAST];
                                                        /* Инферфейсы для амплитудных данных */
  HyScanHSXConverterImagePrm     image_prm;             /* Структура для коррекции отсчетов амплитуд */

  HyScanNavData                 *nmea[HYSCAN_HSX_CONVERTER_NMEA_PARSERS_COUNT];
                                                        /* Парсеры данных NMEA */
  
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

/* gamma, white, black correction */
static void       hyscan_hsx_converter_add_image_prm            (gfloat                    *data,
                                                                 gint                       size,
                                                                 gfloat                     black,
                                                                 gfloat                     white,
                                                                 gfloat                     gamma);

/* Упаковка в размер out_size */
static gfloat*    hyscan_hsx_converter_pack                     (gfloat                    *input,
                                                                 gint                       size,
                                                                 gint                       out_size);

/* Обрезка от 0 до 1 данных %data */
static void       hyscan_hsx_converter_clamp_0_1                (gfloat                    *data,
                                                                 gint                       size);

/* Преобразование %data  в целочисленный массив */
static gint*      hyscan_hsx_converter_float2int                (gfloat                    *data, 
                                                                 gint                       size, 
                                                                 gint                       mult);

/* Определение индекса, ближайшего по времени */
static guint32    hyscan_hsx_converter_nearest                  (gint64                     time,
                                                                 guint32                    lindex,
                                                                 guint32                    rindex,
                                                                 gint64                     ltime, 
                                                                 gint64                     rtime);

static void       hyscan_hsx_converter_sources_clear            (HyScanHSXConverter        *self);

static gboolean   hyscan_hsx_converter_sources_init             (HyScanHSXConverter        *self,
                                                                 HyScanDB                  *db,
                                                                 const gchar               *project_name,
                                                                 const gchar               *track_name);

static void       hyscan_hsx_converter_out_clear                (HyScanHSXConverter        *self);

static gboolean   hyscan_hsx_converter_out_init                 (HyScanHSXConverter        *self,
                                                                 const gchar               *project_name,
                                                                 const gchar               *track_name);

static gboolean   hyscan_hsx_converter_make_header              (HyScanHSXConverter        *self,
                                                                 const gchar               *project_name);


static gpointer   hyscan_hsx_converter_exec                     (HyScanHSXConverter        *self);

void              hyscan_hsx_converter_proc_cb                  (HyScanHSXConverter        *self,
                                                                 gint64                     time,
                                                                 gpointer                   user_data);

/* Обработчик сигнала range. Фиксирует изменившийся диапазон данных. */
void              hyscan_hsx_converter_range_cb                 (HyScanHSXConverter        *self,
                                                                 gint64                     min,
                                                                 gint64                     max,
                                                                 gpointer                   user_data);

static gboolean   hyscan_hsx_converter_process_acoustic         (HyScanHSXConverter        *self,
                                                                 gint64                     time);

static gboolean   hyscan_hsx_converter_process_nmea             (HyScanHSXConverter        *self,
                                                                 gint64                     time);

static gboolean   hyscan_hsx_converter_send_out                 (HyScanHSXConverter        *self);

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
  priv->ampl_factory    = hyscan_amplitude_factory_new (priv->cache);
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
  gint32 pid = 0, tid = 0;
                            
  cache = priv->cache;

  /* TODO узнать список доступных каналов */
  hyscan_data_player_set_track (priv->player, db, project_name, track_name);

  hyscan_amplitude_factory_set_track (priv->ampl_factory, db, project_name, track_name);

  /* Создание объектов амплитуд */
  priv->ampl[HYSCAN_AC_TYPE_PORT] = hyscan_amplitude_factory_produce (
                                    priv->ampl_factory, HYSCAN_SOURCE_SIDE_SCAN_PORT);
  priv->ampl[HYSCAN_AC_TYPE_STARBOARD] = hyscan_amplitude_factory_produce (
                                        priv->ampl_factory, HYSCAN_SOURCE_SIDE_SCAN_STARBOARD);

  /* Получение времени создания галса */
  pid = hyscan_db_project_open (db, project_name);
  tid = hyscan_db_track_open (db, pid, track_name);
  priv->track_time = hyscan_db_track_get_ctime (db, tid);
  
  {
    tidp = hyscan_db_track_param_open (db, tid);
    track_params = hyscan_db_param_object_list (db, tidp);
    while (track_params[i] != NULL)
      {
        HyScanSourceType  source;
        HyScanChannelType type;
        guint             channel;
        hyscan_channel_get_types_by_id (track_params[i], &source, &type, &channel);
        if (type != HYSCAN_CHANNEL_DATA)
          goto skip;

        if (source == HYSCAN_SOURCE_SIDE_SCAN_PORT)
          {
            priv->ampl[HYSCAN_AC_TYPE_PORT] = hyscan_amplitude_factory_produce (
                                              priv->ampl_factory, source);
          }
        if (source == HYSCAN_SOURCE_SIDE_SCAN_STARBOARD)
          {
            priv->ampl[HYSCAN_AC_TYPE_STARBOARD] = hyscan_amplitude_factory_produce (
                                                   priv->ampl_factory, source);
          }

        if (source == HYSCAN_SOURCE_NMEA)
          { /* Разбиение по каналам мало похоже на правду */
            if (channel == 1)
              {
                priv->nmea[HYSCAN_NMEA_FIELD_LAT] = HYSCAN_NAV_DATA(
                  hyscan_nmea_parser_new (db, cache, project_name, track_name, 1, 
                  HYSCAN_NMEA_DATA_RMC, HYSCAN_NMEA_FIELD_LAT));

                priv->nmea[HYSCAN_NMEA_FIELD_LON] = HYSCAN_NAV_DATA(
                  hyscan_nmea_parser_new (db, cache, project_name, track_name, 1,
                  HYSCAN_NMEA_DATA_RMC, HYSCAN_NMEA_FIELD_LON));

                priv->nmea[HYSCAN_NMEA_FIELD_SPEED] = HYSCAN_NAV_DATA(
                  hyscan_nmea_parser_new (db, cache, project_name, track_name, 1,
                  HYSCAN_NMEA_DATA_RMC, HYSCAN_NMEA_FIELD_SPEED));
                
                priv->nmea[HYSCAN_NMEA_FIELD_TRACK] = HYSCAN_NAV_DATA(
                  hyscan_nmea_parser_new (db, cache, project_name, track_name, 1,
                  HYSCAN_NMEA_DATA_RMC, HYSCAN_NMEA_FIELD_TRACK));

                priv->nmea[HYSCAN_NMEA_FIELD_HEADING] = HYSCAN_NAV_DATA(
                  hyscan_nmea_parser_new (db, cache, project_name, track_name, 1,
                  HYSCAN_NMEA_DATA_HDT, HYSCAN_NMEA_FIELD_HEADING));

                priv->nmea[HYSCAN_NMEA_FIELD_FIX_QUAL] = HYSCAN_NAV_DATA(
                  hyscan_nmea_parser_new (db, cache, project_name, track_name, 1,
                  HYSCAN_NMEA_DATA_GGA, HYSCAN_NMEA_FIELD_FIX_QUAL));
                
                priv->nmea[HYSCAN_NMEA_FIELD_N_SATS] = HYSCAN_NAV_DATA(
                  hyscan_nmea_parser_new (db, cache, project_name, track_name, 1,
                  HYSCAN_NMEA_DATA_GGA, HYSCAN_NMEA_FIELD_N_SATS));

                priv->nmea[HYSCAN_NMEA_FIELD_HDOP] = HYSCAN_NAV_DATA(
                  hyscan_nmea_parser_new (db, cache, project_name, track_name, 1,
                  HYSCAN_NMEA_DATA_GGA, HYSCAN_NMEA_FIELD_HDOP));

                priv->nmea[HYSCAN_NMEA_FIELD_ALTITUDE] = HYSCAN_NAV_DATA(
                  hyscan_nmea_parser_new (db, cache, project_name, track_name, 1,
                  HYSCAN_NMEA_DATA_GGA, HYSCAN_NMEA_FIELD_ALTITUDE));

              }
            else if (channel == 2)
              {
                priv->nmea[HYSCAN_NMEA_FIELD_DEPTH] = HYSCAN_NAV_DATA(
                  hyscan_nmea_parser_new (db, cache, project_name, track_name, 2,
                  HYSCAN_NMEA_DATA_DPT, HYSCAN_NMEA_FIELD_DEPTH));
              }
          }

        g_print ("Param name '%s'\n", track_params[i]);

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
      if (hyscan_data_player_add_channel (priv->player, HYSCAN_SOURCE_NMEA, 2, HYSCAN_CHANNEL_DATA) < 0)
        return FALSE;
    }

  /* Пауза чтобы отработал range с данными пределов  - какая нужна? */
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

  priv->out.file_name = g_strdup_printf ("%s%s%s_%s.HSX", priv->out.path, 
                                         G_DIR_SEPARATOR_S,
                                         project_name, track_name);

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
  return  g_output_stream_write_all (priv->out.out_stream, wdata, strlen (wdata), NULL, NULL, NULL);
}

static gpointer
hyscan_hsx_converter_exec (HyScanHSXConverter *self)
{
  HyScanHSXConverterPrivate *priv = self->priv;

  while (g_atomic_int_get (&priv->is_run) == 1)
    {
      hyscan_data_player_seek_next (priv->player);
      g_usleep (G_TIME_SPAN_MILLISECOND * 20);
    }

  return NULL;
}

/* Callback на сигнал process от плеера */
void
hyscan_hsx_converter_proc_cb (HyScanHSXConverter *self,
                              gint64              time,
                              gpointer            user_data)
{

  HyScanHSXConverterPrivate *priv = self->priv;
  priv->state.current_percent = (gint) ((time - priv->zero_time) / priv->state.percent_koeff);
  /*g_print ("signal_process t=%"G_GINT64_FORMAT"- z0=%"G_GINT64_FORMAT" (%d%%)\n",
           time, priv->zero_time,
           priv->state.current_percent);*/
  priv->state.current_percent = CLAMP (priv->state.current_percent, 0, 100);
  g_signal_emit (self, hyscan_hsx_converter_signals[SIGNAL_EXEC],
                 0 , priv->state.current_percent);

  if (priv->state.current_percent == 100)
    {
      g_signal_emit (self, hyscan_hsx_converter_signals[SIGNAL_DONE],
                     0 , priv->state.current_percent);
      /*g_print ("signal_process (%d%%)= time %"G_GINT64_FORMAT"\n", priv->state.current_percent, time);*/
      hyscan_hsx_converter_stop (self);
      return;
    }
  
  /*g_print ("signal_process =      time %"G_GINT64_FORMAT"\n", time);*/

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

/*  g_print ("min_time %"G_GINT64_FORMAT" max_time %"G_GINT64_FORMAT" percent_koef %f\n", 
           priv->state.min_time,
           priv->state.max_time,
           priv->state.percent_koeff); */
}

static gboolean
hyscan_hsx_converter_process_acoustic (HyScanHSXConverter        *self,
                                       gint64                     time)
{
  HyScanHSXConverterPrivate *priv = self->priv;
  HyScanHSXConverterOutData *out_data = &priv->data;
  guint rss_elem_cnt = priv->max_rss_size;  
  guint i = 0;
  gboolean is_acoustic_time = FALSE;
  guint32 li,ri;

  typedef struct
  {
    gboolean exist;
    gboolean noise;
    guint32  points;
    const gfloat *c_ampls;
    gfloat *ampls;
    gfloat *ampl_cut;
    gint64  time;
  }InData;
  
  InData s[HYSCAN_AC_TYPE_LAST];
  memset (&s[0], 0, sizeof (InData));
  memset (&s[1], 0, sizeof (InData));

  if (hyscan_amplitude_find_data (priv->ampl[HYSCAN_AC_TYPE_PORT], time, &li, &ri, NULL, NULL) !=
      HYSCAN_DB_FIND_FAIL)
    {
      is_acoustic_time |= ((li == ri) ? TRUE : FALSE);
    }
  if (hyscan_amplitude_find_data (priv->ampl[HYSCAN_AC_TYPE_STARBOARD], time, &li, &ri, NULL, NULL) !=
      HYSCAN_DB_FIND_FAIL)
    {
      is_acoustic_time |= ((li == ri) ? TRUE : FALSE);
    }
  /* Не буду обрабатывать по времени неакустических данных (например времени от NMEA) */
  if (!is_acoustic_time)
    return FALSE;

  for (; i < HYSCAN_AC_TYPE_LAST; i++)
    {
      HyScanDBFindStatus find_status;
      guint32 lindex = 0, rindex = 0, index = 0;
      gint64 ltime = 0, rtime = 0;

      /* Поиск индекса данных для указанного момента времени.*/
      find_status = hyscan_amplitude_find_data (priv->ampl[i], time, &lindex, &rindex, &ltime, &rtime);

      if (find_status == HYSCAN_DB_FIND_FAIL)
        continue;

      index = hyscan_hsx_converter_nearest (time, lindex, rindex, ltime, rtime);

      s[i].c_ampls = hyscan_amplitude_get_amplitude (priv->ampl[i], index, &s[i].points, &s[i].time, &s[i].noise);

      if (s[i].c_ampls == NULL)
        continue;

      /* Уже использовали это  время */
      if (out_data->acoustic[i].in_time == s[i].time)
        {
          // g_print ("REPEAT ampl[%d][%d] in_time %"G_GINT64_FORMAT" got %"G_GINT64_FORMAT"\n", i, s[i].points, time,  s[i].time);
          continue;
        }

      s[i].exist = !s[i].noise;

      // g_print ("ampl[%d][%d] %"G_GINT64_FORMAT" got %"G_GINT64_FORMAT"\n", i, s[i].points, time, s[i].time);
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

  /* Calc data for board */
  i = 0;
  for (; i < HYSCAN_AC_TYPE_LAST; i++)
    { 
      gfloat fs;
      gint *i_port_amp = NULL;
      HyScanAcousticDataInfo acoustic_info;
      GDateTime *added_time;

      if (!s[i].exist)
        continue;

/*      g_print ("TO Prepare ampl[%d][%d] in_time %"G_GINT64_FORMAT" got %"G_GINT64_FORMAT"\n",
               i, s[i].points, time, s[i].time);*/
      acoustic_info = hyscan_amplitude_get_info (priv->ampl[i]);
      fs = acoustic_info.data_rate;

      s[i].ampls = g_memdup (s[i].c_ampls, s[i].points * sizeof (s[i].c_ampls[i]));

      hyscan_hsx_converter_add_image_prm (s[i].ampls, s[i].points,
                                          priv->image_prm.black,
                                          priv->image_prm.white,
                                          priv->image_prm.gamma);

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

      hyscan_hsx_converter_clamp_0_1 (s[i].ampl_cut, rss_elem_cnt);
      i_port_amp = hyscan_hsx_converter_float2int (s[i].ampl_cut, rss_elem_cnt, priv->max_ampl_value);
      g_free (s[i].ampl_cut);
      added_time = add_microsec (priv->track_time, s[i].time - priv->zero_time);
      
      out_data->acoustic[i].time = get_seconds (added_time);
      /*
      g_print ("ampl[%d] out_time %.3f got %"G_GINT64_FORMAT" diff %"G_GINT64_FORMAT"\n",
               i, out_data->acoustic[i].time, s[i].time, s[i].time - priv->zero_time);
               */
      out_data->acoustic[i].in_time = s[i].time;
      out_data->acoustic[i].data = i_port_amp;
      out_data->acoustic[i].size = rss_elem_cnt;
      out_data->cut_fs = fs;
      
      g_date_time_unref (added_time);
    }

  return TRUE;
}

static gboolean
hyscan_hsx_converter_process_nmea (HyScanHSXConverter        *self,
                                   gint64                     time)
{
  guint32 index;
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
      if (priv->nmea[i] == NULL)
        goto skip;
      find_status = HYSCAN_DB_FIND_FAIL;

      find_status = hyscan_nav_data_find_data (priv->nmea[i], 
                                               time, &index, &rindex, NULL, NULL);

      if (find_status == HYSCAN_DB_FIND_FAIL)
        goto skip;

      if (find_status != HYSCAN_DB_FIND_OK)
        goto skip;

      s[i].exist = hyscan_nav_data_get (priv->nmea[i],
                                        index,
                                        &s[i].time,
                                        &s[i].val);

      /* Защита от одинаковых данных */
      if (prev_time[i] == s[i].time)
        {
          s[i].exist = FALSE;
          // g_print ("REPEAT NMEA time %"G_GINT64_FORMAT"\n", s[i].time);
        }

      prev_time[i] = s[i].time;

      if (s[i].exist)
        ;
        // g_print ("NMEA data %d finded like %d in %"G_GINT64_FORMAT" got %"G_GINT64_FORMAT"\n",
                // i, find_status,time, s[i].time);
    skip:
      i++;
    }
    
  if (s[HYSCAN_NMEA_FIELD_LAT].exist & s[HYSCAN_NMEA_FIELD_LON].exist)
    {
      gdouble x = UNINIT, y = UNINIT;
      GDateTime *added_time;
      if (hyscan_hsx_converter_latlon2utm (s[HYSCAN_NMEA_FIELD_LAT].val,
                                           s[HYSCAN_NMEA_FIELD_LON].val,
                                           22, &x, &y, NULL, NULL) == 0)
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

/* 
 * Convert function was taken from
 * https://github.com/rsieger/LatLongConverter
 * (from ConvertPosition.cpp)
 * and revriten to Glib/C by Maxim Pylaev - maks_shv@mail.ru
*/
static gint
hyscan_hsx_converter_latlon2utm (gdouble      lat,
                                 gdouble      lon,
                                 const gint   ref_ellipsoid,
                                 gdouble     *easting,
                                 gdouble     *northing,
                                 gint        *zone_num,
                                 gchar      **zone)
{

  gint    err                = 0;

  gdouble k0                 = 0.9996;
  gdouble a                  = ellipsoid[ref_ellipsoid].equatorial_radius;
  gdouble ecc_squared        = ellipsoid[ref_ellipsoid].eccentricity_squared;
  gdouble ecc_prime_squared  = (ecc_squared)/(1.-ecc_squared);
  gdouble N, T, C, A, M;

  gdouble deg2rad            = M_PI/180.;

  gdouble lat_rad            = 0.;
  gdouble lan_rad            = 0.;

  gdouble utm_northing       = 0.;
  gdouble utm_easting        = 0.;

  gint    zone_number        = 0;


  if ((lat < -80.) || (lat > 84.))
      err = -1;

  if ((lon < -180.) || ( lon >= 180.))
      err = -2;

  if ( err != 0 )
    {
      *northing = -999.999;
      *easting  = -999.999;
      *zone     = g_strdup ("Z");
      return (err);
    }


    zone_number = (gint)((lon + 180.) / 6.) + 1;

    lat_rad     = lat*deg2rad;
    lan_rad    = lon*deg2rad;

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

    N = a/sqrt(1.-ecc_squared*sin(lat_rad)*sin(lat_rad));
    T = tan(lat_rad)*tan(lat_rad);
    C = ecc_prime_squared*cos(lat_rad)*cos(lat_rad);
    A = cos(lat_rad)*(lan_rad-deg2rad*(((gdouble) zone_number-1.)*6.-180.+3.));
    M = a*((1.-ecc_squared/4.-3.*ecc_squared*ecc_squared/64.-5.*ecc_squared*ecc_squared*ecc_squared/256.)*
        lat_rad-(3.*ecc_squared/8.+3.*ecc_squared*ecc_squared/32.+45.*ecc_squared*ecc_squared*ecc_squared/1024.)*
        sin(2.*lat_rad)+(15.*ecc_squared*ecc_squared/256.+45.*ecc_squared*ecc_squared*ecc_squared/1024.)*
        sin(4.*lat_rad)-(35.*ecc_squared*ecc_squared*ecc_squared/3072.)*sin(6.*lat_rad));

    utm_easting  = k0*N*(A+(1.-T+C)*A*A*A/6.+(5.-18.*T+T*T+72.*C-58.*ecc_prime_squared)*A*A*A*A*A/120.)+500000.;
    utm_northing = k0*(M+N*tan(lat_rad)*(A*A/2.+(5.-T+9.*C+4.*C*C)*A*A*A*A/24.+(61.-58.*T+T*T+600.*C-330.*ecc_prime_squared)*A*A*A*A*A*A/720.));

    if (lat < 0)
      utm_northing += 10000000.; /* 10000000 meter offset for southern hemisphere */

    *northing = utm_northing;
    *easting  = utm_easting;

    if (zone_num != NULL)
      *zone_num = zone_number;
    if (zone != NULL)
      *zone     = utm_letter_designator (lat);

    return (err);
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
>>>>>>> 48a8eef... Исправление расчета времени RSS строки. Дополнение полей данных.
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
 * Функция создает объект конвертера. Выходная папка должна
 * быть передана и не должна быть пустой строкой. 
 * TODO - добавить проверку на папку и её сущестрование.
 */
HyScanHSXConverter*
hyscan_hsx_converter_new (const gchar *path)
{
  HyScanHSXConverter *converter = NULL;

  if (path == NULL)
    return FALSE;
  if (g_strcmp0 (path, "") == 0)
    return FALSE;

  converter = g_object_new (HYSCAN_TYPE_HSX_CONVERTER, "result-path", path, NULL);
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
  g_warning ("HyScanHSXConverter: Can't init . Stop it before set track.");
  return FALSE; 
}

/**
 * hyscan_hsx_converter_set_max_ampl:
 * @self: указатель на себя
 * @ampl_val: максимальное значение амплитуды.
 *
 * Функция устанавливает максимальное значение амплитуды для 
 * выходного файла.
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
 * hyscan_hsx_converter_run:
 * @self: указатель на себя
 *
 * Функция запускает процес конвертации данных.
 *
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

      // g_print ("thread started\n");

      /* Оформляю подписку на плеер */
      hyscan_hsx_converter_player[SIGNAL_PLAYER_PROCESS] =  
        g_signal_connect_swapped (priv->player, "process", 
                                  G_CALLBACK (hyscan_hsx_converter_proc_cb),
                                  self);
      //g_debug ("HyScanHSXConverter: convert thread started");
    }

  return (priv->conv_thread != NULL);
}

/**
 * hyscan_hsx_converter_stop:
 * @self: указатель на себя
 *
 * Функция останавливает поток конвертации данных.
 *
 */
gboolean
hyscan_hsx_converter_stop (HyScanHSXConverter *self)
{
  HyScanHSXConverterPrivate *priv;
  g_return_val_if_fail (HYSCAN_IS_HSX_CONVERTER (self), FALSE);

  priv = self->priv;
 /* Остановим поток опроса гл*/
  if(NULL != priv->conv_thread)
    {
      /* Завершим поток опроса гидролокатора */
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
