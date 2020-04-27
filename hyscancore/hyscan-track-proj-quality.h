#ifndef __HYSCAN_TRACK_PROJ_QUALITY_H__
#define __HYSCAN_TRACK_PROJ_QUALITY_H__

#include <glib-object.h>
#include <hyscan-db.h>
#include <hyscan-cache.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_TRACK_PROJ_QUALITY             (hyscan_track_proj_quality_get_type ())
#define HYSCAN_TRACK_PROJ_QUALITY(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_TRACK_PROJ_QUALITY, HyScanTrackProjQuality))
#define HYSCAN_IS_TRACK_PROJ_QUALITY(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_TRACK_PROJ_QUALITY))
#define HYSCAN_TRACK_PROJ_QUALITY_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_TRACK_PROJ_QUALITY, HyScanTrackProjQualityClass))
#define HYSCAN_IS_TRACK_PROJ_QUALITY_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_TRACK_PROJ_QUALITY))
#define HYSCAN_TRACK_PROJ_QUALITY_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_TRACK_PROJ_QUALITY, HyScanTrackProjQualityClass))

typedef struct _HyScanTrackProjQuality HyScanTrackProjQuality;
typedef struct _HyScanTrackProjQualityPrivate HyScanTrackProjQualityPrivate;
typedef struct _HyScanTrackProjQualityClass HyScanTrackProjQualityClass;

typedef struct
{
  gdouble start;
  gdouble quality;
} HyScanTrackCovSection;

/* !!! Change GObject to type of the base class. !!! */
struct _HyScanTrackProjQuality
{
  GObject parent_instance;

  HyScanTrackProjQualityPrivate *priv;
};

/* !!! Change GObjectClass to type of the base class. !!! */
struct _HyScanTrackProjQualityClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                    hyscan_track_proj_quality_get_type         (void);

HYSCAN_API
HyScanTrackProjQuality * hyscan_track_proj_quality_new            (HyScanDB                         *db,
                                                                   HyScanCache                      *cache,
                                                                   const gchar                      *project,
                                                                   const gchar                      *track);

HYSCAN_API
gboolean                 hyscan_track_proj_quality_get            (HyScanTrackProjQuality           *quality_proj,
                                                                   guint32                           index,
                                                                   const HyScanTrackCovSection     **values,
                                                                   gsize                            *n_values);



G_END_DECLS

#endif /* __HYSCAN_TRACK_PROJ_QUALITY_H__ */
