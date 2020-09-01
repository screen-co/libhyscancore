#ifndef __HYSCAN_OBJECT_H__
#define __HYSCAN_OBJECT_H__

#include <hyscan-types.h>

G_BEGIN_DECLS

/**
 * HYSCAN_OBJECT_DEFINE_EQUAL_FUNC:
 * @func: #GEqualFunc функция для сравнения объектов данного типа
 *
 * Макрос для удобной регистрации функции сравнения объектов #HyScanObject в `_C_` разделе
 * G_DEFINE_BOXED_TYPE_WITH_CODE().
 *
 * Пример использования:
 *
 * |[<!-- language="C" -->
 * G_DEFINE_BOXED_TYPE_WITH_CODE (HyScanFoo,
 *                                hyscan_foo,
 *                                hyscan_foo_copy,
 *                                hyscan_foo_free,
 *                                HYSCAN_OBJECT_DEFINE_EQUAL_FUNC (hyscan_foo_equal));
 * ]|
 */
#define HYSCAN_OBJECT_DEFINE_EQUAL_FUNC(equal_func) \
g_type_set_qdata (g_define_type_id, hyscan_object_equal_func_quark(), (equal_func));

typedef struct _HyScanObjectId HyScanObjectId;
typedef struct _HyScanObject HyScanObject;

/**
 * HyScanObjectId:
 * @type: тип GBoxed
 * @id: строковый идентификатор объекта
 *
 * Структура для однозначной идентификации объекта по его типу и строковому
 * идентификатору.
 */
struct _HyScanObjectId
{
  GType        type;
  gchar       *id;
};

/**
 * HyScanObject:
 * @type: тип GBoxed
 *
 * Все структуры, которые загружаются при помощи #HyScanObjectData, должны быть
 * зарегистрированы как типы GBoxed, а в поле type хранить идентификатор своего типа GType.
 * При передаче структуры в функции hyscan_object_data_add(), hyscan_object_data_modify()
 * и подобные, структура должна быть приведена к типу #HyScanObject.
 */
struct _HyScanObject
{
  GType                type;
};

HYSCAN_API
GQuark                 hyscan_object_equal_func_quark          (void);

HYSCAN_API
HyScanObjectId *       hyscan_object_id_new                    (void);

HYSCAN_API
void                   hyscan_object_id_free                   (HyScanObjectId       *object_id);

HYSCAN_API
HyScanObject *         hyscan_object_copy                      (const HyScanObject *object);

HYSCAN_API
gboolean               hyscan_object_equal                     (const HyScanObject *object1,
                                                                const HyScanObject *object2);

HYSCAN_API
void                   hyscan_object_free                      (HyScanObject       *object);

G_END_DECLS

#endif /* __HYSCAN_OBJECT_H__ */
