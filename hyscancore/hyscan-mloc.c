#include "hyscan-mloc.h"
#include <hyscan-geo.h>
#include <hyscan-nmea-parser.h>

enum
{
  PROP_O,
  PROP_DB,
  PROP_CACHE,
  PROP_PROJECT_NAME,
  PROP_TRACK_NAME,
};


struct _HyScanmLocPrivate
{
  HyScanDB              *db;
  HyScanCache           *cache;
  gchar                 *project;
  gchar                 *track;

  HyScanGeo             *geo;

  HyScanNavData         *lat;
  HyScanNavData         *lon;
  HyScanNavData         *trk;

  HyScanAntennaOffset    position;
};

static void    hyscan_mloc_set_property             (GObject           *object,
                                                     guint              prop_id,
                                                     const GValue      *value,
                                                     GParamSpec        *pspec);
static void    hyscan_mloc_object_constructed       (GObject           *object);
static void    hyscan_mloc_object_finalize          (GObject           *object);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanmLoc, hyscan_mloc, G_TYPE_OBJECT);

static void
hyscan_mloc_class_init (HyScanmLocClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_mloc_set_property;

  object_class->constructed = hyscan_mloc_object_constructed;
  object_class->finalize = hyscan_mloc_object_finalize;

  g_object_class_install_property (object_class, PROP_DB,
    g_param_spec_object ("db", "DB", "HyScanDB interface", HYSCAN_TYPE_DB,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_CACHE,
    g_param_spec_object ("cache", "Cache", "HyScanCache interface", HYSCAN_TYPE_CACHE,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_PROJECT_NAME,
    g_param_spec_string ("project", "ProjectName", "Project name", NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_TRACK_NAME,
    g_param_spec_string ("track", "TrackName", "Track name", NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

}

static void
hyscan_mloc_init (HyScanmLoc *self)
{
  self->priv = hyscan_mloc_get_instance_private (self);
}

static void
hyscan_mloc_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  HyScanmLoc *self = HYSCAN_MLOC (object);
  HyScanmLocPrivate *priv = self->priv;

  if (prop_id == PROP_DB)
    priv->db = g_value_dup_object (value);
  else if (prop_id == PROP_CACHE)
    priv->cache = g_value_dup_object (value);
  else if (prop_id == PROP_PROJECT_NAME)
    priv->project = g_value_dup_string (value);
  else if (prop_id == PROP_TRACK_NAME)
    priv->track = g_value_dup_string (value);
  else
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
}

static void
hyscan_mloc_object_constructed (GObject *object)
{
  HyScanmLoc *self = HYSCAN_MLOC (object);
  HyScanmLocPrivate *priv = self->priv;
  HyScanNMEAParser *plat, *plon, *ptrk;
  HyScanGeoGeodetic origin = {0, 0, 0};

  plat = hyscan_nmea_parser_new (priv->db, priv->cache,
                                 priv->project, priv->track,
                                 1, HYSCAN_NMEA_DATA_RMC,
                                 HYSCAN_NMEA_FIELD_LAT);
  plon = hyscan_nmea_parser_new (priv->db, priv->cache,
                                 priv->project, priv->track,
                                 1, HYSCAN_NMEA_DATA_RMC,
                                 HYSCAN_NMEA_FIELD_LON);
  ptrk = hyscan_nmea_parser_new (priv->db, priv->cache,
                                 priv->project, priv->track,
                                 1, HYSCAN_NMEA_DATA_RMC,
                                 HYSCAN_NMEA_FIELD_TRACK);

  if (plat == NULL || plon == NULL || ptrk == NULL)
    return;

  priv->lat = HYSCAN_NAV_DATA (plat);
  priv->lon = HYSCAN_NAV_DATA (plon);
  priv->trk = HYSCAN_NAV_DATA (ptrk);

  priv->geo = hyscan_geo_new (origin, HYSCAN_GEO_ELLIPSOID_WGS84);

  priv->position = hyscan_nav_data_get_offset (priv->lat);
}

static void
hyscan_mloc_object_finalize (GObject *object)
{
  HyScanmLoc *self = HYSCAN_MLOC (object);
  HyScanmLocPrivate *priv = self->priv;

  g_clear_object (&priv->db);
  g_free (priv->project);
  g_free (priv->track);

  g_clear_object (&priv->geo);

  g_clear_object (&priv->lat);
  g_clear_object (&priv->lon);
  g_clear_object (&priv->trk);

  G_OBJECT_CLASS (hyscan_mloc_parent_class)->finalize (object);
}

HyScanmLoc *
hyscan_mloc_new (HyScanDB    *db,
                 HyScanCache *cache,
                 const gchar *project,
                 const gchar *track)
{
  HyScanmLoc * mloc;

  mloc = g_object_new (HYSCAN_TYPE_MLOC,
                       "db", db,
                       "cache", cache,
                       "project", project,
                       "track", track,
                       NULL);

  if (mloc->priv->lat == NULL || mloc->priv->lon == NULL || mloc->priv->trk == NULL)
    g_clear_object (&mloc);

  return mloc;
}

gboolean
hyscan_mloc_get (HyScanmLoc            *self,
                 HyScanCancellable     *cancellable,
                 gint64                 time,
                 HyScanAntennaOffset    *antenna,
                 gdouble                shiftx,
                 gdouble                shifty,
                 gdouble                shiftz,
                 HyScanGeoGeodetic     *position)
{
  HyScanmLocPrivate *priv;
  HyScanGeoCartesian3D topo;
  HyScanGeoGeodetic origin = {0, 0, 0};
  HyScanDBFindStatus fstatus;
  guint32 lindex, rindex, index;
  gboolean status;

  g_return_val_if_fail (HYSCAN_IS_MLOC (self), FALSE);
  priv = self->priv;

  /* Выясняем координаты приемника в требуемый момент. */
  hyscan_nav_data_get_range (priv->lat, &lindex, &rindex);
  fstatus = hyscan_nav_data_find_data (priv->lat, time, &lindex, &rindex, NULL, NULL);

  if (fstatus != HYSCAN_DB_FIND_OK)
    return FALSE;

  index = lindex;

  hyscan_cancellable_push (cancellable);

  hyscan_cancellable_set (cancellable, 0, 1/3);
  status  = hyscan_nav_data_get (priv->lat, cancellable, index, NULL, &origin.lat);
  hyscan_cancellable_set (cancellable, 1/3, 2/3);
  status &= hyscan_nav_data_get (priv->lon, cancellable, index, NULL, &origin.lon);
  hyscan_cancellable_set (cancellable, 2/3, 1);
  status &= hyscan_nav_data_get (priv->trk, cancellable, index, NULL, &origin.h);

  if (!status)
    goto exit;

  /* Устанавливаем в эту точку (и с этим углом) начало топоцентрической СК. */
  hyscan_geo_set_origin (priv->geo, origin, HYSCAN_GEO_ELLIPSOID_WGS84);

  /* Сдвигаю куда следует. Тут небольшой костыль, т.к. система координат в гео
   * и в остальном хайскане не совпадает. В хайскане Starboard+ - это на правый борт,
   * а в гео - левее. */
  topo.x = -priv->position.forward + antenna->forward + shiftx;
  topo.y = priv->position.starboard - antenna->starboard - shifty;
  topo.z = -priv->position.vertical + antenna->vertical + shiftz;

  /* Перевожу обратно в геодезичесие. */
  hyscan_geo_topo2geo (priv->geo, position, topo);

exit:
  hyscan_cancellable_pop (cancellable);
  return status;
}
