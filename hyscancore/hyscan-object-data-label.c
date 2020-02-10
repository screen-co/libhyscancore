/*
 * hyscan-object-data-label.c
 *
 *  Created on: 13 янв. 2020 г.
 *      Author: Andrey Zakharov <zaharov@screen-co.ru>
 */
#include "hyscan-core-schemas.h"
#include "hyscan-object-data-label.h"

struct _HyScanObjectDataLabelPrivate
{
  HyScanParamList      *read_plist;
};

static void              hyscan_object_data_label_object_constructed   (GObject             *object);
static void              hyscan_object_data_label_object_finalize      (GObject             *object);
static const gchar*      hyscan_object_data_label_get_schema_id        (HyScanObjectData    *data,
                                                                        const HyScanObject  *object);
static HyScanObject*     hyscan_object_data_label_get_full             (HyScanObjectData    *data,
                                                                        HyScanParamList     *read_plist);
static gboolean          hyscan_object_data_label_set_full             (HyScanObjectData    *data,
                                                                        HyScanParamList     *write_plist,
                                                                        const HyScanObject  *object);
static HyScanParamList*  hyscan_object_data_label_get_read_plist       (HyScanObjectData    *data,
                                                                        const gchar         *schema_id);
static HyScanObject*     hyscan_object_data_label_object_copy          (const HyScanObject  *object);
static void              hyscan_object_data_label_object_destroy       (HyScanObject        *object);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanObjectDataLabel, hyscan_object_data_label, HYSCAN_TYPE_OBJECT_DATA);

static void
hyscan_object_data_label_class_init (HyScanObjectDataLabelClass *klass)
{
  GObjectClass          *object_class = G_OBJECT_CLASS (klass);
  HyScanObjectDataClass *data_class   = HYSCAN_OBJECT_DATA_CLASS (klass);

  object_class->constructed = hyscan_object_data_label_object_constructed;
  object_class->finalize    = hyscan_object_data_label_object_finalize;

  data_class->group_name     = LABEL_SCHEMA;
  data_class->get_schema_id  = hyscan_object_data_label_get_schema_id;
  data_class->object_copy    = hyscan_object_data_label_object_copy;
  data_class->object_destroy = hyscan_object_data_label_object_destroy;
  data_class->set_full       = hyscan_object_data_label_set_full;
  data_class->get_full       = hyscan_object_data_label_get_full;
  data_class->get_read_plist = hyscan_object_data_label_get_read_plist;
}

static void
hyscan_object_data_label_init (HyScanObjectDataLabel *object_data_label)
{
  object_data_label->priv = hyscan_object_data_label_get_instance_private (object_data_label);
}

static void
hyscan_object_data_label_object_constructed (GObject *object)
{
  HyScanObjectDataLabel        *data_label = HYSCAN_OBJECT_DATA_LABEL (object);
  HyScanObjectDataLabelPrivate *priv       = data_label->priv;

  G_OBJECT_CLASS (hyscan_object_data_label_parent_class)->constructed (object);

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
}

static void
hyscan_object_data_label_object_finalize (GObject *object)
{
  HyScanObjectDataLabel        *data_label = HYSCAN_OBJECT_DATA_LABEL (object);
  HyScanObjectDataLabelPrivate *priv       = data_label->priv;

  g_object_unref (priv->read_plist);
  G_OBJECT_CLASS (hyscan_object_data_label_parent_class)->finalize (object);
}

static const gchar *
hyscan_object_data_label_get_schema_id (HyScanObjectData   *data,
                                        const HyScanObject *object)
{
  return LABEL_SCHEMA;
}

/* Функция считывает содержимое объекта. */
static HyScanObject*
hyscan_object_data_label_get_full (HyScanObjectData *data,
                                   HyScanParamList  *read_plist)
{
  HyScanLabel *label;
  gint64       sid,
               sver;

  sid  = hyscan_param_list_get_integer (read_plist, "/schema/id");
  sver = hyscan_param_list_get_integer (read_plist, "/schema/version");

  if (sid != LABEL_SCHEMA_ID || sver != LABEL_SCHEMA_VERSION)
    return FALSE;

  label = (HyScanLabel*) hyscan_label_new ();

  hyscan_label_set_text  (label,
                          hyscan_param_list_get_string (read_plist,  "/name"),
                          hyscan_param_list_get_string (read_plist,  "/description"),
                          hyscan_param_list_get_string (read_plist,  "/operator"));
  hyscan_label_set_label (label,
                          hyscan_param_list_get_integer (read_plist, "/label"));
  hyscan_label_set_ctime (label,
                          hyscan_param_list_get_integer (read_plist, "/ctime"));
  hyscan_label_set_mtime (label,
                          hyscan_param_list_get_integer (read_plist, "/mtime"));
  return (HyScanObject*) label;
}

/* Функция записывает значения в существующий объект. */
static gboolean
hyscan_object_data_label_set_full (HyScanObjectData   *data,
                                   HyScanParamList    *write_plist,
                                   const HyScanObject *object)
{
  const HyScanLabel *label;

  g_return_val_if_fail (object->type == HYSCAN_LABEL, FALSE);
  label = (HyScanLabel *) object;

  hyscan_param_list_set_string (write_plist, "/name", label->name);
  hyscan_param_list_set_string (write_plist, "/description", label->description);
  hyscan_param_list_set_string (write_plist, "/operator", label->operator_name);
  hyscan_param_list_set_integer (write_plist, "/label", label->label);
  hyscan_param_list_set_integer (write_plist, "/ctime", label->ctime);
  hyscan_param_list_set_integer (write_plist, "/mtime", label->mtime);

  return TRUE;
}

static HyScanParamList*
hyscan_object_data_label_get_read_plist (HyScanObjectData *data,
                                         const gchar      *schema_id)
{
  HyScanObjectDataLabel *data_label = HYSCAN_OBJECT_DATA_LABEL (data);

  return g_object_ref (data_label->priv->read_plist);
}

static void
hyscan_object_data_label_object_destroy (HyScanObject *object)
{
  hyscan_label_free ( (HyScanLabel*) object);
}

static HyScanObject*
hyscan_object_data_label_object_copy (const HyScanObject *object)
{
  return (HyScanObject*) hyscan_label_copy ( (const HyScanLabel*) object);
}
