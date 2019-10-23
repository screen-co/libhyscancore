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
#define HYSCAN_HSX_CONVERTER_DEFAULT_MAX_RSS_SIZE     2048    /* Максимум значений на один борт в RSS строке */

GDateTime*
add_microsec (GDateTime *dt,gint us) 
{
  return g_date_time_add_seconds (dt, us / 1e6);
}

gdouble
get_seconds (GDateTime *dt)
{
  return ((g_date_time_get_hour (dt) * 3600 + g_date_time_get_minute (dt) * 60 +
           + g_date_time_get_seconds (dt) + g_date_time_get_microsecond (dt) / 1e6));
}

enum
{
  PROP_O,
  PROP_RESULT_PATH
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

/* структура для записи значений в файл */
typedef struct
{
  struct
  {
    gdouble                      time;                  /* Метка времени */
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

  gdouble                        gps_time;              /* Время от датчика GPS */
  gdouble                        heading;               /* Курс (один из track or heading) */
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
                                                        /* Начальная точка для всех данных */
  HyScanAmplitude               *ampl[HYSCAN_AC_TYPE_LAST];
                                                        /* Инферфейсы для амплитудных данных */
  HyScanHSXConverterImagePrm     image_prm;             /* Структура для коррекции отсчетов амплитуд */

  /*HyScanNMEAData                *nmea_data[HYSCAN_NMEA_TYPE_LAST];*/
                                                        /* Объекты данных NMEA */
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

static void       hyscan_hsx_converter_sources_clear            (HyScanHSXConverter        *self);

static gboolean   hyscan_hsx_converter_sources_init             (HyScanHSXConverter        *self,
                                                                 HyScanDB                  *db,
                                                                 const gchar               *project_name,
                                                                 const gchar               *track_name);

static void       hyscan_hsx_converter_out_clear                (HyScanHSXConverter        *self);

static gboolean   hyscan_hsx_converter_out_init                 (HyScanHSXConverter        *self,
                                                                 const gchar               *project_name,
                                                                 const gchar               *track_name);

static void       hyscan_hsx_converter_make_header              (HyScanHSXConverter        *self);


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
                                                                 gint64                     time,
                                                                 HyScanHSXConverterOutData *out_data);

static gboolean   hyscan_hsx_converter_process_nmea             (HyScanHSXConverter        *self,
                                                                 gint64                     time,
                                                                 HyScanHSXConverterOutData *out_data);

static gboolean   hyscan_hsx_converter_send_out                 (HyScanHSXConverter        *self,
                                                                 HyScanHSXConverterOutData *out_data);

static void       hyscan_hsx_converter_clear_out                (HyScanHSXConverterOutData **out_data);

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

  hyscan_hsx_converter_signals[SIGNAL_EXEC] =
    g_signal_new ("exec", HYSCAN_TYPE_HSX_CONVERTER, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__INT, G_TYPE_NONE, 1, G_TYPE_INT);

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
  priv->image_prm.black = 0.0;
  priv->image_prm.white = 1.0;
  priv->image_prm.gamma = 1.0;

  // /* Оформляю подписку на плеер */
  // hyscan_hsx_converter_player[SIGNAL_PLAYER_PROCESS] =  
  //   g_signal_connect_swapped (priv->player, "process", 
  //                             G_CALLBACK (hyscan_hsx_converter_proc_cb),
  //                             hsx_converter);

  // hyscan_hsx_converter_player[SIGNAL_PLAYER_RANGE] = 
  //   g_signal_connect_swapped (priv->player, "range", 
  //                             G_CALLBACK (hyscan_hsx_converter_range_cb),
  //                             hsx_converter);
}

static void
hyscan_hsx_converter_object_finalize (GObject *object)
{
  HyScanHSXConverter *self = HYSCAN_HSX_CONVERTER (object);
  HyScanHSXConverterPrivate *priv = self->priv;

  hyscan_data_player_shutdown (priv->player);
  g_object_unref (priv->player);
  g_object_unref (priv->cache);
  g_object_unref (priv->ampl_factory);

  hyscan_hsx_converter_sources_clear (self);
  hyscan_hsx_converter_out_clear (self);
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
  /*g_print ("koef = %d\n", koef);*/

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
  /*g_print ("clamped\n");*/
  for (i = 0; i < size; ++i)
    data[i] = CLAMP (data[i], 0.0, 1.0);
}

static gint*
hyscan_hsx_converter_float2int (gfloat *data,
                                gint    size, 
                                gint    mult)
{
  gint i;
  /*g_print ("float2int\n");*/
  gint *out = g_malloc0 (size * sizeof (gint));

  for (i = 0; i < size; ++i)
    out[i] = (gint)(data[i] * (gfloat)mult);

  return out;
}

static void
hyscan_hsx_converter_sources_clear (HyScanHSXConverter *self)
{
  HyScanHSXConverter *hsx_converter = HYSCAN_HSX_CONVERTER (self);
  HyScanHSXConverterPrivate *priv = hsx_converter->priv;
  gint i = 0;

  while (i < HYSCAN_AC_TYPE_LAST)
    g_clear_object (&priv->ampl[i++]);
  
  priv->zero_time = 0;
  if (priv->track_time != NULL)
    g_date_time_unref (priv->track_time);

  i = 0;
  while (i < HYSCAN_HSX_CONVERTER_NMEA_PARSERS_COUNT)
    g_clear_object (&priv->nmea[i++]);

  /*i = 0;
  while (i < HYSCAN_NMEA_TYPE_LAST)
    g_clear_object (&priv->nmea_data[i++]);*/
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
  hyscan_data_player_clear_channels (priv->player); 
  if (hyscan_hsx_converter_player[SIGNAL_PLAYER_RANGE] != 0)
    g_signal_handler_disconnect (priv->player, hyscan_hsx_converter_player[SIGNAL_PLAYER_RANGE]);

  hyscan_data_player_set_track (priv->player, db, project_name, track_name);

  hyscan_amplitude_factory_set_track (priv->ampl_factory, db, project_name, track_name);

  /* Создание объектов амплитуд */
  priv->ampl[HYSCAN_AC_TYPE_PORT] = hyscan_amplitude_factory_produce (
                                    priv->ampl_factory, HYSCAN_SOURCE_SIDE_SCAN_PORT);
  priv->ampl[HYSCAN_AC_TYPE_STARBOARD] = hyscan_amplitude_factory_produce (
                                        priv->ampl_factory, HYSCAN_SOURCE_SIDE_SCAN_STARBOARD);

/*  for (;i < HYSCAN_AC_TYPE_LAST; ++i)
    {
      Получение времени первого индекса амплитуд
      if (!hyscan_amplitude_get_range (priv->ampl[i], &lindex, NULL))
        return FALSE;
      if (!hyscan_amplitude_get_amplitude (priv->ampl[i], lindex, NULL, &got_time, NULL))
        return FALSE;
      priv->zero_time[i] = got_time;
    } */

  /* Получение времени создания галса */
  pid = hyscan_db_project_open (db, project_name);
  tid = hyscan_db_track_open (db, pid, track_name);
  priv->track_time = hyscan_db_track_get_ctime (db, tid);
  hyscan_db_close (db, tid);
  hyscan_db_close (db, pid);
  
/* priv->nmea_data[HYSCAN_NMEA_TYPE_RMC] = 
  hyscan_nmea_data_new (db, cache, project_name, track_name, HYSCAN_NMEA_DATA_RMC);

  priv->nmea_data[HYSCAN_NMEA_TYPE_GGA] = 
  hyscan_nmea_data_new (db, cache, project_name, track_name, HYSCAN_NMEA_DATA_GGA);

  priv->nmea_data[HYSCAN_NMEA_TYPE_HDT] = 
  hyscan_nmea_data_new (db, cache, project_name, track_name, HYSCAN_NMEA_DATA_HDT);

  priv->nmea_data[HYSCAN_NMEA_TYPE_DPT] = 
  hyscan_nmea_data_new (db, cache, project_name, track_name, HYSCAN_NMEA_DATA_DPT);
*/
  priv->nmea[HYSCAN_NMEA_FIELD_TIME] = HYSCAN_NAV_DATA(
  hyscan_nmea_parser_new (db, cache, project_name, track_name, 1, HYSCAN_NMEA_DATA_RMC, HYSCAN_NMEA_FIELD_LAT));

  priv->nmea[HYSCAN_NMEA_FIELD_LAT] = HYSCAN_NAV_DATA(
  hyscan_nmea_parser_new (db, cache, project_name, track_name, 1, HYSCAN_NMEA_DATA_RMC, HYSCAN_NMEA_FIELD_LAT));

  priv->nmea[HYSCAN_NMEA_FIELD_LON] = HYSCAN_NAV_DATA(
  hyscan_nmea_parser_new (db, cache, project_name, track_name, 1, HYSCAN_NMEA_DATA_RMC, HYSCAN_NMEA_FIELD_LON));

  priv->nmea[HYSCAN_NMEA_FIELD_SPEED] = HYSCAN_NAV_DATA(
  hyscan_nmea_parser_new (db, cache, project_name, track_name, 1, HYSCAN_NMEA_DATA_RMC, HYSCAN_NMEA_FIELD_SPEED));
  
  priv->nmea[HYSCAN_NMEA_FIELD_TRACK] = HYSCAN_NAV_DATA(
  hyscan_nmea_parser_new (db, cache, project_name, track_name, 1, HYSCAN_NMEA_DATA_RMC, HYSCAN_NMEA_FIELD_TRACK));

  priv->nmea[HYSCAN_NMEA_FIELD_HEADING] = HYSCAN_NAV_DATA(
  hyscan_nmea_parser_new (db, cache, project_name, track_name, 1, HYSCAN_NMEA_DATA_HDT, HYSCAN_NMEA_FIELD_HEADING));

  priv->nmea[HYSCAN_NMEA_FIELD_FIX_QUAL] = HYSCAN_NAV_DATA(
  hyscan_nmea_parser_new (db, cache, project_name, track_name, 1, HYSCAN_NMEA_DATA_GGA, HYSCAN_NMEA_FIELD_FIX_QUAL));
  
  priv->nmea[HYSCAN_NMEA_FIELD_N_SATS] = HYSCAN_NAV_DATA(
  hyscan_nmea_parser_new (db, cache, project_name, track_name, 1, HYSCAN_NMEA_DATA_GGA, HYSCAN_NMEA_FIELD_N_SATS));

  priv->nmea[HYSCAN_NMEA_FIELD_HDOP] = HYSCAN_NAV_DATA(
  hyscan_nmea_parser_new (db, cache, project_name, track_name, 1, HYSCAN_NMEA_DATA_GGA, HYSCAN_NMEA_FIELD_HDOP));

  priv->nmea[HYSCAN_NMEA_FIELD_ALTITUDE] = HYSCAN_NAV_DATA(
  hyscan_nmea_parser_new (db, cache, project_name, track_name, 1, HYSCAN_NMEA_DATA_GGA, HYSCAN_NMEA_FIELD_ALTITUDE));

  priv->nmea[HYSCAN_NMEA_FIELD_DEPTH] = HYSCAN_NAV_DATA(
  hyscan_nmea_parser_new (db, cache, project_name, track_name, 2, HYSCAN_NMEA_DATA_DPT, HYSCAN_NMEA_FIELD_DEPTH));
  
  hyscan_hsx_converter_player[SIGNAL_PLAYER_RANGE] = 
    g_signal_connect_swapped (priv->player, "range", 
                              G_CALLBACK (hyscan_hsx_converter_range_cb),
                              self);

  if (hyscan_hsx_converter_player[SIGNAL_PLAYER_RANGE] <= 0)
    return FALSE;
  if (hyscan_data_player_add_channel (priv->player, HYSCAN_SOURCE_SIDE_SCAN_PORT, 1, HYSCAN_CHANNEL_DATA) < 0)
    return FALSE;
  if (hyscan_data_player_add_channel (priv->player, HYSCAN_SOURCE_SIDE_SCAN_STARBOARD, 1, HYSCAN_CHANNEL_DATA) < 0)
    return FALSE;
  if (hyscan_data_player_add_channel (priv->player, HYSCAN_SOURCE_NMEA, 1, HYSCAN_CHANNEL_DATA) < 0)
    return FALSE;

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

  hyscan_hsx_converter_make_header (self);
  return TRUE;
}

static void
hyscan_hsx_converter_make_header (HyScanHSXConverter *self)
{
  /* TODO Write Header */
}

gboolean
hyscan_hsx_converter_is_run (HyScanHSXConverter *self)
{
  return g_atomic_int_get (&self->priv->is_run) != 0;
}


static gpointer
hyscan_hsx_converter_exec (HyScanHSXConverter *self)
{
  HyScanHSXConverterPrivate *priv = self->priv;

  while (g_atomic_int_get (&priv->is_run) == 1)
    {
      /*g_print ("seek next\n");*/
      hyscan_data_player_seek_next (priv->player);
      g_usleep (G_TIME_SPAN_MILLISECOND * 10);
    }

  return NULL;
}

/* Callback на сигнал process от плеера */
void
hyscan_hsx_converter_proc_cb (HyScanHSXConverter *self,
                              gint64              time,
                              gpointer            user_data)
{
  HyScanHSXConverterOutData *data = g_malloc0 (sizeof (HyScanHSXConverterOutData));
  HyScanHSXConverterPrivate *priv = self->priv;
  priv->state.current_percent = (guint) ((time - priv->zero_time) / priv->state.percent_koeff);
  priv->state.current_percent = CLAMP (priv->state.current_percent, 0, 100);
  g_signal_emit (self, hyscan_hsx_converter_signals[SIGNAL_EXEC],
                 0 , priv->state.current_percent);

  if (priv->state.current_percent == 100)
    {
      g_signal_emit (self, hyscan_hsx_converter_signals[SIGNAL_DONE],
                     0 , priv->state.current_percent);
    }
  
  g_print ("signal_process = time %"G_GINT64_FORMAT"\n", time);

  hyscan_hsx_converter_process_acoustic (self, time, data);

  //hyscan_hsx_converter_process_nmea (self, time, data); 


  hyscan_hsx_converter_send_out (self, data);

  // hyscan_hsx_converter_clear_out (&data);
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

  priv->state.max_time = (priv->state.max_time == 0 || priv->state.max_time < max) ?
                          max : priv->state.max_time;

  priv->state.percent_koeff = (gdouble)(priv->state.max_time - priv->state.min_time) / 100.0;

  g_print ("min_time %"G_GINT64_FORMAT" max_time %"G_GINT64_FORMAT" percent_koef %f\n", 
           priv->state.min_time,
           priv->state.max_time,
           priv->state.percent_koeff);

  priv->zero_time = priv->state.min_time;
}

static gboolean
hyscan_hsx_converter_process_acoustic (HyScanHSXConverter        *self,
                                       gint64                     time,
                                       HyScanHSXConverterOutData *out_data)
{
  HyScanHSXConverterPrivate *priv = self->priv;
  guint rss_elem_cnt = priv->max_rss_size;  
  guint i = 0;

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

  for (; i < HYSCAN_AC_TYPE_LAST; i++)
    {
      HyScanDBFindStatus find_status;
      guint32 index;

      /* Поиск индекса данных для указанного момента времени.*/
      find_status = hyscan_amplitude_find_data (priv->ampl[i], time, &index, NULL, NULL, NULL);

      if (find_status != HYSCAN_DB_FIND_OK)
        continue;

      s[i].c_ampls = hyscan_amplitude_get_amplitude (priv->ampl[i], index, &s[i].points, &s[i].time, &s[i].noise);

      if (s[i].c_ampls != NULL && !s[i].noise)
        s[i].exist = TRUE;

      g_print ("ampl[%d][%d] exist=%d\n", i, s[i].points, (int)s[i].exist);
    }

  /* Минимальный размер массива, к которому нужно привести два массива */
  if (s[0].exist && s[1].exist)
    rss_elem_cnt = MIN (rss_elem_cnt, MIN (s[0].points, s[1].points));
  else if (s[0].exist && !s[1].exist)
    rss_elem_cnt = MIN (rss_elem_cnt, s[0].points);
  else if (!s[0].exist && s[1].exist)
    rss_elem_cnt = MIN (rss_elem_cnt, s[1].points);
  else
    return FALSE;

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
      g_print ("c_ampls[%d][%d] fs = %f\n", i, s[i].points, fs);
      added_time = add_microsec (priv->track_time, s[i].time - priv->zero_time);
      
      out_data->acoustic[i].time = get_seconds (added_time);
      out_data->acoustic[i].data = i_port_amp;
      out_data->acoustic[i].size = rss_elem_cnt;
      out_data->cut_fs = fs;
      
      g_date_time_unref (added_time);
    }

  return TRUE;
}

static gboolean
hyscan_hsx_converter_process_nmea (HyScanHSXConverter        *self,
                                   gint64                     time,
                                   HyScanHSXConverterOutData *out_data)
{
  guint32 index;
  gint i = 0;
  HyScanDBFindStatus find_status;
  HyScanHSXConverterPrivate *priv = self->priv;

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

      find_status = hyscan_nav_data_find_data (priv->nmea[i], 
                                               time, &index, NULL, NULL, NULL);

      if (find_status != HYSCAN_DB_FIND_OK)
        goto skip;
    
      g_print ("NMEA data %d finded\n", i);

      s[i].exist = hyscan_nav_data_get (priv->nmea[i],
                                        index,
                                        &s[i].time,
                                        &s[i].val);
    skip:
      i++;
    }
    
  if (s[HYSCAN_NMEA_FIELD_LAT].exist & s[HYSCAN_NMEA_FIELD_LON].exist)
    {
      double x = 0.0, y = 0.0;
      GDateTime *added_time;
      /* SOME CODE */

      added_time = add_microsec (priv->track_time, s[HYSCAN_NMEA_FIELD_LAT].time - priv->zero_time);     
      out_data->rmc_time = get_seconds (added_time);
      g_date_time_unref (added_time);
      out_data->x = x;
      out_data->y = y;
    }

  if (s[HYSCAN_NMEA_FIELD_TIME].exist)
    out_data->gps_time = s[HYSCAN_NMEA_FIELD_TIME].val;
  if (s[HYSCAN_NMEA_FIELD_SPEED].exist)
    out_data->speed_knots = s[HYSCAN_NMEA_FIELD_SPEED].val;
  if (s[HYSCAN_NMEA_FIELD_TRACK].exist)
    out_data->heading = s[HYSCAN_NMEA_FIELD_SPEED].val;
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


  /* Change to nav data */
  
  /*type = hyscan_nmea_data_check_sentence (s[i].sentence);
  if (type == HYSCAN_NMEA_DATA_ANY)
    s[i].sent = hyscan_nmea_data_split_sentence (s[i].sentence);

  if (s[i].c_ampls != NULL && !s[i].noise)
    s[i].exist = TRUE;*/

  return FALSE;
}

static gboolean
hyscan_hsx_converter_send_out (HyScanHSXConverter        *self,
                               HyScanHSXConverterOutData *out_data)
{
  HyScanHSXConverterPrivate *priv = self->priv;
  gsize written = 0;
  guint i = 0, j = 0;
  gboolean res = FALSE;
  HyScanHSXConverterOutData *od = out_data;
  
  /* Определяю индекс борта с данными или пропускаю RSS строку */ 
  if (od->acoustic[HYSCAN_AC_TYPE_PORT].size == 0 && od->acoustic[HYSCAN_AC_TYPE_STARBOARD].size == 0)
    goto next;
  else
    i = (od->acoustic[HYSCAN_AC_TYPE_PORT].size != 0) ? HYSCAN_AC_TYPE_PORT : HYSCAN_AC_TYPE_STARBOARD;

  res = g_output_stream_printf (priv->out.out_stream,
                                &written, NULL, NULL,
                                "RSS 4 %.3f 100 %d %d %.2f 0 %.2f %.2f 0 %d 3 0\n", 
                                od->acoustic[i].time,
                                od->acoustic[HYSCAN_AC_TYPE_PORT].size,
                                od->acoustic[HYSCAN_AC_TYPE_STARBOARD].size,
                                od->sound_velosity,
                                od->depth,
                                od->cut_fs,
                                priv->max_ampl_value);
  if (!res)
    return FALSE;

  for (; i < HYSCAN_AC_TYPE_LAST; i++)
    {
      for (j = 0; j < od->acoustic[i].size; j++)
        {
          if (!g_output_stream_printf (priv->out.out_stream,
                                       &written, NULL, NULL,
                                       "%d ",od->acoustic[i].data[j]))
            {
              return FALSE;
            }
        }

      if (!g_output_stream_printf (priv->out.out_stream,
                                   &written, NULL, NULL, "\n"))
        {
          return FALSE;
        }
      res = TRUE;
    }

next:    
  if (od->depth != 0)
    {
      if (!g_output_stream_printf (priv->out.out_stream,
                                   &written, NULL, NULL,
                                   "EC1 3 %.3f %.2f\n", od->depth_time, od->depth))
        {
          return FALSE;
        }
    }

  if (od->altitude != 0 || od->roll != 0 || od->pitch != 0)
    {
       if (!g_output_stream_printf (priv->out.out_stream,
                                    &written, NULL, NULL,
                                    "HCP 1 %.3f %.2f %.2f %.2f\n",
                                    od->gga_time, od->altitude,
                                    od->roll, od->pitch))
        {
          return FALSE;
        }
    }

  if (od->heading != 0)
    {
       if (!g_output_stream_printf (priv->out.out_stream,
                                    &written, NULL, NULL,
                                    "GPS 0 %.3f %.1f %.2f %.2f %.0f %.0f\n",
                                    od->gga_time, od->heading, od->speed_knots,
                                    od->hdop_gps, od->quality, (gdouble)od->sat_count))
        {
          return FALSE;
        }

       if (!g_output_stream_printf (priv->out.out_stream,
                                    &written, NULL, NULL,
                                    "GYR 2 %.3f %.2f\n",
                                    od->hdt_time, od->heading))
        {
          return FALSE;
        }
    }

  if (od->x != 0 && od->y != 0)
    {
       if (!g_output_stream_printf (priv->out.out_stream,
                                    &written, NULL, NULL,
                                    "POS 0 %.3f %.2f %.2f\n", od->rmc_time, od->x, od->y))
        {
          return FALSE;
        }
    }

  return TRUE;
}

static void
hyscan_hsx_converter_clear_out (HyScanHSXConverterOutData **out_data)
{
  gint i = 0;
  HyScanHSXConverterOutData *od;
  if (*out_data == NULL)
    return; 

  od = *out_data;
  g_print ("out_data ptr =%p addr =%p\n", od, out_data);
  while (i < HYSCAN_AC_TYPE_LAST)
    g_free (od->acoustic[i].data);

  g_clear_pointer (out_data, g_free);
}

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

void
hyscan_hsx_converter_set_track (HyScanHSXConverter  *self,
                                HyScanDB            *db,
                                const gchar         *project_name,
                                const gchar         *track_name)
{
  g_return_if_fail (HYSCAN_IS_HSX_CONVERTER (self));

  if (hyscan_hsx_converter_is_run (self))
    {
      g_warning ("HyScanHSXConverter: convert thread is execute. Stop it before set track.");
      return;
    }

  hyscan_hsx_converter_sources_clear (self);
  hyscan_hsx_converter_sources_init (self, db, project_name, track_name);
  if (!hyscan_hsx_converter_out_init (self, project_name, track_name))
    {
      hyscan_hsx_converter_sources_clear (self);
      hyscan_hsx_converter_out_clear (self);
      g_warning ("HyScanHSXConverter: Can't init . Stop it before set track.");
      return;
    }
}

/* Максимальное значение амплитуды  */
void
hyscan_hsx_converter_set_max_ampl (HyScanHSXConverter *self,
                                   guint               ampl_val)
{
  HyScanHSXConverterPrivate *priv;
  g_return_if_fail (HYSCAN_IS_HSX_CONVERTER (self));

  priv = self->priv;
  priv->max_ampl_value = ampl_val;

}

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

      g_print ("thread started\n");

      /* Оформляю подписку на плеер */
      hyscan_hsx_converter_player[SIGNAL_PLAYER_PROCESS] =  
        g_signal_connect_swapped (priv->player, "process", 
                                  G_CALLBACK (hyscan_hsx_converter_proc_cb),
                                  self);
      //g_debug ("HyScanHSXConverter: convert thread started");
    }

  return (priv->conv_thread != NULL);
}

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
