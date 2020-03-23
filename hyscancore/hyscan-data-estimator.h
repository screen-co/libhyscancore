#ifndef _HYSCAN_DATA_ESTIMATOR_H_
#define _HYSCAN_DATA_ESTIMATOR_H_

#include <glib-object.h> 
#include "hyscan-acoustic-data.h"
#include "hyscan-nmea-data.h"
#include "hyscan-nav-data.h"


G_BEGIN_DECLS

#define HYSCAN_TYPE_DATA_ESTIMATOR             (hyscan_data_estimator_get_type ())
#define HYSCAN_DATA_ESTIMATOR(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_DATA_ESTIMATOR, HyScanDataEstimator))
#define HYSCAN_IS_DATA_ESTIMATOR(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_DATA_ESTIMATOR))
#define HYSCAN_DATA_ESTIMATOR_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_DATA_ESTIMATOR, HyScanDataEstimatorClass))
#define HYSCAN_IS_DATA_ESTIMATOR_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_DATA_ESTIMATOR))
#define HYSCAN_DATA_ESTIMATOR_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_DATA_ESTIMATOR, HyScanDataEstimatorClass))

typedef struct _HyScanDataEstimator HyScanDataEstimator;
typedef struct _HyScanDataEstimatorPrivate HyScanDataEstimatorPrivate;
typedef struct _HyScanDataEstimatorClass HyScanDataEstimatorClass;

struct _HyScanDataEstimator
{
  GObject parent_instance;

  HyScanDataEstimatorPrivate *priv;
};

struct _HyScanDataEstimatorClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                hyscan_data_estimator_get_type          (void);

HYSCAN_API
HyScanDataEstimator* hyscan_data_estimator_new               (HyScanAcousticData  *signal_data,
                                                              HyScanAcousticData  *noise_data,
                                                              HyScanNavData       *navigation_data);

HYSCAN_API
const guint32 *      hyscan_data_estimator_get_acust_quality (HyScanDataEstimator *self,
                                                              guint32              row_index,
                                                              guint32             *n_points);

HYSCAN_API
void                 hyscan_data_estimator_set_max_quality   (HyScanDataEstimator *self,
                                                              guint32              max_quality);
HYSCAN_API
guint32              hyscan_data_estimator_get_max_quality   (HyScanDataEstimator *self);

HYSCAN_API
gint                 hyscan_data_estimator_get_navig_quality (HyScanDataEstimator *self,
                                                              gint64               signal_time,
                                                              guint32             *quality);

G_END_DECLS

#endif /* __HYSCAN_DATA_ESTIMATOR_H__ */
