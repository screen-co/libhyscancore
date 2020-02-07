/* hyscan-mark.h
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

#ifndef __HYSCAN_MARK_H__
#define __HYSCAN_MARK_H__

#include <hyscan-types.h>
#include <hyscan-param-list.h>
#include <hyscan-geo.h>
#include "hyscan-object-data.h"

#define HYSCAN_MARK_WATERFALL         0x0523a9ab
#define HYSCAN_MARK_GEO               0x1f5c3db7

typedef struct _HyScanMarkAny HyScanMark;
typedef struct _HyScanMarkWaterfall HyScanMarkWaterfall;
typedef struct _HyScanMarkGeo HyScanMarkGeo;

/**
 * HyScanMarkAny:
 * @type: тип метки
 * @name: название метки
 * @description: описание
 * @operator_name: имя оператора
 * @labels: метки
 * @creation_time: время создания
 * @modification_time: время последней модификации
 * @width: ширина
 * @height: высота
 *
 * Общие поля струтуры #HyScanMark. Все типы меток должны иметь указанные в
 * #HyScanAny поля в таком же порядке.
 *
 */
struct _HyScanMarkAny
{
  HyScanObjectType  type;
  gchar            *name;
  gchar            *description;
  gchar            *operator_name;
  guint64           labels;
  gint64            ctime;
  gint64            mtime;
  gdouble           width;
  gdouble           height;
};

/**
 * HyScanMarkWaterfall:
 * @type: тип метки
 * @name: название метки
 * @description: описание
 * @operator_name: имя оператора
 * @labels: метки
 * @creation_time: время создания
 * @modification_time: время последней модификации
 * @width: ширина
 * @height: высота
 * @track: идентификатор галса
 * @source: источник данных
 * @index: индекс данных
 * @count: отсчёт в строке
 *
 * Метка режима "водопад"
 */
struct _HyScanMarkWaterfall
{
  HyScanObjectType  type;
  gchar            *name;
  gchar            *description;
  gchar            *operator_name;
  guint64           labels;
  gint64            ctime;
  gint64            mtime;
  gdouble           width;
  gdouble           height;

  gchar            *track;
  gchar            *source;
  guint32           index;
  guint32           count;
};

/**
 * HyScanMarkGeo:
 * @type: тип метки
 * @name: название метки
 * @description: описание
 * @operator_name: имя оператора
 * @labels: метки
 * @creation_time: время создания
 * @modification_time: время последней модификации
 * @width: ширина области (перпендикулярно направлению на север)
 * @height: высота области (параллельно направлению на север)
 * @center: географические координаты центра метки
 *
 * Географическая метка в виде прямоугольника вдоль направления на север
 */
struct _HyScanMarkGeo
{
  HyScanObjectType  type;
  gchar            *name;
  gchar            *description;
  gchar            *operator_name;
  guint64           labels;
  gint64            ctime;
  gint64            mtime;
  gdouble           width;
  gdouble           height;

  HyScanGeoGeodetic center;
};

HYSCAN_API
HyScanMarkWaterfall *  hyscan_mark_waterfall_new                    (void);

HYSCAN_API
HyScanMarkWaterfall *  hyscan_mark_waterfall_copy                   (const HyScanMarkWaterfall *mark);

HYSCAN_API
void                   hyscan_mark_waterfall_free                   (HyScanMarkWaterfall       *mark);

HYSCAN_API
HyScanMarkGeo *        hyscan_mark_geo_new                          (void);

HYSCAN_API
HyScanMarkGeo *        hyscan_mark_geo_copy                         (const HyScanMarkGeo       *mark);

HYSCAN_API
void                   hyscan_mark_geo_free                         (HyScanMarkGeo             *mark);

HYSCAN_API
void                   hyscan_mark_set_text                         (HyScanMark            *mark,
                                                                     const gchar           *name,
                                                                     const gchar           *description,
                                                                     const gchar           *oper);
HYSCAN_API
void                   hyscan_mark_set_labels                       (HyScanMark            *mark,
                                                                     guint64                labels);

HYSCAN_API
void                   hyscan_mark_set_ctime                        (HyScanMark            *mark,
                                                                     gint64                 creation);
HYSCAN_API
void                   hyscan_mark_set_mtime                        (HyScanMark            *mark,
                                                                     gint64                 modification);
HYSCAN_API
void                   hyscan_mark_set_size                         (HyScanMark            *mark,
                                                                     gdouble                width,
                                                                     gdouble                height);
HYSCAN_API
void                   hyscan_mark_waterfall_set_track              (HyScanMarkWaterfall   *mark,
                                                                     const gchar           *track);
HYSCAN_API
void                   hyscan_mark_waterfall_set_center             (HyScanMarkWaterfall   *mark,
                                                                     const gchar           *source,
                                                                     guint32                index,
                                                                     guint32                count);
HYSCAN_API
void                   hyscan_mark_waterfall_set_center_by_type     (HyScanMarkWaterfall   *mark,
                                                                     HyScanSourceType       source,
                                                                     guint32                index,
                                                                     guint32                count);
HYSCAN_API
void                   hyscan_mark_geo_set_center                   (HyScanMarkGeo         *mark,
                                                                     HyScanGeoGeodetic      center);

G_END_DECLS

#endif /* __HYSCAN_MARK_H__ */