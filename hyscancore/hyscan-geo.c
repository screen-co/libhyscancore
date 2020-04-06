#include <math.h>
#include <stdio.h>
#include "hyscan-geo.h"

#define DEG2RAD(x) ((x) * G_PI / 180.0)
#define RAD2DEG(x) ((x) * 180.0 / G_PI)
#define SEC2RAD(x) ((x) * 4.84813681109536e-6)
#define MAX_ABS_INPUT_LON 180.0
#define MAX_ABS_INPUT_LAT 90.0
#define MAX_ABS_CALC_LON  540.0
#define MAX_ABS_CALC_LAT  180.0
#define EPS 1.0e-6

#define LON_OUT_OF_RANGE(x) (fabs(x)>MAX_ABS_INPUT_LON)
#define LAT_OUT_OF_RANGE(x) (fabs(x)>MAX_ABS_INPUT_LAT)

/* Знак переменной*/
#define hyscan_geo_sign_gdouble(x) ((x) < 0.0 ? -1.0 : 1.0)

/* Входную величину в пределах [-540, 540] преобразовывает к пределам [-180, 180]
 * Долгота не может быть больше 180 градусов по модулю.*/
#define hyscan_geo_fit_lon_in_range(x) (((x) > 180.0) ? (x - 360.0) : (((x) < -180.0) ? (x + 360.0) : x))

/* Входную величину в пределах [-180, 180] преобразовывает к пределам [-90, 90]
 * Широта не может быть больше 90 градусов по модулю.*/
#define hyscan_geo_fit_lat_in_range(x) (((x) > 90.0) ? (180.0 - x) : (((x) < -90.0) ? (-180.0 - x) : x))

struct HyScanGeoPrivate
{
  gdouble                  B0;
  gdouble                  L0;
  gdouble                  A0;

  gdouble                  A1;
  gdouble                  A2;
  gdouble                  A3;

  gdouble                  B1;
  gdouble                  B2;
  gdouble                  B3;

  gdouble                  C1;
  gdouble                  C2;
  gdouble                  C3;

  gdouble                  N0;
  gdouble                  N0_e;

  HyScanGeoEllipsoidParam  sphere_params;

  gint                     initialized;
  guint                    n_iter;
};

static void             hyscan_geo_constructed          (GObject  *object);
static void             hyscan_geo_finalize             (GObject  *object);

static void             hyscan_geo_geo2ecef             (HyScanGeoCartesian3D   *dst,
                                                         HyScanGeoGeodetic       src,
                                                         HyScanGeoEllipsoidParam params);
static void             hyscan_geo_ecef2geo             (HyScanGeoGeodetic      *dst,
                                                         HyScanGeoCartesian3D    src,
                                                         HyScanGeoEllipsoidParam params);
static void             hyscan_geo_topo2ecef            (HyScanGeo            *geo,
                                                         HyScanGeoCartesian3D *dst,
                                                         HyScanGeoCartesian3D  src);
static void             hyscan_geo_ecef2topo            (HyScanGeo            *geo,
                                                         HyScanGeoCartesian3D *dst,
                                                         HyScanGeoCartesian3D  src);

static HyScanGeoEllipsoidType hyscan_geo_get_ellipse_by_cs (HyScanGeoCSType cs_type);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanGeo, hyscan_geo, G_TYPE_OBJECT);

static void
hyscan_geo_class_init (HyScanGeoClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = hyscan_geo_constructed;
  object_class->finalize = hyscan_geo_finalize;
}

static void
hyscan_geo_init (HyScanGeo *geo)
{
  geo->priv = hyscan_geo_get_instance_private (geo);
}

static void
hyscan_geo_constructed (GObject  *object)
{
  HyScanGeo *geo = HYSCAN_GEO (object);

  hyscan_geo_set_number_of_iterations (geo, 1);
}

static void
hyscan_geo_finalize (GObject *object)
{
  G_OBJECT_CLASS (hyscan_geo_parent_class)->finalize (object);
}

/*
 * Пересчет геодезических координат в геоцентрические (Earth centered Earth fixed)
 *
 * \param *dst - геоцентрические координаты на выходе
 * \param  src - геодезические координаты на входе
 * \param params - параметры эллипсоида, должны соответствовать геодезическим координатам src,
 *                  структура с параметрами эллипсоида должна быть инициализирована заранее
 */
static void
hyscan_geo_geo2ecef (HyScanGeoCartesian3D   *dst,
                     HyScanGeoGeodetic       src,
                     HyScanGeoEllipsoidParam params)
{
  gdouble a, e, sinb, cosb, sinl, cosl, H, N;

  /* Параметры эллипсоида: */
  a = params.a; /* большая полуось */
  e = params.e; /* ellipticity */

  sinb = sin (src.lat);
  cosb = cos (src.lat);
  sinl = sin (src.lon);
  cosl = cos (src.lon);
  H = src.h;

  /* Главный вертикал */
  N = e * sinb;
  N = a / sqrt ( 1.0 - N * N );

  /* Координаты */
  a = (N + H) * cosb;
  dst->x = a * cosl;
  dst->y = a * sinl;
  dst->z = (N * (1.0 - params.e2) + H) * sinb;

}

/*
 * Пересчет  геоцентрических координат в топоцентрические
 * (ECEF - Earth Centered Earth Fixed)
 *
 * \param *dst - топоцентрические координаты на выходе
 * \param  src - геоцентрические координаты на входе
 */
static void
hyscan_geo_ecef2topo (HyScanGeo            *geo,
                      HyScanGeoCartesian3D *dst,
                      HyScanGeoCartesian3D  src)
{
  HyScanGeoPrivate *priv = geo->priv;
  gdouble z_c, x_t, y_t, z_t;

  z_c = src.z + priv->N0_e;
  x_t = priv->A1 * src.x + priv->B1 * src.y + priv->C1 * z_c;
  y_t = priv->A2 * src.x + priv->B2 * src.y + priv->C2 * z_c;
  z_t = priv->A3 * src.x + priv->B3 * src.y + priv->C3 * z_c - priv->N0;

  /* Is necessary в pass the same argument at src and dst */
  dst->x = x_t;
  dst->y = y_t;
  dst->z = z_t;

}

/*
 * Пересчет топоцентрических координат в геоцентрические
 * (ECEF - Earth Centered Earth Fixed)
 *
 * \param *dst - геоцентрические координаты на выходе
 * \param  src - топоцентрические координаты на входе
 */
static void
hyscan_geo_topo2ecef (HyScanGeo            *geo,
                      HyScanGeoCartesian3D *dst,
                      HyScanGeoCartesian3D  src)
{
  HyScanGeoPrivate *priv = geo->priv;
  gdouble  x_c, y_c, z_c, z_t;

  z_t = src.z + priv->N0;
  x_c = priv->A1 * src.x + priv->A2 * src.y + priv->A3 * z_t;
  y_c = priv->B1 * src.x + priv->B2 * src.y + priv->B3 * z_t;
  z_c = priv->C1 * src.x + priv->C2 * src.y + priv->C3 * z_t - priv->N0_e;
  dst->x = x_c;
  dst->y = y_c;
  dst->z = z_c;

}

/*
 * Пересчет геоцентрических координат в геодезические
 *
 * \param *dst - геодезические координаты на выходе
 * \param src - геоцентрические координаты на входе
 * \param params - параметры эллипсоида, должны соответствовать геодезическим координатам src,
 *                 структура с параметрами эллипсоида должна быть инициализирована заранее
 */
static void
hyscan_geo_ecef2geo (HyScanGeoGeodetic      *dst,
                     HyScanGeoCartesian3D    src,
                     HyScanGeoEllipsoidParam params)
{
  gdouble a, f, b, c, e_2, e_12, e_12xb, z, lat, lon, h, sin_lat, cos_lat, N, t, p;
  gint i;
  a = params.a; /* Большая полуось. */
  f = params.f; /* Полярное сжатие. */
  b = params.b; /* Малая полуось. */
  c = params.c;
  e_2 = params.e2; /* ellipticity squared. */
  e_12 = params.e12;
  e_12xb = e_12 * b;
  z = src.z ;

  p = hypot (src.x, src.y);
  if (p < EPS) /* p > 0 a priori. */
    {
      dst->lat = G_PI_2 * hyscan_geo_sign_gdouble (src.z);
      dst->lon = 0.0;
      dst->h = fabs (z) - b;
    }
  else
    {
      t = src.z / p * (1.0 + e_12xb / hypot (p, z));
      for (i = 0; i < 2; ++i)
        {
          t = t * (1.0 - f);
          lat = atan (t);
          cos_lat = cos (lat);
          sin_lat = sin (lat);
          t = (z + e_12xb * sin_lat*sin_lat*sin_lat) / (p - e_2 * a * cos_lat*cos_lat*cos_lat);
        }
      lon = atan2 (src.y, src.x);
      lat = atan (t);
      sin_lat = sin (lat);
      cos_lat = cos (lat);
      N = c / sqrt (1.0 + e_12 * cos_lat * cos_lat);
      if (fabs (t) < 1.0)
        {
          h = p / cos_lat - N;
        }
      else
        {
          h = z / sin_lat - N * (1.0 - e_2);
        }
      dst->lat = hyscan_geo_fit_lat_in_range(lat);
      dst->lon = hyscan_geo_fit_lon_in_range(lon);
      dst->h = h;
    }

}

/* Функция определяет параметры эллипсоида по типу СК. */
static HyScanGeoEllipsoidType
hyscan_geo_get_ellipse_by_cs (HyScanGeoCSType cs_type)
{
  switch (cs_type)
    {
    case HYSCAN_GEO_CS_WGS84:
      return HYSCAN_GEO_ELLIPSOID_WGS84;
    case HYSCAN_GEO_CS_SK42:
    case HYSCAN_GEO_CS_SK95:
      return HYSCAN_GEO_ELLIPSOID_KRASSOVSKY;
    case HYSCAN_GEO_CS_PZ90:
    case HYSCAN_GEO_CS_PZ90_02:
    case HYSCAN_GEO_CS_PZ90_11:
      return HYSCAN_GEO_ELLIPSOID_PZ90;
    default:
      return HYSCAN_GEO_ELLIPSOID_INVALID;
    }
}

HyScanGeo *
hyscan_geo_new (HyScanGeoGeodetic      origin,
                HyScanGeoEllipsoidType ell_type)
{
  HyScanGeo *geo;

  geo = g_object_new (HYSCAN_TYPE_GEO, NULL);

  if (!hyscan_geo_set_origin (geo, origin, ell_type))
    g_clear_object (&geo);

  return geo;
}

HyScanGeo *
hyscan_geo_new_user (HyScanGeoGeodetic       origin,
                     HyScanGeoEllipsoidParam ell_params)
{
  HyScanGeo *geo;

  /* Проверка на попадание в диапазон. */
  if (LAT_OUT_OF_RANGE(origin.lat) || LON_OUT_OF_RANGE(origin.lon) || fabs (origin.h) > 360.0)
    return NULL;

  geo = g_object_new (HYSCAN_TYPE_GEO, NULL);

  hyscan_geo_set_origin_user (geo, origin, ell_params);

  return geo;
}

gboolean
hyscan_geo_set_origin (HyScanGeo               *geo,
                       HyScanGeoGeodetic        origin,
                       HyScanGeoEllipsoidType   ell_type)
{
  HyScanGeoEllipsoidParam ell_params;

  g_return_val_if_fail (HYSCAN_IS_GEO (geo), FALSE);

  /* Проверка на попадание в диапазон. */
  if (LAT_OUT_OF_RANGE(origin.lat) || LON_OUT_OF_RANGE(origin.lon) || fabs (origin.h) > 360.0)
    return FALSE;

  if (hyscan_geo_init_ellipsoid (&ell_params, ell_type))
    {
      if (hyscan_geo_set_origin_user  (geo, origin, ell_params))
        return TRUE;
    }

  return FALSE;
}

gboolean
hyscan_geo_set_origin_user (HyScanGeo              *geo,
                            HyScanGeoGeodetic       origin,
                            HyScanGeoEllipsoidParam ell_params)
{
  gdouble sinB0, sinL0, sinA0;
  gdouble cosB0, cosL0, cosA0;
  HyScanGeoPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_GEO (geo), FALSE);
  priv = geo->priv;

  /* Проверка на попадание в диапазон. */
  if (LAT_OUT_OF_RANGE(origin.lat) || LON_OUT_OF_RANGE(origin.lon) || fabs (origin.h) > 360.0)
    return FALSE;

  priv->sphere_params = ell_params;
  priv->B0 = DEG2RAD(origin.lat);
  priv->L0 = DEG2RAD(origin.lon);
  priv->A0 = DEG2RAD(origin.h);

  sinB0 = sin (priv->B0);
  cosB0 = cos (priv->B0);
  sinL0 = sin (priv->L0);
  cosL0 = cos (priv->L0);
  sinA0 = sin (priv->A0);
  cosA0 = cos (priv->A0);

  priv->A1 = -sinB0 * cosL0 * cosA0 - sinL0 * sinA0;
  priv->A2 = -sinB0 * cosL0 * sinA0 + sinL0 * cosA0;
  priv->A3 =  cosB0 * cosL0;

  priv->B1 = -sinB0 * sinL0 * cosA0 + cosL0 * sinA0;
  priv->B2 = -sinB0 * sinL0 * sinA0 - cosL0 * cosA0;
  priv->B3 =  cosB0 * sinL0;

  priv->C1 =  cosB0 * cosA0;
  priv->C2 =  cosB0 * sinA0;
  priv->C3 =  sinB0;

  priv->N0_e = ell_params.e2 * sinB0;
  priv->N0 = ell_params.a / sqrt (1.0 - priv->N0_e * sinB0);
  priv->N0_e = priv->N0 * priv->N0_e;

  /* Устанавливаем флаг готовности. */
  geo->priv->initialized = 1;

  return TRUE;
}

void
hyscan_geo_set_number_of_iterations (HyScanGeo *geo,
                                     guint      iters)
{
  g_return_if_fail (HYSCAN_IS_GEO (geo));

  geo->priv->n_iter = iters;
}

gboolean
hyscan_geo_ready (HyScanGeo *geo,
                  gboolean   uninit)
{
  g_return_val_if_fail (HYSCAN_IS_GEO (geo), FALSE);

  if (uninit)
    geo->priv->initialized = 0;

  return geo->priv->initialized;
}

gboolean
hyscan_geo_geo2topo (HyScanGeo            *geo,
                     HyScanGeoCartesian3D *dst_topo,
                     HyScanGeoGeodetic     src_geod)
{
  HyScanGeoPrivate *priv;
  HyScanGeoGeodetic src_rad;

  g_return_val_if_fail (HYSCAN_IS_GEO (geo), FALSE);
  priv = geo->priv;

  /* Убеждаемся, что эллипсоид и начальная точка заданы. */
  if (priv->initialized == 0)
    return FALSE;

  /* Убеждаемся, что точка лежит в нормальных пределах. */
  if (LAT_OUT_OF_RANGE(src_geod.lat) || LON_OUT_OF_RANGE(src_geod.lon))
    return FALSE;

  src_rad.lat = DEG2RAD (src_geod.lat);
  src_rad.lon = DEG2RAD (src_geod.lon);
  src_rad.h = src_geod.h;

  /* Преобразовываем геодезические в ECEF. */
  hyscan_geo_geo2ecef (dst_topo, src_rad, priv->sphere_params);

  /* Преобразовываем ECEF в топоцентрические. */
  hyscan_geo_ecef2topo (geo, dst_topo, *dst_topo);

  return TRUE;
}

gboolean
hyscan_geo_topo2geo (HyScanGeo           *geo,
                     HyScanGeoGeodetic   *dst_geod,
                     HyScanGeoCartesian3D src_topo)
{
  HyScanGeoPrivate *priv;
  HyScanGeoGeodetic dst_rad;

  g_return_val_if_fail (HYSCAN_IS_GEO (geo), FALSE);
  priv = geo->priv;

  /* Убеждаемся, что эллипсоид и начальная точка заданы. */
  if (priv->initialized == 0)
    return FALSE;

  /* Преобразовываем топоцентрические в ECEF. */
  hyscan_geo_topo2ecef (geo, &src_topo, src_topo);

  /* Преобразовываем ECEF в геодезические. */
  hyscan_geo_ecef2geo (&dst_rad, src_topo, priv->sphere_params);

  dst_geod->lat = RAD2DEG (dst_rad.lat);
  dst_geod->lon = RAD2DEG (dst_rad.lon);
  dst_geod->h = dst_rad.h;

  return TRUE;
}

gboolean
hyscan_geo_geo2topoXY (HyScanGeo            *geo,
                       HyScanGeoCartesian2D *dst_topoXY,
                       HyScanGeoGeodetic     src_geod)
{
  HyScanGeoPrivate *priv;
  HyScanGeoGeodetic src_rad;
  HyScanGeoCartesian3D dst_topo;

  g_return_val_if_fail (HYSCAN_IS_GEO (geo), FALSE);
  priv = geo->priv;

  /* Убеждаемся, что эллипсоид и начальная точка заданы. */
  if (priv->initialized == 0)
    return FALSE;

  /* Убеждаемся, что точка лежит в нормальных пределах. */
  if (LAT_OUT_OF_RANGE(src_geod.lat) || LON_OUT_OF_RANGE(src_geod.lon))
    return FALSE;

  src_rad.lat = DEG2RAD (src_geod.lat);
  src_rad.lon = DEG2RAD (src_geod.lon);
  src_rad.h = src_geod.h;

  /* Преобразовываем геодезические в ECEF. */
  hyscan_geo_geo2ecef (&dst_topo, src_rad, priv->sphere_params);

  /* Преобразовываем ECEF в топоцентрические. */
  hyscan_geo_ecef2topo (geo, &dst_topo, dst_topo);

  dst_topoXY->x = dst_topo.x;
  dst_topoXY->y = dst_topo.y;

  return TRUE;
}

gboolean
hyscan_geo_geo2topoXY0 (HyScanGeo                      *geo,
                        HyScanGeoCartesian2D           *dst_topoXY,
                        HyScanGeoPoint                  src_geod)
{
  HyScanGeoGeodetic geodetic;

  g_return_val_if_fail (HYSCAN_IS_GEO (geo), FALSE);

  geodetic.lat = src_geod.lat;
  geodetic.lon = src_geod.lon;
  geodetic.h = 0;

  return hyscan_geo_geo2topoXY (geo, dst_topoXY, geodetic);
}

gboolean
hyscan_geo_topoXY2geo (HyScanGeo           *geo,
                       HyScanGeoGeodetic   *dst_geod,
                       HyScanGeoCartesian2D src_topoXY,
                       gdouble              h_geodetic)
{
  HyScanGeoPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_GEO (geo), FALSE);
  priv = geo->priv;

  HyScanGeoGeodetic dst_rad;

  gdouble earth_radius = 6371000; /* Радиус Земли в метрах. */
  gdouble dH, Ht, x_orig, y_orig;
  HyScanGeoCartesian3D topoXYZ;
  guint i;

  /* Убеждаемся, что эллипсоид и начальная точка заданы. */
  if (priv->initialized == 0)
    return FALSE;

  x_orig = src_topoXY.x;
  y_orig = src_topoXY.y;

  /* Проверка на попадание в диапазон. Координаты x и y не могут быть больше радиуса Земли. */
  if (fabs(x_orig) > earth_radius || fabs(y_orig) > earth_radius)
    return FALSE;

  dH = earth_radius - sqrt (earth_radius * earth_radius - x_orig * x_orig - y_orig * y_orig);
  Ht = h_geodetic - dH;
  topoXYZ.x = x_orig;
  topoXYZ.y = y_orig;
  topoXYZ.z = Ht;

  /* Первая итерация. */
  hyscan_geo_topo2geo (geo, &dst_rad, topoXYZ);

  /* Если требуется, делаем дополнительные итерации */
  for (i = 0; i < priv->n_iter; ++i)
    {
      /* Устанавливаем высоту. */
      dst_rad.h = h_geodetic;

      /* Переводим в топоцентрические. */
      hyscan_geo_geo2topo (geo, &topoXYZ, dst_rad);

      // Combine new topocentric Z and original X,Y coords
      topoXYZ.x = x_orig;
      topoXYZ.y = y_orig;

      // Преобразовываем old topocentric coords with new z coordinate
      hyscan_geo_topo2geo (geo, &dst_rad, topoXYZ);
    }

  dst_geod->lat = dst_rad.lat;
  dst_geod->lon = dst_rad.lon;
  dst_geod->h = dst_rad.h;

  return TRUE;
}

gboolean
hyscan_geo_topoXY2geo0 (HyScanGeo           *geo,
                        HyScanGeoPoint      *dst_geod,
                        HyScanGeoCartesian2D src_topoXY)
{
  HyScanGeoGeodetic geodetic;

  g_return_val_if_fail (HYSCAN_IS_GEO (geo), FALSE);

  if (!hyscan_geo_topoXY2geo (geo, &geodetic, src_topoXY, 0))
    return FALSE;

  dst_geod->lat = geodetic.lat;
  dst_geod->lon = geodetic.lon;

  return TRUE;
}

/* Перевод геодезических координат. */
gboolean
hyscan_geo_cs_transform (HyScanGeoGeodetic *dst,
                         HyScanGeoGeodetic  src,
                         HyScanGeoCSType    cs_in,
                         HyScanGeoCSType    cs_out)
{
  HyScanGeoDatumParam datum_param;
  HyScanGeoGeodetic src_rad, dst_rad;
  HyScanGeoEllipsoidType elps_in, elps_out;
  HyScanGeoEllipsoidParam el_params_in, el_params_out;

  /* Проверка на попадание в диапазон. */
  if (LON_OUT_OF_RANGE(src.lon) || LAT_OUT_OF_RANGE(src.lat))
    return FALSE;

  src_rad.lat = (src.lat);
  src_rad.lon = (src.lon);
  src_rad.h = (src.h);

  /* Определяем датум */
  datum_param = hyscan_geo_get_datum_params (cs_in, cs_out);

  /* Определяем тип референц-эллипсоида входной координаты */
  elps_in = hyscan_geo_get_ellipse_by_cs (cs_in);

  /* Определяем тип референц-эллипсоида выходной координаты */
  elps_out = hyscan_geo_get_ellipse_by_cs (cs_out);

  /* Определяем параметры референц-эллипсоида входной координаты */
  hyscan_geo_init_ellipsoid (&el_params_in, elps_in);

  /* Определяем параметры референц-эллипсоида выходной координаты */
  hyscan_geo_init_ellipsoid (&el_params_out, elps_out);

  /* Вызываем функцию пересчета */
  hyscan_geo_cs_transform_user (&dst_rad, src_rad, el_params_in, el_params_out, datum_param);

  dst->lat = (dst_rad.lat);
  dst->lon = (dst_rad.lon);
  dst->h = (dst_rad.h);

  return TRUE;
}

gboolean
hyscan_geo_cs_transform_user (HyScanGeoGeodetic       *dst,
                              HyScanGeoGeodetic        src,
                              HyScanGeoEllipsoidParam  el_params_in,
                              HyScanGeoEllipsoidParam  el_params_out,
                              HyScanGeoDatumParam      datum_param)
{
  gdouble wx, wy, wz, m_1, x_w, y_w, z_w;
  HyScanGeoCartesian3D ref;
  HyScanGeoGeodetic src_rad, dst_rad;

  /* Проверка на попадание в диапазон. */
  if (LON_OUT_OF_RANGE(src.lon) || LAT_OUT_OF_RANGE(src.lat))
    return FALSE;

  src_rad.lat = DEG2RAD (src.lat);
  src_rad.lon = DEG2RAD (src.lon);
  src_rad.h = (src.h);

  /* Переводим геодезические в декартовы  */
  hyscan_geo_geo2ecef (&ref, src_rad, el_params_in);

  /* Преобразование Гельмерта. */
  wx = datum_param.wx;
  wy = datum_param.wy;
  wz = datum_param.wz;
  m_1 = datum_param.m + 1;
  x_w = m_1 * ( ref.x      +  wz * ref.y - wy * ref.z) + datum_param.dX;
  y_w = m_1 * (-wz * ref.x + ref.y       + wx * ref.z) + datum_param.dY;
  z_w = m_1 * ( wy * ref.x - wx * ref.y  + ref.z     ) + datum_param.dZ;
  ref.x = x_w;
  ref.y = y_w;
  ref.z = z_w;

  /* Переводим декартовы в геодезические */
  hyscan_geo_ecef2geo (&dst_rad, ref, el_params_out);

  dst->lat = RAD2DEG (dst_rad.lat);
  dst->lon = RAD2DEG (dst_rad.lon);
  dst->h = (dst_rad.h);

  return TRUE;
}

/* Параметры для 7-параметрического преобразовния Гельмерта. */
HyScanGeoDatumParam
hyscan_geo_get_helmert_params_to_wgs84 (HyScanGeoCSType cs_type)
{
  HyScanGeoDatumParam params = {0};
  switch (cs_type)
    {
    /* СК-42 на эллипсоиде Крассовского (ГОСТ 51794-2008). */
    case HYSCAN_GEO_CS_SK42:
      params.dX = 23.92;
      params.dY = -141.27;
      params.dZ = -80.9;
      params.wx = 0;
      params.wy = SEC2RAD (-0.35);
      params.wz = SEC2RAD (-0.86);
      params.m = -0.12e-6;
      break;

    /* СК-95 на эллипсоиде Крассовского (ГОСТ 51794-2008). */
    case HYSCAN_GEO_CS_SK95:
      params.dX = 24.8;
      params.dY = -131.24;
      params.dZ = -82.66;
      params.wx = 0;
      params.wy = 0;
      params.wz = SEC2RAD (-0.20);
      params.m = -0.12e-6;
      break;

    /* ПЗ-90 на эллипсоиде ПЗ-90 (ГОСТ 51794-2008). */
    case HYSCAN_GEO_CS_PZ90:
      params.dX = -1.10;
      params.dY = -0.30;
      params.dZ = -0.90;
      params.wx = 0;
      params.wy = 0;
      params.wz = -0.9696e-6;
      params.m = -0.12e-6;
      break;

    /* ПЗ-90.02 в WGS-84 */
    case HYSCAN_GEO_CS_PZ90_02:
      params.dX = -0.03;
      params.dY = -0.27;
      params.dZ = -0.92;
      params.wx = 0;
      params.wy = 0;
      params.wz = SEC2RAD (-0.07);
      params.m = 0.1e-6;
      break;

    /* ПЗ-90.11 на ПЗ-90 (atminst.ru/up_files/seminar_28-05-2013_doklad1.pdf) */
    case HYSCAN_GEO_CS_PZ90_11:
      params.dX = 0.013;
      params.dY = -0.106;
      params.dZ = -0.022;
      params.wx = SEC2RAD ( 2.30);
      params.wy = SEC2RAD (-3.54);
      params.wz = SEC2RAD ( 4.21);
      params.m = 0.008e-6;
      break;
    default:
      break;
    }

  return params;
}

/* Вычисляет параметры датума по типам входной и выходной СК. */
HyScanGeoDatumParam
hyscan_geo_get_datum_params (HyScanGeoCSType cs_in,
                             HyScanGeoCSType cs_out)
{
  HyScanGeoDatumParam in_params, out_params;

  in_params = hyscan_geo_get_helmert_params_to_wgs84 (cs_in);
  out_params = hyscan_geo_get_helmert_params_to_wgs84 (cs_out);

  in_params.dX -= out_params.dX;
  in_params.dY -= out_params.dY;
  in_params.dZ -= out_params.dZ;
  in_params.wx -= out_params.wx;
  in_params.wy -= out_params.wy;
  in_params.wz -= out_params.wz;
  in_params.m  -= out_params.m;

  return in_params;
}

/* Функция возвращает параметры эллипсоида по его типу. */
gboolean
hyscan_geo_get_ellipse_params (gdouble               *a,
                               gdouble               *f,
                               gdouble               *epsg,
                               HyScanGeoEllipsoidType ell_type)
{
  switch (ell_type)
    {
    /* Эллипсоид WGS-84. */
    case HYSCAN_GEO_ELLIPSOID_WGS84:
      {
        *a = 6378137;
        *f = 298.2572236;
        *epsg = 7030;
        return TRUE;
      }

    /* Эллипсоид Крассовского 1942. */
    case HYSCAN_GEO_ELLIPSOID_KRASSOVSKY:
      {
        *a = 6378245;
        *f = 298.3;
        *epsg = 7024;
        return TRUE;
      }

    /* Эллипсоид ПЗ-90 */
    case HYSCAN_GEO_ELLIPSOID_PZ90:
      {
        *a = 6378136;
        *f = 298.2578393;
        *epsg = 7054;
        return TRUE;
      }
    default:
      return FALSE;
    }
}

/* Инициализация эллипсоида по его типу. */
gboolean
hyscan_geo_init_ellipsoid (HyScanGeoEllipsoidParam *p,
                           HyScanGeoEllipsoidType   ell_type)
{
  gdouble a, perc_f, epsg;

  if (hyscan_geo_get_ellipse_params (&a, &perc_f, &epsg, ell_type))
    {
      if (hyscan_geo_init_ellipsoid_user (p, a, 1 / perc_f))
        return TRUE;
    }

  return FALSE;
}

/* Инициализация эллипсоида по параметрам a и f. */
gboolean
hyscan_geo_init_ellipsoid_user (HyScanGeoEllipsoidParam *p,
                                gdouble                  a,
                                gdouble                  f)
{
  if (a == 0 || f == 1.0 || f == 2.0)
    return FALSE;

  p->a = a;
  p->f = f;
  p->b = a * (1.0 - f);
  p->c = a / (1.0 - f);
  p->e = sqrt ( (a * a - p->b * p->b) ) / a;
  p->e2 = f * (2.0 - f);
  p->e12 = p->e2 / (1.0 - p->e2);

  return TRUE;
};
