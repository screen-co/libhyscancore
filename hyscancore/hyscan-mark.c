/* hyscan-mark.c
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

#include "hyscan-mark.h"

G_DEFINE_BOXED_TYPE (HyScanMark, hyscan_mark,
                     hyscan_mark_copy, hyscan_mark_free)

HyScanMark*
hyscan_mark_new (HyScanMarkType type)
{
  HyScanMark* mark;
  mark = g_slice_new0 (HyScanMark);
  mark->type = type;
  return mark;
}

HyScanMark*
hyscan_mark_copy (HyScanMark *mark)
{
  HyScanMarkAny *any = (HyScanMarkAny *) mark;
  HyScanMark *copy;

  if (mark == NULL)
    return NULL;

  copy = hyscan_mark_new (mark->type);

  switch (mark->type)
    {
    case HYSCAN_MARK_WATERFALL:
      hyscan_mark_waterfall_set_track (&copy->waterfall, mark->waterfall.track);
      hyscan_mark_waterfall_set_center (&copy->waterfall, mark->waterfall.source,
                                        mark->waterfall.index, mark->waterfall.count);
      break;

    case HYSCAN_MARK_GEO:
      hyscan_mark_geo_set_center (&copy->geo, mark->geo.center);
      break;

    default:
      g_return_val_if_reached (NULL);
    }

  /* Копируем общие поля. */
  copy->type = mark->type;
  hyscan_mark_set_text (copy, any->name,
                              any->description,
                              any->operator_name);
  hyscan_mark_set_labels (copy, any->labels);
  hyscan_mark_set_ctime (copy, any->ctime);
  hyscan_mark_set_mtime (copy, any->mtime);
  hyscan_mark_set_size (copy, any->width, any->height);

  return copy;
}


void
hyscan_mark_free (HyScanMark *mark)
{
  HyScanMarkAny *any = (HyScanMarkAny *) mark;

  if (mark == NULL)
    return;

  /* Освобождаем общие поля. */
  g_free (any->name);
  g_free (any->description);
  g_free (any->operator_name);

  if (mark->type == HYSCAN_MARK_WATERFALL)
    {
      g_free (mark->waterfall.track);
      g_free (mark->waterfall.source);
    }

  g_slice_free (HyScanMark, mark);
}


void
hyscan_mark_waterfall_set_track (HyScanMarkWaterfall *mark,
                                 const gchar         *track)
{
  g_free (mark->track);
  mark->track = g_strdup (track);
}

void
hyscan_mark_set_text (HyScanMark  *mark,
                      const gchar *name,
                      const gchar *description,
                      const gchar *oper)
{
  HyScanMarkAny *any = (HyScanMarkAny *) mark;

  g_free (any->name);
  g_free (any->description);
  g_free (any->operator_name);

  any->name = g_strdup (name);
  any->description = g_strdup (description);
  any->operator_name = g_strdup (oper);
}

void
hyscan_mark_set_labels (HyScanMark *mark,
                        guint64     labels)
{
  HyScanMarkAny *any = (HyScanMarkAny *) mark;

  any->labels = labels;
}

void
hyscan_mark_set_ctime (HyScanMark *mark,
                       gint64      creation)
{
  HyScanMarkAny *any = (HyScanMarkAny *) mark;

  any->ctime = creation;
}

void
hyscan_mark_set_mtime (HyScanMark *mark,
                       gint64      modification)
{
  HyScanMarkAny *any = (HyScanMarkAny *) mark;

  any->mtime = modification;
}

void
hyscan_mark_waterfall_set_center (HyScanMarkWaterfall *mark,
                                  const gchar         *source,
                                  guint32              index,
                                  guint32              count)
{
  mark->source = g_strdup (source);
  mark->index = index;
  mark->count = count;
}

void
hyscan_mark_waterfall_set_center_by_type (HyScanMarkWaterfall *mark,
                                          HyScanSourceType     source,
                                          guint32              index,
                                          guint32              count)
{
  hyscan_mark_waterfall_set_center (mark, hyscan_source_get_id_by_type (source),
                                    index, count);
}

void
hyscan_mark_set_size (HyScanMark *mark,
                      gdouble     width,
                      gdouble     height)
{
  HyScanMarkAny *any = (HyScanMarkAny *) mark;

  any->width = width;
  any->height = height;
}

void
hyscan_mark_geo_set_center (HyScanMarkGeo     *mark,
                            HyScanGeoGeodetic  center)
{
  mark->center = center;
}
