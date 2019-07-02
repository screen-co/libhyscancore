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
 * Планировщик галсов хранит список запланированных галсов в виде структур #HyScanPlannerTrack,
 * и позволяет создавать, модифицировать и удалять запланированные галсы.
 *
 * При любом изменении в списке галсов класс рассылает сигнал #HyScanNmeaFileDevice::changed
 *
 */

#include "hyscan-planner.h"

#define TRACK_ID_LEN  33

enum
{
  SIGNAL_CHANGED,
  SIGNAL_LAST,
};

struct _HyScanPlannerPrivate
{
  GMutex      lock;
  GHashTable *tracks;
};

static void         hyscan_planner_object_constructed       (GObject               *object);
static void         hyscan_planner_object_finalize          (GObject               *object);
static GHashTable * hyscan_planner_table_new                (void);

static guint        hyscan_planner_signals[SIGNAL_LAST] = {0};

G_DEFINE_TYPE_WITH_PRIVATE (HyScanPlanner, hyscan_planner, G_TYPE_OBJECT)

static void
hyscan_planner_class_init (HyScanPlannerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = hyscan_planner_object_constructed;
  object_class->finalize = hyscan_planner_object_finalize;

  hyscan_planner_signals[SIGNAL_CHANGED] =
    g_signal_new ("changed", HYSCAN_TYPE_PLANNER,
                  G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

static void
hyscan_planner_init (HyScanPlanner *planner)
{
  planner->priv = hyscan_planner_get_instance_private (planner);
}

static void
hyscan_planner_object_constructed (GObject *object)
{
  HyScanPlanner *planner = HYSCAN_PLANNER (object);
  HyScanPlannerPrivate *priv = planner->priv;

  G_OBJECT_CLASS (hyscan_planner_parent_class)->constructed (object);

  g_mutex_init (&priv->lock);
  priv->tracks = hyscan_planner_table_new ();
}

static void
hyscan_planner_object_finalize (GObject *object)
{
  HyScanPlanner *planner = HYSCAN_PLANNER (object);
  HyScanPlannerPrivate *priv = planner->priv;

  g_mutex_clear (&priv->lock);
  g_hash_table_unref (priv->tracks);

  G_OBJECT_CLASS (hyscan_planner_parent_class)->finalize (object);
}

static GHashTable *
hyscan_planner_table_new (void)
{
  return g_hash_table_new_full (g_str_hash, g_str_equal, NULL, (GDestroyNotify) hyscan_planner_track_free);
}

/* Функция генерирует идентификатор. */
static gchar *
hyscan_planner_create_id (void)
{
  static gchar dict[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
  guint i;
  gchar *id;

  id = g_new (gchar, TRACK_ID_LEN + 1);
  id[TRACK_ID_LEN] = '\0';

  for (i = 0; i < TRACK_ID_LEN; i++)
    {
      gint rnd;

      rnd = g_random_int_range (0, sizeof (dict) - 1);
      id[i] = dict[rnd];
    }

  return id;
}

/**
 * hyscan_planner_new:
 *
 * Создаёт планировщик галсов
 *
 * Returns: указатель на #HyScanPlanner, для удаления g_object_unref().
 */
HyScanPlanner *
hyscan_planner_new (void)
{
  return g_object_new (HYSCAN_TYPE_PLANNER, NULL);
}

/**
 * hyscan_planner_save_ini:
 * @planner: указатель на #HyScanPlanner
 * @file_name: путь к файлу
 *
 * Сохраняет запланированные галсы в файл @file_name
 */
void
hyscan_planner_save_ini (HyScanPlanner *planner,
                         const gchar   *file_name)
{
  HyScanPlannerPrivate *priv;
  GKeyFile *key_file;
  GHashTableIter iter;
  HyScanPlannerTrack *track;
  gchar *id;

  g_return_if_fail (HYSCAN_IS_PLANNER (planner));
  priv = planner->priv;

  key_file = g_key_file_new ();

  g_mutex_lock (&priv->lock);

  g_hash_table_iter_init (&iter, priv->tracks);
  while (g_hash_table_iter_next (&iter, (gpointer *) &id, (gpointer *) &track))
    {
      if (track->name != NULL)
        g_key_file_set_string (key_file, id, "name", track->name);

      g_key_file_set_double (key_file, track->id, "start_lat", track->start.lat);
      g_key_file_set_double (key_file, track->id, "start_lon", track->start.lon);
      g_key_file_set_double (key_file, track->id, "end_lat", track->end.lat);
      g_key_file_set_double (key_file, track->id, "end_lon", track->end.lon);
    }

  g_mutex_unlock (&priv->lock);

  g_key_file_save_to_file (key_file, file_name, NULL);
  g_key_file_unref (key_file);
}

/**
 * hyscan_planner_load_ini:
 * @planner: указатель на #HyScanPlanner
 * @file_name: путь к файлу
 *
 * Загрузка списка запланированных галсов из ini-файла @file_name
 */
void
hyscan_planner_load_ini (HyScanPlanner *planner,
                         const gchar   *file_name)
{
  HyScanPlannerPrivate *priv;
  GKeyFile *key_file;
  gchar **groups;
  gint i;
  
  g_return_if_fail (HYSCAN_IS_PLANNER (planner));
  priv = planner->priv;
  
  key_file = g_key_file_new ();
  g_key_file_load_from_file (key_file, file_name, G_KEY_FILE_NONE, NULL);
  
  g_mutex_lock (&priv->lock);
  g_hash_table_remove_all (priv->tracks);
  groups = g_key_file_get_groups (key_file, NULL);
  for (i = 0; groups[i] != NULL; ++i)
    {
      HyScanPlannerTrack track;
      HyScanPlannerTrack *track_copy;
      gchar *name;

      name = g_key_file_get_string (key_file, groups[i], "name", NULL);

      track.id = groups[i];
      track.name = name;
      track.start.lat = g_key_file_get_double (key_file, groups[i], "start_lat", NULL);
      track.start.lon = g_key_file_get_double (key_file, groups[i], "start_lon", NULL);
      track.end.lat = g_key_file_get_double (key_file, groups[i], "end_lat", NULL);
      track.end.lon = g_key_file_get_double (key_file, groups[i], "end_lon", NULL);

      track_copy = hyscan_planner_track_copy (&track);
      g_hash_table_insert (priv->tracks, track_copy->id, track_copy);
      g_free (name);
    }
  g_mutex_unlock (&priv->lock);

  g_strfreev (groups);
  g_key_file_free (key_file);
  
  g_signal_emit (planner, hyscan_planner_signals[SIGNAL_CHANGED], 0);
}

/**
 * hyscan_planner_get:
 * @planner: указатель на HyScanPlanner:
 *
 * Возвращает таблицу запланированных галсов.
 *
 * Returns: (element-type HyScanPlannerTrack): таблица запланированных галсов.
 *   Для удаления g_hash_table_unref().
 */
GHashTable *
hyscan_planner_get (HyScanPlanner *planner)
{
  HyScanPlannerPrivate *priv;
  GHashTable *tracks;
  GHashTableIter iter;
  HyScanPlannerTrack *track;

  g_return_val_if_fail (HYSCAN_IS_PLANNER (planner), NULL);
  priv = planner->priv;

  tracks = hyscan_planner_table_new ();

  g_mutex_lock (&priv->lock);

  g_hash_table_iter_init (&iter, priv->tracks);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &track))
    {
      HyScanPlannerTrack *track_copy;

      track_copy = hyscan_planner_track_copy (track);
      g_hash_table_insert (tracks, track_copy->id, track_copy);
    }

  g_mutex_unlock (&priv->lock);

  return tracks;
}

/**
 * hyscan_planner_track_copy:
 * @track: указатель на #HyScanPlannerTrack
 *
 * Создаёт копию структуры #HyScanPlannerTrack
 *
 * Returns: указатель на #HyScanPlannerTrack. Для удаления hyscan_planner_track_free().
 */
HyScanPlannerTrack *
hyscan_planner_track_copy (const HyScanPlannerTrack *track)
{
  HyScanPlannerTrack *copy;

  copy = g_slice_new (HyScanPlannerTrack);
  copy->start = track->start;
  copy->end = track->end;
  copy->name = g_strdup (track->name);
  copy->id = g_strdup (track->id);

  return copy;
}

/**
 * hyscan_planner_track_free:
 * @track: указатель на #HyScanPlannerTrack
 *
 * Освобождает память, занятую структурой #HyScanPlannerTrack
 */
void
hyscan_planner_track_free (HyScanPlannerTrack *track)
{
  g_free (track->id);
  g_free (track->name);
  g_slice_free (HyScanPlannerTrack, track);
}

/**
 * hyscan_planner_delete:
 * @planner: указатель на #HyScanPlannerTrack
 * @id: идентификатор галса
 *
 * Удаляет галс из списка запланированных по его идентификатору @id.
 */
void
hyscan_planner_delete (HyScanPlanner *planner,
                       const gchar   *id)
{
  HyScanPlannerPrivate *priv = planner->priv;

  g_return_if_fail (HYSCAN_IS_PLANNER (planner));

  g_mutex_lock (&priv->lock);
  g_hash_table_remove (priv->tracks, id);
  g_mutex_unlock (&priv->lock);

  g_signal_emit (planner, hyscan_planner_signals[SIGNAL_CHANGED], 0);
}

/**
 * hyscan_planner_update:
 * @planner: указатель на #HyScanPlanner
 * @track: указатель на #HyScanPlannerTrack
 *
 * Обновляет параметры запланированного галса @track. Если идентификатор галса
 * равен %NULL или его нет в таблице галсов, то будет создан новый галс.
 */
void
hyscan_planner_update (HyScanPlanner            *planner,
                       const HyScanPlannerTrack *track)
{
  HyScanPlannerPrivate *priv = planner->priv;
  HyScanPlannerTrack *track_copy;

  g_return_if_fail (HYSCAN_IS_PLANNER (planner));

  track_copy = hyscan_planner_track_copy (track);
  g_mutex_lock (&priv->lock);

  if (track_copy->id == NULL)
    track_copy->id = hyscan_planner_create_id ();

  g_hash_table_replace (priv->tracks, track_copy->id, track_copy);
  g_mutex_unlock (&priv->lock);

  g_signal_emit (planner, hyscan_planner_signals[SIGNAL_CHANGED], 0);
}