/**
 *
 * \file hyscan-waterfall-mark-data.h
 *
 * \brief Заголовочный файл класса работы с метками водопада.
 * \author Dmitriev Alexander (m1n7@yandex.ru)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanWaterfallMarkData HyScanWaterfallMarkData - базовый класс работы с метками режима водопад.
 *
 * HyScanWaterfallMarkData - базовый класс работы с метками режима водопад. Он представляет собой
 * обертку над базой данных, что позволяет потребителю работать не с конкретными записями в базе
 * данных, а с метками и их идентификаторами.
 *
 * Класс решает задачи добавления, удаления, модификации и получения меток.
 *
 * Класс не потокобезопасен.
 */

#ifndef __HYSCAN_WATERFALL_MARK_DATA_H__
#define __HYSCAN_WATERFALL_MARK_DATA_H__

#include <hyscan-db.h>
#include <hyscan-core-types.h>
#include <hyscan-waterfall-mark.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_WATERFALL_MARK_DATA             (hyscan_waterfall_mark_data_get_type ())
#define HYSCAN_WATERFALL_MARK_DATA(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_WATERFALL_MARK_DATA, HyScanWaterfallMarkData))
#define HYSCAN_IS_WATERFALL_MARK_DATA(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_WATERFALL_MARK_DATA))
#define HYSCAN_WATERFALL_MARK_DATA_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_WATERFALL_MARK_DATA, HyScanWaterfallMarkDataClass))
#define HYSCAN_IS_WATERFALL_MARK_DATA_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_WATERFALL_MARK_DATA))
#define HYSCAN_WATERFALL_MARK_DATA_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_WATERFALL_MARK_DATA, HyScanWaterfallMarkDataClass))

typedef struct _HyScanWaterfallMarkData HyScanWaterfallMarkData;
typedef struct _HyScanWaterfallMarkDataPrivate HyScanWaterfallMarkDataPrivate;
typedef struct _HyScanWaterfallMarkDataClass HyScanWaterfallMarkDataClass;

struct _HyScanWaterfallMarkData
{
  GObject parent_instance;

  HyScanWaterfallMarkDataPrivate *priv;
};

struct _HyScanWaterfallMarkDataClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                           hyscan_waterfall_mark_data_get_type          (void);

/**
 *
 * Функция создает новый объект работы с метками.
 *
 * \param db указатель на объект \link HyScanDB \endlink;
 * \param project имя проекта;
 *
 * \return Указатель на объект \link HyScanWaterfallMarkData \endlink или NULL.
 *
 */
HYSCAN_API
HyScanWaterfallMarkData        *hyscan_waterfall_mark_data_new               (HyScanDB                   *db,
                                                                              const gchar                *project);

/**
 *
 * Функция добавляет метку в базу данных.
 *
 * \param data указатель на объект \link HyScanWaterfallMarkData \endlink;
 * \param mark указатель на структуру \link HyScanWaterfallMark \endlink.
 *
 * \return TRUE, если удалось добавить метку, иначе FALSE.
 */
HYSCAN_API
gboolean                        hyscan_waterfall_mark_data_add               (HyScanWaterfallMarkData    *data,
                                                                              HyScanWaterfallMark        *mark);

/**
 *
 * Функция удаляет метку из базы данных.
 *
 * \param data указатель на объект \link HyScanWaterfallMarkData \endlink;
 * \param id идентификатор метки.
 *
 * \return TRUE, если удалось удалить метку, иначе FALSE.
 */
HYSCAN_API
gboolean                        hyscan_waterfall_mark_data_remove            (HyScanWaterfallMarkData    *data,
                                                                              const gchar                *id);

/**
 *
 * Функция изменяет метку.
 *
 * \param data указатель на объект \link HyScanWaterfallMarkData \endlink;
 * \param id идентификатор метки;
 * \param mark указатель на структуру \link HyScanWaterfallMark \endlink.
 *
 * \return TRUE, если удалось изменить метку, иначе FALSE.
 */
HYSCAN_API
gboolean                        hyscan_waterfall_mark_data_modify            (HyScanWaterfallMarkData    *data,
                                                                              const gchar                *id,
                                                                              HyScanWaterfallMark        *mark);

/**
 *
 * Функция возвращает список идентификаторов всех меток.
 *
 * \param data указатель на объект \link HyScanWaterfallMarkData \endlink;
 * \param len количество элементов или NULL.
 *
 * \return NULL-терминированный список идентификаторов, NULL если меток нет.
 */
HYSCAN_API
gchar                         **hyscan_waterfall_mark_data_get_ids           (HyScanWaterfallMarkData    *data,
                                                                              guint                      *len);
/**
 *
 * Функция возвращает метку по идентификатору.
 *
 * \param data указатель на объект \link HyScanWaterfallMarkData \endlink;
 * \param id идентификатор метки.
 *
 * \return указатель на структуру \link HyScanWaterfallMark \endlink, NULL в случае ошибки.
 */
HYSCAN_API
HyScanWaterfallMark            *hyscan_waterfall_mark_data_get               (HyScanWaterfallMarkData    *data,
                                                                              const gchar                *id);
/**
 *
 * Функция возвращает счётчик изменений.
 *
 * \param data указатель на объект \link HyScanWaterfallMarkData \endlink;
 *
 * \return номер изменения.
 */
HYSCAN_API
guint32                         hyscan_waterfall_mark_data_get_mod_count     (HyScanWaterfallMarkData    *data);

G_END_DECLS

#endif /* __HYSCAN_WATERFALL_MARK_DATA_H__ */
