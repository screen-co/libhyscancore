/* hyscan-object-data-planner.c
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
 * @Title: HyScanObjectDataPlanner
 *
 * Класс предназначен для чтение и записи данных об объектов планировщика
 * в базу данных.
 *
 */

#include "hyscan-object-data-planner.h"
#include "hyscan-core-schemas.h"
#include <string.h>

#define PREFIX_ZONE    "zone-"     /* Префикс объекта зоны. */
#define PREFIX_TRACK   "track-"    /* Префикс объекта галса. */

struct _HyScanObjectDataPlannerPrivate
{
  HyScanParamList             *track_read_plist;    /* Список параметров для чтения объекта галса. */
  HyScanParamList             *zone_read_plist;     /* Список параметров для чтения объекта зоны. */
  HyScanParamList             *origin_read_plist;   /* Список параметров для чтения объекта точки отсчёта. */
};

static void                hyscan_object_data_planner_object_constructed  (GObject                  *object);
static void                hyscan_object_data_planner_object_finalize     (GObject                  *object);
static HyScanObject *      hyscan_object_data_planner_get_full            (HyScanObjectData         *mdata,
                                                                           HyScanParamList          *read_plist);
static HyScanObject *      hyscan_object_data_planner_get_zone            (HyScanObjectData         *mdata,
                                                                           HyScanParamList          *plist);
static HyScanObject *      hyscan_object_data_planner_get_track           (HyScanObjectData         *mdata,
                                                                           HyScanParamList          *plist);
static HyScanObject *      hyscan_object_data_planner_get_origin          (HyScanObjectData         *mdata,
                                                                           HyScanParamList          *plist);
static gboolean            hyscan_object_data_planner_set_full            (HyScanObjectData         *data,
                                                                           HyScanParamList          *write_plist,
                                                                           const HyScanObject       *object);
static gboolean            hyscan_object_data_planner_set_track           (HyScanObjectData         *data,
                                                                           HyScanParamList          *write_plist,
                                                                           const HyScanPlannerTrack *track);
static gboolean            hyscan_object_data_planner_set_zone            (HyScanObjectData         *data,
                                                                           HyScanParamList          *write_plist,
                                                                           const HyScanPlannerZone  *zone);
static HyScanObject *      hyscan_object_data_planner_object_copy         (const HyScanObject       *object);
static void                hyscan_object_data_planner_object_destroy      (HyScanObject             *object);
static HyScanParamList *   hyscan_object_data_planner_get_read_plist      (HyScanObjectData         *data,
                                                                           const gchar              *id);
static HyScanGeoGeodetic * hyscan_object_data_planner_string_to_points    (const gchar              *string,
                                                                           gsize                    *points_len);
static gchar *             hyscan_object_data_planner_points_to_string    (HyScanGeoGeodetic        *points,
                                                                           gsize                     points_len);
static const gchar *       hyscan_object_data_planner_get_schema_id       (HyScanObjectData         *data,
                                                                           const HyScanObject       *object);
static gchar *             hyscan_object_data_planner_generate_id         (HyScanObjectData         *data,
                                                                           const HyScanObject       *object);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanObjectDataPlanner, hyscan_object_data_planner, HYSCAN_TYPE_OBJECT_DATA)

static void
hyscan_object_data_planner_class_init (HyScanObjectDataPlannerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  HyScanObjectDataClass *data_class = HYSCAN_OBJECT_DATA_CLASS (klass);

  object_class->constructed = hyscan_object_data_planner_object_constructed;
  object_class->finalize = hyscan_object_data_planner_object_finalize;

  data_class->group_name = PLANNER_OBJECT;
  data_class->get_schema_id = hyscan_object_data_planner_get_schema_id;
  data_class->generate_id = hyscan_object_data_planner_generate_id;
  data_class->get_full = hyscan_object_data_planner_get_full;
  data_class->set_full = hyscan_object_data_planner_set_full;
  data_class->object_copy = hyscan_object_data_planner_object_copy;
  data_class->object_destroy = hyscan_object_data_planner_object_destroy;
  data_class->get_read_plist = hyscan_object_data_planner_get_read_plist;
}

static void
hyscan_object_data_planner_init (HyScanObjectDataPlanner *object_data_planner)
{
  object_data_planner->priv = hyscan_object_data_planner_get_instance_private (object_data_planner);
}

static void
hyscan_object_data_planner_object_constructed (GObject *object)
{
  HyScanObjectDataPlanner *object_data_planner = HYSCAN_OBJECT_DATA_PLANNER (object);
  HyScanObjectDataPlannerPrivate *priv = object_data_planner->priv;
  
  G_OBJECT_CLASS (hyscan_object_data_planner_parent_class)->constructed (object);

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
hyscan_object_data_planner_object_finalize (GObject *object)
{
  HyScanObjectDataPlanner *object_data_planner = HYSCAN_OBJECT_DATA_PLANNER (object);
  HyScanObjectDataPlannerPrivate *priv = object_data_planner->priv;

  g_object_unref (priv->track_read_plist);
  g_object_unref (priv->zone_read_plist);
  g_object_unref (priv->origin_read_plist);
  G_OBJECT_CLASS (hyscan_object_data_planner_parent_class)->finalize (object);
}

static const gchar *
hyscan_object_data_planner_get_schema_id (HyScanObjectData   *data,
                                          const HyScanObject *object)
{
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
hyscan_object_data_planner_generate_id (HyScanObjectData   *data,
                                        const HyScanObject *object)
{
  gchar *unique_id;
  gchar *id = NULL;

  unique_id = HYSCAN_OBJECT_DATA_CLASS (hyscan_object_data_planner_parent_class)->generate_id (data, object);

  if (object->type == HYSCAN_PLANNER_ZONE)
    id = g_strconcat (PREFIX_ZONE, unique_id, NULL);
  else if (object->type == HYSCAN_PLANNER_TRACK)
    id = g_strconcat (PREFIX_TRACK, unique_id, NULL);
  else if (object->type == HYSCAN_PLANNER_ORIGIN)
    id = g_strdup (HYSCAN_PLANNER_ORIGIN_ID);

  g_free (unique_id);

  return id;
}

static inline gboolean
hyscan_object_data_planner_zone_validate_id (const gchar *zone_id)
{
  return zone_id != NULL && g_str_has_prefix (zone_id, PREFIX_ZONE);
}

static inline gboolean
hyscan_object_data_planner_track_validate_id (const gchar *track_id)
{
  return track_id != NULL && g_str_has_prefix (track_id, PREFIX_TRACK);
}

static inline gboolean
hyscan_object_data_planner_origin_validate_id (const gchar *origin_id)
{
  return g_strcmp0 (origin_id, HYSCAN_PLANNER_ORIGIN_ID) == 0;
}

static void
hyscan_object_data_planner_object_destroy (HyScanObject *object)
{
  if (object == NULL)
    return;

  if (object->type == HYSCAN_PLANNER_ZONE)
    hyscan_planner_zone_free ((HyScanPlannerZone *) object);
  else if (object->type == HYSCAN_PLANNER_TRACK)
    hyscan_planner_track_free ((HyScanPlannerTrack *) object);
  else if (object->type == HYSCAN_PLANNER_ORIGIN)
    hyscan_planner_origin_free ((HyScanPlannerOrigin *) object);
  else
    g_warn_if_reached ();
}

static HyScanObject *
hyscan_object_data_planner_object_copy (const HyScanObject *object)
{
  if (object == NULL)
    return NULL;

  if (object->type == HYSCAN_PLANNER_ZONE)
    return (HyScanObject *) hyscan_planner_zone_copy ((HyScanPlannerZone *) object);
  else if (object->type == HYSCAN_PLANNER_TRACK)
    return (HyScanObject *) hyscan_planner_track_copy ((HyScanPlannerTrack *) object);
  else if (object->type == HYSCAN_PLANNER_ORIGIN)
    return (HyScanObject *) hyscan_planner_origin_copy ((HyScanPlannerOrigin *) object);
  else
    g_return_val_if_reached (NULL);
}

static HyScanParamList *
hyscan_object_data_planner_get_read_plist (HyScanObjectData *data,
                                           const gchar      *id)
{
  HyScanObjectDataPlanner *object_data_planner = HYSCAN_OBJECT_DATA_PLANNER (data);
  HyScanObjectDataPlannerPrivate *priv = object_data_planner->priv;

  if (hyscan_object_data_planner_track_validate_id (id))
    return g_object_ref (priv->track_read_plist);
  else if (hyscan_object_data_planner_zone_validate_id (id))
    return g_object_ref (priv->zone_read_plist);
  else if (hyscan_object_data_planner_origin_validate_id (id))
    return g_object_ref (priv->origin_read_plist);
  else
    g_return_val_if_reached (NULL);
}

static HyScanGeoGeodetic *
hyscan_object_data_planner_string_to_points (const gchar *string,
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
hyscan_object_data_planner_points_to_string (HyScanGeoGeodetic *points,
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

static HyScanObject *
hyscan_object_data_planner_get_zone (HyScanObjectData *mdata,
                                     HyScanParamList  *plist)
{
  HyScanPlannerZone *zone;
  const gchar *vertices;

  zone = hyscan_planner_zone_new ();
  zone->name = hyscan_param_list_dup_string (plist, "/name");
  zone->mtime = hyscan_param_list_get_integer (plist, "/mtime");
  zone->ctime = hyscan_param_list_get_integer (plist, "/ctime");

  vertices = hyscan_param_list_get_string (plist, "/vertices");
  zone->points = hyscan_object_data_planner_string_to_points (vertices, &zone->points_len);

  return (HyScanObject *) zone;
}

static HyScanObject *
hyscan_object_data_planner_get_track (HyScanObjectData *mdata,
                                      HyScanParamList  *plist)
{
  HyScanPlannerTrack *track;

  track = hyscan_planner_track_new ();
  track->zone_id = hyscan_param_list_dup_string (plist, "/zone-id");
  track->number = hyscan_param_list_get_integer (plist, "/number");
  track->speed = hyscan_param_list_get_double (plist, "/speed");
  track->name = hyscan_param_list_dup_string (plist, "/name");
  track->start.lat = hyscan_param_list_get_double (plist, "/start-lat");
  track->start.lon = hyscan_param_list_get_double (plist, "/start-lon");
  track->end.lat = hyscan_param_list_get_double (plist, "/end-lat");
  track->end.lon = hyscan_param_list_get_double (plist, "/end-lon");

  return (HyScanObject *) track;
}

static HyScanObject *
hyscan_object_data_planner_get_origin (HyScanObjectData *mdata,
                                       HyScanParamList  *plist)
{
  HyScanPlannerOrigin *origin;

  origin = hyscan_planner_origin_new ();
  origin->origin.lat = hyscan_param_list_get_double (plist, "/lat");
  origin->origin.lon = hyscan_param_list_get_double (plist, "/lon");
  origin->origin.h = hyscan_param_list_get_double (plist, "/azimuth");

  return (HyScanObject *) origin;
}

static HyScanObject *
hyscan_object_data_planner_get_full (HyScanObjectData *mdata,
                                     HyScanParamList  *read_plist)
{
  gint64 sid, sver;

  sid = hyscan_param_list_get_integer (read_plist, "/schema/id");
  sver = hyscan_param_list_get_integer (read_plist, "/schema/version");

  if (sid == PLANNER_ZONE_SCHEMA_ID && sver == PLANNER_ZONE_SCHEMA_VERSION)
    return hyscan_object_data_planner_get_zone (mdata, read_plist);

  else if (sid == PLANNER_TRACK_SCHEMA_ID && sver == PLANNER_ZONE_SCHEMA_VERSION)
    return hyscan_object_data_planner_get_track (mdata, read_plist);

  else if (sid == PLANNER_ORIGIN_SCHEMA_ID && sver == PLANNER_ORIGIN_SCHEMA_VERSION)
    return hyscan_object_data_planner_get_origin (mdata, read_plist);

  return NULL;
}

static gboolean
hyscan_object_data_planner_set_track (HyScanObjectData         *data,
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
hyscan_object_data_planner_set_origin (HyScanObjectData          *data,
                                       HyScanParamList           *write_plist,
                                       const HyScanPlannerOrigin *origin)
{
  hyscan_param_list_set_double (write_plist, "/lat", origin->origin.lat);
  hyscan_param_list_set_double (write_plist, "/lon", origin->origin.lon);
  hyscan_param_list_set_double (write_plist, "/azimuth", origin->origin.h);

  return TRUE;
}

static gboolean
hyscan_object_data_planner_set_zone (HyScanObjectData        *data,
                                     HyScanParamList         *write_plist,
                                     const HyScanPlannerZone *zone)
{
  gchar *vertices;

  hyscan_param_list_set_string (write_plist, "/name", zone->name);

  vertices = hyscan_object_data_planner_points_to_string (zone->points, zone->points_len);
  hyscan_param_list_set_string (write_plist, "/vertices", vertices);
  g_free (vertices);

  hyscan_param_list_set_integer (write_plist, "/mtime", zone->mtime);
  hyscan_param_list_set_integer (write_plist, "/ctime", zone->ctime);

  return TRUE;
}

static gboolean
hyscan_object_data_planner_set_full (HyScanObjectData   *mdata,
                                     HyScanParamList    *write_plist,
                                     const HyScanObject *object)
{
  if (object->type == HYSCAN_PLANNER_ZONE)
    return hyscan_object_data_planner_set_zone (mdata, write_plist, (const HyScanPlannerZone *) object);
  else if (object->type == HYSCAN_PLANNER_TRACK)
    return hyscan_object_data_planner_set_track (mdata, write_plist, (const HyScanPlannerTrack *) object);
  else if (object->type == HYSCAN_PLANNER_ORIGIN)
    return hyscan_object_data_planner_set_origin (mdata, write_plist, (const HyScanPlannerOrigin *) object);

  return FALSE;
}

/**
 * hyscan_object_data_planner_new:
 * @db: база данных #HyScanDB
 * @project: имя проекта
 *
 * Returns: указатель на новый объект #HyScanObjectDataPlanner. Для удаления g_object_unref().
 */
HyScanObjectData *
hyscan_object_data_planner_new (HyScanDB    *db,
                                const gchar *project)
{
  HyScanObjectData * data;

  data = g_object_new (HYSCAN_TYPE_OBJECT_DATA_PLANNER,
                       "db", db,
                       "project", project,
                       NULL);

  if (!hyscan_object_data_is_ready (data))
    g_clear_object (&data);

  return data;
}
