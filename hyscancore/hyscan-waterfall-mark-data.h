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
#include <hyscan-types.h>
#include <hyscan-mark-data.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_WATERFALL_MARK_DATA             (hyscan_waterfall_mark_data_get_type ())
#define HYSCAN_WATERFALL_MARK_DATA(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_WATERFALL_MARK_DATA, HyScanWaterfallMarkData))
#define HYSCAN_IS_WATERFALL_MARK_DATA(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_WATERFALL_MARK_DATA))
#define HYSCAN_WATERFALL_MARK_DATA_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_WATERFALL_MARK_DATA, HyScanWaterfallMarkDataClass))
#define HYSCAN_IS_WATERFALL_MARK_DATA_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_WATERFALL_MARK_DATA))
#define HYSCAN_WATERFALL_MARK_DATA_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_WATERFALL_MARK_DATA, HyScanWaterfallMarkDataClass))

typedef struct _HyScanWaterfallMarkData HyScanWaterfallMarkData;
typedef struct _HyScanWaterfallMarkDataClass HyScanWaterfallMarkDataClass;

struct _HyScanWaterfallMarkData
{
  HyScanMarkData parent_instance;
};

struct _HyScanWaterfallMarkDataClass
{
  HyScanMarkDataClass parent_class;
};

HYSCAN_API
GType                           hyscan_waterfall_mark_data_get_type          (void);

G_END_DECLS

#endif /* __HYSCAN_WATERFALL_MARK_DATA_H__ */
