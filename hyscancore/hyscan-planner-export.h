/* hyscan-planner-export.h
 *
 * Copyright 2020 Screen LLC, Alexey Sakhnov <alexsakhnov@gmail.com>
 *
 * This file is part of HyScanGui library.
 *
 * HyScanGui is dual-licensed: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * HyScanGui is distributed in the hope that it will be useful,
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

/* HyScanGui имеет двойную лицензию.
 *
 * Во-первых, вы можете распространять HyScanGui на условиях Стандартной
 * Общественной Лицензии GNU версии 3, либо по любой более поздней версии
 * лицензии (по вашему выбору). Полные положения лицензии GNU приведены в
 * <http://www.gnu.org/licenses/>.
 *
 * Во-вторых, этот программный код можно использовать по коммерческой
 * лицензии. Для этого свяжитесь с ООО Экран - <info@screen-co.ru>.
 */

#ifndef __HYSCAN_PLANNER_EXPORT_H__
#define __HYSCAN_PLANNER_EXPORT_H__

#include <glib-object.h>
#include <hyscan-api.h>
#include <hyscan-db.h>

G_BEGIN_DECLS

HYSCAN_API
gboolean               hyscan_planner_export_xml_to_file          (const gchar         *filename,
                                                                   GHashTable          *objects);

HYSCAN_API
gchar *                hyscan_planner_export_xml_to_str           (GHashTable          *objects);

HYSCAN_API
gboolean               hyscan_planner_export_kml_to_file          (const gchar         *filename,
                                                                   GHashTable          *objects);

HYSCAN_API
gchar *                hyscan_planner_export_kml_to_str           (GHashTable          *objects);

HYSCAN_API
GHashTable *           hyscan_planner_import_xml_from_file        (const gchar         *filename);

HYSCAN_API
gboolean               hyscan_planner_import_to_db                (HyScanDB            *db,
                                                                   const gchar         *project_name,
                                                                   GHashTable          *objects,
                                                                   gboolean             replace);

G_END_DECLS

#endif /* __HYSCAN_PLANNER_EXPORT_H__ */
