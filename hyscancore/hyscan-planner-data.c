/* hyscan-planner-data.c
 *
 * Copyright 2019 Screen LLC, Alexey Sakhnov <alexsakhnov@gmail.com>
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

/**
 * SECTION: hyscan-planner-data
 * @Short_description: Базовый класс работы с объектами планировщика
 * @Title: HyScanPlannerData
 *
 * Класс предназначен для чтение и записи данных об объектов планировщика
 * в базу данных.
 *
 */

#include "hyscan-planner-data.h"
#include "hyscan-core-schemas.h"
#include <string.h>

#define PREFIX_ZONE    "zone-"
#define PREFIX_TRACK   "track-"
#define COMMON_ZONE_ID "common"

struct _HyScanPlannerDataPrivate
{
  HyScanParamList             *track_read_plist;
  HyScanParamList             *zone_read_plist;
  HyScanParamList             *origin_read_plist;
};

static void                hyscan_planner_data_object_constructed  (GObject                  *object);
static void                hyscan_planner_data_object_finalize     (GObject                  *object);
static gboolean            hyscan_planner_data_get_full            (HyScanObjectData         *mdata,
                                                                    HyScanParamList          *read_plist,
                                                                    HyScanObject             *object);
static void                hyscan_planner_data_get_zone            (HyScanObjectData         *mdata,
                                                                    HyScanParamList          *plist,
                                                                    HyScanObject             *object);
static void                hyscan_planner_data_get_track           (HyScanObjectData         *mdata,
                                                                    HyScanParamList          *plist,
                                                                    HyScanObject             *object);
static gboolean            hyscan_planner_data_set_full            (HyScanObjectData         *data,
                                                                    HyScanParamList          *write_plist,
                                                                    const HyScanObject       *object);
static gboolean            hyscan_planner_data_set_track           (HyScanObjectData         *data,
                                                                    HyScanParamList          *write_plist,
                                                                    const HyScanPlannerTrack *track);
static gboolean            hyscan_planner_data_set_zone            (HyScanObjectData         *data,
                                                                    HyScanParamList          *write_plist,
                                                                    const HyScanPlannerZone  *zone);
static HyScanObject *      hyscan_planner_data_object_new          (HyScanObjectData         *data,
                                                                    const gchar              *id);
static HyScanObject *      hyscan_planner_data_object_copy         (const HyScanObject       *object);
static void                hyscan_planner_data_object_destroy      (HyScanObject             *object);
static HyScanParamList *   hyscan_planner_data_get_read_plist      (HyScanObjectData         *data,
                                                                    const gchar              *schema_id);
static HyScanGeoGeodetic * hyscan_planner_data_string_to_points    (const gchar              *string,
                                                                    gsize                    *points_len);
static gchar *             hyscan_planner_data_points_to_string    (HyScanGeoGeodetic        *points,
                                                                    gsize                     points_len);
static const gchar *       hyscan_planner_data_get_schema_id       (HyScanObjectData         *data,
                                                                    HyScanObject             *object);
static gchar *             hyscan_planner_data_generate_id         (HyScanObjectData         *data,
                                                                    const HyScanObject       *object);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanPlannerData, hyscan_planner_data, HYSCAN_TYPE_OBJECT_DATA)

static void
hyscan_planner_data_class_init (HyScanPlannerDataClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  HyScanObjectDataClass *data_class = HYSCAN_OBJECT_DATA_CLASS (klass);

  object_class->constructed = hyscan_planner_data_object_constructed;
  object_class->finalize = hyscan_planner_data_object_finalize;

  data_class->group_name = PLANNER_OBJECT;
  data_class->get_schema_id = hyscan_planner_data_get_schema_id;
  data_class->generate_id = hyscan_planner_data_generate_id;
  data_class->get_full = hyscan_planner_data_get_full;
  data_class->set_full = hyscan_planner_data_set_full;
  data_class->object_new = hyscan_planner_data_object_new;
  data_class->object_copy = hyscan_planner_data_object_copy;
  data_class->object_destroy = hyscan_planner_data_object_destroy;
  data_class->get_read_plist = hyscan_planner_data_get_read_plist;
}

static void
hyscan_planner_data_init (HyScanPlannerData *planner_data)
{
  planner_data->priv = hyscan_planner_data_get_instance_private (planner_data);
}

static void
hyscan_planner_data_object_constructed (GObject *object)
{
  HyScanPlannerData *planner_data = HYSCAN_PLANNER_DATA (object);
  HyScanPlannerDataPrivate *priv = planner_data->priv;
  
  G_OBJECT_CLASS (hyscan_planner_data_parent_class)->constructed (object);

  priv->track_read_plist = hyscan_param_list_new ();
  hyscan_param_list_add (priv->track_read_plist, "/schema/id");
  hyscan_param_list_add (priv->track_read_plist, "/schema/version");
  hyscan_param_list_add (priv->track_read_plist, "/zone-id");
  hyscan_param_list_add (priv->track_read_plist, "/number");
  hyscan_param_list_add (priv->track_read_plist, "/speed");
  hyscan_param_list_add (priv->track_read_plist, "/name");
  hyscan_param_list_add (priv->track_read_plist, "/start-lat");
  hyscan_param_list_add (priv->track_read_plist, "/start-lon");
  hyscan_param_list_add (priv->track_read_plist, "/end-lat");
  hyscan_param_list_add (priv->track_read_plist, "/end-lon");

  priv->zone_read_plist = hyscan_param_list_new ();
  hyscan_param_list_add (priv->zone_read_plist, "/schema/id");
  hyscan_param_list_add (priv->zone_read_plist, "/schema/version");
  hyscan_param_list_add (priv->zone_read_plist, "/name");
  hyscan_param_list_add (priv->zone_read_plist, "/vertices");
  hyscan_param_list_add (priv->zone_read_plist, "/ctime");
  hyscan_param_list_add (priv->zone_read_plist, "/mtime");

  priv->origin_read_plist = hyscan_param_list_new ();
  hyscan_param_list_add (priv->origin_read_plist, "/schema/id");
  hyscan_param_list_add (priv->origin_read_plist, "/schema/version");
  hyscan_param_list_add (priv->origin_read_plist, "/lat");
  hyscan_param_list_add (priv->origin_read_plist, "/lon");
  hyscan_param_list_add (priv->origin_read_plist, "/azimuth");
  hyscan_param_list_add (priv->zone_read_plist, "/ctime");
  hyscan_param_list_add (priv->zone_read_plist, "/mtime");
}

static void
hyscan_planner_data_object_finalize (GObject *object)
{
  HyScanPlannerData *planner_data = HYSCAN_PLANNER_DATA (object);
  HyScanPlannerDataPrivate *priv = planner_data->priv;

  g_object_unref (priv->track_read_plist);
  g_object_unref (priv->zone_read_plist);
  g_object_unref (priv->origin_read_plist);
  G_OBJECT_CLASS (hyscan_planner_data_parent_class)->finalize (object);
}

static const gchar *
hyscan_planner_data_get_schema_id (HyScanObjectData *data,
                                   HyScanObject     *object_)
{
  HyScanPlannerObject *object = object_;

  if (object->type == HYSCAN_PLANNER_ZONE)
    return PLANNER_ZONE_SCHEMA;
  else if (object->type == HYSCAN_PLANNER_TRACK)
    return PLANNER_TRACK_SCHEMA;
  else if (object->type == HYSCAN_PLANNER_ORIGIN)
    return PLANNER_ORIGIN_SCHEMA;
  else
    return NULL;
}

static gchar *
hyscan_planner_data_generate_id (HyScanObjectData   *data,
                                 const HyScanObject *object_)
{
  gchar *unique_id;
  gchar *id = NULL;
  const HyScanPlannerObject *object = object_;

  unique_id = HYSCAN_OBJECT_DATA_CLASS (hyscan_planner_data_parent_class)->generate_id (data, object);
  if (object->type == HYSCAN_PLANNER_ZONE)
    {
      id = g_strconcat (PREFIX_ZONE, unique_id, NULL);
    }

  else if (object->type == HYSCAN_PLANNER_TRACK)
    {
      const gchar *zone_id = object->track.zone_id;

      if (zone_id != NULL && g_str_has_prefix (zone_id, PREFIX_ZONE))
        zone_id = zone_id + strlen (PREFIX_ZONE);
      else
        zone_id = COMMON_ZONE_ID;

      id = g_strconcat (PREFIX_TRACK, zone_id, "/", unique_id, NULL);
    }

  else if (object->type == HYSCAN_PLANNER_ORIGIN)
    {
      id = g_strdup (HYSCAN_PLANNER_ORIGIN_ID);
    }

  g_free (unique_id);
  return id;
}

static inline gboolean
hyscan_planner_data_zone_validate_id (const gchar *zone_id)
{
  return zone_id != NULL && g_str_has_prefix (zone_id, PREFIX_ZONE);
}

static inline gboolean
hyscan_planner_data_track_validate_id (const gchar *track_id)
{
  return track_id != NULL && g_str_has_prefix (track_id, PREFIX_TRACK);
}

static inline gboolean
hyscan_planner_data_origin_validate_id (const gchar *origin_id)
{
  return g_strcmp0 (origin_id, HYSCAN_PLANNER_ORIGIN_ID);
}

static HyScanObject *
hyscan_planner_data_object_new (HyScanObjectData *data,
                                const gchar    *id)
{
  HyScanPlannerObject *object;

  object = g_slice_new0 (HyScanPlannerObject);

  if (hyscan_planner_data_track_validate_id (id))
    object->type = HYSCAN_PLANNER_TRACK;
  else if (hyscan_planner_data_zone_validate_id (id))
    object->type = HYSCAN_PLANNER_ZONE;
  else if (hyscan_planner_data_origin_validate_id (id))
    object->type = HYSCAN_PLANNER_ORIGIN;
  else
    object->type = HYSCAN_PLANNER_INVALID;

  return object;
}

static void
hyscan_planner_data_object_destroy (HyScanObject *object)
{
  HyScanPlannerObject *planner_object = object;

  if (planner_object->type == HYSCAN_PLANNER_ZONE)
    hyscan_planner_zone_free (&planner_object->zone);
  else if (planner_object->type == HYSCAN_PLANNER_TRACK)
    hyscan_planner_track_free (&planner_object->track);
  else if (planner_object->type == HYSCAN_PLANNER_ORIGIN)
    hyscan_planner_origin_free (&planner_object->ref_point);
  else
    g_warn_if_reached ();
}

static HyScanObject *
hyscan_planner_data_object_copy (const HyScanObject *object)
{
  const HyScanPlannerObject *planner_object = object;

  if (planner_object->type == HYSCAN_PLANNER_ZONE)
    return hyscan_planner_zone_copy (&planner_object->zone);
  else if (planner_object->type == HYSCAN_PLANNER_TRACK)
    return hyscan_planner_track_copy (&planner_object->track);
  else if (planner_object->type == HYSCAN_PLANNER_ORIGIN)
    return hyscan_planner_origin_copy (&planner_object->ref_point);
  else
    g_return_val_if_reached (NULL);
}

static HyScanParamList *
hyscan_planner_data_get_read_plist (HyScanObjectData *data,
                                    const gchar    *schema_id)
{
  HyScanPlannerData *planner_data = HYSCAN_PLANNER_DATA (data);
  HyScanPlannerDataPrivate *priv = planner_data->priv;

  if (g_str_equal (schema_id, PLANNER_TRACK_SCHEMA))
    return g_object_ref (priv->track_read_plist);
  else if (g_str_equal (schema_id, PLANNER_ZONE_SCHEMA))
    return g_object_ref (priv->zone_read_plist);
  else if (g_str_equal (schema_id, PLANNER_ORIGIN_SCHEMA))
    return g_object_ref (priv->origin_read_plist);
  else
    g_return_val_if_reached (NULL);
}

static HyScanGeoGeodetic *
hyscan_planner_data_string_to_points (const gchar *string,
                                      gsize       *points_len)
{
  GArray *array;

  array = g_array_new (FALSE, FALSE, sizeof (HyScanGeoGeodetic));

  while (*string != '\0') {
    gchar *num_end;
    HyScanGeoGeodetic point;

    point.lat = g_ascii_strtod (string, &num_end);
    if (string == num_end || *num_end != ',')
      break;

    string = num_end + 1;

    point.lon = g_ascii_strtod (string, &num_end);
    if (string == num_end || *num_end != ' ')
      break;

    string = num_end + 1;

    g_array_append_val (array, point);
  }

  *points_len = array->len;

  return (HyScanGeoGeodetic *) g_array_free (array, FALSE);
}

static gchar *
hyscan_planner_data_points_to_string (HyScanGeoGeodetic *points,
                                      gsize              points_len)
{
  gchar *vertices;
  gchar *vertex, *vertices_end;
  gsize i;
  gsize max_len;

  if (points_len == 0)
    return g_strdup ("");

  /* Число символов для записи вершины: -123.12345678,-123.12345678. */
  max_len = 32 * points_len + 1;
  vertices = g_new (gchar, max_len);
  vertices_end = vertices + max_len;

  vertex = vertices;
  for (i = 0; i < points_len; ++i)
    {
      gint n_bytes;

      n_bytes = g_snprintf (vertex, (gulong) (vertices_end - vertex),
                            "%.8f,%.8f ", points[i].lat, points[i].lon);
      vertex = vertex + n_bytes;
    }

  return vertices;
}

static void
hyscan_planner_data_get_zone (HyScanObjectData *mdata,
                              HyScanParamList  *plist,
                              HyScanObject     *object)
{
  HyScanPlannerZone *zone = object;
  const gchar *vertices;
  
  zone->type = HYSCAN_PLANNER_ZONE;
  zone->name = hyscan_param_list_dup_string (plist, "/name");
  zone->mtime = hyscan_param_list_get_integer (plist, "/mtime");
  zone->ctime = hyscan_param_list_get_integer (plist, "/ctime");

  vertices = hyscan_param_list_get_string (plist, "/vertices");
  zone->points = hyscan_planner_data_string_to_points (vertices, &zone->points_len);
}

static void
hyscan_planner_data_get_track (HyScanObjectData *mdata,
                               HyScanParamList  *plist,
                               HyScanObject     *object)
{
  HyScanPlannerTrack *track = object;

  track->type = HYSCAN_PLANNER_TRACK;
  track->zone_id = hyscan_param_list_dup_string (plist, "/zone-id");
  track->number = hyscan_param_list_get_integer (plist, "/number");
  track->speed = hyscan_param_list_get_double (plist, "/speed");
  track->name = hyscan_param_list_dup_string (plist, "/name");
  track->start.lat = hyscan_param_list_get_double (plist, "/start-lat");
  track->start.lon = hyscan_param_list_get_double (plist, "/start-lon");
  track->end.lat = hyscan_param_list_get_double (plist, "/end-lat");
  track->end.lon = hyscan_param_list_get_double (plist, "/end-lon");
}

static void
hyscan_planner_data_get_origin (HyScanObjectData *mdata,
                                HyScanParamList  *plist,
                                HyScanObject     *object)
{
  HyScanPlannerOrigin *origin = object;

  origin->type = HYSCAN_PLANNER_ORIGIN;
  origin->origin.lat = hyscan_param_list_get_double (plist, "/lat");
  origin->origin.lon = hyscan_param_list_get_double (plist, "/lon");
  origin->origin.h = hyscan_param_list_get_double (plist, "/azimuth");
}

static gboolean
hyscan_planner_data_get_full (HyScanObjectData *mdata,
                              HyScanParamList  *read_plist,
                              HyScanObject     *object)
{
  gint64 sid, sver;

  sid = hyscan_param_list_get_integer (read_plist, "/schema/id");
  sver = hyscan_param_list_get_integer (read_plist, "/schema/version");

  if (sid == PLANNER_ZONE_SCHEMA_ID && sver == PLANNER_ZONE_SCHEMA_VERSION)
    {
      if (object != NULL)
        hyscan_planner_data_get_zone (mdata, read_plist, object);

      return TRUE;
    }

  else if (sid == PLANNER_TRACK_SCHEMA_ID && sver == PLANNER_ZONE_SCHEMA_VERSION)
    {
      if (object != NULL)
        hyscan_planner_data_get_track (mdata, read_plist, object);

      return TRUE;
    }

  else if (sid == PLANNER_ORIGIN_SCHEMA_ID && sver == PLANNER_ORIGIN_SCHEMA_VERSION)
    {
      if (object != NULL)
        hyscan_planner_data_get_origin (mdata, read_plist, object);

      return TRUE;
    }

  return FALSE;
}

static gboolean
hyscan_planner_data_set_track (HyScanObjectData           *data,
                               HyScanParamList          *write_plist,
                               const HyScanPlannerTrack *track)
{
  const gchar *zone_id;

  zone_id = track->zone_id != NULL ? track->zone_id : NULL;
  hyscan_param_list_set_string (write_plist, "/zone-id", zone_id);
  hyscan_param_list_set_string (write_plist, "/name", track->name);
  hyscan_param_list_set_integer (write_plist, "/number", track->number);
  hyscan_param_list_set_double (write_plist, "/speed", track->speed);
  hyscan_param_list_set_double (write_plist, "/start-lat", track->start.lat);
  hyscan_param_list_set_double (write_plist, "/start-lon", track->start.lon);
  hyscan_param_list_set_double (write_plist, "/end-lat", track->end.lat);
  hyscan_param_list_set_double (write_plist, "/end-lon", track->end.lon);

  return TRUE;
}

static gboolean
hyscan_planner_data_set_origin (HyScanObjectData          *data,
                                HyScanParamList           *write_plist,
                                const HyScanPlannerOrigin *origin)
{
  hyscan_param_list_set_double (write_plist, "/lat", origin->origin.lat);
  hyscan_param_list_set_double (write_plist, "/lon", origin->origin.lon);
  hyscan_param_list_set_double (write_plist, "/azimuth", origin->origin.h);

  return TRUE;
}

static gboolean
hyscan_planner_data_set_zone (HyScanObjectData          *data,
                              HyScanParamList         *write_plist,
                              const HyScanPlannerZone *zone)
{
  gchar *vertices;

  hyscan_param_list_set_string (write_plist, "/name", zone->name);

  vertices = hyscan_planner_data_points_to_string (zone->points, zone->points_len);
  hyscan_param_list_set_string (write_plist, "/vertices", vertices);
  g_free (vertices);

  hyscan_param_list_set_integer (write_plist, "/mtime", zone->mtime);
  hyscan_param_list_set_integer (write_plist, "/ctime", zone->ctime);

  return TRUE;
}

static gboolean
hyscan_planner_data_set_full (HyScanObjectData   *mdata,
                              HyScanParamList    *write_plist,
                              const HyScanObject *object)
{
  const HyScanPlannerObject *planner_object = object;

  if (planner_object->type == HYSCAN_PLANNER_ZONE)
    return hyscan_planner_data_set_zone (mdata, write_plist, object);
  else if (planner_object->type == HYSCAN_PLANNER_TRACK)
    return hyscan_planner_data_set_track (mdata, write_plist, object);
  else if (planner_object->type == HYSCAN_PLANNER_ORIGIN)
    return hyscan_planner_data_set_origin (mdata, write_plist, object);

  return FALSE;
}

HyScanObjectData *
hyscan_planner_data_new (HyScanDB    *db,
                         const gchar *project)
{
  HyScanObjectData * data;

  data = g_object_new (HYSCAN_TYPE_PLANNER_DATA,
                       "db", db,
                       "project", project,
                       NULL);

  if (!hyscan_object_data_is_ready (data))
    g_clear_object (&data);

  return data;
}
