#ifndef __HYSCAN_WATERFALL_MARK_H__
#define __HYSCAN_WATERFALL_MARK_H__

#include <hyscan-types.h>
#include <hyscan-param-list.h>

typedef struct _HyScanWaterfallMark HyScanWaterfallMark;

struct _HyScanWaterfallMark
{
  gchar            *track;             /**< Идентификатор галса. */
  gchar            *name;              /**< Название метки. */
  gchar            *description;       /**< Описание. */
  gchar            *operator_name;     /**< Имя оператора. */
  guint64           labels;            /**< Метки. */
  gint64            creation_time;     /**< Время создания. */
  gint64            modification_time; /**< Время последней модификации. */
  HyScanSourceType  source0;           /**< Источник данных. */
  guint32           index0;            /**< Индекс данных. */
  guint32           count0;            /**< Отсчёт в строке. */
  guint32           width;             /**< Ширина. */
  guint32           height;            /**< Высота. */
};

HYSCAN_API
HyScanWaterfallMark   *hyscan_waterfall_mark_new                    (void);

HYSCAN_API
HyScanWaterfallMark   *hyscan_waterfall_mark_copy                   (HyScanWaterfallMark  *mark);

HYSCAN_API
void                   hyscan_waterfall_mark_free                   (HyScanWaterfallMark  *mark);

HYSCAN_API
void                   hyscan_waterfall_mark_set_track              (HyScanWaterfallMark   *mark,
                                                                     const gchar           *track);
HYSCAN_API
void                   hyscan_waterfall_mark_set_text               (HyScanWaterfallMark   *mark,
                                                                     const gchar           *name,
                                                                     const gchar           *description,
                                                                     const gchar           *oper);
HYSCAN_API
void                   hyscan_waterfall_mark_set_labels             (HyScanWaterfallMark   *mark,
                                                                     guint64                labels);

HYSCAN_API
void                   hyscan_waterfall_mark_set_ctime              (HyScanWaterfallMark   *mark,
                                                                     gint64                 creation);
HYSCAN_API
void                   hyscan_waterfall_mark_set_mtime              (HyScanWaterfallMark   *mark,
                                                                     gint64                 modification);
HYSCAN_API
void                   hyscan_waterfall_mark_set_center             (HyScanWaterfallMark   *mark,
                                                                     HyScanSourceType       source,
                                                                     guint32                index,
                                                                     guint32                count);
HYSCAN_API
void                   hyscan_waterfall_mark_set_size               (HyScanWaterfallMark   *mark,
                                                                     guint32                width,
                                                                     guint32                height);
G_END_DECLS

#endif /* __HYSCAN_WATERFALL_MARK_H__ */
