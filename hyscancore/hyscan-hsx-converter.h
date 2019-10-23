/* hyscan-hsx-converter.c
 *
 * Copyright 2019 Screen LLC, Maxim Pylaev <pilaev@screen-co.ru>
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
 * SECTION: hyscan-hsx-converter
 * @Short_description: класс конвертер данных в формат HSX (текстовый)
 * @Title: HyScanHSXConverter
 *
 *
 */

#ifndef __HYSCAN_HSX_CONVERTER_H__
#define __HYSCAN_HSX_CONVERTER_H__

#include <hyscan-api.h>
#include <glib-object.h>
#include <hyscan-db.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_HSX_CONVERTER             (hyscan_hsx_converter_get_type ())
#define HYSCAN_HSX_CONVERTER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj),\
                                               HYSCAN_TYPE_HSX_CONVERTER, HyScanHSXConverter))
#define HYSCAN_IS_HSX_CONVERTER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_HSX_CONVERTER))
#define HYSCAN_HSX_CONVERTER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass),\
                                               HYSCAN_TYPE_HSX_CONVERTER, HyScanHSXConverterClass))
#define HYSCAN_IS_HSX_CONVERTER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_HSX_CONVERTER))
#define HYSCAN_HSX_CONVERTER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj),\
                                               HYSCAN_TYPE_HSX_CONVERTER, HyScanHSXConverterClass))

typedef struct _HyScanHSXConverter HyScanHSXConverter;
typedef struct _HyScanHSXConverterPrivate HyScanHSXConverterPrivate;
typedef struct _HyScanHSXConverterClass HyScanHSXConverterClass;

struct _HyScanHSXConverter
{
  GObject parent_instance;

  HyScanHSXConverterPrivate *priv;
};

struct _HyScanHSXConverterClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                  hyscan_hsx_converter_get_type         (void);

HYSCAN_API
HyScanHSXConverter*    hyscan_hsx_converter_new              (const gchar          *result_path);

HYSCAN_API
void                   hyscan_hsx_converter_set_track        (HyScanHSXConverter   *self,
                                                              HyScanDB             *db,
                                                              const gchar          *project_name,
                                                              const gchar          *track_name);

HYSCAN_API
void                   hyscan_hsx_converter_set_max_ampl     (HyScanHSXConverter   *self,
                                                              guint                 ampl_val);

HYSCAN_API
void                   hyscan_hsx_converter_set_image_prm    (HyScanHSXConverter   *self,
                                                              gfloat                black,
                                                              gfloat                white,
                                                              gfloat                gamma);

HYSCAN_API
gboolean               hyscan_hsx_converter_run              (HyScanHSXConverter   *self);

HYSCAN_API
gboolean               hyscan_hsx_converter_stop             (HyScanHSXConverter   *self);

HYSCAN_API
gboolean               hyscan_hsx_converter_is_run           (HyScanHSXConverter   *self);

G_END_DECLS

#endif /* __HYSCAN_HSX_CONVERTER_H__ */
