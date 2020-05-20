/*
 * hyscan-label.c
 *
 *  Created on: 13 янв. 2020 г.
 *      Author: Andrey Zakharov <zaharov@screen-co.ru>
 */
#include "hyscan-label.h"

G_DEFINE_BOXED_TYPE (HyScanLabel, hyscan_label, hyscan_label_copy, hyscan_label_free)

/**
 * hyscan_label_new:
 *
 * Создаёт структуру #HyScanLabel
 *
 * Returns: указатель на #HyScanLabel. Для удаления hyscan_label_free()
 */
HyScanLabel*
hyscan_label_new (void)
{
  HyScanLabel *self;

  self = g_slice_new0 (HyScanLabel);
  self->type = HYSCAN_TYPE_LABEL;

  return self;
}

/**
 * hyscan_label_copy:
 * @self: указатель на копируемую структуру
 *
 * Создаёт копию структуры @label
 *
 * Returns: указатель на #HyScanLabel. Для удаления hyscan_label_free()
 */
HyScanLabel*
hyscan_label_copy (const HyScanLabel *self)
{
  HyScanLabel *copy;

  if (self == NULL)
    return NULL;

  copy = hyscan_label_new ();
  hyscan_label_set_text (copy, self->name, self->description, self->operator_name);
  hyscan_label_set_icon_name (copy, self->icon_name);
  copy->label = self->label;
  copy->ctime = self->ctime;
  copy->mtime = self->mtime;

  return copy;
}

/**
 * hyscan_label_free:
 * @self: указатель на HyScanLabel
 *
 * Удаляет структуру #HyScanLabel
 */
void
hyscan_label_free (HyScanLabel *self)
{
  if (self == NULL)
    return;

  g_slice_free (HyScanLabel, self);
}

/**
 * hyscan_label_set_text:
 * @self: указатель на HyScanLabel
 * @name: название группы
 * @description: описание группы
 * @oper: оператор
 *
 * Устанавливает текстовые поля для группы
 */
void
hyscan_label_set_text (HyScanLabel *self,
                       const gchar *name,
                       const gchar *description,
                       const gchar *oper)
{
  g_free (self->name);
  g_free (self->description);
  g_free (self->operator_name);

  self->name = g_strdup (name);
  self->description = g_strdup (description);
  self->operator_name = g_strdup (oper);
}

/**
 * hyscan_label_set_icon_name:
 * @self: указатель на HyScanLabel
 * @icon_name: название группы
 *
 * Устанавливает имя иконки группы для отображения в представлении.
 */
void
hyscan_label_set_icon_name (HyScanLabel *self,
                            const gchar *icon_name)
{
  g_free (self->icon_name);
  self->icon_name = g_strdup (icon_name);
}

/**
 * hyscan_label_set_ctime:
 * @self: указатель на HyScanlabel
 * @creation: время создания группы
 *
 * Устанавливает время создания группы
 */
void
hyscan_label_set_ctime (HyScanLabel *self,
                        gint64       creation)
{
  self->ctime = creation;
}

/**
 * hyscan_label_set_mtime:
 * @self: указатель на HyScanLabel
 * @modification: время изменения группы
 *
 * Устанавливает время изменения группы
 */
void
hyscan_label_set_mtime (HyScanLabel *self,
                        gint64       modification)
{
  self->mtime = modification;
}

/**
 * hyscan_label_set_label:
 * @self: указатель на HyScanLabel
 * @label: идентификатор группы (битовая маска)
 *
 * Устанавливает идентификатор группы
 */
void
hyscan_label_set_label (HyScanLabel *self,
                        guint64      label)
{
  self->label = label;
}
