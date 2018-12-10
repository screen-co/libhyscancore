#ifndef __HYSCAN_DEPTH_FACTORY_H__
#define __HYSCAN_DEPTH_FACTORY_H__

#include <hyscan-depthometer.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_DEPTH_FACTORY             (hyscan_depth_factory_get_type ())
#define HYSCAN_DEPTH_FACTORY(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_DEPTH_FACTORY, HyScanDepthFactory))
#define HYSCAN_IS_DEPTH_FACTORY(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_DEPTH_FACTORY))
#define HYSCAN_DEPTH_FACTORY_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_DEPTH_FACTORY, HyScanDepthFactoryClass))
#define HYSCAN_IS_DEPTH_FACTORY_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_DEPTH_FACTORY))
#define HYSCAN_DEPTH_FACTORY_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_DEPTH_FACTORY, HyScanDepthFactoryClass))

typedef struct _HyScanDepthFactory HyScanDepthFactory;
typedef struct _HyScanDepthFactoryPrivate HyScanDepthFactoryPrivate;
typedef struct _HyScanDepthFactoryClass HyScanDepthFactoryClass;

struct _HyScanDepthFactory
{
  GObject parent_instance;

  HyScanDepthFactoryPrivate *priv;
};

struct _HyScanDepthFactoryClass
{
  GObjectClass parent_class;
};

GType                  hyscan_depth_factory_get_type         (void);

HyScanDepthFactory *   hyscan_depth_factory_new              (HyScanCache            *cache);

gchar *                hyscan_depth_factory_get_token        (HyScanDepthFactory     *factory);

guint32                hyscan_depth_factory_get_hash         (HyScanDepthFactory     *factory);

void                   hyscan_depth_factory_set_track        (HyScanDepthFactory     *factory,
                                                              HyScanDB               *db,
                                                              const gchar            *project_name,
                                                              const gchar            *track_name);
HyScanDepthometer *    hyscan_depth_factory_produce          (HyScanDepthFactory     *factory);

G_END_DECLS

#endif /* __HYSCAN_DEPTH_FACTORY_H__ */
