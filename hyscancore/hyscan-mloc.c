#include "hyscan-mloc.h"
#include "hyscan-geo.h"
#include <hyscan-geo.h>
#include <hyscan-nmea-parser.h>

enum
{
  PROP_O,
  PROP_DB,
  PROP_PROJECT_NAME,
  PROP_TRACK_NAME,
};


struct _HyScanmLocPrivate
{
  HyScanDB              *db;
  gchar                 *project;
  gchar                 *track;

  HyScanGeo             *geo;

  HyScanNavData         *lat;
  HyScanNavData         *lon;
  HyScanNavData         *trk;

  HyScanAntennaPosition position;
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
  guint32 first;

  plat = hyscan_nmea_parser_new (priv->db, priv->project, priv->track,
                                 1, HYSCAN_SOURCE_NMEA_RMC,
                                 HYSCAN_NMEA_FIELD_LAT);
  plon = hyscan_nmea_parser_new (priv->db, priv->project, priv->track,
                                 1, HYSCAN_SOURCE_NMEA_RMC,
                                 HYSCAN_NMEA_FIELD_LON);
  ptrk = hyscan_nmea_parser_new (priv->db, priv->project, priv->track,
                                 1, HYSCAN_SOURCE_NMEA_RMC,
                                 HYSCAN_NMEA_FIELD_TRACK);

  if (plat == NULL || plon == NULL || ptrk == NULL)
    return;

  priv->lat = HYSCAN_NAV_DATA (plat);
  priv->lon = HYSCAN_NAV_DATA (plon);
  priv->trk = HYSCAN_NAV_DATA (ptrk);

  hyscan_nav_data_get_range (priv->lat, &first, NULL);
  hyscan_nav_data_get (priv->lat, first, NULL, &origin.lat);
  hyscan_nav_data_get (priv->lon, first, NULL, &origin.lon);

  priv->geo = hyscan_geo_new (origin, HYSCAN_GEO_ELLIPSOID_WGS84);

  priv->position = hyscan_nav_data_get_position (priv->lat);
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
                 const gchar *project,
                 const gchar *track)
{
  HyScanmLoc * mloc;
  mloc = g_object_new (HYSCAN_TYPE_MLOC,
                       "db", db,
                       "project", project,
                       "track", track,
                       NULL);

  if (mloc->priv->lat == NULL ||
      mloc->priv->lon == NULL ||
      mloc->priv->trk == NULL)
    {
      g_clear_object (&mloc);
    }

  return mloc;
}

void
hyscan_mloc_set_cache (HyScanmLoc  *self,
                       HyScanCache *cache)
{
  HyScanmLocPrivate *priv;

  g_return_if_fail (HYSCAN_IS_MLOC (self));
  priv = self->priv;

  hyscan_nav_data_set_cache (priv->lat, cache);
  hyscan_nav_data_set_cache (priv->lon, cache);
  hyscan_nav_data_set_cache (priv->trk, cache);
}

gboolean
hyscan_mloc_get (HyScanmLoc            *self,
                 gint64                 time,
                 HyScanAntennaPosition *antenna,
                 gdouble                shift,
                 HyScanGeoGeodetic     *position)
{
  HyScanmLocPrivate *priv;
  HyScanGeoCartesian3D topo;
  HyScanGeoGeodetic origin;
  HyScanDBFindStatus fstatus;
  guint32 lindex, rindex, index;

  g_return_val_if_fail (HYSCAN_IS_MLOC (self), FALSE);
  priv = self->priv;

  /* Выясняем координаты приемника в требуемый момент. */
  hyscan_nav_data_get_range (priv->lat, &lindex, &rindex);
  fstatus = hyscan_nav_data_find_data (priv->lat, time, &lindex, &rindex, NULL, NULL);

  if (fstatus != HYSCAN_DB_FIND_OK)
    return FALSE;

  index = lindex;

  hyscan_nav_data_get (priv->lat, index, NULL, &origin.lat);
  hyscan_nav_data_get (priv->lon, index, NULL, &origin.lon);
  hyscan_nav_data_get (priv->trk, index, NULL, &origin.h);

  /* Устанавливаем в эту точку (и с этим углом) начало топоцентрической СК. */
  hyscan_geo_set_origin (priv->geo, origin, HYSCAN_GEO_ELLIPSOID_WGS84);

  /* Сдвигаю куда следует. Тут небольшой костыль, т.к. система координат в гео
   * и в остальном хайскане не совпадает. В хайскане Y+ - это на правый борт,
   * а в гео - левее. */
  topo.x = -priv->position.x + antenna->x;
  topo.y = priv->position.y - antenna->y - shift;
  topo.z = -priv->position.z + antenna->z;

  /* Перевожу обратно в геодезичесие. */
  hyscan_geo_topo2geo (priv->geo, position, topo);

  return TRUE;
}

gboolean
hyscan_mloc_get_fl (HyScanmLoc            *self,
                    gint64                 time,
                    HyScanAntennaPosition *antenna,
                    gdouble                shift_x,
                    gdouble                shift_y,
                    HyScanGeoGeodetic     *position)
{
  HyScanmLocPrivate *priv;
  HyScanGeoCartesian3D topo;
  HyScanGeoGeodetic origin;
  HyScanDBFindStatus fstatus;
  guint32 lindex, rindex, index;

  g_return_val_if_fail (HYSCAN_IS_MLOC (self), FALSE);
  priv = self->priv;

  /* Выясняем координаты приемника в требуемый момент. */
  hyscan_nav_data_get_range (priv->lat, &lindex, &rindex);
  fstatus = hyscan_nav_data_find_data (priv->lat, time, &lindex, &rindex, NULL, NULL);

  if (fstatus != HYSCAN_DB_FIND_OK)
    return FALSE;

  index = lindex;

  hyscan_nav_data_get (priv->lat, index, NULL, &origin.lat);
  hyscan_nav_data_get (priv->lon, index, NULL, &origin.lon);
  hyscan_nav_data_get (priv->trk, index, NULL, &origin.h);

  /* Устанавливаем в эту точку (и с этим углом) начало топоцентрической СК. */
  hyscan_geo_set_origin (priv->geo, origin, HYSCAN_GEO_ELLIPSOID_WGS84);

  /* Сдвигаю куда следует. Тут небольшой костыль, т.к. система координат в гео
   * и в остальном хайскане не совпадает. В хайскане Y+ - это на правый борт,
   * а в гео - левее. */
  topo.x = -priv->position.x + antenna->x + shift_x;
  topo.y = priv->position.y - antenna->y - shift_y;
  topo.z = -priv->position.z + antenna->z;

  /* Перевожу обратно в геодезичесие. */
  hyscan_geo_topo2geo (priv->geo, position, topo);

  return TRUE;
}
