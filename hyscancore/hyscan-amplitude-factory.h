#ifndef __HYSCAN_AMPLITUDE_FACTORY_H__
#define __HYSCAN_AMPLITUDE_FACTORY_H__

#include <hyscan-types.h>
#include <hyscan-amplitude.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_AMPLITUDE_FACTORY             (hyscan_amplitude_factory_get_type ())
#define HYSCAN_AMPLITUDE_FACTORY(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_AMPLITUDE_FACTORY, HyScanAmplitudeFactory))
#define HYSCAN_IS_AMPLITUDE_FACTORY(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_AMPLITUDE_FACTORY))
#define HYSCAN_AMPLITUDE_FACTORY_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_AMPLITUDE_FACTORY, HyScanAmplitudeFactoryClass))
#define HYSCAN_IS_AMPLITUDE_FACTORY_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_AMPLITUDE_FACTORY))
#define HYSCAN_AMPLITUDE_FACTORY_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_AMPLITUDE_FACTORY, HyScanAmplitudeFactoryClass))

typedef struct _HyScanAmplitudeFactory HyScanAmplitudeFactory;
typedef struct _HyScanAmplitudeFactoryPrivate HyScanAmplitudeFactoryPrivate;
typedef struct _HyScanAmplitudeFactoryClass HyScanAmplitudeFactoryClass;

struct _HyScanAmplitudeFactory
{
  GObject parent_instance;

  HyScanAmplitudeFactoryPrivate *priv;
};

struct _HyScanAmplitudeFactoryClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                    hyscan_amplitude_factory_get_type        (void);

HYSCAN_API
HyScanAmplitudeFactory * hyscan_amplitude_factory_new             (HyScanCache            *cache);

HYSCAN_API
gchar *                  hyscan_amplitude_factory_get_token       (HyScanAmplitudeFactory *factory);

HYSCAN_API
guint32                  hyscan_amplitude_factory_get_hash        (HyScanAmplitudeFactory *factory);

HYSCAN_API
void                     hyscan_amplitude_factory_set_track       (HyScanAmplitudeFactory *factory,
                                                                   HyScanDB               *db,
                                                                   const gchar            *project_name,
                                                                   const gchar            *track_name);

HYSCAN_API
HyScanAmplitude *        hyscan_amplitude_factory_produce         (HyScanAmplitudeFactory *factory,
                                                                   HyScanSourceType        source);

G_END_DECLS

#endif /* __HYSCAN_AMPLITUDE_FACTORY_H__ */

