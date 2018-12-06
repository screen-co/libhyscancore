#include "hyscan-amplitude-factory.h"

enum
{
  PROP_CACHE = 1
};

struct _HyScanAmplitudeFactoryPrivate
{
  HyScanCache  *cache; /* Кэш. */
};

static void    hyscan_amplitude_factory_set_property             (GObject               *object,
                                                                  guint                  prop_id,
                                                                  const GValue          *value,
                                                                  GParamSpec            *pspec);
static void    hyscan_amplitude_factory_object_constructed       (GObject               *object);
static void    hyscan_amplitude_factory_object_finalize          (GObject               *object);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanAmplitudeFactory, hyscan_amplitude_factory, G_TYPE_OBJECT);

static void
hyscan_amplitude_factory_class_init (HyScanAmplitudeFactoryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_amplitude_factory_set_property;

  object_class->constructed = hyscan_amplitude_factory_object_constructed;
  object_class->finalize = hyscan_amplitude_factory_object_finalize;

  g_object_class_install_property (object_class, PROP_CACHE,
    g_param_spec_object ("cache", "Cache", "HyScanCache interface",
                         HYSCAN_TYPE_CACHE,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_amplitude_factory_init (HyScanAmplitudeFactory *self)
{
  self->priv = hyscan_amplitude_factory_get_instance_private (self);
}

static void
hyscan_amplitude_factory_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  HyScanAmplitudeFactory *self = HYSCAN_AMPLITUDE_FACTORY (object);
  HyScanAmplitudeFactoryPrivate *priv = self->priv;

  switch (prop_id)
    {
    case PROP_CACHE:
      priv->cache = g_value_dup_object (value);
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_amplitude_factory_object_constructed (GObject *object)
{
  /*
  HyScanAmplitudeFactory *self = HYSCAN_AMPLITUDE_FACTORY (object);
  HyScanAmplitudeFactoryPrivate *priv = self->priv;
  */
}

static void
hyscan_amplitude_factory_object_finalize (GObject *object)
{
  HyScanAmplitudeFactory *self = HYSCAN_AMPLITUDE_FACTORY (object);
  HyScanAmplitudeFactoryPrivate *priv = self->priv;

  g_object_unref (priv->cache);
}

/* .*/
HyScanAmplitudeFactory *
hyscan_amplitude_factory_new (HyScanCache *cache)
{
  return g_object_new (HYSCAN_TYPE_AMPLITUDE_FACTORY,
                       "cache", cache, NULL);
}

HyScanAmplitude *
hyscan_amplitude_factory_produce (HyScanAmplitudeFactory *self,
                                  HyScanDB               *db,
                                  const gchar            *project,
                                  const gchar            *track,
                                  HyScanSourceType        source)
{
  g_return_val_if_fail (HYSCAN_IS_AMPLITUDE_FACTORY (self), NULL);

  return HYSCAN_AMPLITUDE (hyscan_amplitude_simple_new (db, self->priv->cache,
                                                        project, track, source,
                                                        1, FALSE));
}
