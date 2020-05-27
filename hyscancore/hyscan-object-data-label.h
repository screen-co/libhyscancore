/* hyscan-object-data-label.h
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

#ifndef __HYSCAN_OBJECT_DATA_LABEL_H__
#define __HYSCAN_OBJECT_DATA_LABEL_H__

#include <hyscan-label.h>
#include <hyscan-object-data.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_OBJECT_DATA_LABEL             (hyscan_object_data_label_get_type ())
#define HYSCAN_OBJECT_DATA_LABEL(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_OBJECT_DATA_LABEL, HyScanObjectDataLabel))
#define HYSCAN_IS_OBJECT_DATA_LABEL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_OBJECT_DATA_LABEL))
#define HYSCAN_OBJECT_DATA_LABEL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_OBJECT_DATA_LABEL, HyScanObjectDataLabelClass))
#define HYSCAN_IS_OBJECT_DATA_LABEL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_OBJECT_DATA_LABEL))
#define HYSCAN_OBJECT_DATA_LABEL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_OBJECT_DATA_LABEL, HyScanObjectDataLabelClass))

typedef struct _HyScanObjectDataLabel        HyScanObjectDataLabel;
typedef struct _HyScanObjectDataLabelPrivate HyScanObjectDataLabelPrivate;
typedef struct _HyScanObjectDataLabelClass   HyScanObjectDataLabelClass;

struct _HyScanObjectDataLabel
{
  HyScanObjectData              parent_instance;
  HyScanObjectDataLabelPrivate *priv;
};

struct _HyScanObjectDataLabelClass
{
  HyScanObjectDataClass parent_class;
};

HYSCAN_API
GType                  hyscan_object_data_label_get_type    (void);

G_END_DECLS

#endif /* __HYSCAN_OBJECT_DATA_LABEL_H__ */
