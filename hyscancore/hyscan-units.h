/* hyscan-units.h
 *
 * Copyright 2020 Screen LLC, Alexey Sakhnov <alexsakhnov@gmail.com>
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

#ifndef __HYSCAN_UNITS_H__
#define __HYSCAN_UNITS_H__

#include <hyscan-api.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_UNITS             (hyscan_units_get_type ())
#define HYSCAN_UNITS(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_UNITS, HyScanUnits))
#define HYSCAN_IS_UNITS(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_UNITS))
#define HYSCAN_UNITS_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_UNITS, HyScanUnitsClass))
#define HYSCAN_IS_UNITS_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_UNITS))
#define HYSCAN_UNITS_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_UNITS, HyScanUnitsClass))

typedef struct _HyScanUnits HyScanUnits;
typedef struct _HyScanUnitsPrivate HyScanUnitsPrivate;
typedef struct _HyScanUnitsClass HyScanUnitsClass;
typedef enum _HyScanUnitType HyScanUnitType;
typedef enum _HyScanUnitsGeo HyScanUnitsGeo;

enum _HyScanUnitType
{
  HYSCAN_UNIT_TYPE_LAT,
  HYSCAN_UNIT_TYPE_LON
};

enum _HyScanUnitsGeo
{
  HYSCAN_UNITS_GEO_INVALID,
  HYSCAN_UNITS_GEO_DD,
  HYSCAN_UNITS_GEO_DDMM,
  HYSCAN_UNITS_GEO_DDMMSS,

  HYSCAN_UNITS_GEO_FIRST = HYSCAN_UNITS_GEO_DD,
  HYSCAN_UNITS_GEO_LAST = HYSCAN_UNITS_GEO_DDMMSS,
};

struct _HyScanUnits
{
  GObject parent_instance;

  HyScanUnitsPrivate *priv;
};

struct _HyScanUnitsClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                  hyscan_units_get_type         (void);

HYSCAN_API
HyScanUnits *          hyscan_units_new              (void);

HYSCAN_API
HyScanUnitsGeo         hyscan_units_get_geo          (HyScanUnits     *units);

HYSCAN_API
void                   hyscan_units_set_geo          (HyScanUnits     *units,
                                                      HyScanUnitsGeo   unit_geo);

HYSCAN_API
gchar *                hyscan_units_format           (HyScanUnits     *units,
                                                      HyScanUnitType   type,
                                                      gdouble          value,
                                                      gint             precision);

HYSCAN_API
const gchar *          hyscan_units_id_by_geo        (HyScanUnitsGeo   unit_geo);

HYSCAN_API
HyScanUnitsGeo         hyscan_units_geo_by_id        (const gchar     *id);


G_END_DECLS

#endif /* __HYSCAN_UNITS_H__ */
