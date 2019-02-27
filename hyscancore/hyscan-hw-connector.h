#ifndef __HYSCAN_HW_CONNECTOR_H__
#define __HYSCAN_HW_CONNECTOR_H__

#include <hyscan-control.h>
#include <hyscan-driver.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_HW_CONNECTOR             (hyscan_hw_connector_get_type ())
#define HYSCAN_HW_CONNECTOR(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_HW_CONNECTOR, HyScanHWConnector))
#define HYSCAN_IS_HW_CONNECTOR(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_HW_CONNECTOR))
#define HYSCAN_HW_CONNECTOR_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_HW_CONNECTOR, HyScanHWConnectorClass))
#define HYSCAN_IS_HW_CONNECTOR_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_HW_CONNECTOR))
#define HYSCAN_HW_CONNECTOR_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_HW_CONNECTOR, HyScanHWConnectorClass))

typedef struct _HyScanHWConnector HyScanHWConnector;
typedef struct _HyScanHWConnectorPrivate HyScanHWConnectorPrivate;
typedef struct _HyScanHWConnectorClass HyScanHWConnectorClass;

/* TODO: Change GObject to type of the base class. */
struct _HyScanHWConnector
{
  GObject parent_instance;

  HyScanHWConnectorPrivate *priv;
};

/* TODO: Change GObjectClass to type of the base class. */
struct _HyScanHWConnectorClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                      hyscan_hw_connector_get_type         (void);

HYSCAN_API
HyScanHWConnector *        hyscan_hw_connector_new              (void);

HYSCAN_API
void                       hyscan_hw_connector_set_driver_paths (HyScanHWConnector   *connector,
                                                                 const gchar * const *paths);

HYSCAN_API
gboolean                   hyscan_hw_connector_read             (HyScanHWConnector   *connector,
                                                                 const gchar         *file);

HYSCAN_API
gboolean                   hyscan_hw_connector_check            (HyScanHWConnector   *connector);

HYSCAN_API
HyScanControl *            hyscan_hw_connector_connect          (HyScanHWConnector   *connector);

G_END_DECLS

#endif /* __HYSCAN_HW_CONNECTOR_H__ */
