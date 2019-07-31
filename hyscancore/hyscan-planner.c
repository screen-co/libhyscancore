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

static void         hyscan_planner_set_property             (GObject               *object,
                                                             guint                  prop_id,
                                                             const GValue          *value,
                                                             GParamSpec            *pspec);
static void         hyscan_planner_object_constructed       (GObject               *object);
static void         hyscan_planner_object_finalize          (GObject               *object);

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
  guint i;
  gchar *id;
  gsize id_size, prefix_len;

  prefix_len = strlen (prefix);
  id_size = prefix_len + TRACK_ID_LEN + 1;
  id = g_new (gchar, id_size);

  g_strlcpy (id, prefix, id_size);
  for (i = prefix_len; i < TRACK_ID_LEN; i++)
    {
      gint rnd;

      rnd = g_random_int_range (0, sizeof (dict) - 1);
      id[i] = dict[rnd];
    }
  id[TRACK_ID_LEN] = '\0';

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

static gboolean
hyscan_planner_zone_set_internal (HyScanPlanner           *planner,
                                  const gchar             *zone_id,
                                  const HyScanPlannerZone *zone)
{
  HyScanPlannerPrivate *priv = planner->priv;
  HyScanParamList *param_list;
  gchar *vertices;

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
  HyScanPlannerPrivate *priv;
  gchar **objects;
  gchar **buffer;
  gchar **zones;
  gsize i, n_zones;

  g_return_val_if_fail (HYSCAN_IS_PLANNER (planner), NULL);
  priv = planner->priv;

  objects = hyscan_db_param_object_list (priv->db, priv->param_id);
  if (objects == NULL)
    return NULL;

  /* Складываем в буфер объекты с нужным префиксом. */
  buffer = g_new (gchar *, g_strv_length (objects) + 1);
  n_zones = 0;
  for (i = 0; objects[i] != NULL; ++i)
    {
      if (g_str_has_prefix (objects[i], PREFIX_ZONE))
        buffer[n_zones++] = objects[i];
    }
  buffer[n_zones] = NULL;

  zones = g_strdupv (buffer);

  g_free (buffer);
  g_strfreev (objects);

  return zones;
}

/**
 *
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
    {
      g_free (zone_id);
      g_message ("HyScanPlanner: failed to create zone: %s", zone_id);

      return NULL;
    }

  if (hyscan_planner_zone_set_internal (planner, zone_id, zone))
    return zone_id;

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

  if (schema_id != PLANNER_SCHEMA_ID || schema_version != PLANNER_SCHEMA_VERSION)
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

  g_return_val_if_fail (HYSCAN_IS_PLANNER (planner), FALSE);
  priv = planner->priv;

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
