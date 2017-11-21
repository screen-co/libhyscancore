/**
 *
 * \file hyscan-mark-manager.h
 *
 * \brief Заголовочный файл класса асинхронной работы с метками водопада.
 * \author Dmitriev Alexander (m1n7@yandex.ru)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanMarkManager HyScanMarkManager - класс асинхронной работы с метками водопада.
 *
 * HyScanMarkManager - класс асинхронной работы с метками режима водопад.
 * Он представляет собой обертку над HyScanWaterfallMarkData. Класс предоставляет
 * все методы, необходимые для создания, изменения и удаления меток.
 *
 * Сигнал "changed" сигнализирует о том, что есть изменения в списке меток.
 * Прямо в обработчике сигнала можно получить актуальный список меток.
 * \code
 * void
 * changed_cb (HyScanMarkManager *man,
 *             gpointer           userdata);
 * \endcode
 *
 * Класс полностью потокобезопасен и может использоваться в MainLoop.
 *
 */
#ifndef __HYSCAN_MARK_MANAGER_H__
#define __HYSCAN_MARK_MANAGER_H__

#include <hyscan-waterfall-mark-data.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_MARK_MANAGER             (hyscan_mark_manager_get_type ())
#define HYSCAN_MARK_MANAGER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_MARK_MANAGER, HyScanMarkManager))
#define HYSCAN_IS_MARK_MANAGER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_MARK_MANAGER))
#define HYSCAN_MARK_MANAGER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_MARK_MANAGER, HyScanMarkManagerClass))
#define HYSCAN_IS_MARK_MANAGER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_MARK_MANAGER))
#define HYSCAN_MARK_MANAGER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_MARK_MANAGER, HyScanMarkManagerClass))

typedef struct _HyScanMarkManager HyScanMarkManager;
typedef struct _HyScanMarkManagerPrivate HyScanMarkManagerPrivate;
typedef struct _HyScanMarkManagerClass HyScanMarkManagerClass;

struct _HyScanMarkManager
{
  GObject parent_instance;

  HyScanMarkManagerPrivate *priv;
};

struct _HyScanMarkManagerClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                   hyscan_mark_manager_get_type      (void);

/**
 * Функция создает новый объект HyScanMarkManager.
 *
 * \param db указатель на \link HyScanDB \endlink.
 *
 * \return объект HyScanMarkManager.
 */
HYSCAN_API
HyScanMarkManager*      hyscan_mark_manager_new           (void);

/**
 * Функция устанавливает проект.
 *
 * \param manager указатель на \link HyScanMarkManager \endlink;
 * \param project имя проекта.
 *
 */
HYSCAN_API
void                    hyscan_mark_manager_set_project   (HyScanMarkManager         *manager,
                                                           HyScanDB                  *db,
                                                           const gchar               *project);

/**
 * Функция инициирует принудительное обновление списка меток.
 *
 * \param manager указатель на \link HyScanMarkManager \endlink.
 *
 */
HYSCAN_API
void                    hyscan_mark_manager_refresh       (HyScanMarkManager         *manager);

/**
 * Функция создает метку в базе данных.
 *
 * \param manager указатель на \link HyScanMarkManager \endlink;
 * \param mark создаваемая метка.
 *
 */
HYSCAN_API
void                    hyscan_mark_manager_add_mark      (HyScanMarkManager         *manager,
                                                           const HyScanWaterfallMark *mark);

/**
 * Функция изменяет метку в базе данных.
 * В результате этой функции все поля метки будут перезаписаны.
 *
 * \param manager указатель на \link HyScanMarkManager \endlink;
 * \param id идентификатор метки;
 * \param mark новые значения.
 *
 */
HYSCAN_API
void                    hyscan_mark_manager_modify_mark   (HyScanMarkManager         *manager,
                                                           const gchar               *id,
                                                           const HyScanWaterfallMark *mark);

/**
 * Функция удаляет метку из базы данных.
 *
 * \param manager указатель на \link HyScanMarkManager \endlink;
 * \param id идентификатор метки.
 *
 */
HYSCAN_API
void                    hyscan_mark_manager_remove_mark   (HyScanMarkManager         *manager,
                                                           const gchar               *id);

/**
 * Функция возвращает список меток из внутреннего буфера.
 *
 * \param manager указатель на \link HyScanMarkManager \endlink;
 *
 * \return GHashTable, где ключом является идентификатор метки,
 * а значением - структура HyScanWaterfallMark.
 */
HYSCAN_API
GHashTable*             hyscan_mark_manager_get           (HyScanMarkManager         *manager);

G_END_DECLS

#endif /* __HYSCAN_MARK_MANAGER_H__ */
