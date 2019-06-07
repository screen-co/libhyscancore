/**
 *
 * \file hyscan-mark-data.h
 *
 * \brief Заголовочный файл класса работы с метками водопада.
 * \author Dmitriev Alexander (m1n7@yandex.ru)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanMarkData HyScanMarkData - базовый класс работы с метками режима водопад.
 *
 * HyScanMarkData - базовый класс работы с метками режима водопад. Он представляет собой
 * обертку над базой данных, что позволяет потребителю работать не с конкретными записями в базе
 * данных, а с метками и их идентификаторами.
 *
 * Класс решает задачи добавления, удаления, модификации и получения меток.
 *
 * Класс не потокобезопасен.
 */

#ifndef __HYSCAN_MARK_DATA_H__
#define __HYSCAN_MARK_DATA_H__

#include <hyscan-db.h>
#include <hyscan-types.h>
#include <hyscan-mark.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_MARK_DATA             (hyscan_mark_data_get_type ())
#define HYSCAN_MARK_DATA(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_MARK_DATA, HyScanMarkData))
#define HYSCAN_IS_MARK_DATA(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_MARK_DATA))
#define HYSCAN_MARK_DATA_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_MARK_DATA, HyScanMarkDataClass))
#define HYSCAN_IS_MARK_DATA_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_MARK_DATA))
#define HYSCAN_MARK_DATA_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_MARK_DATA, HyScanMarkDataClass))

typedef struct _HyScanMarkData HyScanMarkData;
typedef struct _HyScanMarkDataPrivate HyScanMarkDataPrivate;
typedef struct _HyScanMarkDataClass HyScanMarkDataClass;

struct _HyScanMarkData
{
  GObject parent_instance;

  HyScanMarkDataPrivate *priv;
};

/**
 * HyScanMarkDataClass:
 * @init_plist: функция добавляет в список параметров дополнительные ключи
 * @get_schema: функция возвращает информацию о схеме данных (название, ИД, версия)
 * @get: функция считывает содержимое объекта
 * @set: функция записывает значения в существующий объект
 */
struct _HyScanMarkDataClass
{
  GObjectClass parent_class;

  void           (*init_plist)   (HyScanMarkData    *data,
                                  HyScanParamList   *plist);

  const gchar *  (*get_schema)   (HyScanMarkData    *data,
                                  gint64            *id,
                                  gint64            *version);

  void           (*get)          (HyScanMarkData    *data,
                                  HyScanParamList   *read_plist,
                                  HyScanMark        *mark);

  void           (*set)          (HyScanMarkData    *data,
                                  HyScanParamList   *write_plist,
                                  const HyScanMark  *mark);
};

HYSCAN_API
GType                           hyscan_mark_data_get_type          (void);

/**
 *
 * Функция добавляет метку в базу данных.
 *
 * \param data указатель на объект \link HyScanMarkData \endlink;
 * \param mark указатель на структуру \link HyScanMark \endlink.
 *
 * \return TRUE, если удалось добавить метку, иначе FALSE.
 */
HYSCAN_API
gboolean                        hyscan_mark_data_add               (HyScanMarkData    *data,
                                                                    HyScanMark        *mark);

/**
 *
 * Функция удаляет метку из базы данных.
 *
 * \param data указатель на объект \link HyScanMarkData \endlink;
 * \param id идентификатор метки.
 *
 * \return TRUE, если удалось удалить метку, иначе FALSE.
 */
HYSCAN_API
gboolean                        hyscan_mark_data_remove            (HyScanMarkData    *data,
                                                                    const gchar       *id);

/**
 *
 * Функция изменяет метку.
 *
 * \param data указатель на объект \link HyScanMarkData \endlink;
 * \param id идентификатор метки;
 * \param mark указатель на структуру \link HyScanMark \endlink.
 *
 * \return TRUE, если удалось изменить метку, иначе FALSE.
 */
HYSCAN_API
gboolean                        hyscan_mark_data_modify            (HyScanMarkData    *data,
                                                                    const gchar       *id,
                                                                    HyScanMark        *mark);

/**
 *
 * Функция возвращает список идентификаторов всех меток.
 *
 * \param data указатель на объект \link HyScanMarkData \endlink;
 * \param len количество элементов или NULL.
 *
 * \return NULL-терминированный список идентификаторов, NULL если меток нет.
 */
HYSCAN_API
gchar                         **hyscan_mark_data_get_ids           (HyScanMarkData    *data,
                                                                    guint             *len);
/**
 *
 * Функция возвращает метку по идентификатору.
 *
 * \param data указатель на объект \link HyScanMarkData \endlink;
 * \param id идентификатор метки.
 *
 * \return указатель на структуру \link HyScanMark \endlink, NULL в случае ошибки.
 */
HYSCAN_API
HyScanMark                     *hyscan_mark_data_get               (HyScanMarkData    *data,
                                                                    const gchar       *id);
/**
 *
 * Функция возвращает счётчик изменений.
 *
 * \param data указатель на объект \link HyScanMarkData \endlink;
 *
 * \return номер изменения.
 */
HYSCAN_API
guint32                         hyscan_mark_data_get_mod_count     (HyScanMarkData    *data);

G_END_DECLS

#endif /* __HYSCAN_MARK_DATA_H__ */
