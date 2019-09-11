#ifndef __HYSCAN_PLANNER_MODEL_H__
#define __HYSCAN_PLANNER_MODEL_H__

#include <hyscan-object-model.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_PLANNER_MODEL             (hyscan_planner_model_get_type ())
#define HYSCAN_PLANNER_MODEL(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_PLANNER_MODEL, HyScanPlannerModel))
#define HYSCAN_IS_PLANNER_MODEL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_PLANNER_MODEL))
#define HYSCAN_PLANNER_MODEL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_PLANNER_MODEL, HyScanPlannerModelClass))
#define HYSCAN_IS_PLANNER_MODEL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_PLANNER_MODEL))
#define HYSCAN_PLANNER_MODEL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_PLANNER_MODEL, HyScanPlannerModelClass))

typedef struct _HyScanPlannerModel HyScanPlannerModel;
typedef struct _HyScanPlannerModelPrivate HyScanPlannerModelPrivate;
typedef struct _HyScanPlannerModelClass HyScanPlannerModelClass;

struct _HyScanPlannerModel
{
  HyScanObjectModel parent_instance;

  HyScanPlannerModelPrivate *priv;
};

struct _HyScanPlannerModelClass
{
  HyScanObjectModelClass parent_class;
};

HYSCAN_API
GType                  hyscan_planner_model_get_type         (void);

HYSCAN_API
HyScanPlannerModel *   hyscan_planner_model_new              (void);

HYSCAN_API
HyScanGeo *            hyscan_planner_model_get_geo          (HyScanPlannerModel       *pmodel);

HYSCAN_API
void                   hyscan_planner_model_set_origin       (HyScanPlannerModel       *pmodel,
                                                              const HyScanGeoGeodetic  *origin);

HYSCAN_API
HyScanPlannerOrigin *  hyscan_planner_model_get_origin       (HyScanPlannerModel       *pmodel);

G_END_DECLS

#endif /* __HYSCAN_PLANNER_MODEL_H__ */
