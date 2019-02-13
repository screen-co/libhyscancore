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
#include <hyscan-projector.h>

enum
{
  PROP_CACHE = 1,
  PROP_AMPLITUDE_FACTORY,
  PROP_DEPTH_FACTORY
};

typedef struct
{
  /* БД, проект, галс. */
  HyScanTileFlags         flags;                 /* Флаги генерации. */
  HyScanSourceType        source;                /* Источник данных. */

  gfloat                  ship_speed;            /* Скорость движения. */
  GArray                 *sound_velocity;        /* Скорость звука. */
  gfloat                  sound_velocity1;       /* Скорость звука для тех, кто не умеет в профиль скорости звука. */

  gboolean                amp_changed;           /* Флаг на смену AmplitudeFactory. */
  gboolean                dpt_changed;           /* Флаг на смену DepthFactory. */
  gboolean                source_changed;        /* Флаг на смену типа источника. */
  gboolean                flags_changed;         /* Флаг на смену, хм, флагов. */
  gboolean                speed_changed;         /* Флаг на смену скорости движения. */
  gboolean                velocity_changed;      /* Флаг на смену скорости звука. */
} HyScanTrackRectState;

struct _HyScanTrackRectPrivate
{
  /* Кэш. */
  HyScanCache            *cache;         /* Интерфейс системы кэширования. */
  HyScanAmplitudeFactory *af;
  HyScanDepthFactory     *df;

  GThread                *watcher;       /* Поток слежения за КД. */
  gint                    stop;          /* Флаг остановки. */
  gint                    have_data;     /* Флаг наличия данных. */
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

  HyScanAmplitude        *dc;
  HyScanProjector        *pj;
  HyScanDepthometer      *depth;
};

static void      hyscan_track_rect_set_property       (GObject                    *object,
                                                       guint                       prop_id,
                                                       const GValue               *value,
                                                       GParamSpec                 *pspec);

static void      hyscan_track_rect_object_constructed  (GObject    *object);
static void      hyscan_track_rect_object_finalize     (GObject    *object);

static HyScanProjector   *hyscan_track_rect_open_projector   (HyScanTrackRectState *state,
                                                              HyScanAmplitude      *dc);
static gboolean  hyscan_track_rect_sync_states         (HyScanTrackRect *self);
static gboolean  hyscan_track_rect_apply_updates       (HyScanTrackRect *self);
static gpointer  hyscan_track_rect_watcher             (gpointer    data);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanTrackRect, hyscan_track_rect, G_TYPE_OBJECT);

static void
hyscan_track_rect_class_init (HyScanTrackRectClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_track_rect_set_property;
  object_class->constructed = hyscan_track_rect_object_constructed;
  object_class->finalize = hyscan_track_rect_object_finalize;

  g_object_class_install_property (object_class, PROP_CACHE,
    g_param_spec_object ("cache", "Cache", "HyScanCache interface",
                         HYSCAN_TYPE_CACHE,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_AMPLITUDE_FACTORY,
    g_param_spec_object ("amp-factory", "AmplitudeFactory",
                         "HyScanAmplitudeFactory",
                         HYSCAN_TYPE_AMPLITUDE_FACTORY,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_DEPTH_FACTORY,
    g_param_spec_object ("dpt-factory", "DepthFactory",
                         "HyScanDepthFactory",
                         HYSCAN_TYPE_DEPTH_FACTORY,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_track_rect_init (HyScanTrackRect *self)
{
  self->priv = hyscan_track_rect_get_instance_private (self);
}

static void
hyscan_track_rect_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  HyScanTrackRect *self = HYSCAN_TRACK_RECT (object);
  HyScanTrackRectPrivate *priv = self->priv;

  if (prop_id == PROP_CACHE)
    priv->cache = g_value_dup_object (value);
  else if (prop_id == PROP_AMPLITUDE_FACTORY)
    priv->af = g_value_dup_object (value);
  else if (prop_id == PROP_DEPTH_FACTORY)
    priv->df = g_value_dup_object (value);
  else
    G_OBJECT_WARN_INVALID_PROPERTY_ID (self, prop_id, pspec);
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
  g_array_unref (priv->new_state.sound_velocity);

  G_OBJECT_CLASS (hyscan_track_rect_parent_class)->finalize (object);
}

static HyScanProjector*
hyscan_track_rect_open_projector (HyScanTrackRectState *state,
                                  HyScanAmplitude      *dc)
{
  HyScanProjector *pj = NULL;

  if (dc == NULL)
    return NULL;

  pj = hyscan_projector_new (dc);
  if (pj == NULL)
    return NULL;

  hyscan_projector_set_ship_speed (pj, state->ship_speed);
  hyscan_projector_set_sound_velocity (pj, state->sound_velocity);

  return pj;
}


static gboolean
hyscan_track_rect_sync_states (HyScanTrackRect *self)
{
  HyScanTrackRectState *new_st = &self->priv->new_state;
  HyScanTrackRectState *cur_st = &self->priv->cur_state;

  if (new_st->amp_changed)
    {
      new_st->amp_changed = FALSE;
      cur_st->amp_changed = TRUE;
    }
  if (new_st->dpt_changed)
    {
      new_st->dpt_changed = FALSE;
      cur_st->dpt_changed = TRUE;
    }
  if (new_st->source_changed)
    {
      cur_st->source = new_st->source;
      new_st->source_changed = FALSE;
      cur_st->source_changed = TRUE;
    }
  if (new_st->speed_changed)
    {
      cur_st->ship_speed = new_st->ship_speed;

      cur_st->speed_changed = TRUE;
      new_st->speed_changed = FALSE;
    }
  if (new_st->velocity_changed)
    {
      if (cur_st->sound_velocity != NULL)
        g_clear_pointer (&cur_st->sound_velocity, g_array_unref);

      cur_st->sound_velocity = g_array_ref (new_st->sound_velocity);
      cur_st->sound_velocity1 = new_st->sound_velocity1;

      cur_st->velocity_changed = TRUE;
      new_st->velocity_changed = FALSE;
    }

  return TRUE;
}

static gboolean
hyscan_track_rect_apply_updates (HyScanTrackRect *self)
{
  HyScanTrackRectPrivate *priv = self->priv;
  HyScanTrackRectState *state = &priv->cur_state;

  if (state->amp_changed || state->flags_changed || state->source_changed)
    {
      g_clear_object (&priv->dc);
      g_clear_object (&priv->pj);
      state->amp_changed = FALSE;
      state->flags_changed = FALSE;
      state->source_changed = FALSE;
    }
  else if (state->dpt_changed)
    {
      g_clear_object (&priv->depth);
      state->dpt_changed = FALSE;
    }

  if (state->velocity_changed)
    {
      if (priv->pj != NULL)
        hyscan_projector_set_sound_velocity (priv->pj, state->sound_velocity);

      state->velocity_changed = FALSE;
    }
  if (state->speed_changed)
    {
      if (priv->pj != NULL)
        hyscan_projector_set_ship_speed (priv->pj, state->ship_speed);
      state->speed_changed = FALSE;
    }

  return TRUE;
}

/* Поток обработки информации. */
static gpointer
hyscan_track_rect_watcher (gpointer data)
{
  HyScanTrackRect *self = data;
  HyScanTrackRectPrivate *priv = self->priv;

  guint32  nvals     = 0;
  guint32  index0    = 0;
  guint32  index1    = 0;
  gint64   time0     = 0;
  gint64   time1     = 0;
  gint64   itime     = 0;
  gdouble  length    = 0.0;
  gdouble  width_max = 0.0;
  gboolean init      = FALSE;
  gboolean writeable = TRUE;
  gint64   end_time;
  gdouble  dpt;
  guint32  i;

  gdouble along0, along1;
  gdouble across;

  HyScanProjector *pj = NULL;
  HyScanAmplitude *dc = NULL;
  HyScanDepthometer *depth = NULL;
  GMutex cond_lock;
  g_mutex_init (&cond_lock);

  /* Параметры каналов данных. */
  while (g_atomic_int_get(&priv->stop) == 0)
    {
      if (g_atomic_int_get (&priv->state_changed))
        {
          g_mutex_lock (&priv->state_lock);
          hyscan_track_rect_sync_states (self);
          g_atomic_int_set (&priv->state_changed, FALSE);
          g_atomic_int_set (&priv->abort, FALSE);
          g_mutex_unlock (&priv->state_lock);

          if (hyscan_track_rect_apply_updates (self))
            {
              init = FALSE;
              width_max = across = along0 = along1 = 0.0;
            }
        }

      /* Открываем КД. */
      if (priv->pj == NULL)
        {
          if (priv->dc == NULL)
            priv->dc = dc = hyscan_amplitude_factory_produce (priv->af, priv->cur_state.source);
          priv->pj = pj = hyscan_track_rect_open_projector (&priv->cur_state, dc);

          if (pj == NULL)
            {
              g_usleep (10);
              continue;
            }
        }

      /* И ещё надо открыть глубину. */
      if (priv->cur_state.flags & HYSCAN_TILE_GROUND && priv->depth == NULL)
        {
          priv->depth = depth = hyscan_depth_factory_produce (priv->df);
        }

      /* После открытия КД нужно дождаться, пока в них хоть что-то появится. */
      if (!init)
        {
          if (hyscan_amplitude_get_range (dc, &i, &index1))
            init = TRUE;
          else
            goto next;
        }

      /* Проходим по всем строкам и найти максимальную ширину и общую длину. */
      if (hyscan_amplitude_get_range (dc, &index0, &index1))
        {
          /* Ширина. */
          for (; i <= index1; i++)
            {
              if (g_atomic_int_get (&priv->abort))
                goto next;

              /* Узнаем количество точек в строке. */
              hyscan_amplitude_get_size_time (dc, i, &nvals, &itime);

              /* Определяем глубину. */
              if (priv->cur_state.flags & HYSCAN_TILE_GROUND && depth != NULL)
                dpt = hyscan_depthometer_get (depth, itime);
              else
                dpt = 0.0;

              /* Переводим. */
              hyscan_projector_count_to_coord (pj, nvals, &across, dpt);

              width_max = MAX (across, width_max);
            }

          /* Длина. */
          hyscan_amplitude_get_size_time (dc, index0, NULL, &time0);
          hyscan_amplitude_get_size_time (dc, index1, NULL, &time1);

          hyscan_projector_index_to_coord (pj, index0, &along0);
          hyscan_projector_index_to_coord (pj, index1, &along1);
          length = along1 - along0;
        }

      /* Проверяем режим доступа к КД. */
      writeable = (init) ? hyscan_amplitude_is_writable (dc) : TRUE;

      /* Записываем параметры галса. */
      g_mutex_lock (&priv->lock);

      priv->width = width_max;
      priv->length = length;
      priv->writeable = writeable;

      /* Задаем этот флаг только если никаких изменений не было. */
      if (init & !priv->state_changed)
        priv->have_data = TRUE;

      g_mutex_unlock (&priv->lock);

next:
      end_time = g_get_monotonic_time () + 250 * G_TIME_SPAN_MILLISECOND;

      g_mutex_lock (&cond_lock);
      if (!g_atomic_int_get(&priv->abort))
        g_cond_wait_until (&priv->cond, &cond_lock, end_time);
      g_mutex_unlock (&cond_lock);
    }

  g_clear_object (&priv->pj);
  g_clear_object (&priv->depth);

  g_mutex_clear (&cond_lock);

  return NULL;
}

/* Фукнция создает новый объект HyScanTrackRect. */
HyScanTrackRect*
hyscan_track_rect_new (HyScanCache            *cache,
                       HyScanAmplitudeFactory *amp_factory,
                       HyScanDepthFactory     *dpt_factory)
{
  return g_object_new (HYSCAN_TYPE_TRACK_RECT,
                       "cache", cache,
                       "amp-factory", amp_factory,
                       "dpt-factory", dpt_factory,
                       NULL);
}

void
hyscan_track_rect_amp_changed (HyScanTrackRect *self)
{
  HyScanTrackRectPrivate *priv;

  g_return_if_fail (HYSCAN_IS_TRACK_RECT (self));
  priv = self->priv;

  g_mutex_lock (&priv->state_lock);

  priv->new_state.amp_changed = TRUE;
  priv->state_changed = TRUE;
  priv->have_data = FALSE;

  g_mutex_unlock (&priv->state_lock);
}

void
hyscan_track_rect_dpt_changed (HyScanTrackRect *self)
{
  HyScanTrackRectPrivate *priv;

  g_return_if_fail (HYSCAN_IS_TRACK_RECT (self));
  priv = self->priv;

  g_mutex_lock (&priv->state_lock);

  priv->new_state.dpt_changed = TRUE;
  priv->state_changed = TRUE;
  priv->have_data = FALSE;

  g_mutex_unlock (&priv->state_lock);
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

/* Функция выбора наклонной или горизонтальной дальности. */
void
hyscan_track_rect_set_type (HyScanTrackRect *self,
                            HyScanTileFlags  flags)
{
  HyScanTrackRectPrivate *priv;

  g_return_if_fail (HYSCAN_IS_TRACK_RECT (self));
  priv = self->priv;

  g_mutex_lock (&priv->state_lock);

  priv->new_state.flags = flags;
  priv->new_state.flags_changed = TRUE;
  priv->state_changed = TRUE;

  g_atomic_int_set (&priv->abort, TRUE);
  g_cond_signal (&priv->cond);

  g_mutex_unlock (&priv->state_lock);
}

void
hyscan_track_rect_set_source (HyScanTrackRect  *self,
                              HyScanSourceType  source)
{
  HyScanTrackRectPrivate *priv;

  g_return_if_fail (HYSCAN_IS_TRACK_RECT (self));
  priv = self->priv;

  g_mutex_lock (&priv->state_lock);
  priv->new_state.source = source;
  priv->new_state.source_changed = TRUE;
  priv->have_data = FALSE;
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
