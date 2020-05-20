/*
 * hyscan-label.h
 *
 *  Created on: 13 янв. 2020 г.
 *      Author: Andrey Zakharov <zaharov@screen-co.ru>
 */
#ifndef __HYSCAN_LABEL_H__
#define __HYSCAN_LABEL_H__

#include "hyscan-object-data.h"

/*#define HYSCAN_LABEL               0x25f3cb7d*/

G_BEGIN_DECLS

#define HYSCAN_TYPE_LABEL (hyscan_label_get_type ())

typedef struct _HyScanLabel HyScanLabel;

/**
 * HyScanLabel:
 * @type: тип объекта (группа)
 * @name: название группы
 * @description: описание группы
 * @operator_name: имя оператора
 * @label: идентификатор группы (битовая маска)
 * @creation_time: время создания
 * @modification_time: время последней модификации*
 */
struct _HyScanLabel
{
  GType             type;
  gchar            *name;
  gchar            *description;
  gchar            *operator_name;
  gchar            *icon_name;
  guint64           label;
  gint64            ctime;
  gint64            mtime;
};

HYSCAN_API
GType                hyscan_label_get_type                     (void);

HYSCAN_API
HyScanLabel*         hyscan_label_new                          (void);

HYSCAN_API
HyScanLabel*         hyscan_label_copy                         (const HyScanLabel      *self);

HYSCAN_API
void                 hyscan_label_free                         (HyScanLabel            *self);

HYSCAN_API
void                 hyscan_label_set_text                     (HyScanLabel            *self,
                                                                const gchar            *name,
                                                                const gchar            *description,
                                                                const gchar            *oper);

HYSCAN_API
void                 hyscan_label_set_icon_name                (HyScanLabel            *self,
                                                                const gchar            *icon_name);

HYSCAN_API
void                 hyscan_label_set_ctime                    (HyScanLabel            *self,
                                                                gint64                  creation);

HYSCAN_API
void                 hyscan_label_set_mtime                    (HyScanLabel            *self,
                                                                gint64                  modification);

HYSCAN_API
void                 hyscan_label_set_label                    (HyScanLabel            *self,
                                                                guint64                 label);

G_END_DECLS

#endif /* __HYSCAN_LABEL_H__ */
