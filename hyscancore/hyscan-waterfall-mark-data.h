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
 * Метка однозначно определяется её идентификатором. Все операции (кроме добавления меток)
 * требуют специально указанного параметра id. Здесь нужно сделать важное замечание:
 *
 * \warning Относительно методов #hyscan_waterfall_mark_data_add и #hyscan_waterfall_mark_data_modify:
 * id, указанный внутри переданной структуры, будет полностью проигнорирован! Значение имеет только
 * идентификатор, переданный как параметр!
 *
 *
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

HYSCAN_API
HyScanWaterfallMarkData        *hyscan_waterfall_mark_data_new               (HyScanDB                   *db,
                                                                              const gchar                *project,
                                                                              const gchar                *track,
                                                                              const gchar                *profile);

HYSCAN_API
gboolean                        hyscan_waterfall_mark_data_add               (HyScanWaterfallMarkData    *data,
                                                                              HyScanWaterfallMark        *mark);

HYSCAN_API
gboolean                        hyscan_waterfall_mark_data_add_full          (HyScanWaterfallMarkData    *data,
                                                                              gchar                      *name,
                                                                              gchar                      *description,
                                                                              gchar                      *operator_name,
                                                                              guint64                     labels,
                                                                              gint64                      creation_time,
                                                                              gint64                      modification_time,
                                                                              HyScanSourceType            source0,
                                                                              guint32                     index0,
                                                                              guint32                     count0,
                                                                              HyScanSourceType            source1,
                                                                              guint32                     index1,
                                                                              guint32                     count1);

HYSCAN_API
gboolean                        hyscan_waterfall_mark_data_remove            (HyScanWaterfallMarkData    *data,
                                                                              const gchar                *id);

HYSCAN_API
gboolean                        hyscan_waterfall_mark_data_modify            (HyScanWaterfallMarkData    *data,
                                                                              const gchar                *id,
                                                                              HyScanWaterfallMark        *mark);

HYSCAN_API
gboolean                        hyscan_waterfall_mark_data_modify_full       (HyScanWaterfallMarkData    *data,
                                                                              const gchar                *id,
                                                                              gchar                      *name,
                                                                              gchar                      *description,
                                                                              gchar                      *operator_name,
                                                                              guint64                     labels,
                                                                              gint64                      creation_time,
                                                                              gint64                      modification_time,
                                                                              HyScanSourceType            source0,
                                                                              guint32                     index0,
                                                                              guint32                     count0,
                                                                              HyScanSourceType            source1,
                                                                              guint32                     index1,
                                                                              guint32                     count1);


HYSCAN_API
gchar                         **hyscan_waterfall_mark_data_get_ids           (HyScanWaterfallMarkData    *data,
                                                                              guint                      *len);
HYSCAN_API
HyScanWaterfallMark            *hyscan_waterfall_mark_data_get               (HyScanWaterfallMarkData    *data,
                                                                              const gchar                *id);
HYSCAN_API
gboolean                        hyscan_waterfall_mark_data_get_full          (HyScanWaterfallMarkData    *data,
                                                                              const gchar                *id,
                                                                              HyScanWaterfallMark        *mark);

HYSCAN_API
guint32                         hyscan_waterfall_mark_data_get_mod_count     (HyScanWaterfallMarkData    *data);

G_END_DECLS

#endif /* __HYSCAN_WATERFALL_MARK_DATA_H__ */
