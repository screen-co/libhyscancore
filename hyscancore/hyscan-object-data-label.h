/*
 * hyscan-object-data-label.h
 *
 *  Created on: 13 янв. 2020 г.
 *      Author: Andrey Zakharov <zaharov@screen-co.ru>
 */

#ifndef __HYSCAN_OBJECT_DATA_LABEL_H__
#define __HYSCAN_OBJECT_DATA_LABEL_H__

#include <hyscan-label.h>
#include <hyscan-object-data.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_OBJECT_DATA_LABEL             (hyscan_object_data_label_get_type ())
#define HYSCAN_OBJECT_DATA_LABEL(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_OBJECT_DATA_LABEL, HyScanObjectDataLabel))
#define HYSCAN_IS_OBJECT_DATA_LABEL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_OBJECT_DATA_LABEL))
#define HYSCAN_OBJECT_DATA_LABEL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_OBJECT_DATA_LABEL, HyScanObjectDataLabelClass))
#define HYSCAN_IS_OBJECT_DATA_LABEL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_OBJECT_DATA_LABEL))
#define HYSCAN_OBJECT_DATA_LABEL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_OBJECT_DATA_LABEL, HyScanObjectDataLabelClass))

typedef struct _HyScanObjectDataLabel        HyScanObjectDataLabel;
typedef struct _HyScanObjectDataLabelPrivate HyScanObjectDataLabelPrivate;
typedef struct _HyScanObjectDataLabelClass   HyScanObjectDataLabelClass;

struct _HyScanObjectDataLabel
{
  HyScanObjectData              parent_instance;
  HyScanObjectDataLabelPrivate *priv;
};

struct _HyScanObjectDataLabelClass
{
  HyScanObjectDataClass parent_class;
};

HYSCAN_API
GType                  hyscan_object_data_label_get_type    (void);

G_END_DECLS

#endif /* __HYSCAN_OBJECT_DATA_LABEL_H__ */
