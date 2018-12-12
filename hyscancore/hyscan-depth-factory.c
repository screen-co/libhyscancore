#include "hyscan-depth-factory.h"
#include <hyscan-nmea-parser.h>
#include <string.h>
#include <zlib.h>

enum
{
  PROP_0,
  PROP_CACHE,
};

struct _HyScanDepthFactoryPrivate
{
  HyScanCache  *cache;

  HyScanDB     *db;
  gchar        *project; /* */
  gchar        *track;

  GMutex        lock;
  guint32       hash;
  gchar        *token; // TODO: а нужен ли он ваще?
};

static void    hyscan_depth_factory_set_property             (GObject               *object,
                                                              guint                  prop_id,
                                                              const GValue          *value,
                                                              GParamSpec            *pspec);
static void    hyscan_depth_factory_object_constructed       (GObject               *object);
static void    hyscan_depth_factory_object_finalize          (GObject               *object);
static void    hyscan_depth_factory_updated                  (HyScanDepthFactory    *self);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanDepthFactory, hyscan_depth_factory, G_TYPE_OBJECT);

static void
hyscan_depth_factory_class_init (HyScanDepthFactoryClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);

  oclass->set_property = hyscan_depth_factory_set_property;
  oclass->constructed = hyscan_depth_factory_object_constructed;
  oclass->finalize = hyscan_depth_factory_object_finalize;

  g_object_class_install_property (oclass, PROP_CACHE,
    g_param_spec_object ("cache", "Cache", "HyScanCache interface",
                         HYSCAN_TYPE_CACHE,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_depth_factory_init (HyScanDepthFactory *depth_factory)
{
  depth_factory->priv = hyscan_depth_factory_get_instance_private (depth_factory);
}

static void
hyscan_depth_factory_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  HyScanDepthFactory *depth_factory = HYSCAN_DEPTH_FACTORY (object);
  HyScanDepthFactoryPrivate *priv = depth_factory->priv;

  switch (prop_id)
    {
    case PROP_CACHE:
      priv->cache = g_value_dup_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_depth_factory_object_constructed (GObject *object)
{
  HyScanDepthFactory *depth_factory = HYSCAN_DEPTH_FACTORY (object);
  HyScanDepthFactoryPrivate *priv = depth_factory->priv;

  g_mutex_init (&priv->lock);
}

static void
hyscan_depth_factory_object_finalize (GObject *object)
{
  HyScanDepthFactory *depth_factory = HYSCAN_DEPTH_FACTORY (object);
  HyScanDepthFactoryPrivate *priv = depth_factory->priv;

  g_object_unref (priv->cache);

  g_object_unref (priv->db);
  g_free (priv->project);
  g_free (priv->track);

  g_free (priv->token);

  g_mutex_clear (&priv->lock);

  G_OBJECT_CLASS (hyscan_depth_factory_parent_class)->finalize (object);
}

/* Функция обновляет состояние объекта. Вызывать строго за мьютексом! */
static void
hyscan_depth_factory_updated (HyScanDepthFactory *self)
{
  HyScanDepthFactoryPrivate *priv = self->priv;
  gchar *uri = NULL;

  g_clear_pointer (&priv->token, g_free);

  if (priv->db == NULL || priv->project == NULL || priv->track == NULL)
    {
      priv->token = NULL;
      priv->hash = 0;
      return;
    }

  uri = hyscan_db_get_uri (priv->db);

  priv->token = g_strdup_printf ("DepthFactory.%s.%s.%s",
                                 uri, priv->project, priv->track);

  priv->hash = crc32 (0L, (gpointer)(priv->token), strlen (priv->token));

  g_free (uri);
}

HyScanDepthFactory *
hyscan_depth_factory_new (HyScanCache *cache)
{
  return g_object_new (HYSCAN_TYPE_DEPTH_FACTORY,
                       "cache", cache,
                       NULL);
}

gchar *
hyscan_depth_factory_get_token (HyScanDepthFactory *self)
{
  HyScanDepthFactoryPrivate *priv;
  gchar *token = NULL;

  g_return_val_if_fail (HYSCAN_IS_DEPTH_FACTORY (self), NULL);
  priv = self->priv;

  g_mutex_lock (&priv->lock);
  if (priv->token != NULL)
    token = g_strdup (priv->token);
  g_mutex_unlock (&priv->lock);

  return token;
}

guint32
hyscan_depth_factory_get_hash (HyScanDepthFactory *self)
{
  HyScanDepthFactoryPrivate *priv;
  guint32 hash;

  g_return_val_if_fail (HYSCAN_IS_DEPTH_FACTORY (self), 0);
  priv = self->priv;

  g_mutex_lock (&priv->lock);
  hash = priv->hash;
  g_mutex_unlock (&priv->lock);

  return hash;
}


void
hyscan_depth_factory_set_track (HyScanDepthFactory *self,
                                HyScanDB           *db,
                                const gchar        *project_name,
                                const gchar        *track_name)
{
  HyScanDepthFactoryPrivate *priv;

  g_return_if_fail (HYSCAN_IS_DEPTH_FACTORY (self));
  priv = self->priv;

  g_mutex_lock (&priv->lock);

  g_clear_object (&priv->db);
  g_clear_pointer (&priv->project, g_free);
  g_clear_pointer (&priv->track, g_free);

  priv->db = g_object_ref (db);
  priv->project = g_strdup (project_name);
  priv->track = g_strdup (track_name);

  hyscan_depth_factory_updated (self);

  g_mutex_unlock (&priv->lock);
}

HyScanDepthometer *
hyscan_depth_factory_produce (HyScanDepthFactory *self)
{
  HyScanNMEAParser *parser = NULL;
  HyScanDepthometer *depth = NULL;
  HyScanDepthFactoryPrivate *priv;
  HyScanDB * db;
  gchar * project;
  gchar * track;

  g_return_val_if_fail (HYSCAN_IS_DEPTH_FACTORY (self), NULL);
  priv = self->priv;

  g_mutex_lock (&priv->lock);
  db = (priv->db != NULL) ? g_object_ref (priv->db) : NULL;
  project = (priv->project != NULL) ? g_strdup (priv->project) : NULL;
  track = (priv->track != NULL) ? g_strdup (priv->track) : NULL;
  g_mutex_unlock (&priv->lock);

  if (db == NULL || project == NULL || track == NULL)
    {
      depth = NULL;
      goto fail;
    }

  parser = hyscan_nmea_parser_new (priv->db, priv->cache,
                                   priv->project, priv->track, 1,
                                   HYSCAN_SOURCE_NMEA_DPT,
                                   HYSCAN_NMEA_FIELD_DEPTH);

  if (parser == NULL)
    {
      depth = NULL;
      goto fail;
    }

  depth = hyscan_depthometer_new (HYSCAN_NAV_DATA (parser), priv->cache);

fail:
  g_clear_object (&db);
  g_clear_object (&parser);
  g_clear_pointer (&project, g_free);
  g_clear_pointer (&track, g_free);

  return depth;
}
