/*
 * \file hyscan-projector.c
 *
 * \brief Исходный файл класса сопоставления индексов и отсчётов реальным координатам.
 * \author Dmitriev Alexander (m1n7@yandex.ru)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 *
 */
#include "hyscan-projector.h"
#include <hyscan-acoustic-data.h>
#include <math.h>

enum
{
  PROP_O,
  PROP_DB,
  PROP_PROJECT_NAME,
  PROP_TRACK_NAME,
  PROP_SOURCE_TYPE,
  PROP_RAW
};

struct _HyScanProjectorPrivate
{
  HyScanDB             *db;              /* БД. */
  gchar                *project;         /* Проект. */
  gchar                *track;           /* Галс. */
  HyScanSourceType      source;          /* Тип источника. */
  gboolean              raw;             /* Сырые данные. */

  HyScanAcousticData   *dc;              /* Основной КД. */
  HyScanAntennaPosition position;        /* Местоположение антенны. */

  HyScanAcousticData  **addon_dc;        /* Дополнительные КД. */

  GArray               *sound_velocity;  /* Профиль скорости звука. */
  gfloat                dfreq;           /* Частота дискретизации. */
  gfloat                old_dfreq;       /* Предыдущая частота дискретизации. */
  gfloat               *sv_counts;       /* Массив глубин. */
  gfloat               *sv_sound;        /* Массив скоростей звука. */
  gfloat               *sv_depth;        /* Массив скоростей звука. */
  guint                 size;

  gdouble              *precalc_counts;  /* Массив предрассчитанных точек для перевода из отсчётов в координаты. */
  guint32               n_precalc;       /* Количество предрассчитанных точек. */

  gfloat                ship_speed;      /* Скорость судна. */

  gint64                zero_time;       /* Самое раннее время во всем галсе. */
};

static void    hyscan_projector_set_property             (GObject                *object,
                                                          guint                   prop_id,
                                                          const GValue           *value,
                                                          GParamSpec             *pspec);
static void    hyscan_projector_object_constructed       (GObject                *object);
static void    hyscan_projector_object_finalize          (GObject                *object);
static void    hyscan_projector_precalculate_points      (HyScanProjector        *self);
static void    hyscan_projector_sound_velocity_parser    (HyScanProjectorPrivate *priv);
static gdouble hyscan_projector_count_to_coord_internal  (HyScanProjector        *self,
                                                          guint32                 count);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanProjector, hyscan_projector, G_TYPE_OBJECT);

static void
hyscan_projector_class_init (HyScanProjectorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_projector_set_property;

  object_class->constructed = hyscan_projector_object_constructed;
  object_class->finalize = hyscan_projector_object_finalize;

  g_object_class_install_property (object_class, PROP_DB,
    g_param_spec_object ("db", "DB", "HyScanDB interface", HYSCAN_TYPE_DB,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_PROJECT_NAME,
    g_param_spec_string ("project-name", "ProjectName", "Project name", NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_TRACK_NAME,
    g_param_spec_string ("track-name", "TrackName", "Track name", NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_SOURCE_TYPE,
    g_param_spec_int ("source-type", "SourceType", "Source type", 0, G_MAXINT, 0,
                      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_RAW,
    g_param_spec_boolean ("raw", "Raw", "Use raw data type", FALSE,
                          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_projector_init (HyScanProjector *self)
{
  self->priv = hyscan_projector_get_instance_private (self);
}

static void
hyscan_projector_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  HyScanProjector *data = HYSCAN_PROJECTOR (object);
  HyScanProjectorPrivate *priv = data->priv;

  switch (prop_id)
    {
    case PROP_DB:
      priv->db = g_value_dup_object (value);
      break;

    case PROP_PROJECT_NAME:
      priv->project = g_value_dup_string (value);
      break;

    case PROP_TRACK_NAME:
      priv->track = g_value_dup_string (value);
      break;

    case PROP_SOURCE_TYPE:
      priv->source = g_value_get_int (value);
      break;

    case PROP_RAW:
      priv->raw = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_projector_object_constructed (GObject *object)
{
  HyScanProjector *self = HYSCAN_PROJECTOR (object);
  HyScanProjectorPrivate *priv = self->priv;
  HyScanAcousticDataInfo info;
  guint32 index0, npoints;

  hyscan_projector_set_ship_speed (self, 1.0);
  hyscan_projector_set_sound_velocity (self, NULL);

  if (priv->db == NULL || priv->project == NULL || priv->track == NULL)
    return;

  priv->dc = hyscan_acoustic_data_new (priv->db, priv->project, priv->track,
                                       priv->source, priv->raw);
  if (priv->dc == NULL)
    return;

  info = hyscan_acoustic_data_get_info (priv->dc);
  priv->dfreq = info.data.rate;

  if (!hyscan_acoustic_data_get_range (priv->dc, &index0, NULL))
    {
      g_clear_object (&priv->dc);
      return;
    }

  priv->position = hyscan_acoustic_data_get_position (priv->dc);

  /* Инициализируем самое ранее время галса. */
  hyscan_acoustic_data_get_values (priv->dc, index0, &npoints, &priv->zero_time);
}

static void
hyscan_projector_object_finalize (GObject *object)
{
  HyScanProjector *self = HYSCAN_PROJECTOR (object);
  HyScanProjectorPrivate *priv = self->priv;

  g_clear_object (&priv->db);
  g_free (priv->project);
  g_free (priv->track);
  g_clear_object (&priv->dc);

  g_array_unref (priv->sound_velocity);
  g_free (priv->sv_counts);
  g_free (priv->sv_sound);
  g_free (priv->sv_depth);
  g_free (priv->precalc_counts);

  G_OBJECT_CLASS (hyscan_projector_parent_class)->finalize (object);
}

/* Функция выполняет предрассчёт точек. */
static void
hyscan_projector_precalculate_points (HyScanProjector *self)
{
  guint i;
  HyScanProjectorPrivate *priv = self->priv;

  g_clear_pointer (&priv->precalc_counts, g_free);

  /* Выходим, если нечего рассчитывать. */
  if (priv->n_precalc == 0)
    return;

  priv->precalc_counts = g_malloc0 (priv->n_precalc * sizeof (gdouble));

  for (i = 0; i < priv->n_precalc; i++)
    priv->precalc_counts[i] = hyscan_projector_count_to_coord_internal (self, i);
}

/* Функция парсит GArray с HyScanSoundVelocity.*/
static void
hyscan_projector_sound_velocity_parser (HyScanProjectorPrivate *priv)
{
  guint i, len;
  HyScanSoundVelocity *link, *prelink;
  gfloat dfreq;

  len = priv->sound_velocity->len;
  dfreq = priv->dfreq;

  /* Реаллоцируем память. */
  g_free (priv->sv_counts);
  g_free (priv->sv_sound);
  g_free (priv->sv_depth);

  priv->size = len;
  priv->sv_counts = g_malloc0 (len * sizeof (gfloat));
  priv->sv_sound = g_malloc0 (len * sizeof (gfloat));
  priv->sv_depth = g_malloc0 (len * sizeof (gfloat));

  for (i = 0; i < len; i++)
    {
      link = &g_array_index (priv->sound_velocity, HyScanSoundVelocity, i);

      /* В sv_counts мы записываем, НАЧИНАЯ с какого
       * индекса действует эта скорость звука.
       * i == 0 - особый случай. В первый элемент
       * просто переписываем, т.к. он всегда обязан
       * соответствовать нулевой глубине.
       * C дальнейшими элементами чуть трудней.
       */
      if (i == 0)
        {
          priv->sv_counts[0] = priv->sv_depth[0] = 0;
          priv->sv_sound[0] = link->velocity;
        }
      else
        {
          prelink = &g_array_index (priv->sound_velocity, HyScanSoundVelocity, i - 1);
          priv->sv_counts[i] = (link->depth - prelink->depth) * dfreq * 2 / prelink->velocity + priv->sv_counts[i - 1];
          priv->sv_sound[i] = link->velocity;
          priv->sv_depth[i] = link->depth;
        }
    }

  priv->old_dfreq = dfreq;
}

/* Функция вычисляет координату отсчёта. */
static gdouble
hyscan_projector_count_to_coord_internal (HyScanProjector   *self,
                                          guint32            count)
{
  HyScanProjectorPrivate *priv;
  gdouble coord = 0;
  gint32 max, i;
  gint32 size;
  gfloat *sv_counts, *sv_sound;

  priv = self->priv;

  size = priv->size;
  sv_counts = priv->sv_counts;
  sv_sound = priv->sv_sound;

  for (max = i = 0; i < size; i++)
    {
      if (count <= sv_counts[i])
        break;

      max = i;
      if (i > 0)
        coord += (sv_counts[i] - sv_counts[i - 1]) * sv_sound[i - 1];
    }
    
  coord += (count - sv_counts[max]) * sv_sound[max];
  coord /= priv->dfreq * 2;

  return coord;
}

/* Функция вычисляет координату отсчёта. */
static gdouble
hyscan_projector_coord_to_count_internal (HyScanProjector   *self,
                                          gdouble            coord)
{
  HyScanProjectorPrivate *priv;
  gdouble count = 0;
  gint32 max, i;
  gint32 size;
  gfloat *sv_counts, *sv_sound, *sv_depth;

  priv = self->priv;

  size = priv->size;
  sv_counts = priv->sv_counts;
  sv_sound = priv->sv_sound;
  sv_depth = priv->sv_depth;

  count = coord * priv->dfreq * 2;

  for (max = i = 0; i < size; i++)
    {
      if (coord <= sv_depth[i])
        break;
      max = i;
    }


  for (i = max; i > 0; i--)
    count -= (sv_counts[i] - sv_counts[i - 1]) * sv_sound[i - 1];

  count /= sv_sound[max];
  count += sv_counts[max];

  return (guint32) round (count);
}

HyScanProjector*
hyscan_projector_new (HyScanDB         *db,
                      const gchar      *project,
                      const gchar      *track,
                      HyScanSourceType  source,
                      gboolean          raw)
{
  HyScanProjector *self;
  self = g_object_new (HYSCAN_TYPE_PROJECTOR,
                            "db", db,
                            "project-name", project,
                            "track-name", track,
                            "source-type", source,
                            "raw", raw,
                            NULL);

  if (self->priv->dc == NULL)
    g_clear_object (&self);

  return self;
}

void
hyscan_projector_set_cache (HyScanProjector *self,
                            HyScanCache     *cache)
{
  g_return_if_fail (HYSCAN_IS_PROJECTOR (self));

  if (self->priv->dc == NULL)
    return;

  hyscan_acoustic_data_set_cache (self->priv->dc, cache, "PREFIX");
}

gboolean
hyscan_projector_check_source (HyScanProjector   *self,
                               HyScanSourceType   source,
                               gboolean           raw,
                               gboolean          *changed)
{
  HyScanProjectorPrivate *priv;
  HyScanAcousticData *dc;
  gboolean _changed, status;
  guint32 index0, npoints;
  gint64 new_time;

  g_return_val_if_fail (HYSCAN_IS_PROJECTOR (self), FALSE);
  priv = self->priv;

  dc = hyscan_acoustic_data_new (priv->db, priv->project,
                                 priv->track, source, raw);

  if (dc == NULL || !hyscan_acoustic_data_get_range (dc, &index0, NULL))
    {
      status = FALSE;
      goto exit;
    }

  hyscan_acoustic_data_get_values (dc, index0, &npoints, &new_time);
  if (new_time == -1)
    {
      status = FALSE;
      goto exit;
    }

  if (new_time < priv->zero_time)
    {
      priv->zero_time = new_time;
      _changed = TRUE;
    }
  else
    {
      _changed = FALSE;
    }

  status = TRUE;

  if (changed != NULL)
    *changed = _changed;

exit:
  g_clear_object (&dc);
  return status;
}

void
hyscan_projector_set_ship_speed (HyScanProjector *self,
                                 gfloat           speed)
{
  g_return_if_fail (HYSCAN_IS_PROJECTOR (self));

  if (speed > 0)
    self->priv->ship_speed = speed;
}

void
hyscan_projector_set_sound_velocity (HyScanProjector   *self,
                                     GArray            *velocity)
{
  HyScanProjectorPrivate *priv;

  g_return_if_fail (HYSCAN_IS_PROJECTOR (self));
  priv = self->priv;

  /* Пересоздаем. */
  g_clear_pointer (&priv->sound_velocity, g_array_unref);
  priv->sound_velocity = g_array_new (FALSE, TRUE, sizeof (HyScanSoundVelocity));

  /* Если передан NULL, то надо установить параметры по умолчанию. */
  if (velocity == NULL || velocity->len == 0)
    {
      HyScanSoundVelocity value;
      value.depth = 0.0;
      value.velocity = 1500.0;
      g_array_append_val (priv->sound_velocity, value);
    }
  /* Иначе переписываем всё. */
  else
    {
      HyScanSoundVelocity *link;
      guint i;

      for (i = 0; i < velocity->len; i++)
        {
          link = &g_array_index (velocity, HyScanSoundVelocity, i);
          g_array_append_val (priv->sound_velocity, *link);
        }
    }

  /* Теперь нужно распарсить priv->sound_velocity. */
  hyscan_projector_sound_velocity_parser (priv);
  hyscan_projector_precalculate_points (self);
}

void
hyscan_projector_set_precalc_points (HyScanProjector   *self,
                                     guint32            points)
{
  g_return_if_fail (HYSCAN_IS_PROJECTOR (self));

  self->priv->n_precalc = points;

  hyscan_projector_precalculate_points (self);
}

const HyScanAcousticData*
hyscan_projector_get_acoustic_data (HyScanProjector *self)
{
  g_return_val_if_fail (HYSCAN_IS_PROJECTOR (self), NULL);
  return self->priv->dc;
}


HyScanDBFindStatus
hyscan_projector_find_index_by_coord (HyScanProjector   *self,
                                      gdouble            coord,
                                      guint32           *lindex,
                                      guint32           *rindex)
{
  HyScanProjectorPrivate *priv;
  gdouble time;
  gint64 _time;

  g_return_val_if_fail (HYSCAN_IS_PROJECTOR (self), HYSCAN_DB_FIND_FAIL);
  priv = self->priv;

  /* В случае постоянной скорости надо просто учесть
   * сдвижку вдоль судна. Все вычисления будем проводить в double,
   * а приведем к gint64 перед самым использованием. */

  time = (coord - priv->position.x) / priv->ship_speed;
  time *= G_TIME_SPAN_SECOND;
  time += priv->zero_time;

  _time = time;
  return hyscan_acoustic_data_find_data (priv->dc, _time, lindex, rindex, NULL, NULL);
}

gboolean
hyscan_projector_index_to_coord (HyScanProjector   *self,
                                 guint32            index,
                                 gdouble           *along)
{
  HyScanProjectorPrivate *priv;
  gint64 time;
  guint32 npoints;
  gdouble coord;

  g_return_val_if_fail (HYSCAN_IS_PROJECTOR (self), FALSE);
  priv = self->priv;

  /* Время приема данных. */
  hyscan_acoustic_data_get_values (priv->dc, index, &npoints, &time);

  if (time < 0)
    return FALSE;

  /* Координата от абсолютного начала галса. */
  coord = (time - priv->zero_time) * priv->ship_speed;

  /* Так как время в БД указывается в микросекундах: */
  coord /= G_TIME_SPAN_SECOND;

  /* Теперь надо учесть сдвиг датчика относительно центра масс. */
  coord += priv->position.x;

  *along = coord;
  return TRUE;
}

gboolean
hyscan_projector_coord_to_index (HyScanProjector   *self,
                                 gdouble            along,
                                 guint32           *index)
{
  gint64 time;
  guint32 lindex, rindex;
  gint64 ltime, rtime;
  HyScanDBFindStatus found;
  HyScanProjectorPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_PROJECTOR (self), FALSE);
  priv = self->priv;

  /* Задача обратная к той, что решается в функции hyscan_projector_index_to_coord. */
  along -= priv->position.x;
  along *= G_TIME_SPAN_SECOND;
  time = along / priv->ship_speed + priv->zero_time;

  found = hyscan_acoustic_data_find_data (priv->dc, time, &lindex, &rindex, &ltime, &rtime);

  if (found != HYSCAN_DB_FIND_OK)
    return FALSE;

  if (fabs (ltime - time) < fabs (rtime - time))
    *index = lindex;
  else
    *index = rindex;

  return TRUE;
}

/* Функция вычисляет координату отсчёта. */
gboolean
hyscan_projector_count_to_coord (HyScanProjector   *self,
                                 guint32            count,
                                 gdouble           *across,
                                 gdouble            depth)
{
  HyScanProjectorPrivate *priv;
  gdouble coord = 0;

  g_return_val_if_fail (HYSCAN_IS_PROJECTOR (self), FALSE);
  priv = self->priv;

  /* TODO: смещение по у. */
  /* Сначала проверяем, есть ли предрассчитанная точка. */
  if (count < priv->n_precalc)
    coord = priv->precalc_counts[count];
  else
    coord = hyscan_projector_count_to_coord_internal (self, count);

  if (depth > 0.0)
    coord = sqrt (coord * coord - depth * depth);
  else if (depth < 0.0)
    return FALSE;

  *across = coord;

  return TRUE;
}

gboolean
hyscan_projector_coord_to_count (HyScanProjector   *self,
                                 gdouble            across,
                                 guint32           *count,
                                 gdouble            depth)
{
  g_return_val_if_fail (HYSCAN_IS_PROJECTOR (self), FALSE);

  if (depth > 0.0)
    across = sqrt (across * across + depth * depth);
  else if (depth < 0.0)
    return FALSE;

  *count = hyscan_projector_coord_to_count_internal (self, across);
  return TRUE;
}
