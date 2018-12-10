#include "hyscan-amplitude-factory.h"
#include <hyscan-acoustic-data.h>
#include <string.h>
#include <zlib.h> /* crc32 */

enum
{
  PROP_CACHE = 1
};

struct _HyScanAmplitudeFactoryPrivate
{
  HyScanCache  *cache;   /* Кэш. */

  HyScanDB     *db;
  gchar        *project; /* */
  gchar        *track;

  GMutex        lock;
  guint32       hash;
  gchar        *token; // TODO: а нужен ли он ваще?
};

static void    hyscan_amplitude_factory_set_property             (GObject                *object,
                                                                  guint                   prop_id,
                                                                  const GValue           *value,
                                                                  GParamSpec             *pspec);
static void    hyscan_amplitude_factory_object_constructed       (GObject                *object);
static void    hyscan_amplitude_factory_object_finalize          (GObject                *object);
static void    hyscan_amplitude_factory_updated                  (HyScanAmplitudeFactory *self);

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
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_amplitude_factory_object_constructed (GObject *object)
{
  HyScanAmplitudeFactory *self = HYSCAN_AMPLITUDE_FACTORY (object);
  HyScanAmplitudeFactoryPrivate *priv = self->priv;

  g_mutex_init (&priv->lock);
}

static void
hyscan_amplitude_factory_object_finalize (GObject *object)
{
  HyScanAmplitudeFactory *self = HYSCAN_AMPLITUDE_FACTORY (object);
  HyScanAmplitudeFactoryPrivate *priv = self->priv;

  g_object_unref (priv->cache);

  g_object_unref (priv->db);
  g_free (priv->project);
  g_free (priv->track);

  g_free (priv->token);

  g_mutex_clear (&priv->lock);

  G_OBJECT_CLASS (hyscan_amplitude_factory_parent_class)->finalize (object);
}

/* Функция обновляет состояние объекта. Вызывать строго за мьютексом! */
static void
hyscan_amplitude_factory_updated (HyScanAmplitudeFactory *self)
{
  HyScanAmplitudeFactoryPrivate *priv = self->priv;
  gchar *uri = NULL;

  g_clear_pointer (&priv->token, g_free);

  if (priv->db == NULL || priv->project == NULL || priv->track == NULL)
    {
      priv->token = NULL;
      priv->hash = 0;
      return;
    }

  uri = hyscan_db_get_uri (priv->db);

  priv->token = g_strdup_printf ("AmplitudeFactory.%s.%s.%s",
                                 uri, priv->project, priv->track);

  priv->hash = crc32 (0L, (gpointer)(priv->token), strlen (priv->token));

  g_free (uri);
}

/* */
HyScanAmplitudeFactory *
hyscan_amplitude_factory_new (HyScanCache *cache)
{
  return g_object_new (HYSCAN_TYPE_AMPLITUDE_FACTORY,
                       "cache", cache, NULL);
}

gchar *
hyscan_amplitude_factory_get_token (HyScanAmplitudeFactory *self)
{
  HyScanAmplitudeFactoryPrivate *priv;
  gchar *token = NULL;

  g_return_val_if_fail (HYSCAN_IS_AMPLITUDE_FACTORY (self), NULL);
  priv = self->priv;

  g_mutex_lock (&priv->lock);
  token = g_strdup (priv->token);
  g_mutex_unlock (&priv->lock);

  return token;
}

guint32
hyscan_amplitude_factory_get_hash (HyScanAmplitudeFactory *self)
{
  HyScanAmplitudeFactoryPrivate *priv;
  guint32 hash;

  g_return_val_if_fail (HYSCAN_IS_AMPLITUDE_FACTORY (self), 0);
  priv = self->priv;

  g_mutex_lock (&priv->lock);
  hash = priv->hash;
  g_mutex_unlock (&priv->lock);

  return hash;
}

void
hyscan_amplitude_factory_set_track (HyScanAmplitudeFactory *self,
                                    HyScanDB               *db,
                                    const gchar            *project_name,
                                    const gchar            *track_name)
{
  HyScanAmplitudeFactoryPrivate *priv;

  g_return_if_fail (HYSCAN_IS_AMPLITUDE_FACTORY (self));
  priv = self->priv;

  g_mutex_lock (&priv->lock);

  g_clear_object (&priv->db);
  g_clear_pointer (&priv->project, g_free);
  g_clear_pointer (&priv->track, g_free);

  priv->db = g_object_ref (db);
  priv->project = g_strdup (project_name);
  priv->track = g_strdup (track_name);

  hyscan_amplitude_factory_updated (self);

  g_mutex_unlock (&priv->lock);
}

HyScanAmplitude *
hyscan_amplitude_factory_produce (HyScanAmplitudeFactory *self,
                                  HyScanSourceType        source)
{
  HyScanAmplitudeFactoryPrivate *priv;
  HyScanAmplitude * out;
  HyScanDB * db = NULL;
  gchar * project = NULL;
  gchar * track = NULL;

  g_return_val_if_fail (HYSCAN_IS_AMPLITUDE_FACTORY (self), NULL);
  priv = self->priv;

  /* Запоминаем актуальные значения. */
  g_mutex_lock (&priv->lock);
  db = g_object_ref (priv->db);
  project = g_strdup (priv->project);
  track = g_strdup (priv->track);
  g_mutex_unlock (&priv->lock);

  out = HYSCAN_AMPLITUDE (hyscan_acoustic_data_new (db, self->priv->cache,
                                                    project, track, source,
                                                    1, FALSE));

  g_object_unref (db);
  g_free (project);
  g_free (track);

  return out;
}
