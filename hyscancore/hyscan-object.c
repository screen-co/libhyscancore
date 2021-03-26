#include "hyscan-object.h"

/**
 * hyscan_object_equal_func:
 *
 * Функция получает GQuark для функции сравнения объектов.
 *
 * Returns: a #GQuark.
 */
G_DEFINE_QUARK (hyscan-object-equal-func-quark, hyscan_object_equal_func)

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
 * hyscan_object_equal:
 * @object1: указатель на структуру #HyScanObject
 * @object2: указатель на структуру #HyScanObject для сравнения
 *
 * Функция сравнивает две структуры #HyScanObject, и возвращает %TRUE, если они равны.
 *
 * Returns: %TRUE, если структуры равны; иначе %FALSE.
 */
gboolean
hyscan_object_equal (const HyScanObject *object1,
                     const HyScanObject *object2)
{
  GEqualFunc equal_func;

  g_return_val_if_fail (object1->type == object2->type, FALSE);

  equal_func = g_type_get_qdata (object1->type, hyscan_object_equal_func_quark ());
  if (equal_func == NULL)
    return FALSE;

  return equal_func (object1, object2);
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
