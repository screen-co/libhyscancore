/* hyscan-units.c
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

/**
 * SECTION: hyscan-units
 * @Short_description: класс единиц измерения величин
 * @Title: HyScanUnits
 *
 * HyScanUnits - класс для хранения текущих единиц измерения и перевода
 * величин из внутренних единиц измерения в текущие.
 *
 */

#include "hyscan-units.h"

enum
{
  PROP_O,
  PROP_GEO
};

struct _HyScanUnitsPrivate
{
  HyScanUnitsGeo               geo;  /* Единицы измерения широты и долготы. */
};

static void        hyscan_units_set_property             (GObject               *object,
                                                          guint                  prop_id,
                                                          const GValue          *value,
                                                          GParamSpec            *pspec);
static void        hyscan_units_get_property             (GObject               *object,
                                                          guint                  prop_id,
                                                          GValue                *value,
                                                          GParamSpec            *pspec);
static void        hyscan_units_object_constructed       (GObject               *object);
static void        hyscan_units_object_finalize          (GObject               *object);

static GParamSpec *hyscan_units_property_geo;

G_DEFINE_TYPE_WITH_PRIVATE (HyScanUnits, hyscan_units, G_TYPE_OBJECT)

static void
hyscan_units_class_init (HyScanUnitsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_units_set_property;
  object_class->get_property = hyscan_units_get_property;

  object_class->constructed = hyscan_units_object_constructed;
  object_class->finalize = hyscan_units_object_finalize;

  hyscan_units_property_geo = g_param_spec_int ("geo", "Geo Units", "Units of geographic coordinates",
                                                HYSCAN_UNITS_GEO_FIRST, HYSCAN_UNITS_GEO_LAST, HYSCAN_UNITS_GEO_DD,
                                                G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
  g_object_class_install_property (object_class, PROP_GEO, hyscan_units_property_geo);
}

static void
hyscan_units_init (HyScanUnits *units)
{
  units->priv = hyscan_units_get_instance_private (units);
}

static void
hyscan_units_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  HyScanUnits *units = HYSCAN_UNITS (object);

  switch (prop_id)
    {
    case PROP_GEO:
      hyscan_units_set_geo (units, g_value_get_int (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_units_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  HyScanUnits *units = HYSCAN_UNITS (object);
  HyScanUnitsPrivate *priv = units->priv;

  switch ( prop_id )
    {
    case PROP_GEO:
      g_value_set_int (value, priv->geo);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_units_object_constructed (GObject *object)
{
  HyScanUnits *units = HYSCAN_UNITS (object);
  HyScanUnitsPrivate *priv = units->priv;

  G_OBJECT_CLASS (hyscan_units_parent_class)->constructed (object);

  priv->geo = HYSCAN_UNITS_GEO_DD;
}

static void
hyscan_units_object_finalize (GObject *object)
{
  G_OBJECT_CLASS (hyscan_units_parent_class)->finalize (object);
}

HyScanUnits *
hyscan_units_new (void)
{
  return g_object_new (HYSCAN_TYPE_UNITS, NULL);
}

HyScanUnitsGeo
hyscan_units_get_geo (HyScanUnits *units)
{
  g_return_val_if_fail (HYSCAN_IS_UNITS (units), HYSCAN_UNITS_GEO_INVALID);

  return units->priv->geo;
}

void
hyscan_units_set_geo (HyScanUnits     *units,
                      HyScanUnitsGeo   unit_geo)
{
  g_return_if_fail (HYSCAN_IS_UNITS (units));

  units->priv->geo = unit_geo;
  g_object_notify_by_pspec (G_OBJECT (units), hyscan_units_property_geo);
}

gchar *
hyscan_units_format (HyScanUnits     *units,
                     HyScanUnitType   type,
                     gdouble          value,
                     gint             precision)
{
  HyScanUnitsPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_UNITS (units), HYSCAN_UNITS_GEO_INVALID);

  priv = units->priv;

  if (type == HYSCAN_UNIT_TYPE_LAT || type == HYSCAN_UNIT_TYPE_LON)
    {
      gchar text_format[128];
      const gchar *suffix;

      if (type == HYSCAN_UNIT_TYPE_LON)
        suffix = value > 0 ? "E" : (value < 0 ? "W" : "");
      else
        suffix = value < 0 ? "S" : (value > 0 ? "N" : "");

      value = ABS (value);
      if (priv->geo == HYSCAN_UNITS_GEO_DD)
        {
          g_snprintf (text_format, sizeof (text_format), "%%.%df°%%s", precision > 0 ? precision : 0);

          return g_strdup_printf (text_format, value, suffix);
        }

      else if (priv->geo == HYSCAN_UNITS_GEO_DDMM)
        {
          gint size;
          gint deg;
          gdouble minute;

          deg = value;
          minute = (value - deg) * 60;

          /* Требуемая точность для минут ниже, чем для градусов на log10(60) ~ 1.7 Берём 1 с запасом. */
          precision = precision > 1 ? precision - 1 : 0;

          /* Для минут < 10 добавляем ведущий ноль. */
          size = precision + 3;

          g_snprintf (text_format, sizeof (text_format), "%%d°%%0%d.%df′%%s", size, precision);

          return g_strdup_printf (text_format, deg, minute, suffix);
        }

      else if (priv->geo == HYSCAN_UNITS_GEO_DDMMSS)
        {
          gint deg, minute;
          gdouble seconds;

          deg = value;
          value -= deg;
          minute = value * 60;
          value -= minute / 60.;
          seconds = value * 3600;

          /* Требуемая точность для секунд ниже, чем для градусов на log10(3600) ~ 3.5 Берём 3 с запасом. */
          precision = precision > 3 ? precision - 3 : 0;

          g_snprintf (text_format, sizeof (text_format), "%%d°%%02d′%%.%df″%%s", precision);

          return g_strdup_printf (text_format, deg, minute, seconds, suffix);
        }
    }

  g_warning ("HyScanUnits: failed to format unit of type %d", type);

  return NULL;
}

const gchar *
hyscan_units_id_by_geo (HyScanUnitsGeo unit_geo)
{
  switch (unit_geo)
    {
      case HYSCAN_UNITS_GEO_DD:
        return "dd";

      case HYSCAN_UNITS_GEO_DDMM:
        return "ddmm";

      case HYSCAN_UNITS_GEO_DDMMSS:
        return "ddmmss";

      default:
        return NULL;
    }
}

HyScanUnitsGeo
hyscan_units_geo_by_id (const gchar *id)
{
  if (g_strcmp0 ("dd", id) == 0)
    return HYSCAN_UNITS_GEO_DD;

  else if (g_strcmp0 ("ddmm", id) == 0)
    return HYSCAN_UNITS_GEO_DDMM;

  else if (g_strcmp0 ("ddmmss", id) == 0)
    return HYSCAN_UNITS_GEO_DDMMSS;

  return HYSCAN_UNITS_GEO_INVALID;
}
