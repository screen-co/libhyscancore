/* hyscan-label.c
 *
 * Copyright 2020 Screen LLC, Andrey Zakharov <zaharov@screen-co.ru>
 *
 * This file is part of HyScanCore library.
 *
 * HyScanCore is dual-licensed: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * HyScanCore is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Alternatively, you can license this code under a commercial license.
 * Contact the Screen LLC in this case - <info@screen-co.ru>.
 */

/* HyScanCore имеет двойную лицензию.
 *
 * Во-первых, вы можете распространять HyScanCore на условиях Стандартной
 * Общественной Лицензии GNU версии 3, либо по любой более поздней версии
 * лицензии (по вашему выбору). Полные положения лицензии GNU приведены в
 * <http://www.gnu.org/licenses/>.
 *
 * Во-вторых, этот программный код можно использовать по коммерческой
 * лицензии. Для этого свяжитесь с ООО Экран - <info@screen-co.ru>.
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
  hyscan_label_set_icon_data (copy, self->icon_data);
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

  g_free (self->name);
  g_free (self->icon_data);
  g_free (self->description);
  g_free (self->operator_name);
  g_slice_free (HyScanLabel, self);
}

/**
 * hyscan_label_set_text:
 * @self: указатель на HyScanLabel
 * @name: название группы
 * @description: описание группы
 * @operator_name: оператор
 *
 * Устанавливает текстовые поля для группы
 */
void
hyscan_label_set_text (HyScanLabel *self,
                       const gchar *name,
                       const gchar *description,
                       const gchar *operator_name)
{
  g_free (self->name);
  g_free (self->description);
  g_free (self->operator_name);

  self->name = g_strdup (name);
  self->description = g_strdup (description);
  self->operator_name = g_strdup (operator_name);
}

/**
 * hyscan_label_set_icon_data:
 * @self: указатель на HyScanLabel
 * @icon_data: строка содержащая графическое изображение для группы (иконку) в формате BASE64
 *
 * Устанавливает графическое изображение группы для отображения в представлении.
 */
void
hyscan_label_set_icon_data (HyScanLabel *self,
                            const gchar *icon_data)
{
  g_free (self->icon_data);
  self->icon_data = g_strdup (icon_data);
}

/**
 * hyscan_label_set_ctime:
 * @self: указатель на HyScanlabel
 * @ctime: время создания группы
 *
 * Устанавливает время создания группы
 */
void
hyscan_label_set_ctime (HyScanLabel *self,
                        gint64       ctime)
{
  self->ctime = ctime;
}

/**
 * hyscan_label_set_mtime:
 * @self: указатель на HyScanLabel
 * @mtime: время изменения группы
 *
 * Устанавливает время изменения группы
 */
void
hyscan_label_set_mtime (HyScanLabel *self,
                        gint64       mtime)
{
  self->mtime = mtime;
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
