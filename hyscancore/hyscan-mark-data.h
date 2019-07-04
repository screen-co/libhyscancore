/* hyscan-mark-data.h
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

#ifndef __HYSCAN_MARK_DATA_H__
#define __HYSCAN_MARK_DATA_H__

#include <hyscan-db.h>
#include <hyscan-types.h>
#include <hyscan-mark.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_MARK_DATA             (hyscan_mark_data_get_type ())
#define HYSCAN_MARK_DATA(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_MARK_DATA, HyScanMarkData))
#define HYSCAN_IS_MARK_DATA(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_MARK_DATA))
#define HYSCAN_MARK_DATA_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_MARK_DATA, HyScanMarkDataClass))
#define HYSCAN_IS_MARK_DATA_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_MARK_DATA))
#define HYSCAN_MARK_DATA_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_MARK_DATA, HyScanMarkDataClass))

typedef struct _HyScanMarkData HyScanMarkData;
typedef struct _HyScanMarkDataPrivate HyScanMarkDataPrivate;
typedef struct _HyScanMarkDataClass HyScanMarkDataClass;

struct _HyScanMarkData
{
  GObject parent_instance;

  HyScanMarkDataPrivate *priv;
};

/**
 * HyScanMarkDataClass:
 * @mark_type: тип метки
 * @schema_id: идентификатор схемы меток
 * @param_sid: значение параметра /schema/id для метки
 * @param_sver: значение параметра /schema/version для метки
 * @init_plist: функция добавляет в список параметров дополнительные ключи
 * @get: функция считывает содержимое объекта
 * @set: функция записывает значения в существующий объект
 */
struct _HyScanMarkDataClass
{
  GObjectClass   parent_class;

  HyScanMarkType mark_type;
  const gchar   *schema_id;
  gint64         param_sid;
  gint64         param_sver;

  void           (*init_plist)   (HyScanMarkData    *data,
                                  HyScanParamList   *plist);

  void           (*get)          (HyScanMarkData    *data,
                                  HyScanParamList   *read_plist,
                                  HyScanMark        *mark);

  void           (*set)          (HyScanMarkData    *data,
                                  HyScanParamList   *write_plist,
                                  const HyScanMark  *mark);
};

HYSCAN_API
GType                           hyscan_mark_data_get_type          (void);

HYSCAN_API
gboolean                        hyscan_mark_data_is_ready          (HyScanMarkData    *data);

HYSCAN_API
gboolean                        hyscan_mark_data_add               (HyScanMarkData    *data,
                                                                    HyScanMark        *mark);

HYSCAN_API
gboolean                        hyscan_mark_data_remove            (HyScanMarkData    *data,
                                                                    const gchar       *id);

HYSCAN_API
gboolean                        hyscan_mark_data_modify            (HyScanMarkData    *data,
                                                                    const gchar       *id,
                                                                    HyScanMark        *mark);

HYSCAN_API
gchar **                        hyscan_mark_data_get_ids           (HyScanMarkData    *data,
                                                                    guint             *len);

HYSCAN_API
HyScanMark *                    hyscan_mark_data_get               (HyScanMarkData    *data,
                                                                    const gchar       *id);

HYSCAN_API
guint32                         hyscan_mark_data_get_mod_count     (HyScanMarkData    *data);

G_END_DECLS

#endif /* __HYSCAN_MARK_DATA_H__ */
