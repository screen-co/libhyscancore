/* hyscan-object-store.c
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

/**
 * SECTION: hyscan-object-store
 * @Short_description: Хранилище объектов HyScanObject
 * @Title: HyScanObjectStore
 * @See_also: HyScanObject
 *
 * Интерфейс HyScanObjectStore определяет набор функций для записи, получения
 * и модификации объектов HyScanObject в некотором хранилище.
 *
 * Хранилище может управлять как одним, так и несколькими типами объектов #HyScanObject.
 * Получить список типов объектов, которые могут находится в данном хранилище
 * можно при помощи функции hyscan_object_store_list_types().
 *
 * Для получения информации об объектах доступны следующие функции:
 *
 * - hyscan_object_store_get() - получение одного объекта,
 * - hyscan_object_store_get_all() - получение всех объектов указанного типа,
 * - hyscan_object_store_get_ids() - получение идентификаторо всех объектов.
 *
 * Для изменения объектов используются функции:
 *
 * - hyscan_object_store_add() - добавляет новый объект,
 * - hyscan_object_store_modify() - модифицирует существующий объект,
 * - hyscan_object_store_remove() - удаляет существующий объект,
 * - hyscan_object_store_set() - устанавливает значение объекта.
 *
 * Последняя функция в зависимости от переданных параметров, может как создать,
 * так и модифицировать или удалить объект. Её полезно использовать, если
 * идентификатор объекта известен заранее.
 *
 * Информацию о том, что в хранилище что-то поменялось, можно получить при помощи
 * функции hyscan_object_store_get_mod_count(). Возвращаемое значение этой функции
 * меняется каждый раз при поступлении каких-либо изменений; или остаётся постоянным,
 * если никаких изменений не было.
 *
 */

#include "hyscan-object-store.h"

G_DEFINE_INTERFACE (HyScanObjectStore, hyscan_object_store, G_TYPE_OBJECT)

static void
hyscan_object_store_default_init (HyScanObjectStoreInterface *iface)
{
}

/**
 * hyscan_object_store_get:
 * @store: указатель на #HyScanObjectStore
 * @type: #GType тип объекта
 * @id: идентификатор объекта
 *
 * Функция получает объект #HyScanObject по его идентификатору и типу.
 *
 * Returns: (transfer full): объект #HyScanObject, для удаления hyscan_object_free()
 */
HyScanObject *
hyscan_object_store_get (HyScanObjectStore *store,
                         GType              type,
                         const gchar       *id)
{
  HyScanObjectStoreInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_OBJECT_STORE (store), NULL);

  iface = HYSCAN_OBJECT_STORE_GET_IFACE (store);
  if (iface->get != NULL)
    return  (* iface->get) (store, type, id);

  return NULL;
}

/**
 * hyscan_object_store_get_ids:
 * @store: указатель на #HyScanObjectStore
 *
 * Функция получает список идентификаторов всех объектов.
 *
 * Returns: (transfer full) (element-type HyScanObjectId): список идентификаторов,
 * для удаления g_list_free_full().
 */
GList *
hyscan_object_store_get_ids (HyScanObjectStore *store)
{
  HyScanObjectStoreInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_OBJECT_STORE (store), NULL);

  iface = HYSCAN_OBJECT_STORE_GET_IFACE (store);
  if (iface->get_ids != NULL)
    return (* iface->get_ids) (store);

  return NULL;
}

/**
 * hyscan_object_store_get_all:
 * @store: указатель на #HyScanObjectStore
 * @type: #GType тип объектов
 *
 * Функция получает все объекты типа @type, существующие в хранилище, в виде
 * хэш-таблицы. Ключом является идентификатор объекта, а значением - сам объект.
 *
 * Returns: (transfer full) (element-type HyScanObject): хэш-таблица объектов,
 * для удаления g_hash_table_unref()
 */
GHashTable *
hyscan_object_store_get_all (HyScanObjectStore *store,
                             GType              type)
{
  HyScanObjectStoreInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_OBJECT_STORE (store), NULL);

  iface = HYSCAN_OBJECT_STORE_GET_IFACE (store);
  if (iface->get_all != NULL)
    return (* iface->get_all) (store, type);

  return NULL;
}

/**
 * hyscan_object_store_add:
 * @store: указатель на #HyScanObjectStore
 * @object: объект #HyScanObject
 * @given_id: (nullable) (out): установленный идентификатор объекта или %NULL
 *
 * Функция добавляет объект #HyScanObject и возвращает назначенный ему идентификатор.
 *
 * Returns: %TRUE в случае успеха, иначе %FALSE
 */
gboolean
hyscan_object_store_add (HyScanObjectStore  *store,
                         const HyScanObject *object,
                         gchar             **given_id)
{
  HyScanObjectStoreInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_OBJECT_STORE (store), FALSE);

  iface = HYSCAN_OBJECT_STORE_GET_IFACE (store);
  if (iface->add != NULL)
    return (* iface->add) (store, object, given_id);

  return FALSE;
}

/**
 * hyscan_object_store_modify:
 * @store: указатель на #HyScanObjectStore
 * @id: идентификатор объекта
 * @object: объект #HyScanObject
 *
 * Функция изменяет объект #HyScanObject.
 *
 * Returns: %TRUE в случае успеха, иначе %FALSE
 */
gboolean
hyscan_object_store_modify (HyScanObjectStore  *store,
                            const gchar        *id,
                            const HyScanObject *object)
{
  HyScanObjectStoreInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_OBJECT_STORE (store), FALSE);

  iface = HYSCAN_OBJECT_STORE_GET_IFACE (store);
  if (iface->modify != NULL)
    return (* iface->modify) (store, id, object);

  return FALSE;
}

/**
 * hyscan_object_data_set:
 * @store: указатель на #HyScanObjectStore
 * @type: тип объекта
 * @id: (allow-none): идентификатор объекта
 * @object: (allow-none): указатель на структуру #HyScanObject
 *
 * Функция автоматически управляет объектами.
 * Если id задан, то объект создается или модифицируется (при отсутствии и
 * наличии в БД соответственно).
 * Если id не задан, объект создается. Если id задан, а object не задан, объект
 * удаляется.
 * Ошибкой является не задать и id, и object.
 *
 * Returns: %TRUE, если удалось добавить/удалить/модифицировать объект.
 */
gboolean
hyscan_object_store_set (HyScanObjectStore  *store,
                         GType               type,
                         const gchar        *id,
                         const HyScanObject *object)
{
  HyScanObjectStoreInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_OBJECT_STORE (store), FALSE);

  iface = HYSCAN_OBJECT_STORE_GET_IFACE (store);
  if (iface->set != NULL)
    return (* iface->set) (store, type, id, object);

  return FALSE;
}

/**
 * hyscan_object_store_remove:
 * @store: указатель на #HyScanObjectStore
 * @type: #GType тип объекта
 * @id: идентификатор объекта
 *
 * Функция удаляет объект по его типу и идентификатору.
 *
 * Returns: %TRUE в случае успеха, иначе %FALSE
 */
gboolean
hyscan_object_store_remove (HyScanObjectStore *store,
                            GType              type,
                            const gchar       *id)
{
  HyScanObjectStoreInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_OBJECT_STORE (store), FALSE);

  iface = HYSCAN_OBJECT_STORE_GET_IFACE (store);
  if (iface->remove != NULL)
    return (* iface->remove) (store, type, id);

  return FALSE;
}

/**
 * hyscan_object_store_get_mod_count:
 * @store: указатель на #HyScanObjectStore
 * @type: #GType тип объектов
 *
 * Функция получает номер изменения объектов типа @type. Если номер не изменился,
 * то гарантируется отсутствие изменений в объектах указанного типа
 *
 * Чтобы запросить информацию обо всех данных в хранилище, следует использовать
 * тип %G_TYPE_BOXED.
 *
 * Программа не должна полагаться на значение номера изменения, важен только факт
 * смены номера по сравнению с предыдущим запросом.
 *
 * Returns: номер изменения
 */
guint32
hyscan_object_store_get_mod_count (HyScanObjectStore *store,
                                   GType              type)
{
  HyScanObjectStoreInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_OBJECT_STORE (store), 0);

  iface = HYSCAN_OBJECT_STORE_GET_IFACE (store);
  if (iface->get_mod_count != NULL)
    return (* iface->get_mod_count) (store, type);

  return 0;
}

/**
 * hyscan_object_store_get_mod_count:
 * @store: указатель на #HyScanObjectStore
 * @type: #GType тип объектов
 *
 * Функция возвращает список типов объектов, которые обрабатываются данными
 * хранилищем объектов.
 *
 * Returns: (array-size=len): список типов #HyScanObject
 */
const GType *
hyscan_object_store_list_types (HyScanObjectStore *store,
                                guint             *len)
{
  HyScanObjectStoreInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_OBJECT_STORE (store), NULL);

  iface = HYSCAN_OBJECT_STORE_GET_IFACE (store);
  if (iface->list_types != NULL)
    return (* iface->list_types) (store, len);

  len != NULL ? (*len = 0) : 0;

  return NULL;
}
