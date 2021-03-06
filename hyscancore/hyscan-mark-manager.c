/*
 * \file hyscan-mark-manager.c
 *
 * \brief Исходный файл класса асинхронной работы с метками водопада
 * \author Dmitriev Alexander (m1n7@yandex.ru)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-mark-manager.h"
#include <hyscan-mloc.h>
#include <hyscan-projector.h>

#define DELAY (1 * G_TIME_SPAN_SECOND)

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

/* Задание. Структура с информацией о том, что требуется сделать. */
typedef struct
{
  gchar               *id;        /* Идентификатор метки. */
  HyScanWaterfallMark *mark;      /* Метка. */
  gint                 action;    /* Требуемое действие. */
} HyScanMarkManagerTask;

/* Для координат. */
typedef struct
{
  gchar               *track;      /* Название галса. */
  HyScanmLoc          *mloc;   /* Объект для определения местоположения. */
  GHashTable          *projectors; /* Проекторы (для каждого канала). */
} HyScanMarkManagerLocation;

/* Состояние объекта. */
typedef struct
{
  HyScanDB                *db;               /* БД. */
  gchar                   *project;          /* Проект. */
  gboolean                 project_changed;  /* Флаг смены БД/проекта. */
} HyScanMarkManagerState;

struct _HyScanMarkManagerPrivate
{
  HyScanMarkManagerState   cur_state;        /* Текущее состояние. */
  HyScanMarkManagerState   new_state;        /* Желаемое состояние. */
  GMutex                   state_lock;       /* Блокировка состояния. */

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

  GHashTable              *marks_w_coords;   /* Список меток с координатами . */
};

static void     hyscan_mark_manager_object_constructed       (GObject                   *object);
static void     hyscan_mark_manager_object_finalize          (GObject                   *object);

static void     hyscan_mark_manager_clear_state              (HyScanMarkManagerState    *state);
static gboolean hyscan_mark_manager_track_sync               (HyScanMarkManagerPrivate  *priv);

static void     hyscan_mark_manager_add_task                 (HyScanMarkManagerPrivate  *priv,
                                                              const gchar               *id,
                                                              const HyScanWaterfallMark *mark,
                                                              gint                       action);
static void     hyscan_mark_manager_free_task                (gpointer                   data);
static void     hyscan_mark_manager_do_task                  (gpointer                   data,
                                                              gpointer                   user_data);
static void     hyscan_mark_manager_do_all_tasks             (HyScanMarkManagerPrivate  *priv,
                                                              HyScanWaterfallMarkData   *data);
static gpointer hyscan_mark_manager_processing               (gpointer                   data);
static gboolean hyscan_mark_manager_signaller                (gpointer                   data);

static guint    hyscan_mark_manager_signals[SIGNAL_LAST] = {0};

G_DEFINE_TYPE_WITH_PRIVATE (HyScanMarkManager, hyscan_mark_manager, G_TYPE_OBJECT);

static void
hyscan_mark_manager_class_init (HyScanMarkManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = hyscan_mark_manager_object_constructed;
  object_class->finalize = hyscan_mark_manager_object_finalize;

  hyscan_mark_manager_signals[SIGNAL_CHANGED] =
    g_signal_new ("changed", HYSCAN_TYPE_MARK_MANAGER,
                  G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

static void
hyscan_mark_manager_init (HyScanMarkManager *manager)
{
  manager->priv = hyscan_mark_manager_get_instance_private (manager);
}

static void
hyscan_mark_manager_object_constructed (GObject *object)
{
  HyScanMarkManager *manager = HYSCAN_MARK_MANAGER (object);
  HyScanMarkManagerPrivate *priv = manager->priv;

  g_mutex_init (&priv->tasks_lock);
  g_mutex_init (&priv->marks_lock);
  g_mutex_init (&priv->state_lock);
  g_cond_init (&priv->im_cond);

  priv->stop = FALSE;
  priv->processing = g_thread_new ("wf-mark-process", hyscan_mark_manager_processing, priv);
  priv->alerter = g_timeout_add (1000, hyscan_mark_manager_signaller, manager);

  priv->marks = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                       (GDestroyNotify)hyscan_waterfall_mark_free);
}

static void
hyscan_mark_manager_object_finalize (GObject *object)
{
  HyScanMarkManager *manager = HYSCAN_MARK_MANAGER (object);
  HyScanMarkManagerPrivate *priv = manager->priv;

  if (priv->alerter > 0)
    g_source_remove (priv->alerter);

  g_atomic_int_set (&priv->stop, TRUE);
  g_clear_pointer (&priv->processing, g_thread_join);

  g_mutex_clear (&priv->tasks_lock);
  g_mutex_clear (&priv->marks_lock);
  g_mutex_clear (&priv->state_lock);
  g_cond_clear (&priv->im_cond);

  hyscan_mark_manager_clear_state (&priv->new_state);
  hyscan_mark_manager_clear_state (&priv->cur_state);
  g_hash_table_unref (priv->marks);

  G_OBJECT_CLASS (hyscan_mark_manager_parent_class)->finalize (object);
}

/* Функция очищает состояние. */
static void
hyscan_mark_manager_clear_state (HyScanMarkManagerState *state)
{
  g_clear_pointer (&state->project, g_free);
  g_clear_object (&state->db);
  state->project_changed = FALSE;
}

/* Функция синхронизирует состояния. */
static gboolean
hyscan_mark_manager_track_sync (HyScanMarkManagerPrivate *priv)
{
  g_mutex_lock (&priv->state_lock);

  /* Проверяем, нужна ли синхронизация. */
  if (!priv->new_state.project_changed)
    {
      g_mutex_unlock (&priv->state_lock);
      return FALSE;
    }

  g_clear_pointer (&priv->cur_state.project, g_free);
  priv->cur_state.db = priv->new_state.db;
  priv->cur_state.project = priv->new_state.project;
  priv->new_state.db = NULL;
  priv->new_state.project = NULL;
  priv->new_state.project_changed = FALSE;

  g_mutex_unlock (&priv->state_lock);
  return TRUE;
}

/* Функция создает новое задание. */
static void
hyscan_mark_manager_add_task (HyScanMarkManagerPrivate  *priv,
                              const gchar               *id,
                              const HyScanWaterfallMark *mark,
                              gint                       action)
{
  /* Создаем задание. */
  HyScanMarkManagerTask *task;

  task = g_new (HyScanMarkManagerTask, 1);
  task->action = action;
  task->id = (id != NULL) ? g_strdup (id) : NULL;
  task->mark = hyscan_waterfall_mark_copy ((HyScanWaterfallMark*)mark);

  /* Отправляем задание в очередь. */
  g_mutex_lock (&priv->tasks_lock);
  priv->tasks = g_slist_append (priv->tasks, task);
  g_mutex_unlock (&priv->tasks_lock);

  /* Сигнализируем об изменениях.
   * Документация гласит, что блокировать мьютекс хорошо,
   * но не обязательно. */
  g_atomic_int_set (&priv->im_flag, 1);
  g_cond_signal (&priv->im_cond);
}

/* Функция освобождает задание. */
static void
hyscan_mark_manager_free_task (gpointer data)
{
  HyScanMarkManagerTask *task = data;
  g_clear_pointer (&task->mark, hyscan_waterfall_mark_free);
  g_clear_pointer (&task->id, g_free);
  g_free (task);
}

HyScanMarkManagerMarkLoc *
hyscan_mark_manager_mark_coords (GHashTable             * locstore,
                                 HyScanWaterfallMark    * mark,
                                 HyScanMarkManagerState * state)
{
  gdouble along, across;
  HyScanProjector *projector;
  HyScanAcousticData *adata;
  HyScanAntennaPosition apos;
  gint64 time;
  guint32 n;
  HyScanGeoGeodetic position;
  HyScanMarkManagerMarkLoc *res;
  HyScanMarkManagerLocation *location = g_hash_table_lookup (locstore, mark->track);

  if (location == NULL)
    return NULL;

  projector = g_hash_table_lookup (location->projectors, GINT_TO_POINTER (mark->source0));
  if (projector == NULL)
    {
      projector = hyscan_projector_new (state->db, state->project, location->track, mark->source0, FALSE);
      if (projector == NULL)
        projector = hyscan_projector_new (state->db, state->project, location->track, mark->source0, TRUE);
      if (projector == NULL)
        return NULL;

      g_hash_table_insert (location->projectors, GINT_TO_POINTER (mark->source0), projector);
    }

  hyscan_projector_index_to_coord (projector, mark->index0, &along);
  hyscan_projector_count_to_coord (projector, mark->count0, &across, 0);

  adata = (HyScanAcousticData*) hyscan_projector_get_acoustic_data (projector);
  apos = hyscan_acoustic_data_get_position (adata);
  hyscan_acoustic_data_get_values (adata, mark->index0, &n, &time);

  if (mark->source0 == HYSCAN_SOURCE_SIDE_SCAN_PORT)
    across *= -1;

  hyscan_mloc_get (location->mloc, time, &apos, across, &position);

  res = g_new (HyScanMarkManagerMarkLoc, 1);
  res->mark = hyscan_waterfall_mark_copy (mark);
  res->lat = position.lat;
  res->lon = position.lon;

  return res;
}

/* Функция выполняет задание. */
static void
hyscan_mark_manager_do_task (gpointer _task,
                             gpointer _mdata)
{
  HyScanMarkManagerTask *task = _task;
  HyScanWaterfallMarkData *mdata = _mdata;

  switch (task->action)
    {
    case MARK_ADD:
      if (!hyscan_waterfall_mark_data_add (mdata, task->mark))
        g_warning ("Failed to add mark");
      break;

    case MARK_MODIFY:
      if (!hyscan_waterfall_mark_data_modify (mdata, task->id, task->mark))
        g_warning ("Failed to modify mark <%s>", task->id);
      break;

    case MARK_REMOVE:
      if (!hyscan_waterfall_mark_data_remove (mdata, task->id))
        g_warning ("Failed to remove mark <%s>", task->id);
      break;
    }
}

/* Функция выполняет все задания. */
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

static void
hyscan_mark_manager_location_free (gpointer data)
{
  HyScanMarkManagerLocation *loc = data;
  g_clear_pointer(&loc->track, g_free);
  g_clear_object (&loc->mloc);
  g_clear_pointer(&loc->projectors, g_hash_table_unref);
}

static HyScanMarkManagerLocation *
hyscan_mark_manager_location_new (HyScanDB    *db,
                                  const gchar *project,
                                  const gchar *track)
{
  HyScanMarkManagerLocation *loc;

  loc = g_new0 (HyScanMarkManagerLocation, 1);
  loc->track = g_strdup (track);
  loc->mloc = hyscan_mloc_new (db, project, track);
  loc->projectors = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL,
                                           (GDestroyNotify)g_object_unref);

  if (loc->mloc == NULL)
    g_clear_pointer (&loc, hyscan_mark_manager_location_free);

  return loc;
}

/* Поток асинхронной работы с БД. */
static gpointer
hyscan_mark_manager_processing (gpointer data)
{
  HyScanMarkManagerPrivate *priv = data;
  HyScanWaterfallMarkData *mdata = NULL; /* Объект работы с метками. */
  gchar **id_list;                       /* Идентификаторы меток. */
  GHashTable *mark_list = NULL;          /* Метки из БД. */
  GHashTable *mark_coords_list = NULL;   /* Метки из БД. */
  GHashTable *projects_ids = NULL;       /* Метки из БД. */
  GMutex im_lock;                        /* Мьютекс нужен для GCond. */
  HyScanWaterfallMark *mark = NULL;
  guint len, i;
  guint32 oldmc, mc;                     /* Значения счетчика изменений. */

  guint32 project_oldmc, project_mc;     /* Значения счетчика изменений уровня проекта. */
  gint32 project_fd = -1;

  project_oldmc = project_mc = oldmc = mc = 0;
  g_mutex_init (&im_lock);

  while (!g_atomic_int_get (&priv->stop))
    {
      /* Ждём, когда нужно действовать. */
      if (mdata != NULL)
        mc = hyscan_waterfall_mark_data_get_mod_count (mdata);

      if (oldmc == mc && g_atomic_int_get (&priv->im_flag) == 0)
        {
          gboolean triggered;
          gint64 until = g_get_monotonic_time () + DELAY;

          g_mutex_lock (&im_lock);
          triggered = g_cond_wait_until (&priv->im_cond, &im_lock, until);
          g_mutex_unlock (&im_lock);

          if (!triggered)
            continue;
        }

      g_atomic_int_set (&priv->im_flag, 0);

      /* Если проект поменялся, надо выполнить все
       * текущие задачи и пересоздать объект. */
      if (hyscan_mark_manager_track_sync (priv))
        {
          /* TODO: сигнал, сообщающий о том, что сейчас поменяется mdata?
           * и что это последняя возможность запушить актуальные задачи */
          if (mdata != NULL)
            hyscan_mark_manager_do_all_tasks (priv, mdata);
          g_clear_object (&mdata);

          hyscan_db_close (priv->cur_state.db, project_fd);
          project_fd = -1;
        }

      /* Если надо, создаем объект работы с метками. */
      if (mdata == NULL)
        {
          if (priv->cur_state.db != NULL && priv->cur_state.project != NULL)
            {
              mdata = hyscan_waterfall_mark_data_new (priv->cur_state.db,
                                                      priv->cur_state.project);
              project_fd = hyscan_db_project_open (priv->cur_state.db,
                                                   priv->cur_state.project);
            }

          /* Если не получилось (например потому, что проект ещё не создан),
           * повторим через некоторое время. */
          if (mdata == NULL)
            {
              g_atomic_int_set (&priv->im_flag, 1);
              g_usleep (DELAY);
              continue;
            }

          mc = hyscan_waterfall_mark_data_get_mod_count (mdata);
          project_mc = hyscan_db_get_mod_count (priv->cur_state.db, project_fd);
          project_oldmc = ~project_mc;
        }

      oldmc = mc;

      /* Выполняем все задания. */
      hyscan_mark_manager_do_all_tasks (priv, mdata);

      /* Проверяем, есть ли новые галсы в проекте. */
      project_mc = hyscan_db_get_mod_count (priv->cur_state.db, project_fd);
      if (project_mc != project_oldmc)
        {
          HyScanDB *db = priv->cur_state.db;
          gchar ** track_list;
          gchar ** name;

          if (projects_ids == NULL)
            projects_ids = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                                  (GDestroyNotify)hyscan_mark_manager_location_free);

          track_list = hyscan_db_track_list (db, project_fd);

          for (name = track_list; name != NULL && *name != NULL; ++name)
            {
              gint32 track_fd, param_fd;
              gchar *id;

              track_fd = hyscan_db_track_open (db, project_fd, *name);
              param_fd = hyscan_db_track_param_open (db, track_fd);
              id = hyscan_db_param_get_string (db, param_fd, NULL, "/id");

              if (id == NULL)
                continue;

              /* Ежели этого галса ещё нет в нашей чудесной хэш-таблице,
               * внедрим его туда. */
              if (!g_hash_table_contains (projects_ids, id))
                {
                  HyScanMarkManagerLocation *loc;
                  loc = hyscan_mark_manager_location_new (db, priv->cur_state.project, *name);

                  g_hash_table_insert (projects_ids, g_strdup (id), loc);
                }

              hyscan_db_close (db, track_fd);
              hyscan_db_close (db, param_fd);
              g_free (id);
            }

          g_strfreev (track_list);
        }

      /* Забираем метки из БД во временную хэш-таблицу. */
      mark_list = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                         (GDestroyNotify)hyscan_waterfall_mark_free);
      mark_coords_list = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                         (GDestroyNotify)hyscan_mark_manager_mark_loc_free);

      id_list = hyscan_waterfall_mark_data_get_ids (mdata, &len);
      for (i = 0; i < len; i++)
        {
          HyScanMarkManagerMarkLoc * mwcoords;
          mark = hyscan_waterfall_mark_data_get (mdata, id_list[i]);
          if (mark == NULL)
            continue;

          /* Кладем в список меток. */
          g_hash_table_insert (mark_list, g_strdup (id_list[i]), mark);

          /* Ещё надо координаты посчитать. */
          mwcoords = hyscan_mark_manager_mark_coords (projects_ids, mark, &priv->cur_state);
          if (mwcoords != NULL)
            g_hash_table_insert (mark_coords_list, g_strdup (id_list[i]), mwcoords);

        }
      g_strfreev (id_list);

      /* Очищаем хэш-таблицу из привата и помещаем туда свежесозданную. */
      g_mutex_lock (&priv->marks_lock);
      g_clear_pointer (&priv->marks, g_hash_table_unref);
      g_clear_pointer (&priv->marks_w_coords, g_hash_table_unref);
      priv->marks = mark_list;
      priv->marks_w_coords = mark_coords_list;
      priv->marks_changed = TRUE;
      g_mutex_unlock (&priv->marks_lock);
    }

  g_clear_object (&mdata);
  hyscan_db_close (priv->cur_state.db, project_fd);
  g_mutex_clear (&im_lock);
  return NULL;
}

/* Функция выполняется в MainLoop и сигнализирует об изменениях. */
static gboolean
hyscan_mark_manager_signaller (gpointer data)
{
  HyScanMarkManager *manager = data;
  HyScanMarkManagerPrivate *priv = manager->priv;
  gboolean marks_changed;

  /* Если есть изменения, сигнализируем. */
  g_mutex_lock (&priv->marks_lock);
  marks_changed = priv->marks_changed;
  priv->marks_changed = FALSE;
  g_mutex_unlock (&priv->marks_lock);

  if (marks_changed)
    g_signal_emit (manager, hyscan_mark_manager_signals[SIGNAL_CHANGED], 0, NULL);

  return G_SOURCE_CONTINUE;
}

/* Функция создает новый объект HyScanMarkManager. */
HyScanMarkManager*
hyscan_mark_manager_new (void)
{
  return g_object_new (HYSCAN_TYPE_MARK_MANAGER, NULL);
}

/* Функция устанавливает проект. */
void
hyscan_mark_manager_set_project (HyScanMarkManager *manager,
                                 HyScanDB          *db,
                                 const gchar       *project)
{
  HyScanMarkManagerPrivate *priv;

  g_return_if_fail (HYSCAN_IS_MARK_MANAGER (manager));
  priv = manager->priv;

  if (project == NULL || db == NULL)
    return;

  g_mutex_lock (&priv->state_lock);

  hyscan_mark_manager_clear_state (&priv->new_state);

  priv->new_state.db = g_object_ref (db);
  priv->new_state.project = g_strdup (project);
  priv->new_state.project_changed = TRUE;

  /* Сигнализируем об изменениях. */
  g_atomic_int_set (&priv->im_flag, 1);
  g_cond_signal (&priv->im_cond);

  g_mutex_unlock (&priv->state_lock);
}

/* Функция инициирует принудительное обновление списка меток. */
void
hyscan_mark_manager_refresh (HyScanMarkManager  *manager)
{
  g_return_if_fail (HYSCAN_IS_MARK_MANAGER (manager));

  g_atomic_int_set (&manager->priv->im_flag, 1);
  g_cond_signal (&manager->priv->im_cond);
}

/* Функция создает метку в базе данных. */
void
hyscan_mark_manager_add_mark (HyScanMarkManager         *manager,
                              const HyScanWaterfallMark *mark)
{
  g_return_if_fail (HYSCAN_IS_MARK_MANAGER (manager));

  hyscan_mark_manager_add_task (manager->priv, NULL, mark, MARK_ADD);
}

/* Функция изменяет метку в базе данных. */
void
hyscan_mark_manager_modify_mark (HyScanMarkManager         *manager,
                                 const gchar               *id,
                                 const HyScanWaterfallMark *mark)
{
  g_return_if_fail (HYSCAN_IS_MARK_MANAGER (manager));

  hyscan_mark_manager_add_task (manager->priv, id, mark, MARK_MODIFY);
}

/* Функция удаляет метку из базы данных. */
void
hyscan_mark_manager_remove_mark (HyScanMarkManager *manager,
                                 const gchar       *id)
{
  g_return_if_fail (HYSCAN_IS_MARK_MANAGER (manager));

  hyscan_mark_manager_add_task (manager->priv, id, NULL, MARK_REMOVE);
}

/* Функция возвращает список меток из внутреннего буфера. */
GHashTable*
hyscan_mark_manager_get (HyScanMarkManager *manager)
{
  HyScanMarkManagerPrivate *priv;
  GHashTable *marks;
  GHashTableIter iter;
  gpointer key, value;

  g_return_val_if_fail (HYSCAN_IS_MARK_MANAGER (manager), NULL);
  priv = manager->priv;

  g_mutex_lock (&priv->marks_lock);

  /* Проверяем, что метки есть. */
  if (priv->marks == NULL)
    {
      g_mutex_unlock (&priv->marks_lock);
      return NULL;
    }

  marks = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                 (GDestroyNotify)hyscan_waterfall_mark_free);
  /* Переписываем метки. */
  g_hash_table_iter_init (&iter, priv->marks);
  while (g_hash_table_iter_next (&iter, &key, &value))
    g_hash_table_insert (marks, g_strdup (key), hyscan_waterfall_mark_copy (value));

  g_mutex_unlock (&priv->marks_lock);

  return marks;
}

GHashTable *
hyscan_mark_manager_get_w_coords (HyScanMarkManager *manager)
{
  HyScanMarkManagerPrivate *priv;
  GHashTable *marks;
  GHashTableIter iter;
  gpointer key, value;

  g_return_val_if_fail (HYSCAN_IS_MARK_MANAGER (manager), NULL);
  priv = manager->priv;

  g_mutex_lock (&priv->marks_lock);

  /* Проверяем, что метки есть. */
  if (priv->marks_w_coords == NULL)
    {
      g_mutex_unlock (&priv->marks_lock);
      return NULL;
    }

  marks = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                 (GDestroyNotify)hyscan_mark_manager_mark_loc_free);
  /* Переписываем метки. */
  g_hash_table_iter_init (&iter, priv->marks_w_coords);
  while (g_hash_table_iter_next (&iter, &key, &value))
    g_hash_table_insert (marks, g_strdup (key), hyscan_mark_manager_mark_loc_copy (value));

  g_mutex_unlock (&priv->marks_lock);

  return marks;
}

HyScanMarkManagerMarkLoc *
hyscan_mark_manager_mark_loc_copy (gpointer data)
{
  HyScanMarkManagerMarkLoc *src = data;
  HyScanMarkManagerMarkLoc *dst = g_new (HyScanMarkManagerMarkLoc, 1);
  dst->mark = hyscan_waterfall_mark_copy (src->mark);
  dst->lat = src->lat;
  dst->lon = src->lon;

  return dst;
}

void
hyscan_mark_manager_mark_loc_free (gpointer data)
{
  HyScanMarkManagerMarkLoc *loc = data;

  hyscan_waterfall_mark_free (loc->mark);
}
