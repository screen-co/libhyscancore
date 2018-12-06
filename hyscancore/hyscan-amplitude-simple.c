#include "hyscan-amplitude-simple.h"

enum
{
  PROP_O,
  PROP_DB,
  PROP_CACHE,
  PROP_PROJECT_NAME,
  PROP_TRACK_NAME,
  PROP_SOURCE,
  PROP_CHANNEL,
  PROP_NOISE
};

struct _HyScanAmplitudeSimplePrivate
{
  HyScanCache                 *cache;                  /* Кэш данных. */
  HyScanDB                    *db;                     /* Интерфейс базы данных. */
  gchar                       *project_name;           /* Название проекта. */
  gchar                       *track_name;             /* Название галса. */
  HyScanSourceType             source;                 /* Тип источника данных. */
  guint                        channel;                /* Индекс канала данных. */
  gboolean                     noise;                  /* Канал с шумами. */

  HyScanAcousticData          *data;                   /* Канал данных. */
  gchar                       *token;                  /* Токен. */
};

static void    hyscan_amplitude_simple_interface_init           (HyScanAmplitudeInterface *iface);
static void    hyscan_amplitude_simple_set_property             (GObject               *object,
                                                                 guint                  prop_id,
                                                                 const GValue          *value,
                                                                 GParamSpec            *pspec);
static void    hyscan_amplitude_simple_object_constructed       (GObject               *object);
static void    hyscan_amplitude_simple_object_finalize          (GObject               *object);

G_DEFINE_TYPE_WITH_CODE (HyScanAmplitudeSimple, hyscan_amplitude_simple, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (HyScanAmplitudeSimple)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_AMPLITUDE, hyscan_amplitude_simple_interface_init));


static void
hyscan_amplitude_simple_class_init (HyScanAmplitudeSimpleClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);

  oclass->set_property = hyscan_amplitude_simple_set_property;
  oclass->constructed = hyscan_amplitude_simple_object_constructed;
  oclass->finalize = hyscan_amplitude_simple_object_finalize;

  g_object_class_install_property (oclass, PROP_DB,
    g_param_spec_object ("db", "DB", "HyScanDB interface", HYSCAN_TYPE_DB,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (oclass, PROP_CACHE,
    g_param_spec_object ("cache", "Cache", "HyScanCache interface", HYSCAN_TYPE_CACHE,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (oclass, PROP_PROJECT_NAME,
    g_param_spec_string ("project-name", "ProjectName", "Project name", NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (oclass, PROP_TRACK_NAME,
    g_param_spec_string ("track-name", "TrackName", "Track name", NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (oclass, PROP_SOURCE,
    g_param_spec_int ("source", "Source", "Source type", 0, G_MAXINT, 0,
                      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (oclass, PROP_CHANNEL,
    g_param_spec_uint ("channel", "Channel", "Source channel", 1, G_MAXUINT, 1,
                       G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (oclass, PROP_NOISE,
    g_param_spec_boolean ("noise", "Noise", "Use noise channel", FALSE,
                          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_amplitude_simple_init (HyScanAmplitudeSimple *self)
{
  self->priv = hyscan_amplitude_simple_get_instance_private (self);
}

static void
hyscan_amplitude_simple_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  HyScanAmplitudeSimple *self = HYSCAN_AMPLITUDE_SIMPLE (object);
  HyScanAmplitudeSimplePrivate *priv = self->priv;

  switch (prop_id)
    {
    case PROP_DB:
      priv->db = g_value_dup_object (value);
      break;

    case PROP_CACHE:
      priv->cache = g_value_dup_object (value);
      break;

    case PROP_PROJECT_NAME:
      priv->project_name = g_value_dup_string (value);
      break;

    case PROP_TRACK_NAME:
      priv->track_name = g_value_dup_string (value);
      break;

    case PROP_SOURCE:
      priv->source = g_value_get_int (value);
      break;

    case PROP_CHANNEL:
      priv->channel = g_value_get_uint (value);
      break;

    case PROP_NOISE:
      priv->noise = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_amplitude_simple_object_constructed (GObject *object)
{
  gchar *db_uri;
  HyScanAcousticData *adata;
  HyScanAmplitudeSimple *self = HYSCAN_AMPLITUDE_SIMPLE (object);
  HyScanAmplitudeSimplePrivate *priv = self->priv;

  adata = hyscan_acoustic_data_new (priv->db, priv->cache,
                                    priv->project_name, priv->track_name,
                                    priv->source, priv->channel, priv->noise);

  if (adata == NULL)
    return;

  priv->data = adata;

  db_uri = hyscan_db_get_uri (priv->db);

  priv->token = g_strdup_printf ("AmplitudeSimple.%s%s%s%i%i%i",
                                 db_uri, priv->project_name, priv->track_name,
                                 priv->source, priv->channel, priv->noise);
  g_free (db_uri);
}

static void
hyscan_amplitude_simple_object_finalize (GObject *object)
{
  HyScanAmplitudeSimple *self = HYSCAN_AMPLITUDE_SIMPLE (object);
  HyScanAmplitudeSimplePrivate *priv = self->priv;

  g_object_unref (priv->cache);
  g_object_unref (priv->db);
  g_free (priv->project_name);
  g_free (priv->track_name);

  g_object_unref (priv->data);
  g_free (priv->token);

  G_OBJECT_CLASS (hyscan_amplitude_simple_parent_class)->finalize (object);
}

HyScanAmplitudeSimple *
hyscan_amplitude_simple_new (HyScanDB         *db,
                             HyScanCache      *cache,
                             const gchar      *project_name,
                             const gchar      *track_name,
                             HyScanSourceType  source,
                             guint             channel,
                             gboolean          noise)
{
  HyScanAmplitudeSimple *self;

  self = g_object_new (HYSCAN_TYPE_AMPLITUDE_SIMPLE,
                       "db", db,
                       "cache", cache,
                       "project-name", project_name,
                       "track-name", track_name,
                       "source", source,
                       "channel", channel,
                       "noise", noise,
                       NULL);

  if (self->priv->data == NULL)
    g_clear_object (&self);

  return self;
}

const gchar *
hyscan_amplitude_simple_get_token (HyScanAmplitude *amplitude)
{
  HyScanAmplitudeSimple *self = HYSCAN_AMPLITUDE_SIMPLE (amplitude);
  return self->priv->token;
}

HyScanAntennaPosition
hyscan_amplitude_simple_get_position (HyScanAmplitude *amplitude)
{
  HyScanAmplitudeSimple *self = HYSCAN_AMPLITUDE_SIMPLE (amplitude);
  return hyscan_acoustic_data_get_position (self->priv->data);
}

HyScanAcousticDataInfo
hyscan_amplitude_simple_get_info (HyScanAmplitude *amplitude)
{
  HyScanAmplitudeSimple *self = HYSCAN_AMPLITUDE_SIMPLE (amplitude);
  return hyscan_acoustic_data_get_info (self->priv->data);
}

gboolean
hyscan_amplitude_simple_is_writable (HyScanAmplitude *amplitude)
{
  HyScanAmplitudeSimple *self = HYSCAN_AMPLITUDE_SIMPLE (amplitude);
  return hyscan_acoustic_data_is_writable (self->priv->data);
}

guint32
hyscan_amplitude_simple_get_mod_count (HyScanAmplitude *amplitude)
{
  HyScanAmplitudeSimple *self = HYSCAN_AMPLITUDE_SIMPLE (amplitude);
  return hyscan_acoustic_data_get_mod_count (self->priv->data);
}

gboolean
hyscan_amplitude_simple_get_range (HyScanAmplitude *amplitude,
                                   guint32         *first,
                                   guint32         *last)
{
  HyScanAmplitudeSimple *self = HYSCAN_AMPLITUDE_SIMPLE (amplitude);
  return hyscan_acoustic_data_get_range (self->priv->data, first, last);
}

HyScanDBFindStatus
hyscan_amplitude_simple_find_data (HyScanAmplitude *amplitude,
                                   gint64           time,
                                   guint32         *lindex,
                                   guint32         *rindex,
                                   gint64          *ltime,
                                   gint64          *rtime)
{
  HyScanAmplitudeSimple *self = HYSCAN_AMPLITUDE_SIMPLE (amplitude);
  return hyscan_acoustic_data_find_data (self->priv->data, time,
                                         lindex, rindex, ltime, rtime);
}

gboolean
hyscan_amplitude_simple_get_size_time (HyScanAmplitude *amplitude,
                                       guint32          index,
                                       guint32         *n_points,
                                       gint64          *time)
{
  HyScanAmplitudeSimple *self = HYSCAN_AMPLITUDE_SIMPLE (amplitude);
  return hyscan_acoustic_data_get_size_time (self->priv->data,
                                             index, n_points, time);
}

const gfloat *
hyscan_amplitude_simple_get_amplitude (HyScanAmplitude *amplitude,
                                       guint32          index,
                                       guint32         *n_points,
                                       gint64          *time,
                                       gboolean        *noise)
{
  HyScanAmplitudeSimple *self = HYSCAN_AMPLITUDE_SIMPLE (amplitude);

  if (noise != NULL)
    *noise = self->priv->noise;

  return hyscan_acoustic_data_get_amplitude (self->priv->data, index,
                                             n_points, time);
}
static void
hyscan_amplitude_simple_interface_init (HyScanAmplitudeInterface *iface)
{
  iface->get_token = hyscan_amplitude_simple_get_token;
  iface->get_position = hyscan_amplitude_simple_get_position;
  iface->get_info = hyscan_amplitude_simple_get_info;
  iface->is_writable = hyscan_amplitude_simple_is_writable;
  iface->get_mod_count = hyscan_amplitude_simple_get_mod_count;
  iface->get_range = hyscan_amplitude_simple_get_range;
  iface->find_data = hyscan_amplitude_simple_find_data;
  iface->get_size_time = hyscan_amplitude_simple_get_size_time;
  iface->get_amplitude = hyscan_amplitude_simple_get_amplitude;
}
