#include "hyscan-mark-data-geo.h"
#include "hyscan-core-schemas.h"

struct _HyScanMarkDataGeoPrivate
{
  HyScanParamList      *read_plist;
};

static void              hyscan_mark_data_geo_object_constructed   (GObject           *object);
static void              hyscan_mark_data_geo_object_finalize      (GObject           *object);
static const gchar *     hyscan_mark_data_geo_get_schema_id        (HyScanMarkData    *data,
                                                                    gpointer           mark);
static gboolean          hyscan_mark_data_geo_get_full             (HyScanMarkData    *data,
                                                                    HyScanParamList   *read_plist,
                                                                    gpointer           object);
static gboolean          hyscan_mark_data_geo_set_full             (HyScanMarkData    *data,
                                                                    HyScanParamList   *write_plist,
                                                                    gconstpointer      object);
static HyScanParamList * hyscan_mark_data_geo_get_read_plist       (HyScanMarkData    *data,
                                                                    const gchar       *schema_id);
static gpointer          hyscan_mark_data_geo_object_new           (HyScanMarkData    *data,
                                                                    const gchar       *id);
static gpointer          hyscan_mark_data_geo_object_copy          (gconstpointer      object);
static void              hyscan_mark_data_geo_object_destroy       (gpointer           object);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanMarkDataGeo, hyscan_mark_data_geo, HYSCAN_TYPE_MARK_DATA);

static void
hyscan_mark_data_geo_class_init (HyScanMarkDataGeoClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  HyScanMarkDataClass *data_class = HYSCAN_MARK_DATA_CLASS (klass);

  object_class->constructed = hyscan_mark_data_geo_object_constructed;
  object_class->finalize = hyscan_mark_data_geo_object_finalize;

  data_class->group_name = GEO_MARK_SCHEMA;
  data_class->get_schema_id = hyscan_mark_data_geo_get_schema_id;
  data_class->object_new = hyscan_mark_data_geo_object_new;
  data_class->object_copy = hyscan_mark_data_geo_object_copy;
  data_class->object_destroy = hyscan_mark_data_geo_object_destroy;
  data_class->set_full = hyscan_mark_data_geo_set_full;
  data_class->get_full = hyscan_mark_data_geo_get_full;
  data_class->get_read_plist = hyscan_mark_data_geo_get_read_plist;
}

static void
hyscan_mark_data_geo_init (HyScanMarkDataGeo *mark_data_geo)
{
  mark_data_geo->priv = hyscan_mark_data_geo_get_instance_private (mark_data_geo);
}

static void
hyscan_mark_data_geo_object_constructed (GObject *object)
{
  HyScanMarkDataGeo *data_geo = HYSCAN_MARK_DATA_GEO (object);
  HyScanMarkDataGeoPrivate *priv = data_geo->priv;

  G_OBJECT_CLASS (hyscan_mark_data_geo_parent_class)->constructed (object);

  /* Добавляем названия считываемых параметров. */
  priv->read_plist = hyscan_param_list_new ();
  hyscan_param_list_add (priv->read_plist, "/schema/id");
  hyscan_param_list_add (priv->read_plist, "/schema/version");
  hyscan_param_list_add (priv->read_plist, "/name");
  hyscan_param_list_add (priv->read_plist, "/description");
  hyscan_param_list_add (priv->read_plist, "/operator");
  hyscan_param_list_add (priv->read_plist, "/label");
  hyscan_param_list_add (priv->read_plist, "/ctime");
  hyscan_param_list_add (priv->read_plist, "/mtime");
  hyscan_param_list_add (priv->read_plist, "/width");
  hyscan_param_list_add (priv->read_plist, "/height");
  hyscan_param_list_add (priv->read_plist, "/lat");
  hyscan_param_list_add (priv->read_plist, "/lon");
}

static void
hyscan_mark_data_geo_object_finalize (GObject *object)
{
  HyScanMarkDataGeo *data_geo = HYSCAN_MARK_DATA_GEO (object);
  HyScanMarkDataGeoPrivate *priv = data_geo->priv;

  g_object_unref (priv->read_plist);
  G_OBJECT_CLASS (hyscan_mark_data_geo_parent_class)->finalize (object);
}

static const gchar *
hyscan_mark_data_geo_get_schema_id (HyScanMarkData *data,
                                    gpointer        mark)
{
  return GEO_MARK_SCHEMA;
}

/* Функция считывает содержимое объекта. */
static gboolean
hyscan_mark_data_geo_get_full (HyScanMarkData  *data,
                               HyScanParamList *read_plist,
                               gpointer         object)
{
  HyScanMark *mark = object;
  HyScanGeoGeodetic coord;
  gint64 sid, sver;

  sid = hyscan_param_list_get_integer (read_plist, "/schema/id");
  sver = hyscan_param_list_get_integer (read_plist, "/schema/version");

  if (sid != GEO_MARK_SCHEMA_ID || sver != GEO_MARK_SCHEMA_VERSION)
    return FALSE;

  if (mark == NULL)
    return TRUE;

  g_return_val_if_fail (mark->type == HYSCAN_MARK_GEO, FALSE);

  hyscan_mark_set_text   (mark,
                          hyscan_param_list_get_string (read_plist,  "/name"),
                          hyscan_param_list_get_string (read_plist,  "/description"),
                          hyscan_param_list_get_string (read_plist,  "/operator"));
  hyscan_mark_set_labels (mark,
                          hyscan_param_list_get_integer (read_plist, "/label"));
  hyscan_mark_set_ctime  (mark,
                          hyscan_param_list_get_integer (read_plist, "/ctime"));
  hyscan_mark_set_mtime  (mark,
                          hyscan_param_list_get_integer (read_plist, "/mtime"));
  hyscan_mark_set_size   (mark,
                          hyscan_param_list_get_double (read_plist, "/width"),
                          hyscan_param_list_get_double (read_plist, "/height"));

  coord.lat = hyscan_param_list_get_double (read_plist, "/lat");
  coord.lon = hyscan_param_list_get_double (read_plist, "/lon");
  hyscan_mark_geo_set_center ((HyScanMarkGeo *) mark, coord);

  return TRUE;
}

/* Функция записывает значения в существующий объект. */
static gboolean
hyscan_mark_data_geo_set_full (HyScanMarkData   *data,
                               HyScanParamList  *write_plist,
                               gconstpointer     object)
{
  const HyScanMarkAny *any = object;
  const HyScanMarkGeo *mark_geo;

  g_return_val_if_fail (any->type == HYSCAN_MARK_GEO, FALSE);
  mark_geo = (HyScanMarkGeo *) object;

  hyscan_param_list_set_string (write_plist, "/name", any->name);
  hyscan_param_list_set_string (write_plist, "/description", any->description);
  hyscan_param_list_set_integer (write_plist, "/label", any->labels);
  hyscan_param_list_set_string (write_plist, "/operator", any->operator_name);
  hyscan_param_list_set_integer (write_plist, "/ctime", any->ctime);
  hyscan_param_list_set_integer (write_plist, "/mtime", any->mtime);
  hyscan_param_list_set_double (write_plist, "/width", any->width);
  hyscan_param_list_set_double (write_plist, "/height", any->height);

  hyscan_param_list_set_double (write_plist, "/lat", mark_geo->center.lat);
  hyscan_param_list_set_double (write_plist, "/lon", mark_geo->center.lon);

  return TRUE;
}

static HyScanParamList *
hyscan_mark_data_geo_get_read_plist (HyScanMarkData *data,
                                     const gchar    *schema_id)
{
  HyScanMarkDataGeo *data_geo = HYSCAN_MARK_DATA_GEO (data);

  return g_object_ref (data_geo->priv->read_plist);
}

static gpointer
hyscan_mark_data_geo_object_new (HyScanMarkData *data,
                                 const gchar    *id)
{
  return hyscan_mark_new (HYSCAN_MARK_GEO);
}

static void
hyscan_mark_data_geo_object_destroy (gpointer object)
{
  hyscan_mark_free (object);
}

static gpointer
hyscan_mark_data_geo_object_copy (gconstpointer object)
{
  return hyscan_mark_copy ((HyScanMark *) object);
}
