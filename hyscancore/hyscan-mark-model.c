/*
 * \file hyscan-mark-model.c
 *
 * \brief Исходный файл класса асинхронной работы с метками водопада
 * \author Dmitriev Alexander (m1n7@yandex.ru)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-mark-model.h"
#include "hyscan-mark-data-waterfall.h"
#include "hyscan-mark-data-geo.h"

#define DELAY (250 * G_TIME_SPAN_MILLISECOND)

enum
{
  PROP_O,
  PROP_DATA_TYPE
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

/* Задание, структура с информацией о том, что требуется сделать. */
typedef struct
{
  gchar               *id;        /* Идентификатор метки. */
  gpointer             mark;      /* Метка. */
  gint                 action;    /* Требуемое действие. */
} HyScanMarkModelTask;

/* Состояние объекта. */
typedef struct
{
  HyScanDB                *db;               /* БД. */
  gchar                   *project;          /* Проект. */
  gboolean                 project_changed;  /* Флаг смены БД/проекта. */
} HyScanMarkModelState;

struct _HyScanMarkModelPrivate
{
  GType                    data_type;        /* Класс работы с данными. */
  HyScanMarkModelState     cur_state;        /* Текущее состояние. */
  HyScanMarkModelState     new_state;        /* Желаемое состояние. */
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
};

static void     hyscan_mark_model_object_constructed       (GObject                   *object);
static void     hyscan_mark_model_object_finalize          (GObject                   *object);
static void     hyscan_mark_model_set_property             (GObject                   *object,
                                                            guint                      prop_id,
                                                            const GValue              *value,
                                                            GParamSpec                *pspec);

static void     hyscan_mark_model_clear_state              (HyScanMarkModelState      *state);
static gboolean hyscan_mark_model_track_sync               (HyScanMarkModelPrivate    *priv);

static GHashTable * hyscan_mark_model_make_ht              (HyScanMarkModelPrivate    *priv);
static void     hyscan_mark_model_add_task                 (HyScanMarkModel           *model,
                                                            const gchar               *id,
                                                            gconstpointer              mark,
                                                            gint                       action);
static void     hyscan_mark_model_free_task                (gpointer                   _task,
                                                            gpointer                   _mdata);
static void     hyscan_mark_model_do_task                  (gpointer                   data,
                                                            gpointer                   user_data);
static void     hyscan_mark_model_do_all_tasks             (HyScanMarkModelPrivate    *priv,
                                                            HyScanMarkData            *data);
static GHashTable * hyscan_mark_model_get_all_marks        (HyScanMarkModelPrivate    *priv,
                                                            HyScanMarkData            *data);
static gpointer hyscan_mark_model_processing               (gpointer                   data);
static gboolean hyscan_mark_model_signaller                (gpointer                   data);

static guint    hyscan_mark_model_signals[SIGNAL_LAST] = {0};

G_DEFINE_TYPE_WITH_PRIVATE (HyScanMarkModel, hyscan_mark_model, G_TYPE_OBJECT);

static void
hyscan_mark_model_class_init (HyScanMarkModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = hyscan_mark_model_object_constructed;
  object_class->finalize = hyscan_mark_model_object_finalize;
  object_class->set_property = hyscan_mark_model_set_property;

  g_object_class_install_property (object_class, PROP_DATA_TYPE,
    g_param_spec_gtype ("data-type", "Mark data type",
                        "The GType of HyScanMarkData inheritor", HYSCAN_TYPE_MARK_DATA,
                        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  hyscan_mark_model_signals[SIGNAL_CHANGED] =
    g_signal_new ("changed", HYSCAN_TYPE_MARK_MODEL,
                  G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

static void
hyscan_mark_model_init (HyScanMarkModel *model)
{
  model->priv = hyscan_mark_model_get_instance_private (model);
}

static void
hyscan_mark_model_object_constructed (GObject *object)
{
  HyScanMarkModel *model = HYSCAN_MARK_MODEL (object);
  HyScanMarkModelPrivate *priv = model->priv;

  g_mutex_init (&priv->tasks_lock);
  g_mutex_init (&priv->marks_lock);
  g_mutex_init (&priv->state_lock);
  g_cond_init (&priv->im_cond);

  priv->stop = FALSE;
  priv->processing = g_thread_new ("wf-mark-process", hyscan_mark_model_processing, priv);
  priv->alerter = g_timeout_add (500, hyscan_mark_model_signaller, model);
}

static void
hyscan_mark_model_object_finalize (GObject *object)
{
  HyScanMarkModel *model = HYSCAN_MARK_MODEL (object);
  HyScanMarkModelPrivate *priv = model->priv;

  if (priv->alerter > 0)
    g_source_remove (priv->alerter);

  g_atomic_int_set (&priv->stop, TRUE);
  g_clear_pointer (&priv->processing, g_thread_join);

  g_mutex_clear (&priv->tasks_lock);
  g_mutex_clear (&priv->marks_lock);
  g_mutex_clear (&priv->state_lock);
  g_cond_clear (&priv->im_cond);

  hyscan_mark_model_clear_state (&priv->new_state);
  hyscan_mark_model_clear_state (&priv->cur_state);
  g_clear_pointer (&priv->marks, g_hash_table_unref);

  G_OBJECT_CLASS (hyscan_mark_model_parent_class)->finalize (object);
}

static void
hyscan_mark_model_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  HyScanMarkModel *data = HYSCAN_MARK_MODEL (object);
  HyScanMarkModelPrivate *priv = data->priv;

  switch (prop_id)
    {
    case PROP_DATA_TYPE:
      priv->data_type = g_value_get_gtype (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/* Функция очищает состояние. */
static void
hyscan_mark_model_clear_state (HyScanMarkModelState *state)
{
  g_clear_pointer (&state->project, g_free);
  g_clear_object (&state->db);
}

/* Функция синхронизирует состояния. */
static gboolean
hyscan_mark_model_track_sync (HyScanMarkModelPrivate *priv)
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

static GHashTable *
hyscan_mark_model_make_ht (HyScanMarkModelPrivate *priv)
{
  HyScanMarkDataClass *data_class;
  GDestroyNotify destroy_func;

  data_class = g_type_class_ref (priv->data_type);
  destroy_func = data_class->object_destroy;
  g_type_class_unref (data_class);

  return g_hash_table_new_full (g_str_hash, g_str_equal, g_free, destroy_func);
}

/* Функция создает новое задание. */
static void
hyscan_mark_model_add_task (HyScanMarkModel           *model,
                            const gchar               *id,
                            gconstpointer              mark,
                            gint                       action)
{
  HyScanMarkDataClass *klass;
  HyScanMarkModelPrivate *priv = model->priv;
  HyScanMarkModelTask *task;

  klass = g_type_class_ref (priv->data_type);

  /* Создаем задание. */
  task = g_new (HyScanMarkModelTask, 1);
  task->action = action;
  task->id = (id != NULL) ? g_strdup (id) : NULL;
  task->mark = klass->object_copy (mark);

  g_type_class_unref (klass);

  /* Отправляем задание в очередь. */
  g_mutex_lock (&priv->tasks_lock);
  priv->tasks = g_slist_append (priv->tasks, task);
  g_mutex_unlock (&priv->tasks_lock);

  /* Сигнализируем об изменениях.
   * Документация гласит, что блокировать мьютекс хорошо, но не обязательно. */
  g_atomic_int_set (&priv->im_flag, 1);
  g_cond_signal (&priv->im_cond);
}

/* Функция освобождает задание. */
static void
hyscan_mark_model_free_task (gpointer _task,
                             gpointer _mdata)
{
  HyScanMarkModelTask *task = _task;
  HyScanMarkData *mdata = _mdata;

  hyscan_mark_data_destroy (mdata, task->mark);
  g_free (task->id);
  g_free (task);
}

/* Функция выполняет задание. */
static void
hyscan_mark_model_do_task (gpointer _task,
                           gpointer _mdata)
{
  HyScanMarkModelTask *task = _task;
  HyScanMarkData *mdata = _mdata;

  switch (task->action)
    {
    case MARK_ADD:
      if (!hyscan_mark_data_add (mdata, task->mark, NULL))
        g_warning ("Failed to add mark");
      break;

    case MARK_MODIFY:
      if (!hyscan_mark_data_modify (mdata, task->id, task->mark))
        g_warning ("Failed to modify mark <%s>", task->id);
      break;

    case MARK_REMOVE:
      if (!hyscan_mark_data_remove (mdata, task->id))
        g_warning ("Failed to remove mark <%s>", task->id);
      break;
    }
}

/* Функция выполняет все задания. */
static void
hyscan_mark_model_do_all_tasks (HyScanMarkModelPrivate  *priv,
                                HyScanMarkData          *data)
{
  GSList *tasks;

  /* Переписываем задания во временный список, чтобы
   * как можно меньше задерживать основной поток. */
  g_mutex_lock (&priv->tasks_lock);
  tasks = priv->tasks;
  priv->tasks = NULL;
  g_mutex_unlock (&priv->tasks_lock);

  /* Все задания отправляем в БД и очищаем наш временный список. */
  g_slist_foreach (tasks, hyscan_mark_model_do_task, data);
  g_slist_foreach (tasks, hyscan_mark_model_free_task, data);
  g_slist_free (tasks);
}

/* Функция забирает метки из БД. */
static GHashTable *
hyscan_mark_model_get_all_marks (HyScanMarkModelPrivate *priv,
                                 HyScanMarkData         *data)
{
  HyScanMark * mark;
  GHashTable * mark_list;
  gchar **id_list;
  guint len, i;

  /* Считываем список идентификаторов. Прошу обратить внимание, что
   * возврат хэш-таблицы с 0 элементов -- это нормальная ситуация, например,
   * если раньше была 1 метка, а потом её удалили. */
  mark_list = hyscan_mark_model_make_ht (priv);
  id_list = hyscan_mark_data_get_ids (data, &len);

  /* Поштучно копируем из БД в хэш-таблицу. */
  for (i = 0; i < len; i++)
    {
      mark = hyscan_mark_data_get (data, id_list[i]);
      if (mark == NULL)
        continue;

      g_hash_table_insert (mark_list, g_strdup (id_list[i]), mark);
    }

  g_strfreev (id_list);

  return mark_list;
}

/* Поток асинхронной работы с БД. */
static gpointer
hyscan_mark_model_processing (gpointer data)
{
  HyScanMarkModelPrivate *priv = data;
  HyScanMarkData *mdata = NULL; /* Объект работы с метками. */
  GHashTable *mark_list;        /* Метки из БД. */
  GHashTable *temp;             /* Для обмена списка меток. */
  GMutex im_lock;               /* Мьютекс нужен для GCond. */
  guint32 oldmc, mc;            /* Значения счетчика изменений. */

  oldmc = mc = 0;
  g_mutex_init (&im_lock);

  while (!g_atomic_int_get (&priv->stop))
    {
      /* Ждём, когда нужно действовать. */
      if (mdata != NULL)
        mc = hyscan_mark_data_get_mod_count (mdata);

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
      if (hyscan_mark_model_track_sync (priv))
        {
          /* TODO: сигнал, сообщающий о том, что сейчас поменяется mdata?
           * и что это последняя возможность запушить актуальные задачи */
          if (mdata != NULL)
            hyscan_mark_model_do_all_tasks (priv, mdata);
          g_clear_object (&mdata);
        }

      if (mdata == NULL)
        {
          /* Создаем объект работы с метками. */
          if (priv->cur_state.db != NULL && priv->cur_state.project != NULL)
            {
              mdata = g_object_new (priv->data_type,
                                    "db", priv->cur_state.db,
                                    "project", priv->cur_state.project,
                                    NULL);

              if (!hyscan_mark_data_is_ready (mdata))
                g_clear_object (&mdata);
            }

          /* Если не получилось (например потому, что проект ещё не создан),
           * повторим через некоторое время. */
          if (mdata == NULL)
            {
              g_atomic_int_set (&priv->im_flag, 1);
              g_usleep (DELAY);
              continue;
            }

          mc = hyscan_mark_data_get_mod_count (mdata);
        }

      /* Выполняем все задания. */
      hyscan_mark_model_do_all_tasks (priv, mdata);

      /* Запоминаем мод_каунт перед(!) забором меток. */
      oldmc = hyscan_mark_data_get_mod_count (mdata);
      mark_list = hyscan_mark_model_get_all_marks (priv, mdata);

      /* Воруем хэш-таблицу из привата, помещаем туда свежесозданную,
       * инициируем отправление сигнала. */
      g_mutex_lock (&priv->marks_lock);
      temp = priv->marks;
      priv->marks = g_hash_table_ref (mark_list);
      priv->marks_changed = TRUE;
      g_mutex_unlock (&priv->marks_lock);

      /* Убираем свои ссылки на хэш-таблицы. */
      g_clear_pointer (&temp, g_hash_table_unref);
      g_clear_pointer (&mark_list, g_hash_table_unref);
    }

  g_clear_object (&mdata);
  g_mutex_clear (&im_lock);
  return NULL;
}

static gboolean
hyscan_mark_model_signaller (gpointer data)
{
  HyScanMarkModel *model = data;
  HyScanMarkModelPrivate *priv = model->priv;
  gboolean marks_changed;

  /* Если есть изменения, сигнализируем. */
  g_mutex_lock (&priv->marks_lock);
  marks_changed = priv->marks_changed;
  priv->marks_changed = FALSE;
  g_mutex_unlock (&priv->marks_lock);

  if (marks_changed)
    g_signal_emit (model, hyscan_mark_model_signals[SIGNAL_CHANGED], 0, NULL);

  return G_SOURCE_CONTINUE;
}

/* Функция создает новый объект HyScanMarkModel. */
HyScanMarkModel*
hyscan_mark_model_new (GType data_type)
{
  return g_object_new (HYSCAN_TYPE_MARK_MODEL,
                       "data-type", data_type, NULL);
}

/* Функция устанавливает проект. */
void
hyscan_mark_model_set_project (HyScanMarkModel *model,
                               HyScanDB          *db,
                               const gchar       *project)
{
  HyScanMarkModelPrivate *priv;

  g_return_if_fail (HYSCAN_IS_MARK_MODEL (model));
  priv = model->priv;

  if (project == NULL || db == NULL)
    return;

  g_mutex_lock (&priv->state_lock);

  hyscan_mark_model_clear_state (&priv->new_state);

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
hyscan_mark_model_refresh (HyScanMarkModel *model)
{
  g_return_if_fail (HYSCAN_IS_MARK_MODEL (model));

  g_atomic_int_set (&model->priv->im_flag, 1);
  g_cond_signal (&model->priv->im_cond);
}

/* Функция создает метку в базе данных. */
void
hyscan_mark_model_add_mark (HyScanMarkModel  *model,
                            gconstpointer     mark)
{
  g_return_if_fail (HYSCAN_IS_MARK_MODEL (model));

  hyscan_mark_model_add_task (model, NULL, mark, MARK_ADD);
}

/* Функция изменяет метку в базе данных. */
void
hyscan_mark_model_modify_mark (HyScanMarkModel  *model,
                               const gchar      *id,
                               const HyScanMark *mark)
{
  g_return_if_fail (HYSCAN_IS_MARK_MODEL (model));

  hyscan_mark_model_add_task (model, id, mark, MARK_MODIFY);
}

/* Функция удаляет метку из базы данных. */
void
hyscan_mark_model_remove_mark (HyScanMarkModel *model,
                               const gchar     *id)
{
  g_return_if_fail (HYSCAN_IS_MARK_MODEL (model));

  hyscan_mark_model_add_task (model, id, NULL, MARK_REMOVE);
}

/* Функция возвращает список меток из внутреннего буфера. */
GHashTable*
hyscan_mark_model_get (HyScanMarkModel *model)
{
  HyScanMarkModelPrivate *priv;
  GHashTable *marks;

  g_return_val_if_fail (HYSCAN_IS_MARK_MODEL (model), NULL);
  priv = model->priv;

  g_mutex_lock (&priv->marks_lock);

  /* Проверяем, что метки есть. */
  if (priv->marks == NULL)
    {
      g_mutex_unlock (&priv->marks_lock);
      return NULL;
    }

  /* Копируем метки. */
  marks = hyscan_mark_model_copy (model, priv->marks);
  g_mutex_unlock (&priv->marks_lock);

  return marks;
}

GHashTable *
hyscan_mark_model_copy (HyScanMarkModel *model,
                        GHashTable      *src)
{
  HyScanMarkModelPrivate *priv;
  HyScanMarkDataClass *data_class;
  GHashTable *dst;
  GHashTableIter iter;
  gpointer k, v;

  g_return_val_if_fail (HYSCAN_IS_MARK_MODEL (model), NULL);
  priv = model->priv;

  if (src == NULL)
    return NULL;

  dst = hyscan_mark_model_make_ht (priv);
  data_class = g_type_class_ref (priv->data_type);

  /* Переписываем метки. */
  g_hash_table_iter_init (&iter, src);
  while (g_hash_table_iter_next (&iter, &k, &v))
    g_hash_table_insert (dst, g_strdup (k), data_class->object_copy (v));

  g_type_class_unref (data_class);

  return dst;
}
