/**
 *
 * \file hyscan-object-model.h
 *
 * \brief Заголовочный файл класса асинхронной работы с метками водопада.
 * \author Dmitriev Alexander (m1n7@yandex.ru)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanObjectModel HyScanObjectModel - класс асинхронной работы с метками водопада.
 *
 * HyScanObjectModel - класс асинхронной работы с метками режима водопад.
 * Он представляет собой обертку над HyScanObjectData. Класс предоставляет
 * все методы, необходимые для создания, изменения и удаления меток.
 *
 * Сигнал "changed" сигнализирует о том, что есть изменения в списке меток.
 * Прямо в обработчике сигнала можно получить актуальный список меток.
 * \code
 * void
 * changed_cb (HyScanObjectModel *man,
 *             gpointer           userdata);
 * \endcode
 *
 * Класс полностью потокобезопасен и может использоваться в MainLoop.
 *
 */
#ifndef __HYSCAN_OBJECT_MODEL_H__
#define __HYSCAN_OBJECT_MODEL_H__

#include <hyscan-object-data.h>
#include <hyscan-mark-data-geo.h>
#include <hyscan-mark-data-waterfall.h>
#include <hyscan-planner-data.h>
#include <hyscan-mark.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_OBJECT_MODEL             (hyscan_object_model_get_type ())
#define HYSCAN_OBJECT_MODEL(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_OBJECT_MODEL, HyScanObjectModel))
#define HYSCAN_IS_OBJECT_MODEL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_OBJECT_MODEL))
#define HYSCAN_OBJECT_MODEL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_OBJECT_MODEL, HyScanObjectModelClass))
#define HYSCAN_IS_OBJECT_MODEL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_OBJECT_MODEL))
#define HYSCAN_OBJECT_MODEL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_OBJECT_MODEL, HyScanObjectModelClass))

typedef struct _HyScanObjectModel HyScanObjectModel;
typedef struct _HyScanObjectModelPrivate HyScanObjectModelPrivate;
typedef struct _HyScanObjectModelClass HyScanObjectModelClass;

struct _HyScanObjectModel
{
  GObject parent_instance;

  HyScanObjectModelPrivate *priv;
};

struct _HyScanObjectModelClass
{
  GObjectClass parent_class;

  /* Сигналы. */
  void         (*changed)             (HyScanObjectModel  *model);
};

HYSCAN_API
GType                   hyscan_object_model_get_type          (void);

/**
 * Функция создает новый объект HyScanObjectModel.
 *
 * \param data_type тип класса работы с метками.
 *
 * \return объект HyScanObjectModel.
 */
HYSCAN_API
HyScanObjectModel*        hyscan_object_model_new             (GType                      data_type);

/**
 * Функция устанавливает проект.
 *
 * \param model указатель на \link HyScanObjectModel \endlink;
 * \param project имя проекта.
 *
 */
HYSCAN_API
void                    hyscan_object_model_set_project       (HyScanObjectModel         *model,
                                                               HyScanDB                  *db,
                                                               const gchar               *project);

/**
 * Функция инициирует принудительное обновление списка меток.
 *
 * \param model указатель на \link HyScanObjectModel \endlink.
 *
 */
HYSCAN_API
void                    hyscan_object_model_refresh           (HyScanObjectModel         *model);

/**
 * Функция создает метку в базе данных.
 *
 * \param model указатель на \link HyScanObjectModel \endlink;
 * \param object создаваемая метка.
 *
 */
HYSCAN_API
void                    hyscan_object_model_add_object        (HyScanObjectModel         *model,
                                                               const HyScanObject        *object);

/**
 * Функция изменяет метку в базе данных.
 * В результате этой функции все поля метки будут перезаписаны.
 *
 * \param model указатель на \link HyScanObjectModel \endlink;
 * \param id идентификатор метки;
 * \param object новые значения.
 *
 */
HYSCAN_API
void                    hyscan_object_model_modify_object     (HyScanObjectModel         *model,
                                                               const gchar               *id,
                                                               const HyScanObject        *object);

/**
 * Функция удаляет метку из базы данных.
 *
 * \param model указатель на \link HyScanObjectModel \endlink;
 * \param id идентификатор метки.
 *
 */
HYSCAN_API
void                    hyscan_object_model_remove_object     (HyScanObjectModel         *model,
                                                               const gchar               *id);

/**
 * Функция возвращает список меток из внутреннего буфера.
 *
 * \param model указатель на \link HyScanObjectModel \endlink;
 *
 * \return GHashTable, где ключом является идентификатор метки,
 * а значением - структура HyScanObject.
 */
HYSCAN_API
GHashTable*             hyscan_object_model_get               (HyScanObjectModel         *model);

/**
 * Функция возвращает копию объекта по его ID из внутреннего буфера.
 *
 * \param model указатель на \link HyScanObjectModel \endlink;
 *
 * \return HyScanObject или NULL.
 */
HYSCAN_API
HyScanObject *          hyscan_object_model_get_id            (HyScanObjectModel         *model,
                                                               const gchar               *id);

/**
 * Вспомогательная функция для создания копии таблицы меток.
 */
HYSCAN_API
GHashTable*             hyscan_object_model_copy              (HyScanObjectModel         *model,
                                                               GHashTable                *objects);

G_END_DECLS

#endif /* __HYSCAN_OBJECT_MODEL_H__ */
