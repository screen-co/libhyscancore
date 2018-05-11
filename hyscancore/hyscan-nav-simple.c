#include "hyscan-nav-simple.h"

enum
{
  PROP_O,
  PROP_NAV_DATA
};

struct _HyScanNavSimplePrivate
{
  HyScanNavData *ndata;

  gchar         *token;
};

static void    hyscan_nav_simple_interface_init           (HyScanNavDataInterface *iface);
static void    hyscan_nav_simple_set_property             (GObject                *object,
                                                           guint                   prop_id,
                                                           const GValue           *value,
                                                           GParamSpec             *pspec);
static void    hyscan_nav_simple_object_constructed       (GObject                *object);
static void    hyscan_nav_simple_object_finalize          (GObject                *object);

G_DEFINE_TYPE_WITH_CODE (HyScanNavSimple, hyscan_nav_simple, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (HyScanNavSimple)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_NAV_DATA, hyscan_nav_simple_interface_init));

static void
hyscan_nav_simple_class_init (HyScanNavSimpleClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_nav_simple_set_property;

  object_class->constructed = hyscan_nav_simple_object_constructed;
  object_class->finalize = hyscan_nav_simple_object_finalize;

  g_object_class_install_property (object_class, PROP_A,
    g_param_spec_object ("nav", "NavData", "Sublayer NavData", HYSCAN_TYPE_NAV_DATA,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_nav_simple_init (HyScanNavSimple *nav_simple)
{
  nav_simple->priv = hyscan_nav_simple_get_instance_private (nav_simple);
}

static void
hyscan_nav_simple_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  HyScanNavSimple *nav_simple = HYSCAN_NAV_SIMPLE (object);
  HyScanNavSimplePrivate *priv = nav_simple->priv;

  if (prop_id == PROP_NAV_DATA)
    priv->ndata = g_value_dup_object (value);
  else
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
}

static void
hyscan_nav_simple_object_constructed (GObject *object)
{
  const gchar * subtoken;
  HyScanNavSimple *nav_simple = HYSCAN_NAV_SIMPLE (object);
  HyScanNavSimplePrivate *priv = nav_simple->priv;

  subtoken = hyscan_nav_data_get_token (priv->ndata);
  priv->token = g_strdup_printf ("Simple.%s", subtoken);
}

static void
hyscan_nav_simple_object_finalize (GObject *object)
{
  HyScanNavSimple *nav_simple = HYSCAN_NAV_SIMPLE (object);
  HyScanNavSimplePrivate *priv = nav_simple->priv;

  g_clear_object (&priv->ndata);
  g_free (priv->token);
  G_OBJECT_CLASS (hyscan_nav_simple_parent_class)->finalize (object);
}

static void
hyscan_nav_simple_set_cache (HyScanNavData *ndata,
                             HyScanCache   *cache)
{
  HyScanNavSimple *simple = HYSCAN_NAV_SIMPLE (ndata);
  hyscan_nmea_data_set_cache (simple->priv->ndata);
}

static gboolean
hyscan_nav_simple_get (HyScanNavData *ndata,
                       guint32        index,
                       gint64        *time,
                       gdouble       *value)
{
  HyScanNavSimple *simple = HYSCAN_NAV_SIMPLE (ndata);
}

static HyScanDBFindStatus
hyscan_nav_simple_find_data (HyScanNavData *ndata,
                             gint64         time,
                             guint32       *lindex,
                             guint32       *rindex,
                             gint64        *ltime,
                             gint64        *rtime)
{
  HyScanNavSimple *simple = HYSCAN_NAV_SIMPLE (ndata);

  //!! hyscan_nmea_data_find_data (time, lindex, rindex, ltime, rtime);
}

static gboolean
hyscan_nav_simple_get_range (HyScanNavData *ndata,
                             guint32       *first,
                             guint32       *last)
{
  HyScanNavSimple *simple = HYSCAN_NAV_SIMPLE (ndata);
  //!! hyscan_nmea_data_get_range (simple->priv->ndata, first, last);
}


static HyScanAntennaPosition
hyscan_nav_simple_get_position (HyScanNavData *ndata)
{
  HyScanNavSimple *simple = HYSCAN_NAV_SIMPLE (ndata);
  hyscan_nmea_data_get_position (simple->priv->ndata);
}

static gboolean
hyscan_nav_simple_is_writable (HyScanNavData *ndata)
{
  HyScanNavSimple *simple = HYSCAN_NAV_SIMPLE (ndata);
  hyscan_nmea_data_is_writable (simple->priv->ndata);
}

static const gchar *
hyscan_nav_simple_get_token (HyScanNavData *ndata)
{
  HyScanNavSimple *simple = HYSCAN_NAV_SIMPLE (ndata);
  return simple->priv->token;
}

static guint32
hyscan_nav_simple_get_mod_count (HyScanNavData *ndata)
{
  HyScanNavSimple *simple = HYSCAN_NAV_SIMPLE (ndata);
  return hyscan_nmea_data_get_mod_count (simple->priv->ndata);
}


static void
hyscan_nav_simple_interface_init (HyScanNavDataInterface *iface)
{
  iface->set_cache = hyscan_nav_simple_set_cache;
  iface->get = hyscan_nav_simple_get;
  iface->find_data = hyscan_nav_simple_find_data;
  iface->get_range = hyscan_nav_simple_get_range;
  iface->get_position = hyscan_nav_simple_get_position;
  iface->is_writable = hyscan_nav_simple_is_writable;
  iface->get_token = hyscan_nav_simple_get_token;
  iface->get_mod_count = hyscan_nav_simple_get_mod_count;
}
