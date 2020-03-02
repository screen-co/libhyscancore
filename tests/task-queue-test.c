/* task-queue-test.c
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

#include <hyscan-task-queue.h>

#define DATA_KEY "label"                     /* Ключ таблицы данных GObject'а, где хранится идентификатор объекта. */

static gchar *test_user_data = "user data";  /* Пользовательские данные очереди. */
static guint  total_tasks = 1000;            /* Число задач для тестирования. */
static guint  created_tasks = 0;             /* Счётчик созданных задач. */
static guint  processed_tasks = 0;           /* Счётчик обработанных задач. */

static void              object_task_func          (GObject         *task,
                                                    gpointer         user_data,
                                                    GCancellable    *cancellable);
static gint              object_task_cmp           (gconstpointer    task1,
                                                    gconstpointer    task2);
static void              object_task_destroy       (gpointer         task);
static GObject *         object_task_new           (gint             i);
static void              task_func                 (gpointer         task,
                                                    gpointer         user_data,
                                                    GCancellable    *cancellable);
static void              task_destroy              (gpointer         task);
static gpointer          task_new                  (gint             i);
static void              test_queue                (HyScanTaskQueue *queue);
static void              long_task_func            (gpointer         task,
                                                    gpointer         user_data,
                                                    GCancellable    *cancellable);
static void              test_task_queue_gpointer  (void);
static void              test_task_queue_gobject   (void);
static void              test_task_queue_duplicate (void);

/* Функция обработки GObject-задания. */
static void
object_task_func (GObject      *task,
                  gpointer      user_data,
                  GCancellable *cancellable)
{
  g_assert_true (test_user_data == user_data);
  g_atomic_int_inc (&processed_tasks);
  g_message ("Processing GObject-task \"%s\"", (gchar *) g_object_get_data (task, DATA_KEY));
}

/* Функция сравнения двух GObject-заданий. */
static gint
object_task_cmp (gconstpointer task1,
                 gconstpointer task2)
{
  return g_strcmp0 (g_object_get_data (G_OBJECT (task1), DATA_KEY),
                    g_object_get_data (G_OBJECT (task2), DATA_KEY));
}

/* Функция выполняется при удалении GObject-задания. */
static void
object_task_destroy (gpointer task)
{
  g_atomic_int_add (&created_tasks, -1);

  g_free (task);
}

/* Функция создаёт новое GObject-задание. */
static GObject *
object_task_new (gint i)
{
  GObject *task;
  g_atomic_int_add (&created_tasks, 1);

  task = g_object_new (G_TYPE_OBJECT, NULL);
  g_object_set_data_full (task, DATA_KEY, g_strdup_printf ("task %d", i), object_task_destroy);

  return task;
}

/* Функция обрабатывает задание. */
static void
task_func (gpointer      task,
           gpointer      user_data,
           GCancellable *cancellable)
{
  g_assert_true (test_user_data == user_data);
  g_atomic_int_inc (&processed_tasks);
  g_message ("Processing task \"%s\"", (gchar *) task);
}

/* Функция удаляет задание. */
static void
task_destroy (gpointer task)
{
  g_atomic_int_add (&created_tasks, -1);
  g_free (task);
}

/* Функция создаёт новое задание. */
static gpointer
task_new (gint i)
{
  ++created_tasks;
  return g_strdup_printf ("task %d", i);
}

/* Тест очереди задач из gpointer'ов. */
static void
test_task_queue_gpointer (void)
{
  HyScanTaskQueue *queue;
  guint i;

  queue = hyscan_task_queue_new_full (task_func, test_user_data, (GCompareFunc) g_strcmp0, task_destroy);

  for (i = 0; i < total_tasks; ++i) {
      gpointer task;

      task = task_new (i);
      hyscan_task_queue_push_full (queue, task);
  }

  test_queue (queue);
}

/* Тест очереди задач из GObject'ов. */
static void
test_task_queue_gobject (void)
{
  HyScanTaskQueue *queue;
  guint i;

  queue = hyscan_task_queue_new ((HyScanTaskQueueFunc) object_task_func, test_user_data, object_task_cmp);

  for (i = 0; i < total_tasks; ++i) {
      gpointer task;

      task = object_task_new (i);
      hyscan_task_queue_push_full (queue, task);
  }

  test_queue (queue);
}

/* Тест проверяет, что все задачи были обработаны и удалены. */
static void
test_queue (HyScanTaskQueue *queue)
{
  processed_tasks = 0;

  /* Проверяем, что задачи созданы в нужном количестве. */
  g_assert_cmpint (created_tasks, ==, total_tasks);

  hyscan_task_queue_push_end (queue);

  /* Ждём, пока обработка задач завершится. */
  while (hyscan_task_queue_processing (queue))
    g_usleep (20 * G_TIME_SPAN_MILLISECOND);

  hyscan_task_queue_shutdown (queue);
  g_object_unref (queue);

  /* Проверяем, что все задачи обработаны и удалены. */
  g_assert_cmpint (created_tasks, ==, 0);
  g_assert_cmpint (processed_tasks, ==, total_tasks);
}

/* Функция обрабатывает задание. */
static void
long_task_func (gpointer      task,
                gpointer      user_data,
                GCancellable *cancellable)
{
  static gint processing = 0;

  g_atomic_int_inc (&processing);

  if (g_atomic_int_get (&processing) > 1)
    g_error ("The same task must not be processed in parallel.");

  g_usleep (100 * G_TIME_SPAN_MILLISECOND);

  g_atomic_int_add (&processing, -1);
}

/* Тест: добавление в очередь задачи, идентичной той, что уже обрабатывается,
 * не приводит к её повторной обработке. */
static void
test_task_queue_duplicate (void)
{
  HyScanTaskQueue *queue;

  queue = hyscan_task_queue_new_full (long_task_func, test_user_data, (GCompareFunc) g_strcmp0, g_free);

  hyscan_task_queue_push_full (queue, g_strdup ("Long task"));
  hyscan_task_queue_push_end (queue);

  hyscan_task_queue_push_full (queue, g_strdup ("Long task"));
  hyscan_task_queue_push_end (queue);

  while (hyscan_task_queue_processing (queue))
    g_usleep (20 * G_TIME_SPAN_MILLISECOND);

  hyscan_task_queue_shutdown (queue);
  g_object_unref (queue);
}

int
main (int    argc,
      char **argv)
{
  g_message ("Test task queue of gpointer's");
  test_task_queue_gpointer ();

  g_message ("Test task queue of GObject's");
  test_task_queue_gobject ();

  g_message ("Test duplicate task");
  test_task_queue_duplicate ();

  return 0;
}
