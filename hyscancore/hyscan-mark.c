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

static void  hyscan_mark_copy_any (const HyScanMark *mark,
                                   HyScanMark       *copy);
static void  hyscan_mark_free_any (HyScanMark       *mark);

/* Удаляет общие поля. */
static void
hyscan_mark_free_any (HyScanMark *mark)
{
  g_free (mark->name);
  g_free (mark->description);
  g_free (mark->operator_name);
}

/* Копирует общие поля. */
static void
hyscan_mark_copy_any (const HyScanMark *mark,
                      HyScanMark       *copy)
{
  hyscan_mark_set_text (copy, mark->name,
                              mark->description,
                              mark->operator_name);
  hyscan_mark_set_labels (copy, mark->labels);
  hyscan_mark_set_ctime (copy, mark->ctime);
  hyscan_mark_set_mtime (copy, mark->mtime);
  hyscan_mark_set_size (copy, mark->width, mark->height);
}

/**
 * hyscan_mark_waterfall_new:
 * 
 * Создаёт структуру #HyScanMarkWaterfall
 * 
 * Returns: указатель на #HyScanMarkWaterfall. Для удаления hyscan_mark_waterfall_free()
 */
HyScanMarkWaterfall *
hyscan_mark_waterfall_new (void)
{
  HyScanMarkWaterfall *mark;

  mark = g_slice_new0 (HyScanMarkWaterfall);
  mark->type = HYSCAN_MARK_WATERFALL;

  return mark;
}

/**
 * hyscan_mark_waterfall_copy:
 * @mark: указатель на копируемую структуру
 * 
 * Создаёт копию структуру @mark
 * 
 * Returns: указатель на #HyScanMarkWaterfall. Для удаления hyscan_mark_waterfall_free()
 */
HyScanMarkWaterfall *
hyscan_mark_waterfall_copy (const HyScanMarkWaterfall *mark)
{
  HyScanMarkWaterfall *copy;

  if (mark == NULL)
    return NULL;

  copy = hyscan_mark_waterfall_new ();
  hyscan_mark_waterfall_set_track (copy, mark->track);
  hyscan_mark_waterfall_set_center (copy, mark->source, mark->index, mark->count);
  hyscan_mark_copy_any ((const HyScanMark *) mark, (HyScanMark *) copy);

  return copy;
}

/**
 * hyscan_mark_waterfall_free:
 * @mark: указатель на HyScanWaterfallMark
 * 
 * Удаляет структуру #HyScanMarkWaterfall
 */
void
hyscan_mark_waterfall_free (HyScanMarkWaterfall *mark)
{
  if (mark == NULL)
    return;

  g_free (mark->track);
  g_free (mark->source);
  hyscan_mark_free_any ((HyScanMark *) mark);
  g_slice_free (HyScanMarkWaterfall, mark);
}

/**
 * hyscan_mark_geo_new:
 * 
 * Создаёт структуру #HyScanMarkGeo
 * 
 * Returns: указатель на #HyScanMarkGeo. Для удаления hyscan_mark_geo_free()
 */
HyScanMarkGeo *
hyscan_mark_geo_new (void)
{
  HyScanMarkGeo *mark;

  mark = g_slice_new0 (HyScanMarkGeo);
  mark->type = HYSCAN_MARK_GEO;

  return mark;
}

/**
 * hyscan_mark_geo_copy:
 * @mark: указатель на копируемую структуру
 * 
 * Создаёт копию структуру @mark
 * 
 * Returns: указатель на #HyScanMarkGeo. Для удаления hyscan_mark_geo_free()
 */
HyScanMarkGeo *
hyscan_mark_geo_copy (const HyScanMarkGeo *mark)
{
  HyScanMarkGeo *copy;

  if (mark == NULL)
    return NULL;

  copy = hyscan_mark_geo_new ();
  hyscan_mark_geo_set_center (copy, mark->center);
  hyscan_mark_copy_any ((const HyScanMark *) mark, (HyScanMark *) copy);

  return copy;
}

/**
 * hyscan_mark_geo_free:
 * @mark: указатель на HyScanGeoMark
 * 
 * Удаляет структуру #HyScanMarkGeo
 */
void
hyscan_mark_geo_free (HyScanMarkGeo *mark)
{
  if (mark == NULL)
    return;

  hyscan_mark_free_any ((HyScanMark *) mark);
  g_slice_free (HyScanMarkGeo, mark);
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
  g_free (mark->name);
  g_free (mark->description);
  g_free (mark->operator_name);

  mark->name = g_strdup (name);
  mark->description = g_strdup (description);
  mark->operator_name = g_strdup (oper);
}

void
hyscan_mark_set_labels (HyScanMark *mark,
                        guint64     labels)
{
  mark->labels = labels;
}

void
hyscan_mark_set_ctime (HyScanMark *mark,
                       gint64      creation)
{
  mark->ctime = creation;
}

void
hyscan_mark_set_mtime (HyScanMark *mark,
                       gint64      modification)
{
  mark->mtime = modification;
}

void
hyscan_mark_waterfall_set_center (HyScanMarkWaterfall *mark,
                                  const gchar         *source,
                                  guint32              index,
                                  guint32              count)
{
  g_clear_pointer (&mark->source, g_free);
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
  mark->width = width;
  mark->height = height;
}

void
hyscan_mark_geo_set_center (HyScanMarkGeo     *mark,
                            HyScanGeoGeodetic  center)
{
  mark->center = center;
}
