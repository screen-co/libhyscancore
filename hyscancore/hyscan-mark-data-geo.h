#ifndef __HYSCAN_MARK_DATA_GEO_H__
#define __HYSCAN_MARK_DATA_GEO_H__

#include <hyscan-mark-data.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_MARK_DATA_GEO             (hyscan_mark_data_geo_get_type ())
#define HYSCAN_MARK_DATA_GEO(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_MARK_DATA_GEO, HyScanMarkDataGeo))
#define HYSCAN_IS_MARK_DATA_GEO(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_MARK_DATA_GEO))
#define HYSCAN_MARK_DATA_GEO_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_MARK_DATA_GEO, HyScanMarkDataGeoClass))
#define HYSCAN_IS_MARK_DATA_GEO_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_MARK_DATA_GEO))
#define HYSCAN_MARK_DATA_GEO_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_MARK_DATA_GEO, HyScanMarkDataGeoClass))

typedef struct _HyScanMarkDataGeo HyScanMarkDataGeo;
typedef struct _HyScanMarkDataGeoClass HyScanMarkDataGeoClass;

struct _HyScanMarkDataGeo
{
  HyScanMarkData parent_instance;
};

struct _HyScanMarkDataGeoClass
{
  HyScanMarkDataClass parent_class;
};

HYSCAN_API
GType                  hyscan_mark_data_geo_get_type    (void);

HYSCAN_API
HyScanMarkData *       hyscan_mark_data_geo_new         (HyScanDB    *db,
                                                         const gchar *project);
G_END_DECLS

#endif /* __HYSCAN_MARK_DATA_GEO_H__ */
