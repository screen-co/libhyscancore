#include "hyscan-seabed.h"

G_DEFINE_INTERFACE (HyScanSeabed, hyscan_seabed, G_TYPE_OBJECT);

static void
hyscan_seabed_default_init (HyScanSeabedInterface *iface)
{
}

gdouble
hyscan_seabed_get_depth_by_index (HyScanSeabed *seabed, gint32 index)
{
  HyScanSeabedInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_SEABED(seabed), 0);

  iface = HYSCAN_SEABED_GET_IFACE(seabed);
  if (iface->get_depth_by_index != NULL)
    return (*iface->get_depth_by_index) (seabed, index);

  return 0;
}

void
hyscan_seabed_set_soundspeed (HyScanSeabed *seabed, GArray *soundspeedtable)
{
  HyScanSeabedInterface *iface;

  iface = HYSCAN_SEABED_GET_IFACE(seabed);
  (*iface->set_soundspeed) (seabed, soundspeedtable);
}
