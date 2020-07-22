/* hyscan-object-data-wfmark.c
 *
 * Copyright 2017-2019 Screen LLC, Dmitriev Alexander <m1n7@yandex.ru>
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
 * SECTION: hyscan-object-data-wfmark
 * @Short_description: Базовый класс работы с метками режима водопад
 * @Title: HyScanObjectDataWfmark
 * @See_also: HyScanObjectData
 *
 * HyScanObjectDataWfmark - базовый класс работы с метками режима водопад.
 * Принципы работы класса подробно описаны в #HyScanObjectData.
 */

#include "hyscan-object-data-wfmark.h"
#include "hyscan-core-schemas.h"

struct _HyScanObjectDataWfmarkPrivate
{
  HyScanParamList        *read_plist;        /* Список параметров для чтения из БД. */
};

static void              hyscan_object_data_wfmark_object_constructed (GObject            *object);
static void              hyscan_object_data_wfmark_object_finalize    (GObject            *object);
static HyScanObject *    hyscan_object_data_wfmark_get_full           (HyScanObjectData   *data,
                                                                       HyScanParamList    *read_plist);
static gboolean          hyscan_object_data_wfmark_set_full           (HyScanObjectData   *data,
                                                                       HyScanParamList    *write_plist,
                                                                       const HyScanObject *object);
static HyScanParamList * hyscan_object_data_wfmark_get_read_plist     (HyScanObjectData   *data,
                                                                       const gchar        *schema_id);
static const gchar *     hyscan_object_data_wfmark_get_schema_id      (HyScanObjectData   *data,
                                                                       const HyScanObject *object);


G_DEFINE_TYPE_WITH_PRIVATE (HyScanObjectDataWfmark, hyscan_object_data_wfmark, HYSCAN_TYPE_OBJECT_DATA);

static void
hyscan_object_data_wfmark_class_init (HyScanObjectDataWfmarkClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  HyScanObjectDataClass *data_class = HYSCAN_OBJECT_DATA_CLASS (klass);

  object_class->constructed = hyscan_object_data_wfmark_object_constructed;
  object_class->finalize = hyscan_object_data_wfmark_object_finalize;

  data_class->group_name = WATERFALL_MARK_SCHEMA;
  data_class->get_schema_id = hyscan_object_data_wfmark_get_schema_id;
  data_class->set_full = hyscan_object_data_wfmark_set_full;
  data_class->get_full = hyscan_object_data_wfmark_get_full;
  data_class->get_read_plist = hyscan_object_data_wfmark_get_read_plist;
}

static void
hyscan_object_data_wfmark_init (HyScanObjectDataWfmark *data)
{
  data->priv = hyscan_object_data_wfmark_get_instance_private (data);
}

static void
hyscan_object_data_wfmark_object_constructed (GObject *object)
{
  HyScanObjectDataWfmarkPrivate *priv = HYSCAN_OBJECT_DATA_WFMARK (object)->priv;

  G_OBJECT_CLASS (hyscan_object_data_wfmark_parent_class)->constructed (object);

  /* Добавляем названия считываемых параметров. */
  priv->read_plist = hyscan_param_list_new ();
  hyscan_param_list_add (priv->read_plist, "/schema/id");
  hyscan_param_list_add (priv->read_plist, "/schema/version");
  hyscan_param_list_add (priv->read_plist, "/name");
  hyscan_param_list_add (priv->read_plist, "/description");
  hyscan_param_list_add (priv->read_plist, "/operator");
  hyscan_param_list_add (priv->read_plist, "/labels");
  hyscan_param_list_add (priv->read_plist, "/ctime");
  hyscan_param_list_add (priv->read_plist, "/mtime");
  hyscan_param_list_add (priv->read_plist, "/width");
  hyscan_param_list_add (priv->read_plist, "/height");
  hyscan_param_list_add (priv->read_plist, "/track");
  hyscan_param_list_add (priv->read_plist, "/source");
  hyscan_param_list_add (priv->read_plist, "/index");
  hyscan_param_list_add (priv->read_plist, "/count");
}

static void
hyscan_object_data_wfmark_object_finalize (GObject *object)
{
  HyScanObjectDataWfmarkPrivate *priv = HYSCAN_OBJECT_DATA_WFMARK (object)->priv;

  g_object_unref (priv->read_plist);
  G_OBJECT_CLASS (hyscan_object_data_wfmark_parent_class)->finalize (object);
}

/* Функция получает список параметров для чтения объекта. */
static HyScanParamList *
hyscan_object_data_wfmark_get_read_plist (HyScanObjectData *data,
                                          const gchar      *id)
{
  HyScanObjectDataWfmarkPrivate *priv = HYSCAN_OBJECT_DATA_WFMARK (data)->priv;

  return g_object_ref (priv->read_plist);
}

/* Функция получает ид схемы данных объекта. */
static const gchar *
hyscan_object_data_wfmark_get_schema_id (HyScanObjectData   *data,
                                         const HyScanObject *object)
{
  return WATERFALL_MARK_SCHEMA;
}

/* Функция считывает содержимое объекта. */
static HyScanObject *
hyscan_object_data_wfmark_get_full (HyScanObjectData *data,
                                    HyScanParamList  *read_plist)
{
  HyScanMark *mark;
  HyScanMarkWaterfall *mark_wf;
  gint64 sid, sver;

  sid = hyscan_param_list_get_integer (read_plist, "/schema/id");
  sver = hyscan_param_list_get_integer (read_plist, "/schema/version");

  if (sid != WATERFALL_MARK_SCHEMA_ID || sver != WATERFALL_MARK_SCHEMA_VERSION)
    return NULL;

  mark_wf = hyscan_mark_waterfall_new ();
  mark = (HyScanMark *) mark_wf;

  hyscan_mark_set_text   (mark,
                          hyscan_param_list_get_string (read_plist,  "/name"),
                          hyscan_param_list_get_string (read_plist,  "/description"),
                          hyscan_param_list_get_string (read_plist,  "/operator"));
  hyscan_mark_set_labels (mark,
                          hyscan_param_list_get_integer (read_plist, "/labels"));
  hyscan_mark_set_ctime  (mark,
                          hyscan_param_list_get_integer (read_plist, "/ctime"));
  hyscan_mark_set_mtime  (mark,
                          hyscan_param_list_get_integer (read_plist, "/mtime"));
  hyscan_mark_set_size   (mark,
                          hyscan_param_list_get_double (read_plist, "/width"),
                          hyscan_param_list_get_double (read_plist, "/height"));

  hyscan_mark_waterfall_set_track  (mark_wf,
                                    hyscan_param_list_get_string (read_plist,  "/track"));
  hyscan_mark_waterfall_set_center (mark_wf,
                                    hyscan_param_list_get_string (read_plist, "/source"),
                                    hyscan_param_list_get_integer (read_plist, "/index"),
                                    hyscan_param_list_get_integer (read_plist, "/count"));

  return (HyScanObject *) mark_wf;
}

/* Функция записывает значения в существующий объект. */
static gboolean
hyscan_object_data_wfmark_set_full (HyScanObjectData   *data,
                                    HyScanParamList    *write_plist,
                                    const HyScanObject *object)
{
  HyScanMark *any = (HyScanMark *) object;
  const HyScanMarkWaterfall *mark_wf = (const HyScanMarkWaterfall*)(object);

  g_return_val_if_fail (object->type == HYSCAN_TYPE_MARK_WATERFALL, FALSE);

  hyscan_param_list_set_string (write_plist, "/name", any->name);
  hyscan_param_list_set_string (write_plist, "/description", any->description);
  hyscan_param_list_set_integer (write_plist, "/labels", any->labels);
  hyscan_param_list_set_string (write_plist, "/operator", any->operator_name);
  hyscan_param_list_set_integer (write_plist, "/ctime", any->ctime);
  hyscan_param_list_set_integer (write_plist, "/mtime", any->mtime);
  hyscan_param_list_set_double (write_plist, "/width", any->width);
  hyscan_param_list_set_double (write_plist, "/height", any->height);

  hyscan_param_list_set_string (write_plist, "/track", mark_wf->track);
  hyscan_param_list_set_integer (write_plist, "/index", mark_wf->index);
  hyscan_param_list_set_integer (write_plist, "/count", mark_wf->count);

  hyscan_param_list_set_string (write_plist, "/source", mark_wf->source);

  return TRUE;
}

/**
 * hyscan_object_data_wfmark_new:
 * @db: указатель на #HyScanDB
 * @project: имя проекта
 *
 * Создаёт новый объект для работы с акустическими метками. Для удаления g_object_unref().
 */
HyScanObjectData *
hyscan_object_data_wfmark_new (HyScanDB    *db,
                               const gchar *project)
{
  return hyscan_object_data_new (HYSCAN_TYPE_OBJECT_DATA_WFMARK, db, project);
}
