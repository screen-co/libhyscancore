/* hyscan-mark-manager-extension.c
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

/**
 * SECTION: hyscan-mark-manager-extension
 * @Short_description: Набор функций для работы со структурой содержащей информацию о состоянии объектов Журнала Меток.
 * @Title: HyScanMarkManagerExtension
 *
 * Используется для синхронизации представления данных в различных экземплярах Журнала Меток.
 *
 * Информация об объекте хранится в структуре #HyScanMarkManagerExtension.
 *
 * Для создания, копирования, удаления группы используются функции:
 *
 * - hyscan_mark_manager_extension_new(), hyscan_mark_manager_extension_copy(), hyscan_mark_manager_extension_free();
 *
 */

#include <hyscan-mark-manager-extension.h>

/**
 * hyscan_mark_manager_extension_new:
 * @type: тип записи
 * @active: cостояние чек-бокса
 * @expand: Развёрнут ли объект (для древовидного представления)
 *
 * Создаёт структуру #HyScanMarkManagerExtension.

 * Returns: указатель на структуру #HyScanMarkManagerExtension. Для удаления необходимо
 * использовать hyscan_mark_manager_extension_free ().
 */
HyScanMarkManagerExtension*
hyscan_mark_manager_extension_new (HyScanMarkManagerExtensionType  type,
                                   gboolean                        active,
                                   gboolean                        expanded)
{
  HyScanMarkManagerExtension *extension = g_new (HyScanMarkManagerExtension, 1);
  extension->type     = type;
  extension->active   = active;
  extension->expanded = expanded;
  return extension;
}

/**
 * hyscan_mark_manager_extension_copy:
 * @self: указатель на копируемую структуру
 *
 * Создаёт копию структуры #HyScanMarkManagerExtension.
 *
 * Returns: указатель на #HyScanMarkManagerExtension. Для удаления необходимо
 * использовать hyscan_mark_manager_extension_free ().
 */
HyScanMarkManagerExtension*
hyscan_mark_manager_extension_copy (HyScanMarkManagerExtension *self)
{
  HyScanMarkManagerExtension *copy = g_new (HyScanMarkManagerExtension, 1);
  copy->type     = self->type;
  copy->active   = self->active;
  copy->expanded = self->expanded;
  return copy;
}

/**
 * hyscan_mark_manager_extension_free:
 * @data: указатель на структуру #HyScanMarkManager.
 *
 * Удаляет структуру #HyScanMarkManagerExtension.
 */
void
hyscan_mark_manager_extension_free (gpointer data)
{
  if (data != NULL)
    {
      HyScanMarkManagerExtension *extension = (HyScanMarkManagerExtension*)data;
      extension->type     = PARENT;
      extension->active   =
      extension->expanded = FALSE;
      g_free (extension);
    }
}
