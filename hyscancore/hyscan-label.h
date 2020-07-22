/* hyscan-label.h
 *
 * Copyright 2020 Screen LLC, Andrey Zakharov <zaharov@screen-co.ru>
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
#ifndef __HYSCAN_LABEL_H__
#define __HYSCAN_LABEL_H__

#include <hyscan-object-data.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_LABEL (hyscan_label_get_type ())

typedef struct _HyScanLabel HyScanLabel;

/**
 * HyScanLabel:
 * @type: тип объекта (группа)
 * @name: название группы
 * @description: описание группы
 * @operator_name: имя оператора
 * @icon_data: строка содержащая графическое изображение для группы (иконку) в формате BASE64
 * @label: идентификатор группы (битовая маска)
 * @ctime: время создания (Unix-время в секундах)
 * @mtime: время последней модификации (Unix-время в секундах)
 */
struct _HyScanLabel
{
  GType             type;
  gchar            *name;
  gchar            *description;
  gchar            *operator_name;
  gchar            *icon_data;
  guint64           label;
  gint64            ctime;
  gint64            mtime;
};

HYSCAN_API
GType                hyscan_label_get_type                     (void);

HYSCAN_API
HyScanLabel*         hyscan_label_new                          (void);

HYSCAN_API
HyScanLabel*         hyscan_label_copy                         (const HyScanLabel      *self);

HYSCAN_API
void                 hyscan_label_free                         (HyScanLabel            *self);

HYSCAN_API
void                 hyscan_label_set_text                     (HyScanLabel            *self,
                                                                const gchar            *name,
                                                                const gchar            *description,
                                                                const gchar            *operator_name);

HYSCAN_API
void                 hyscan_label_set_icon_data                (HyScanLabel            *self,
                                                                const gchar            *icon_data);

HYSCAN_API
void                 hyscan_label_set_ctime                    (HyScanLabel            *self,
                                                                gint64                  ctime);

HYSCAN_API
void                 hyscan_label_set_mtime                    (HyScanLabel            *self,
                                                                gint64                  mtime);

HYSCAN_API
void                 hyscan_label_set_label                    (HyScanLabel            *self,
                                                                guint64                 label);

G_END_DECLS

#endif /* __HYSCAN_LABEL_H__ */
