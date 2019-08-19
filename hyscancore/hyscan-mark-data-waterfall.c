/* hyscan-mark-data-waterfall.c
 *
 * Copyright 2017-2019 Screen LLC, Dmitriev Alexander <m1n7@yandex.ru>
 * Copyright 2019 Screen LLC, Alexey Sakhnov <alexsakhnov@gmail.com>
 *
 * This file is part of HyScanGui library.
 *
 * HyScanGui is dual-licensed: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * HyScanGui is distributed in the hope that it will be useful,
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

/* HyScanGui имеет двойную лицензию.
 *
 * Во-первых, вы можете распространять HyScanGui на условиях Стандартной
 * Общественной Лицензии GNU версии 3, либо по любой более поздней версии
 * лицензии (по вашему выбору). Полные положения лицензии GNU приведены в
 * <http://www.gnu.org/licenses/>.
 *
 * Во-вторых, этот программный код можно использовать по коммерческой
 * лицензии. Для этого свяжитесь с ООО Экран - <info@screen-co.ru>.
 */

/**
 * SECTION: hyscan-mark-data-waterfall
 * @Short_description: Базовый класс работы с метками режима водопад
 * @Title: HyScanMarkDataWaterfall
 * @See_also: HyScanMarkData
 *
 * HyScanMarkDataWaterfall - базовый класс работы с метками режима водопад. Он представляет собой
 * обертку над базой данных, что позволяет потребителю работать не с конкретными записями в базе
 * данных, а с метками и их идентификаторами.
 *
 * Класс решает задачи добавления, удаления, модификации и получения меток.
 *
 * Класс не потокобезопасен.
 */

#include "hyscan-mark-data-waterfall.h"
#include "hyscan-core-schemas.h"

#define WATERFALL_MARK_SCHEMA_VERSION_OLD          20190100

static  HyScanSourceType type_table[] =
{
  HYSCAN_SOURCE_INVALID,
  HYSCAN_SOURCE_LOG,
  HYSCAN_SOURCE_SIDE_SCAN_STARBOARD,
  HYSCAN_SOURCE_SIDE_SCAN_STARBOARD_LOW,
  HYSCAN_SOURCE_SIDE_SCAN_STARBOARD_HI,
  HYSCAN_SOURCE_SIDE_SCAN_PORT,
  HYSCAN_SOURCE_SIDE_SCAN_PORT_LOW,
  HYSCAN_SOURCE_SIDE_SCAN_PORT_HI,
  HYSCAN_SOURCE_ECHOSOUNDER,
  HYSCAN_SOURCE_ECHOSOUNDER_LOW,
  HYSCAN_SOURCE_ECHOSOUNDER_HI,
  HYSCAN_SOURCE_BATHYMETRY_STARBOARD,
  HYSCAN_SOURCE_BATHYMETRY_PORT,
  HYSCAN_SOURCE_PROFILER,
  HYSCAN_SOURCE_PROFILER_ECHO,
  HYSCAN_SOURCE_LOOK_AROUND_STARBOARD,
  HYSCAN_SOURCE_LOOK_AROUND_PORT,
  HYSCAN_SOURCE_FORWARD_LOOK,
  HYSCAN_SOURCE_FORWARD_ECHO,
  HYSCAN_SOURCE_ENCODER,
  HYSCAN_SOURCE_SAS,
  HYSCAN_SOURCE_NMEA,
  HYSCAN_SOURCE_1PPS,
  HYSCAN_SOURCE_LAST
};

struct _HyScanMarkDataWaterfallPrivate
{
  gint64 schema_id;
  gint64 schema_version;
};

static void           hyscan_mark_data_waterfall_init_plist     (HyScanMarkData   *data,
                                                                 HyScanParamList  *plist);
static void           hyscan_mark_data_waterfall_get            (HyScanMarkData   *data,
                                                                 HyScanParamList  *read_plist,
                                                                 HyScanMark       *mark);
static gboolean       hyscan_mark_data_waterfall_get_full       (HyScanMarkData   *data,
                                                                 HyScanParamList  *read_plist,
                                                                 HyScanMark       *mark);
static void           hyscan_mark_data_waterfall_set            (HyScanMarkData   *data,
                                                                 HyScanParamList  *write_plist,
                                                                 const HyScanMark *mark);
static gboolean       hyscan_mark_data_waterfall_set_full       (HyScanMarkData   *data,
                                                                 HyScanParamList  *write_plist,
                                                                 const HyScanMark *mark);
static void
hyscan_mark_data_waterfall_init_object (HyScanMarkData *data,
                                        gint32          param_id,
                                        HyScanDB       *db);


G_DEFINE_TYPE_WITH_PRIVATE (HyScanMarkDataWaterfall, hyscan_mark_data_waterfall, HYSCAN_TYPE_MARK_DATA);

static void
hyscan_mark_data_waterfall_class_init (HyScanMarkDataWaterfallClass *klass)
{
  HyScanMarkDataClass *data_class = HYSCAN_MARK_DATA_CLASS (klass);

  data_class->mark_type = HYSCAN_MARK_WATERFALL;
  data_class->schema_id = WATERFALL_MARK_SCHEMA;
  data_class->param_sid = WATERFALL_MARK_SCHEMA_ID;
  data_class->param_sver = WATERFALL_MARK_SCHEMA_VERSION;

  data_class->init_plist = hyscan_mark_data_waterfall_init_plist;
  data_class->init_obj = hyscan_mark_data_waterfall_init_object;
  data_class->set = hyscan_mark_data_waterfall_set;
  data_class->set_full = hyscan_mark_data_waterfall_set_full;
  data_class->get = hyscan_mark_data_waterfall_get;
  data_class->get_full = hyscan_mark_data_waterfall_get_full;
}

static void
hyscan_mark_data_waterfall_init (HyScanMarkDataWaterfall *data)
{
  data->priv = hyscan_mark_data_waterfall_get_instance_private (data);
}

static void
hyscan_mark_data_waterfall_init_plist (HyScanMarkData  *data,
                                       HyScanParamList *plist)
{
  /* Добавляем названия параметров в списки. */
  hyscan_param_list_add (plist, "/track");
  hyscan_param_list_add (plist, "/source");
  hyscan_param_list_add (plist, "/index");
  hyscan_param_list_add (plist, "/count");
}

static void
hyscan_mark_data_waterfall_init_object (HyScanMarkData *data,
                                        gint32          param_id,
                                        HyScanDB       *db)
{
  HyScanMarkDataWaterfallPrivate *priv = HYSCAN_MARK_DATA_WATERFALL (data)->priv;
  HyScanParamList *list = hyscan_param_list_new ();
  hyscan_param_list_add (list, "/schema/id");
  hyscan_param_list_add (list, "/schema/version");

  /*status = */
  if (!hyscan_db_param_object_create (db, param_id, "test_object", WATERFALL_MARK_SCHEMA))
    {
      g_message ("1");
    }
  /*status = */
  if (!hyscan_db_param_get (db, param_id, "test_object", list))
    {
      g_message ("2");
    }

  priv->schema_id = hyscan_param_list_get_integer (list, "/schema/id");
  priv->schema_version = hyscan_param_list_get_integer (list, "/schema/version");

  /*status = */
  hyscan_db_param_object_remove (db, param_id, "test_object");

  g_object_unref (list);
}

/* Функция считывает содержимое объекта. */
static void
hyscan_mark_data_waterfall_get (HyScanMarkData  *data,
                                HyScanParamList *read_plist,
                                HyScanMark      *mark)
{
  HyScanMarkWaterfall *mark_wf;

  g_return_if_fail (mark->type == HYSCAN_MARK_WATERFALL);
  mark_wf = (HyScanMarkWaterfall *) mark;

  hyscan_mark_waterfall_set_track  (mark_wf,
                                    hyscan_param_list_get_string (read_plist,  "/track"));

  hyscan_mark_waterfall_set_center (mark_wf,
                                    hyscan_param_list_get_string (read_plist, "/source"),
                                    hyscan_param_list_get_integer (read_plist, "/index"),
                                    hyscan_param_list_get_integer (read_plist, "/count"));
}

/* Функция считывает содержимое объекта. */
static gboolean
hyscan_mark_data_waterfall_get_full (HyScanMarkData  *data,
                                     HyScanParamList *read_plist,
                                     HyScanMark      *mark)
{
  HyScanMarkDataWaterfallPrivate *priv = HYSCAN_MARK_DATA_WATERFALL (data)->priv;
  HyScanMarkWaterfall *mark_wf = (HyScanMarkWaterfall *) mark;
  gint64 sid, sver;


  sid = hyscan_param_list_get_integer (read_plist, "/schema/id");
  sver = hyscan_param_list_get_integer (read_plist, "/schema/version");

  if (sid != priv->schema_id || sver != priv->schema_version)
    return FALSE;

  if (mark != NULL)
    {
      g_return_val_if_fail (mark->type == HYSCAN_MARK_WATERFALL, FALSE);

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

      hyscan_mark_waterfall_set_track  (mark_wf,
                                        hyscan_param_list_get_string (read_plist,  "/track"));
      /* new */
      if (sver == WATERFALL_MARK_SCHEMA_VERSION)
        {
          hyscan_mark_waterfall_set_center (mark_wf,
                                            hyscan_param_list_get_string (read_plist, "/source"),
                                            hyscan_param_list_get_integer (read_plist, "/index"),
                                            hyscan_param_list_get_integer (read_plist, "/count"));
        }
      else if (sver == WATERFALL_MARK_SCHEMA_VERSION_OLD)
        {
          hyscan_mark_waterfall_set_center (mark_wf,
                                            hyscan_source_get_id_by_type (type_table[
                                              hyscan_param_list_get_integer (read_plist, "/source")
                                              ]),
                                            hyscan_param_list_get_integer (read_plist, "/index"),
                                            hyscan_param_list_get_integer (read_plist, "/count"));
        }
    }

  return TRUE;
}

/* Функция записывает значения в существующий объект. */
static void
hyscan_mark_data_waterfall_set (HyScanMarkData   *data,
                                HyScanParamList  *write_plist,
                                const HyScanMark *mark)
{
  HyScanMarkWaterfall *mark_wf;

  g_return_if_fail (mark->type == HYSCAN_MARK_WATERFALL);
  mark_wf = (HyScanMarkWaterfall *) mark;

  hyscan_param_list_set_string (write_plist, "/track", mark_wf->track);
  hyscan_param_list_set_string (write_plist, "/source", mark_wf->source);
  hyscan_param_list_set_integer (write_plist, "/index", mark_wf->index);
  hyscan_param_list_set_integer (write_plist, "/count", mark_wf->count);
}

/* Функция записывает значения в существующий объект. */
static gboolean
hyscan_mark_data_waterfall_set_full (HyScanMarkData   *data,
                                     HyScanParamList  *write_plist,
                                     const HyScanMark *mark)
{
  HyScanMarkDataWaterfallPrivate *priv = HYSCAN_MARK_DATA_WATERFALL (data)->priv;
  HyScanMarkAny *any = (HyScanMarkAny *) mark;
  const HyScanMarkWaterfall *mark_wf = (const HyScanMarkWaterfall*)(&mark->waterfall);

  g_return_val_if_fail (mark->type == HYSCAN_MARK_WATERFALL, FALSE);

  hyscan_param_list_set_string (write_plist, "/name", any->name);
  hyscan_param_list_set_string (write_plist, "/description", any->description);
  hyscan_param_list_set_integer (write_plist, "/label", any->labels);
  hyscan_param_list_set_string (write_plist, "/operator", any->operator_name);
  hyscan_param_list_set_integer (write_plist, "/ctime", any->ctime);
  hyscan_param_list_set_integer (write_plist, "/mtime", any->mtime);
  hyscan_param_list_set_double (write_plist, "/width", any->width);
  hyscan_param_list_set_double (write_plist, "/height", any->height);

  hyscan_param_list_set_string (write_plist, "/track", mark_wf->track);
  hyscan_param_list_set_integer (write_plist, "/index", mark_wf->index);
  hyscan_param_list_set_integer (write_plist, "/count", mark_wf->count);

  if (priv->schema_version == WATERFALL_MARK_SCHEMA_VERSION)
    {
      hyscan_param_list_set_string (write_plist, "/source", mark_wf->source);
    }
  else if (priv->schema_version == WATERFALL_MARK_SCHEMA_VERSION_OLD)
    {
      guint i;
      for (i = 0; i < G_N_ELEMENTS (type_table); ++i)
        {
          if (0 != g_strcmp0 (mark_wf->source, hyscan_source_get_id_by_type (type_table[i])))
            continue;

          hyscan_param_list_set_integer (write_plist, "/source", i);
        }
    }
  else
    {
      return FALSE;
    }

  return TRUE;
}

HyScanMarkData *
hyscan_mark_data_waterfall_new (HyScanDB    * db,
                                const gchar * project)
{
  HyScanMarkData * data;

  data = g_object_new (HYSCAN_TYPE_MARK_DATA_WATERFALL,
                       "db", db,
                       "project", project,
                       NULL);

  if (!hyscan_mark_data_is_ready (data))
    g_clear_object (&data);

  return data;
}
