/* hyscan-object-data-planner.c
 *
 * Copyright 2019 Screen LLC, Alexey Sakhnov <alexsakhnov@gmail.com>
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
 * SECTION: hyscan-object-data-planner
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
#define OBJECT_ID_LEN   20         /* Длина случайно части идентификатора объекта. */

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
static HyScanParamList *   hyscan_object_data_planner_get_read_plist      (HyScanObjectData         *data,
                                                                           const gchar              *id);
static HyScanGeoPoint *    hyscan_object_data_planner_string_to_points    (const gchar              *string,
                                                                           gsize                    *points_len);
static gchar *             hyscan_object_data_planner_points_to_string    (HyScanGeoPoint           *points,
                                                                           gsize                     points_len);
static const gchar *       hyscan_object_data_planner_get_schema_id       (HyScanObjectData         *data,
                                                                           const HyScanObject       *object);
static gchar *             hyscan_object_data_planner_generate_id         (HyScanObjectData         *data,
                                                                           const HyScanObject       *object);
static GType               hyscan_object_data_planner_get_object_type     (HyScanObjectData         *data,
                                                                           const gchar              *id);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanObjectDataPlanner, hyscan_object_data_planner, HYSCAN_TYPE_OBJECT_DATA)

static void
hyscan_object_data_planner_class_init (HyScanObjectDataPlannerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  HyScanObjectDataClass *data_class = HYSCAN_OBJECT_DATA_CLASS (klass);
  static GType types[3];

  types[0] = HYSCAN_TYPE_PLANNER_TRACK;
  types[1] = HYSCAN_TYPE_PLANNER_ZONE;
  types[2] = HYSCAN_TYPE_PLANNER_ORIGIN;

  object_class->constructed = hyscan_object_data_planner_object_constructed;
  object_class->finalize = hyscan_object_data_planner_object_finalize;

  data_class->group_name = PLANNER_OBJECT;
  data_class->data_types = types;
  data_class->n_data_types = G_N_ELEMENTS (types);

  data_class->get_schema_id = hyscan_object_data_planner_get_schema_id;
  data_class->generate_id = hyscan_object_data_planner_generate_id;
  data_class->get_full = hyscan_object_data_planner_get_full;
  data_class->set_full = hyscan_object_data_planner_set_full;
  data_class->get_read_plist = hyscan_object_data_planner_get_read_plist;
  data_class->get_object_type = hyscan_object_data_planner_get_object_type;
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
  hyscan_param_list_add (priv->track_read_plist, "/records");
  hyscan_param_list_add (priv->track_read_plist, "/start/lat");
  hyscan_param_list_add (priv->track_read_plist, "/start/lon");
  hyscan_param_list_add (priv->track_read_plist, "/end/lat");
  hyscan_param_list_add (priv->track_read_plist, "/end/lon");

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

/* HyScanObjectDataClass.get_schema_id.
 * Возвращает идентификатор схемы объекта. */
static const gchar *
hyscan_object_data_planner_get_schema_id (HyScanObjectData   *data,
                                          const HyScanObject *object)
{
  if (HYSCAN_IS_PLANNER_ZONE (object))
    return PLANNER_ZONE_SCHEMA;
  else if (HYSCAN_IS_PLANNER_TRACK (object))
    return PLANNER_TRACK_SCHEMA;
  else if (HYSCAN_IS_PLANNER_ORIGIN (object))
    return PLANNER_ORIGIN_SCHEMA;
  else
    return NULL;
}

/* HyScanObjectDataClass.generate_id.
 * Генерирует идентификатор для нового объекта. */
static gchar *
hyscan_object_data_planner_generate_id (HyScanObjectData   *data,
                                        const HyScanObject *object)
{
  gchar *id = NULL;
  const gchar *prefix;
  guint buf_size;

  if (HYSCAN_IS_PLANNER_ORIGIN (object))
    return g_strdup (HYSCAN_PLANNER_ORIGIN_ID);

  if (HYSCAN_IS_PLANNER_ZONE (object))
    prefix = PREFIX_ZONE;
  else if (HYSCAN_IS_PLANNER_TRACK (object))
    prefix = PREFIX_TRACK;
  else
    g_return_val_if_reached (NULL);

  buf_size = strlen (prefix) + OBJECT_ID_LEN;

  id = g_new (gchar, buf_size);
  g_strlcpy (id, prefix, buf_size);

  hyscan_rand_id (id + strlen (prefix), OBJECT_ID_LEN);
  return id;
}

/* HyScanObjectDataClass.get_object_type.
 * Получает тип объекта по его идентификатору. */
static GType
hyscan_object_data_planner_get_object_type (HyScanObjectData   *data,
                                            const gchar        *id)
{
  g_return_val_if_fail (id != NULL, G_TYPE_INVALID);

  if (g_str_has_prefix (id, PREFIX_ZONE))
    return HYSCAN_TYPE_PLANNER_ZONE;
  else if (g_str_has_prefix (id, PREFIX_TRACK))
    return HYSCAN_TYPE_PLANNER_TRACK;
  else if (g_str_equal (id, HYSCAN_PLANNER_ORIGIN_ID))
    return HYSCAN_TYPE_PLANNER_ORIGIN;

  return G_TYPE_INVALID;
}

/* HyScanObjectDataClass.get_read_plist.
 * Возвращает список параметров для чтения по идентификатору объекта. */
static HyScanParamList *
hyscan_object_data_planner_get_read_plist (HyScanObjectData *data,
                                           const gchar      *id)
{
  HyScanObjectDataPlanner *object_data_planner = HYSCAN_OBJECT_DATA_PLANNER (data);
  HyScanObjectDataPlannerPrivate *priv = object_data_planner->priv;
  GType type;

  type = hyscan_object_data_planner_get_object_type (data, id);

  if (type == HYSCAN_TYPE_PLANNER_TRACK)
    return g_object_ref (priv->track_read_plist);
  else if (type == HYSCAN_TYPE_PLANNER_ZONE)
    return g_object_ref (priv->zone_read_plist);
  else if (type == HYSCAN_TYPE_PLANNER_ORIGIN)
    return g_object_ref (priv->origin_read_plist);
  else
    g_return_val_if_reached (NULL);
}

/* Получает массив геоточек из строки. */
static HyScanGeoPoint *
hyscan_object_data_planner_string_to_points (const gchar *string,
                                             gsize       *points_len)
{
  GArray *array;

  array = g_array_new (FALSE, FALSE, sizeof (HyScanGeoPoint));
  if (string == NULL)
    goto exit;

  while (*string != '\0') {
    gchar *num_end;
    HyScanGeoPoint point;

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

exit:
  *points_len = array->len;

  return (HyScanGeoPoint *) g_array_free (array, FALSE);
}

/* Переводит массив геоточек в строку. */
static gchar *
hyscan_object_data_planner_points_to_string (HyScanGeoPoint    *points,
                                             gsize              points_len)
{
  gchar *vertices;
  gchar *vertex;
  gsize i;
  gsize max_len;
  gint buf_len = 16;

  if (points_len == 0)
    return g_strdup ("");

  /* Число символов для записи одного числа buf_len: {lat,lon} = "-123.1234567890\0".
   * Число символов для записи вершины: "{lat},{lon} ". */
  max_len = (buf_len + 1 + buf_len + 1) * points_len + 1;
  vertices = g_new (gchar, max_len);

  vertex = vertices;
  for (i = 0; i < points_len; ++i)
    {
      g_ascii_dtostr (vertex, buf_len, points[i].lat);
      vertex += strlen (vertex);

      *vertex = ',';
      vertex += 1;

      g_ascii_dtostr (vertex, buf_len, points[i].lon);
      vertex += strlen (vertex);

      *vertex = ' ';
      vertex += 1;
    }

  *vertex = '\0';

  return vertices;
}

/* Создаёт объект HyScanPlannerZone из считанных параметров. */
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

/* Создаёт объект HyScanPlannerTrack из считанных параметров. */
static HyScanObject *
hyscan_object_data_planner_get_track (HyScanObjectData *mdata,
                                      HyScanParamList  *plist)
{
  HyScanPlannerTrack *track;
  const gchar *records;

  track = hyscan_planner_track_new ();
  track->zone_id = hyscan_param_list_dup_string (plist, "/zone-id");
  track->number = hyscan_param_list_get_integer (plist, "/number");
  track->plan.speed = hyscan_param_list_get_double (plist, "/speed");
  track->name = hyscan_param_list_dup_string (plist, "/name");
  records = hyscan_param_list_get_string (plist, "/records");
  if (records != NULL && records[0] != '\0')
    track->records = g_strsplit (records, ",", -1);
  track->plan.start.lat = hyscan_param_list_get_double (plist, "/start/lat");
  track->plan.start.lon = hyscan_param_list_get_double (plist, "/start/lon");
  track->plan.end.lat = hyscan_param_list_get_double (plist, "/end/lat");
  track->plan.end.lon = hyscan_param_list_get_double (plist, "/end/lon");

  return (HyScanObject *) track;
}

/* Создаёт объект HyScanPlannerOrigin из считанных параметров. */
static HyScanObject *
hyscan_object_data_planner_get_origin (HyScanObjectData *mdata,
                                       HyScanParamList  *plist)
{
  HyScanPlannerOrigin *origin;

  origin = hyscan_planner_origin_new ();
  origin->origin.lat = hyscan_param_list_get_double (plist, "/lat");
  origin->origin.lon = hyscan_param_list_get_double (plist, "/lon");
  origin->ox = hyscan_param_list_get_double (plist, "/azimuth");

  return (HyScanObject *) origin;
}

/* HyScanObjectDataClass.get_full.
 * Создаёт объект планировщика из считанных параметров. */
static HyScanObject *
hyscan_object_data_planner_get_full (HyScanObjectData *mdata,
                                     HyScanParamList  *read_plist)
{
  gint64 sid, sver;

  sid = hyscan_param_list_get_integer (read_plist, "/schema/id");
  sver = hyscan_param_list_get_integer (read_plist, "/schema/version");

  if (sid == PLANNER_ZONE_SCHEMA_ID && sver == PLANNER_ZONE_SCHEMA_VERSION)
    return hyscan_object_data_planner_get_zone (mdata, read_plist);

  else if (sid == PLANNER_TRACK_SCHEMA_ID && sver == PLANNER_TRACK_SCHEMA_VERSION)
    return hyscan_object_data_planner_get_track (mdata, read_plist);

  else if (sid == PLANNER_ORIGIN_SCHEMA_ID && sver == PLANNER_ORIGIN_SCHEMA_VERSION)
    return hyscan_object_data_planner_get_origin (mdata, read_plist);

  return NULL;
}

/* Устанавливает параметры объекта HyScanPlannerTrack для записи в БД. */
static gboolean
hyscan_object_data_planner_set_track (HyScanObjectData         *data,
                                      HyScanParamList          *write_plist,
                                      const HyScanPlannerTrack *track)
{
  gchar *records;

  records = track->records != NULL ? g_strjoinv (",", track->records) : NULL;
  hyscan_param_list_set_string (write_plist, "/zone-id", track->zone_id);
  hyscan_param_list_set_string (write_plist, "/name", track->name);
  hyscan_param_list_set_string (write_plist, "/records", records);
  hyscan_param_list_set_integer (write_plist, "/number", track->number);
  hyscan_param_list_set_double (write_plist, "/speed", track->plan.speed);
  hyscan_param_list_set_double (write_plist, "/start/lat", track->plan.start.lat);
  hyscan_param_list_set_double (write_plist, "/start/lon", track->plan.start.lon);
  hyscan_param_list_set_double (write_plist, "/end/lat", track->plan.end.lat);
  hyscan_param_list_set_double (write_plist, "/end/lon", track->plan.end.lon);
  g_free (records);

  return TRUE;
}

/* Устанавливает параметры объекта HyScanPlannerOrigin для записи в БД. */
static gboolean
hyscan_object_data_planner_set_origin (HyScanObjectData          *data,
                                       HyScanParamList           *write_plist,
                                       const HyScanPlannerOrigin *origin)
{
  hyscan_param_list_set_double (write_plist, "/lat", origin->origin.lat);
  hyscan_param_list_set_double (write_plist, "/lon", origin->origin.lon);
  hyscan_param_list_set_double (write_plist, "/azimuth", origin->ox);

  return TRUE;
}

/* Устанавливает параметры объекта HyScanPlannerZone для записи в БД. */
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

/* HyScanObjectDataClass.set_full.
 * Устанавливает параметры объекта для записи в БД. */
static gboolean
hyscan_object_data_planner_set_full (HyScanObjectData   *mdata,
                                     HyScanParamList    *write_plist,
                                     const HyScanObject *object)
{
  if (HYSCAN_IS_PLANNER_ZONE (object))
    return hyscan_object_data_planner_set_zone (mdata, write_plist, (const HyScanPlannerZone *) object);
  else if (HYSCAN_IS_PLANNER_TRACK (object))
    return hyscan_object_data_planner_set_track (mdata, write_plist, (const HyScanPlannerTrack *) object);
  else if (HYSCAN_IS_PLANNER_ORIGIN (object))
    return hyscan_object_data_planner_set_origin (mdata, write_plist, (const HyScanPlannerOrigin *) object);

  return FALSE;
}

/**
 * hyscan_object_data_planner_new:
 *
 * Returns: указатель на новый объект #HyScanObjectDataPlanner. Для удаления g_object_unref().
 */
HyScanObjectData *
hyscan_object_data_planner_new (void)
{
  HyScanObjectData * data;

  data = g_object_new (HYSCAN_TYPE_OBJECT_DATA_PLANNER,
                       NULL);

  return data;
}
