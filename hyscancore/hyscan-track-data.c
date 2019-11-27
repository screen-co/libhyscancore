#include "hyscan-track-data.h"
#include <math.h>

#define EARTH_RADIUS 6378137.0               /* Радиус Земли. */
#define DEG2RAD(x) (x / 180.0 * G_PI)
#define RAD2DEG(x) (x * 180.0 / G_PI)
#define VALID_LAT(x) (fabs (x) <= 90.0)
#define VALID_LON(x) (fabs (x) <= 180.0)
#define FIT_ANGLE(x) ((x) < 0 ? (x) + 360 : ((x) >= 360 ? (x) - 360 : (x)))

enum
{
  PROP_O,
  PROP_LAT,
  PROP_LON,
};

struct _HyScanTrackDataPrivate
{
  HyScanNavData                *lat;        /* Широта. */
  HyScanNavData                *lon;        /* Долгота. */
  guint                         before;     /* Количество предыдущих индексов. */
  guint                         after;      /* Количество следующих индексов. */
};

static void    hyscan_track_data_interface_init        (HyScanNavDataInterface *iface);
static void    hyscan_track_data_set_property          (GObject                *object,
                                                        guint                   prop_id,
                                                        const GValue           *value,
                                                        GParamSpec             *pspec);
static void    hyscan_track_data_object_constructed    (GObject                *object);
static void    hyscan_track_data_object_finalize       (GObject                *object);

G_DEFINE_TYPE_WITH_CODE (HyScanTrackData, hyscan_track_data, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (HyScanTrackData)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_NAV_DATA, hyscan_track_data_interface_init))

static void
hyscan_track_data_class_init (HyScanTrackDataClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_track_data_set_property;

  object_class->constructed = hyscan_track_data_object_constructed;
  object_class->finalize = hyscan_track_data_object_finalize;

  g_object_class_install_property (object_class, PROP_LAT,
    g_param_spec_object ("lat", "Lat NavData", "HyScanNavData with latitude", HYSCAN_TYPE_NAV_DATA,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_LON,
    g_param_spec_object ("lon", "Lon NavData", "HyScanNavData with longitude", HYSCAN_TYPE_NAV_DATA,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_track_data_init (HyScanTrackData *track_data)
{
  track_data->priv = hyscan_track_data_get_instance_private (track_data);
}

static void
hyscan_track_data_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  HyScanTrackData *track_data = HYSCAN_TRACK_DATA (object);
  HyScanTrackDataPrivate *priv = track_data->priv;

  switch (prop_id)
    {
    case PROP_LAT:
      priv->lat = g_value_dup_object (value);
      break;

    case PROP_LON:
      priv->lon = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_track_data_object_constructed (GObject *object)
{
  HyScanTrackData *track_data = HYSCAN_TRACK_DATA (object);
  HyScanTrackDataPrivate *priv = track_data->priv;

  priv->after = 10;
  priv->before = 10;

  G_OBJECT_CLASS (hyscan_track_data_parent_class)->constructed (object);
}

static void
hyscan_track_data_object_finalize (GObject *object)
{
  HyScanTrackData *track_data = HYSCAN_TRACK_DATA (object);
  HyScanTrackDataPrivate *priv = track_data->priv;

  g_clear_object (&priv->lat);
  g_clear_object (&priv->lon);

  G_OBJECT_CLASS (hyscan_track_data_parent_class)->finalize (object);
}


static gboolean
hyscan_track_data_get (HyScanNavData     *ndata,
                       HyScanCancellable *cancellable,
                       guint32            index,
                       gint64            *time,
                       gdouble           *value)
{
  HyScanTrackDataPrivate *priv;
  guint32 i, j;
  gdouble track;
  gdouble lat0 = -1000, lat1 = -1000, lon0 = -1000, lon1 = -1000;

  g_return_val_if_fail (HYSCAN_IS_TRACK_DATA (ndata), FALSE);
  priv = HYSCAN_TRACK_DATA (ndata)->priv;

  /* Получаем время. */
  if (time != NULL)
    hyscan_nav_data_get (priv->lat, NULL, index, time, NULL);

  if (index < priv->before)
    return FALSE;

  j = 0;
  track = 0.0;
  for (i = index - priv->before; i <= index + priv->after; ++i)
    {
      if (!hyscan_nav_data_get (priv->lat, NULL, i, NULL, &lat1) ||
          !hyscan_nav_data_get (priv->lon, NULL, i, NULL, &lon1))
        continue;

      if (!VALID_LAT (lat1) || !VALID_LON (lon1))
        continue;

      if (VALID_LAT (lat0) && VALID_LON (lon0))
        {
          gdouble track_avg, track_i;

          track_avg = j > 0 ? (track / j) : 0.0;
          track_i = hyscan_track_data_calc_track (lat0, lon0, lat1, lon1);
          if (track_i - track_avg > 180)
            track_i -= 360;
          else if (track_i - track_avg < -180)
            track_i += 360;

          j++;
          track += track_i;
        }

      lat0 = lat1;
      lon0 = lon1;
    }

  if (j == 0)
    return FALSE;

  if (value != NULL)
    *value = FIT_ANGLE (track / j);

  return TRUE;
}

static HyScanDBFindStatus
hyscan_track_data_find_data (HyScanNavData *ndata,
                             gint64         time,
                             guint32       *lindex,
                             guint32       *rindex,
                             gint64        *ltime,
                             gint64        *rtime)
{
  HyScanTrackDataPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_TRACK_DATA (ndata), HYSCAN_DB_FIND_FAIL);

  priv = HYSCAN_TRACK_DATA (ndata)->priv;

  return hyscan_nav_data_find_data (priv->lat, time, lindex, rindex, ltime, rtime);
}

static gboolean
hyscan_track_data_get_range (HyScanNavData *ndata,
                             guint32       *first,
                             guint32       *last)
{
  HyScanTrackDataPrivate *priv;
  guint32 lat_first, lat_last;

  g_return_val_if_fail (HYSCAN_IS_TRACK_DATA (ndata), FALSE);

  priv = HYSCAN_TRACK_DATA (ndata)->priv;

  if (!hyscan_nav_data_get_range (priv->lat, &lat_first, &lat_last))
    return FALSE;

  /* Урезаем диапазон на before и after индексов. */
  lat_first += priv->before;
  lat_last -= priv->after;
  if (lat_first > lat_last)
    return FALSE;

  (first != NULL) ? *first = lat_first : 0;
  (last != NULL) ? *last = lat_last : 0;

  return TRUE;
}

static HyScanAntennaOffset
hyscan_track_data_get_offset (HyScanNavData *ndata)
{
  HyScanTrackDataPrivate *priv;
  HyScanAntennaOffset zero = {0};

  g_return_val_if_fail (HYSCAN_IS_TRACK_DATA (ndata), zero);

  priv = HYSCAN_TRACK_DATA (ndata)->priv;

  return hyscan_nav_data_get_offset (priv->lat);
}

static gboolean
hyscan_track_data_is_writable (HyScanNavData *ndata)
{
  HyScanTrackDataPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_TRACK_DATA (ndata), 0);

  priv = HYSCAN_TRACK_DATA (ndata)->priv;

  return hyscan_nav_data_is_writable (priv->lat) || hyscan_nav_data_is_writable (priv->lon);
}

static const gchar *
hyscan_track_data_get_token (HyScanNavData *ndata)
{
  HyScanTrackDataPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_TRACK_DATA (ndata), 0);

  priv = HYSCAN_TRACK_DATA (ndata)->priv;

  return hyscan_nav_data_get_token (priv->lat);
}

static guint32
hyscan_track_data_get_mod_count (HyScanNavData *ndata)
{
  HyScanTrackDataPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_TRACK_DATA (ndata), 0);

  priv = HYSCAN_TRACK_DATA (ndata)->priv;

  return hyscan_nav_data_get_mod_count (priv->lat);
}

static void
hyscan_track_data_interface_init (HyScanNavDataInterface *iface)
{
  iface->find_data = hyscan_track_data_find_data;
  iface->get = hyscan_track_data_get;
  iface->get_mod_count = hyscan_track_data_get_mod_count;
  iface->get_offset = hyscan_track_data_get_offset;
  iface->get_range = hyscan_track_data_get_range;
  iface->get_token = hyscan_track_data_get_token;
  iface->is_writable = hyscan_track_data_is_writable;
}

HyScanNavData *
hyscan_track_data_new (HyScanNavData *lat,
                       HyScanNavData *lon)
{
  return g_object_new (HYSCAN_TYPE_TRACK_DATA,
                       "lat", lat,
                       "lon", lon,
                       NULL);
}

gdouble
hyscan_track_data_calc_track (gdouble      lat1,
                              gdouble      lon1,
                              gdouble      lat2,
                              gdouble      lon2)
{
  gdouble dlon;
  gdouble angle;

  lat1 = DEG2RAD (lat1);
  lon1 = DEG2RAD (lon1);
  lat2 = DEG2RAD (lat2);
  lon2 = DEG2RAD (lon2);
  dlon = lon2 - lon1;

  angle = atan2 (sin (dlon) * cos (lat2), cos (lat1) * sin (lat2) - sin (lat1) * cos (lat2) * cos (dlon));
  if (angle < 0)
    angle += G_PI * 2.0;

  return RAD2DEG (angle);
}

gdouble
hyscan_track_data_calc_dist (gdouble lat1,
                             gdouble lon1,
                             gdouble lat2,
                             gdouble lon2)
{
  gdouble lon1r;
  gdouble lat1r;

  gdouble lon2r;
  gdouble lat2r;

  gdouble u;
  gdouble v;

  lat1r = DEG2RAD (lat1);
  lon1r = DEG2RAD (lon1);
  lat2r = DEG2RAD (lat2);
  lon2r = DEG2RAD (lon2);

  u = sin ((lat2r - lat1r) / 2);
  v = sin ((lon2r - lon1r) / 2);

  return 2.0 * EARTH_RADIUS * asin (sqrt (u * u + cos (lat1r) * cos (lat2r) * v * v));
}
