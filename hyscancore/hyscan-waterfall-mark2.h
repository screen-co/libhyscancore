#ifndef __HYSCAN_WATERFALL_MARK2_H__
#define __HYSCAN_WATERFALL_MARK2_H__

#include <hyscan-core-types.h>
#include <glib-object.h>

typedef struct
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
} HyScanWaterfallMark2;

HyScanWaterfallMark2  *hyscan_waterfall_mark2_new                   (void);

/* Full */
const gchar *          hyscan_waterfall_mark2_get_track             (HyScanWaterfallMark2 *mark);
void                   hyscan_waterfall_mark2_set_track             (HyScanWaterfallMark2 *mark,
                                                                     const gchar          *track);
const gchar *          hyscan_waterfall_mark2_get_name              (HyScanWaterfallMark2 *mark);
void                   hyscan_waterfall_mark2_set_name              (HyScanWaterfallMark2 *mark,
                                                                     const gchar          *name);
const gchar *          hyscan_waterfall_mark2_get_description       (HyScanWaterfallMark2 *mark);
void                   hyscan_waterfall_mark2_set_description       (HyScanWaterfallMark2 *mark,
                                                                     const gchar          *description);
const gchar *          hyscan_waterfall_mark2_get_operator_name     (HyScanWaterfallMark2 *mark);
void                   hyscan_waterfall_mark2_set_operator_name     (HyScanWaterfallMark2 *mark,
                                                                     const gchar          *operator_name);
guint64                hyscan_waterfall_mark2_get_labels            (HyScanWaterfallMark2 *mark);
void                   hyscan_waterfall_mark2_set_labels            (HyScanWaterfallMark2 *mark,
                                                                     guint64               labels);
gint64                 hyscan_waterfall_mark2_get_creation_time     (HyScanWaterfallMark2 *mark);
void                   hyscan_waterfall_mark2_set_creation_time     (HyScanWaterfallMark2 *mark,
                                                                     gint64                creation_time);
gint64                 hyscan_waterfall_mark2_get_modification_time (HyScanWaterfallMark2 *mark);
void                   hyscan_waterfall_mark2_set_modification_time (HyScanWaterfallMark2 *mark,
                                                                     gint64                modification_time);
HyScanSourceType       hyscan_waterfall_mark2_get_source0           (HyScanWaterfallMark2 *mark);
void                   hyscan_waterfall_mark2_set_source0           (HyScanWaterfallMark2 *mark,
                                                                     HyScanSourceType      source0);
guint32                hyscan_waterfall_mark2_get_index0            (HyScanWaterfallMark2 *mark);
void                   hyscan_waterfall_mark2_set_index0            (HyScanWaterfallMark2 *mark,
                                                                     guint32               index0);
guint32                hyscan_waterfall_mark2_get_count0            (HyScanWaterfallMark2 *mark);
void                   hyscan_waterfall_mark2_set_count0            (HyScanWaterfallMark2 *mark,
                                                                     guint32               count0);
guint32                hyscan_waterfall_mark2_get_width             (HyScanWaterfallMark2 *mark);
void                   hyscan_waterfall_mark2_set_width             (HyScanWaterfallMark2 *mark,
                                                                     guint32               width);
guint32                hyscan_waterfall_mark2_get_height            (HyScanWaterfallMark2 *mark);
void                   hyscan_waterfall_mark2_set_height            (HyScanWaterfallMark2 *mark,
                                                                     guint32               height);

/* Reduced */
void                   hyscan_waterfall_mark2_get_track             (HyScanWaterfallMark2  *mark,
                                                                     const gchar          **track);
void                   hyscan_waterfall_mark2_set_track             (HyScanWaterfallMark2  *mark,
                                                                     const gchar           *track);
void                   hyscan_waterfall_mark2_get_labels            (HyScanWaterfallMark2  *mark,
                                                                     guint64               *labels);
void                   hyscan_waterfall_mark2_set_labels            (HyScanWaterfallMark2  *mark,
                                                                     guint64                labels);
void                   hyscan_waterfall_mark2_get_text              (HyScanWaterfallMark2  *mark,
                                                                     const gchar          **name,
                                                                     const gchar          **description,
                                                                     const gchar          **oper);
void                   hyscan_waterfall_mark2_set_text              (HyScanWaterfallMark2  *mark,
                                                                     const gchar           *name,
                                                                     const gchar           *description,
                                                                     const gchar           *oper);
void                   hyscan_waterfall_mark2_get_time              (HyScanWaterfallMark2  *mark,
                                                                     gint64                *creation,
                                                                     gint64                *modification);
void                   hyscan_waterfall_mark2_set_time              (HyScanWaterfallMark2  *mark,
                                                                     gint64                 creation,
                                                                     gint64                 modification);
void                   hyscan_waterfall_mark2_get_center            (HyScanWaterfallMark2  *mark,
                                                                     HyScanSourceType      *source,
                                                                     guint32               *index,
                                                                     guint32               *count);
void                   hyscan_waterfall_mark2_set_center            (HyScanWaterfallMark2  *mark,
                                                                     HyScanSourceType       source,
                                                                     guint32                index,
                                                                     guint32                count);
void                   hyscan_waterfall_mark2_get_size              (HyScanWaterfallMark2  *mark,
                                                                     guint32               *width,
                                                                     guint32               *height);
void                   hyscan_waterfall_mark2_get_size              (HyScanWaterfallMark2  *mark,
                                                                     guint32               *width,
                                                                     guint32               *height);


G_END_DECLS

#endif /* __HYSCAN_WATERFALL_MARK2_H__ */
