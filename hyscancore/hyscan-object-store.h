/* hyscan-object-store.h
 *
 * Copyright 2020 Screen LLC, Alexey Sakhnov <alexsakhnov@gmail.com>
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

#ifndef __HYSCAN_OBJECT_STORE_H__
#define __HYSCAN_OBJECT_STORE_H__

#include <hyscan-types.h>
#include <hyscan-object.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_OBJECT_STORE            (hyscan_object_store_get_type ())
#define HYSCAN_OBJECT_STORE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_OBJECT_STORE, HyScanObjectStore))
#define HYSCAN_IS_OBJECT_STORE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_OBJECT_STORE))
#define HYSCAN_OBJECT_STORE_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), HYSCAN_TYPE_OBJECT_STORE, HyScanObjectStoreInterface))

typedef struct _HyScanObjectStore HyScanObjectStore;
typedef struct _HyScanObjectStoreInterface HyScanObjectStoreInterface;

/**
 * HyScanObjectStoreInterface:
 * @get: Функция получает объект по его типу и идентификатору.
 * @get_ids: Функция возвращает идентификаторы всех объектов в хранилище.
 * @get_all: Функция возвращает хэш-таблицу со всеми объектами указанного типа.
 * @add: Функция добавляет объект.
 * @modify: Функция модифицирует объект.
 * @set: Функция устанавливает значение объекта.
 * @remove: Функция удаляет объект.
 * @get_mod_count: Функция получает номер изменения объектов указанного типа.
 * @list_types: Функция возвращает список типов объектов в хранилище.
 */
struct _HyScanObjectStoreInterface
{
  GTypeInterface       g_iface;

  HyScanObject *       (*get)                                  (HyScanObjectStore  *store,
                                                                GType               type,
                                                                const gchar        *id);

  GList *              (*get_ids)                              (HyScanObjectStore  *store);

  GHashTable *         (*get_all)                              (HyScanObjectStore  *store,
                                                                GType               type);

  gboolean             (*add)                                  (HyScanObjectStore  *store,
                                                                const HyScanObject *object,
                                                                gchar             **given_id);

  gboolean             (*modify)                               (HyScanObjectStore  *store,
                                                                const gchar        *id,
                                                                const HyScanObject *object);

  gboolean             (*set)                                  (HyScanObjectStore  *store,
                                                                GType               type,
                                                                const gchar        *id,
                                                                const HyScanObject *object);

  gboolean             (*remove)                               (HyScanObjectStore  *store,
                                                                GType               type,
                                                                const gchar        *id);

  guint32              (*get_mod_count)                        (HyScanObjectStore  *store,
                                                                GType               type);

  const GType *        (*list_types)                           (HyScanObjectStore  *store,
                                                                guint              *len);
};

HYSCAN_API
GType                  hyscan_object_store_get_type            (void);

HYSCAN_API
HyScanObject *         hyscan_object_store_get                 (HyScanObjectStore  *store,
                                                                GType               type,
                                                                const gchar        *id);

HYSCAN_API
GList *                hyscan_object_store_get_ids             (HyScanObjectStore  *store);

HYSCAN_API
GHashTable *           hyscan_object_store_get_all             (HyScanObjectStore  *store,
                                                                GType               type);

HYSCAN_API
gboolean               hyscan_object_store_add                 (HyScanObjectStore  *store,
                                                                const HyScanObject *object,
                                                                gchar             **given_id);

HYSCAN_API
gboolean               hyscan_object_store_modify              (HyScanObjectStore  *store,
                                                                const gchar        *id,
                                                                const HyScanObject *object);

HYSCAN_API
gboolean               hyscan_object_store_set                 (HyScanObjectStore  *store,
                                                                GType               type,
                                                                const gchar        *id,
                                                                const HyScanObject *object);

HYSCAN_API
gboolean               hyscan_object_store_remove              (HyScanObjectStore  *store,
                                                                GType               type,
                                                                const gchar        *id);

HYSCAN_API
guint32                hyscan_object_store_get_mod_count       (HyScanObjectStore  *store,
                                                                GType               type);

HYSCAN_API
const GType *          hyscan_object_store_list_types          (HyScanObjectStore  *store,
                                                                guint              *len);

G_END_DECLS

#endif /* __HYSCAN_OBJECT_STORE_H__ */
