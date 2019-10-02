#ifndef __HYSCAN_OBJECT_DATA_GEOMARK_H__
#define __HYSCAN_OBJECT_DATA_GEOMARK_H__

#include <hyscan-object-data.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_OBJECT_DATA_GEOMARK             (hyscan_object_data_geomark_get_type ())
#define HYSCAN_OBJECT_DATA_GEOMARK(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_OBJECT_DATA_GEOMARK, HyScanObjectDataGeomark))
#define HYSCAN_IS_OBJECT_DATA_GEOMARK(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_OBJECT_DATA_GEOMARK))
#define HYSCAN_OBJECT_DATA_GEOMARK_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_OBJECT_DATA_GEOMARK, HyScanObjectDataGeomarkClass))
#define HYSCAN_IS_OBJECT_DATA_GEOMARK_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_OBJECT_DATA_GEOMARK))
#define HYSCAN_OBJECT_DATA_GEOMARK_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_OBJECT_DATA_GEOMARK, HyScanObjectDataGeomarkClass))

typedef struct _HyScanObjectDataGeomark HyScanObjectDataGeomark;
typedef struct _HyScanObjectDataGeomarkPrivate HyScanObjectDataGeomarkPrivate;
typedef struct _HyScanObjectDataGeomarkClass HyScanObjectDataGeomarkClass;

struct _HyScanObjectDataGeomark
{
  HyScanObjectData                parent_instance;
  HyScanObjectDataGeomarkPrivate *priv;
};

struct _HyScanObjectDataGeomarkClass
{
  HyScanObjectDataClass parent_class;
};

HYSCAN_API
GType                  hyscan_object_data_geomark_get_type    (void);

G_END_DECLS

#endif /* __HYSCAN_OBJECT_DATA_GEOMARK_H__ */
