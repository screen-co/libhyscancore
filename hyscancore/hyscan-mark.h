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

typedef enum _HyScanMarkType HyScanMarkType;
typedef union _HyScanMark HyScanMark;
typedef struct _HyScanMarkAny HyScanMarkAny;
typedef struct _HyScanMarkWaterfall HyScanMarkWaterfall;
typedef struct _HyScanMarkGeo HyScanMarkGeo;

/**
 * HyScanMarkType:
 * @HYSCAN_MARK_WATERFALL: метка водопада #HyScanMarkWaterfall
 * @HYSCAN_MARK_GEO: географическая метка #HyScanMarkGeo
 *
 * Тип метки
 */
enum _HyScanMarkType
{
  HYSCAN_MARK_WATERFALL,
  HYSCAN_MARK_GEO,
};

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
  HyScanMarkType    type;
  gchar            *name;
  gchar            *description;
  gchar            *operator_name;
  guint64           labels;
  gint64            creation_time;
  gint64            modification_time;
  guint32           width;
  guint32           height;
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
 * @source0: источник данных
 * @index0: индекс данных
 * @count0: отсчёт в строке
 *
 * Метка режима "водопад"
 */
struct _HyScanMarkWaterfall
{
  HyScanMarkType    type;
  gchar            *name;             
  gchar            *description;
  gchar            *operator_name;
  guint64           labels;
  gint64            creation_time;
  gint64            modification_time;
  guint32           width;
  guint32           height;

  gchar            *track;
  HyScanSourceType  source0;
  guint32           index0;
  guint32           count0;
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
  HyScanMarkType    type;
  gchar            *name;
  gchar            *description;
  gchar            *operator_name;
  guint64           labels;
  gint64            creation_time;
  gint64            modification_time;
  guint32           width;
  guint32           height;

  HyScanGeoGeodetic center;
};

/**
 * HyScanMark:
 * @type: тип метки
 * @any: общие поля разных типов меток
 * @waterfall: метка водопада
 * @geo: географическая метка
 *
 * Объединение различных типов меток. Тип метки всегда хранится в первом поле
 * type. Чтобы получить значения других полей, необходимо привести #HyScanMark
 * к нужному типу.
 */
union _HyScanMark
{
  HyScanMarkType        type;
  HyScanMarkAny         any;
  HyScanMarkWaterfall   waterfall;
  HyScanMarkGeo         geo;
};

HYSCAN_API
HyScanMark            *hyscan_mark_new                              (HyScanMarkType         type);

HYSCAN_API
HyScanMark            *hyscan_mark_copy                             (HyScanMark            *mark);

HYSCAN_API
void                   hyscan_mark_free                             (HyScanMark            *mark);

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
                                                                     guint32                width,
                                                                     guint32                height);
HYSCAN_API
void                   hyscan_mark_waterfall_set_track              (HyScanMarkWaterfall   *mark,
                                                                     const gchar           *track);
HYSCAN_API
void                   hyscan_mark_waterfall_set_center             (HyScanMarkWaterfall   *mark,
                                                                     HyScanSourceType       source,
                                                                     guint32                index,
                                                                     guint32                count);
HYSCAN_API
void                   hyscan_mark_geo_set_center                   (HyScanMarkGeo         *mark,
                                                                     HyScanGeoGeodetic      center);

G_END_DECLS

#endif /* __HYSCAN_MARK_H__ */
