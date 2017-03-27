/**
 *
 * \file hyscan-tile-common.h
 *
 * \brief Структуры, перечисления и вспомогательные функции для тайлов.
 * \author Dmitriev Alexander (m1n7@yandex.ru)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanTileCommon HyScanTileCommon - Структуры, перечисления
 * и вспомогательные функции для тайлов.
 *
 */

#ifndef __HYSCAN_WATERFALL_MARK_H__
#define __HYSCAN_WATERFALL_MARK_H__

#include <hyscan-core-types.h>

typedef struct
{
  gchar            *name;
  gchar            *description;
  gchar            *operator_name;
  guint64           labels;
  gint64            creation_time;
  gint64            modification_time;
  HyScanSourceType  source0;
  guint32           index0;
  guint32           count0;
  HyScanSourceType  source1;
  guint32           index1;
  guint32           count1;
} HyScanWaterfallMark;

HYSCAN_API
void                 hyscan_waterfall_mark_free         (HyScanWaterfallMark *mark);

HYSCAN_API
void                 hyscan_waterfall_mark_deep_free    (HyScanWaterfallMark *mark);

HYSCAN_API
HyScanWaterfallMark *hyscan_waterfall_mark_copy         (const HyScanWaterfallMark *mark);

#endif /* __HYSCAN_WATERFALL_MARK_H__ */
