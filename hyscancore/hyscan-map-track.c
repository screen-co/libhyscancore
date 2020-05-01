/* hyscan-map-track.c
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
 * SECTION: hyscan-map-track
 * @Short_description: Проекция галса на карту
 * @Title: HyScanMapTrack
 *
 * HyScanMapTrack позволяет проецировать галс на карту в виде линии движения
 * судна и дальности обнаружения по каждому из бортов.
 *
 * Класс производит загрузку данных галса из БД по и расчёт координат точек на текущей картографической проекции
 * с учётом смещений используемых датчиков.
 *
 * Функции:
 * - hyscan_map_track_new() - создаёт новый объект;
 * - hyscan_map_track_set_projection() - устанавливает картографическую проекцию;
 * - hyscan_map_track_update() - проверяет и загружает новые данные по галсу;
 * - hyscan_map_track_get() - получает список точек галса;
 * - hyscan_map_track_view() - определяет границы галса.
 *
 */

#include "hyscan-map-track.h"
#include "hyscan-map-track-param.h"
#include <hyscan-acoustic-data.h>
#include <hyscan-cartesian.h>
#include <hyscan-depthometer.h>
#include <hyscan-nmea-parser.h>
#include <hyscan-projector.h>
#include <hyscan-nav-smooth.h>
#include <glib/gi18n-lib.h>
#include <math.h>

#define STRAIGHT_LINE_MAX_ANGLE   0.26             /* Максимальное изменение курса на прямолинейном участке, рад. */
#define STRAIGHT_LINE_MIN_DIST    30.0             /* Минимальная длина прямолинейного участка, метры. */
#define DEFAULT_HAPERTURE         0.15             /* Апертура антенны по умолчанию (если её нет в параметрах галса). */
#define SOUND_VELOCITY            1500.            /* Скорость звука. */
#define DEFAULT_CHANNEL_SS        1                /* Номер канала ГБО. */

enum
{
  PROP_O,
  PROP_DB,
  PROP_PROJECT,
  PROP_NAME,
  PROP_CACHE,
  PROP_PROJECTION,
};

enum
{
  SIGNAL_AREA_MOD,
  SIGNAL_LAST,
};

/* Каналы по типу данных. */
enum
{
  CHANNEL_NMEA_RMC,
  CHANNEL_NMEA_DPT,
  CHANNEL_PORT,
  CHANNEL_STARBOARD,
};

/* Параметры и данные одного борта ГБО. */
typedef struct
{
  HyScanAntennaOffset             offset;            /* Смещение антенны. */
  gboolean                        writeable;         /* Признак записи в канал. */
  guint32                         mod_count;         /* Mod-count амплитудных данных. */
  GList                          *points;            /* Точки галса HyScanMapTrackPoint. */

  HyScanMapTrackSource            source;            /* Источник. */
  HyScanAmplitude                *amplitude;         /* Амплитудные данные трека. */
  HyScanProjector                *projector;         /* Сопоставления индексов и отсчётов реальным координатам. */

  gdouble                         antenna_length;    /* Длина антенны. */
  gdouble                         beam_width;        /* Ширина луча, радианы. */
  gdouble                         near_field;        /* Граница ближней зоны. */
  HyScanTrackProjQuality         *quality;
} HyScanMapTrackSide;

/* Параметры и данные навигации. */
typedef struct
{
  gboolean                        opened;            /* Признак того, что актуальные обработчики открыты. */
  HyScanAntennaOffset             offset;            /* Смещение антенны. */
  gboolean                        writeable;         /* Признак записи в канал. */
  guint32                         mod_count;         /* Mod-count канала навигационных данных. */
  GList                          *points;            /* Точки галса HyScanMapTrackPoint. */

  HyScanNavData                  *lat_data;          /* Навигационные данные - широта. */
  HyScanNavData                  *lon_data;          /* Навигационные данные - долгота. */
  HyScanNavData                  *trk_data;          /* Навигационные данные - курс в градусах. */
  HyScanNavSmooth                *lat_smooth;        /* Интерполяция данных широты. */
  HyScanNavSmooth                *lon_smooth;        /* Интерполяция данных долготы. */
  HyScanNavSmooth                *trk_smooth;        /* Интерполяция данных курса. */
} HyScanMapTrackNav;

/* Параметры глубины. */
typedef struct
{
  guint                           channel;           /* Номер канала NMEA с DPT. */
  HyScanAntennaOffset             offset;            /* Смещение эхолота. */
  HyScanDepthometer              *meter;             /* Определение глубины. */
} HyScanGtkMapTrackDepth;

struct _HyScanMapTrackPrivate
{
  HyScanDB                       *db;                /* База данных. */
  HyScanCache                    *cache;             /* Кэш. */
  HyScanMapTrackParam            *param;             /* Параметры галса. */
  guint32                         param_mod_count;

  gchar                          *project;           /* Название проекта. */
  gchar                          *name;              /* Название галса. */

  GList                          *mod_list;          /* Список изменённых регионов. */

  gboolean                        opened;            /* Признак того, что каналы галса открыты. */
  gboolean                        loaded;            /* Признак того, что данные галса загружены. */

  HyScanGeoProjection            *projection;        /* Картографическая проекция. */

  HyScanMapTrackSide             port;               /* Данные левого борта. */
  HyScanMapTrackSide             starboard;          /* Данные правого борта. */
  HyScanMapTrackNav              nav;                /* Данные навигации. */
  HyScanGtkMapTrackDepth          depth;             /* Данные по глубине. */

  HyScanGeoCartesian2D            extent_from;       /* Минимальные координаты точек галса. */
  HyScanGeoCartesian2D            extent_to;         /* Максимальные координаты точек галса. */
  gboolean                        proj_changed;      /* Признак того, что проекция изменилась. */
  guint32                         loaded_mod_count;  /* Мод-каунт последних загруженных данных. */
};

static void     hyscan_map_track_set_property        (GObject                    *object,
                                                       guint                       prop_id,
                                                       const GValue              *value,
                                                       GParamSpec                *pspec);
static void     hyscan_map_track_object_constructed  (GObject                    *object);
static void     hyscan_map_track_object_finalize     (GObject                    *object);
static void     hyscan_gtk_map_track_side_clear       (HyScanMapTrackSide        *side);
static void     hyscan_map_track_load_side           (HyScanMapTrack             *track,
                                                      HyScanMapTrackSide         *side);
static void     hyscan_map_track_remove_expired      (GList                      *points,
                                                      guint32                     first_index,
                                                      guint32                     last_index);
static gboolean hyscan_map_track_load                (HyScanMapTrack             *track);
static void     hyscan_map_track_mod_free            (HyScanMapTrackMod          *mod);
static void     hyscan_map_track_param_update        (HyScanMapTrack             *track);
static void     hyscan_map_track_reset_extent        (HyScanMapTrackPrivate      *priv);
static void     hyscan_map_track_update_extent       (GList                      *points,
                                                       HyScanGeoCartesian2D      *from,
                                                       HyScanGeoCartesian2D      *to);
inline static void hyscan_map_track_extend           (HyScanMapTrackPoint        *point,
                                                       HyScanGeoCartesian2D      *from,
                                                       HyScanGeoCartesian2D      *to);
static void     hyscan_map_track_load_length_by_idx  (HyScanMapTrack             *track,
                                                       HyScanMapTrackSide        *side,
                                                       HyScanMapTrackPoint       *point);
static gboolean hyscan_map_track_is_straight         (GList                      *l_point);
static void     hyscan_map_track_cartesian           (HyScanMapTrack             *track,
                                                       GList                     *points);
static void     hyscan_map_track_move_point          (HyScanGeoCartesian2D       *point,
                                                       gdouble                    angle,
                                                       gdouble                    length,
                                                       HyScanGeoCartesian2D      *destination);
static void     hyscan_map_track_open                (HyScanMapTrack             *track);
static void     hyscan_map_track_open_depth          (HyScanMapTrackPrivate      *priv);
static void     hyscan_map_track_open_nav            (HyScanMapTrackPrivate      *priv);
static void     hyscan_map_track_open_side           (HyScanMapTrackPrivate      *priv,
                                                       HyScanMapTrackSide        *side,
                                                       HyScanSourceType           source);
static void     hyscan_map_track_load_nav            (HyScanMapTrack             *track);
static GList *  hyscan_map_track_add_modified        (GList                      *list,
                                                      HyScanGeoCartesian2D       *from,
                                                      HyScanGeoCartesian2D       *to);

static guint    hyscan_map_track_signals[SIGNAL_LAST] = {0};

G_DEFINE_TYPE_WITH_CODE (HyScanMapTrack, hyscan_map_track, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (HyScanMapTrack))

static void
hyscan_map_track_class_init (HyScanMapTrackClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_map_track_set_property;

  object_class->constructed = hyscan_map_track_object_constructed;
  object_class->finalize = hyscan_map_track_object_finalize;

  g_object_class_install_property (object_class, PROP_DB,
    g_param_spec_object ("db", "Database", "The HyScanDB for reading track data", HYSCAN_TYPE_DB,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_CACHE,
    g_param_spec_object ("cache", "Cache", "The HyScanCache for internal structures", HYSCAN_TYPE_CACHE,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_PROJECTION,
    g_param_spec_object ("projection", "Map projection", "The projection of track coordinates",
                         HYSCAN_TYPE_GEO_PROJECTION,
                         G_PARAM_WRITABLE));
  g_object_class_install_property (object_class, PROP_PROJECT,
    g_param_spec_string ("project", "Project name", "The project containing track", NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_NAME,
    g_param_spec_string ("name", "Track name", "The name of the track", NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  /**
   * HyScanMapTrack::area-mod:
   * @track: указатель на #HyScanMapTrack
   * @list: список отрезков, которые были добавлены
   *
   * Сигнал посылается при измененнии данных внутри некоторой области.
   */
  hyscan_map_track_signals[SIGNAL_AREA_MOD] =
      g_signal_new ("area-mod", HYSCAN_TYPE_MAP_TRACK, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                    g_cclosure_marshal_VOID__POINTER,
                    G_TYPE_NONE, 1, G_TYPE_POINTER);

}

static void
hyscan_map_track_init (HyScanMapTrack *gtk_map_track_item)
{
  gtk_map_track_item->priv = hyscan_map_track_get_instance_private (gtk_map_track_item);
}

static void
hyscan_map_track_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  HyScanMapTrack *track = HYSCAN_MAP_TRACK (object);
  HyScanMapTrackPrivate *priv = track->priv;

  switch (prop_id)
    {
    case PROP_DB:
      priv->db = g_value_dup_object (value);
      break;

    case PROP_CACHE:
      priv->cache = g_value_dup_object (value);
      break;

    case PROP_PROJECTION:
      hyscan_map_track_set_projection (track, g_value_get_object (value));
      break;

    case PROP_PROJECT:
      priv->project = g_value_dup_string (value);
      break;

    case PROP_NAME:
      priv->name = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_map_track_object_constructed (GObject *object)
{
  HyScanMapTrack *track = HYSCAN_MAP_TRACK (object);
  HyScanMapTrackPrivate *priv = track->priv;

  G_OBJECT_CLASS (hyscan_map_track_parent_class)->constructed (object);

  priv->starboard.source = HYSCAN_GTK_MAP_TRACK_DRAW_SOURCE_RIGHT;
  priv->port.source = HYSCAN_GTK_MAP_TRACK_DRAW_SOURCE_LEFT;
  priv->param = hyscan_map_track_param_new (NULL, priv->db, priv->project, priv->name);

  /* Область, в которой находится галс. */
  hyscan_map_track_reset_extent (priv);
}

static void
hyscan_map_track_object_finalize (GObject *object)
{
  HyScanMapTrack *gtk_map_track_item = HYSCAN_MAP_TRACK (object);
  HyScanMapTrackPrivate *priv = gtk_map_track_item->priv;

  g_clear_object (&priv->db);
  g_clear_object (&priv->cache);
  g_clear_object (&priv->projection);
  g_clear_object (&priv->param);

  g_free (priv->project);
  g_free (priv->name);

  hyscan_gtk_map_track_side_clear (&priv->port);
  hyscan_gtk_map_track_side_clear (&priv->starboard);

  g_clear_object (&priv->depth.meter);
  g_clear_object (&priv->nav.lat_data);
  g_clear_object (&priv->nav.lon_data);
  g_clear_object (&priv->nav.trk_data);
  g_clear_object (&priv->nav.lat_smooth);
  g_clear_object (&priv->nav.lon_smooth);
  g_clear_object (&priv->nav.trk_smooth);

  g_list_free_full (priv->nav.points, (GDestroyNotify) hyscan_map_track_point_free);
  g_list_free_full (priv->port.points, (GDestroyNotify) hyscan_map_track_point_free);
  g_list_free_full (priv->starboard.points, (GDestroyNotify) hyscan_map_track_point_free);

  G_OBJECT_CLASS (hyscan_map_track_parent_class)->finalize (object);
}

static void
hyscan_gtk_map_track_side_clear (HyScanMapTrackSide *side)
{
  g_clear_object (&side->quality);
  g_clear_object (&side->amplitude);
  g_clear_object (&side->projector);
}

/* Определяет отображаемую длину луча по борту side для индекса index. */
static void
hyscan_map_track_load_length_by_idx (HyScanMapTrack      *track,
                                      HyScanMapTrackSide  *side,
                                      HyScanMapTrackPoint *point)
{
  HyScanMapTrackPrivate *priv = track->priv;
  HyScanAntennaOffset *amp_offset = &side->offset;
  gint64 time;
  guint32 n_points;
  gdouble depth;
  gdouble length;

  hyscan_amplitude_get_size_time (side->amplitude, point->index, &n_points, &time);

  /* Определяем глубину под собой. */
  if (priv->depth.meter != NULL)
    {
      HyScanAntennaOffset *depth_offset = &priv->depth.offset;

      depth = hyscan_depthometer_get (priv->depth.meter, NULL, time);
      if (depth < 0)
        depth = 0;

      depth += depth_offset->vertical;
    }
  else
    {
      depth = 0;
    }
  depth -= amp_offset->vertical;

  /* Проекция дальности. */
  if (hyscan_projector_count_to_coord (side->projector, n_points, &length, depth))
    point->b_length_m = length;
  else
    point->b_length_m = 0;

  /* Проекция ближней зоны. */
  point->nr_length_m = side->near_field > depth ? sqrt (side->near_field * side->near_field - depth * depth) : 0.0;
  point->nr_length_m = MIN (point->nr_length_m, point->b_length_m);
}

/* Определяет, находится ли точка на прямолинейном отрезке. */
static gboolean
hyscan_map_track_is_straight (GList *l_point)
{
  HyScanMapTrackPoint *point = l_point->data;
  gdouble prev_dist, next_dist;
  gdouble min_distance;
  gboolean is_straight = TRUE;
  GList *list;

  min_distance = STRAIGHT_LINE_MIN_DIST / point->scale;

  /* Смотрим предыдущие строки. */
  list = l_point->prev;
  prev_dist = 0.0;
  while (prev_dist < min_distance && list != NULL)
    {
      HyScanMapTrackPoint *point_prev = list->data;

      if (ABS (point->b_angle - point_prev->b_angle) > STRAIGHT_LINE_MAX_ANGLE)
        {
          is_straight = FALSE;
          break;
        }

      prev_dist = ABS (point->dist_along - point_prev->dist_along);
      list = list->prev;
    }

  /* Смотрим следующие строки. */
  list = l_point->next;
  next_dist = 0.0;
  while (next_dist < min_distance && list != NULL)
    {
      HyScanMapTrackPoint *point_next = list->data;

      if (ABS (point->b_angle - point_next->b_angle) > STRAIGHT_LINE_MAX_ANGLE)
        {
          is_straight = FALSE;
          break;
        }

      next_dist = ABS (point->dist_along - point_next->dist_along);
      list = list->next;
    }

  return is_straight || (next_dist + prev_dist >= min_distance);
}

/* Находит координаты точки destination, находящейся от точки point
 * на расстоянии length по направлению angle. */
static void
hyscan_map_track_move_point (HyScanGeoCartesian2D *point,
                              gdouble               angle,
                              gdouble               length,
                              HyScanGeoCartesian2D *destination)
{
  destination->x = point->x + length * cos (angle);
  destination->y = point->y + length * sin (angle);
}

/* Вычисляет координаты галса в СК картографической проекции. */
static void
hyscan_map_track_cartesian (HyScanMapTrack *track,
                             GList           *points)
{
  HyScanMapTrackPrivate *priv = track->priv;
  GList *point_l;

  /* Вычисляем положение приёмника GPS в картографической проекции. */
  for (point_l = points; point_l != NULL; point_l = point_l->next)
    {
      HyScanMapTrackPoint *point = point_l->data;
      hyscan_geo_projection_geo_to_value (priv->projection, point->geo, &point->ship_c2d);
    }

  for (point_l = points; point_l != NULL; point_l = point_l->next)
    {
      HyScanMapTrackPoint *point, *neighbour_point;
      HyScanMapTrackSide *side;
      GList *neighbour;
      gdouble hdg, hdg_sin, hdg_cos;
      HyScanAntennaOffset *amp_offset, *nav_offset;

      point = point_l->data;
      point->scale = hyscan_geo_projection_get_scale (priv->projection, point->geo);

      /* Делаем поправки на смещение приёмника GPS: 1-2. */
      nav_offset = &priv->nav.offset;

      /* 1. Поправка курса. */
      hdg = point->course / 180.0 * G_PI - nav_offset->yaw;
      hdg_sin = sin (hdg);
      hdg_cos = cos (hdg);

      /* 2. Поправка смещений x, y. */
      point->ship_c2d.x -= nav_offset->forward / point->scale * hdg_sin;
      point->ship_c2d.y -= nav_offset->forward / point->scale * hdg_cos;
      point->ship_c2d.x -= nav_offset->starboard / point->scale * hdg_cos;
      point->ship_c2d.y -= -nav_offset->starboard / point->scale * hdg_sin;

      /* Определяем положения антенн левого и правого бортов ГБО. */
      if (point->source == HYSCAN_GTK_MAP_TRACK_DRAW_SOURCE_LEFT)
        amp_offset = &priv->port.offset;
      else if (point->source == HYSCAN_GTK_MAP_TRACK_DRAW_SOURCE_RIGHT)
        amp_offset = &priv->starboard.offset;
      else
        amp_offset = NULL;

      if (amp_offset != NULL)
        {
          point->b_angle = hdg - amp_offset->yaw;
          point->start_c2d.x = point->ship_c2d.x +
                                 (amp_offset->forward * hdg_sin + amp_offset->starboard * hdg_cos) / point->scale;
          point->start_c2d.y = point->ship_c2d.y +
                                 (amp_offset->forward * hdg_cos - amp_offset->starboard * hdg_sin) / point->scale;
        }

      /* Определяем расстояние от начала галса на основе соседней точки. */
      neighbour = point_l->prev;
      neighbour_point = neighbour != NULL ? neighbour->data : NULL;
      if (neighbour_point != NULL)
        {
          point->dist_along = neighbour_point->dist_along +
                              hyscan_cartesian_distance (&point->ship_c2d, &neighbour_point->ship_c2d);
        }
      else
        {
          point->dist_along = 0;
        }

      /* Правый и левый борт. */
      if (point->source == HYSCAN_GTK_MAP_TRACK_DRAW_SOURCE_LEFT)
        side = &priv->port;
      else if (point->source == HYSCAN_GTK_MAP_TRACK_DRAW_SOURCE_RIGHT)
        side = &priv->starboard;
      else
        side = NULL;

      if (side != NULL)
        {
          gdouble beam_width = side->beam_width;
          gdouble near_field;
          gdouble angle;

          /* Направление луча в СК картографической проекции. */
          if (side->source == HYSCAN_GTK_MAP_TRACK_DRAW_SOURCE_RIGHT)
            angle = -point->b_angle;
          else
            angle = G_PI - point->b_angle;

          /* Переводим из метров в масштаб проекции. */
          point->b_dist = point->b_length_m / point->scale;
          near_field = side->near_field / point->scale;
          point->aperture = side->antenna_length / point->scale;

          /* Урезаем ближнюю зону до длины луча при необходимости. */
          near_field = MIN (near_field, point->b_dist);

          /* Находим координаты точек диаграммы направленности. */
          hyscan_map_track_move_point (&point->start_c2d, angle, point->b_dist, &point->fr_c2d);
          hyscan_map_track_move_point (&point->start_c2d, angle, near_field, &point->nr_c2d);
          hyscan_map_track_move_point (&point->start_c2d, angle + beam_width / 2, point->b_dist, &point->fr1_c2d);
          hyscan_map_track_move_point (&point->start_c2d, angle - beam_width / 2, point->b_dist, &point->fr2_c2d);
        }
    }

    /* todo: надо ли это, если есть качество? */
    for (point_l = points; point_l != NULL; point_l = point_l->next)
      {
        HyScanMapTrackPoint *point = point_l->data;

        point->straight = hyscan_map_track_is_straight (point_l);
      }
}

/* Расширяет область from - to так, чтобы в нее поместилась точка point. */
inline static void
hyscan_map_track_extend (HyScanMapTrackPoint  *point,
                         HyScanGeoCartesian2D *from,
                         HyScanGeoCartesian2D *to)
{
  from->x = MIN (from->x, point->ship_c2d.x - 1.1 * point->b_dist);
  from->y = MIN (from->y, point->ship_c2d.y - 1.1 * point->b_dist);
  to->x   = MAX (to->x, point->ship_c2d.x + 1.1 * point->b_dist);
  to->y   = MAX (to->y, point->ship_c2d.y + 1.1 * point->b_dist);
}

/* Загружает новые точки по индексам навигации.
 */
static void
hyscan_map_track_load_nav (HyScanMapTrack *track)
{
  HyScanMapTrackPrivate *priv = track->priv;
  HyScanMapTrackNav *nav = &priv->nav;
  GList *last_link;
  GList *points = NULL;
  guint32 first, last, index;
  guint32 mod_count;

  if (nav->lat_data == NULL)
    return;

  mod_count = hyscan_nav_data_get_mod_count (nav->lat_data);

  hyscan_nav_data_get_range (nav->lat_data, &first, &last);
  hyscan_map_track_remove_expired (nav->points, first, last);

  /* Определяем индекс навигационных данных, с которого надо начать загрузку. */
  if ((last_link = g_list_last (nav->points)) != NULL)
    index = ((HyScanMapTrackPoint *) last_link->data)->index + 1;
  else
    index = first;

  for (; index <= last; ++index)
    {
      gint64 time;
      HyScanGeoPoint coords;
      gdouble course;
      HyScanMapTrackPoint point = { 0 };

      if (!hyscan_nav_data_get (nav->lat_data, NULL, index, &time, &coords.lat))
        continue;

      if (!hyscan_nav_data_get (nav->lon_data, NULL, index, &time, &coords.lon))
        continue;

      if (!hyscan_nav_data_get (nav->trk_data, NULL, index, &time, &course))
        continue;

      point.source = HYSCAN_GTK_MAP_TRACK_DRAW_SOURCE_NAV;
      point.time = time;
      point.index = index;
      point.geo = coords;
      point.course = course;

      points = g_list_append (points, hyscan_map_track_point_copy(&point));
    }

  /* Переводим географические координаты в логические. */
  nav->points = g_list_concat (nav->points, points);
  hyscan_map_track_cartesian (track, points);

  /* Отмечаем область, в которую поступили новые данные. */
  if (points != NULL)
    {
      GList *link;
      HyScanMapTrackPoint *point, *prev_point;

      for (link = points; link != NULL; link = link->next)
        {
          if (link->prev == NULL)
            continue;

          prev_point = link->prev->data;
          point = link->data;

          /* Составляем список новых отрезков. */
          priv->mod_list = hyscan_map_track_add_modified (priv->mod_list, &point->ship_c2d, &prev_point->ship_c2d);

          /* Расширяем область, внутри которой находится изображение галса. */
          hyscan_map_track_extend (point, &priv->extent_from, &priv->extent_to);
        }
    }

  nav->mod_count = mod_count;
}

static GList *
hyscan_map_track_add_modified (GList                *list,
                               HyScanGeoCartesian2D *from,
                               HyScanGeoCartesian2D *to)
{
  HyScanMapTrackMod *extent = g_slice_new (HyScanMapTrackMod);
  extent->from = *from;
  extent->to = *to;

  return g_list_prepend (list, extent);
}

/* Загружает новые точки по индексам амплитуды на указанном борту. */
static void
hyscan_map_track_load_side (HyScanMapTrack     *track,
                             HyScanMapTrackSide *side)
{
  HyScanMapTrackPrivate *priv = track->priv;
  HyScanMapTrackNav *nav = &priv->nav;
  GList *points = NULL;
  guint32 index, first, last;
  GList *last_link;
  guint32 mod_count;

  if (side->amplitude == NULL || !nav->opened)
    return;

  mod_count = hyscan_amplitude_get_mod_count (side->amplitude);
  hyscan_amplitude_get_range (side->amplitude, &first, &last);
  hyscan_map_track_remove_expired (side->points, first, last);

  /* Определяем индекс амплитудных данных, с которого надо начать загрузку. */
  if ((last_link = g_list_last (side->points)) != NULL)
    index = ((HyScanMapTrackPoint *) last_link->data)->index + 1;
  else
    index = first;

  for (; index <= last; ++index)
    {
      gint64 time;
      HyScanGeoPoint coords;
      gdouble course;
      HyScanMapTrackPoint point = { 0 };
      gboolean nav_found;

      /* Определяем время получения амплитудных данных. */
      hyscan_amplitude_get_size_time (side->amplitude, index, NULL, &time);

      /* Находим навигационные данные для этой метки времени. */
      nav_found = hyscan_nav_smooth_get (nav->lat_smooth, NULL, time, &coords.lat) &&
                  hyscan_nav_smooth_get (nav->lon_smooth, NULL, time, &coords.lon) &&
                  hyscan_nav_smooth_get (nav->trk_smooth, NULL, time, &course);

      if (!nav_found)
        continue;

      point.source = side->source;
      point.index = index;
      point.geo = coords;
      point.course = course;
      hyscan_map_track_load_length_by_idx (track, side, &point);

      points = g_list_append (points, hyscan_map_track_point_copy (&point));
    }

  /* Переводим географические координаты в логические. */
  side->points = g_list_concat (side->points, points);
  hyscan_map_track_cartesian (track, points);

  /* Отмечаем на тайловом слое устаревшую область. */
  if (points != NULL)
    {
      GList *link;
      HyScanMapTrackPoint *point;

      for (link = points->prev != NULL ? points->prev : points; link != NULL; link = link->next)
        {
          point = link->data;

          priv->mod_list = hyscan_map_track_add_modified (priv->mod_list, &point->ship_c2d, &point->fr_c2d);

          /* Расширяем область, внутри которой находится изображение галса. */
          hyscan_map_track_extend (point, &priv->extent_from, &priv->extent_to);
        }
    }

  side->mod_count = mod_count;
}

/* Расширяет область from - to так, чтобы в нее поместились все точки points. */
static void
hyscan_map_track_update_extent (GList                *points,
                                HyScanGeoCartesian2D *from,
                                HyScanGeoCartesian2D *to)
{
  GList *link;

  for (link = points; link != NULL; link = link->next)
    hyscan_map_track_extend (link->data, from, to);
}

/* Удаляет из трека путевые точки, которые не попадают в указанный диапазон. */
static void
hyscan_map_track_remove_expired (GList   *points,
                                 guint32  first_index,
                                 guint32  last_index)
{
  GList *point_l;
  HyScanMapTrackPoint *point;

  if (points == NULL)
    return;

  /* Проходим все точки галса и оставляем только актуальную информацию: 1-3. */
  point_l = points;

  /* 1. Удаляем точки до first_index. */
  while (point_l != NULL)
    {
      GList *next = point_l->next;
      point = point_l->data;

      if (point->index >= first_index)
        break;

      hyscan_map_track_point_free (point);
      points = g_list_delete_link (points, point_l);

      point_l = next;
    }

  /* 2. Оставляем точки до last_index, по которым загружены данные бортов. */
  while (point_l != NULL)
    {
      GList *next = point_l->next;
      point = point_l->data;

      if (point->index > last_index)
        break;

      point_l = next;
    }

  /* 3. Остальное удаляем. */
  while (point_l != NULL)
    {
      GList *next = point_l->next;
      point = point_l->data;

      hyscan_map_track_point_free (point);
      points = g_list_delete_link (points, point_l);

      point_l = next;
    }
}

/* Функция открывает обработчик одного из бортов ГБО. */
static void
hyscan_map_track_open_side (HyScanMapTrackPrivate *priv,
                             HyScanMapTrackSide    *side,
                             HyScanSourceType        source)
{
  HyScanAcousticData *signal;
  HyScanAcousticDataInfo info;
  gdouble lambda;
  HyScanParamList *list;
  guint channel = 0;
  const gchar *param_name = source == HYSCAN_SOURCE_SIDE_SCAN_PORT ? "/channel-port" : "/channel-starboard";

  /* Удаляем текущие объекты. */
  hyscan_gtk_map_track_side_clear (side);
  side->writeable = FALSE;

  /* Считываем параметры. */
  list = hyscan_param_list_new ();
  hyscan_param_list_add (list, param_name);
  if (hyscan_param_get (HYSCAN_PARAM (priv->param), list))
    channel = hyscan_param_list_get_boolean (list, param_name) ? DEFAULT_CHANNEL_SS : 0;
  g_object_unref (list);

  if (channel == 0)
    return;

  signal = hyscan_acoustic_data_new (priv->db, priv->cache, priv->project, priv->name, source, channel, FALSE);
  if (signal == NULL)
    {
      g_warning ("HyScanMapTrack: failed to open acoustic data");
      return;
    }

  side->amplitude = HYSCAN_AMPLITUDE (signal);
  side->writeable = hyscan_amplitude_is_writable (side->amplitude);
  side->projector = hyscan_projector_new (side->amplitude);
  side->offset = hyscan_amplitude_get_offset (side->amplitude);

  /* Параметры диаграммы направленности. */
  info = hyscan_amplitude_get_info (side->amplitude);
  lambda = SOUND_VELOCITY / info.signal_frequency;
  side->antenna_length = info.antenna_haperture > 0 ? info.antenna_haperture : DEFAULT_HAPERTURE;
  side->beam_width = asin (lambda / side->antenna_length);
  side->near_field = (side->antenna_length * side->antenna_length) / lambda;

  /* Обработчик качества. */
  side->quality = hyscan_track_proj_quality_new (priv->db, priv->cache, priv->project, priv->name, source);
}

static void
hyscan_map_track_open_nav (HyScanMapTrackPrivate *priv)
{
  HyScanMapTrackNav *nav = &priv->nav;

  g_clear_object (&nav->lat_data);
  g_clear_object (&nav->lon_data);
  g_clear_object (&nav->trk_data);
  g_clear_object (&nav->lat_smooth);
  g_clear_object (&nav->lon_smooth);
  g_clear_object (&nav->trk_smooth);
  nav->writeable = FALSE;

  nav->lat_data = hyscan_map_track_param_get_nav_data (priv->param, HYSCAN_NMEA_FIELD_LAT, priv->cache);
  nav->lon_data = hyscan_map_track_param_get_nav_data (priv->param, HYSCAN_NMEA_FIELD_LON, priv->cache);
  nav->trk_data = hyscan_map_track_param_get_nav_data (priv->param, HYSCAN_NMEA_FIELD_TRACK, priv->cache);

  nav->opened = (nav->lat_data != NULL && nav->lon_data != NULL && nav->trk_data != NULL);
  if (!nav->opened)
    return;

  nav->lat_smooth = hyscan_nav_smooth_new (nav->lat_data);
  nav->lon_smooth = hyscan_nav_smooth_new (nav->lon_data);
  nav->trk_smooth = hyscan_nav_smooth_new_circular (nav->trk_data);
  nav->writeable = hyscan_nav_data_is_writable (nav->lat_data);
  nav->offset = hyscan_nav_data_get_offset (nav->lat_data);
}

static void
hyscan_map_track_open_depth (HyScanMapTrackPrivate *priv)
{
  HyScanNavData *dpt_parser;
  
  g_clear_object (&priv->depth.meter);
  
  priv->depth.meter = hyscan_map_track_param_get_depthometer (priv->param, priv->cache);
  if (priv->depth.meter == NULL)
    return;

  dpt_parser = hyscan_depthometer_get_nav_data (priv->depth.meter);
  priv->depth.offset = hyscan_nav_data_get_offset (HYSCAN_NAV_DATA (dpt_parser));
  g_object_unref (dpt_parser);
}

/* Открывает (или переоткрывает) каналы данных в указанном галсе. */
static void
hyscan_map_track_open (HyScanMapTrack *track)
{
  HyScanMapTrackPrivate *priv = track->priv;

  hyscan_map_track_open_nav (priv);
  hyscan_map_track_open_depth (priv);
  hyscan_map_track_open_side (priv, &priv->starboard, HYSCAN_SOURCE_SIDE_SCAN_STARBOARD);
  hyscan_map_track_open_side (priv, &priv->port, HYSCAN_SOURCE_SIDE_SCAN_PORT);

  priv->loaded = FALSE;
  priv->opened = TRUE;
}

static void
hyscan_map_track_reset_extent (HyScanMapTrackPrivate *priv)
{
  priv->extent_from.x = G_MAXDOUBLE;
  priv->extent_from.y = G_MAXDOUBLE;
  priv->extent_to.x = -G_MAXDOUBLE;
  priv->extent_to.y = -G_MAXDOUBLE;
}

/* Функция удаляет структуру #HyScanMapTrackMod. */
static void
hyscan_map_track_mod_free (HyScanMapTrackMod *mod)
{
  g_slice_free (HyScanMapTrackMod, mod);
}

/* Функция проверяет, обновились ли параметры галса. */
static void
hyscan_map_track_param_update (HyScanMapTrack *track)
{
  HyScanMapTrackPrivate *priv = track->priv;
  guint32 mod_count;

  mod_count = hyscan_map_track_param_get_mod_count (priv->param);
  if (priv->param_mod_count == mod_count)
    return;

  /* Ставим флаг о необходимости переоткрыть галс. */
  priv->opened = FALSE;
  priv->loaded = FALSE;
  priv->param_mod_count = mod_count;
}

/* Загружает путевые точки трека и его ширину. */
static gboolean
hyscan_map_track_load (HyScanMapTrack *track)
{
  HyScanMapTrackPrivate *priv = track->priv;
  guint32 mod_count;

  /* Обновляем параметры. */
  hyscan_map_track_param_update (track);

  /* Открываем каналы данных. */
  if (!priv->opened)
    hyscan_map_track_open (track);

  /* Приводим список точек в соответствие с флагом priv->loaded. */
  if (!priv->loaded)
    {
      hyscan_map_track_reset_extent (priv);
      g_list_free_full (priv->nav.points, (GDestroyNotify) hyscan_map_track_point_free);
      g_list_free_full (priv->port.points, (GDestroyNotify) hyscan_map_track_point_free);
      g_list_free_full (priv->starboard.points, (GDestroyNotify) hyscan_map_track_point_free);
      priv->nav.points = NULL;
      priv->port.points = NULL;
      priv->starboard.points = NULL;
    }

  /* Если проекция изменилась, то пересчитываем координаты точек. */
  if (priv->proj_changed)
    {
      hyscan_map_track_cartesian (track, priv->nav.points);
      hyscan_map_track_cartesian (track, priv->port.points);
      hyscan_map_track_cartesian (track, priv->starboard.points);

      /* Обновляем границы галса. */
      hyscan_map_track_reset_extent (priv);
      hyscan_map_track_update_extent (priv->nav.points, &priv->extent_from, &priv->extent_to);
      hyscan_map_track_update_extent (priv->port.points, &priv->extent_from, &priv->extent_to);
      hyscan_map_track_update_extent (priv->starboard.points, &priv->extent_from, &priv->extent_to);
      priv->proj_changed = FALSE;
    }

  /* Если нет навигационных данных, то невозможно ничего загрузить. Выходим. */
  if (!priv->nav.opened)
    return FALSE;

  /* Проверяем наличие новых данных. */
  mod_count = hyscan_map_track_get_mod_count (track);
  if (priv->loaded && priv->loaded_mod_count == mod_count)
    return FALSE;

  /* Очищаем список изменённых регионов. */
  g_list_free_full (priv->mod_list, (GDestroyNotify) hyscan_map_track_mod_free);
  priv->mod_list = NULL;

  /* Загружаем точки. */
  hyscan_map_track_load_nav (track);
  hyscan_map_track_load_side (track, &priv->port);
  hyscan_map_track_load_side (track, &priv->starboard);

  priv->loaded = TRUE;
  priv->loaded_mod_count = mod_count;

  g_signal_emit (track, hyscan_map_track_signals[SIGNAL_AREA_MOD], 0, priv->mod_list);

  return TRUE;
}

/**
 * hyscan_map_track_new:
 * @db: указатель на базу данных #HyScanDB
 * @cache: кэш #HyScanCache
 * @project_name: название проекта
 * @track_name: название галса
 * @projection: картографическая проекция
 *
 * Создаёт объект проекции галса.
 *
 * Returns: новый объект #HyScanMapTrack. Для удаления g_object_unref().
 */
HyScanMapTrack *
hyscan_map_track_new (HyScanDB            *db,
                       HyScanCache         *cache,
                       const gchar         *project_name,
                       const gchar         *track_name,
                       HyScanGeoProjection *projection)
{
  return g_object_new (HYSCAN_TYPE_MAP_TRACK,
                       "db", db,
                       "cache", cache,
                       "project", project_name,
                       "name", track_name,
                       "projection", projection,
                       NULL);
}

/**
 * hyscan_map_track_get:
 * @track: указатель на #HyScanMapTrack
 * @data: (out) (transfer none): структура со списком точек
 *
 * Функция получает список точек галса.
 */
gboolean
hyscan_map_track_get (HyScanMapTrack     *track,
                      HyScanMapTrackData *data)
{
  HyScanMapTrackPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_MAP_TRACK (track), FALSE);
  g_return_val_if_fail (data != NULL, FALSE);

  priv = track->priv;

  /* Обновляем данные. */
  hyscan_map_track_load (track);

  data->port = priv->port.points;
  data->starboard = priv->starboard.points;
  data->nav = priv->nav.points;
  data->from = priv->extent_from;
  data->to = priv->extent_to;

  return TRUE;
}

/**
 * hyscan_map_track_view:
 * @track_layer: указатель на слой #HyScanGtkMapTrack
 * @track_name: название галса
 * @from: (out): координата левой верхней точки границы
 * @to: (out): координата правой нижней точки границы
 *
 * Возвращает границы области, в которой находится галс.
 *
 * Returns: %TRUE, если получилось определить границы галса; иначе %FALSE
 */
gboolean
hyscan_map_track_view (HyScanMapTrack         *track,
                        HyScanGeoCartesian2D  *from,
                        HyScanGeoCartesian2D  *to)
{
  HyScanMapTrackPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_MAP_TRACK (track), FALSE);
  priv = track->priv;

  /* Обновляем данные. */
  hyscan_map_track_load (track);

  if (priv->nav.points == NULL)
    return FALSE;

  *from = priv->extent_from;
  *to = priv->extent_to;

  return TRUE;
}

/**
 * hyscan_map_track_get_mod_count:
 * @track: указатель на #HyScanMapTrack
 *
 * Функция возвращает номер изменения в данных. Программа не должна полагаться на значение номера изменения,
 * важен только факт смены номера по сравнению с предыдущим запросом. Изменениями считаются события в базе данных,
 * связанные с используемыми каналоми данных.
 *
 * Returns: значение счётчика изменений.
 */
guint32
hyscan_map_track_get_mod_count (HyScanMapTrack *track)
{
  HyScanMapTrackPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_MAP_TRACK (track), 0);
  priv = track->priv;

  return (priv->nav.lat_data == NULL ? 0 : hyscan_nav_data_get_mod_count (priv->nav.lat_data)) +
         (priv->port.amplitude == NULL ? 0 : hyscan_amplitude_get_mod_count (priv->port.amplitude)) +
         (priv->starboard.amplitude == NULL ? 0 : hyscan_amplitude_get_mod_count (priv->starboard.amplitude));
}

/**
 * hyscan_map_track_get_param:
 * @track: указатель на #HyScanMapTrack
 *
 * Returns: (transfer none): указатель на параметры галса.
 */
HyScanMapTrackParam *
hyscan_map_track_get_param (HyScanMapTrack *track)
{
  g_return_val_if_fail (HYSCAN_IS_MAP_TRACK (track), NULL);

  return track->priv->param;
}

/**
 * hyscan_map_track_get_quality_port:
 * @track: указатель на #HyScanMapTrack
 *
 * Функция получает объект оценки качества акустических данных для левого борта ГБО.
 *
 * Returns: (transfer none): данные качества левого борта
 */
HyScanTrackProjQuality *
hyscan_map_track_get_quality_port (HyScanMapTrack *track)
{
  g_return_val_if_fail (HYSCAN_IS_MAP_TRACK (track), NULL);

  return track->priv->port.quality;
}

/**
 * hyscan_map_track_get_quality_starboard:
 * @track: указатель на #HyScanMapTrack
 *
 * Функция получает объект оценки качества акустических данных для правого борта ГБО.
 *
 * Returns: (transfer none): данные качества правого борта
 */
HyScanTrackProjQuality *
hyscan_map_track_get_quality_starboard (HyScanMapTrack *track)
{
  g_return_val_if_fail (HYSCAN_IS_MAP_TRACK (track), NULL);

  return track->priv->starboard.quality;
}

/**
 * hyscan_map_track_set_projection:
 * @track: указатель на #HyScanMapTrack
 * @projection: проекция #HyScanGeoProjection
 *
 * Устанавливает картографическую проекцию галса.
 */
void
hyscan_map_track_set_projection (HyScanMapTrack         *track,
                                  HyScanGeoProjection   *projection)
{
  HyScanMapTrackPrivate *priv;

  g_return_if_fail (HYSCAN_IS_MAP_TRACK (track));
  priv = track->priv;

  priv->proj_changed = priv->projection == NULL ||
                       hyscan_geo_projection_hash (projection) != hyscan_geo_projection_hash (priv->projection);

  g_clear_object (&priv->projection);
  priv->projection = g_object_ref (projection);
}

/**
 * hyscan_map_track_point_free:
 * @point: указатель на #HyScanMapTrackPoint
 *
 * Удаляет структуру #HyScanMapTrackPoint.
 */
void
hyscan_map_track_point_free (HyScanMapTrackPoint *point)
{
  if (point == NULL)
    return;

  g_slice_free (HyScanMapTrackPoint, point);
}

/**
 * hyscan_map_track_point_copy:
 * @point: указатель на #HyScanMapTrackPoint
 *
 * Создаёт копию структуры #HyScanMapTrackPoint.
 *
 * Returns: копия структуры HyScanMapTrackPoint, для удаления hyscan_map_track_point_free().
 */
HyScanMapTrackPoint *
hyscan_map_track_point_copy (const HyScanMapTrackPoint *point)
{
  return point != NULL ? g_slice_dup (HyScanMapTrackPoint, point) : NULL;
}
