#ifndef __HYSCAN_TRACK_DATA_H__
#define __HYSCAN_TRACK_DATA_H__

#include <hyscan-nav-data.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_TRACK_DATA             (hyscan_track_data_get_type ())
#define HYSCAN_TRACK_DATA(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_TRACK_DATA, HyScanTrackData))
#define HYSCAN_IS_TRACK_DATA(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_TRACK_DATA))
#define HYSCAN_TRACK_DATA_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_TRACK_DATA, HyScanTrackDataClass))
#define HYSCAN_IS_TRACK_DATA_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_TRACK_DATA))
#define HYSCAN_TRACK_DATA_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_TRACK_DATA, HyScanTrackDataClass))

typedef struct _HyScanTrackData HyScanTrackData;
typedef struct _HyScanTrackDataPrivate HyScanTrackDataPrivate;
typedef struct _HyScanTrackDataClass HyScanTrackDataClass;

struct _HyScanTrackData
{
  GObject parent_instance;

  HyScanTrackDataPrivate *priv;
};

struct _HyScanTrackDataClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType           hyscan_track_data_get_type (void);

HYSCAN_API
HyScanNavData * hyscan_track_data_new      (HyScanNavData *lat,
                                            HyScanNavData *lon);

G_END_DECLS

#endif /* __HYSCAN_TRACK_DATA_H__ */
