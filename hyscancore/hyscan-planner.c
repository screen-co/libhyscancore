/* hyscan-planner.c
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
 * SECTION: hyscan-planner
 * @Short_description: модель данных списку запланированных галсов
 * @Title: HyScanPlanner
 *
 * Планировщик галсов позволяет загружать и сохранять схему плановых галсов проекта.
 *
 */

#include "hyscan-planner.h"
#include "hyscan-core-schemas.h"
#include <string.h>

#define TRACK_ID_LEN  33
#define PREFIX_ZONE   "zone-"
#define PREFIX_TRACK  "track-"

enum
{
  PROP_O,
  PROP_DB,
  PROP_PROJECT,
};

struct _HyScanPlannerPrivate
{
  HyScanDB    *db;                  /* База данных. */
  gchar       *project_name;        /* Название проекта. */
  gint32       param_id;            /* Идентификатор группы параметров плановых галсов. */
};

static void                hyscan_planner_set_property             (GObject                  *object,
                                                                    guint                     prop_id,
                                                                    const GValue             *value,
                                                                    GParamSpec               *pspec);
static void                hyscan_planner_object_constructed       (GObject                  *object);
static void                hyscan_planner_object_finalize          (GObject                  *object);
static gchar *             hyscan_planner_points_to_string         (HyScanGeoGeodetic        *points,
                                                                    gsize                     points_len);
static gchar **            hyscan_planner_object_list              (HyScanPlanner            *planner,
                                                                    const gchar              *prefix);
static HyScanGeoGeodetic * hyscan_planner_string_to_points         (const gchar              *string,
                                                                    gsize                    *points_len);
static gboolean            hyscan_planner_zone_set_internal        (HyScanPlanner            *planner,
                                                                    const gchar              *zone_id,
                                                                    const HyScanPlannerZone  *zone);
static gboolean            hyscan_planner_track_set_internal       (HyScanPlanner            *planner,
                                                                    const gchar              *track_id,
                                                                    const HyScanPlannerTrack *track);
static inline gboolean     hyscan_planner_zone_validate_id         (const gchar              *zone_id);
static inline gboolean     hyscan_planner_track_validate_id        (const gchar              *track_id);
static inline gchar *      hyscan_planner_track_prefix_create      (const gchar              *zone_id);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanPlanner, hyscan_planner, G_TYPE_OBJECT)

static void
hyscan_planner_class_init (HyScanPlannerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_planner_set_property;
  object_class->constructed = hyscan_planner_object_constructed;
  object_class->finalize = hyscan_planner_object_finalize;

  g_object_class_install_property (object_class, PROP_DB,
    g_param_spec_object ("db", "DB", "HyScanDB interface", HYSCAN_TYPE_DB,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_PROJECT,
    g_param_spec_string ("project", "Project", "Project name", NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_planner_init (HyScanPlanner *planner)
{
  planner->priv = hyscan_planner_get_instance_private (planner);
}

static void
hyscan_planner_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  HyScanPlanner *planner = HYSCAN_PLANNER (object);
  HyScanPlannerPrivate *priv = planner->priv;

  switch (prop_id)
    {
    case PROP_DB:
      priv->db = g_value_dup_object (value);
      break;

    case PROP_PROJECT:
      priv->project_name = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_planner_object_constructed (GObject *object)
{
  HyScanPlanner *planner = HYSCAN_PLANNER (object);
  HyScanPlannerPrivate *priv = planner->priv;

  G_OBJECT_CLASS (hyscan_planner_parent_class)->constructed (object);

  if (priv->db != NULL)
    {
      gint32 project_id;

      project_id = hyscan_db_project_open (priv->db, priv->project_name);
      priv->param_id = hyscan_db_project_param_open (priv->db, project_id, PLANNER_OBJECT);
      hyscan_db_close (priv->db, project_id);
    }
}

static void
hyscan_planner_object_finalize (GObject *object)
{
  HyScanPlanner *planner = HYSCAN_PLANNER (object);
  HyScanPlannerPrivate *priv = planner->priv;

  g_clear_object (&priv->db);
  g_free (priv->project_name);

  G_OBJECT_CLASS (hyscan_planner_parent_class)->finalize (object);
}

/* Функция генерирует идентификатор. */
static gchar *
hyscan_planner_create_id (const gchar *prefix)
{
  static gchar dict[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
  gchar *id;
  gsize i;
  gsize id_size, prefix_len;

  prefix_len = strlen (prefix);
  id_size = prefix_len + TRACK_ID_LEN + 1;
  
  id = g_new (gchar, id_size);
  g_strlcpy (id, prefix, id_size);
  
  for (i = prefix_len; i < id_size; i++)
    {
      gint rnd;

      rnd = g_random_int_range (0, sizeof (dict) - 1);
      id[i] = dict[rnd];
    }
  id[i] = '\0';

  return id;
}

static gchar *
hyscan_planner_points_to_string (HyScanGeoGeodetic *points,
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

/* Возвращает список объектов с указанным префиксом. */
static gchar **
hyscan_planner_object_list (HyScanPlanner *planner,
                            const gchar   *prefix)
{
  HyScanPlannerPrivate *priv = planner->priv;
  gchar **buffer;
  gint i;
  gint n_tracks;
  gchar **objects, **prefixed_objects;
  
  objects = hyscan_db_param_object_list (priv->db, priv->param_id);
  if (objects == NULL)
    return NULL;

  /* Складываем в буфер объекты с нужным префиксом. */
  buffer = g_new (gchar *, g_strv_length (objects) + 1);
  n_tracks = 0;
  for (i = 0; objects[i] != NULL; ++i)
    {
      if (g_str_has_prefix (objects[i], prefix))
        buffer[n_tracks++] = objects[i];
    }
  buffer[n_tracks] = NULL;

  /* Если искомые объекты найдены, то копируем их. Иначе просто NULL. */
  if (n_tracks != 0)
    prefixed_objects = g_strdupv (buffer);
  else
    prefixed_objects = NULL;

  g_free (buffer);
  g_strfreev (objects);

  return prefixed_objects;
}

static HyScanGeoGeodetic *
hyscan_planner_string_to_points (const gchar *string,
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

static inline gboolean
hyscan_planner_zone_validate_id (const gchar *zone_id)
{
  return zone_id != NULL && g_str_has_prefix (zone_id, PREFIX_ZONE);
}

static inline gboolean
hyscan_planner_track_validate_id (const gchar *track_id)
{
  return track_id != NULL && g_str_has_prefix (track_id, PREFIX_TRACK);
}

static inline gchar *
hyscan_planner_track_prefix_create (const gchar *zone_id)
{
  return g_strconcat (PREFIX_TRACK, zone_id + strlen (PREFIX_ZONE), "/", NULL);
}

static gboolean
hyscan_planner_zone_set_internal (HyScanPlanner           *planner,
                                  const gchar             *zone_id,
                                  const HyScanPlannerZone *zone)
{
  HyScanPlannerPrivate *priv = planner->priv;
  HyScanParamList *param_list;
  gchar *vertices;

  g_return_val_if_fail (hyscan_planner_zone_validate_id (zone_id), FALSE);

  param_list = hyscan_param_list_new ();
  hyscan_param_list_set_string (param_list, "/name", zone->name);

  vertices = hyscan_planner_points_to_string (zone->points, zone->points_len);
  hyscan_param_list_set_string (param_list, "/vertices", vertices);
  g_free (vertices);

  hyscan_param_list_set_integer (param_list, "/mtime", zone->mtime);
  hyscan_param_list_set_integer (param_list, "/ctime", zone->ctime);

  if (!hyscan_db_param_set (priv->db, priv->param_id, zone_id, param_list))
    {
      g_message ("HyScanPlanner: failed to set zone: %s", zone_id);

      return FALSE;
    }

  return TRUE;
}

static gboolean
hyscan_planner_track_set_internal (HyScanPlanner            *planner,
                                   const gchar              *track_id,
                                   const HyScanPlannerTrack *track)
{
  HyScanPlannerPrivate *priv = planner->priv;
  HyScanParamList *param_list;

  g_return_val_if_fail (hyscan_planner_track_validate_id (track_id), FALSE);

  param_list = hyscan_param_list_new ();
  hyscan_param_list_set_string (param_list, "/zone-id", track->zone_id);
  hyscan_param_list_set_integer (param_list, "/number", track->number);
  hyscan_param_list_set_double (param_list, "/speed", track->speed);
  hyscan_param_list_set_string (param_list, "/name", track->name);
  hyscan_param_list_set_double (param_list, "/start-lat", track->start.lat);
  hyscan_param_list_set_double (param_list, "/start-lon", track->start.lon);
  hyscan_param_list_set_double (param_list, "/end-lat", track->end.lat);
  hyscan_param_list_set_double (param_list, "/end-lon", track->end.lon);

  if (!hyscan_db_param_set (priv->db, priv->param_id, track_id, param_list))
    {
      g_message ("HyScanPlanner: failed to set track: %s", track_id);

      return FALSE;
    }

  return TRUE;
}

/**
 * hyscan_planner_new:
 * @db: указатель на базу данных #HyScanDB
 * @project_name: имя проекта
 *
 * Создаёт планировщик галсов
 *
 * Returns: указатель на #HyScanPlanner, для удаления g_object_unref().
 */
HyScanPlanner *
hyscan_planner_new (HyScanDB    *db,
                    const gchar *project_name)
{
  return g_object_new (HYSCAN_TYPE_PLANNER,
                       "db", db,
                       "project", project_name,
                       NULL);
}

/**
 * hyscan_planner_zone_list:
 * @planner: указатель на #HyScanPlanner
 *
 * Возращает список зон полигона.
 *
 * Returns: нуль-терминированный список идентфикаторов зон или %NULL, если нет
 *   ни одной зоны.
 */
gchar **
hyscan_planner_zone_list (HyScanPlanner *planner)
{
  g_return_val_if_fail (HYSCAN_IS_PLANNER (planner), NULL);

  return hyscan_planner_object_list (planner, PREFIX_ZONE);
}

/**
 * hyscan_planner_zone_create:
 * @planner: указатель на #HyScanPlanner
 * @zone: зона полигона
 *
 * Создаёт зону полигона и устанавливает её параметры, указанные в структуре @zone.
 * В случае успеха функция возвращает идентификатор созданной зоны; иначе - %NULL.
 *
 * Returns: идентификатор созданной зоны
 */
gchar *
hyscan_planner_zone_create (HyScanPlanner           *planner,
                            const HyScanPlannerZone *zone)
{
  HyScanPlannerPrivate *priv;
  gchar *zone_id;

  g_return_val_if_fail (HYSCAN_IS_PLANNER (planner), NULL);
  priv = planner->priv;

  zone_id = hyscan_planner_create_id (PREFIX_ZONE);
  if (!hyscan_db_param_object_create (priv->db, priv->param_id, zone_id, PLANNER_ZONE_SCHEMA))
    goto error;

  if (!hyscan_planner_zone_set_internal (planner, zone_id, zone))
    goto error;

  return zone_id;

error:
  g_free (zone_id);
  g_message ("HyScanPlanner: failed to create zone: %s", zone_id);

  return NULL;
}

/**
 * hyscan_planner_zone_set:
 * @planner: указатель на #HyScanPlanner
 * @zone: структура #HyScanPlannerZone с параметрами зоны
 *
 * Сохраняет параметры зоны в параметрах проекта, считывая их из структуры @zone.
 *
 * Returns: %TRUE, если параметры были установлены
 */
gboolean
hyscan_planner_zone_set (HyScanPlanner           *planner,
                         const HyScanPlannerZone *zone)
{
  g_return_val_if_fail (HYSCAN_IS_PLANNER (planner), FALSE);
  g_return_val_if_fail (zone->id != NULL, FALSE);

  return hyscan_planner_zone_set_internal (planner, zone->id, zone);
}

/**
 * hyscan_planner_zone_get:
 * @planner: указатель на #HyScanPlanner
 * @zone_id: идентификатор зоны полигона
 *
 * Загружает из параметров проекта зону полигона с идентификатором @zone_id.
 *
 * Returns: указатель на структуру #HyScanPlannerZone, для удаления hyscan_planner_zone_free().
 */
HyScanPlannerZone *
hyscan_planner_zone_get (HyScanPlanner *planner,
                         const gchar   *zone_id)
{
  HyScanPlannerPrivate *priv;
  HyScanPlannerZone *zone;
  HyScanParamList *param_list;

  const gchar *vertices;
  gint64 schema_id, schema_version;
  
  g_return_val_if_fail (HYSCAN_IS_PLANNER (planner), NULL);
  g_return_val_if_fail (hyscan_planner_zone_validate_id (zone_id), NULL);
  priv = planner->priv;

  param_list = hyscan_param_list_new ();
  hyscan_param_list_add (param_list, "/schema/id");
  hyscan_param_list_add (param_list, "/schema/version");
  hyscan_param_list_add (param_list, "/name");
  hyscan_param_list_add (param_list, "/vertices");
  hyscan_param_list_add (param_list, "/ctime");
  hyscan_param_list_add (param_list, "/mtime");

  if (!hyscan_db_param_get (priv->db, priv->param_id, zone_id, param_list))
    return NULL;

  schema_id = hyscan_param_list_get_integer (param_list, "/schema/id");
  schema_version = hyscan_param_list_get_integer (param_list, "/schema/version");

  if (schema_id != PLANNER_ZONE_SCHEMA_ID || schema_version != PLANNER_ZONE_SCHEMA_VERSION)
    return FALSE;

  zone = g_slice_new (HyScanPlannerZone);
  zone->id = g_strdup (zone_id);
  zone->name = hyscan_param_list_dup_string (param_list, "/name");
  zone->mtime = hyscan_param_list_get_integer (param_list, "/mtime");
  zone->ctime = hyscan_param_list_get_integer (param_list, "/ctime");

  vertices = hyscan_param_list_get_string (param_list, "/vertices");
  zone->points = hyscan_planner_string_to_points (vertices, &zone->points_len);

  g_object_unref (param_list);

  return zone;
}

/**
 * hyscan_planner_zone_remove:
 * @planner: указатель на #HyScanPlanner
 * @zone_id: идентификатор зоны
 *
 * Удаляет из проекта зону полигона с идентификатором @zone_id.
 *
 * Returns: %TRUE, если зона была удалена успешно
 */
gboolean
hyscan_planner_zone_remove (HyScanPlanner *planner,
                            const gchar   *zone_id)
{
  HyScanPlannerPrivate *priv;
  gchar **tracks;
  gint i;

  g_return_val_if_fail (HYSCAN_IS_PLANNER (planner), FALSE);
  g_return_val_if_fail (hyscan_planner_zone_validate_id (zone_id), FALSE);
  priv = planner->priv;

  /* Удаляем все галсы в этой зоне. */
  tracks = hyscan_planner_track_list (planner, zone_id);
  if (tracks != NULL)
    {
      for (i = 0; tracks[i] != NULL; ++i)
        hyscan_planner_track_remove (planner, tracks[i]);
    }

  /* Удаляем саму зону. */
  if (!hyscan_db_param_object_remove (priv->db, priv->param_id, zone_id))
    return FALSE;

  return TRUE;
}

/**
 * hyscan_planner_zone_free:
 * @zone: указатель на структуру #HyScanPlannerZone
 *
 * Освобождает память, занятую структурой #HyScanPlannerZone
 */
void
hyscan_planner_zone_free (HyScanPlannerZone *zone)
{
  g_free (zone->points);
  g_free (zone->name);
  g_free (zone->id);
  g_slice_free (HyScanPlannerZone, zone);
}

/**
 * hyscan_planner_track_list:
 * @planner: указатель на #HyScanPlanner
 * @zone_id: идентификатор зоны
 *
 * Возращает схему галсов в зоне с идентфикатором @zone_id.
 *
 * Returns: нуль-терминированный список идентфикаторов галсов или %NULL, если нет
 *   ни одного галса.
 */
gchar **
hyscan_planner_track_list (HyScanPlanner *planner,
                           const gchar   *zone_id)
{
  gchar **tracks;
  gchar *prefix;

  g_return_val_if_fail (HYSCAN_IS_PLANNER (planner), NULL);
  g_return_val_if_fail (hyscan_planner_zone_validate_id (zone_id), NULL);

  prefix = hyscan_planner_track_prefix_create (zone_id);
  tracks = hyscan_planner_object_list (planner, prefix);
  g_free (prefix);

  return tracks;
}

/**
 * hyscan_planner_track_create:
 * @planner: указатель на #HyScanPlanner
 * @track: параметры планового галса
 *
 * Создаёт плановый галс и устанавливает его параметры, указанные в структуре @track.
 * В случае успеха функция возвращает идентификатор созданного галса; иначе - %NULL.
 *
 * Returns: идентификатор созданной зоны
 */
gchar *
hyscan_planner_track_create (HyScanPlanner           *planner,
                             const HyScanPlannerTrack *track)
{
  HyScanPlannerPrivate *priv;
  gchar *track_id;
  gchar *prefix;

  g_return_val_if_fail (HYSCAN_IS_PLANNER (planner), NULL);
  g_return_val_if_fail (hyscan_planner_zone_validate_id (track->zone_id), NULL);

  priv = planner->priv;

  prefix = hyscan_planner_track_prefix_create (track->zone_id);
  track_id = hyscan_planner_create_id (prefix);
  g_free (prefix);

  if (!hyscan_db_param_object_create (priv->db, priv->param_id, track_id, PLANNER_TRACK_SCHEMA))
    goto error;

  if (!hyscan_planner_track_set_internal (planner, track_id, track))
    goto error;

  return track_id;

error:
  g_message ("HyScanPlanner: failed to create track: %s", track_id);
  g_free (track_id);

  return NULL;
}

/**
 * hyscan_planner_track_set:
 * @planner: указатель на #HyScanPlanner
 * @track: структура #HyScanPlannerTrack с параметрами зоны
 *
 * Сохраняет параметры зоны в параметрах проекта, считывая их из структуры @track.
 *
 * Returns: %TRUE, если параметры были установлены
 */
gboolean
hyscan_planner_track_set (HyScanPlanner            *planner,
                          const HyScanPlannerTrack *track)
{
  g_return_val_if_fail (HYSCAN_IS_PLANNER (planner), FALSE);
  g_return_val_if_fail (track->id != NULL, FALSE);

  return hyscan_planner_track_set_internal (planner, track->id, track);
}

/**
 * hyscan_planner_track_get:
 * @planner: указатель на #HyScanPlanner
 * @track_id: идентификатор планового галса
 *
 * Загружает из параметров проекта плановый галс с идентификатором @track_id.
 *
 * Returns: указатель на структуру #HyScanPlannerTrack, для удаления hyscan_planner_track_free().
 */
HyScanPlannerTrack *
hyscan_planner_track_get (HyScanPlanner *planner,
                          const gchar   *track_id)
{
  HyScanPlannerPrivate *priv;
  HyScanPlannerTrack *track = NULL;
  HyScanParamList *param_list;

  gint64 schema_id, schema_version;
  
  g_return_val_if_fail (HYSCAN_IS_PLANNER (planner), NULL);
  priv = planner->priv;

  param_list = hyscan_param_list_new ();
  hyscan_param_list_add (param_list, "/schema/id");
  hyscan_param_list_add (param_list, "/schema/version");
  hyscan_param_list_add (param_list, "/zone-id");
  hyscan_param_list_add (param_list, "/number");
  hyscan_param_list_add (param_list, "/speed");
  hyscan_param_list_add (param_list, "/name");
  hyscan_param_list_add (param_list, "/start-lat");
  hyscan_param_list_add (param_list, "/start-lon");
  hyscan_param_list_add (param_list, "/end-lat");
  hyscan_param_list_add (param_list, "/end-lon");

  if (!hyscan_db_param_get (priv->db, priv->param_id, track_id, param_list))
    goto exit;

  schema_id = hyscan_param_list_get_integer (param_list, "/schema/id");
  schema_version = hyscan_param_list_get_integer (param_list, "/schema/version");

  if (schema_id != PLANNER_TRACK_SCHEMA_ID || schema_version != PLANNER_TRACK_SCHEMA_VERSION)
    goto exit;

  track = g_slice_new (HyScanPlannerTrack);
  track->id = g_strdup (track_id);
  track->zone_id = hyscan_param_list_dup_string (param_list, "/zone-id");
  track->number = hyscan_param_list_get_integer (param_list, "/number");
  track->speed = hyscan_param_list_get_double (param_list, "/speed");
  track->name = hyscan_param_list_dup_string (param_list, "/name");
  track->start.lat = hyscan_param_list_get_double (param_list, "/start-lat");
  track->start.lon = hyscan_param_list_get_double (param_list, "/start-lon");
  track->end.lat = hyscan_param_list_get_double (param_list, "/end-lat");
  track->end.lon = hyscan_param_list_get_double (param_list, "/end-lon");

exit:
  g_object_unref (param_list);

  return track;
}

/**
 * hyscan_planner_track_remove:
 * @planner: указатель на #HyScanPlanner
 * @track_id: идентификатор галса
 *
 * Удаляет из проекта плановый галс с идентификатором @track_id.
 *
 * Returns: %TRUE, если галс был удалён успешно
 */
gboolean
hyscan_planner_track_remove (HyScanPlanner *planner,
                             const gchar   *track_id)
{
  HyScanPlannerPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_PLANNER (planner), FALSE);
  g_return_val_if_fail (hyscan_planner_track_validate_id (track_id), FALSE);
  priv = planner->priv;

  if (!hyscan_db_param_object_remove (priv->db, priv->param_id, track_id))
    return FALSE;

  return TRUE;
}

/**
 * hyscan_planner_track_free:
 * @track: указатель на структуру #HyScanPlannerTrack
 *
 * Освобождает память, занятую структурой #HyScanPlannerTrack
 */
void
hyscan_planner_track_free (HyScanPlannerTrack *track)
{
  g_free (track->id);
  g_free (track->zone_id);
  g_free (track->name);
  g_slice_free (HyScanPlannerTrack, track);
}
