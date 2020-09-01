/* hyscan-object-data.h
 *
 * Copyright 2017-2019 Screen LLC, Dmitriev Alexander <m1n7@yandex.ru>
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

#ifndef __HYSCAN_OBJECT_DATA_H__
#define __HYSCAN_OBJECT_DATA_H__

#include <hyscan-db.h>
#include <hyscan-object-store.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_OBJECT_DATA             (hyscan_object_data_get_type ())
#define HYSCAN_OBJECT_DATA(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_OBJECT_DATA, HyScanObjectData))
#define HYSCAN_IS_OBJECT_DATA(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_OBJECT_DATA))
#define HYSCAN_OBJECT_DATA_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_OBJECT_DATA, HyScanObjectDataClass))
#define HYSCAN_IS_OBJECT_DATA_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_OBJECT_DATA))
#define HYSCAN_OBJECT_DATA_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_OBJECT_DATA, HyScanObjectDataClass))

typedef struct _HyScanObjectData HyScanObjectData;
typedef struct _HyScanObjectDataPrivate HyScanObjectDataPrivate;
typedef struct _HyScanObjectDataClass HyScanObjectDataClass;

struct _HyScanObjectData
{
  GObject parent_instance;

  HyScanObjectDataPrivate *priv;
};

/**
 * HyScanObjectDataClass:
 * @group_name: названия группы параметров проекта
 * @data_types (array-size=n_data_types): обрабатываемые типы объектов HyScanObject
 * @n_data_types: количество обрабатываемые типов
 * @get_read_plist: функция возвращает список параметров #HyScanParamList для чтения объекта с указанным ID
 * @get_full: функция считывает содержимое объекта
 * @set_full: функция записывает значения в существующий объект
 * @generate_id: функция потокобезопасно генерирует уникальный идентификатор для указанного объекта
 * @get_schema_id: функция возвращает ИД схемы для указанного объекта
 */
struct _HyScanObjectDataClass
{
  GObjectClass       parent_class;

  const gchar       *group_name;

  const GType       *data_types;

  guint              n_data_types;

  HyScanParamList *  (*get_read_plist)   (HyScanObjectData    *data,
                                          const gchar         *id);

  HyScanObject *     (*get_full)         (HyScanObjectData    *data,
                                          HyScanParamList     *read_plist);

  gboolean           (*set_full)         (HyScanObjectData    *data,
                                          HyScanParamList     *write_plist,
                                          const HyScanObject  *object);

  gchar *            (*generate_id)      (HyScanObjectData    *data,
                                          const HyScanObject  *object);

  const gchar *      (*get_schema_id)    (HyScanObjectData    *data,
                                          const HyScanObject  *object);

  GType              (*get_object_type)  (HyScanObjectData    *data,
                                          const gchar         *id);
};

HYSCAN_API
GType                           hyscan_object_data_get_type          (void);

HYSCAN_API
HyScanObjectData *              hyscan_object_data_new               (GType               type);

HYSCAN_API
gboolean                        hyscan_object_data_project_open      (HyScanObjectData   *data,
                                                                      HyScanDB           *db,
                                                                      const gchar        *project);

HYSCAN_API
gboolean                        hyscan_object_data_is_ready          (HyScanObjectData   *data);

HYSCAN_API
gchar *                         hyscan_object_data_generate_id       (HyScanObjectData   *data,
                                                                      const HyScanObject *object);

G_END_DECLS

#endif /* __HYSCAN_OBJECT_DATA_H__ */
