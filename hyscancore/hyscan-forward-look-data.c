/* hyscan-forward-look-data.c
 *
 * Copyright 2017-2018 Screen LLC, Andrei Fadeev <andrei@webcontrol.ru>
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
 * Contact the Screen LLC in this case - info@screen-co.ru
 */

/* HyScanCore имеет двойную лицензию.
 *
 * Во-первых, вы можете распространять HyScanCore на условиях Стандартной
 * Общественной Лицензии GNU версии 3, либо по любой более поздней версии
 * лицензии (по вашему выбору). Полные положения лицензии GNU приведены в
 * <http://www.gnu.org/licenses/>.
 *
 * Во-вторых, этот программный код можно использовать по коммерческой
 * лицензии. Для этого свяжитесь с ООО Экран - info@screen-co.ru.
 */

/**
 * SECTION: hyscan-forward-look-data
 * @Short_description: класс обработки данных вперёдсмотрящего локатора
 * @Title: HyScanForwardLookData
 *
 * Класс HyScanForwardLookData используется для получения обработанных данных
 * вперёдсмотрящего локатора, таких как угол и дистанция до цели, а также
 * интенсивности отражения. Данные возвращаются в виде массива структур
 * #HyScanForwardLookDOA.
 *
 * Создание объекта производится с помощью функции #hyscan_forward_look_data_new.
 *
 * В своей работе класс HyScanForwardLookData может использовать внешний кэш
 * для хранения обработанных данных. В случае запроса ранее обработанных данных
 * пользователь получит их копию из кэша. Несколько экземпляров класса
 * HyScanForwardLookData, обрабатывающих одинаковые каналы данных могут
 * использовать один и тот же кэш. Таким образом, данные обработанные в одной
 * части приложения не потребуют повторной обработки в другой его части.
 *
 * Функции #hyscan_forward_look_data_get_db,
 * #hyscan_forward_look_data_get_project_name,
 * #hyscan_forward_look_data_get_track_name,
 * #hyscan_forward_look_data_get_position,
 * #hyscan_forward_look_data_is_writable и
 * #hyscan_forward_look_data_get_alpha предназначены для получения информации
 * о канале данных и типа данных в нём.
 *
 * Функции #hyscan_forward_look_data_get_mod_count,
 * #hyscan_forward_look_data_get_range и
 * #hyscan_forward_look_data_find_data предназначены для определения границ
 * записанных данных и их поиска по метке времени. Они аналогичны функциям
 * #hyscan_db_get_mod_count, #hyscan_db_channel_get_data_range и
 * #hyscan_db_channel_find_data интерфейса #HyScanDB.
 *
 * Для точной обработки данных необходимо установить скорость звука в воде,
 * для этих целей используется функция
 * #hyscan_forward_look_data_set_sound_velocity. По умолчанию используется
 * значение 1500 м/с.
 *
 * Для чтения и обработки данных используются функции
 * #hyscan_forward_look_data_get_size_time и
 * #hyscan_forward_look_data_get_doa_values.
 *
 * HyScanForwardLookData не поддерживает работу в многопоточном режиме.
 * Рекомендуется создавать свой экземпляр объекта обработки данных в каждом
 * потоке и использовать единый кэш данных.
 */

#include "hyscan-forward-look-data.h"
#include "hyscan-acoustic-data.h"
#include "hyscan-core-schemas.h"

#include <string.h>
#include <math.h>

#define DOUBLE_STR_LENGTH      64                      /* Максимальная длина строки с числом типа double. */
#define CACHE_HEADER_MAGIC     0x8a09be31              /* Идентификатор заголовка кэша. */

enum
{
  PROP_O,
  PROP_DB,
  PROP_CACHE,
  PROP_PROJECT_NAME,
  PROP_TRACK_NAME
};

/* Структруа заголовка кэша. */
typedef struct
{
  guint32                      magic;                  /* Идентификатор заголовка. */
  guint32                      n_points;               /* Число точек данных. */
  gint64                       time;                   /* Метка времени. */
} HyScanForwardLookDataCacheHeader;

struct _HyScanForwardLookDataPrivate
{
  HyScanDB                    *db;                     /* Интерфейс базы данных. */
  gchar                       *db_uri;                 /* URI базы данных. */

  gchar                       *project_name;           /* Название проекта. */
  gchar                       *track_name;             /* Название галса. */

  gdouble                      data_rate;              /* Частота дискретизации. */
  gdouble                      sound_velocity;         /* Скорость звука, м/с. */
  gdouble                      antenna_base;           /* Расстояние между антеннами, м. */
  gdouble                      frequency;              /* Центральная рабочая частота, Гц. */
  gdouble                      wave_length;            /* Длина волны, м. */
  gdouble                      alpha;                  /* Максимальный/минимальный угол по азимуту, рад. */

  HyScanAcousticData          *channel1;               /* Данные канала 1. */
  HyScanAcousticData          *channel2;               /* Данные канала 2. */

  HyScanBuffer                *doa_buffer;             /* Буфер обработанных данных. */
  HyScanBuffer                *cache_buffer;           /* Буфер заголовка кэша данных. */

  HyScanCache                 *cache;                  /* Интерфейс системы кэширования. */
  gchar                       *cache_token;            /* Основа ключа кэширования. */
  gchar                       *cache_key;              /* Ключ кэширования. */
  gsize                        cache_key_length;       /* Максимальная длина ключа кэширования. */
  gchar                       *detail_key;             /* Ключ детализации кэширования. */
};

static void    hyscan_forward_look_data_set_property           (GObject                       *object,
                                                                guint                          prop_id,
                                                                const GValue                  *value,
                                                                GParamSpec                    *pspec);
static void    hyscan_forward_look_data_object_constructed     (GObject                       *object);
static void    hyscan_forward_look_data_object_finalize        (GObject                       *object);

static void    hyscan_forward_look_data_update_cache_key       (HyScanForwardLookDataPrivate  *priv,
                                                                guint32                        index);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanForwardLookData, hyscan_forward_look_data, G_TYPE_OBJECT)

static void
hyscan_forward_look_data_class_init (HyScanForwardLookDataClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_forward_look_data_set_property;

  object_class->constructed = hyscan_forward_look_data_object_constructed;
  object_class->finalize = hyscan_forward_look_data_object_finalize;

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
}

static void
hyscan_forward_look_data_init (HyScanForwardLookData *data)
{
  data->priv = hyscan_forward_look_data_get_instance_private (data);
}

static void
hyscan_forward_look_data_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  HyScanForwardLookData *data = HYSCAN_FORWARD_LOOK_DATA (object);
  HyScanForwardLookDataPrivate *priv = data->priv;

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

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_forward_look_data_object_constructed (GObject *object)
{
  HyScanForwardLookData *data = HYSCAN_FORWARD_LOOK_DATA (object);
  HyScanForwardLookDataPrivate *priv = data->priv;

  HyScanAcousticDataInfo channel_info1;
  HyScanAcousticDataInfo channel_info2;

  gchar *db_uri;

  if (priv->db == NULL)
    {
      g_warning ("HyScanForwardLookData: db is not specified");
      return;
    }
  if (priv->project_name == NULL)
    {
      g_warning ("HyScanForwardLookData: empty project name");
      return;
    }
  if (priv->track_name == NULL)
    {
      g_warning ("HyScanForwardLookData: empty track name");
      return;
    }

  priv->db_uri = hyscan_db_get_uri (priv->db);

  /* Каналы данных. */
  priv->channel1 = hyscan_acoustic_data_new (priv->db, NULL, priv->project_name, priv->track_name,
                                             HYSCAN_SOURCE_FORWARD_LOOK, 1, FALSE);
  priv->channel2 = hyscan_acoustic_data_new (priv->db, NULL, priv->project_name, priv->track_name,
                                             HYSCAN_SOURCE_FORWARD_LOOK, 2, FALSE);
  if ((priv->channel1 == NULL) || (priv->channel2 == NULL))
    {
      g_clear_object (&priv->channel1);
      g_clear_object (&priv->channel2);
      return;
    }

  /* Параметры каналов данных. */
  channel_info1 = hyscan_acoustic_data_get_info (priv->channel1);
  channel_info2 = hyscan_acoustic_data_get_info (priv->channel2);

  /* Проверяем параметры каналов данных.
   * Должны быть указаны рабочая частота и база антенны, а также должны
   * совпадать рабочие частоты каналов и частоты оцифровки данных.*/
  if ((channel_info1.antenna_frequency < 1.0) ||
      (fabs (channel_info1.antenna_hoffset - channel_info2.antenna_hoffset) < 1e-4) ||
      (fabs (channel_info1.data_rate - channel_info2.data_rate) > 0.1) ||
      (fabs (channel_info1.antenna_frequency - channel_info2.antenna_frequency) > 0.1))
    {
      g_warning ("HyScanForwardLookData: error in channels parameters");
      g_clear_object (&priv->channel1);
      g_clear_object (&priv->channel2);

      return;
    }

  /* Буферы для обработки. */
  priv->cache_buffer = hyscan_buffer_new ();
  priv->doa_buffer = hyscan_buffer_new ();

  /* Параметры обработки. */
  priv->data_rate = channel_info1.data_rate;
  priv->antenna_base = channel_info2.antenna_hoffset - channel_info1.antenna_hoffset;
  priv->frequency = channel_info1.antenna_frequency;
  hyscan_forward_look_data_set_sound_velocity (data, 1500.0);

  db_uri = hyscan_db_get_uri (priv->db);
  priv->cache_token = g_strdup_printf ("FORWARDLOOK.%s.%s.%s",
                                       db_uri,
                                       priv->project_name,
                                       priv->track_name);
  g_free (db_uri);
}

static void
hyscan_forward_look_data_object_finalize (GObject *object)
{
  HyScanForwardLookData *data = HYSCAN_FORWARD_LOOK_DATA (object);
  HyScanForwardLookDataPrivate *priv = data->priv;

  g_clear_object (&priv->cache_buffer);
  g_clear_object (&priv->doa_buffer);

  g_clear_object (&priv->cache);
  g_clear_object (&priv->channel1);
  g_clear_object (&priv->channel2);
  g_clear_object (&priv->db);

  g_free (priv->project_name);
  g_free (priv->track_name);
  g_free (priv->db_uri);

  g_free (priv->cache_token);
  g_free (priv->detail_key);
  g_free (priv->cache_key);

  G_OBJECT_CLASS (hyscan_forward_look_data_parent_class)->finalize (object);
}

/* Функция создаёт ключ кэширования данных. */
static void
hyscan_forward_look_data_update_cache_key (HyScanForwardLookDataPrivate *priv,
                                           guint32                       index)
{
  gchar svv[64];

  if (priv->cache_key == NULL)
    {
      priv->cache_key = g_strdup_printf ("%s.%u", priv->cache_token, G_MAXUINT32);
      priv->cache_key_length = strlen (priv->cache_key);
      priv->detail_key = g_malloc (DOUBLE_STR_LENGTH);
    }

  g_snprintf (priv->cache_key, priv->cache_key_length, "%s.%u", priv->cache_token, index);

  g_ascii_dtostr (svv, sizeof (svv), priv->sound_velocity);
  g_snprintf (priv->detail_key, DOUBLE_STR_LENGTH, "%s", svv);
}


/**
 * hyscan_forward_look_data_new:
 * @db: указатель на #HyScanDB
 * @cache: (nullable): указатель на #HyScanCache
 * @project_name: название проекта
 * @track_name: название галса
 *
 * Функция создаёт новый объект #HyScanForwardLookData. В случае ошибки
 * функция вернёт значение NULL.
 *
 * Returns: #HyScanForwardLookData или NULL. Для удаления #g_object_unref.
 */
HyScanForwardLookData *
hyscan_forward_look_data_new (HyScanDB    *db,
                              HyScanCache *cache,
                              const gchar *project_name,
                              const gchar *track_name)
{
  HyScanForwardLookData *data;

  data = g_object_new (HYSCAN_TYPE_FORWARD_LOOK_DATA,
                       "db", db,
                       "cache", cache,
                       "project-name", project_name,
                       "track-name", track_name,
                       NULL);

  if ((data->priv->channel1 == NULL) || (data->priv->channel2 == NULL))
    g_clear_object (&data);

  return data;
}

/**
 * hyscan_forward_look_data_get_db:
 * @data: указатель на #HyScanForwardLookData
 *
 * Функция возвращает указатель на используемый #HyScanDB.
 *
 * Returns: #HyScanDB. Для удаления #g_object_unref.
 */
HyScanDB *
hyscan_forward_look_data_get_db (HyScanForwardLookData *data)
{
  HyScanForwardLookDataPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_FORWARD_LOOK_DATA (data), NULL);

  priv = data->priv;

  return (priv->channel1 != NULL) ? g_object_ref (priv->db) : NULL;
}

/**
 * hyscan_forward_look_data_get_project_name:
 * @data: указатель на #HyScanForwardLookData
 *
 * Функция возвращает название используемого проекта.
 *
 * Returns: Название проекта.
 */
const gchar *
hyscan_forward_look_data_get_project_name (HyScanForwardLookData *data)
{
  HyScanForwardLookDataPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_FORWARD_LOOK_DATA (data), NULL);

  priv = data->priv;

  return (priv->channel1 != NULL) ? priv->project_name : NULL;
}

/**
 * hyscan_forward_look_data_get_track_name:
 * @data: указатель на #HyScanForwardLookData
 *
 * Функция возвращает название используемого галса.
 *
 * Returns: Название галса.
 */
const gchar *
hyscan_forward_look_data_get_track_name (HyScanForwardLookData *data)
{
  HyScanForwardLookDataPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_FORWARD_LOOK_DATA (data), NULL);

  priv = data->priv;

  return (priv->channel1 != NULL) ? priv->track_name : NULL;
}

/**
 * hyscan_forward_look_data_get_position:
 * @data: указатель на #HyScanForwardLookData
 *
 * Функция возвращает информацию о местоположении приёмной
 * антенны гидролокатора.
 *
 * Returns: Местоположение приёмной антенны.
 */
HyScanAntennaPosition
hyscan_forward_look_data_get_position (HyScanForwardLookData *data)
{
  HyScanForwardLookDataPrivate *priv;
  HyScanAntennaPosition zero = {0};

  g_return_val_if_fail (HYSCAN_IS_FORWARD_LOOK_DATA (data), zero);

  priv = data->priv;

  if (priv->channel1 == NULL)
    return zero;

  return hyscan_acoustic_data_get_position (priv->channel1);
}

/**
 * hyscan_forward_look_data_is_writable:
 * @data: указатель на #HyScanForwardLookData
 *
 * Функция определяет возможность изменения данных. Если функция вернёт
 * значение TRUE, возможна ситуация когда могут появиться новые данные
 * или исчезнуть уже записанные.
 *
 * Returns: %TRUE если в канал возможна запись данных, иначе %FALSE.
 */
gboolean
hyscan_forward_look_data_is_writable (HyScanForwardLookData *data)
{
  HyScanForwardLookDataPrivate *priv;
  gboolean is_writable1;
  gboolean is_writable2;

  g_return_val_if_fail (HYSCAN_IS_FORWARD_LOOK_DATA (data), FALSE);

  priv = data->priv;

  if (priv->channel1 == NULL)
    return FALSE;

  is_writable1 = hyscan_acoustic_data_is_writable (priv->channel1);
  is_writable2 = hyscan_acoustic_data_is_writable (priv->channel2);

  return (is_writable1 | is_writable2);
}

/**
 * hyscan_forward_look_data_get_alpha:
 * @data: указатель на #HyScanForwardLookData
 *
 * Функция возвращает угол, определяющий сектор обзора локатора в
 * горизонтальной плоскости. Сектор обзора составляет [-angle +angle],
 * где angle - значение возвращаемой этой функцией. Сектор обзора зависит
 * от скорости звука.
 *
 * Returns: Угол сектора обзора, рад.
 */
gdouble
hyscan_forward_look_data_get_alpha (HyScanForwardLookData *data)
{
  g_return_val_if_fail (HYSCAN_IS_FORWARD_LOOK_DATA (data), 0.0);

  return data->priv->alpha;
}

/**
 * hyscan_forward_look_data_get_mod_count:
 * @data: указатель на #HyScanForwardLookData
 *
 * Функция возвращает номер изменения в данных. Программа не должна полагаться
 * на значение номера изменения, важен только факт смены номера по сравнению с
 * предыдущим запросом.
 *
 * Returns: Номер изменения данных.
 */
guint32
hyscan_forward_look_data_get_mod_count (HyScanForwardLookData *data)
{
  HyScanForwardLookDataPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_FORWARD_LOOK_DATA (data), 0);

  priv = data->priv;

  if (priv->channel1 == NULL)
    return 0;

  return hyscan_acoustic_data_get_mod_count (priv->channel1);
}

/**
 * hyscan_forward_look_data_get_range:
 * @data: указатель на #HyScanForwardLookData
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
hyscan_forward_look_data_get_range (HyScanForwardLookData *data,
                                    guint32               *first_index,
                                    guint32               *last_index)
{
  HyScanForwardLookDataPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_FORWARD_LOOK_DATA (data), FALSE);

  priv = data->priv;

  if (priv->channel1 == NULL)
    return FALSE;

  return hyscan_acoustic_data_get_range (priv->channel1, first_index, last_index);
}

/**
 * hyscan_forward_look_data_find_data:
 * @data: указатель на #HyScanForwardLookData
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
hyscan_forward_look_data_find_data (HyScanForwardLookData *data,
                                    gint64                 time,
                                    guint32               *lindex,
                                    guint32               *rindex,
                                    gint64                *ltime,
                                    gint64                *rtime)
{
  HyScanForwardLookDataPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_FORWARD_LOOK_DATA (data), HYSCAN_DB_FIND_FAIL);

  priv = data->priv;

  if (priv->channel1 == NULL)
    return HYSCAN_DB_FIND_FAIL;

  return hyscan_acoustic_data_find_data (priv->channel1, time, lindex, rindex, ltime, rtime);
}

/**
 * hyscan_forward_look_data_set_sound_velocity:
 * @data: указатель на #HyScanForwardLookData
 * @sound_velocity: скорость звука, м/c
 *
 * Функция задаёт скорость звука в воде, используемую для обработки данных.
 */
void
hyscan_forward_look_data_set_sound_velocity (HyScanForwardLookData *data,
                                             gdouble                sound_velocity)
{
  HyScanForwardLookDataPrivate *priv;

  g_return_if_fail (HYSCAN_IS_FORWARD_LOOK_DATA (data));

  priv = data->priv;

  if (sound_velocity <= 0.0)
    return;

  priv->sound_velocity = sound_velocity;
  priv->wave_length = sound_velocity / priv->frequency;
  priv->alpha = asin (sound_velocity / (2.0 * priv->antenna_base * priv->frequency));
  priv->alpha = ABS (priv->alpha);
}

/**
 * hyscan_forward_look_data_get_size_time:
 * @data: указатель на #HyScanForwardLookData
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
hyscan_forward_look_data_get_size_time (HyScanForwardLookData *data,
                                        guint32                index,
                                        guint32               *n_points,
                                        gint64                *time)
{
  HyScanForwardLookDataPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_FORWARD_LOOK_DATA (data), FALSE);

  priv = data->priv;

  if (priv->channel1 == NULL)
    return FALSE;

  return hyscan_acoustic_data_get_size_time (priv->channel1, index, n_points, time);
}

/**
 * hyscan_forward_look_data_get_doa:
 * @data: указатель на #HyScanForwardLookData
 * @index: индекс считываемых данныx
 * @n_points: (out): число точек данныx
 * @time: (out) (nullable): метка времени данныx
 *
 * Функция возвращает данные вперёдсмотрящего локатора.
 *
 * Функция возвращает указатель на внутренний буфер, данные в котором
 * действительны до следующего вызова функций HyScanForwardLookData.
 * Пользователь не должен модифицировать эти данные.
 *
 * Returns: (nullable) (array length=n_points) (transfer none):
 *          Данные вперёдсмотрящего локатора или NULL.
 */
const HyScanForwardLookDOA *
hyscan_forward_look_data_get_doa (HyScanForwardLookData *data,
                                  guint32                index,
                                  guint32               *n_points,
                                  gint64                *time)

{
  HyScanForwardLookDataPrivate *priv;
  HyScanDBFindStatus find_status;
  const HyScanComplexFloat *data1;
  const HyScanComplexFloat *data2;
  HyScanForwardLookDOA *doa;

  guint32 n_points1, n_points2;
  guint32 index1, index2;
  gint64 time1, time2;

  guint32 i;

  g_return_val_if_fail (HYSCAN_IS_FORWARD_LOOK_DATA (data), NULL);

  priv = data->priv;

  if (priv->channel1 == NULL)
    return NULL;

  /* Ищем данные в кэше. */
  if (priv->cache != NULL)
    {
      HyScanForwardLookDataCacheHeader header;

      /* Ключ кэширования. */
      hyscan_forward_look_data_update_cache_key (priv, index);

      /* Ищем данные в кэше. */
      hyscan_buffer_wrap_data (priv->cache_buffer, HYSCAN_DATA_BLOB, &header, sizeof (header));
      if (hyscan_cache_get2 (priv->cache, priv->cache_key, priv->detail_key,
                             sizeof (header), priv->cache_buffer, priv->doa_buffer))
        {
          guint32 cached_n_points;

          cached_n_points  = hyscan_buffer_get_size (priv->doa_buffer);
          cached_n_points /= sizeof (HyScanForwardLookDOA);

          /* Верификация данных. */
          if ((header.magic == CACHE_HEADER_MAGIC) &&
              (header.n_points == cached_n_points))
            {
              (time != NULL) ? *time = header.time : 0;
              *n_points = cached_n_points;

              return hyscan_buffer_get_data (priv->doa_buffer, &cached_n_points);
            }
        }
    }

  /* Считываем данные первого канала. */
  index1 = index;
  data1 = hyscan_acoustic_data_get_complex (priv->channel1, index1, &n_points1, &time1);
  if (data1 == NULL)
    return NULL;

  /* Ищем парную строку для указанного индекса. */
  find_status = hyscan_acoustic_data_find_data (priv->channel2, time1, &index2, NULL, &time2, NULL);
  if ((find_status != HYSCAN_DB_FIND_OK) || (time1 != time2))
    return NULL;

  /* Считываем данные второго канала. */
  data2 = hyscan_acoustic_data_get_complex (priv->channel2, index2, &n_points2, NULL);
  if (data2 == NULL)
    return NULL;

  if (n_points1 != n_points2)
    {
      g_warning ("HyScanForwardLookData: data size mismatch in '%s.%s' for index %d",
                 priv->project_name, priv->track_name, index);
    }

  /* Корректируем размер буфера данных. */
  *n_points = n_points1 = n_points2 = MIN (n_points1, n_points2);
  hyscan_buffer_set_size (priv->doa_buffer, n_points1 * sizeof (HyScanForwardLookDOA));

  /* Расчитываем углы прихода и интенсивности. */
  doa = hyscan_buffer_get_data (priv->doa_buffer, &n_points1);
  n_points1 /= sizeof (HyScanForwardLookDOA);
  for (i = 0; i < n_points1; i++)
    {
      gfloat re1 = data1[i].re;
      gfloat im1 = data1[i].im;
      gfloat re2 = data2[i].re;
      gfloat im2 = data2[i].im;

      gfloat re = re1 * re2 - im1 * -im2;
      gfloat im = re1 * -im2 + im1 * re2;
      gfloat phase = atan2f (im, re);

      doa[i].angle = asinf (phase * priv->wave_length / (2.0 * G_PI * priv->antenna_base));
      doa[i].distance = i * priv->sound_velocity / (2 * priv->data_rate);
      doa[i].amplitude = sqrtf (re1 * re1 + im1 * im1) * sqrtf (re2 * re2 + im2 * im2);
    }

  /* Сохраняем данные в кэше. */
  if (priv->cache != NULL)
    {
      HyScanForwardLookDataCacheHeader header;

      header.magic = CACHE_HEADER_MAGIC;
      header.n_points = n_points1;
      header.time = time1;
      hyscan_buffer_wrap_data (priv->cache_buffer, HYSCAN_DATA_BLOB, &header, sizeof (header));

      hyscan_cache_set2 (priv->cache, priv->cache_key, priv->detail_key, priv->cache_buffer, priv->doa_buffer);
    }

  (time != NULL) ? *time = time1 : 0;

  return doa;
}
