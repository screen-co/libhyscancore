/**
 *
 * \file hyscan-mark-model.h
 *
 * \brief Заголовочный файл класса асинхронной работы с метками водопада.
 * \author Dmitriev Alexander (m1n7@yandex.ru)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanMarkModel HyScanMarkModel - класс асинхронной работы с метками водопада.
 *
 * HyScanMarkModel - класс асинхронной работы с метками режима водопад.
 * Он представляет собой обертку над HyScanMarkData. Класс предоставляет
 * все методы, необходимые для создания, изменения и удаления меток.
 *
 * Сигнал "changed" сигнализирует о том, что есть изменения в списке меток.
 * Прямо в обработчике сигнала можно получить актуальный список меток.
 * \code
 * void
 * changed_cb (HyScanMarkModel *man,
 *             gpointer         userdata);
 * \endcode
 *
 * Класс полностью потокобезопасен и может использоваться в MainLoop.
 *
 */
#ifndef __HYSCAN_MARK_MODEL_H__
#define __HYSCAN_MARK_MODEL_H__

#include "hyscan-mark-data.h"
#include "hyscan-mark.h"

G_BEGIN_DECLS

#define HYSCAN_TYPE_MARK_MODEL             (hyscan_mark_model_get_type ())
#define HYSCAN_MARK_MODEL(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_MARK_MODEL, HyScanMarkModel))
#define HYSCAN_IS_MARK_MODEL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_MARK_MODEL))
#define HYSCAN_MARK_MODEL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_MARK_MODEL, HyScanMarkModelClass))
#define HYSCAN_IS_MARK_MODEL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_MARK_MODEL))
#define HYSCAN_MARK_MODEL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_MARK_MODEL, HyScanMarkModelClass))

typedef struct _HyScanMarkModel HyScanMarkModel;
typedef struct _HyScanMarkModelPrivate HyScanMarkModelPrivate;
typedef struct _HyScanMarkModelClass HyScanMarkModelClass;

struct _HyScanMarkModel
{
  GObject parent_instance;

  HyScanMarkModelPrivate *priv;
};

struct _HyScanMarkModelClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                   hyscan_mark_model_get_type        (void);

/**
 * Функция создает новый объект HyScanMarkModel.
 *
 * \param data_type тип класса работы с метками.
 *
 * \return объект HyScanMarkModel.
 */
HYSCAN_API
HyScanMarkModel*        hyscan_mark_model_new             (HyScanMarkType             mark_type);

/**
 * Функция устанавливает проект.
 *
 * \param model указатель на \link HyScanMarkModel \endlink;
 * \param project имя проекта.
 *
 */
HYSCAN_API
void                    hyscan_mark_model_set_project     (HyScanMarkModel           *model,
                                                           HyScanDB                  *db,
                                                           const gchar               *project);

/**
 * Функция инициирует принудительное обновление списка меток.
 *
 * \param model указатель на \link HyScanMarkModel \endlink.
 *
 */
HYSCAN_API
void                    hyscan_mark_model_refresh         (HyScanMarkModel           *model);

/**
 * Функция создает метку в базе данных.
 *
 * \param model указатель на \link HyScanMarkModel \endlink;
 * \param mark создаваемая метка.
 *
 */
HYSCAN_API
void                    hyscan_mark_model_add_mark        (HyScanMarkModel           *model,
                                                           const HyScanMark          *mark);

/**
 * Функция изменяет метку в базе данных.
 * В результате этой функции все поля метки будут перезаписаны.
 *
 * \param model указатель на \link HyScanMarkModel \endlink;
 * \param id идентификатор метки;
 * \param mark новые значения.
 *
 */
HYSCAN_API
void                    hyscan_mark_model_modify_mark     (HyScanMarkModel           *model,
                                                           const gchar               *id,
                                                           const HyScanMark          *mark);

/**
 * Функция удаляет метку из базы данных.
 *
 * \param model указатель на \link HyScanMarkModel \endlink;
 * \param id идентификатор метки.
 *
 */
HYSCAN_API
void                    hyscan_mark_model_remove_mark     (HyScanMarkModel           *model,
                                                           const gchar               *id);

/**
 * Функция возвращает список меток из внутреннего буфера.
 *
 * \param model указатель на \link HyScanMarkModel \endlink;
 *
 * \return GHashTable, где ключом является идентификатор метки,
 * а значением - структура HyScanMark.
 */
HYSCAN_API
GHashTable*             hyscan_mark_model_get             (HyScanMarkModel           *model);

/**
 * Вспомогательная функция для создания копии таблицы меток.
 */
HYSCAN_API
GHashTable*             hyscan_mark_model_copy            (GHashTable                *marks);

G_END_DECLS

#endif /* __HYSCAN_MARK_MODEL_H__ */
