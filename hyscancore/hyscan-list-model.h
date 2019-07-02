/* hyscan-list-model.h
 *
 * Copyright 2019 Screen LLC, Alexey Sakhnov <alexsakhnov@gmail.com>
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

#ifndef __HYSCAN_LIST_MODEL_H__
#define __HYSCAN_LIST_MODEL_H__

#include <glib-object.h>
#include <hyscan-api.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_LIST_MODEL             (hyscan_list_model_get_type ())
#define HYSCAN_LIST_MODEL(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_LIST_MODEL, HyScanListModel))
#define HYSCAN_IS_LIST_MODEL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_LIST_MODEL))
#define HYSCAN_LIST_MODEL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_LIST_MODEL, HyScanListModelClass))
#define HYSCAN_IS_LIST_MODEL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_LIST_MODEL))
#define HYSCAN_LIST_MODEL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_LIST_MODEL, HyScanListModelClass))

typedef struct _HyScanListModel HyScanListModel;
typedef struct _HyScanListModelPrivate HyScanListModelPrivate;
typedef struct _HyScanListModelClass HyScanListModelClass;

struct _HyScanListModel
{
  GObject parent_instance;

  HyScanListModelPrivate *priv;
};

struct _HyScanListModelClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                  hyscan_list_model_get_type         (void);

HYSCAN_API
HyScanListModel *      hyscan_list_model_new              (void);

HYSCAN_API
gchar **               hyscan_list_model_get              (HyScanListModel      *list_model);

HYSCAN_API
void                   hyscan_list_model_add              (HyScanListModel      *list_model,
                                                           const gchar          *key);

HYSCAN_API
void                   hyscan_list_model_remove           (HyScanListModel      *list_model,
                                                           const gchar          *key);

HYSCAN_API
void                   hyscan_list_model_remove_all       (HyScanListModel      *list_model);

HYSCAN_API
gboolean               hyscan_list_model_has              (HyScanListModel      *list_model,
                                                           const gchar          *key);


G_END_DECLS

#endif /* __HYSCAN_LIST_MODEL_H__ */