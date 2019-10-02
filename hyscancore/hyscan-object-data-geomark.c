#include "hyscan-object-data-geomark.h"
#include "hyscan-core-schemas.h"
#include "hyscan-mark.h"

struct _HyScanObjectDataGeomarkPrivate
{
  HyScanParamList      *read_plist;
};

static void              hyscan_object_data_geomark_object_constructed   (GObject             *object);
static void              hyscan_object_data_geomark_object_finalize      (GObject             *object);
static const gchar *     hyscan_object_data_geomark_get_schema_id        (HyScanObjectData    *data,
                                                                          const HyScanObject  *object);
static HyScanObject *    hyscan_object_data_geomark_get_full             (HyScanObjectData    *data,
                                                                          HyScanParamList     *read_plist);
static gboolean          hyscan_object_data_geomark_set_full             (HyScanObjectData    *data,
                                                                          HyScanParamList     *write_plist,
                                                                          const HyScanObject  *object);
static HyScanParamList * hyscan_object_data_geomark_get_read_plist       (HyScanObjectData    *data,
                                                                          const gchar         *schema_id);
static HyScanObject *    hyscan_object_data_geomark_object_copy          (const HyScanObject  *object);
static void              hyscan_object_data_geomark_object_destroy       (HyScanObject        *object);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanObjectDataGeomark, hyscan_object_data_geomark, HYSCAN_TYPE_OBJECT_DATA);

static void
hyscan_object_data_geomark_class_init (HyScanObjectDataGeomarkClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  HyScanObjectDataClass *data_class = HYSCAN_OBJECT_DATA_CLASS (klass);

  object_class->constructed = hyscan_object_data_geomark_object_constructed;
  object_class->finalize = hyscan_object_data_geomark_object_finalize;

  data_class->group_name = GEO_MARK_SCHEMA;
  data_class->get_schema_id = hyscan_object_data_geomark_get_schema_id;
  data_class->object_copy = hyscan_object_data_geomark_object_copy;
  data_class->object_destroy = hyscan_object_data_geomark_object_destroy;
  data_class->set_full = hyscan_object_data_geomark_set_full;
  data_class->get_full = hyscan_object_data_geomark_get_full;
  data_class->get_read_plist = hyscan_object_data_geomark_get_read_plist;
}

static void
hyscan_object_data_geomark_init (HyScanObjectDataGeomark *object_data_geomark)
{
  object_data_geomark->priv = hyscan_object_data_geomark_get_instance_private (object_data_geomark);
}

static void
hyscan_object_data_geomark_object_constructed (GObject *object)
{
  HyScanObjectDataGeomark *data_geo = HYSCAN_OBJECT_DATA_GEOMARK (object);
  HyScanObjectDataGeomarkPrivate *priv = data_geo->priv;

  G_OBJECT_CLASS (hyscan_object_data_geomark_parent_class)->constructed (object);

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
hyscan_object_data_geomark_object_finalize (GObject *object)
{
  HyScanObjectDataGeomark *data_geo = HYSCAN_OBJECT_DATA_GEOMARK (object);
  HyScanObjectDataGeomarkPrivate *priv = data_geo->priv;

  g_object_unref (priv->read_plist);
  G_OBJECT_CLASS (hyscan_object_data_geomark_parent_class)->finalize (object);
}

static const gchar *
hyscan_object_data_geomark_get_schema_id (HyScanObjectData   *data,
                                          const HyScanObject *object)
{
  return GEO_MARK_SCHEMA;
}

/* Функция считывает содержимое объекта. */
static HyScanObject *
hyscan_object_data_geomark_get_full (HyScanObjectData *data,
                                     HyScanParamList  *read_plist)
{
  HyScanMark *mark;
  HyScanGeoGeodetic coord;
  gint64 sid, sver;

  sid = hyscan_param_list_get_integer (read_plist, "/schema/id");
  sver = hyscan_param_list_get_integer (read_plist, "/schema/version");

  if (sid != GEO_MARK_SCHEMA_ID || sver != GEO_MARK_SCHEMA_VERSION)
    return FALSE;

  mark = (HyScanMark *) hyscan_mark_geo_new ();

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

  return (HyScanObject *) mark;
}

/* Функция записывает значения в существующий объект. */
static gboolean
hyscan_object_data_geomark_set_full (HyScanObjectData   *data,
                                     HyScanParamList    *write_plist,
                                     const HyScanObject *object)
{
  const HyScanMark *any;
  const HyScanMarkGeo *mark_geo;

  g_return_val_if_fail (object->type == HYSCAN_MARK_GEO, FALSE);
  mark_geo = (HyScanMarkGeo *) object;
  any = (HyScanMark *) object;

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
hyscan_object_data_geomark_get_read_plist (HyScanObjectData *data,
                                           const gchar      *schema_id)
{
  HyScanObjectDataGeomark *data_geo = HYSCAN_OBJECT_DATA_GEOMARK (data);

  return g_object_ref (data_geo->priv->read_plist);
}

static void
hyscan_object_data_geomark_object_destroy (HyScanObject *object)
{
  hyscan_mark_geo_free ((HyScanMarkGeo *) object);
}

static HyScanObject *
hyscan_object_data_geomark_object_copy (const HyScanObject *object)
{
  return (HyScanObject *) hyscan_mark_geo_copy ((const HyScanMarkGeo *) object);
}
