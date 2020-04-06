/* hyscan-proj.c
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
 * SECTION: hyscan-proj
 * @Short_description: Картографическая проекция, определённая в PROJ
 * @Title: HyScanProj
 *
 * Класс реализует интерфейс #HyScanGeoProjection.
 *
 * Класс является адаптером проекций бибилотеки PROJ для интерфейса #HyScanGeoProjection.
 * Для создания проекции, необходимо передать в функцию hyscan_proj_new() определение
 * проекции PROJ.
 *
 * Для часто встречающихся проекций определены макросы:
 * - %HYSCAN_PROJ_WEBMERC - сферическая проекция Меркатора (используется в OpenStreetMap),
 * - %HYSCAN_PROJ_MERC - эллипсоидальная проекция Меркатора
 *
 */

#include "hyscan-proj.h"
#include <math.h>
#include <proj_api.h>

#define LAT_SEC_TO_METER    30.8707932            /* Длина одной секунды меридиана в метрах. */
#define SEC_TO_DEG          1. / 3600             /* Число градусов в одной секунде. */

enum
{
  PROP_0,
  PROP_DEFINITION,
};

struct _HyScanProjPrivate
{
  gchar                       *definition;  /* Определения проекции в формате PROJ. */
  projPJ                       proj_lonlat; /* Исходная система координат (широта, долгота). */
  projPJ                       proj_topo;   /* Целевая система координат (x, y). */
  gboolean                     mercator;    /* Является ли проекция разновидностью Меркатора. */

  gdouble                      min_x;       /* Минимальная координата по оси OX. */
  gdouble                      max_x;       /* Максимальная координата по оси OX. */
  gdouble                      min_y;       /* Минимальная координата по оси OY. */
  gdouble                      max_y;       /* Максимальная координата по оси OY. */
};

static void          hyscan_proj_interface_init       (HyScanGeoProjectionInterface *iface);
static void          hyscan_proj_object_constructed   (GObject                      *object);
static void          hyscan_proj_object_finalize      (GObject                      *object);
static void          hyscan_proj_set_property         (GObject                      *object,
                                                       guint                         prop_id,
                                                       const GValue                 *value,
                                                       GParamSpec                   *pspec);
static void          hyscan_proj_set_limits           (HyScanProj                   *proj);

G_DEFINE_TYPE_WITH_CODE (HyScanProj, hyscan_proj, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (HyScanProj)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_GEO_PROJECTION, hyscan_proj_interface_init))

static void
hyscan_proj_class_init (HyScanProjClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = hyscan_proj_object_constructed;
  object_class->finalize = hyscan_proj_object_finalize;
  object_class->set_property = hyscan_proj_set_property;

  g_object_class_install_property (object_class, PROP_DEFINITION,
    g_param_spec_string ("definition", "PROJ definition", "Definition of target projection", NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_proj_init (HyScanProj *proj)
{
  proj->priv = hyscan_proj_get_instance_private (proj);
}

static void
hyscan_proj_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  HyScanProj *proj = HYSCAN_PROJ (object);
  HyScanProjPrivate *priv = proj->priv;

  switch (prop_id)
    {
    case PROP_DEFINITION:
      priv->definition = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_proj_object_constructed (GObject *object)
{
  HyScanProj *proj = HYSCAN_PROJ (object);
  HyScanProjPrivate *priv = proj->priv;

  priv->mercator = g_strcmp0 (priv->definition, HYSCAN_PROJ_MERC) == 0 ||
                   g_strcmp0 (priv->definition, HYSCAN_PROJ_WEBMERC) == 0;

  priv->proj_topo = pj_init_plus (priv->definition);
  if (priv->proj_topo == NULL)
    g_warning ("HyScanProj: %s", pj_strerrno (pj_errno));

  priv->proj_lonlat = pj_init_plus ("+proj=latlong +ellps=WGS84");

  hyscan_proj_set_limits (proj);
}

static void
hyscan_proj_object_finalize (GObject *object)
{
  HyScanProj *proj = HYSCAN_PROJ (object);
  HyScanProjPrivate *priv = proj->priv;

  g_free (priv->definition);
  pj_free (priv->proj_topo);
  pj_free (priv->proj_lonlat);
}

/* Устанавливает границы определения проекции по широте. */
static void
hyscan_proj_set_limits (HyScanProj *proj)
{
  HyScanProjPrivate *priv = proj->priv;
  HyScanGeoPoint coord;
  HyScanGeoCartesian2D boundary;

  coord.lat = 0;
  coord.lon = -180.0;
  hyscan_geo_projection_geo_to_value (HYSCAN_GEO_PROJECTION (proj), coord, &boundary);
  priv->min_x = boundary.x;
  
  coord.lat = 0;
  coord.lon = 180.0;
  hyscan_geo_projection_geo_to_value (HYSCAN_GEO_PROJECTION (proj), coord, &boundary);
  priv->max_x = boundary.x;
  
  priv->min_y = priv->min_x;
  priv->max_y = priv->max_x;
}

/* Переводит географические координаты @coords в координаты (@x, @y) проекции. */
static void
hyscan_proj_geo_to_value (HyScanGeoProjection  *proj,
                          HyScanGeoPoint        coords,
                          HyScanGeoCartesian2D *c2d)
{
  HyScanProjPrivate *priv = HYSCAN_PROJ (proj)->priv;
  gdouble x, y;
  gint code;

  x = DEG_TO_RAD * coords.lon;
  y = DEG_TO_RAD * coords.lat;
  if ((code = pj_transform (priv->proj_lonlat, priv->proj_topo, 1, 1, &x, &y, NULL)) != 0)
    g_warning ("HyScanProj: %s", pj_strerrno (code));

  c2d->x = x;
  c2d->y = y;
}

/* Переводит координаты на карте (@x, @y) в географические координаты @coords. */
static void
hyscan_proj_value_to_geo (HyScanGeoProjection *proj,
                          HyScanGeoPoint      *coords,
                          gdouble              x,
                          gdouble              y)
{
  HyScanProjPrivate *priv = HYSCAN_PROJ (proj)->priv;

  pj_transform (priv->proj_topo, priv->proj_lonlat, 1, 1, &x, &y, NULL);

  coords->lon = RAD_TO_DEG * x;
  coords->lat = RAD_TO_DEG * y;
}

/* Определяет границы проекции. */
static void
hyscan_proj_get_limits (HyScanGeoProjection *proj,
                        gdouble             *min_x,
                        gdouble             *max_x,
                        gdouble             *min_y,
                        gdouble             *max_y)
{
  HyScanProjPrivate *priv = HYSCAN_PROJ (proj)->priv;

  (min_x != NULL) ? *min_x = priv->min_x : 0;
  (min_y != NULL) ? *min_y = priv->min_y : 0;
  (max_x != NULL) ? *max_x = priv->max_x : 0;
  (max_y != NULL) ? *max_y = priv->max_y : 0;
}

/* Определяет масштаб проекции в указанной точке @coords. */
static gdouble
hyscan_proj_get_scale (HyScanGeoProjection *proj,
                       HyScanGeoPoint       coords)
{
  HyScanProjPrivate *priv = HYSCAN_PROJ (proj)->priv;

  /* В проекции Меркатора масштаб можно посчитать более простым образом. */
  if (priv->mercator)
    {
      return cos (DEG_TO_RAD * coords.lat);
    }

  /* В общем случае считаем длину одной угловой секунды вдоль меридиана в текущей проекции.
   * Длина одной угловой секунды примерно постоянная (±1%) и равна LAT_SEC_TO_METER метров. */
  else
    {
      HyScanGeoCartesian2D c2d1, c2d2;
      HyScanGeoPoint coords2 = coords;
      coords2.lat += G_UNLIKELY (coords2.lat) > 89. ? SEC_TO_DEG : -SEC_TO_DEG;

      hyscan_geo_projection_geo_to_value (proj, coords, &c2d1);
      hyscan_geo_projection_geo_to_value (proj, coords2, &c2d2);

      return LAT_SEC_TO_METER / hypot (c2d1.x - c2d2.x, c2d1.y - c2d2.y);
    }
}

static guint
hyscan_proj_hash (HyScanGeoProjection *geo_projection)
{
  HyScanProj *proj = HYSCAN_PROJ (geo_projection);
  HyScanProjPrivate *priv = proj->priv;

  return g_str_hash (priv->definition);
}

static void
hyscan_proj_interface_init (HyScanGeoProjectionInterface  *iface)
{
  iface->geo_to_value = hyscan_proj_geo_to_value;
  iface->value_to_geo = hyscan_proj_value_to_geo;
  iface->get_limits = hyscan_proj_get_limits;
  iface->get_scale = hyscan_proj_get_scale;
  iface->hash = hyscan_proj_hash;
}

/**
 * hyscan_proj_new:
 * @p: параметры земного эллипсоида
 *
 * Создаёт объект картографической проекции Меркатора, которая определяет связь
 * географических координат поверхности Земли с координатами на карте.
 *
 * Returns: новый объект #HyScanProj. Для удаления g_object_unref().
 */
HyScanGeoProjection *
hyscan_proj_new (const gchar *definition)
{
  return g_object_new (HYSCAN_TYPE_PROJ,
                       "definition", definition,
                       NULL);
}
