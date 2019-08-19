#ifndef __HYSCAN_PLANNER_DATA_H__
#define __HYSCAN_PLANNER_DATA_H__

#include <hyscan-planner.h>
#include <hyscan-object-data.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_PLANNER_DATA             (hyscan_planner_data_get_type ())
#define HYSCAN_PLANNER_DATA(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_PLANNER_DATA, HyScanPlannerData))
#define HYSCAN_IS_PLANNER_DATA(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_PLANNER_DATA))
#define HYSCAN_PLANNER_DATA_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_PLANNER_DATA, HyScanPlannerDataClass))
#define HYSCAN_IS_PLANNER_DATA_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_PLANNER_DATA))
#define HYSCAN_PLANNER_DATA_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_PLANNER_DATA, HyScanPlannerDataClass))

typedef struct _HyScanPlannerData HyScanPlannerData;
typedef struct _HyScanPlannerDataPrivate HyScanPlannerDataPrivate;
typedef struct _HyScanPlannerDataClass HyScanPlannerDataClass;

struct _HyScanPlannerData
{
  HyScanObjectData parent_instance;

  HyScanPlannerDataPrivate *priv;
};

struct _HyScanPlannerDataClass
{
  HyScanObjectDataClass parent_class;
};

HYSCAN_API
GType                  hyscan_planner_data_get_type         (void);

HYSCAN_API
HyScanObjectData *     hyscan_planner_data_new              (HyScanDB    *db,
                                                             const gchar *project);

G_END_DECLS

#endif /* __HYSCAN_PLANNER_DATA_H__ */
