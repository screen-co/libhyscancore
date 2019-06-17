#include "hyscan-mark-data-geo.h"
#include "hyscan-core-schemas.h"

static void           hyscan_mark_data_geo_init_plist  (HyScanMarkData    *data,
                                                        HyScanParamList   *plist);

static void           hyscan_mark_data_geo_get         (HyScanMarkData    *data,
                                                        HyScanParamList   *read_plist,
                                                        HyScanMark        *mark);

static void           hyscan_mark_data_geo_set         (HyScanMarkData    *data,
                                                        HyScanParamList   *write_plist,
                                                        const HyScanMark  *mark);

G_DEFINE_TYPE (HyScanMarkDataGeo, hyscan_mark_data_geo, HYSCAN_TYPE_MARK_DATA);

static void
hyscan_mark_data_geo_class_init (HyScanMarkDataGeoClass *klass)
{
  HyScanMarkDataClass *data_class = HYSCAN_MARK_DATA_CLASS (klass);

  data_class->mark_type = HYSCAN_MARK_GEO;
  data_class->schema_id = GEO_MARK_SCHEMA;
  data_class->param_sid = GEO_MARK_SCHEMA_ID;
  data_class->param_sver = GEO_MARK_SCHEMA_VERSION;

  data_class->init_plist = hyscan_mark_data_geo_init_plist;
  data_class->set = hyscan_mark_data_geo_set;
  data_class->get = hyscan_mark_data_geo_get;
}

static void
hyscan_mark_data_geo_init (HyScanMarkDataGeo *mark_data_geo)
{

}

/* Функция добавляет названия параметров в списки. */
static void
hyscan_mark_data_geo_init_plist (HyScanMarkData  *data,
                                 HyScanParamList *plist)
{
  hyscan_param_list_add (plist, "/coordinates/lat");
  hyscan_param_list_add (plist, "/coordinates/lon");
}

/* Функция считывает содержимое объекта. */
static void
hyscan_mark_data_geo_get (HyScanMarkData  *data,
                          HyScanParamList *read_plist,
                          HyScanMark      *mark)
{
  HyScanMarkGeo *mark_geo;
  HyScanGeoGeodetic coord;

  g_return_if_fail (mark->type == HYSCAN_MARK_GEO);
  mark_geo = (HyScanMarkGeo *) mark;

  coord.lat = hyscan_param_list_get_double (read_plist, "/coordinates/lat");
  coord.lon = hyscan_param_list_get_double (read_plist, "/coordinates/lon");
  hyscan_mark_geo_set_center (mark_geo, coord);
}

/* Функция записывает значения в существующий объект. */
static void
hyscan_mark_data_geo_set (HyScanMarkData   *data,
                          HyScanParamList  *write_plist,
                          const HyScanMark *mark)
{
  HyScanMarkGeo *mark_geo;

  g_return_if_fail (mark->type == HYSCAN_MARK_GEO);
  mark_geo = (HyScanMarkGeo *) mark;

  hyscan_param_list_set_double (write_plist, "/coordinates/lat", mark_geo->center.lat);
  hyscan_param_list_set_double (write_plist, "/coordinates/lon", mark_geo->center.lon);
}
