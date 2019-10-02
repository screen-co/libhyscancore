/* hyscan-object-data.h
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

#ifndef __HYSCAN_OBJECT_DATA_H__
#define __HYSCAN_OBJECT_DATA_H__

#include <hyscan-db.h>
#include <hyscan-types.h>

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
typedef struct _HyScanObject HyScanObject;
typedef gint32 HyScanObjectType;

struct _HyScanObjectData
{
  GObject parent_instance;

  HyScanObjectDataPrivate *priv;
};

/**
 * HyScanObjectDataClass:
 * @group_name: названия группы параметров проекта
 * @init_obj: функция выполняется при создании объекта класса #HyScanObjectData
 * @object_copy: функция создаёт структуру с копией указанного объекта
 * @object_destroy: освобождает память, выделенную функциями object_copy и get_full
 * @get_read_plist: функция возвращает список параметров #HyScanParamList для чтения объекта с указанным ID
 * @get_full: функция считывает содержимое объекта
 * @set_full: функция записывает значения в существующий объект
 * @generate_id: функция генерирует уникальный идентификатор для указанного объекта
 * @get_schema_id: функция возвращает ИД схемы для указанного объекта
 */
struct _HyScanObjectDataClass
{
  GObjectClass       parent_class;

  const gchar       *group_name;

  void               (*init_obj)         (HyScanObjectData    *data,
                                          gint32               param_id,
                                          HyScanDB            *db);

  HyScanObject *     (*object_copy)      (const HyScanObject  *object);

  void               (*object_destroy)   (HyScanObject        *object);

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
};

/**
 * HyScanObject:
 * @type: тип объекта
 *
 * Все структуры, которые загружаются при помощи HyScanObjectData должны иметь
 * первое поле типа #HyScanObjectType. При передачи структуры пользователя в такие
 * функции как hyscan_object_data_add() и hyscan_object_data_modify() структура
 * должна приводится к типу #HyScanObject.
 */
struct _HyScanObject
{
  HyScanObjectType       type;
};

HYSCAN_API
GType                           hyscan_object_data_get_type          (void);

HYSCAN_API
gboolean                        hyscan_object_data_is_ready          (HyScanObjectData    *data);

HYSCAN_API
gboolean                        hyscan_object_data_add               (HyScanObjectData    *data,
                                                                      HyScanObject        *object,
                                                                      gchar              **id);

HYSCAN_API
gboolean                        hyscan_object_data_remove            (HyScanObjectData    *data,
                                                                      const gchar         *id);

HYSCAN_API
gboolean                        hyscan_object_data_modify            (HyScanObjectData    *data,
                                                                      const gchar         *id,
                                                                      const HyScanObject  *object);

HYSCAN_API
gchar **                        hyscan_object_data_get_ids           (HyScanObjectData    *data,
                                                                      guint               *len);

HYSCAN_API
HyScanObject *                  hyscan_object_data_get               (HyScanObjectData    *data,
                                                                      const gchar         *id);

HYSCAN_API
guint32                         hyscan_object_data_get_mod_count     (HyScanObjectData    *data);

G_END_DECLS

#endif /* __HYSCAN_OBJECT_DATA_H__ */
