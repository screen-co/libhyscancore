/* hyscan-object.c
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
 * SECTION: hyscan-object
 * @Short_description: Структуры для хранения данных в HyScanObjectStore
 * @Title: HyScanObject
 * @See_also: HyScanObjectStore
 *
 * HyScanObject содержит вспомогательные типы и функции для структур, с
 * которыми может работать интерфейс #HyScanObjectStore.
 *
 * Все структуры Си, с которыми планируется работать через #HyScanObjectStore, должны быть
 * зарегистрированы как типы GBoxed, и иметь первое поле type с идентификатором своего типа GType.
 * Таким образом все такие структуры могут быть приведены к типу #HyScanObject,
 * с которым уже работает #HyScanObjectStore.
 *
 * При регистрации GBoxed типа помимо стандартных функций для копирования и удаления
 * структуры может быть определена функция для сравнения объектов типа при помощи
 * макроса HYSCAN_OBJECT_DEFINE_EQUAL_FUNC():
 *
 * |[<!-- language="C" -->
 * G_DEFINE_BOXED_TYPE_WITH_CODE (HyScanFoo,
 *                                hyscan_foo,
 *                                hyscan_foo_copy,
 *                                hyscan_foo_free,
 *                                HYSCAN_OBJECT_DEFINE_EQUAL_FUNC (hyscan_foo_equal));
 * ]|
 *
 * Для структур #HyScanObject определены следущие вспомогательные функции:
 *
 * - hyscan_object_copy() - копирование структуры,
 * - hyscan_object_equal() - сравнение значений двух структур,
 * - hyscan_object_free() - удаление структуры.
 *
 * Структура #HyScanObjectId используется для идентификации объекта в хранилище -
 * она содержит его тип и строковый идентификатор. Функции для работы с HyScanObjectId:
 *
 * - hyscan_object_id_new() - создание структуры,
 * - hyscan_object_id_free() - удаление структуры.
 *
 */

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
