/* hyscan-acoustic-data.c
 *
 * Copyright 2015-2018 Screen LLC, Andrei Fadeev <andrei@webcontrol.ru>
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
 * SECTION: hyscan-acoustic-data
 * @Short_description: класс обработки гидроакустических данных
 * @Title: HyScanAcousticData
 *
 * Класс HyScanAcousticData используется для обработки гидроакустических данных,
 * такой как преобразование типов данных, свёртки, вычисления аплитуды сигнала.
 *
 * Объект обработки данных создаётся функцией #hyscan_acoustic_data_new.
 *
 * В своей работе класс HyScanAcousticData может использовать внешний кэш для
 * хранения обработанных данных. В случае запроса ранее обработанных данных
 * пользователь получит их копию из кэша. Несколько экземпляров класса,
 * обрабатывающих одинаковые каналы данных могут использовать один и тот же кэш.
 * Таким образом, данные обработанные в одной части приложения не потребуют
 * повторной обработки в другой его части.
 *
 * Функции #hyscan_acoustic_data_get_db, #hyscan_acoustic_data_get_project_name,
 * #hyscan_acoustic_data_get_track_name, #hyscan_acoustic_data_get_source,
 * #hyscan_acoustic_data_get_channel, #hyscan_acoustic_data_is_noise,
 * #hyscan_acoustic_data_get_discretization, #hyscan_acoustic_data_get_offset,
 * #hyscan_acoustic_data_get_info, #hyscan_acoustic_data_is_writable и
 * #hyscan_acoustic_data_has_tvg предназначены для получения информации
 * о канале данных и типе данных в нём.
 *
 * Функции #hyscan_acoustic_data_get_mod_count, #hyscan_acoustic_data_get_range и
 * #hyscan_acoustic_data_find_data предназначены для определения границ
 * записанных данных и их поиска по метке времени. Они аналогичны функциям
 * #hyscan_db_get_mod_count, #hyscan_db_channel_get_data_range и
 * #hyscan_db_channel_find_data интерфейса #HyScanDB.
 *
 * Функция #hyscan_acoustic_data_set_convolve устанавливает параметры свёртки
 * гидроакустических данных с излучаемым сигналом.
 *
 * Для чтения и обработки данных используются функции
 * #hyscan_acoustic_data_get_size_time, #hyscan_acoustic_data_get_signal,
 * #hyscan_acoustic_data_get_tvg, #hyscan_acoustic_data_get_amplitude и
 * #hyscan_acoustic_data_get_complex.
 *
 * Класс HyScanAcousticData не поддерживает работу в многопоточном режиме.
 * Рекомендуется создавать свой экземпляр объекта обработки данных в каждом
 * потоке и использовать единый кэш данных.
 */

#include "hyscan-acoustic-data.h"
#include "hyscan-core-common.h"

#include <hyscan-convolution.h>

#include <string.h>
#include <math.h>

#define CACHE_DATA_MAGIC       0xf97603e8              /* Идентификатор заголовка кэша данных. */
#define CACHE_META_MAGIC       0x1e4a8071              /* Идентификатор заголовка кэша метаинформации. */
#define CONV_SCALE             100.0                   /* Коэффициент перевода масштаба свёртки в integer. */

enum
{
  PROP_O,
  PROP_DB,
  PROP_CACHE,
  PROP_PROJECT_NAME,
  PROP_TRACK_NAME,
  PROP_SOURCE,
  PROP_CHANNEL,
  PROP_NOISE
};

enum
{
  DATA_TYPE_REAL,
  DATA_TYPE_COMPLEX,
  DATA_TYPE_AMPLITUDE,
  DATA_TYPE_TVG,
  DATA_TYPE_META
};

/* Структруа заголовка кэша. */
typedef struct
{
  guint32                      magic;                  /* Идентификатор заголовка. */
  guint32                      n_points;               /* Число точек данных. */
  gint64                       time;                   /* Метка времени. */
} HyScanAcousticDataCacheHeader;

/* Структура с образом сигнала и объектом свёртки. */
typedef struct
{
  gint64                       time;                   /* Время начала действия сигнала. */
  guint32                      index;                  /* Индекс начала действия сигнала. */
  HyScanComplexFloat          *image;                  /* Образ сигнала для свёртки. */
  guint32                      n_points;               /* Число точек образа сигнала. */
  HyScanConvolution           *convolution;            /* Объект свёртки. */
} HyScanAcousticDataSignal;

struct _HyScanAcousticDataPrivate
{
  HyScanDB                    *db;                     /* Интерфейс базы данных. */
  gchar                       *project_name;           /* Название проекта. */
  gchar                       *track_name;             /* Название галса. */
  HyScanSourceType             source;                 /* Тип источника данных. */
  guint                        channel;                /* Индекс канала данных. */
  gboolean                     noise;                  /* Канал с шумами. */

  HyScanAntennaOffset          offset;                 /* Смещение приёмной антенны. */
  HyScanAcousticDataInfo       info;                   /* Параметры гидроакустических данных. */
  HyScanDiscretizationType     discretization;         /* Тип дискретизации данных. */

  gint32                       channel_id;             /* Идентификатор открытого канала данных. */
  gint32                       signal_id;              /* Идентификатор открытого канала с образами сигналов. */
  gint32                       tvg_id;                 /* Идентификатор открытого канала с коэффициентами усиления. */

  HyScanBuffer                *channel_buffer;         /* Буфер канальных данных. */
  HyScanBuffer                *real_buffer;            /* Буфер действительных данных. */
  HyScanBuffer                *complex_buffer;         /* Буфер комплексных данных. */
  gint64                       data_time;              /* Метка времени обрабатываемых данных. */

  HyScanCache                 *cache;                  /* Кэш данных. */
  HyScanBuffer                *cache_buffer;           /* Буфер кэша данных. */
  gchar                       *cache_token;            /* Основа ключа кэширования. */
  GString                     *cache_key;              /* Ключ кэширования. */

  GArray                      *signals;                /* Массив сигналов. */
  guint32                      last_signal_index;      /* Индекс последнего загруженного сигнала. */
  guint64                      signals_mod_count;      /* Номер изменений сигналов. */
  gboolean                     convolve;               /* Выполнять или нет свёртку. */
  guint                        conv_scale;             /* Коэффициент дополнительного масштабирования свёртки. */
};

static void            hyscan_acoustic_data_interface_init     (HyScanAmplitudeInterface      *iface);
static void            hyscan_acoustic_data_set_property       (GObject                       *object,
                                                                guint                          prop_id,
                                                                const GValue                  *value,
                                                                GParamSpec                    *pspec);
static void            hyscan_acoustic_data_object_constructed (GObject                       *object);
static void            hyscan_acoustic_data_object_finalize    (GObject                       *object);

static void            hyscan_acoustic_data_update_cache_key   (HyScanAcousticDataPrivate     *priv,
                                                                gint                           type,
                                                                guint32                        index);

static void            hyscan_acoustic_data_load_signals       (HyScanAcousticDataPrivate     *priv);
static HyScanAcousticDataSignal *
                       hyscan_acoustic_data_find_signal        (HyScanAcousticDataPrivate     *priv,
                                                                guint32                        index);

static gboolean        hyscan_acoustic_data_read_channel_data  (HyScanAcousticDataPrivate     *priv,
                                                                guint32                        index);

static gboolean        hyscan_acoustic_data_real2complex       (HyScanAcousticDataPrivate     *priv);

static gboolean        hyscan_acoustic_data_convolution        (HyScanAcousticDataPrivate     *priv,
                                                                guint32                        index);

static gboolean        hyscan_acoustic_data_calc_amplitude     (HyScanAcousticDataPrivate     *priv);

static gboolean        hyscan_acoustic_data_check_data_cache   (HyScanAcousticDataPrivate     *priv,
                                                                gint                           type,
                                                                guint32                        index);

static gboolean        hyscan_acoustic_data_check_meta_cache   (HyScanAcousticDataPrivate     *priv,
                                                                guint32                        index,
                                                                guint32                       *n_points,
                                                                gint64                        *time);

G_DEFINE_TYPE_WITH_CODE (HyScanAcousticData, hyscan_acoustic_data, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (HyScanAcousticData)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_AMPLITUDE, hyscan_acoustic_data_interface_init))

static void
hyscan_acoustic_data_class_init (HyScanAcousticDataClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_acoustic_data_set_property;

  object_class->constructed = hyscan_acoustic_data_object_constructed;
  object_class->finalize = hyscan_acoustic_data_object_finalize;

  g_object_class_install_property (object_class, PROP_DB,
    g_param_spec_object ("db", "DB", "HyScanDB interface", HYSCAN_TYPE_DB,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_CACHE,
    g_param_spec_object ("cache", "Cache", "HyScanCache interface", HYSCAN_TYPE_CACHE,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_PROJECT_NAME,
    g_param_spec_string ("project-name", "ProjectName", "Project name", NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_TRACK_NAME,
    g_param_spec_string ("track-name", "TrackName", "Track name", NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_SOURCE,
    g_param_spec_int ("source", "Source", "Source type", 0, G_MAXINT, 0,
                      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_CHANNEL,
    g_param_spec_uint ("channel", "Channel", "Source channel", 1, G_MAXUINT, 1,
                       G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_NOISE,
    g_param_spec_boolean ("noise", "Noise", "Use noise channel", FALSE,
                          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_acoustic_data_init (HyScanAcousticData *acoustic)
{
  acoustic->priv = hyscan_acoustic_data_get_instance_private (acoustic);
}

static void
hyscan_acoustic_data_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  HyScanAcousticData *acoustic = HYSCAN_ACOUSTIC_DATA (object);
  HyScanAcousticDataPrivate *priv = acoustic->priv;

  switch (prop_id)
    {
    case PROP_DB:
      priv->db = g_value_dup_object (value);
      break;

    case PROP_CACHE:
      priv->cache = g_value_dup_object (value);
      break;

    case PROP_PROJECT_NAME:
      priv->project_name = g_value_dup_string (value);
      break;

    case PROP_TRACK_NAME:
      priv->track_name = g_value_dup_string (value);
      break;

    case PROP_SOURCE:
      priv->source = g_value_get_int (value);
      break;

    case PROP_CHANNEL:
      priv->channel = g_value_get_uint (value);
      break;

    case PROP_NOISE:
      priv->noise = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_acoustic_data_object_constructed (GObject *object)
{
  HyScanAcousticData *acoustic = HYSCAN_ACOUSTIC_DATA (object);
  HyScanAcousticDataPrivate *priv = acoustic->priv;

  const gchar *data_channel_name;
  const gchar *signal_channel_name;
  const gchar *tvg_channel_name;
  gchar *db_uri = NULL;

  gint32 project_id = -1;
  gint32 track_id = -1;
  gint32 param_id = -1;

  gboolean status = FALSE;

  /* Проверка входных параметров. */
  if (priv->db == NULL)
    {
      g_warning ("HyScanAcousticData: db isn't specified");
      goto exit;
    }

  if (priv->project_name == NULL)
    {
      g_warning ("HyScanAcousticData: empty project name");
      goto exit;
    }

  if (priv->track_name == NULL)
    {
      g_warning ("HyScanAcousticData: empty track name");
      goto exit;
    }

  if (!hyscan_source_is_sonar (priv->source))
    {
      g_warning ("HyScanAcousticData: unsupported source type");
      goto exit;
    }

  priv->channel_buffer = hyscan_buffer_new ();
  priv->real_buffer = hyscan_buffer_new ();
  priv->complex_buffer = hyscan_buffer_new ();

  hyscan_buffer_set_float (priv->real_buffer, NULL, 0);
  hyscan_buffer_set_complex_float (priv->complex_buffer, NULL, 0);

  /* Открываем канал с данными. */
  data_channel_name = hyscan_channel_get_name_by_types (priv->source,
                        priv->noise ? HYSCAN_CHANNEL_NOISE : HYSCAN_CHANNEL_DATA,
                        priv->channel);
  signal_channel_name = hyscan_channel_get_name_by_types (priv->source,
                                                          HYSCAN_CHANNEL_SIGNAL,
                                                          priv->channel);
  tvg_channel_name = hyscan_channel_get_name_by_types (priv->source,
                                                       HYSCAN_CHANNEL_TVG,
                                                       priv->channel);
  if ((data_channel_name == NULL) ||
      (signal_channel_name == NULL) ||
      (tvg_channel_name == NULL))
    {
      g_warning ("HyScanAcousticData: unsupported data channel");
      goto exit;
    }

  project_id = hyscan_db_project_open (priv->db, priv->project_name);
  if (project_id < 0)
    {
      g_info ("HyScanAcousticData: can't open project '%s'",
              priv->project_name);
      goto exit;
    }

  track_id = hyscan_db_track_open (priv->db, project_id, priv->track_name);
  if (track_id < 0)
    {
      g_info ("HyScanAcousticData: can't open track '%s.%s'",
              priv->project_name, priv->track_name);
      goto exit;
    }

  priv->channel_id = hyscan_db_channel_open (priv->db, track_id, data_channel_name);
  if (priv->channel_id < 0)
    {
      g_info ("HyScanAcousticData: can't open channel '%s.%s.%s'",
              priv->project_name, priv->track_name, data_channel_name);
      goto exit;
    }

  /* Проверяем, что в канале есть хотя бы одна запись. */
  if (!hyscan_db_channel_get_data_range (priv->db, priv->channel_id, NULL, NULL))
    goto exit;

  /* Параметры канала данных. */
  param_id = hyscan_db_channel_param_open (priv->db, priv->channel_id);
  if (param_id < 0)
    {
      g_warning ("HyScanAcousticData: '%s.%s.%s': can't open parameters",
                 priv->project_name, priv->track_name, data_channel_name);
      goto exit;
    }

  status = hyscan_core_params_load_antenna_offset (priv->db, param_id,
                                                     ACOUSTIC_CHANNEL_SCHEMA_ID,
                                                     ACOUSTIC_CHANNEL_SCHEMA_VERSION,
                                                     &priv->offset);
  if (!status)
    {
      g_warning ("HyScanAcousticData: '%s.%s.%s': can't read antenna offset",
                 priv->project_name, priv->track_name, data_channel_name);
      goto exit;
    }

  status = hyscan_core_params_load_acoustic_data_info (priv->db, param_id, &priv->info);
  if (!status)
    {
      g_warning ("HyScanAcousticData: '%s.%s.%s': can't read parameters",
                 priv->project_name, priv->track_name, data_channel_name);
      goto exit;
    }

  hyscan_db_close (priv->db, param_id);
  param_id = -1;

  /* Образы сигналов для свёртки. */
  priv->signal_id = hyscan_db_channel_open (priv->db, track_id, signal_channel_name);
  if (priv->signal_id > 0)
    {
      param_id = hyscan_db_channel_param_open (priv->db, priv->signal_id);
      if (param_id < 0)
        {
          g_warning ("HyScanAcousticData: '%s.%s.%s': can't open parameters",
                     priv->project_name, priv->track_name, signal_channel_name);
          goto exit;
        }

      status = hyscan_core_params_check_signal_info (priv->db, param_id, priv->info.data_rate);
      if (!status)
        {
          g_warning ("HyScanAcousticData: '%s.%s.%s': error in parameters",
                     priv->project_name, priv->track_name, signal_channel_name);
          goto exit;
        }
      else
        {
          priv->signals_mod_count = hyscan_db_get_mod_count (priv->db, priv->signal_id) - 1;
          priv->last_signal_index = -1;
          hyscan_acoustic_data_load_signals (priv);
        }
      status = FALSE;

      hyscan_db_close (priv->db, param_id);
      param_id = -1;
    }

  /* Параметры ВАРУ. */
  priv->tvg_id = hyscan_db_channel_open (priv->db, track_id, tvg_channel_name);
  if (priv->tvg_id > 0)
    {
      param_id = hyscan_db_channel_param_open (priv->db, priv->tvg_id);
      if (param_id < 0)
        {
          g_warning ("HyScanAcousticData: '%s.%s.%s': can't open parameters",
                     priv->project_name, priv->track_name, tvg_channel_name);
          goto exit;
        }

      status = hyscan_core_params_check_tvg_info (priv->db, param_id, priv->info.data_rate);
      if (!status)
        {
          g_warning ("HyScanAcousticData: '%s.%s.%s': error in parameters",
                     priv->project_name, priv->track_name, tvg_channel_name);
          goto exit;
        }
      status = FALSE;

      hyscan_db_close (priv->db, param_id);
      param_id = -1;
    }

  priv->discretization = hyscan_discretization_get_type_by_data (priv->info.data_type);

  priv->convolve = TRUE;
  if (priv->discretization == HYSCAN_DISCRETIZATION_REAL)
    priv->conv_scale = 2.0 * CONV_SCALE;
  else
    priv->conv_scale = 1.0 * CONV_SCALE;

  /* Ключ кэширования. */
  priv->cache_key = g_string_new (NULL);
  priv->cache_buffer = hyscan_buffer_new ();

  db_uri = hyscan_db_get_uri (priv->db);
  priv->cache_token = g_strdup_printf ("ACOUSTIC.%s.%s.%s.%u.%u",
                                       db_uri,
                                       priv->project_name,
                                       priv->track_name,
                                       priv->source,
                                       priv->channel);
  g_free (db_uri);

  status = TRUE;

exit:
  if (project_id > 0)
    hyscan_db_close (priv->db, project_id);
  if (track_id > 0)
    hyscan_db_close (priv->db, track_id);
  if (param_id > 0)
    hyscan_db_close (priv->db, param_id);

  if (status)
    return;

  if (priv->channel_id > 0)
    {
      hyscan_db_close (priv->db, priv->channel_id);
      priv->channel_id = -1;
    }
  if (priv->signal_id > 0)
    {
      hyscan_db_close (priv->db, priv->signal_id);
      priv->signal_id = -1;
    }
  if (priv->tvg_id > 0)
    {
      hyscan_db_close (priv->db, priv->tvg_id);
      priv->tvg_id = -1;
    }
}

static void
hyscan_acoustic_data_object_finalize (GObject *object)
{
  HyScanAcousticData *acoustic = HYSCAN_ACOUSTIC_DATA (object);
  HyScanAcousticDataPrivate *priv = acoustic->priv;

  if (priv->channel_id > 0)
    hyscan_db_close (priv->db, priv->channel_id);
  if (priv->signal_id > 0)
    hyscan_db_close (priv->db, priv->signal_id);
  if (priv->tvg_id > 0)
    hyscan_db_close (priv->db, priv->tvg_id);

  if (priv->signals != NULL)
    {
      guint i;

      for (i = 0; i < priv->signals->len; i++)
        {
          HyScanAcousticDataSignal *signal;
          signal = &g_array_index (priv->signals, HyScanAcousticDataSignal, i);
          g_clear_object (&signal->convolution);
          g_free (signal->image);
        }
      g_array_unref (priv->signals);
    }

  g_clear_object (&priv->channel_buffer);
  g_clear_object (&priv->real_buffer);
  g_clear_object (&priv->complex_buffer);
  g_clear_object (&priv->db);

  if (priv->cache_key != NULL)
    g_string_free (priv->cache_key, TRUE);
  g_free (priv->cache_token);
  g_clear_object (&priv->cache_buffer);
  g_clear_object (&priv->cache);

  g_free (priv->project_name);
  g_free (priv->track_name);

  G_OBJECT_CLASS (hyscan_acoustic_data_parent_class)->finalize (object);
}

/* Функция обновляет ключ кэширования данных. */
static void
hyscan_acoustic_data_update_cache_key (HyScanAcousticDataPrivate *priv,
                                       gint                       type,
                                       guint32                    index)
{
  const gchar *dts = "XXX";

  switch (type)
    {
    case DATA_TYPE_REAL:
      dts = "REL";
      break;

    case DATA_TYPE_COMPLEX:
      dts = priv->convolve ? "QCV" : "QNC";
      break;

    case DATA_TYPE_AMPLITUDE:
      dts = priv->convolve ? "ACV" : "ANC";
      break;

    case DATA_TYPE_TVG:
      dts = "TVG";
      break;

    case DATA_TYPE_META:
      dts = "MTA";
      break;
    }

  g_string_printf (priv->cache_key, "%s.%s.%u.%u",
                   priv->cache_token, dts,
                   priv->convolve ? priv->conv_scale : 0,
                   index);
}

/* Функция загружает образы сигналов для свёртки. */
static void
hyscan_acoustic_data_load_signals (HyScanAcousticDataPrivate *priv)
{
  guint32 first_signal_index;
  guint32 last_signal_index;
  guint64 signals_mod_count;

  gboolean status;
  guint32 i;

  /* Если нет канала с образами сигналов или сигналы не изменяются, выходим. */
  if (priv->signal_id < 0)
    return;

  /* Проверяем изменения в сигналах. */
  signals_mod_count = hyscan_db_get_mod_count (priv->db, priv->signal_id);
  if (priv->signals_mod_count == signals_mod_count)
    return;

  /* Массив образов сигналов. */
  if (priv->signals == NULL)
    priv->signals = g_array_new (TRUE, TRUE, sizeof (HyScanAcousticDataSignal));

  /* Проверяем индекс последнего загруженного образа сигнала. */
  status = hyscan_db_channel_get_data_range (priv->db, priv->signal_id,
                                             &first_signal_index, &last_signal_index);
  if (!status)
    return;

  if (priv->last_signal_index == last_signal_index)
    return;

  /* Загружаем "новые" образы сигналов. */
  if (priv->signals->len == 0)
    i = 0;
  else
    i = priv->last_signal_index + 1;

  if (i < first_signal_index)
    i = first_signal_index;

  for (; i <= last_signal_index; i++)
    {
      HyScanAcousticDataSignal signal_info = {0};
      HyScanDBFindStatus find_status;
      guint32 lindex, rindex;
      gint64 ltime, rtime;
      gpointer image;
      guint32 size;
      gint64 time;

      /* Считываем образ сигнала. */
      status = hyscan_db_channel_get_data (priv->db, priv->signal_id, i,
                                           priv->channel_buffer, &time);
      if (!status)
        return;

      hyscan_buffer_set_data_type (priv->channel_buffer, HYSCAN_DATA_COMPLEX_FLOAT32LE);
      if (!hyscan_buffer_import (priv->complex_buffer, priv->channel_buffer))
        return;

      image = hyscan_buffer_get (priv->complex_buffer, NULL, &size);

      /* Ищем индекс данных с которого начинает действовать сигнал. */
      find_status = hyscan_db_channel_find_data (priv->db, priv->channel_id, time,
                                                 &lindex, &rindex, &ltime, &rtime);
      if (find_status == HYSCAN_DB_FIND_OK)
        signal_info.index = rindex;
      else if (find_status == HYSCAN_DB_FIND_LESS)
        signal_info.index = first_signal_index;
      else
        return;

      /* Объект свёртки. */
      if (size >= 2 * sizeof (HyScanComplexFloat))
        {
          signal_info.time = time;
          signal_info.image = g_memdup (image, size);
          signal_info.n_points = size / sizeof (HyScanComplexFloat);
          signal_info.convolution = hyscan_convolution_new ();
          hyscan_convolution_set_image (signal_info.convolution,
                                        signal_info.image,
                                        signal_info.n_points);
        }

      g_array_append_val (priv->signals, signal_info);
      priv->last_signal_index = i;
    }

  priv->signals_mod_count = signals_mod_count;

  /* Если запись в канал данных завершена, перестаём обновлять образы сигналов. */
  if (!hyscan_db_channel_is_writable (priv->db, priv->signal_id))
    {
      hyscan_db_close (priv->db, priv->signal_id);
      priv->signal_id = -1;
    }
}

/* Функция ищет образ сигнала для свёртки для указанного индекса. */
static HyScanAcousticDataSignal *
hyscan_acoustic_data_find_signal (HyScanAcousticDataPrivate *priv,
                                  guint32                    index)
{
  gint i;

  if (priv->signals == NULL)
    return NULL;

  /* Ищем сигнал для момента времени time. */
  for (i = priv->signals->len - 1; i >= 0; i--)
    {
      HyScanAcousticDataSignal *signal;

      signal = &g_array_index (priv->signals, HyScanAcousticDataSignal, i);
      if(index >= signal->index)
        return signal;
    }

  return NULL;
}

/* Функция считывает данные из канала в буфер канальных данных. */
static gboolean
hyscan_acoustic_data_read_channel_data (HyScanAcousticDataPrivate *priv,
                                        guint32                    index)
{
  gpointer data;
  guint32 size;

  /* Загружаем образы сигналов. */
  hyscan_acoustic_data_load_signals (priv);

  /* Считываем данные канала. */
  if (!hyscan_db_channel_get_data (priv->db, priv->channel_id, index, priv->channel_buffer, &priv->data_time))
    return FALSE;

  data = hyscan_buffer_get (priv->channel_buffer, NULL, &size);
  if ((data == NULL) || (size == 0))
    return FALSE;

  /* Размер считанных данных должен быть кратен размеру точки. */
  if ((size % hyscan_data_get_point_size (priv->info.data_type)) != 0)
    return FALSE;

  /* Импорт данных. */
  hyscan_buffer_set_data_type (priv->channel_buffer, priv->info.data_type);
  if (priv->discretization == HYSCAN_DISCRETIZATION_REAL)
    {
      if (!hyscan_buffer_import (priv->real_buffer, priv->channel_buffer))
        return FALSE;
    }
  else if (priv->discretization == HYSCAN_DISCRETIZATION_COMPLEX)
    {
      if (!hyscan_buffer_import (priv->complex_buffer, priv->channel_buffer))
        return FALSE;
    }
  else if (priv->discretization == HYSCAN_DISCRETIZATION_AMPLITUDE)
    {
      if (!hyscan_buffer_import (priv->real_buffer, priv->channel_buffer))
        return FALSE;
    }
  else
    {
      g_warning ("HyScanAcousticData: '%s.%s.%s': unsupported discretization type",
                 priv->project_name, priv->track_name,
                 hyscan_source_get_name_by_type (priv->source));

      return FALSE;
    }

  return TRUE;
}

/* Функция преобразовывает действительные данные АЦП в комплексные отсчёты. */
static gboolean
hyscan_acoustic_data_real2complex (HyScanAcousticDataPrivate *priv)
{
  HyScanComplexFloat *complex_data;
  gfloat *real_data;
  guint32 n_points;
  gdouble phase_step;
  gdouble phase;
  guint32 i;

  real_data = hyscan_buffer_get_float (priv->real_buffer, &n_points);
  if ((real_data == NULL) || (n_points == 0))
    return FALSE;

  hyscan_buffer_set_complex_float (priv->complex_buffer, NULL, n_points);
  complex_data = hyscan_buffer_get_complex_float (priv->complex_buffer, &n_points);
  if ((complex_data == NULL) || (n_points == 0))
    return FALSE;

  phase_step = 2.0 * G_PI;
  phase_step *= priv->info.signal_frequency;
  phase_step /= priv->info.data_rate;
  phase = 0.0;

  for (i = 0; i < n_points; i++)
    {
      complex_data[i].re = real_data[i] * sin (phase);
      complex_data[i].im = real_data[i] * cos (phase);
      phase += phase_step;
    }

  return TRUE;
}

/* Функция выполняет свёртку данных с образом сигнала. */
static gboolean
hyscan_acoustic_data_convolution (HyScanAcousticDataPrivate *priv,
                                  guint32                    index)
{
  HyScanAcousticDataSignal *signal;
  HyScanComplexFloat *data;
  guint32 n_points;

  /* Свёрка отключена. */
  if (!priv->convolve)
    return TRUE;

  /* Свёртка не нужна. */
  signal = hyscan_acoustic_data_find_signal (priv, index);
  if ((signal == NULL) || (signal->convolution == NULL))
    return TRUE;

  data = hyscan_buffer_get_complex_float (priv->complex_buffer, &n_points);
  if (data != NULL)
    {
      return hyscan_convolution_convolve (signal->convolution, data, n_points,
                                          priv->conv_scale / CONV_SCALE);
    }

  return FALSE;
}

/* Функция вычисляет амплитуду гидроакустических данных. */
static gboolean
hyscan_acoustic_data_calc_amplitude (HyScanAcousticDataPrivate *priv)
{
  HyScanComplexFloat *complex;
  gfloat *amplitude;
  guint32 n_points;
  guint32 i;

  if (priv->discretization == HYSCAN_DISCRETIZATION_AMPLITUDE)
    return TRUE;

  complex = hyscan_buffer_get_complex_float (priv->complex_buffer, &n_points);
  if ((complex == NULL) || (n_points == 0))
    return FALSE;

  hyscan_buffer_set_float (priv->real_buffer, NULL, n_points);
  amplitude = hyscan_buffer_get_float (priv->real_buffer, &n_points);
  for (i = 0; i < n_points; i++)
    {
      gfloat re = complex[i].re;
      gfloat im = complex[i].im;
      amplitude[i] = sqrtf (re * re + im * im);
    }

  return TRUE;
}

/* Функция проверяет кэш на наличие данных указанного типа и
 * если такие есть, считывает их. */
static gboolean
hyscan_acoustic_data_check_data_cache (HyScanAcousticDataPrivate *priv,
                                       gint                       type,
                                       guint32                    index)
{
  HyScanAcousticDataCacheHeader header;
  HyScanBuffer *data_buffer;
  guint32 data_n_points;
  gboolean status;

  if (priv->cache == NULL)
    return FALSE;

  /* Ключ кэширования. */
  hyscan_acoustic_data_update_cache_key (priv, type, index);

  /* Буфер для чтения данных. */
  hyscan_buffer_wrap (priv->cache_buffer, HYSCAN_DATA_BLOB, &header, sizeof (header));
  if (type == DATA_TYPE_COMPLEX)
    data_buffer = priv->complex_buffer;
  else if ((type == DATA_TYPE_AMPLITUDE) || (type == DATA_TYPE_TVG))
    data_buffer = priv->real_buffer;
  else
    return FALSE;

  /* Ищем данные в кэше. */
  status = hyscan_cache_get2 (priv->cache, priv->cache_key->str, NULL,
                              sizeof (header), priv->cache_buffer, data_buffer);
  if (!status)
    return FALSE;

  /* Верификация данных. */
  data_n_points  = hyscan_buffer_get_data_size (data_buffer);
  data_n_points /= hyscan_data_get_point_size (hyscan_buffer_get_data_type (data_buffer));
  if ((header.magic != CACHE_DATA_MAGIC) || (header.n_points != data_n_points))
    return FALSE;

  priv->data_time = header.time;

  return TRUE;
}

/* Функция проверяет кэш на наличие метаданных и если такие есть,
 * считывает их. */
static gboolean
hyscan_acoustic_data_check_meta_cache (HyScanAcousticDataPrivate *priv,
                                       guint32                    index,
                                       guint32                   *n_points,
                                       gint64                    *time)
{
  HyScanAcousticDataCacheHeader header;

  if (priv->cache == NULL)
    return FALSE;

  /* Ключ кэширования. */
  hyscan_acoustic_data_update_cache_key (priv, DATA_TYPE_META, index);

  /* Буфер для чтения данных. */
  hyscan_buffer_wrap (priv->cache_buffer, HYSCAN_DATA_BLOB, &header, sizeof (header));

  /* Ищем данные в кэше. */
  if (!hyscan_cache_get (priv->cache, priv->cache_key->str, NULL, priv->cache_buffer))
    return FALSE;

  /* Верификация данных. */
  if (header.magic != CACHE_META_MAGIC)
    return FALSE;

  (n_points != NULL) ? *n_points = header.n_points : 0;
  (time != NULL) ? *time = header.time : 0;

  return TRUE;
}

/**
 * hyscan_acoustic_data_new:
 * @db: указатель на #HyScanDB
 * @cache: (nullable): указатель на #HyScanCache
 * @project_name: название проекта
 * @track_name: название галса
 * @source: тип источника данных
 * @channel: индекс канала данных
 * @noise: признак шумовых данных
 *
 * Функция создаёт новый объект #HyScanAcousticData. В случае ошибки
 * функция вернёт значение NULL.
 *
 * Returns: #HyScanAcousticData или NULL. Для удаления #g_object_unref.
 */
HyScanAcousticData *
hyscan_acoustic_data_new (HyScanDB         *db,
                          HyScanCache      *cache,
                          const gchar      *project_name,
                          const gchar      *track_name,
                          HyScanSourceType  source,
                          guint             channel,
                          gboolean          noise)
{
  HyScanAcousticData *data;

  data = g_object_new (HYSCAN_TYPE_ACOUSTIC_DATA,
                       "db", db,
                       "cache", cache,
                       "project-name", project_name,
                       "track-name", track_name,
                       "source", source,
                       "channel", channel,
                       "noise", noise,
                       NULL);

  if (data->priv->channel_id <= 0)
    g_clear_object (&data);

  return data;
}

/**
 * hyscan_acoustic_data_get_db:
 * @data: указатель на #HyScanAcousticData
 *
 * Функция возвращает указатель на используемый #HyScanDB.
 *
 * Returns: #HyScanDB. Для удаления #g_object_unref.
 */
HyScanDB *
hyscan_acoustic_data_get_db (HyScanAcousticData *data)
{
  HyScanAcousticDataPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_ACOUSTIC_DATA (data), NULL);

  priv = data->priv;

  return (priv->channel_id > 0) ? g_object_ref (priv->db) : NULL;
}

/**
 * hyscan_acoustic_data_get_project_name:
 * @data: указатель на #HyScanAcousticData
 *
 * Функция возвращает название используемого проекта.
 *
 * Returns: Название проекта.
 */
const gchar *
hyscan_acoustic_data_get_project_name (HyScanAcousticData *data)
{
  HyScanAcousticDataPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_ACOUSTIC_DATA (data), NULL);

  priv = data->priv;

  return (priv->channel_id > 0) ? priv->project_name : NULL;
}

/**
 * hyscan_acoustic_data_get_track_name:
 * @data: указатель на #HyScanAcousticData
 *
 * Функция возвращает название используемого галса.
 *
 * Returns: Название галса.
 */
const gchar *
hyscan_acoustic_data_get_track_name (HyScanAcousticData *data)
{
  HyScanAcousticDataPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_ACOUSTIC_DATA (data), NULL);

  priv = data->priv;

  return (priv->channel_id > 0) ? priv->track_name : NULL;
}

/**
 * hyscan_acoustic_data_get_source:
 * @data: указатель на #HyScanAcousticData
 *
 * Функция возвращает тип используемого источника данных.
 *
 * Returns: Тип источника данных.
 */
HyScanSourceType
hyscan_acoustic_data_get_source (HyScanAcousticData *data)
{
  HyScanAcousticDataPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_ACOUSTIC_DATA (data), HYSCAN_SOURCE_INVALID);

  priv = data->priv;

  return (priv->channel_id > 0) ? priv->source : HYSCAN_SOURCE_INVALID;
}

/**
 * hyscan_acoustic_data_get_channel:
 * @data: указатель на #HyScanAcousticData
 *
 * Функция возвращает индекс используемого канала данных.
 *
 * Returns: Индекс канала данных.
 */
guint
hyscan_acoustic_data_get_channel (HyScanAcousticData *data)
{
  HyScanAcousticDataPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_ACOUSTIC_DATA (data), 0);

  priv = data->priv;

  return (priv->channel_id > 0) ? priv->channel : 0;
}

/**
 * hyscan_acoustic_data_is_noise:
 * @data: указатель на #HyScanAcousticData
 *
 * Функция возвращает признак шумовых данных.
 *
 * Returns: Признак шумовых данных.
 */
gboolean
hyscan_acoustic_data_is_noise (HyScanAcousticData *data)
{
  g_return_val_if_fail (HYSCAN_IS_ACOUSTIC_DATA (data), FALSE);

  return data->priv->noise;
}

/**
 * hyscan_acoustic_data_get_discretization:
 * @data: указатель на #HyScanAcousticData
 *
 * Функция возвращает тип дискретизации данных.
 *
 * Returns: Тип дискретизации данных.
 */
HyScanDiscretizationType
hyscan_acoustic_data_get_discretization (HyScanAcousticData *data)
{
  g_return_val_if_fail (HYSCAN_IS_ACOUSTIC_DATA (data), FALSE);

  return data->priv->discretization;
}

/**
 * hyscan_acoustic_data_get_offset:
 * @data: указатель на #HyScanAcousticData
 *
 * Функция возвращает информацию о смещении приёмной
 * антенны гидролокатора.
 *
 * Returns: Смещение приёмной антенны.
 */
HyScanAntennaOffset
hyscan_acoustic_data_get_offset (HyScanAcousticData *data)
{
  HyScanAcousticDataPrivate *priv;
  HyScanAntennaOffset zero = {0};

  g_return_val_if_fail (HYSCAN_IS_ACOUSTIC_DATA (data), zero);

  priv = data->priv;

  return (priv->channel_id > 0) ? priv->offset : zero;
}

/**
 * hyscan_acoustic_data_get_info:
 * @data: указатель на #HyScanAcousticData
 *
 * Функция возвращает параметры канала гидроакустических данных.
 *
 * Returns: Параметры канала гидроакустических данных.
 */
HyScanAcousticDataInfo
hyscan_acoustic_data_get_info (HyScanAcousticData *data)
{
  HyScanAcousticDataPrivate *priv;
  HyScanAcousticDataInfo zero;

  memset (&zero, 0, sizeof (HyScanAcousticDataInfo));

  g_return_val_if_fail (HYSCAN_IS_ACOUSTIC_DATA (data), zero);

  priv = data->priv;

  return (priv->channel_id > 0) ? priv->info : zero;
}

/**
 * hyscan_acoustic_data_is_writable:
 * @data: указатель на #HyScanAcousticData
 *
 * Функция определяет возможность изменения данных. Если функция вернёт
 * значение TRUE, возможна ситуация когда могут появиться новые данные
 * или исчезнуть уже записанные.
 *
 * Returns: %TRUE если в канал возможна запись данных, иначе %FALSE.
 */
gboolean
hyscan_acoustic_data_is_writable (HyScanAcousticData *data)
{
  HyScanAcousticDataPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_ACOUSTIC_DATA (data), FALSE);

  priv = data->priv;

  if (priv->channel_id <= 0)
    return FALSE;

  return hyscan_db_channel_is_writable (priv->db, priv->channel_id);
}

/**
 * hyscan_acoustic_data_has_tvg:
 * @data: указатель на #HyScanAcousticData
 *
 * Функция возвращает признак наличия информации о параметрах системы ВАРУ.
 *
 * Returns: %TRUE если в канале есть параметры системы ВАРУ, иначе %FALSE.
 */
gboolean
hyscan_acoustic_data_has_tvg (HyScanAcousticData *data)
{
  g_return_val_if_fail (HYSCAN_IS_ACOUSTIC_DATA (data), FALSE);

  return data->priv->tvg_id > 0;
}

/**
 * hyscan_acoustic_data_get_mod_count:
 * @data: указатель на #HyScanAcousticData
 *
 * Функция возвращает номер изменения в данных. Программа не должна полагаться
 * на значение номера изменения, важен только факт смены номера по сравнению с
 * предыдущим запросом.
 *
 * Returns: Номер изменения данных.
 */
guint32
hyscan_acoustic_data_get_mod_count (HyScanAcousticData *data)
{
  HyScanAcousticDataPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_ACOUSTIC_DATA (data), 0);

  priv = data->priv;

  if (priv->channel_id <= 0)
    return 0;

  return hyscan_db_get_mod_count (priv->db, priv->channel_id);
}

/**
 * hyscan_acoustic_data_get_range:
 * @data: указатель на #HyScanAcousticData
 * @first_index: (out) (nullable): начальный индекс данныx
 * @last_index: (out) (nullable): конечный индекс данныx
 *
 * Функция возвращает диапазон значений индексов записанных данных. Функция
 * вернёт значения начального и конечного индекса записей. Подробнее в описании
 * #hyscan_db_channel_get_data_range.
 *
 * Returns: %TRUE если границы записей определены, иначе %FALSE.
 */
gboolean
hyscan_acoustic_data_get_range (HyScanAcousticData *data,
                                guint32            *first_index,
                                guint32            *last_index)
{
  HyScanAcousticDataPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_ACOUSTIC_DATA (data), FALSE);

  priv = data->priv;

  if (priv->channel_id <= 0)
    return FALSE;

  return hyscan_db_channel_get_data_range (priv->db, priv->channel_id,
                                           first_index, last_index);
}

/**
 * hyscan_acoustic_data_find_data:
 * @data: указатель на #HyScanAcousticData
 * @time: искомый момент времени
 * @lindex: (out) (nullable): "левый" индекс данныx
 * @rindex: (out) (nullable): "правый" индекс данныx
 * @ltime: (out) (nullable): "левая" метка времени данныx
 * @rtime: (out) (nullable): "правая" метка времени данныx
 *
 * Функция ищет индекс данных для указанного момента времени. Подробнее
 * #hyscan_db_channel_find_data.
 *
 * Returns: Статус поиска индекса данных.
 */
HyScanDBFindStatus
hyscan_acoustic_data_find_data (HyScanAcousticData *data,
                                gint64              time,
                                guint32            *lindex,
                                guint32            *rindex,
                                gint64             *ltime,
                                gint64             *rtime)
{
  HyScanAcousticDataPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_ACOUSTIC_DATA (data), HYSCAN_DB_FIND_FAIL);

  priv = data->priv;

  if (priv->channel_id <= 0)
    return HYSCAN_DB_FIND_FAIL;

  return hyscan_db_channel_find_data (priv->db, priv->channel_id,
                                      time, lindex, rindex, ltime, rtime);
}

/**
 * hyscan_acoustic_data_set_convolve:
 * @data: указатель на #HyScanAcousticData
 * @convolve: признак использования свёртки
 * @scale: коэффициент масштабирования свёртки
 *
 * Функция включает или отключает выполнения свёртки данных, а также задаёт
 * коэффициент масштабирования данных после свёртки.
 */
void
hyscan_acoustic_data_set_convolve (HyScanAcousticData *data,
                                   gboolean            convolve,
                                   gdouble             scale)
{
  HyScanAcousticDataPrivate *priv;

  g_return_if_fail (HYSCAN_IS_ACOUSTIC_DATA (data));

  priv = data->priv;

  priv->convolve = convolve;

  if (priv->discretization == HYSCAN_DISCRETIZATION_REAL)
    scale *= 2.0;

  if (scale > 0.0)
    priv->conv_scale = scale;
}

/**
 * hyscan_acoustic_data_get_size_time:
 * @data: указатель на #HyScanAcousticData
 * @index: индекс считываемых данныx
 * @n_points: (out) (nullable): число точек данныx
 * @time: (out) (nullable): метка времени данныx
 *
 * Функция возвращает число точек данных и метку времени для указанного
 * индекса данных.
 *
 * Returns: %TRUE если данные считаны, иначе %FALSE.
 */
gboolean
hyscan_acoustic_data_get_size_time (HyScanAcousticData *data,
                                    guint32             index,
                                    guint32            *n_points,
                                    gint64             *time)
{
  HyScanAcousticDataPrivate *priv;
  guint32 readed_size;
  gint64 readed_time;

  g_return_val_if_fail (HYSCAN_IS_ACOUSTIC_DATA (data), FALSE);

  priv = data->priv;

  if (priv->channel_id <= 0)
    return FALSE;

  /* Проверяем наличие информации в кэше. */
  if (hyscan_acoustic_data_check_meta_cache (priv, index, n_points, time))
    return TRUE;

  readed_time = hyscan_db_channel_get_data_time (priv->db, priv->channel_id, index);
  readed_size = hyscan_db_channel_get_data_size (priv->db, priv->channel_id, index);
  readed_size /= sizeof (gfloat);

  if ((readed_size == 0) || (readed_time < 0))
    return FALSE;

  /* Сохраняем данные в кэше. */
  if (priv->cache != NULL)
    {
      HyScanAcousticDataCacheHeader header;

      header.magic = CACHE_META_MAGIC;
      header.n_points = readed_size;
      header.time = readed_time;
      hyscan_buffer_wrap (priv->cache_buffer, HYSCAN_DATA_BLOB, &header, sizeof (header));

      hyscan_acoustic_data_update_cache_key (priv, DATA_TYPE_META, index);
      hyscan_cache_set (priv->cache, priv->cache_key->str, NULL, priv->cache_buffer);
    }

  (n_points != NULL) ? *n_points = readed_size : 0;
  (time != NULL) ? *time = readed_time : 0;

  return TRUE;
}

/**
 * hyscan_acoustic_data_get_signal:
 * @data: указатель на #HyScanAcousticData
 * @index: индекс считываемых данныx
 * @n_points: (out): число точек сигнала
 * @time: (out) (nullable): метка времени начала действия сигнала
 *
 * Функция возвращает образ сигнала для указанного индекса данных.
 *
 * Функция возвращает указатель на внутренний буфер, данные в котором
 * действительны до следующего вызова функций HyScanAcousticData. Пользователь
 * не должен модифицировать эти данные.
 *
 * Returns: (nullable) (array length=n_points) (transfer none):
 *          Образ сигнала или NULL.
 */
const HyScanComplexFloat *
hyscan_acoustic_data_get_signal (HyScanAcousticData *data,
                                 guint32             index,
                                 guint32            *n_points,
                                 gint64             *time)
{
  HyScanAcousticDataPrivate *priv;
  HyScanAcousticDataSignal *signal;

  g_return_val_if_fail (HYSCAN_IS_ACOUSTIC_DATA (data), NULL);

  priv = data->priv;

  if (priv->channel_id <= 0)
    return NULL;

  hyscan_acoustic_data_load_signals (priv);
  signal = hyscan_acoustic_data_find_signal (priv, index);
  if ((signal == NULL) || (signal->image == NULL))
    return NULL;

  (time != NULL) ? *time = signal->time : 0;
  *n_points = signal->n_points;

  return signal->image;
}

/**
 * hyscan_acoustic_data_get_tvg:
 * @data: указатель на #HyScanAcousticData
 * @index: индекс считываемых данныx
 * @n_points: (out): число коэффициентов усиления
 * @time: (out) (nullable): метка времени начала действия коэффициентов усиления
 *
 * Функция возвращает значения коэффициентов усиления для указанного индекса
 * данных.
 *
 * Функция возвращает указатель на внутренний буфер, данные в котором
 * действительны до следующего вызова функций HyScanAcousticData. Пользователь
 * не должен модифицировать эти данные.
 *
 * Returns: (nullable) (array length=n_points) (transfer none):
 *          Коэффициенты усиления или NULL.
 */
const gfloat *
hyscan_acoustic_data_get_tvg (HyScanAcousticData *data,
                              guint32             index,
                              guint32            *n_points,
                              gint64             *time)
{
  HyScanAcousticDataPrivate *priv;

  HyScanDBFindStatus find_status;
  guint32 last_index;
  guint32 left_index;

  guint32 tvg_index;
  gint64 tvg_time;

  g_return_val_if_fail (HYSCAN_IS_ACOUSTIC_DATA (data), NULL);

  priv = data->priv;

  if (priv->tvg_id <= 0)
    return NULL;

  /* Проверяем наличие данных ВАРУ в кэше. */
  if (hyscan_acoustic_data_check_data_cache (priv, DATA_TYPE_TVG, index))
    {
      (time != NULL) ? *time = priv->data_time : 0;
      return hyscan_buffer_get_float (priv->real_buffer, n_points);
    }

  /* Ищем индекс записи с нужными коэффициентами ВАРУ. */
  tvg_time = hyscan_db_channel_get_data_time (priv->db, priv->channel_id, index);
  if (tvg_time < 0)
    return NULL;

  /* Последний индекс в канале ВАРУ. */
  if (!hyscan_db_channel_get_data_range (priv->db, priv->tvg_id, NULL, &last_index))
    return NULL;

  find_status = hyscan_db_channel_find_data (priv->db, priv->tvg_id, tvg_time,
                                             &left_index, NULL, NULL, NULL);

  /* Нашли точный индекс данных или попали между двумя изменениями ВАРУ.
   * Нам нужен меньший индекс. */
  if (find_status == HYSCAN_DB_FIND_OK)
    tvg_index = left_index;

  /* Данные записаны после крайнего изменения коэффициентов ВАРУ.
   * Используем последний индекс. */
  else if (find_status == HYSCAN_DB_FIND_GREATER)
    tvg_index = last_index;

  /* Ошибка поиска или данные записаны раньше чем существующие коэффициенты ВАРУ. */
  else
    return NULL;

  /* Если в кэше ничего нет, считываем данные из базы. */
  if (!hyscan_db_channel_get_data (priv->db, priv->tvg_id, tvg_index, priv->channel_buffer, &priv->data_time))
    return NULL;

  hyscan_buffer_set_data_type (priv->channel_buffer, HYSCAN_DATA_FLOAT32LE);
  if (!hyscan_buffer_import (priv->real_buffer, priv->channel_buffer))
    return NULL;

  /* Сохраняем данные в кэше. */
  if (priv->cache != NULL)
    {
      HyScanAcousticDataCacheHeader header;

      header.magic = CACHE_DATA_MAGIC;
      header.n_points = hyscan_buffer_get_data_size (priv->real_buffer);
      header.n_points /= sizeof (gfloat);
      header.time = priv->data_time;
      hyscan_buffer_wrap (priv->cache_buffer, HYSCAN_DATA_BLOB, &header, sizeof (header));

      hyscan_acoustic_data_update_cache_key (priv, DATA_TYPE_TVG, index);
      hyscan_cache_set2 (priv->cache, priv->cache_key->str, NULL,
                         priv->cache_buffer, priv->real_buffer);
    }

  (time != NULL) ? *time = priv->data_time : 0;

  return hyscan_buffer_get_float (priv->real_buffer, n_points);
}

/**
 * hyscan_acoustic_data_get_real:
 * @data: указатель на #HyScanAcousticData
 * @index: индекс считываемых данныx
 * @n_points: (out): число точек данныx
 * @time: (out) (nullable): метка времени данныx
 *
 * Функция возвращает значения действительных данных. Эту функцию можно
 * использовать, только если данные имеют тип дискретизации
 * #HYSCAN_DISCRETIZATION_REAL.
 *
 * Функция возвращает указатель на внутренний буфер, данные в котором
 * действительны до следующего вызова функций HyScanAcousticData. Пользователь
 * не должен модифицировать эти данные.
 *
 * Returns: (nullable) (array length=n_points) (transfer none):
 *          Значения действительных данных или NULL.
 */
const gfloat *
hyscan_acoustic_data_get_real (HyScanAcousticData *data,
                               guint32             index,
                               guint32            *n_points,
                               gint64             *time)
{
  HyScanAcousticDataPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_ACOUSTIC_DATA (data), NULL);

  priv = data->priv;

  if (priv->channel_id <= 0)
    return NULL;

  if (priv->discretization != HYSCAN_DISCRETIZATION_REAL)
    return NULL;

  /* Проверяем наличие действительных данных в кэше. */
  if (hyscan_acoustic_data_check_data_cache (priv, DATA_TYPE_REAL, index))
    {
      (time != NULL) ? *time = priv->data_time : 0;

      return hyscan_buffer_get_float (priv->real_buffer, n_points);
    }

  /* Если в кэше ничего нет, считываем данные из базы. */
  if (!hyscan_acoustic_data_read_channel_data (priv, index))
    return NULL;

  /* Сохраняем данные в кэше. */
  if (priv->cache != NULL)
    {
      HyScanAcousticDataCacheHeader header;

      header.magic = CACHE_DATA_MAGIC;
      header.n_points = hyscan_buffer_get_data_size (priv->real_buffer);
      header.n_points /= sizeof (gfloat);
      header.time = priv->data_time;

      hyscan_acoustic_data_update_cache_key (priv, DATA_TYPE_REAL, index);
      hyscan_buffer_wrap (priv->cache_buffer, HYSCAN_DATA_BLOB, &header, sizeof (header));
      hyscan_cache_set2 (priv->cache, priv->cache_key->str, NULL,
                         priv->cache_buffer, priv->real_buffer);
    }

  (time != NULL) ? *time = priv->data_time : 0;

  return hyscan_buffer_get_float (priv->real_buffer, n_points);
}

/**
 * hyscan_acoustic_data_get_complex:
 * @data: указатель на #HyScanAcousticData
 * @index: индекс считываемых данныx
 * @n_points: (out): число точек данныx
 * @time: (out) (nullable): метка времени данныx
 *
 * Функция возвращает комплексные значения данных. Эту функцию нельзя
 * использовать для типа дискретизации #HYSCAN_DISCRETIZATION_AMPLITUDE.
 *
 * Функция возвращает указатель на внутренний буфер, данные в котором
 * действительны до следующего вызова функций HyScanAcousticData. Пользователь
 * не должен модифицировать эти данные.
 *
 * Returns: (nullable) (array length=n_points) (transfer none):
 *          Комплексные значения данных или NULL.
 */
const HyScanComplexFloat *
hyscan_acoustic_data_get_complex (HyScanAcousticData    *data,
                                  guint32                index,
                                  guint32               *n_points,
                                  gint64                *time)
{
  HyScanAcousticDataPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_ACOUSTIC_DATA (data), NULL);

  priv = data->priv;

  if (priv->channel_id <= 0)
    return NULL;

  if (priv->discretization == HYSCAN_DISCRETIZATION_AMPLITUDE)
    return NULL;

  /* Проверяем наличие комплексных данных в кэше. */
  if (hyscan_acoustic_data_check_data_cache (priv, DATA_TYPE_COMPLEX, index))
    {
      (time != NULL) ? *time = priv->data_time : 0;

      return hyscan_buffer_get_complex_float (priv->complex_buffer, n_points);
    }

  /* Если в кэше ничего нет, считываем данные из базы. */
  if (!hyscan_acoustic_data_read_channel_data (priv, index))
    return NULL;

  /* Преобразуем действительные отсчёты в комплексные. */
  if (priv->discretization == HYSCAN_DISCRETIZATION_REAL)
    {
      if (!hyscan_acoustic_data_real2complex (priv))
        return NULL;
    }

  /* Свёртка данных. */
  if (!hyscan_acoustic_data_convolution (priv, index))
    return NULL;

  /* Сохраняем данные в кэше. */
  if (priv->cache != NULL)
    {
      HyScanAcousticDataCacheHeader header;

      header.magic = CACHE_DATA_MAGIC;
      header.n_points = hyscan_buffer_get_data_size (priv->complex_buffer);
      header.n_points /= sizeof (HyScanComplexFloat);
      header.time = priv->data_time;

      hyscan_buffer_wrap (priv->cache_buffer, HYSCAN_DATA_BLOB, &header, sizeof (header));
      hyscan_cache_set2 (priv->cache, priv->cache_key->str, NULL,
                         priv->cache_buffer, priv->complex_buffer);
    }

  (time != NULL) ? *time = priv->data_time : 0;

  return hyscan_buffer_get_complex_float (priv->complex_buffer, n_points);
}

/**
 * hyscan_acoustic_data_get_amplitude:
 * @data: указатель на #HyScanAcousticData
 * @index: индекс считываемых данныx
 * @n_points: (out): число точек данныx
 * @time: (out) (nullable): метка времени данныx
 *
 * Функция возвращает значения амплитуды данных.
 *
 * Функция возвращает указатель на внутренний буфер, данные в котором
 * действительны до следующего вызова функций HyScanAcousticData. Пользователь
 * не должен модифицировать эти данные.
 *
 * Returns: (nullable) (array length=n_points) (transfer none):
 *          Значения амплитуды данных или NULL.
 */
const gfloat *
hyscan_acoustic_data_get_amplitude (HyScanAcousticData *data,
                                    guint32             index,
                                    guint32            *n_points,
                                    gint64             *time)
{
  HyScanAcousticDataPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_ACOUSTIC_DATA (data), NULL);

  priv = data->priv;

  if (priv->channel_id <= 0)
    return NULL;

  /* Проверяем наличие амплитудных данных в кэше. */
  if (hyscan_acoustic_data_check_data_cache (priv, DATA_TYPE_AMPLITUDE, index))
    {
      (time != NULL) ? *time = priv->data_time : 0;

      return hyscan_buffer_get_float (priv->real_buffer, n_points);
    }

  /* Проверяем наличие комплексных данных в кэше,
   * если в кэше ничего нет, считываем данные из базы. */
  if (!hyscan_acoustic_data_check_data_cache (priv, DATA_TYPE_COMPLEX, index))
    {
      if (!hyscan_acoustic_data_read_channel_data (priv, index))
        return NULL;

      /* Преобразуем действительные отсчёты в комплексные. */
      if (priv->discretization == HYSCAN_DISCRETIZATION_REAL)
        {
          if (!hyscan_acoustic_data_real2complex (priv))
            return NULL;
        }

      /* Свёртка данных. */
      if (!hyscan_acoustic_data_convolution (priv, index))
        return NULL;
    }

  /* Расчёт амлитуды. */
  if (!hyscan_acoustic_data_calc_amplitude (priv))
    return NULL;

  /* Сохраняем данные в кэше. */
  if (priv->cache != NULL)
    {
      HyScanAcousticDataCacheHeader header;

      header.magic = CACHE_DATA_MAGIC;
      header.n_points = hyscan_buffer_get_data_size (priv->real_buffer);
      header.n_points /= sizeof (gfloat);
      header.time = priv->data_time;

      hyscan_acoustic_data_update_cache_key (priv, DATA_TYPE_AMPLITUDE, index);
      hyscan_buffer_wrap (priv->cache_buffer, HYSCAN_DATA_BLOB, &header, sizeof (header));
      hyscan_cache_set2 (priv->cache, priv->cache_key->str, NULL,
                         priv->cache_buffer, priv->real_buffer);
    }

  (time != NULL) ? *time = priv->data_time : 0;

  return hyscan_buffer_get_float (priv->real_buffer, n_points);
}

static const gchar *
hyscan_acoustic_data_amplitude_get_token (HyScanAmplitude *amplitude)
{
  HyScanAcousticData *data = HYSCAN_ACOUSTIC_DATA (amplitude);

  return data->priv->cache_token;
}

static HyScanAntennaOffset
hyscan_acoustic_data_amplitude_get_offset (HyScanAmplitude *amplitude)
{
  HyScanAcousticData *data = HYSCAN_ACOUSTIC_DATA (amplitude);

  return hyscan_acoustic_data_get_offset (data);
}

static HyScanAcousticDataInfo
hyscan_acoustic_data_amplitude_get_info (HyScanAmplitude *amplitude)
{
  HyScanAcousticData *data = HYSCAN_ACOUSTIC_DATA (amplitude);

  return hyscan_acoustic_data_get_info (data);
}

static gboolean
hyscan_acoustic_data_amplitude_is_writable (HyScanAmplitude *amplitude)
{
  HyScanAcousticData *data = HYSCAN_ACOUSTIC_DATA (amplitude);

  return hyscan_acoustic_data_is_writable (data);
}

static guint32
hyscan_acoustic_data_amplitude_get_mod_count (HyScanAmplitude *amplitude)
{
  HyScanAcousticData *data = HYSCAN_ACOUSTIC_DATA (amplitude);

  return hyscan_acoustic_data_get_mod_count (data);
}

static gboolean
hyscan_acoustic_data_amplitude_get_range (HyScanAmplitude *amplitude,
                                          guint32         *first_index,
                                          guint32         *last_index)
{
  HyScanAcousticData *data = HYSCAN_ACOUSTIC_DATA (amplitude);

  return hyscan_acoustic_data_get_range (data, first_index, last_index);
}

static HyScanDBFindStatus
hyscan_acoustic_data_amplitude_find_data (HyScanAmplitude *amplitude,
                                          gint64           time,
                                          guint32         *lindex,
                                          guint32         *rindex,
                                          gint64          *ltime,
                                          gint64          *rtime)
{
  HyScanAcousticData *data = HYSCAN_ACOUSTIC_DATA (amplitude);

  return hyscan_acoustic_data_find_data (data, time, lindex, rindex, ltime, rtime);
}

static gboolean
hyscan_acoustic_data_amplitude_get_size_time (HyScanAmplitude *amplitude,
                                              guint32          index,
                                              guint32         *n_points,
                                              gint64          *time)
{
  HyScanAcousticData *data = HYSCAN_ACOUSTIC_DATA (amplitude);

  return hyscan_acoustic_data_get_size_time (data, index, n_points, time);
}

static const gfloat *
hyscan_acoustic_data_amplitude_get_amplitude (HyScanAmplitude *amplitude,
                                              guint32          index,
                                              guint32         *n_points,
                                              gint64          *time,
                                              gboolean        *noise)
{
  HyScanAcousticData *data = HYSCAN_ACOUSTIC_DATA (amplitude);

  (noise != NULL) ? *noise = data->priv->noise : 0;

  return hyscan_acoustic_data_get_amplitude (data, index, n_points, time);
}

static void
hyscan_acoustic_data_interface_init (HyScanAmplitudeInterface *iface)
{
  iface->get_token = hyscan_acoustic_data_amplitude_get_token;
  iface->get_offset = hyscan_acoustic_data_amplitude_get_offset;
  iface->get_info = hyscan_acoustic_data_amplitude_get_info;
  iface->is_writable = hyscan_acoustic_data_amplitude_is_writable;
  iface->get_mod_count = hyscan_acoustic_data_amplitude_get_mod_count;
  iface->get_range = hyscan_acoustic_data_amplitude_get_range;
  iface->find_data = hyscan_acoustic_data_amplitude_find_data;
  iface->get_size_time = hyscan_acoustic_data_amplitude_get_size_time;
  iface->get_amplitude = hyscan_acoustic_data_amplitude_get_amplitude;
}
