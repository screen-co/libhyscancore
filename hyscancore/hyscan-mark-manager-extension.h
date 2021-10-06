/* hyscan-mark-manager-extension.h
 *
 * Copyright 2021 Screen LLC, Andrey Zakharov <zaharov@screen-co.ru>
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

#ifndef __HYSCAN_MARK_MANAGER_EXTENSION_H__
#define __HYSCAN_MARK_MANAGER_EXTENSION_H__

#include <glib.h>
#include <hyscan-api.h>
/* Типы записей в модели. */
typedef enum
{
  PARENT,  /* Узел. */
  CHILD,   /* Объект. */
  ITEM     /* Атрибут объекта. */
} HyScanMarkManagerExtensionType;

/**
 * HyScanMarkManagerExtension:
 * @type: тип записи
 * @active: cостояние чек-бокса
 * @expand: развёрнут ли объект (для древовидного представления)
 *
 * Структура содержащая расширеную информацию об объектах.
 */
typedef struct
{
  HyScanMarkManagerExtensionType  type;
  gboolean                        active,
                                  expanded;
}HyScanMarkManagerExtension;

HYSCAN_API
HyScanMarkManagerExtension*  hyscan_mark_manager_extension_new  (HyScanMarkManagerExtensionType  type,
                                                                 gboolean                        active,
                                                                 gboolean                        expanded);

HYSCAN_API
HyScanMarkManagerExtension*  hyscan_mark_manager_extension_copy (HyScanMarkManagerExtension     *self);

HYSCAN_API
void                         hyscan_mark_manager_extension_free (gpointer                        data);

#endif /* __HYSCAN_MARK_MANAGER_EXTENSION_H__ */
