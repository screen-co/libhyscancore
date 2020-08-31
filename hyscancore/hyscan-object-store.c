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

/**
 * hyscan_object_id_new:
 *
 * Функция создаёт структуру #HyScanObjectId.
 *
 * Returns: (transfer full): новая структура #HyScanObjectId, для удаления
 * hyscan_object_id_free().
 */
HyScanObjectId *
hyscan_object_id_new (void)
{
  return g_slice_new0 (HyScanObjectId);
}

/**
 * hyscan_object_id_free:
 * @object_id: указатель на #HyScanObjectId
 *
 * Функция удаляет структуру #HyScanObjectId.
 */
void
hyscan_object_id_free (HyScanObjectId *object_id)
{
  if (object_id == NULL)
    return;

  g_free (object_id->id);
  g_slice_free (HyScanObjectId, object_id);
}

/**
 * hyscan_object_copy:
 * @object: указатель на структуру #HyScanObject
 *
 * Функция создаёт структуру с копией указанного объекта @object.
 *
 * Returns: (transfer full): копия структуры или %NULL
 */
HyScanObject *
hyscan_object_copy (const HyScanObject *object)
{
  if (object == NULL)
    return NULL;

  return g_boxed_copy (object->type, object);
}

/**
 * hyscan_object_free:
 * @object: указатель на структуру #HyScanObject
 *
 * Функция удаляет структуру @object.
 */
void
hyscan_object_free (HyScanObject *object)
{
  if (object == NULL)
    return;

  g_boxed_free (object->type, object);
}
