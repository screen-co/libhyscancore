#ifndef __HYSCAN_MAP_TRACK_PARAM_H__
#define __HYSCAN_MAP_TRACK_PARAM_H__

#include <hyscan-db.h>
#include <hyscan-param.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_MAP_TRACK_PARAM             (hyscan_map_track_param_get_type ())
#define HYSCAN_MAP_TRACK_PARAM(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_MAP_TRACK_PARAM, HyScanMapTrackParam))
#define HYSCAN_IS_MAP_TRACK_PARAM(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_MAP_TRACK_PARAM))
#define HYSCAN_MAP_TRACK_PARAM_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_MAP_TRACK_PARAM, HyScanMapTrackParamClass))
#define HYSCAN_IS_MAP_TRACK_PARAM_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_MAP_TRACK_PARAM))
#define HYSCAN_MAP_TRACK_PARAM_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_MAP_TRACK_PARAM, HyScanMapTrackParamClass))

typedef struct _HyScanMapTrackParam HyScanMapTrackParam;
typedef struct _HyScanMapTrackParamPrivate HyScanMapTrackParamPrivate;
typedef struct _HyScanMapTrackParamClass HyScanMapTrackParamClass;

struct _HyScanMapTrackParam
{
  GObject parent_instance;

  HyScanMapTrackParamPrivate *priv;
};

struct _HyScanMapTrackParamClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                  hyscan_map_track_param_get_type         (void);

HYSCAN_API
HyScanMapTrackParam *  hyscan_map_track_param_new              (gchar                *profile,
                                                                HyScanDB             *db,
                                                                const gchar          *project_name,
                                                                const gchar          *track_name);

HYSCAN_API
guint32                hyscan_map_track_param_get_mod_count    (HyScanMapTrackParam  *param);

HYSCAN_API
gboolean               hyscan_map_track_param_has_rmc         (HyScanMapTrackParam  *param);

HYSCAN_API
gboolean               hyscan_map_track_param_clear            (HyScanMapTrackParam  *param);

G_END_DECLS

#endif /* __HYSCAN_MAP_TRACK_PARAM_H__ */
