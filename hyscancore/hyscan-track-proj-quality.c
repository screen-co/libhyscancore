#include "hyscan-track-proj-quality.h"
#include "hyscan-data-estimator.h"
#include "hyscan-projector.h"
#include "hyscan-depthometer.h"
#include "hyscan-map-track-param.h"

#define CACHE_DATA_MAGIC       0x64747071              /* Идентификатор заголовка записи кэша. */

enum
{
  PROP_O,
  PROP_SOURCE,
  PROP_DB,
  PROP_CACHE,
  PROP_PROJECT,
  PROP_TRACK,
};

/* Структруа заголовка записи кэша. */
typedef struct
{
  guint32                      magic;                  /* Идентификатор заголовка. */
  guint32                      n_values;               /* Число точек данных. */
} HyScanTrackProjQualityCacheHeader;

struct _HyScanTrackProjQualityPrivate
{
  gchar                       *project;            /* Имя проекта. */
  gchar                       *track;              /* Имя галса. */
  HyScanSourceType             source;             /* Источник амплитудных данных. */

  HyScanDB                    *db;                 /* База данных. */
  HyScanCache                 *cache;              /* Кэш. */
  HyScanAmplitude             *amplitude;          /* Амплитудные данные. */
  HyScanDepthometer           *depthometer;        /* Данные глубины. */
  HyScanDataEstimator         *estimator;          /* Объект оценки качества. */
  HyScanProjector             *projector;          /* Проекция наклонной дальности на горизонтальную. */

  gdouble                      quality;            /* Минимальное значение качества. */
  GArray                      *squashed_array;     /* Массив с границам отрезков хорошего качества. */

  guint                        n_sections;         /* Число отрезков на луче. */
  guint32                     *buff_counts;        /* Массив отсчётов на границе отрезков. */
  gdouble                     *buff_values;        /* Массив значений качества на отрезках. */

  gchar                       *cache_key;          /* Ключ кэширования. */
  gint                         cache_key_len;      /* Длина буфера cache_key. */
  gchar                       *cache_key_prefix;   /* Префикс ключа кэширования. */
  HyScanBuffer                *header_buffer;      /* Буфрер для заголовка записи кэша. */
  HyScanBuffer                *data_buffer;        /* Буфер для данных записи кэша. */
};

static void    hyscan_track_proj_quality_set_property             (GObject               *object,
                                                                   guint                  prop_id,
                                                                   const GValue          *value,
                                                                   GParamSpec            *pspec);
static void    hyscan_track_proj_quality_object_constructed       (GObject               *object);
static void    hyscan_track_proj_quality_object_finalize          (GObject               *object);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanTrackProjQuality, hyscan_track_proj_quality, G_TYPE_OBJECT)

static void
hyscan_track_proj_quality_class_init (HyScanTrackProjQualityClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_track_proj_quality_set_property;
  object_class->constructed = hyscan_track_proj_quality_object_constructed;
  object_class->finalize = hyscan_track_proj_quality_object_finalize;

  g_object_class_install_property (object_class, PROP_SOURCE,
    g_param_spec_int ("source", "Source", "Source type", 0, G_MAXINT, 0,
                      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_DB,
    g_param_spec_object ("db", "HyScanDB", "HyScanDB", HYSCAN_TYPE_DB,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_CACHE,
    g_param_spec_object ("cache", "HyScanCache", "HyScanCache", HYSCAN_TYPE_CACHE,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_PROJECT,
    g_param_spec_string ("project", "Project", "Project", NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_TRACK,
    g_param_spec_string ("track", "Track", "Track", NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_track_proj_quality_init (HyScanTrackProjQuality *track_proj_quality)
{
  track_proj_quality->priv = hyscan_track_proj_quality_get_instance_private (track_proj_quality);
}

static void
hyscan_track_proj_quality_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  HyScanTrackProjQuality *track_proj_quality = HYSCAN_TRACK_PROJ_QUALITY (object);
  HyScanTrackProjQualityPrivate *priv = track_proj_quality->priv;

  switch (prop_id)
    {
    case PROP_SOURCE:
      priv->source = g_value_get_int (value);
      break;

    case PROP_DB:
      priv->db = g_value_dup_object (value);
      break;

    case PROP_CACHE:
      priv->cache = g_value_dup_object (value);
      break;

    case PROP_PROJECT:
      priv->project = g_value_dup_string (value);
      break;

    case PROP_TRACK:
      priv->track = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_track_proj_quality_object_constructed (GObject *object)
{
  HyScanTrackProjQuality *track_proj_quality = HYSCAN_TRACK_PROJ_QUALITY (object);
  HyScanTrackProjQualityPrivate *priv = track_proj_quality->priv;
  HyScanAcousticData *signal, *noise;
  gint64 channel_rmc = -1, channel_dpt = -1;

  G_OBJECT_CLASS (hyscan_track_proj_quality_parent_class)->constructed (object);

  signal = hyscan_acoustic_data_new (priv->db, priv->cache, priv->project, priv->track, priv->source, 1, FALSE);
  noise = hyscan_acoustic_data_new (priv->db, priv->cache, priv->project, priv->track, priv->source, 1, TRUE);
  priv->estimator = hyscan_data_estimator_new (signal, noise, NULL);
  priv->amplitude = g_object_ref (HYSCAN_AMPLITUDE (signal));
  priv->projector = hyscan_projector_new (priv->amplitude);

  {
    HyScanMapTrackParam *track_param;
    HyScanParamList *list;

    track_param = hyscan_map_track_param_new (NULL, priv->db, priv->project, priv->track);

    list = hyscan_param_list_new ();
    hyscan_param_list_add (list, "/quality");
    hyscan_param_list_add (list, "/channel-rmc");
    hyscan_param_list_add (list, "/channel-dpt");

    if (hyscan_param_get (HYSCAN_PARAM (track_param), list) != 0)
      {
        priv->quality = hyscan_param_list_get_double (list, "/quality");
        channel_rmc = hyscan_param_list_get_enum (list, "/channel-rmc");
        channel_dpt = hyscan_param_list_get_enum (list, "/channel-dpt");
      }
    else
      {
        g_warning ("HyScanTrackProjQuality: failed to read track params");
      }

    priv->depthometer = hyscan_map_track_param_get_depthometer (track_param, priv->cache);

    g_object_unref (track_param);
  }

  priv->header_buffer = hyscan_buffer_new ();
  priv->data_buffer = hyscan_buffer_new ();

  priv->cache_key_prefix = g_strdup_printf ("%s.%s.%d.%ld.%ld",
                                            priv->project, priv->track, priv->source, channel_rmc, channel_dpt);
  priv->cache_key = g_strdup_printf ("%s.%u", priv->cache_key_prefix, G_MAXUINT32);
  priv->cache_key_len = strlen (priv->cache_key);

  /* Разбиваем каждую строку на 100 секций и усредняем качество по каждой. */
  priv->n_sections = 100;
  priv->buff_counts = g_new (guint32, priv->n_sections);
  priv->buff_values = g_new (gdouble, priv->n_sections);

  priv->squashed_array = g_array_new (FALSE, FALSE, sizeof (gdouble));

  g_clear_object (&signal);
  g_clear_object (&noise);
}

static void
hyscan_track_proj_quality_object_finalize (GObject *object)
{
  HyScanTrackProjQuality *track_proj_quality = HYSCAN_TRACK_PROJ_QUALITY (object);
  HyScanTrackProjQualityPrivate *priv = track_proj_quality->priv;

  g_free (priv->project);
  g_free (priv->track);

  g_object_unref (priv->db);
  g_object_unref (priv->cache);
  g_object_unref (priv->amplitude);
  g_object_unref (priv->depthometer);
  g_object_unref (priv->estimator);
  g_object_unref (priv->projector);

  g_array_free (priv->squashed_array, TRUE);

  g_object_unref (priv->header_buffer);
  g_object_unref (priv->data_buffer);

  g_free (priv->cache_key);
  g_free (priv->cache_key_prefix);

  G_OBJECT_CLASS (hyscan_track_proj_quality_parent_class)->finalize (object);
}

static gboolean
hyscan_quality_get_values_estimator (HyScanTrackProjQuality *quality,
                                     guint32        index)
{
  HyScanTrackProjQualityPrivate *priv = quality->priv;
  const guint32 *quality_values;
  guint32 n_points, i, j;
  guint32 max_quality;

  quality_values = hyscan_data_estimator_get_acust_quality (priv->estimator, index, &n_points);
  if (quality_values == NULL)
    return FALSE;

  max_quality = hyscan_data_estimator_get_max_quality (priv->estimator);
  for (i = 0; i < priv->n_sections; i++)
    {
      guint32 section_quality;
      guint32 first, last;

      first = i == 0 ? 0 : (priv->buff_counts[i - 1] + 1);
      last = priv->buff_counts[i];

      section_quality = 0;
      for (j = first; j <= last; j++)
        section_quality += quality_values[j];

      priv->buff_values[i] = (gdouble) section_quality / max_quality / (last - first);
    }

  return TRUE;
}

gboolean
hyscan_track_proj_quality_cache_get (HyScanTrackProjQuality      *quality_proj,
                                     guint32                      index,
                                     const HyScanTrackCovSection **values,
                                     gsize                        *n_values)
{
  HyScanTrackProjQualityPrivate *priv = quality_proj->priv;
  HyScanTrackProjQualityCacheHeader header;
  gboolean status;
  gpointer data;
  guint32 data_size, data_n_values;

  if (priv->cache == NULL)
    return FALSE;

  /* Чтение данных из кэша. */
  hyscan_buffer_wrap (priv->header_buffer, HYSCAN_DATA_BLOB, &header, sizeof (header));
  g_snprintf (priv->cache_key, priv->cache_key_len, "%s.%u", priv->cache_key_prefix, index);
  status = hyscan_cache_get2 (priv->cache, priv->cache_key, NULL,
                              sizeof (header), priv->header_buffer, priv->data_buffer);

  if (!status)
    return FALSE;

  /* Верификация данных. */
  data = hyscan_buffer_get (priv->data_buffer, NULL, &data_size);
  data_n_values = data_size/sizeof (HyScanTrackCovSection);
  if ((header.magic != CACHE_DATA_MAGIC) || (header.n_values != data_n_values))
    return FALSE;

  *values = data;
  *n_values = data_n_values;

  return TRUE;
}

static void
hyscan_track_proj_quality_cache_set (HyScanTrackProjQuality      *quality_proj,
                                     guint32                      index,
                                     const HyScanTrackCovSection *values,
                                     gsize                        n_values)
{
  HyScanTrackProjQualityPrivate *priv = quality_proj->priv;
  HyScanTrackProjQualityCacheHeader header;

  if (priv->cache == NULL)
    return;

  header.magic = CACHE_DATA_MAGIC;
  header.n_values = n_values;

  hyscan_buffer_wrap (priv->header_buffer, HYSCAN_DATA_BLOB, &header, sizeof (header));
  g_snprintf (priv->cache_key, priv->cache_key_len, "%s.%u", priv->cache_key_prefix, index);

  hyscan_buffer_set (priv->data_buffer, HYSCAN_DATA_BLOB, (gpointer) values, sizeof (*values) * n_values);

  hyscan_cache_set2 (priv->cache, priv->cache_key, NULL,
                     priv->header_buffer, priv->data_buffer);
}

static gboolean
hyscan_track_proj_quality_get_real (HyScanTrackProjQuality      *quality_proj,
                                    guint32                      index,
                                    const HyScanTrackCovSection **values,
                                    gsize                        *n_values)
{
  HyScanTrackProjQualityPrivate *priv;
  HyScanTrackCovSection section;
  GArray *sections;
  guint32 n_points;
  gint64 time;
  gdouble length, max_length;
  gdouble depth;
  gboolean status = FALSE;

  g_return_val_if_fail (HYSCAN_IS_TRACK_PROJ_QUALITY (quality_proj), FALSE);
  priv = quality_proj->priv;

  hyscan_amplitude_get_size_time (priv->amplitude, index, &n_points, &time);

  depth = hyscan_depthometer_get (priv->depthometer, NULL, time);
  if (depth < 0)
    depth = 0;

  /* Длина луча. */
  hyscan_projector_count_to_coord (priv->projector, n_points, &max_length, depth);

  sections = g_array_new (FALSE, FALSE, sizeof (HyScanTrackCovSection));

  /* Считаем качество по всем отсчётам. */
  if (priv->n_sections == 0)
    {
      guint i;
      guint max_quality = 10;
      guint cur_value = max_quality + 1;
      guint32 n_quality;
      const guint32 *quality;

      hyscan_data_estimator_set_max_quality (priv->estimator, max_quality);
      quality = hyscan_data_estimator_get_acust_quality (priv->estimator, index, &n_quality);
      if (quality == NULL)
        goto exit;

      for (i = 0; i < n_quality; i++)
        {
          if (cur_value == quality[i])
            continue;

          if (!hyscan_projector_count_to_coord (priv->projector, i, &length, depth))
            continue;

          cur_value = quality[i];
          section.quality = (gdouble) quality[i] / max_quality;
          section.start = length / max_length;
          g_array_append_val (sections, section);
        }
    }

    /* Считаем качество по выбранным отсчётам. */
  else
    {
      guint i;

      for (i = 0; i < priv->n_sections; i++)
        priv->buff_counts[i] = (i + 1) * n_points / (priv->n_sections + 1);

      if (!hyscan_quality_get_values_estimator (quality_proj, index))
        goto exit;

      for (i = 0; i < priv->n_sections; i++)
        {
          if (!hyscan_projector_count_to_coord (priv->projector, priv->buff_counts[i], &length, depth))
            continue;

          section.quality = priv->buff_values[i];
          section.start = length / max_length;
          g_array_append_val (sections, section);
        }
    }

  /* Последний отрезок. */
  section.quality = 0.0;
  section.start = 1.0;
  g_array_append_val (sections, section);
  status = TRUE;

  exit:
  *n_values = sections->len;
  *values = (HyScanTrackCovSection *) g_array_free (sections, FALSE);

  return status;
}

/**
 * hyscan_track_proj_quality_new:
 * @db: база данных #HyScanDB
 * @cache: кэш #HyScanCache
 * @project: имя проекта
 * @track: имя галса
 * @source: источник амплитудных данных
 *
 * Returns: (transfer full): указатель на новый объект #HyScanTrackProjQuality
 */
HyScanTrackProjQuality *
hyscan_track_proj_quality_new (HyScanDB         *db,
                               HyScanCache      *cache,
                               const gchar      *project,
                               const gchar      *track,
                               HyScanSourceType source)
{
  return g_object_new (HYSCAN_TYPE_TRACK_PROJ_QUALITY,
                       "db", db,
                       "cache", cache,
                       "project", project,
                       "track", track,
                       "source", source,
                       NULL);
}


/**
 * hyscan_track_proj_quality_get:
 * @quality_proj: указатель на #HyScanTrackProjQuality
 * @index: индекс акустических данных
 * @values: (out) (transfer none) (array length=n_values): значения данных качества
 * @n_values:(out): число точек
 *
 * Функция получает данные качества акустических данных в строке с указанным индексом.
 *
 * Returns: %TRUE, если данные успешно получены; иначе %FALSE
 */
gboolean
hyscan_track_proj_quality_get (HyScanTrackProjQuality      *quality_proj,
                               guint32                      index,
                               const HyScanTrackCovSection **values,
                               gsize                        *n_values)
{
  g_return_val_if_fail (HYSCAN_IS_TRACK_PROJ_QUALITY (quality_proj), FALSE);

  if (hyscan_track_proj_quality_cache_get (quality_proj, index, values, n_values))
    return TRUE;

  if (!hyscan_track_proj_quality_get_real (quality_proj, index, values, n_values))
    return FALSE;

  hyscan_track_proj_quality_cache_set (quality_proj, index, *values, *n_values);

  return TRUE;
}

/**
 * hyscan_track_proj_quality_squash:
 * @quality_proj: указатель на #HyScanTrackProjQuality
 * @index: индекс акустических данных
 * @n_values: (out): число точек в массиве
 *
 * Функция возвращает координаты отрезков, на которых качество акустических данных считается "хорошим", т.е. превышает
 * минимальное значение, указанного в параметрах галса. Координаты отрезков указаны в долях горизонтальной дальности,
 * аналогично структуре #HyScanTrackCovSection.
 *
 * Для N отрезков возвращается массив размера 2 * N; координаты начала и конца k-го отрезка соответствует
 * элементам массива с индексами 2k и (2k + 1).
 *
 * Returns: (transfer none) (array length=n_values): массив координат отрезков с "хорошими" акустическими данными.
 */
const gdouble *
hyscan_track_proj_quality_squash (HyScanTrackProjQuality *quality_proj,
                                  guint32                 index,
                                  gsize                  *n_points)
{
  HyScanTrackProjQualityPrivate *priv;
  const HyScanTrackCovSection *values;
  gsize i, n_values;

  g_return_val_if_fail (HYSCAN_IS_TRACK_PROJ_QUALITY (quality_proj), NULL);
  priv = quality_proj->priv;

  g_array_set_size (priv->squashed_array, 0);

  if (!hyscan_track_proj_quality_get (quality_proj, index, &values, &n_values))
    goto exit;

  for (i = 0; i < n_values; i++)
    {
      gboolean good;
      gboolean array_good;

      /* Признак того, что текущий отрезок хорошего качества. */
      good = (values[i].quality > priv->quality);

      /* Признак того, что в массиве сейчас идёт отрезок хорошего качества.
       * Подробнее: первая и прочие нечётные точки соответствуют началу хороших отрезков;
       * вторая и прочие чётные - плохим. */
      array_good = (priv->squashed_array->len % 2 == 1);

      if (good == array_good)
        continue;

      g_array_append_val (priv->squashed_array, values[i].start);
    }

  /* При необходимости дополняем массив до чётного количества точек. */
  if (priv->squashed_array->len % 2 == 1)
    {
      gdouble end = 1.0;
      g_array_append_val (priv->squashed_array, end);
    }

exit:
  *n_points = priv->squashed_array->len;

  return (const gdouble *) priv->squashed_array->data;
}
