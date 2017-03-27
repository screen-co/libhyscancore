/*
 * \file hyscan-track-rect.c
 *
 * \brief Исходный файл класса определения геометрических параметров галса.
 * \author Dmitriev Alexander (m1n7@yandex.ru)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-track-rect.h"
#include <hyscan-acoustic-data.h>
#include <hyscan-depth-nmea.h>
#include <hyscan-depthometer.h>
#include <math.h>
#include <string.h>

typedef struct
{
  /* БД, проект, галс. */
  HyScanDB               *db;                    /* Интерфейс базы данных. */
  gchar                  *project;               /* Название проекта. */
  gchar                  *track;                 /* Название галса. */
  HyScanSourceType        source;                /* Источник данных. */
  gboolean                raw;                   /* Использовать сырые данные. */

  HyScanTileType          type;                  /* Тип тайлов. */
  /* Кэш. */
  HyScanCache            *cache;                 /* Интерфейс системы кэширования. */
  gchar                  *prefix;                /* Префикс ключа кэширования. */

  /* Определение глубины. */
  HyScanSourceType        depth_source;          /* Источник данных. */
  guint                   depth_channel;         /* Номер канала. */
  gulong                  depth_time;            /* Время округления. */
  gint                    depth_size;            /* Размер фильтра. */

  gfloat                  ship_speed;            /* Скорость движения. */

  GArray                 *sound_velocity;        /* Скорость звука. */
  gfloat                  sound_velocity1;       /* Скорость звука для тех, кто не умеет в профиль скорости звука. */

  gboolean                track_changed;         /* Флаг на смену галса. */
  gboolean                type_changed;          /* Флаг на смену типа тайлов. */
  gboolean                cache_changed;         /* Флаг на смену кэша. */
  gboolean                depth_source_changed;  /* Флаг на смену источника данных глубины. */
  gboolean                depth_time_changed;    /* Флаг на смену временного окна данных глубины. */
  gboolean                depth_size_changed;    /* Флаг на смену размера фильтра. */
  gboolean                speed_changed;         /* Флаг на смену скорости движения. */
  gboolean                velocity_changed;      /* Флаг на смену скорости звука. */
} HyScanTrackRectState;

struct _HyScanTrackRectPrivate
{
  GThread                *watcher;       /* Поток слежения за КД. */
  gint                    stop;          /* Флаг остановки. */
  gint                    have_data;     /* Флаг наличия данных. */
  gulong                  user_delay;    /* Интервал опроса БД. */
  gulong                  real_delay;    /* Интервал опроса БД. */
  GMutex                  lock;          /* Блокировка множественного доступа. */
  GCond                   cond;          /* Кондишн для остановки потока. */
  gboolean                abort;         /* Прекратить обработку текущего галса. */

  gdouble                 width;         /* Ширина КД. */
  gdouble                 length;        /* Длина КД. */
  gboolean                writeable;     /* Возможность дозаписи. */

  HyScanTrackRectState    cur_state;
  HyScanTrackRectState    new_state;
  gboolean                state_changed;
  GMutex                  state_lock;

  HyScanAcousticData     *dc;
  HyScanDepthometer      *depth;
  HyScanDepth            *idepth;
};

static void      hyscan_track_rect_object_constructed  (GObject    *object);
static void      hyscan_track_rect_object_finalize     (GObject    *object);

static HyScanAcousticData *hyscan_track_rect_open_dc          (HyScanTrackRect *self);
static HyScanDepth        *hyscan_track_rect_open_depth       (HyScanTrackRect *self);
static HyScanDepthometer  *hyscan_track_rect_open_depthometer (HyScanTrackRect *self);
static gboolean  hyscan_track_rect_sync_states         (HyScanTrackRect *self);
static gboolean  hyscan_track_rect_apply_updates       (HyScanTrackRect *self);
static gpointer  hyscan_track_rect_watcher             (gpointer    data);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanTrackRect, hyscan_track_rect, G_TYPE_OBJECT);

static void
hyscan_track_rect_class_init (HyScanTrackRectClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = hyscan_track_rect_object_constructed;
  object_class->finalize = hyscan_track_rect_object_finalize;
}

static void
hyscan_track_rect_init (HyScanTrackRect *self)
{
  self->priv = hyscan_track_rect_get_instance_private (self);
}

static void
hyscan_track_rect_object_constructed (GObject *object)
{
  HyScanTrackRect *self = HYSCAN_TRACK_RECT (object);
  HyScanTrackRectPrivate *priv = self->priv;

  g_mutex_init (&priv->lock);
  g_mutex_init (&priv->state_lock);
  g_cond_init (&priv->cond);

  //////// via setters priv->type = HYSCAN_TILE_SLANT;
  //////// via setters priv->writeable = TRUE;

  //////// via setters priv->ship_speed = 1.0;
  //////// via setters priv->sound_velocity1 = 1500.0 / 2.0;

  priv->user_delay = 250 * G_TIME_SPAN_MILLISECOND;
  priv->stop = 0;
  priv->watcher = g_thread_new ("trkrect-watcher", hyscan_track_rect_watcher, self);
}

static void
hyscan_track_rect_object_finalize (GObject *object)
{
  HyScanTrackRect *self = HYSCAN_TRACK_RECT (object);
  HyScanTrackRectPrivate *priv = self->priv;

  g_atomic_int_set (&priv->stop, TRUE);
  g_atomic_int_set (&priv->abort, TRUE);
  g_clear_pointer (&priv->watcher, g_thread_join);

  g_mutex_clear (&priv->lock);
  g_mutex_clear (&priv->state_lock);
  g_cond_clear (&priv->cond);

  g_array_unref (priv->cur_state.sound_velocity);
  g_clear_object (&priv->cur_state.db);
  g_clear_object (&priv->cur_state.cache);
  g_free (priv->cur_state.project);
  g_free (priv->cur_state.track);
  g_free (priv->cur_state.prefix);

  g_array_unref (priv->new_state.sound_velocity);
  g_clear_object (&priv->new_state.db);
  g_clear_object (&priv->new_state.cache);
  g_free (priv->new_state.project);
  g_free (priv->new_state.track);
  g_free (priv->new_state.prefix);

  G_OBJECT_CLASS (hyscan_track_rect_parent_class)->finalize (object);
}

static HyScanAcousticData*
hyscan_track_rect_open_dc (HyScanTrackRect *self)
{
  HyScanTrackRectState *state = &self->priv->cur_state;
  HyScanAcousticData *dc;

  if (state->db == NULL || state->project == NULL || state->track == NULL)
    return NULL;

  dc = hyscan_acoustic_data_new (state->db, state->project, state->track, state->source, state->raw);

  if (dc == NULL)
    return NULL;

  hyscan_acoustic_data_set_cache (dc, state->cache, state->prefix);

  return dc;
}

static HyScanDepth*
hyscan_track_rect_open_depth (HyScanTrackRect *self)
{
  HyScanTrackRectPrivate *priv = self->priv;
  HyScanDepth *idepth = NULL;

  if (hyscan_source_is_acoustic (priv->cur_state.depth_source))
    {
      HyScanDepthAcoustic *dacoustic;
      dacoustic = hyscan_depth_acoustic_new (priv->cur_state.db,
                                             priv->cur_state.project,
                                             priv->cur_state.track,
                                             priv->cur_state.depth_source,
                                             priv->cur_state.raw);

      if (dacoustic != NULL)
        hyscan_depth_acoustic_set_sound_velocity (dacoustic, priv->cur_state.sound_velocity);

      idepth = HYSCAN_DEPTH (dacoustic);
    }

  else if (HYSCAN_SOURCE_NMEA_DPT == priv->cur_state.depth_source)
    {
      HyScanDepthNMEA *dnmea;
      dnmea = hyscan_depth_nmea_new (priv->cur_state.db,
                                     priv->cur_state.project,
                                     priv->cur_state.track,
                                     priv->cur_state.depth_channel);
      idepth = HYSCAN_DEPTH (dnmea);
    }

  hyscan_depth_set_cache (idepth, priv->cur_state.cache, priv->cur_state.prefix);
  return idepth;
}

static HyScanDepthometer*
hyscan_track_rect_open_depthometer (HyScanTrackRect *self)
{
  HyScanTrackRectPrivate *priv = self->priv;
  HyScanDepthometer *depth = NULL;

  depth = hyscan_depthometer_new (priv->idepth);
  if (depth == NULL)
    return NULL;

  hyscan_depthometer_set_cache (depth, priv->cur_state.cache, priv->cur_state.prefix);
  hyscan_depthometer_set_filter_size (depth, priv->cur_state.depth_size);
  hyscan_depthometer_set_time_precision (depth, priv->cur_state.depth_time);

  return depth;
}

static gboolean
hyscan_track_rect_sync_states (HyScanTrackRect *self)
{
  HyScanTrackRectState *new = &self->priv->new_state;
  HyScanTrackRectState *cur = &self->priv->cur_state;

  if (new->track_changed)
    {
      /* Очищаем текущее. */
      g_clear_object (&cur->db);
      g_clear_pointer (&cur->project, g_free);
      g_clear_pointer (&cur->track, g_free);

      /* Копируем из нового. */
      cur->db = g_object_ref (new->db);
      cur->project = g_strdup (new->project);
      cur->track = g_strdup (new->track);
      cur->source = new->source;
      cur->raw = new->raw;

      /* Выставляем флаги. */
      new->track_changed = FALSE;
      cur->track_changed = TRUE;
    }
  if (new->cache_changed)
    {
      g_clear_object (&cur->cache);
      g_clear_pointer (&cur->prefix, g_free);

      cur->cache = g_object_ref (new->cache);
      cur->prefix = g_strdup (new->prefix);

      new->cache_changed = FALSE;
      cur->cache_changed = TRUE;
    }
  if (new->depth_source_changed)
    {
      cur->depth_source = new->depth_source;
      cur->depth_channel = new->depth_channel;

      new->depth_source_changed = FALSE;
      cur->depth_source_changed = TRUE;
    }
  if (new->depth_time_changed)
    {
      cur->depth_time = new->depth_time;
      cur->depth_size = new->depth_size;

      new->depth_time_changed = FALSE;
      cur->depth_time_changed = TRUE;
    }
  if (new->depth_size_changed)
    {
      cur->depth_time = new->depth_time;
      cur->depth_size = new->depth_size;

      new->depth_size_changed = FALSE;
      cur->depth_size_changed = TRUE;
    }
  if (new->speed_changed)
    {
      cur->ship_speed = new->ship_speed;

      cur->speed_changed = TRUE;
      new->speed_changed = FALSE;
    }
  if (new->velocity_changed)
    {
      if (cur->sound_velocity != NULL)
        g_clear_pointer (&cur->sound_velocity, g_array_unref);

      cur->sound_velocity = g_array_ref (new->sound_velocity);
      cur->sound_velocity1 = new->sound_velocity1;

      cur->velocity_changed = TRUE;
      new->velocity_changed = FALSE;
    }

  return TRUE;
}

static gboolean
hyscan_track_rect_apply_updates (HyScanTrackRect *self)
{
  HyScanTrackRectPrivate *priv = self->priv;
  HyScanTrackRectState *state = &priv->cur_state;

  if (state->track_changed || state->type_changed)
    {
      g_clear_object (&priv->dc);
      g_clear_object (&priv->depth);
      g_clear_object (&priv->idepth);
      state->track_changed = FALSE;
      state->type_changed = FALSE;
    }
  else if (state->depth_source_changed)
    {
      g_clear_object (&priv->depth);
      state->depth_source_changed = FALSE;
    }
  else if (state->depth_time_changed || state->depth_size_changed)
    {
      if (priv->depth != NULL)
        {
          hyscan_depthometer_set_time_precision (priv->depth, state->depth_time);
          hyscan_depthometer_set_filter_size (priv->depth, state->depth_size);
        }
      state->depth_time_changed = FALSE;
      state->depth_size_changed = FALSE;
    }
  else if (state->cache_changed)
    {
      if (priv->dc != NULL)
        hyscan_acoustic_data_set_cache (priv->dc, state->cache, state->prefix);
      if (priv->depth != NULL)
        hyscan_depthometer_set_cache (priv->depth, state->cache, state->prefix);
      if (priv->idepth != NULL)
        hyscan_depth_set_cache (priv->idepth, state->cache, state->prefix);

      state->cache_changed = FALSE;
      if (!state->speed_changed && !state->velocity_changed && !state->type_changed)
        return FALSE;
    }

  if (state->velocity_changed)
    {
      if (HYSCAN_IS_DEPTH_ACOUSTIC (priv->idepth))
        {
          HyScanDepthAcoustic *da = HYSCAN_DEPTH_ACOUSTIC (priv->idepth);
          hyscan_depth_acoustic_set_sound_velocity (da, state->sound_velocity);
        }
      state->velocity_changed = FALSE;
    }

  g_clear_object (&priv->dc);
  g_clear_object (&priv->depth);
  g_clear_object (&priv->idepth);
  return TRUE;
}

/* Поток обработки информации. */
static gpointer
hyscan_track_rect_watcher (gpointer data)
{
  HyScanTrackRect *self = data;
  HyScanTrackRectPrivate *priv = self->priv;

  gdouble dpt;
  HyScanAcousticDataInfo dc_info;

  guint32  values_count = 0;
  guint32  first_index  = 0;
  guint32  last_index   = 0;
  gint64   first_time   = 0;
  gint64   last_time    = 0;
  gint64   itime        = 0;
  gdouble  length       = 0.0;
  gdouble  width        = 0.0;
  gdouble  width_max    = 0.0;
  guint32  i;
  gboolean init         = FALSE;
  gboolean writeable    = TRUE;
  gint64 end_time;

  gboolean restart = FALSE;

  HyScanAcousticData *dc = NULL;
  HyScanDepthometer *depth = NULL;
  GMutex cond_lock;
  g_mutex_init (&cond_lock);

  /* Параметры каналов данных. */
  while (g_atomic_int_get(&priv->stop) == 0)
    {
      if (g_atomic_int_get(&priv->state_changed))
        {
          g_mutex_lock (&priv->state_lock);
          hyscan_track_rect_sync_states (self);
          g_atomic_int_set (&priv->state_changed, FALSE);
          g_atomic_int_set (&priv->abort, FALSE);
          g_mutex_unlock (&priv->state_lock);

          restart = hyscan_track_rect_apply_updates (self);
          if (restart)
            init = FALSE;
        }

      /* Открываем КД. */
      if (priv->dc == NULL)
        {
          priv->dc = dc = hyscan_track_rect_open_dc (self);
        }
      /* И ещё надо открыть глубину. */
      if (priv->cur_state.type == HYSCAN_TILE_GROUND && priv->depth == NULL)
        {
          if (priv->idepth == NULL)
            priv->idepth = hyscan_track_rect_open_depth (self);

          priv->depth = depth = hyscan_track_rect_open_depthometer (self);
        }

      /* После открытия КД нужно дождаться, пока в них хоть что-то появится. */
      if (!init && dc != NULL)
        {
          dc_info = hyscan_acoustic_data_get_info (dc);
          init = hyscan_acoustic_data_get_range (dc, &i, &last_index);
        }

      /* Нужно пройтись по всем строкам и найти максимальную ширину левого борта,
       * правого борта и общую длину. */
      if (!init)
        {
          goto next;
        }
      else if (hyscan_acoustic_data_get_range (dc, &first_index, &last_index))
        {
          /* Ширина. */
          for (; i <= last_index; i++)
            {
              if (g_atomic_int_get (&priv->abort))
                goto next;

              hyscan_acoustic_data_get_values (dc, i, &values_count, &itime);
              width = values_count * priv->cur_state.sound_velocity1 / dc_info.data.rate;
              if (priv->cur_state.type == HYSCAN_TILE_GROUND && depth != NULL)
                {
                  dpt = hyscan_depthometer_get (depth, itime);
                  width = sqrt (width * width - dpt * dpt);
                }

              if (width > width_max)
                width_max = width;
            }
          /* Длина. */
          hyscan_acoustic_data_get_values (dc, first_index, &values_count, &first_time);
          hyscan_acoustic_data_get_values (dc, last_index, &values_count, &last_time);
          length = priv->cur_state.ship_speed * (last_time - first_time) / 1000000.0;
        }

      /* Проверяем режим доступа к КД. */
      writeable = (init) ? hyscan_acoustic_data_is_writable (dc) : TRUE;

      /* Записываем параметры галса. */
      g_mutex_lock (&priv->lock);

      priv->width = width_max;
      priv->length = length;
      priv->writeable = writeable;

      if (init)
        priv->have_data = TRUE;

      g_mutex_unlock (&priv->lock);


next:
      end_time = g_get_monotonic_time () + 250 * G_TIME_SPAN_MILLISECOND;

      g_mutex_lock (&cond_lock);
      if (!g_atomic_int_get(&priv->abort))
        g_cond_wait_until (&priv->cond, &cond_lock, end_time);
      g_mutex_unlock (&cond_lock);

    }

  g_mutex_clear (&cond_lock);
  return NULL;
}

/* Фукнция создает новый объект HyScanTrackRect. */
HyScanTrackRect*
hyscan_track_rect_new (void)
{
  return g_object_new (HYSCAN_TYPE_TRACK_RECT, NULL);
}

/* Функция устанавливает скорость. */
void
hyscan_track_rect_set_ship_speed (HyScanTrackRect *self,
                                 gfloat           ship)
{
  HyScanTrackRectPrivate *priv;

  g_return_if_fail (HYSCAN_IS_TRACK_RECT (self));
  priv = self->priv;

  g_mutex_lock (&priv->state_lock);

  priv->new_state.ship_speed = ship;
  priv->new_state.speed_changed = TRUE;
  priv->state_changed = TRUE;

  g_mutex_unlock (&priv->state_lock);
}

/* Функция устанавливает скорость. */
void
hyscan_track_rect_set_sound_velocity (HyScanTrackRect *self,
                                      GArray          *sound)
{
  HyScanTrackRectPrivate *priv;
  HyScanSoundVelocity *link;

  g_return_if_fail (HYSCAN_IS_TRACK_RECT (self));
  priv = self->priv;

  g_mutex_lock (&priv->state_lock);

  if (priv->new_state.sound_velocity != NULL)
    g_clear_pointer (&priv->new_state.sound_velocity, g_array_unref);

  if (sound == NULL || sound->len == 0)
    {
      HyScanSoundVelocity sv = {.depth = 0.0, .velocity = 1500.0};
      priv->new_state.sound_velocity = g_array_new (FALSE, FALSE, sizeof (HyScanSoundVelocity));
      g_array_append_val (priv->new_state.sound_velocity, sv);
    }
  else
    {
      priv->new_state.sound_velocity = g_array_ref (sound);
    }

  link = &g_array_index (priv->new_state.sound_velocity, HyScanSoundVelocity, 0);
  priv->new_state.sound_velocity1 = link->velocity / 2.0;

  priv->new_state.velocity_changed = TRUE;
  priv->state_changed = TRUE;

  g_atomic_int_set (&priv->abort, TRUE); 
  g_cond_signal (&priv->cond);

  g_mutex_unlock (&priv->state_lock);
}

/* Функция устанавливает кэш.*/
void
hyscan_track_rect_set_cache (HyScanTrackRect *self,
                            HyScanCache     *cache,
                            const gchar     *prefix)
{
  HyScanTrackRectPrivate *priv;

  g_return_if_fail (HYSCAN_IS_TRACK_RECT (self));
  priv = self->priv;

  g_mutex_lock (&priv->state_lock);

  g_clear_object (&priv->new_state.cache);
  g_free (priv->new_state.prefix);

  priv->new_state.cache = (cache != NULL) ? g_object_ref (cache) : NULL;
  priv->new_state.prefix = (prefix != NULL) ? g_strdup (prefix) : NULL;
  priv->new_state.cache_changed = TRUE;

  g_atomic_int_set (&priv->abort, TRUE);
  g_cond_signal (&priv->cond);

  g_mutex_unlock (&priv->state_lock);
}
/* Функция настраивает объект измерения глубины. */

void
hyscan_track_rect_set_depth_source (HyScanTrackRect   *self,
                                    HyScanSourceType   source,
                                    guint              channel)
{
  HyScanTrackRectPrivate *priv;

  g_return_if_fail (HYSCAN_IS_TRACK_RECT (self));
  priv = self->priv;

  g_mutex_lock (&priv->state_lock);

  priv->new_state.depth_source = source;
  priv->new_state.depth_channel = channel;
  priv->new_state.depth_source_changed = TRUE;
  priv->state_changed = TRUE;

  g_atomic_int_set (&priv->abort, TRUE);
  g_cond_signal (&priv->cond);

  g_mutex_unlock (&priv->state_lock);
}

void
hyscan_track_rect_set_depth_filter_size (HyScanTrackRect   *self,
                                        gint               size)
{
  HyScanTrackRectPrivate *priv;

  g_return_if_fail (HYSCAN_IS_TRACK_RECT (self));
  priv = self->priv;

  g_mutex_lock (&priv->state_lock);

  priv->new_state.depth_size = size;
  priv->new_state.depth_size_changed = TRUE;
  priv->state_changed = TRUE;

  g_atomic_int_set (&priv->abort, TRUE);
  g_cond_signal (&priv->cond);

  g_mutex_unlock (&priv->state_lock);
}

void
hyscan_track_rect_set_depth_time (HyScanTrackRect   *self,
                                  gulong             usecs)
{
  HyScanTrackRectPrivate *priv;

  g_return_if_fail (HYSCAN_IS_TRACK_RECT (self));
  priv = self->priv;

  g_mutex_lock (&priv->state_lock);

  priv->new_state.depth_time = usecs;
  priv->new_state.depth_time_changed = TRUE;
  priv->state_changed = TRUE;

  g_atomic_int_set (&priv->abort, TRUE);
  g_cond_signal (&priv->cond);

  g_mutex_unlock (&priv->state_lock);
}

/* Функция выбора наклонной или горизонтальной дальности. */
void
hyscan_track_rect_set_type (HyScanTrackRect *self,
                            HyScanTileType   type)
{
  HyScanTrackRectPrivate *priv;

  g_return_if_fail (HYSCAN_IS_TRACK_RECT (self));
  priv = self->priv;

  g_mutex_lock (&priv->state_lock);

  priv->new_state.type = type;
  priv->new_state.type_changed = TRUE;
  priv->state_changed = TRUE;

  g_atomic_int_set (&priv->abort, TRUE);
  g_cond_signal (&priv->cond);

  g_mutex_unlock (&priv->state_lock);
}

/* Функция открывает галс. */
void
hyscan_track_rect_open (HyScanTrackRect *self,
                        HyScanDB        *db,
                        const gchar     *project,
                        const gchar     *track,
                        HyScanSourceType source,
                        gboolean         raw)
{
  HyScanTrackRectPrivate *priv;

  g_return_if_fail (HYSCAN_IS_TRACK_RECT (self));
  priv = self->priv;

  g_mutex_lock (&priv->state_lock);
  g_clear_object (&priv->new_state.db);
  g_clear_pointer (&priv->new_state.project, g_free);
  g_clear_pointer (&priv->new_state.track, g_free);

  priv->new_state.db = g_object_ref (db);
  priv->new_state.project = g_strdup (project);
  priv->new_state.track = g_strdup (track);
  priv->new_state.source = source;
  priv->new_state.raw = raw;

  priv->have_data = FALSE;

  priv->width = priv->length = 0.0;
  priv->real_delay = priv->user_delay = 1000 * G_TIME_SPAN_MILLISECOND;

  priv->new_state.track_changed = TRUE;
  priv->state_changed = TRUE;

  g_atomic_int_set (&priv->abort, TRUE);
  g_cond_signal (&priv->cond);

  g_mutex_unlock (&priv->state_lock);
}

/* Функция отдает параметры галса. */
gboolean
hyscan_track_rect_get (HyScanTrackRect *self,
                      gboolean        *writeable,
                      gdouble         *width,
                      gdouble         *length)
{
  HyScanTrackRectPrivate *priv;
  g_return_val_if_fail (HYSCAN_IS_TRACK_RECT (self), TRUE);
  priv = self->priv;

  g_mutex_lock (&priv->lock);

  if (!priv->have_data)
    {
      g_mutex_unlock (&priv->lock);
      return FALSE;
    }

  if (width != NULL)
    *width = priv->width;
  if (length != NULL)
    *length = priv->length;
  if (writeable != NULL)
    *writeable = priv->writeable;

  g_mutex_unlock (&priv->lock);

  return TRUE;
}
