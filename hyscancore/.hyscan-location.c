#include "hyscan-location.h"

G_DEFINE_INTERFACE (HyScanLocation, hyscan_location, G_TYPE_OBJECT);

static void
hyscan_location_default_init (HyScanLocationInterface *iface)
{
}

void
hyscan_location_set_soundspeed (HyScanLocation *location,
                                GArray         *soundspeedtable)
{
  HyScanLocationInterface *iface;

  g_return_if_fail (HYSCAN_IS_LOCATION (location));

  iface = HYSCAN_LOCATION_GET_IFACE (location);
  if (iface->set_soundspeed != NULL)
    (*iface->set_soundspeed) (location, soundspeedtable);
}


HyScanDepthData
hyscan_location_get_depth (HyScanLocation *location,
                           gint32          index)
{
  HyScanLocationInterface *iface;
  HyScanDepthData default_output = {0};
  g_return_val_if_fail (HYSCAN_IS_LOCATION(location), default_output);

  iface = HYSCAN_LOCATION_GET_IFACE(location);
  if (iface->get_depth != NULL)
    return (*iface->get_depth) (location, index);
  return default_output;
}

HyScanLatLongData
hyscan_location_get_latlong (HyScanLocation *location,
                             gint32          index)
{
  HyScanLocationInterface *iface;
  HyScanLatLongData default_output = {0};
  g_return_val_if_fail (HYSCAN_IS_LOCATION(location), default_output);

  iface = HYSCAN_LOCATION_GET_IFACE(location);
  if (iface->get_latlong != NULL)
    return (*iface->get_latlong) (location, index);
  return default_output;
}

HyScanAltitudeData
hyscan_location_get_altitude (HyScanLocation *location,
                             gint32          index)
{
  HyScanLocationInterface *iface;
  HyScanAltitudeData default_output = {0};
  g_return_val_if_fail (HYSCAN_IS_LOCATION(location), default_output);

  iface = HYSCAN_LOCATION_GET_IFACE(location);
  if (iface->get_altitude != NULL)
    return (*iface->get_altitude) (location, index);
  return default_output;
}

HyScanSpeedData
hyscan_location_get_speed (HyScanLocation *location,
                           gint32          index)
{
  HyScanLocationInterface *iface;
  HyScanSpeedData default_output = {0};
  g_return_val_if_fail (HYSCAN_IS_LOCATION(location), default_output);

  iface = HYSCAN_LOCATION_GET_IFACE(location);
  if (iface->get_speed != NULL)
    return (*iface->get_speed) (location, index);
  return default_output;
}

HyScanTrackData
hyscan_location_get_track (HyScanLocation *location,
                           gint32          index)
{
  HyScanLocationInterface *iface;
  HyScanTrackData default_output = {0};
  g_return_val_if_fail (HYSCAN_IS_LOCATION(location), default_output);

  iface = HYSCAN_LOCATION_GET_IFACE(location);
  if (iface->get_track != NULL)
    return (*iface->get_track) (location, index);
  return default_output;
}

HyScanRollPitchData
hyscan_location_get_rollpitch (HyScanLocation *location,
                               gint32          index)
{
  HyScanLocationInterface *iface;
  HyScanRollPitchData default_output = {0};
  g_return_val_if_fail (HYSCAN_IS_LOCATION(location), default_output);

  iface = HYSCAN_LOCATION_GET_IFACE(location);
  if (iface->get_rollpitch != NULL)
    return (*iface->get_rollpitch) (location, index);
  return default_output;
}
