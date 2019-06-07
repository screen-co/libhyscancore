#ifndef __HYSCAN_MARK_H__
#define __HYSCAN_MARK_H__

#include <hyscan-types.h>
#include <hyscan-param-list.h>
#include <hyscan-geo.h>

typedef enum _HyScanMarkType HyScanMarkType;
typedef union _HyScanMark HyScanMark;
typedef struct _HyScanMarkAny HyScanMarkAny;
typedef struct _HyScanMarkWaterfall HyScanMarkWaterfall;
typedef struct _HyScanMarkGeo HyScanMarkGeo;

enum _HyScanMarkType
{
  HYSCAN_MARK_WATERFALL,
  HYSCAN_MARK_GEO,
};

struct _HyScanMarkAny
{
  HyScanMarkType    type;              /**< Тип метки. */
  gchar            *name;              /**< Название метки. */
  gchar            *description;       /**< Описание. */
  gchar            *operator_name;     /**< Имя оператора. */
  guint64           labels;            /**< Метки. */
  gint64            creation_time;     /**< Время создания. */
  gint64            modification_time; /**< Время последней модификации. */
  guint32           width;             /**< Ширина. */
  guint32           height;            /**< Высота. */
};

struct _HyScanMarkWaterfall
{
  HyScanMarkType    type;              /**< Тип метки. */
  gchar            *name;              /**< Название метки. */
  gchar            *description;       /**< Описание. */
  gchar            *operator_name;     /**< Имя оператора. */
  guint64           labels;            /**< Метки. */
  gint64            creation_time;     /**< Время создания. */
  gint64            modification_time; /**< Время последней модификации. */
  guint32           width;             /**< Ширина. */
  guint32           height;            /**< Высота. */

  gchar            *track;             /**< Идентификатор галса. */
  HyScanSourceType  source0;           /**< Источник данных. */
  guint32           index0;            /**< Индекс данных. */
  guint32           count0;            /**< Отсчёт в строке. */
};

struct _HyScanMarkGeo
{
  HyScanMarkType    type;              /**< Тип метки. */
  gchar            *name;              /**< Название метки. */
  gchar            *description;       /**< Описание. */
  gchar            *operator_name;     /**< Имя оператора. */
  guint64           labels;            /**< Метки. */
  gint64            creation_time;     /**< Время создания. */
  gint64            modification_time; /**< Время последней модификации. */
  guint32           width;             /**< Ширина. */
  guint32           height;            /**< Высота. */

  HyScanGeoGeodetic center;            /**< Местоположение. */
};

union _HyScanMark
{
  HyScanMarkType        type;          /**< Тип метки. */
  HyScanMarkAny         any;           /**< Общие поля меток. */
  HyScanMarkWaterfall   waterfall;     /**< Метка водопада. */
  HyScanMarkGeo         geo;           /**< Геометка. */
};

HYSCAN_API
HyScanMark            *hyscan_mark_new                              (void);

HYSCAN_API
HyScanMark            *hyscan_mark_copy                             (HyScanMark            *mark);

HYSCAN_API
void                   hyscan_mark_free                             (HyScanMark            *mark);

HYSCAN_API
void                   hyscan_mark_waterfall_set_track              (HyScanMarkWaterfall   *mark,
                                                                     const gchar           *track);
HYSCAN_API
void                   hyscan_mark_set_text                         (HyScanMark            *mark,
                                                                     const gchar           *name,
                                                                     const gchar           *description,
                                                                     const gchar           *oper);
HYSCAN_API
void                   hyscan_mark_set_labels                       (HyScanMark            *mark,
                                                                     guint64                labels);

HYSCAN_API
void                   hyscan_mark_set_ctime                        (HyScanMark            *mark,
                                                                     gint64                 creation);
HYSCAN_API
void                   hyscan_mark_set_mtime                        (HyScanMark            *mark,
                                                                     gint64                 modification);
HYSCAN_API
void                   hyscan_mark_waterfall_set_center             (HyScanMarkWaterfall   *mark,
                                                                     HyScanSourceType       source,
                                                                     guint32                index,
                                                                     guint32                count);
HYSCAN_API
void                   hyscan_mark_geo_set_center                   (HyScanMarkGeo         *mark,
                                                                     HyScanGeoGeodetic      center);
HYSCAN_API
void                   hyscan_mark_set_size                         (HyScanMark            *mark,
                                                                     guint32                width,
                                                                     guint32                height);
G_END_DECLS

#endif /* __HYSCAN_MARK_H__ */
