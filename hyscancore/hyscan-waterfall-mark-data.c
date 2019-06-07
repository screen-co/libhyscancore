/*
 * \file hyscan-waterfall-mark-data.c
 *
 * \brief Исходный файл класса работы с метками водопада
 * \author Dmitriev Alexander (m1n7@yandex.ru)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-waterfall-mark-data.h"
#include "hyscan-core-schemas.h"
#include <string.h>

static const gchar *  hyscan_waterfall_mark_data_get_schema              (HyScanMarkData                  *data,
                                                                          gint64                          *id,
                                                                          gint64                          *version);
static void           hyscan_waterfall_mark_data_init_plist              (HyScanMarkData                  *data,
                                                                          HyScanParamList                 *plist);
static void           hyscan_waterfall_mark_data_get_internal            (HyScanMarkData                  *data,
                                                                          HyScanParamList                 *read_plist,
                                                                          HyScanMark                      *mark);
static void           hyscan_waterfall_mark_data_set_internal            (HyScanMarkData                  *data,
                                                                          HyScanParamList                 *write_plist,
                                                                          const HyScanMark                *mark);

G_DEFINE_TYPE (HyScanWaterfallMarkData, hyscan_waterfall_mark_data, HYSCAN_TYPE_MARK_DATA);

static void
hyscan_waterfall_mark_data_class_init (HyScanWaterfallMarkDataClass *klass)
{
  HyScanMarkDataClass *data_class = HYSCAN_MARK_DATA_CLASS (klass);

  data_class->get_schema = hyscan_waterfall_mark_data_get_schema;
  data_class->init_plist = hyscan_waterfall_mark_data_init_plist;
  data_class->set = hyscan_waterfall_mark_data_set_internal;
  data_class->get = hyscan_waterfall_mark_data_get_internal;
}

static void
hyscan_waterfall_mark_data_init (HyScanWaterfallMarkData *data)
{
}

/* Функция возвращает информацию о схеме. */
static const gchar *
hyscan_waterfall_mark_data_get_schema (HyScanMarkData *data,
                                       gint64         *id,
                                       gint64         *version)
{
  if (version != NULL)
    *version = WATERFALL_MARK_SCHEMA_VERSION;

  if (id != NULL)
    *id = WATERFALL_MARK_SCHEMA_ID;

  return WATERFALL_MARK_SCHEMA;
}

static void
hyscan_waterfall_mark_data_init_plist (HyScanMarkData  *data,
                                       HyScanParamList *plist)
{
  /* Добавляем названия параметров в списки. */
  hyscan_param_list_add (plist, "/track");
  hyscan_param_list_add (plist, "/coordinates/source0");
  hyscan_param_list_add (plist, "/coordinates/index0");
  hyscan_param_list_add (plist, "/coordinates/count0");
}

/* Функция считывает содержимое объекта. */
static void
hyscan_waterfall_mark_data_get_internal (HyScanMarkData  *data,
                                         HyScanParamList *read_plist,
                                         HyScanMark      *mark)
{
  HyScanMarkWaterfall *mark_wf;

  g_return_if_fail (mark->type == HYSCAN_MARK_WATERFALL);
  mark_wf = (HyScanMarkWaterfall *) mark;

  hyscan_mark_waterfall_set_track  (mark_wf,
                                    hyscan_param_list_get_string (read_plist,  "/track"));

  hyscan_mark_waterfall_set_center (mark_wf,
                                    hyscan_param_list_get_integer (read_plist, "/coordinates/source0"),
                                    hyscan_param_list_get_integer (read_plist, "/coordinates/index0"),
                                    hyscan_param_list_get_integer (read_plist, "/coordinates/count0"));
}

/* Функция записывает значения в существующий объект. */
static void
hyscan_waterfall_mark_data_set_internal (HyScanMarkData   *data,
                                         HyScanParamList  *write_plist,
                                         const HyScanMark *mark)
{
  HyScanMarkWaterfall *mark_wf;

  g_return_if_fail (mark->type == HYSCAN_MARK_WATERFALL);
  mark_wf = (HyScanMarkWaterfall *) mark;

  hyscan_param_list_set (write_plist,       "/track",
                         g_variant_new_string (mark_wf->track));
  hyscan_param_list_set (write_plist,       "/coordinates/source0",
                         g_variant_new_int64  (mark_wf->source0));
  hyscan_param_list_set (write_plist,       "/coordinates/index0",
                         g_variant_new_int64  (mark_wf->index0));
  hyscan_param_list_set (write_plist,       "/coordinates/count0",
                         g_variant_new_int64  (mark_wf->count0));
}
