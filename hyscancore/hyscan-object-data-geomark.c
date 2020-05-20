/* hyscan-object-data-geomark.c
 *
 * Copyright 2019 Screen LLC, Alexey Sakhnov <alexsakhnov@gmail.com>
 *
 * This file is part of HyScanCore library.
 *
 * HyScanCore is dual-licensed: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * HyScanCore is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Alternatively, you can license this code under a commercial license.
 * Contact the Screen LLC in this case - <info@screen-co.ru>.
 */

/* HyScanCore имеет двойную лицензию.
 *
 * Во-первых, вы можете распространять HyScanCore на условиях Стандартной
 * Общественной Лицензии GNU версии 3, либо по любой более поздней версии
 * лицензии (по вашему выбору). Полные положения лицензии GNU приведены в
 * <http://www.gnu.org/licenses/>.
 *
 * Во-вторых, этот программный код можно использовать по коммерческой
 * лицензии. Для этого свяжитесь с ООО Экран - <info@screen-co.ru>.
 */

/**
 * SECTION: hyscan-object-data-geomark
 * @Short_description: Базовый класс работы с географическими метками
 * @Title: HyScanObjectDataGeomark
 * @See_also: HyScanObjectData
 *
 * HyScanObjectDataWfmark - базовый класс работы с географическими метками #HyScanMarkGeo.
 * Принципы работы класса подробно описаны в #HyScanObjectData.
 *
 */

#include "hyscan-object-data-geomark.h"
#include "hyscan-core-schemas.h"

struct _HyScanObjectDataGeomarkPrivate
{
  HyScanParamList      *read_plist;         /* Список параметров для чтения. */
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
  HyScanGeoPoint coord;
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

  g_return_val_if_fail (object->type == HYSCAN_TYPE_MARK_GEO, FALSE);
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

/**
 * hyscan_object_data_geomark_new:
 * @db: указатель на #HyScanDB
 * @project: имя проекта
 *
 * Создаёт новый объект для работы с географическими метками. Для удаления g_object_unref().
 */
HyScanObjectData *
hyscan_object_data_geomark_new (HyScanDB    *db,
                                const gchar *project)
{
  return hyscan_object_data_new (HYSCAN_TYPE_OBJECT_DATA_GEOMARK, db, project);
}
