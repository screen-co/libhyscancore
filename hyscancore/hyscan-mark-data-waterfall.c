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

static void           hyscan_mark_data_waterfall_init_plist              (HyScanMarkData                  *data,
                                                                          HyScanParamList                 *plist);
static void           hyscan_mark_data_waterfall_get_internal            (HyScanMarkData                  *data,
                                                                          HyScanParamList                 *read_plist,
                                                                          HyScanMark                      *mark);
static void           hyscan_mark_data_waterfall_set_internal            (HyScanMarkData                  *data,
                                                                          HyScanParamList                 *write_plist,
                                                                          const HyScanMark                *mark);

G_DEFINE_TYPE (HyScanMarkDataWaterfall, hyscan_mark_data_waterfall, HYSCAN_TYPE_MARK_DATA);

static void
hyscan_mark_data_waterfall_class_init (HyScanMarkDataWaterfallClass *klass)
{
  HyScanMarkDataClass *data_class = HYSCAN_MARK_DATA_CLASS (klass);

  data_class->mark_type = HYSCAN_MARK_WATERFALL;
  data_class->schema_id = WATERFALL_MARK_SCHEMA;
  data_class->param_sid = WATERFALL_MARK_SCHEMA_ID;
  data_class->param_sver = WATERFALL_MARK_SCHEMA_VERSION;

  data_class->init_plist = hyscan_mark_data_waterfall_init_plist;
  data_class->set = hyscan_mark_data_waterfall_set_internal;
  data_class->get = hyscan_mark_data_waterfall_get_internal;
}

static void
hyscan_mark_data_waterfall_init (HyScanMarkDataWaterfall *data)
{
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

/* Функция считывает содержимое объекта. */
static void
hyscan_mark_data_waterfall_get_internal (HyScanMarkData  *data,
                                         HyScanParamList *read_plist,
                                         HyScanMark      *mark)
{
  HyScanMarkWaterfall *mark_wf;

  g_return_if_fail (mark->type == HYSCAN_MARK_WATERFALL);
  mark_wf = (HyScanMarkWaterfall *) mark;

  hyscan_mark_waterfall_set_track  (mark_wf,
                                    hyscan_param_list_get_string (read_plist,  "/track"));

  hyscan_mark_waterfall_set_center (mark_wf,
                                    hyscan_param_list_get_integer (read_plist, "/source"),
                                    hyscan_param_list_get_integer (read_plist, "/index"),
                                    hyscan_param_list_get_integer (read_plist, "/count"));
}

/* Функция записывает значения в существующий объект. */
static void
hyscan_mark_data_waterfall_set_internal (HyScanMarkData   *data,
                                         HyScanParamList  *write_plist,
                                         const HyScanMark *mark)
{
  HyScanMarkWaterfall *mark_wf;

  g_return_if_fail (mark->type == HYSCAN_MARK_WATERFALL);
  mark_wf = (HyScanMarkWaterfall *) mark;

  hyscan_param_list_set_string (write_plist, "/track", mark_wf->track);
  hyscan_param_list_set_integer (write_plist, "/source", mark_wf->source);
  hyscan_param_list_set_integer (write_plist, "/index", mark_wf->index);
  hyscan_param_list_set_integer (write_plist, "/count", mark_wf->count);
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
