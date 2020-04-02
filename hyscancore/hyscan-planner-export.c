/* hyscan-planner-export.c
 *
 * Copyright 2020 Screen LLC, Alexey Sakhnov <alexsakhnov@gmail.com>
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
 * SECTION: hyscan-planner-export
 * @Title: Экспорт и импорт данных планировщика
 * @Short_description: различные функции, связанные с экспортом и импортом данных планировщика
 *
 * Функции для экспорта и импорта объектов планировщика. Поддерживается экспорт в формат KML,
 * а также экспорт и импорт во внутреннем формате XML:
 * - hyscan_planner_import_xml_from_file() - импорт из XML
 * - hyscan_planner_export_xml_to_file() - экспорт в XML
 * - hyscan_planner_export_xml_to_str() - экспорт в XML
 * - hyscan_planner_export_kml_to_file() - экспорт в KML
 * - hyscan_planner_export_kml_to_str() - экспорт в KML
 *
 * Хэш таблицу объектов можно записать в базу данных при помощи функции
 * hyscan_planner_import_to_db().
 *
 */

#include "hyscan-planner-export.h"
#include "hyscan-object-data-planner.h"
#include <libxml/xmlwriter.h>
#include <glib/gi18n-lib.h>

#define MY_ENCODING "UTF-8"
#define VERSION "20190101"

/* Размер буфера для строки с n координатами в формате KML: "<lat>,<lon>\n". */
#define PREALLOC_SIZE(n) (n * (2 * G_ASCII_DTOSTR_BUF_SIZE + 2) + 1)

static void               hyscan_planner_export_write_point        (xmlTextWriterPtr     writer,
                                                                    HyScanGeoPoint      *point,
                                                                    const gchar         *lat_field,
                                                                    const gchar         *lon_field);
static gboolean           hyscan_planner_export_read_point         (xmlNodePtr           node,
                                                                    HyScanGeoPoint      *point,
                                                                    const gchar         *lat_field,
                                                                    const gchar         *lon_field);
static gboolean           hyscan_planner_export_read_ll            (xmlNodePtr           node,
                                                                    const gchar         *name,
                                                                    gint64              *value);
static gboolean           hyscan_planner_export_read_ull           (xmlNodePtr           node,
                                                                    const gchar         *name,
                                                                    guint64              *value);
static gboolean           hyscan_planner_export_read_d             (xmlNodePtr           node,
                                                                    const gchar         *name,
                                                                    gdouble             *value);
static void               hyscan_planner_export_read_tracks        (xmlNodePtr           tracks,
                                                                    GHashTable          *objects);
static void               hyscan_planner_export_read_zones         (xmlNodePtr           zones,
                                                                    GHashTable          *objects);
static void               hyscan_planner_export_read_origin        (xmlNodePtr           node,
                                                                    GHashTable          *objects);
static gboolean           hyscan_planner_export_xml_inner          (xmlTextWriterPtr     writer,
                                                                    GHashTable          *objects);
static void               hyscan_planner_export_kml_coord          (GString             *string,
                                                                    HyScanGeoGeodetic   *coord);
static void               hyscan_planner_export_kml_zone           (xmlTextWriterPtr     writer,
                                                                    HyScanPlannerZone   *zone);
static void               hyscan_planner_export_kml_track          (xmlTextWriterPtr     writer,
                                                                    HyScanPlannerTrack  *track);
static gboolean           hyscan_planner_export_kml_inner          (xmlTextWriterPtr     writer,
                                                                    GHashTable          *objects);
static void               hyscan_planner_export_kml_origin         (xmlTextWriterPtr     writer,
                                                                    HyScanPlannerOrigin *origin);
static void               hyscan_planner_export_write_double       (xmlTextWriterPtr     writer,
                                                                    const gchar         *name,
                                                                    gdouble              value);

static void
hyscan_planner_export_write_double (xmlTextWriterPtr  writer,
                                    const gchar      *name,
                                    gdouble          value)
{
  gchar str[G_ASCII_DTOSTR_BUF_SIZE];

  g_ascii_dtostr (str, sizeof (str), value);
  xmlTextWriterWriteAttribute (writer, BAD_CAST name, BAD_CAST str);
}

static void
hyscan_planner_export_write_point (xmlTextWriterPtr writer,
                                   HyScanGeoPoint  *point,
                                   const gchar     *lat_field,
                                   const gchar     *lon_field)
{
  hyscan_planner_export_write_double (writer, lat_field, point->lat);
  hyscan_planner_export_write_double (writer, lon_field, point->lon);
}

static gboolean
hyscan_planner_export_read_point (xmlNodePtr      node,
                                  HyScanGeoPoint *point,
                                  const gchar    *lat_field,
                                  const gchar    *lon_field)
{
  xmlChar *lat, *lon;

  lat = xmlGetProp (node, BAD_CAST lat_field);
  if (lat == NULL)
    return FALSE;

  point->lat = g_ascii_strtod ((gchar *) lat, NULL);
  xmlFree (lat);

  lon = xmlGetProp (node, BAD_CAST lon_field);
  if (lon == NULL)
    return FALSE;

  point->lon = g_ascii_strtod ((gchar *) lon, NULL);
  xmlFree (lon);

  return TRUE;
}

static gboolean
hyscan_planner_export_read_ll (xmlNodePtr   node,
                               const gchar *name,
                               gint64      *value)
{
  xmlChar *prop;

  prop = xmlGetProp (node, BAD_CAST name);
  if (value == NULL)
    return FALSE;

  *value = g_ascii_strtoll ((const gchar *) prop, NULL, 10);
  xmlFree (prop);

  return TRUE;
}

static gboolean
hyscan_planner_export_read_ull (xmlNodePtr   node,
                                const gchar *name,
                                guint64      *value)
{
  xmlChar *prop;

  prop = xmlGetProp (node, BAD_CAST name);
  if (value == NULL)
    return FALSE;

  *value = g_ascii_strtoull ((const gchar *) prop, NULL, 10);
  xmlFree (prop);

  return TRUE;
}

static gboolean
hyscan_planner_export_read_d (xmlNodePtr   node,
                              const gchar *name,
                              gdouble     *value)
{
  xmlChar *prop;

  prop = xmlGetProp (node, BAD_CAST name);
  if (value == NULL)
    return FALSE;

  *value = g_ascii_strtod ((const gchar *) prop, NULL);
  xmlFree (prop);

  return TRUE;
}

static void
hyscan_planner_export_read_zones (xmlNodePtr  zones,
                                  GHashTable *objects)
{
  xmlNodePtr node, vertices, vertex;

  for (node = zones->children; node != NULL; node = node->next)
    {
      HyScanPlannerZone zone = { .type = HYSCAN_PLANNER_ZONE };
      HyScanPlannerZone *zone_copy;
      gchar *id;
      xmlChar *prop;

      if (node->type != XML_ELEMENT_NODE)
        continue;

      if (g_ascii_strcasecmp ((gchar*) node->name, "zone") != 0)
        continue;

      prop = xmlGetProp (node, BAD_CAST "id");
      if (prop == NULL)
        continue;

      id = g_strdup ((gchar *) prop);
      xmlFree (prop);

      zone_copy = hyscan_planner_zone_copy (&zone);

      prop = xmlGetProp (node, BAD_CAST "name");
      zone_copy->name = g_strdup ((gchar *) prop);
      xmlFree (prop);

      hyscan_planner_export_read_ll (node, "ctime", &zone_copy->ctime);
      hyscan_planner_export_read_ll (node, "mtime", &zone_copy->mtime);

      /* Считываем вершины. */
      for (vertices = node->children; vertices != NULL; vertices = vertices->next)
        {
          if (vertices->type != XML_ELEMENT_NODE)
            continue;

          if (g_ascii_strcasecmp ((const gchar *) vertices->name, "vertices") != 0)
            continue;

          for (vertex = vertices->children; vertex != NULL; vertex = vertex->next)
            {
              HyScanGeoPoint point;
              if (hyscan_planner_export_read_point (vertex, &point, "lat", "lon"))
                hyscan_planner_zone_vertex_append (zone_copy, point);
            }
        }

      g_hash_table_insert (objects, id, zone_copy);
    }
}

static void
hyscan_planner_export_read_tracks (xmlNodePtr  tracks,
                                   GHashTable *objects)
{
  xmlNodePtr node;

  for (node = tracks->children; node != NULL; node = node->next)
    {
      HyScanPlannerTrack track = { .type = HYSCAN_PLANNER_TRACK };
      HyScanPlannerTrack *track_copy;
      gchar *id;
      xmlChar *prop;
      guint64 number;
      gboolean result = TRUE;

      if (node->type != XML_ELEMENT_NODE)
        continue;

      if (g_ascii_strcasecmp ((gchar*) node->name, "track") != 0)
        continue;

      prop = xmlGetProp (node, BAD_CAST "id");
      if (prop == NULL)
        continue;

      id = g_strdup ((gchar *) prop);
      xmlFree (prop);

      track_copy = hyscan_planner_track_copy (&track);

      prop = xmlGetProp (node, BAD_CAST "name");
      track_copy->name = g_strdup ((gchar *) prop);
      xmlFree (prop);

      prop = xmlGetProp (node, BAD_CAST "zone-id");
      track_copy->zone_id = g_strdup ((gchar *) prop);
      xmlFree (prop);

      if (hyscan_planner_export_read_ull (node, "number", &number))
        track_copy->number = number;

      result &= hyscan_planner_export_read_d (node, "velocity", &track_copy->plan.velocity);
      result &= hyscan_planner_export_read_point (node, &track_copy->plan.start, "start-lat", "start-lon");
      result &= hyscan_planner_export_read_point (node, &track_copy->plan.end, "end-lat", "end-lon");

      if (result)
        {
          g_hash_table_insert (objects, id, track_copy);
        }
      else
        {
          g_free (id);
          hyscan_planner_track_free (track_copy);
        }
    }
}

static void
hyscan_planner_export_read_origin (xmlNodePtr  node,
                                   GHashTable *objects)
{
  HyScanPlannerOrigin origin = { .type = HYSCAN_PLANNER_ORIGIN };
  HyScanPlannerOrigin *origin_copy;
  gboolean result = TRUE;

  origin_copy = hyscan_planner_origin_copy (&origin);

  result &= hyscan_planner_export_read_point (node, &origin_copy->origin, "lat", "lon");
  result &= hyscan_planner_export_read_d (node, "ox", &origin_copy->origin.h);

  if (result)
    g_hash_table_insert (objects, g_strdup (HYSCAN_PLANNER_ORIGIN_ID), origin_copy);
  else
    hyscan_planner_origin_free (origin_copy);
}

static gboolean
hyscan_planner_export_xml_inner (xmlTextWriterPtr  writer,
                                 GHashTable       *objects)
{
  GHashTableIter iter;
  gchar *key;
  HyScanPlannerZone *zone;
  HyScanPlannerTrack *track;
  HyScanPlannerOrigin *origin;

  xmlTextWriterStartDocument (writer, NULL, MY_ENCODING, NULL);

  xmlTextWriterStartElement (writer, BAD_CAST "plan");
  xmlTextWriterWriteAttribute (writer, BAD_CAST "version", BAD_CAST VERSION);

  origin = g_hash_table_lookup (objects, HYSCAN_PLANNER_ORIGIN_ID);
  if (HYSCAN_IS_PLANNER_ORIGIN (origin))
    {
      xmlTextWriterStartElement (writer, BAD_CAST "origin");
      hyscan_planner_export_write_point (writer, &origin->origin, "lat", "lon");
      hyscan_planner_export_write_double (writer, "ox", origin->origin.h);
      xmlTextWriterEndElement (writer);
    }

  xmlTextWriterStartElement (writer, BAD_CAST "zones");
  g_hash_table_iter_init (&iter, objects);
  while (g_hash_table_iter_next (&iter, (gpointer *) &key, (gpointer *) &zone))
    {
      if (!HYSCAN_IS_PLANNER_ZONE (zone))
        continue;

      xmlTextWriterStartElement (writer, BAD_CAST "zone");
      xmlTextWriterWriteAttribute (writer, BAD_CAST "id", BAD_CAST key);
      xmlTextWriterWriteAttribute (writer, BAD_CAST "name", BAD_CAST zone->name);
      xmlTextWriterWriteFormatAttribute (writer, BAD_CAST "ctime", "%ld", zone->ctime);
      xmlTextWriterWriteFormatAttribute (writer, BAD_CAST "mtime", "%ld", zone->mtime);

      xmlTextWriterStartElement (writer, BAD_CAST "vertices");
      {
        guint i;

        for (i = 0; i < zone->points_len; i++)
          {
            xmlTextWriterStartElement (writer, BAD_CAST "vertex");
            hyscan_planner_export_write_point (writer, &zone->points[i], "lat", "lon");
            xmlTextWriterEndElement (writer);
          }
      }
      xmlTextWriterEndElement (writer);

      xmlTextWriterEndElement (writer);
    }
  xmlTextWriterEndElement (writer);

  xmlTextWriterStartElement (writer, BAD_CAST "tracks");
  g_hash_table_iter_init (&iter, objects);
  while (g_hash_table_iter_next (&iter, (gpointer *) &key, (gpointer *) &track))
    {
      if (!HYSCAN_IS_PLANNER_TRACK (track))
        continue;

      xmlTextWriterStartElement (writer, BAD_CAST "track");
      xmlTextWriterWriteAttribute (writer, BAD_CAST "id", BAD_CAST key);
      xmlTextWriterWriteFormatAttribute (writer, BAD_CAST "number", "%d", track->number);
      if (track->zone_id != NULL)
        xmlTextWriterWriteAttribute (writer, BAD_CAST "zone-id", BAD_CAST track->zone_id);
      if (track->name != NULL)
        xmlTextWriterWriteAttribute (writer, BAD_CAST "name", BAD_CAST track->name);
      hyscan_planner_export_write_point (writer, &track->plan.start, "start-lat", "start-lon");
      hyscan_planner_export_write_point (writer, &track->plan.end, "end-lat", "end-lon");
      hyscan_planner_export_write_double (writer, "velocity", track->plan.velocity);
      xmlTextWriterEndElement (writer);
    }

  xmlTextWriterEndDocument (writer);

  return TRUE;
}

static void
hyscan_planner_export_kml_coord (GString           *string,
                                 HyScanGeoGeodetic *coord)
{
  gchar buffer[G_ASCII_DTOSTR_BUF_SIZE];

  g_ascii_dtostr (buffer, sizeof (buffer), coord->lon);
  g_string_append (string, buffer);
  g_string_append (string, ",");
  g_ascii_dtostr (buffer, sizeof (buffer), coord->lat);
  g_string_append (string, buffer);
  g_string_append (string, "\n");
}

static void
hyscan_planner_export_kml_zone (xmlTextWriterPtr   writer,
                                HyScanPlannerZone *zone)
{
  guint i;
  GString *string;

  if (zone->points_len == 0)
    return;

  xmlTextWriterStartElement (writer, BAD_CAST "Placemark");
  xmlTextWriterWriteElement (writer, BAD_CAST "name", BAD_CAST zone->name);

  xmlTextWriterStartElement (writer, BAD_CAST "Polygon");
  xmlTextWriterStartElement (writer, BAD_CAST "outerBoundaryIs");
  xmlTextWriterStartElement (writer, BAD_CAST "LinearRing");

  /* Каждая координата представляется строкой "<double_number>,<double_number>\n",
   * можем сразу выделить достаточно памяти. */
  string = g_string_sized_new (PREALLOC_SIZE (zone->points_len));
  for (i = 0; i < zone->points_len; i++)
    hyscan_planner_export_kml_coord (string, &zone->points[i]);
  hyscan_planner_export_kml_coord (string, &zone->points[0]);
  xmlTextWriterWriteElement (writer, BAD_CAST "coordinates", BAD_CAST string->str);
  g_string_free (string, TRUE);

  xmlTextWriterEndElement (writer);
  xmlTextWriterEndElement (writer);
  xmlTextWriterEndElement (writer);
  xmlTextWriterEndElement (writer);
}

static void
hyscan_planner_export_kml_track (xmlTextWriterPtr   writer,
                                 HyScanPlannerTrack *track)
{
  GString *string;

  xmlTextWriterStartElement (writer, BAD_CAST "Placemark");
  xmlTextWriterWriteFormatElement (writer, BAD_CAST "name", "%d", track->number);

  xmlTextWriterStartElement (writer, BAD_CAST "LineString");

  string = g_string_sized_new (PREALLOC_SIZE (2));
  hyscan_planner_export_kml_coord (string, &track->plan.start);
  hyscan_planner_export_kml_coord (string, &track->plan.end);
  xmlTextWriterWriteElement (writer, BAD_CAST "coordinates", BAD_CAST string->str);
  g_string_free (string, TRUE);

  xmlTextWriterEndElement (writer);
  xmlTextWriterEndElement (writer);
}

static void
hyscan_planner_export_kml_origin (xmlTextWriterPtr   writer,
                                  HyScanPlannerOrigin *origin)
{
  GString *string;

  xmlTextWriterStartElement (writer, BAD_CAST "Folder");
  xmlTextWriterWriteElement (writer, BAD_CAST "name", BAD_CAST _("Origin"));

  xmlTextWriterStartElement (writer, BAD_CAST "Placemark");
  xmlTextWriterWriteElement (writer, BAD_CAST "name", BAD_CAST _("Origin"));

  xmlTextWriterStartElement (writer, BAD_CAST "Point");

  string = g_string_sized_new (PREALLOC_SIZE (1));
  hyscan_planner_export_kml_coord (string, &origin->origin);
  xmlTextWriterWriteElement (writer, BAD_CAST "coordinates", BAD_CAST string->str);
  g_string_free (string, TRUE);

  xmlTextWriterEndElement (writer);
  xmlTextWriterEndElement (writer);
  xmlTextWriterEndElement (writer);
}

static gboolean
hyscan_planner_export_kml_inner (xmlTextWriterPtr  writer,
                                 GHashTable       *objects)
{
  GHashTableIter iter;
  gchar *key;
  HyScanPlannerZone *zone;
  HyScanPlannerTrack *track;
  HyScanPlannerOrigin *origin;
  GHashTable *exported_tracks;
  GHashTableIter track_iter;
  gchar *track_id;

  exported_tracks = g_hash_table_new (g_str_hash, g_str_equal);

  xmlTextWriterStartDocument (writer, NULL, MY_ENCODING, NULL);

  /* <kml xmlns="http://www.opengis.net/kml/2.2"><Document>... */
  xmlTextWriterStartElement (writer, BAD_CAST "kml");
  xmlTextWriterStartElement (writer, BAD_CAST "Document");
  xmlTextWriterWriteAttribute (writer, BAD_CAST "xmlns", BAD_CAST "http://www.opengis.net/kml/2.2");

  origin = g_hash_table_lookup (objects, HYSCAN_PLANNER_ORIGIN_ID);
  if (HYSCAN_IS_PLANNER_ORIGIN (origin))
    hyscan_planner_export_kml_origin (writer, origin);

  g_hash_table_iter_init (&iter, objects);
  while (g_hash_table_iter_next (&iter, (gpointer *) &key, (gpointer *) &zone))
    {
      if (!HYSCAN_IS_PLANNER_ZONE (zone))
        continue;

      /* Папку по названию зоны, помещаем туда границы зоны и галсы внутри неё. */
      xmlTextWriterStartElement (writer, BAD_CAST "Folder");
      xmlTextWriterWriteElement (writer, BAD_CAST "name", BAD_CAST zone->name);

      hyscan_planner_export_kml_zone (writer, zone);

      g_hash_table_iter_init (&track_iter, objects);
      while (g_hash_table_iter_next (&track_iter, (gpointer *) &track_id, (gpointer *) &track))
        {
          if (!HYSCAN_IS_PLANNER_TRACK (track) || g_strcmp0 (track->zone_id, key) != 0)
            continue;

          g_hash_table_add (exported_tracks, track_id);
          hyscan_planner_export_kml_track (writer, track);
        }

      xmlTextWriterEndElement (writer);
    }

  /* Папка для галсов, которые ещё не добавили в KML. */
  xmlTextWriterStartElement (writer, BAD_CAST "Folder");
  xmlTextWriterWriteElement (writer, BAD_CAST "name", BAD_CAST _("Other tracks"));

  g_hash_table_iter_init (&iter, objects);
  while (g_hash_table_iter_next (&iter, (gpointer *) &track_id, (gpointer *) &track))
    {
      if (!HYSCAN_IS_PLANNER_TRACK (track) || g_hash_table_contains (exported_tracks, track_id))
        continue;

      g_hash_table_add (exported_tracks, track_id);
      hyscan_planner_export_kml_track (writer, track);
    }
  xmlTextWriterEndElement (writer);

  /* </Document></kml> */
  xmlTextWriterEndElement (writer);
  xmlTextWriterEndElement (writer);

  xmlTextWriterEndDocument (writer);

  g_hash_table_destroy (exported_tracks);

  return TRUE;
}

/**
 * hyscan_planner_import_to_db:
 * @db: указатель на базу данных #HyScanDB
 * @project_name: имя проекта
 * @objects: хэш-таблица объектов планировщика
 * @replace: надо ли удалить из БД существующие объекты
 *
 * Функция записывает данных из хэш-таблицы в базу данных. Связи между объектами,
 * определённые их идентификаторами будут сохранены, однако сами идентификаторы
 * при записи в БД не сохраняются.
 *
 * Returns: %TRUE, если импорт завершён успешно
 */
gboolean
hyscan_planner_import_to_db (HyScanDB    *db,
                             const gchar *project_name,
                             GHashTable  *objects,
                             gboolean     replace)
{
  HyScanObjectData *data;
  GHashTable *id_map = NULL;
  GHashTableIter iter;
  HyScanPlannerZone *zone;
  HyScanPlannerTrack *track;
  HyScanPlannerOrigin *origin;
  gchar *key;
  gboolean result = FALSE;

  g_return_val_if_fail (HYSCAN_IS_DB (db), FALSE);

  data = hyscan_object_data_planner_new (db, project_name);
  if (data == NULL)
    return FALSE;

  /* Удаляем объекты из БД. */
  if (replace)
    {
      gint i;
      gchar **ids;

      ids = hyscan_object_data_get_ids (data, NULL);
      if (ids != NULL)
        {
          for (i = 0; ids[i] != NULL; i++)
            hyscan_object_data_remove (data, ids[i]);
          g_strfreev (ids);
        }
    }

  /* Устанавливаем точку отсчёта, если такая есть. */
  origin = g_hash_table_lookup (objects, HYSCAN_PLANNER_ORIGIN_ID);
  if (HYSCAN_IS_PLANNER_ORIGIN (origin))
    {
      gchar **ids;

      ids = hyscan_object_data_get_ids (data, NULL);
      if (ids != NULL && g_strv_contains ((const gchar *const *) ids, HYSCAN_PLANNER_ORIGIN_ID))
        hyscan_object_data_remove (data, HYSCAN_PLANNER_ORIGIN_ID);

      if (!hyscan_object_data_add (data, (HyScanObject *) origin, NULL))
        goto exit;
    }

  /* Вставляем все зоны. */
  id_map = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);
  g_hash_table_iter_init (&iter, objects);
  while (g_hash_table_iter_next (&iter, (gpointer *) &key, (gpointer *) &zone))
    {
      gchar *new_id;

      if (!HYSCAN_IS_PLANNER_ZONE (zone))
        continue;

      if (!hyscan_object_data_add (data, (HyScanObject *) zone, &new_id))
        goto exit;

      g_hash_table_insert (id_map, key, new_id);
    }

  /* Теперь галсы, при этом меняем id на те, что записались в БД. */
  g_hash_table_iter_init (&iter, objects);
  while (g_hash_table_iter_next (&iter, (gpointer *) &key, (gpointer *) &track))
    {
      HyScanPlannerTrack *track_copy;
      gboolean track_added;

      if (!HYSCAN_IS_PLANNER_TRACK (track))
        continue;

      track_copy = hyscan_planner_track_copy (track);
      if (track_copy->zone_id != NULL)
        {
          const gchar *new_zone_id;

          new_zone_id = g_hash_table_lookup (id_map, track_copy->zone_id);
          g_free (track_copy->zone_id);
          track_copy->zone_id = g_strdup (new_zone_id);
        }

      track_added = hyscan_object_data_add (data, (HyScanObject *) track_copy, NULL);
      hyscan_planner_track_free (track_copy);

      if (!track_added)
        goto exit;
    }

  result = TRUE;

exit:
  g_clear_pointer (&id_map, g_hash_table_destroy);
  g_object_unref (data);

  return result;
}

/**
 * hyscan_planner_import_xml_from_file:
 * @filename: имя файла
 *
 * Функция загружает список объектов планировщика из xml-файла. Для создания xml-файла
 * используется функция hyscan_planner_export_xml_to_file().
 *
 * Returns: (transfer full): указатель на таблицк загруженных объектов, для удаления g_hash_table_unref().
 */
GHashTable *
hyscan_planner_import_xml_from_file (const gchar *filename)
{
  HyScanObjectDataPlannerClass *planner_data_class = NULL;
  HyScanObjectDataClass *data_class = NULL;
  GHashTable *objects = NULL;
  xmlNodePtr node;
  xmlDocPtr doc;
  xmlChar *version = NULL;

  doc = xmlReadFile (filename, NULL, 0);
  if (doc == NULL)
    {
      g_warning ("HyScanPlannerExport: failed to parse %s", filename);
      goto exit;
    }

  /* Поиск корневого узла plan. */
  for (node = xmlDocGetRootElement (doc); node != NULL; node = node->next)
    if (node->type == XML_ELEMENT_NODE)
      if (g_ascii_strcasecmp ((gchar *) node->name, "plan") == 0)
        break;


  if (node == NULL)
    goto exit;

  /* Проверяем версию файла */
  version = xmlGetProp (node, BAD_CAST "version");
  if (g_strcmp0 ((gchar *) version, VERSION) != 0)
    {
      g_warning ("HyScanPlannerExport: wrong version %s", version);
      goto exit;
    }

  planner_data_class = g_type_class_ref (HYSCAN_TYPE_OBJECT_DATA_PLANNER);
  data_class = HYSCAN_OBJECT_DATA_CLASS (planner_data_class);
  objects = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) data_class->object_destroy);

  for (node = node->children; node != NULL; node = node->next)
    {
      if (node->type != XML_ELEMENT_NODE)
        continue;

      if (g_ascii_strcasecmp ((gchar*) node->name, "zones") == 0)
        hyscan_planner_export_read_zones (node, objects);

      if (g_ascii_strcasecmp ((gchar*) node->name, "tracks") == 0)
        hyscan_planner_export_read_tracks (node, objects);

      if (g_ascii_strcasecmp ((gchar*) node->name, "origin") == 0)
        hyscan_planner_export_read_origin (node, objects);
    }

exit:
  g_clear_pointer (&version, xmlFree);
  xmlFreeDoc (doc);
  g_clear_pointer (&planner_data_class, g_type_class_unref);
  return objects;
}

/**
 * hyscan_planner_export_xml_to_file:
 * @filename: имя файла
 * @objects: хэш-таблица объектов
 *
 * Функция сохраняет объекты планировщика из таблицы @objects в указанный xml-файл.
 * Объекты могут быть структурами #HyScanPlannerOrigin, #HyScanPlannerTrack и
 * #HyScanPlannerZone.
 *
 * Returns: %TRUE, если экспорт прошёл успешно.
 */
gboolean
hyscan_planner_export_xml_to_file (const gchar *filename,
                                   GHashTable  *objects)
{
  xmlTextWriterPtr writer;
  gboolean result;

  writer = xmlNewTextWriterFilename (filename, 0);
  if (writer == NULL)
    {
      g_warning ("HyScanPlannerExport: error creating the xml file writer");
      return FALSE;
    }

  result = hyscan_planner_export_xml_inner (writer, objects);
  xmlFreeTextWriter (writer);

  return result;
}

/**
 * hyscan_planner_export_xml_to_str:
 * @objects: хэш-таблица объектов
 *
 * Функция сохраняет объекты планировщика из таблицы @objects в виде xml-строки.
 * Объекты могут быть структурами #HyScanPlannerOrigin, #HyScanPlannerTrack и
 * #HyScanPlannerZone.
 *
 * Returns: (transfer full): xml-строка, для удаления g_free().
 */
gchar *
hyscan_planner_export_xml_to_str (GHashTable *objects)
{
  xmlTextWriterPtr writer;
  xmlBufferPtr buffer;
  gchar *xml = NULL;

  buffer = xmlBufferCreate ();
  writer = xmlNewTextWriterMemory (buffer, 0);
  if (writer == NULL)
    {
      g_warning ("HyScanPlannerExport: error creating the xml writer");
      return NULL;
    }

  if (hyscan_planner_export_xml_inner (writer, objects))
    xml = g_strdup ((gchar *) buffer->content);

  xmlFreeTextWriter (writer);

  return xml;
}

/**
 * hyscan_planner_export_kml_to_file:
 * @filename: имя файла
 * @objects: хэш-таблица объектов
 *
 * Функция сохраняет объекты планировщика из таблицы @objects в формате KML.
 * Объекты могут быть структурами #HyScanPlannerOrigin, #HyScanPlannerTrack и
 * #HyScanPlannerZone.
 *
 * Returns: %TRUE, если экспорт прошёл успешно.
 */
gboolean
hyscan_planner_export_kml_to_file (const gchar *filename,
                                   GHashTable  *objects)
{
  xmlTextWriterPtr writer;
  gboolean result;

  writer = xmlNewTextWriterFilename (filename, 0);
  if (writer == NULL)
    {
      g_warning ("HyScanPlannerExport: error creating the xml file writer");
      return FALSE;
    }

  result = hyscan_planner_export_kml_inner (writer, objects);
  xmlFreeTextWriter (writer);

  return result;
}

/**
 * hyscan_planner_export_kml_to_str:
 * @objects: хэш-таблица объектов
 *
 * Функция сохраняет объекты планировщика из таблицы @objects в строки в формате KML.
 * Объекты могут быть структурами #HyScanPlannerOrigin, #HyScanPlannerTrack и
 * #HyScanPlannerZone.
 *
 * Returns: (transfer full): xml-строка, для удаления g_free().
 */
gchar *
hyscan_planner_export_kml_to_str (GHashTable *objects)
{
  xmlTextWriterPtr writer;
  xmlBufferPtr buffer;
  gchar *xml = NULL;

  buffer = xmlBufferCreate ();
  writer = xmlNewTextWriterMemory (buffer, 0);
  if (writer == NULL)
    {
      g_warning ("HyScanPlannerExport: error creating the xml writer");
      return NULL;
    }

  if (hyscan_planner_export_kml_inner (writer, objects))
    xml = g_strdup ((gchar *) buffer->content);

  xmlFreeTextWriter (writer);

  return xml;
}
