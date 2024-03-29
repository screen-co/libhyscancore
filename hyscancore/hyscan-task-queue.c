/* hyscan-task-queue.c
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
 * SECTION: hyscan-task-queue
 * @Short_description: Очередь задач
 * @Title: HyScanTaskQueue
 *
 * HyScanTaskQueue позволяет выполнять последовательную обработку задач в порядке
 * очереди. Работа с очередью сводится к следующим шагам.
 *
 * 1. Функция hyscan_task_queue_new() создаёт новый объект очереди.
 * 2. Задачи сначала по одной регистрируются в очередь с помощью функции
 *    hyscan_task_queue_push(). В этот момент они еще не поступают в обработку.
 * 3. Чтобы передать группу зарегистрированных задач в обработку, необходимо
 *    вызвать функцию hyscan_task_queue_push_end().
 * 4. Перед началом обработки отменяются все текущие задачи, которых не оказалось
 *    в новой группе. Идентичность задач определяется функцией #GCompareFunc,
 *    переданной при создании очереди.
 * 5. Шаги 1-3 повторяются.
 * 6. Для завершения работы очереди необходимо вызвать hyscan_task_queue_shutdown().
 *
 * В функции обработки задачи #HyScanTaskQueueFunc рекомендуется реализовать
 * возможность отмены обработки при помощи объекта #GCancellable. Это сделает
 * работу очереди более отзывчивой.
 *
 * Если задача, передаваемая в #HyScanTaskQueue, не наследует класс #GObject,
 * то следует пользоваться функциями:
 * - hyscan_task_queue_new_full() - создание очереди задач,
 * - hyscan_task_queue_push_full() - добавление задачи в очередь.
 *
 */

#include "hyscan-task-queue.h"

enum
{
  PROP_O,
  PROP_TASK_FUNC,
  PROP_USER_DATA,
  PROP_CMP_FUNC,
  PROP_DESTROY_FUNC
};

/* Обертка для пользовательской задачи. */
typedef struct
{
  GObject            *task;                 /* Объект задачи. */
  GCompareFunc        cmp_func;             /* Функция для сравнения двух объектов. */
  GDestroyNotify      destroy_func;         /* Функция удаления данных задачи. */
  GCancellable       *cancellable;          /* Объект для отмены этой задачи. */
} HyScanTaskQueueWrap;

struct _HyScanTaskQueuePrivate
{
  HyScanTaskQueueFunc task_func;            /* Функция выполнения задачи. */
  GCompareFunc        cmp_func;             /* Функция сравнения двух задач. */
  GDestroyNotify      destroy_func;         /* Функция удаления данных задачи. */
  guint               max_concurrent;       /* Максимальное количество задач, выполняемых одновременно. */

  /* Состояние очереди задач. */
  GQueue             *prequeue;             /* Предварительная очередь. */
  GQueue             *queue;                /* Очередь задач, ожидающих обработки. */
  GList              *processing_tasks;     /* Список задач, находящихся в обработке. */
  guint               processing_count;     /* Длина списка processing_tasks. */
  GMutex              queue_lock;           /* Блокировка доступа к полям очереди. */

  GThreadPool        *pool;                 /* Пул обработки задач. */
  gpointer            user_data;            /* Пользовательские данные для пула задач. */
  gboolean            shutdown;             /* Признак завершения работы. */
};

static void     hyscan_task_queue_set_property             (GObject               *object,
                                                            guint                  prop_id,
                                                            const GValue          *value,
                                                            GParamSpec            *pspec);
static void     hyscan_task_queue_object_constructed       (GObject               *object);
static void     hyscan_task_queue_object_finalize          (GObject               *object);
static void     hyscan_task_queue_free_wrap                (gpointer               data);
static void     hyscan_task_queue_process                  (HyScanTaskQueueWrap   *wrap,
                                                            HyScanTaskQueue       *queue);
static gboolean hyscan_task_queue_try_next                 (HyScanTaskQueue       *queue);
static gint     hyscan_task_queue_cmp                      (HyScanTaskQueueWrap   *a,
                                                            HyScanTaskQueueWrap   *b);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanTaskQueue, hyscan_task_queue, G_TYPE_OBJECT)

static void
hyscan_task_queue_class_init (HyScanTaskQueueClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_task_queue_set_property;

  object_class->constructed = hyscan_task_queue_object_constructed;
  object_class->finalize = hyscan_task_queue_object_finalize;

  g_object_class_install_property (object_class, PROP_TASK_FUNC,
                                   g_param_spec_pointer ("task-func", "Task function", "GFunc",
                                   G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_DESTROY_FUNC,
                                   g_param_spec_pointer ("destroy-func", "Task destruction function", "GDestroyNotify",
                                   G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_USER_DATA,
                                   g_param_spec_pointer ("task-data", "User data", "",
                                   G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_CMP_FUNC,
                                   g_param_spec_pointer ("cmp-func", "Compare function", "GCompareFunc",
                                   G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_task_queue_init (HyScanTaskQueue *task_queue)
{
  task_queue->priv = hyscan_task_queue_get_instance_private (task_queue);
}

static void
hyscan_task_queue_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  HyScanTaskQueue *task_queue = HYSCAN_TASK_QUEUE (object);
  HyScanTaskQueuePrivate *priv = task_queue->priv;

  switch (prop_id)
    {
    case PROP_TASK_FUNC:
      priv->task_func = g_value_get_pointer (value);
      break;

    case PROP_DESTROY_FUNC:
      priv->destroy_func = g_value_get_pointer (value);
      break;

    case PROP_USER_DATA:
      priv->user_data = g_value_get_pointer (value);
      break;

    case PROP_CMP_FUNC:
      priv->cmp_func = g_value_get_pointer (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_task_queue_object_constructed (GObject *object)
{
  HyScanTaskQueue *task_queue = HYSCAN_TASK_QUEUE (object);
  HyScanTaskQueuePrivate *priv = task_queue->priv;

  G_OBJECT_CLASS (hyscan_task_queue_parent_class)->constructed (object);

  priv->max_concurrent = g_get_num_processors ();
  priv->pool = g_thread_pool_new ((GFunc) hyscan_task_queue_process, task_queue,
                                  priv->max_concurrent, FALSE, NULL);
  priv->prequeue = g_queue_new ();
  priv->queue = g_queue_new ();

  g_mutex_init (&priv->queue_lock);
}

static void
hyscan_task_queue_object_finalize (GObject *object)
{
  HyScanTaskQueue *task_queue = HYSCAN_TASK_QUEUE (object);
  HyScanTaskQueuePrivate *priv = task_queue->priv;

  if (!g_atomic_int_get (&priv->shutdown))
    g_warning ("HyScanTaskQueue: hyscan_task_queue_shutdown() must be called before finalize");

  /* Блокируем очередь и освобождаем память. */
  g_mutex_lock (&priv->queue_lock);

  g_queue_free_full (priv->prequeue, hyscan_task_queue_free_wrap);
  g_queue_free_full (priv->queue, hyscan_task_queue_free_wrap);

  g_mutex_unlock (&priv->queue_lock);

  /* Придётся подождать, пока не завершится выполнение уже отправленных задач. */
  g_thread_pool_free (priv->pool, FALSE, TRUE);

  g_mutex_clear (&priv->queue_lock);

  G_OBJECT_CLASS (hyscan_task_queue_parent_class)->finalize (object);
}

/* Освобождает память, занятую структурой HyScanTaskQueueWrap. */
static void
hyscan_task_queue_free_wrap (gpointer data)
{
  HyScanTaskQueueWrap *wrap = data;

  if (wrap->destroy_func != NULL)
    wrap->destroy_func (wrap->task);
  g_object_unref (wrap->cancellable);
  g_free (wrap);
}

/* Обработка задачи из очереди. */
static void
hyscan_task_queue_process (HyScanTaskQueueWrap *wrap,
                           HyScanTaskQueue     *queue)
{
  HyScanTaskQueuePrivate *priv = queue->priv;

  /* Выполняем задачу. */
  priv->task_func (wrap->task, priv->user_data, wrap->cancellable);

  g_mutex_lock (&priv->queue_lock);

  /* Удаляем задачу из очереди. */
  priv->processing_tasks = g_list_remove (priv->processing_tasks, wrap);
  priv->processing_count--;

  /* Пробуем обработать следующую задачу. */
  hyscan_task_queue_try_next (queue);

  g_mutex_unlock (&priv->queue_lock);

  /* Освобождаем память от задачи. */
  hyscan_task_queue_free_wrap (wrap);
}

/* Пробует отправить на обработку следующую задачу. Возвращает %TRUE, если отправила.
 * Функция должна вызываться за мьютексом priv->queue_lock. */
static gboolean
hyscan_task_queue_try_next (HyScanTaskQueue *queue)
{
  HyScanTaskQueuePrivate *priv = queue->priv;
  GError *error = NULL;
  HyScanTaskQueueWrap *wrap;

  /* Объект находится в стадии завершения, больше не добавляем задачи. */
  if (g_atomic_int_get (&priv->shutdown))
    return FALSE;

  /* Не обрабатываем одновременно более n задач. */
  if (priv->processing_count > priv->max_concurrent)
    return FALSE;

  /* Добавляем задачу task из начала очереди в обработку. */
  wrap = g_queue_pop_head (priv->queue);
  if (wrap == NULL)
    return FALSE;

  priv->processing_tasks = g_list_append (priv->processing_tasks, wrap);
  priv->processing_count++;
  if (!g_thread_pool_push (priv->pool, wrap, &error))
    {
      g_warning ("HyScanTaskQueue: %s", error->message);
      g_clear_error (&error);
    }

  return TRUE;
}

/* Сравнивает задачи a и b. Возвращает 0, если они одинаковые. */
static gint
hyscan_task_queue_cmp (HyScanTaskQueueWrap *a,
                       HyScanTaskQueueWrap *b)
{
  return a->cmp_func (a->task, b->task);
}

/**
 * hyscan_task_queue_new:
 * @task_func: функция обработки задачи
 * @user_data: пользовательские данные
 * @cmp_func: функция сравнения задач
 *
 * Создаёт новый объект #HyScanTaskQueue. Очередь будет использовать функцию
 * @task_func для обработки поступающих задач, передавая ей в качестве параметров
 * задачу и пользовательские данные @user_data. Функция @cmp_func позволяет
 * не добавлять повторно в очередь задачу, которая там уже есть.
 *
 * Returns: #HyScanTaskQueue. Перед удалением необходимо завершить работу очереди
 * с помощью #hyscan_task_queue_shutdown.
 */
HyScanTaskQueue *
hyscan_task_queue_new (HyScanTaskQueueFunc task_func,
                       gpointer            user_data,
                       GCompareFunc        cmp_func)
{
  return g_object_new (HYSCAN_TYPE_TASK_QUEUE,
                       "destroy-func", g_object_unref,
                       "task-func", task_func,
                       "task-data", user_data,
                       "cmp-func", cmp_func,
                       NULL);
}

/**
 * hyscan_task_queue_new_full:
 * @task_func: функция обработки задачи
 * @user_data: пользовательские данные
 * @cmp_func: функция сравнения задач
 * @destroy_func: функция удаления задачи
 *
 * Создаёт новый объект #HyScanTaskQueue. Очередь будет использовать функцию
 * @task_func для обработки поступающих задач, передавая ей в качестве параметров
 * задачу и пользовательские данные @user_data. Функция @cmp_func позволяет
 * не добавлять повторно в очередь задачу, которая там уже есть.
 *
 * Returns: #HyScanTaskQueue. Перед удалением необходимо завершить работу очереди
 * с помощью #hyscan_task_queue_shutdown.
 */
HyScanTaskQueue *
hyscan_task_queue_new_full (HyScanTaskQueueFunc task_func,
                            gpointer            user_data,
                            GCompareFunc        cmp_func,
                            GDestroyNotify      destroy_func)
{
  return g_object_new (HYSCAN_TYPE_TASK_QUEUE,
                       "task-func", task_func,
                       "destroy-func", destroy_func,
                       "task-data", user_data,
                       "cmp-func", cmp_func,
                       NULL);
}

/**
 * hyscan_task_queue_push:
 * @queue: указатель на #HyScanTaskQueue
 * @task: указатель на объект задачи #GObject
 *
 * Добавляет задачу в предварительную очередь.
 */
void
hyscan_task_queue_push (HyScanTaskQueue *queue,
                        GObject         *task)
{
  g_return_if_fail (HYSCAN_IS_TASK_QUEUE (queue));

  hyscan_task_queue_push_full (queue, g_object_ref (task));
}

/**
 * hyscan_task_queue_push_full:
 * @queue: указатель на #HyScanTaskQueue
 * @task: указатель на объект задачи
 *
 * Добавляет задачу в предварительную очередь.
 */
void
hyscan_task_queue_push_full (HyScanTaskQueue *queue,
                             gpointer         task)
{
  HyScanTaskQueueWrap *wrapper;
  HyScanTaskQueuePrivate *priv;

  g_return_if_fail (HYSCAN_IS_TASK_QUEUE (queue));
  priv = queue->priv;

  wrapper = g_new (HyScanTaskQueueWrap, 1);
  wrapper->cancellable = g_cancellable_new ();
  wrapper->cmp_func = priv->cmp_func;
  wrapper->destroy_func = priv->destroy_func;
  wrapper->task = task;

  g_queue_push_tail (queue->priv->prequeue, wrapper);
}

/**
 * hyscan_task_queue_push_end:
 * @queue: указатель на #HyScanTaskQueue
 *
 * Завершает добавление в очередь и запускает обработку задач из сформированной
 * очереди.
 */
void
hyscan_task_queue_push_end (HyScanTaskQueue *queue)
{
  HyScanTaskQueuePrivate *priv;
  GList *wrap_l;
  HyScanTaskQueueWrap *wrap;

  g_return_if_fail (HYSCAN_IS_TASK_QUEUE (queue));
  priv = queue->priv;

  /* Блокируем другим потокам доступ к очереди задач. */
  g_mutex_lock (&priv->queue_lock);

  /* Отменяем задачи в обработке, которых нет в новой очереди, и оставляем те, что есть. */
  for (wrap_l = priv->processing_tasks; wrap_l != NULL; wrap_l = wrap_l->next)
    {
      GList *found;
      wrap = wrap_l->data;

      found = g_queue_find_custom (priv->prequeue, wrap, (GCompareFunc) hyscan_task_queue_cmp);
      if (found != NULL)
        {
          hyscan_task_queue_free_wrap (found->data);
          g_queue_delete_link (priv->prequeue, found);
        }
      else
        {
          g_cancellable_cancel (wrap->cancellable);
        }
    }

  /* Копируем всё остальное из предварительной очереди в настоящую. */
  g_queue_free_full (priv->queue, hyscan_task_queue_free_wrap);
  priv->queue = g_queue_copy (priv->prequeue);

  /* Очищаем предварительную очередь. */
  g_queue_free (priv->prequeue);
  priv->prequeue = g_queue_new ();

  /* Запускаем задачи, пока они запускаются. */
  while (hyscan_task_queue_try_next (queue))
    ;

  /* Операции с очередью сделаны — разблокируем доступ. */
  g_mutex_unlock (&priv->queue_lock);
}

/**
 * hyscan_task_queue_processing:
 * @queue: указатель на #HyScanTaskQueue
 *
 * Проверяет статус обработки задач. Функция возвращает %FALSE, если обработка
 * всех ранее поступивших задач была завершена.
 *
 * Returns: %TRUE, если некоторые задачи ещё обрабатываются; %FALSE, если
 *   обработка задач завершена.
 */
gboolean
hyscan_task_queue_processing (HyScanTaskQueue *queue)
{
  HyScanTaskQueuePrivate *priv;
  gboolean processing;

  g_return_val_if_fail (HYSCAN_IS_TASK_QUEUE (queue), FALSE);
  priv = queue->priv;

  g_mutex_lock (&priv->queue_lock);
  processing = (priv->processing_count > 0);
  g_mutex_unlock (&priv->queue_lock);

  return processing;
}

/**
 * hyscan_task_queue_shutdown:
 * @queue: указатель на #HyScanTaskQueue
 *
 * Останавливает выполнение задач в очереди. Должна быть вызвана перед
 * удалением объекта g_object_unref().
 */
void
hyscan_task_queue_shutdown (HyScanTaskQueue *queue)
{
  HyScanTaskQueuePrivate *priv;
  GList *task_l;
  HyScanTaskQueueWrap *wrap;

  g_return_if_fail (HYSCAN_IS_TASK_QUEUE (queue));
  priv = queue->priv;

  /* Ставим флаг о завершении работы; после этого новые задачи поступать не будут. */
  g_atomic_int_set (&priv->shutdown, TRUE);

  /* Отменяем все текущие задачи. */
  g_mutex_lock (&priv->queue_lock);
  for (task_l = priv->processing_tasks; task_l != NULL; task_l = task_l->next)
    {
      wrap = task_l->data;
      g_cancellable_cancel (wrap->cancellable);
    }
  g_mutex_unlock (&priv->queue_lock);
}
