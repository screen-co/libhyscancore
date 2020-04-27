#include "hyscan-track-proj-quality.h"
#include "hyscan-data-estimator.h"
#include "hyscan-projector.h"
#include "hyscan-quality.h"
#include "hyscan-depthometer.h"

enum
{
  PROP_O,
  PROP_A
};

struct _HyScanTrackProjQualityPrivate
{
  gint                         prop_a;
  guint n_sections;
  HyScanDataEstimator *estimator;
  HyScanProjector *projector;
  guint32 *buff_counts;
  gdouble quality;
  gdouble *buff_values;
  HyScanAmplitude *amplitude;
  HyScanDepthometer *depthometer;
};

static void    hyscan_track_proj_quality_set_property             (GObject               *object,
                                                        guint                  prop_id,
                                                        const GValue          *value,
                                                        GParamSpec            *pspec);
static void    hyscan_track_proj_quality_get_property             (GObject               *object,
                                                        guint                  prop_id,
                                                        GValue                *value,
                                                        GParamSpec            *pspec);
static void    hyscan_track_proj_quality_object_constructed       (GObject               *object);
static void    hyscan_track_proj_quality_object_finalize          (GObject               *object);

/* !!! Change G_TYPE_OBJECT to type of the base class. !!! */
G_DEFINE_TYPE_WITH_PRIVATE (HyScanTrackProjQuality, hyscan_track_proj_quality, G_TYPE_OBJECT)

static void
hyscan_track_proj_quality_class_init (HyScanTrackProjQualityClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_track_proj_quality_set_property;
  object_class->get_property = hyscan_track_proj_quality_get_property;

  object_class->constructed = hyscan_track_proj_quality_object_constructed;
  object_class->finalize = hyscan_track_proj_quality_object_finalize;

  g_object_class_install_property (object_class, PROP_A,
    g_param_spec_int ("param-a", "ParamA", "Parameter A", G_MININT32, G_MAXINT32, 0,
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
    case PROP_A:
      priv->prop_a = g_value_get_int (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_track_proj_quality_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  HyScanTrackProjQuality *track_proj_quality = HYSCAN_TRACK_PROJ_QUALITY (object);
  HyScanTrackProjQualityPrivate *priv = track_proj_quality->priv;

  switch ( prop_id )
    {
    case PROP_A:
      g_value_set_int (value, priv->prop_a);
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

  /* Remove this call then class is derived from GObject.
     This call is strongly needed then class is derived from GtkWidget. */
  G_OBJECT_CLASS (hyscan_track_proj_quality_parent_class)->constructed (object);

  priv->prop_a = 1;
}

static void
hyscan_track_proj_quality_object_finalize (GObject *object)
{
  HyScanTrackProjQuality *track_proj_quality = HYSCAN_TRACK_PROJ_QUALITY (object);
  HyScanTrackProjQualityPrivate *priv = track_proj_quality->priv;

  priv->prop_a = 0;

  G_OBJECT_CLASS (hyscan_track_proj_quality_parent_class)->finalize (object);
}

static gboolean
hyscan_quality_get_values_estimator (HyScanTrackProjQuality *quality,
                                     guint32        index,
                                     const guint32 *counts,
                                     gdouble       *values,
                                     guint          n_values)
{
  HyScanTrackProjQualityPrivate *priv = quality->priv;
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


HyScanTrackProjQuality *
hyscan_track_proj_quality_new (HyScanDB    *db,
                               HyScanCache *cache,
                               const gchar *project,
                               const gchar *track)
{
  return g_object_new (HYSCAN_TYPE_TRACK_PROJ_QUALITY,
                       "db", db,
                       "cache", cache,
                       "project", cache,
                       "track", cache,
                       NULL);
}

gboolean
hyscan_track_proj_quality_cache_get (HyScanTrackProjQuality      *quality_proj,
                                     guint32                      index,
                                     const HyScanTrackCovSection **values,
                                     gsize                        *n_values)
{
  // todo: implement
  return FALSE;
}

static void
hyscan_track_proj_quality_cache_set (HyScanTrackProjQuality      *quality_proj,
                                     guint32                      index,
                                     const HyScanTrackCovSection **values,
                                     gsize                        *n_values)
{
  // todo: implement
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
  gdouble length;
  gdouble depth;

  g_return_val_if_fail (HYSCAN_IS_TRACK_PROJ_QUALITY (quality_proj), FALSE);
  priv = quality_proj->priv;

  hyscan_amplitude_get_size_time (priv->amplitude, index, &n_points, &time);

  depth = hyscan_depthometer_get (priv->depthometer, NULL, time);
  if (depth < 0)
    depth = 0;

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
        return FALSE;

      sections = g_array_new (FALSE, FALSE, sizeof (HyScanTrackProjQuality));
      for (i = 0; i < n_quality; i++)
        {
          if (cur_value == quality[i])
            continue;

          if (!hyscan_projector_count_to_coord (priv->projector, i, &length, depth))
            continue;

          cur_value = quality[i];
          section.quality = (gdouble) quality[i] / max_quality;
          section.start = length;
          g_array_append_val (sections, section);
        }
    }

  /* Считаем качество по выбранным отсчётам. */
  else
    {
      guint i;

      for (i = 0; i < priv->n_sections; i++)
        priv->buff_counts[i] = (i + 1) * n_points / (priv->n_sections + 1);

      if (!hyscan_quality_get_values_estimator (quality_proj, index, priv->buff_counts, priv->buff_values, priv->n_sections))
        return FALSE;

      sections = g_array_new (FALSE, FALSE, sizeof (HyScanTrackProjQuality));
      for (i = 0; i < priv->n_sections; i++)
        {
          if (!hyscan_projector_count_to_coord (priv->projector, priv->buff_counts[i], &length, depth))
            continue;

          section.quality = priv->buff_values[i];
          section.start = length;
          g_array_append_val (sections, section);
        }
    }

  /* Последний отрезок. */
  hyscan_projector_count_to_coord (priv->projector, n_points, &length, depth);
  section.quality = 0.0;
  section.start = length;
  g_array_append_val (sections, section);

  *n_values = sections->len;
  *values = (HyScanTrackCovSection *) g_array_free (sections, FALSE);

  return TRUE;
}


/**
 * hyscan_track_proj_quality_get:
 * @quality_proj
 * @index: индекс акустических данных
 * @values: значения данных
 * @n_values
 * @return
 */
gboolean
hyscan_track_proj_quality_get (HyScanTrackProjQuality      *quality_proj,
                               guint32                      index,
                               const HyScanTrackCovSection **values,
                               gsize                        *n_values)
{
  HyScanTrackProjQualityPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_TRACK_PROJ_QUALITY (quality_proj), FALSE);
  priv = quality_proj->priv;

  if (hyscan_track_proj_quality_cache_get (quality_proj, index, values, n_values))
    return TRUE;

  if (!hyscan_track_proj_quality_get_real (quality_proj, index, values, n_values))
    return FALSE;

  hyscan_track_proj_quality_cache_set (quality_proj, index, values, n_values);

  return TRUE;
}