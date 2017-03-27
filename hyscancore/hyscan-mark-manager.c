#include "hyscan-mark-manager.h"

#define DELAY (1 * G_TIME_SPAN_SECOND)

enum
{
  PROP_DB = 1,
};

enum
{
  MARK_ADD    = 1001,
  MARK_MODIFY = 1002,
  MARK_REMOVE = 1003
};

enum
{
  SIGNAL_CHANGED,
  SIGNAL_LAST
};

typedef struct
{
  gchar               *id;
  HyScanWaterfallMark *mark;
  gint                 action;
} HyScanMarkManagerTask;

typedef struct
{
  gchar                   *project;          /* Проект. */
  gchar                   *track;            /* Галс. */
  gboolean                 track_changed;

  gchar                   *profile;          /* Имя профиля. */
  gboolean                 profile_changed;
} HyScanMarkManagerState;

struct _HyScanMarkManagerPrivate
{
  HyScanDB                *db;               /* БД. */

  HyScanMarkManagerState   new_state;
  HyScanMarkManagerState   cur_state;
  gint                     state_changed1;   /* Флаг смены галса. */
  GMutex                   state_lock;       /* Блокировка названия проекта и галса. */

  GThread                 *processing;       /* Поток обработки. */
  gint                     stop;             /* Флаг остановки. */
  GSList                  *tasks;            /* Список заданий. */
  GMutex                   tasks_lock;       /* Блокировка списка заданий. */

  GCond                    im_cond;          /* Триггер отправки изменений в БД. */
  gint                     im_flag;          /* Наличие изменений в БД. */

  guint                    alerter;          /* Идентификатор обработчика сигнализирующего об изменениях. */
  gint                     marks_changed;    /* Флаг, сигнализирующий о том, что список меток поменялся. */

  GHashTable              *marks;            /* Список меток (отдаваемый наружу). */
  GMutex                   marks_lock;       /* Блокировка списка меток. */
};

static void     hyscan_mark_manager_set_property             (GObject                *object,
                                                              guint                   prop_id,
                                                              const GValue           *value,
                                                              GParamSpec             *pspec);
static void     hyscan_mark_manager_object_constructed       (GObject                *object);
static void     hyscan_mark_manager_object_finalize          (GObject                *object);

static void     hyscan_mark_manager_clear_state              (HyScanMarkManagerState   *state);
static gboolean hyscan_mark_manager_track_sync               (HyScanMarkManagerPrivate *priv);

static void     hyscan_mark_manager_add_task                 (HyScanMarkManagerPrivate    *priv,
                                                              const gchar               *id,
                                                              const HyScanWaterfallMark *mark,
                                                              gint                       action);
static void     hyscan_mark_manager_free_task                (gpointer                data);
static void     hyscan_mark_manager_do_task                  (gpointer                data,
                                                              gpointer                user_data);
static void     hyscan_mark_manager_do_all_tasks             (HyScanMarkManagerPrivate  *priv,
                                                              HyScanWaterfallMarkData *data);
static gpointer hyscan_mark_manager_processing               (gpointer                data);
static gboolean hyscan_mark_manager_signaller                (gpointer                data);

static guint    hyscan_mark_manager_signals[SIGNAL_LAST] = {0};

G_DEFINE_TYPE_WITH_PRIVATE (HyScanMarkManager, hyscan_mark_manager, G_TYPE_OBJECT);

static void
hyscan_mark_manager_class_init (HyScanMarkManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_mark_manager_set_property;

  object_class->constructed = hyscan_mark_manager_object_constructed;
  object_class->finalize = hyscan_mark_manager_object_finalize;

  g_object_class_install_property (object_class, PROP_DB,
    g_param_spec_object ("db", "DB", "HyScan DB", HYSCAN_TYPE_DB,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  hyscan_mark_manager_signals[SIGNAL_CHANGED] =
    g_signal_new ("changed", HYSCAN_TYPE_MARK_MANAGER,
                  G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

static void
hyscan_mark_manager_init (HyScanMarkManager *self)
{
  self->priv = hyscan_mark_manager_get_instance_private (self);
}

static void
hyscan_mark_manager_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  HyScanMarkManager *self = HYSCAN_MARK_MANAGER (object);
  HyScanMarkManagerPrivate *priv = self->priv;

  if (prop_id == PROP_DB)
    priv->db = g_value_dup_object (value);
  else
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
}

static void
hyscan_mark_manager_object_constructed (GObject *object)
{
  HyScanMarkManager *self = HYSCAN_MARK_MANAGER (object);
  HyScanMarkManagerPrivate *priv = self->priv;

  g_mutex_init (&priv->tasks_lock);
  g_mutex_init (&priv->marks_lock);
  g_mutex_init (&priv->state_lock);
  g_cond_init (&priv->im_cond);

  priv->stop = FALSE;
  priv->processing = g_thread_new ("wf-mark-process",
                                   hyscan_mark_manager_processing, priv);
  priv->alerter = g_timeout_add (1000, hyscan_mark_manager_signaller, self);

  priv->marks = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                       (GDestroyNotify)hyscan_waterfall_mark_deep_free);
}

static void
hyscan_mark_manager_object_finalize (GObject *object)
{
  HyScanMarkManager *self = HYSCAN_MARK_MANAGER (object);
  HyScanMarkManagerPrivate *priv = self->priv;

  if (priv->alerter > 0)
    g_source_remove (priv->alerter);

  g_atomic_int_set (&priv->stop, TRUE);
  g_clear_pointer (&priv->processing, g_thread_join);

  g_mutex_clear (&priv->tasks_lock);
  g_mutex_clear (&priv->marks_lock);
  g_mutex_clear (&priv->state_lock);
  g_cond_clear (&priv->im_cond);

  g_object_unref (priv->db);
  hyscan_mark_manager_clear_state (&priv->new_state);
  hyscan_mark_manager_clear_state (&priv->cur_state);
  g_hash_table_unref (priv->marks);

  G_OBJECT_CLASS (hyscan_mark_manager_parent_class)->finalize (object);
}

/* Функция очищает состояние. */
static void
hyscan_mark_manager_clear_state (HyScanMarkManagerState   *state)
{
  g_clear_pointer (&state->project, g_free);
  g_clear_pointer (&state->track, g_free);
  g_clear_pointer (&state->profile, g_free);
}

/* Функция синхронизирует состояния. */
static gboolean
hyscan_mark_manager_track_sync (HyScanMarkManagerPrivate *priv)
{
  g_mutex_lock (&priv->state_lock);

  /* Проверяем, нужна ли синхронизация. */
  if (!priv->new_state.track_changed && !priv->new_state.profile_changed)
    {
      g_mutex_unlock (&priv->state_lock);
      return FALSE;
    }

  if (priv->new_state.track_changed)
    {
      g_clear_pointer (&priv->cur_state.project, g_free);
      g_clear_pointer (&priv->cur_state.track, g_free);
      priv->cur_state.project = priv->new_state.project;
      priv->cur_state.track   = priv->new_state.track;
      priv->new_state.project = NULL;
      priv->new_state.track   = NULL;
      priv->new_state.track_changed = FALSE;
    }
  if (priv->new_state.profile_changed)
    {
      g_clear_pointer (&priv->cur_state.profile, g_free);
      priv->cur_state.profile = priv->new_state.profile;
      priv->new_state.profile = NULL;
      priv->new_state.profile_changed = FALSE;
    }


  g_mutex_unlock (&priv->state_lock);
  return TRUE;
}

static void
hyscan_mark_manager_add_task (HyScanMarkManagerPrivate    *priv,
                              const gchar                 *id,
                              const HyScanWaterfallMark   *mark,
                              gint                         action)
{
  /* Создаем задание. */
  HyScanMarkManagerTask *task;

  task = g_new (HyScanMarkManagerTask, 1);
  task->action = action;
  task->id = (id != NULL) ? g_strdup (id) : NULL;
  task->mark = (mark != NULL) ? hyscan_waterfall_mark_copy (mark) : NULL;

  /* Отправляем задание в очередь. */
  g_mutex_lock (&priv->tasks_lock);
  priv->tasks = g_slist_prepend (priv->tasks, task);
  g_mutex_unlock (&priv->tasks_lock);

  /* Сигнализируем об изменениях.
   * Документация гласит, что блокировать мьютекс хорошо,
   * но не обязательно. */
  g_atomic_int_set (&priv->im_flag, 1);
  g_cond_signal (&priv->im_cond);
}

static void
hyscan_mark_manager_free_task (gpointer data)
{
  HyScanMarkManagerTask *task = data;
  g_free (task->id);
  hyscan_waterfall_mark_deep_free (task->mark);
  g_free (task);
}

static void
hyscan_mark_manager_do_task (gpointer data,
                           gpointer user_data)
{
  HyScanMarkManagerTask *task = data;
  HyScanWaterfallMarkData *mark_data = user_data;

  switch (task->action)
    {
    case MARK_ADD:
      if (!hyscan_waterfall_mark_data_add (mark_data, task->mark))
        g_message ("Failed to add mark");
      break;

    case MARK_MODIFY:
      if (!hyscan_waterfall_mark_data_modify (mark_data, task->id, task->mark))
        g_message ("Failed to modify mark <%s>", task->id);
      break;

    case MARK_REMOVE:
      if (!hyscan_waterfall_mark_data_remove (mark_data, task->id))
        g_message ("Failed to remove mark <%s>", task->id);
      break;
    }
}

static void
hyscan_mark_manager_do_all_tasks (HyScanMarkManagerPrivate  *priv,
                                  HyScanWaterfallMarkData *data)
{
  GSList *tasks;

  /* Переписываем задания во временный список, чтобы
   * как можно меньше задерживать основной поток. */
  g_mutex_lock (&priv->tasks_lock);
  tasks = priv->tasks;
  priv->tasks = NULL;
  g_mutex_unlock (&priv->tasks_lock);

  /* Все задания отправляем в БД и очищаем наш временный список. */
  g_slist_foreach (tasks, hyscan_mark_manager_do_task, data);
  g_slist_free_full (tasks, hyscan_mark_manager_free_task);

}
static gpointer
hyscan_mark_manager_processing (gpointer data)
{
  HyScanWaterfallMarkData *mark_data = NULL;             /* Объект работы с метками. */
  HyScanMarkManagerPrivate *priv = data;
  guint32 mc, old_mc;     /* Счетчик изменений. */
  gchar **id_list;        /* Идентификаторы меток. */
  GHashTable *mark_list;  /* Метки из БД. */
  GMutex im_lock;         /* Мьютекс нужен для GCond. */
  HyScanWaterfallMark *mark;
  guint len, i;

  g_mutex_init (&im_lock);

  while (!g_atomic_int_get (&priv->stop))
    {
      /* Ждём, когда нужно действовать. */
      if (g_atomic_int_get (&priv->im_flag) == 0)
        {
          gboolean res;
          gint64 until = g_get_monotonic_time () + DELAY;

          g_mutex_lock (&im_lock);
          res = g_cond_wait_until (&priv->im_cond, &im_lock, until);
          g_mutex_unlock (&im_lock);
          if (!res)
            continue;
        }

      /* Если галс поменялся, надо выполнить все
       * текущие задачи и пересоздать объект. */
      if (hyscan_mark_manager_track_sync (priv))
        {
          /* TODO: сигнал, сообщающий о том, что сейчас поменяется
           * mark_data и что это последняя возможность запушить
           * актуальные задачи */
          if (mark_data != NULL)
            hyscan_mark_manager_do_all_tasks (priv, mark_data);
          g_clear_object (&mark_data);
        }

      if (mark_data == NULL)
        {
          /* Создаем объект работы с метками. */
          mark_data = hyscan_waterfall_mark_data_new (priv->db,
                                                      priv->cur_state.project,
                                                      priv->cur_state.track,
                                                      priv->cur_state.profile);
          /* Если не получилось (например потому, что галс ещё не создан),
           * повторим через некоторое время. */
          if (mark_data == NULL)
            {
              g_usleep (DELAY);
              continue;
            }

          /* Проинициализируем mod_count заведомо неактуальным значением. */
          old_mc = ~hyscan_waterfall_mark_data_get_mod_count (mark_data);
        }

      g_atomic_int_set (&priv->im_flag, 0);

      /* Выполняем все задания. */
      hyscan_mark_manager_do_all_tasks (priv, mark_data);

      /* Запрашиваем у базы обновленный mod_count. */
      mc = hyscan_waterfall_mark_data_get_mod_count (mark_data);
      if (old_mc == mc)
        continue;
      else
        old_mc = mc;

      /* Забираем метки из БД во временную хэш-таблицу. */
      mark_list = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                         (GDestroyNotify)hyscan_waterfall_mark_deep_free);

      id_list = hyscan_waterfall_mark_data_get_ids (mark_data, &len);
      for (i = 0; i < len; i++)
        {
          mark = hyscan_waterfall_mark_data_get (mark_data, id_list[i]);
          if (mark != NULL)
            g_hash_table_insert (mark_list, g_strdup (id_list[i]), mark);
        }
      g_strfreev (id_list);

      /* Очищаем хэш-таблицу из привата и помещаем туда свежесозданную. */
      g_mutex_lock (&priv->marks_lock);
      g_clear_pointer (&priv->marks, g_hash_table_unref);
      priv->marks = mark_list;
      priv->marks_changed = TRUE;
      g_mutex_unlock (&priv->marks_lock);
    }

  g_clear_object (&mark_data);
  g_mutex_clear (&im_lock);
  return NULL;
}

static gboolean
hyscan_mark_manager_signaller (gpointer data)
{
  HyScanMarkManager *self = data;
  HyScanMarkManagerPrivate *priv = self->priv;
  gboolean marks_changed;

  /* Если есть изменения, сигнализируем. */
  g_mutex_lock (&priv->marks_lock);
  marks_changed = priv->marks_changed;
  priv->marks_changed = FALSE;
  g_mutex_unlock (&priv->marks_lock);

  if (marks_changed)
    g_signal_emit (self, hyscan_mark_manager_signals[SIGNAL_CHANGED], 0, NULL);
  return G_SOURCE_CONTINUE;
}

HyScanMarkManager*
hyscan_mark_manager_new (HyScanDB *db)
{
  return g_object_new (HYSCAN_TYPE_MARK_MANAGER, "db", db, NULL);
}

void
hyscan_mark_manager_set_profile (HyScanMarkManager *self,
                                 const gchar       *profile)
{
  HyScanMarkManagerPrivate *priv;

  g_return_if_fail (HYSCAN_IS_MARK_MANAGER (self));
  priv = self->priv;

  if (profile == NULL)
    profile = "default";

  g_mutex_lock (&priv->state_lock);

  /* Проверим, что профиль действительно поменялся. */
  if (0 == g_strcmp0 (profile, priv->cur_state.profile))
    goto exit;

  /* Действительно поменялся! */
  g_clear_pointer (&priv->new_state.profile, g_free);

  priv->new_state.profile = g_strdup (profile);
  priv->new_state.profile_changed = TRUE;

  /* Сигнализируем об изменениях. */
  g_atomic_int_set (&priv->im_flag, 1);
  g_cond_signal (&priv->im_cond);

exit:
  g_mutex_unlock (&priv->state_lock);
}

void
hyscan_mark_manager_set_track (HyScanMarkManager *self,
                               const gchar       *project,
                               const gchar       *track)
{
  HyScanMarkManagerPrivate *priv;

  g_return_if_fail (HYSCAN_IS_MARK_MANAGER (self));
  priv = self->priv;

  if (project == NULL || track == NULL)
    return;

  g_mutex_lock (&priv->state_lock);

  /* Проверим, что проект, галс и профиль действительно поменялись. */
  if (0 == g_strcmp0 (project, priv->cur_state.project) &&
      0 == g_strcmp0 (track, priv->cur_state.track))
    {
      goto exit;
    }

  /* Действительно поменялись! */
  g_clear_pointer (&priv->new_state.project, g_free);
  g_clear_pointer (&priv->new_state.track, g_free);

  priv->new_state.project = g_strdup (project);
  priv->new_state.track = g_strdup (track);
  priv->new_state.track_changed = TRUE;

  /* Сигнализируем об изменениях. */
  g_atomic_int_set (&priv->im_flag, 1);
  g_cond_signal (&priv->im_cond);

exit:
  g_mutex_unlock (&priv->state_lock);
}

void
hyscan_mark_manager_add (HyScanMarkManager     *self,
                         HyScanWaterfallMark *mark)
{
  g_return_if_fail (HYSCAN_IS_MARK_MANAGER (self));

  hyscan_mark_manager_add_task (self->priv, NULL, mark, MARK_ADD);
}

void
hyscan_mark_manager_modify (HyScanMarkManager     *self,
                            const gchar         *id,
                            HyScanWaterfallMark *mark)
{
  g_return_if_fail (HYSCAN_IS_MARK_MANAGER (self));

  hyscan_mark_manager_add_task (self->priv, id, mark, MARK_MODIFY);
}

void
hyscan_mark_manager_remove (HyScanMarkManager     *self,
                            const gchar         *id)
{
  g_return_if_fail (HYSCAN_IS_MARK_MANAGER (self));

  hyscan_mark_manager_add_task (self->priv, id, NULL, MARK_REMOVE);
}

GHashTable*
hyscan_mark_manager_get (HyScanMarkManager *self)
{
  HyScanMarkManagerPrivate *priv;
  GHashTable *marks;
  GHashTableIter iter;
  gpointer key, value;

  g_return_val_if_fail (HYSCAN_IS_MARK_MANAGER (self), NULL);
  priv = self->priv;

  marks = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                 (GDestroyNotify)hyscan_waterfall_mark_deep_free);

  g_mutex_lock (&priv->marks_lock);

  if (priv->marks != NULL)
    {
      g_hash_table_iter_init (&iter, priv->marks);
      while (g_hash_table_iter_next (&iter, &key, &value))
        g_hash_table_insert (marks, g_strdup (key), hyscan_waterfall_mark_copy (value));
    }
  else
    {
      g_hash_table_unref (marks);
      marks = NULL;
    }

  g_mutex_unlock (&priv->marks_lock);

  return marks;
}
