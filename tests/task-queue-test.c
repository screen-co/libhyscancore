#include <hyscan-task-queue.h>

static gchar *test_user_data = "user data";
static guint  created_tasks = 0;

/* Специально оставляю это здесь, чтобы понимать, как пользоваться
 * в случае GObject-тасков
void
task_func (GObject      *task,
           gpointer      user_data,
           GCancellable *cancellable)
{
  g_assert_true (test_user_data == user_data);
  g_message ("Processing task %s", (gchar *) g_object_get_data (task, "label"));
}

void
del_task (gpointer task)
{
  g_atomic_int_add (&created_tasks, -1);

  g_free (task);
}

GObject *
new_task (gint i)
{
  GObject *task;
  g_atomic_int_add (&created_tasks, 1);

  task = g_object_new (G_TYPE_OBJECT, NULL);
  g_object_set_data_full (task, "label", g_strdup_printf ("task %d", i), del_task);

  return task;
}
*/

void
task_func (gpointer      task,
           gpointer      user_data,
           GCancellable *cancellable)
{
  g_assert_true (test_user_data == user_data);
  g_message ("Processing task %s", (gchar *) task);
}

void
del_task (gpointer task)
{
  g_atomic_int_add (&created_tasks, -1);
  g_free (task);
}

gpointer
new_task (gint i)
{
  ++created_tasks;
  return g_strdup_printf ("task %d", i);
}

int
main (int    argc,
      char **argv)
{
  HyScanTaskQueue *queue;
  gint i;
  gint n = 1000;

  queue = hyscan_task_queue_new_full (task_func, test_user_data, (GCompareFunc) g_strcmp0, del_task);

  for (i = 0; i < n; ++i) {
      gpointer task;

      task = new_task (i);
      hyscan_task_queue_push_full (queue, task);
  }

  hyscan_task_queue_push_end (queue);

  g_usleep (G_USEC_PER_SEC);
  hyscan_task_queue_shutdown (queue);
  g_object_unref (queue);

  g_assert_cmpint (g_atomic_int_get (&created_tasks), ==, 0);

  return 0;
}
