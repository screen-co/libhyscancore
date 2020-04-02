#ifndef __HYSCAN_NMEA_HELPER_H__
#define __HYSCAN_NMEA_HELPER_H__

#include <hyscan-geo.h>

G_BEGIN_DECLS

HYSCAN_API
gchar *                hyscan_nmea_helper_make_rmc         (HyScanGeoGeodetic  coord,
                                                            gdouble            velocity,
                                                            gint64             time);

HYSCAN_API
gchar *                hyscan_nmea_helper_make_gga         (HyScanGeoGeodetic  coord,
                                                            gint64             time);

HYSCAN_API
gchar *                hyscan_nmea_helper_wrap             (const gchar       *inner);

G_END_DECLS

#endif /* __HYSCAN_NMEA_HELPER_H__ */
