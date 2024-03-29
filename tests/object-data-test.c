/* object-data-test.c
 *
 * Copyright 2017-2019 Screen LLC, Dmitriev Alexander <m1n7@yandex.ru>
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

#include <hyscan-object-data-wfmark.h>
#include <hyscan-object-data-geomark.h>
#include <hyscan-data-writer.h>

#define N_TEST_DATA   4           /* Длина массива с тестовыми данными. */
#define PROJECT_NAME  "test"      /* Имя проекта. */
#define TRACK_NAME    "test"      /* Имя галса. */
#define OBJECT_NAME   "test"      /* Название объекта. */

#define list_nth_id(list, n) (((HyScanObjectId *) g_list_nth_data ((list), (n)))->id)

HyScanDB      *db;                /* БД. */
gchar         *db_uri;            /* Путь к БД. */

gboolean          object_lookup (HyScanObject   *mark,
                                 HyScanObject  **marks);

gboolean          make_track    (void);

void              check_object  (HyScanObjectStore *data,
                                 GType              object_type,
                                 const gchar       *id,
                                 HyScanObject     **objects);

HyScanMarkWaterfall test_marks_wf[N_TEST_DATA] =
{
  {0, "test-mark",      "this mark is for testing purposes", "tester", 12345678,
   100, 10, 1, 10, "gals",
   "HYSCAN_SOURCE_SIDE_SCAN_PORT", 0},
  {0, "ac dc",          "i've got some rock'n'roll thunder", "rocker", 87654321,
   200, 20, 3, 32, "gals",
   "HYSCAN_SOURCE_SIDE_SCAN_STARBOARD", 2},
  {0, "rolling stones", "all i hear is doom and gloom",      "rocker", 2468,
   300, 30, 5, 54, "gals",
   "HYSCAN_SOURCE_SIDE_SCAN_STARBOARD", 4},
  {0, "modified mark",  "this mark was modified",            "modder", 1357,
   400, 40, 7, 76, "gals",
   "HYSCAN_SOURCE_SIDE_SCAN_STARBOARD", 6}
};

HyScanMarkGeo test_marks_geo[N_TEST_DATA] =
{
  {0, "test-mark",      "this mark is for testing purposes", "tester", 12345678,
   100, 10, 1, 10, {10, 20}},
  {0, "ac dc",          "i've got some rock'n'roll thunder", "rocker", 87654321,
   200, 20, 3, 32, {30, 40}},
  {0, "rolling stones", "all i hear is doom and gloom",      "rocker", 2468,
   300, 30, 5, 54, {50, 60}},
  {0, "modified mark",  "this mark was modified",            "modder", 1357,
   400, 40, 7, 76, {70, 80}}
};

/* Функция тестирует класс. */
void
test_class (GType          gtype,
            const gchar   *type_name,
            GType          object_type,
            HyScanObject **objects)
{
  HyScanObjectStore *data = NULL;
  HyScanObject *object;

  GList *list, *link;
  guint list_len;
  guint i;
  guint add_n = 3;
  guint32 mod_count;

  g_message ("Test %s...", type_name);

  /* Создаём экземпляр класса для тестирования. */
  data = HYSCAN_OBJECT_STORE (hyscan_object_data_new (gtype));
  if (!hyscan_object_data_project_open (HYSCAN_OBJECT_DATA (data), db, PROJECT_NAME))
    g_error ("Failed to open project %s", PROJECT_NAME);

  /* Тестируем копирование структуры. */
  object = hyscan_object_copy (objects[0]);
  if (object == NULL || !hyscan_object_equal (object, objects[0]))
    g_error ("Object copy is not equal to the source");
  g_clear_pointer (&object, hyscan_object_free);

  /* Отправим несколько объектов в БД. */
  g_message ("Adding objects...");
  for (i = 0; i < add_n; i++)
    hyscan_object_store_add (data, objects[0], NULL);

  /* Запоминаем мод-каунт. */
  mod_count = hyscan_object_store_get_mod_count (data, object_type);

  /* Проверим, что получилось. */
  list = hyscan_object_store_get_ids (data);
  list_len = g_list_length (list);
  if (list_len != add_n)
    g_error ("Expected %d objects, but got %d", add_n, list_len);

  for (link = list; link != NULL; link = link->next)
    {
      HyScanObjectId *object_id = link->data;
      check_object (data, object_type, object_id->id, objects);
    }

  /* Проверим, что мод-каунт не поменялся. */
  if (mod_count != hyscan_object_store_get_mod_count (data, object_type))
    g_error ("Mod count has changed unexpectedly");

  /* Изменяем какую-то метку. */
  g_message ("Modifying object...");
  hyscan_object_store_modify (data, list_nth_id (list, 1), (const HyScanObject *) objects[3]);

  /* Теперь мод-каунт должен был измениться. */
  if (mod_count == hyscan_object_store_get_mod_count (data, object_type))
    g_error ("Mod count has not changed after modify call");

  g_list_free_full (list, (GDestroyNotify) hyscan_object_id_free);
  list = hyscan_object_store_get_ids (data);
  for (link = list; link != NULL; link = link->next)
    {
      HyScanObjectId *object_id = link->data;
      check_object (data, object_type, object_id->id, objects);
    }

  /* Удаляем метку. */
  g_message ("Removing mark...");
  hyscan_object_store_remove (data, object_type, list_nth_id (list, 2));

  g_list_free_full (list, (GDestroyNotify) hyscan_object_id_free);
  list = hyscan_object_store_get_ids (data);
  for (link = list; link != NULL; link = link->next)
    {
      HyScanObjectId *object_id = link->data;
      check_object (data, object_type, object_id->id, objects);
    }
  g_list_free_full (list, (GDestroyNotify) hyscan_object_id_free);

  /* Тестируем автомат. */
  /* Сначала всё удаляем. */
  list = hyscan_object_store_get_ids (data);
  for (link = list; link != NULL; link = link->next)
    {
      HyScanObjectId *object_id = link->data;
      hyscan_object_store_remove (data, object_type, object_id->id);
    }
  g_list_free_full (list, (GDestroyNotify) hyscan_object_id_free);

  /* Объекты с генерируемым айди. */
  g_message ("Automatic management...");

  /* Создаем. */
  if (!hyscan_object_store_set (data, object_type, NULL, (const HyScanObject *) objects[0]))
    g_error ("Autoadd failed");

  {
    list = hyscan_object_store_get_ids (data);
    list_len = g_list_length (list);
    if (list_len != 1)
      g_error ("Extra objects in DB");

    object = hyscan_object_store_get (data, object_type, list_nth_id (list, 0));
    if (!hyscan_object_equal (object, objects[0]))
      g_error ("Wrong object in DB");
    g_clear_pointer (&object, hyscan_object_free);

    /* модифицирую этот объект. */
    if (!hyscan_object_store_set (data, object_type, list_nth_id (list, 0), (const HyScanObject *) objects[0]))
      g_error ("Automodify failed");

    /* Удаляю этот объект. */
    if (!hyscan_object_store_set (data, object_type, list_nth_id (list, 0), NULL))
      g_error ("Autodelete failed");

    /* Проверяю, что удалился. */
    if (NULL != (object = hyscan_object_store_get (data, object_type, list_nth_id (list, 0))))
      g_error ("Automodify failed");
    g_clear_pointer (&object, hyscan_object_free);

    g_list_free_full (list, (GDestroyNotify) hyscan_object_id_free);
  }

  /* Объекты с заданным айди. */
  /* Создаем. */
  if (!hyscan_object_store_set (data, object_type, OBJECT_NAME, (const HyScanObject *) objects[0]))
    g_error ("Autoadd failed");

  {
    list = hyscan_object_store_get_ids (data);
    list_len = g_list_length (list);
    if (list_len != 1 || !g_str_equal (OBJECT_NAME, list_nth_id (list, 0)))
      g_error ("Extra objects in DB");
    g_list_free_full (list, (GDestroyNotify) hyscan_object_id_free);

    object = hyscan_object_store_get (data, object_type, OBJECT_NAME);
    if (!hyscan_object_equal (object, objects[0]))
      g_error ("Wrong object in DB");
    g_clear_pointer (&object, hyscan_object_free);

    /* модифицирую этот объект. */
    if (!hyscan_object_store_set (data, object_type, OBJECT_NAME, (const HyScanObject *) objects[0]))
      g_error ("Automodify failed");

    /* Удаляю этот объект. */
    if (!hyscan_object_store_set (data, object_type, OBJECT_NAME, NULL))
      g_error ("Autodelete failed");

    /* Проверяю, что удалился. */
    if (NULL != (object = hyscan_object_store_get (data, object_type, OBJECT_NAME)))
      g_error ("Automodify failed");
    g_clear_pointer (&object, hyscan_object_free);
  }

  g_object_unref (data);
}

gboolean
object_lookup (HyScanObject   *mark,
               HyScanObject  **marks)
{
  gint i;

  for (i = 0; i < N_TEST_DATA; i++)
    {
      if (hyscan_object_equal (mark, marks[i]))
        return TRUE;
    }

  return FALSE;
}

void
check_object (HyScanObjectStore  *data,
              GType               object_type,
              const gchar        *id,
              HyScanObject      **objects)
{
  HyScanObject *object = hyscan_object_store_get (data, object_type, id);
  if (object == NULL || !object_lookup (object, objects))
    g_error ("Failed to get mark <%s>", id);
  else
    hyscan_object_free (object);
}

gboolean
make_track (void)
{
  HyScanAcousticDataInfo info = {.data_type = HYSCAN_DATA_FLOAT, .data_rate = 1.0};
  HyScanDataWriter *writer = hyscan_data_writer_new ();
  HyScanBuffer *buffer = hyscan_buffer_new ();
  gint i;

  hyscan_data_writer_set_db (writer, db);
  if (!hyscan_data_writer_start (writer, PROJECT_NAME, TRACK_NAME, HYSCAN_TRACK_SURVEY, NULL, -1))
    g_error ("Couldn't start data writer.");

  hyscan_data_writer_acoustic_create (writer, HYSCAN_SOURCE_SIDE_SCAN_PORT, 1,
                                      NULL, NULL, &info);

  /* Запишем что-то в галс. */
  for (i = 0; i < 2; i++)
    {
      gfloat vals = {0};
      hyscan_buffer_wrap_float (buffer, &vals, 1);
      hyscan_data_writer_acoustic_add_data (writer, HYSCAN_SOURCE_SIDE_SCAN_PORT,
                                            1, FALSE, 1 + i, buffer);
    }

  g_object_unref (writer);
  g_object_unref (buffer);
  return TRUE;
}

int
main (int argc, char **argv)
{
  HyScanObject *test_data[N_TEST_DATA];
  gint i;

  {
    gchar **args;
    GError *error = NULL;
    GOptionContext *context;

#ifdef G_OS_WIN32
    args = g_win32_get_command_line ();
#else
    args = g_strdupv (argv);
#endif

    context = g_option_context_new ("<db-uri>");
    g_option_context_set_help_enabled (context, TRUE);
    g_option_context_set_ignore_unknown_options (context, FALSE);
    if (!g_option_context_parse_strv (context, &args, &error))
      {
        g_print ("%s\n", error->message);
        return -1;
      }

    if (g_strv_length (args) != 2)
      {
        g_print ("%s", g_option_context_get_help (context, FALSE, NULL));
        return 0;
      }

    g_option_context_free (context);

    db_uri = g_strdup (args[1]);
    g_strfreev (args);
  }

  /* Откроем БД, создадим проект и галс. */
  db = hyscan_db_new (db_uri);
  if (db == NULL)
    g_error ("Can't open db at %s", db_uri);

  make_track ();

  /* Тест класса водопадных меток. */
  for (i = 0; i < N_TEST_DATA; i++)
    {
      test_data[i] = (HyScanObject *) &test_marks_wf[i];
      test_data[i]->type = HYSCAN_TYPE_MARK_WATERFALL;
    }

  g_message ("Testing type: %ld", HYSCAN_TYPE_MARK_WATERFALL);
  test_class (HYSCAN_TYPE_OBJECT_DATA_WFMARK, "HyScanMarkWaterfall", HYSCAN_TYPE_MARK_WATERFALL, test_data);

  /* Тест класса геометок. */
  for (i = 0; i < N_TEST_DATA; i++)
    {
      test_data[i] = (HyScanObject *) &test_marks_geo[i];
      test_data[i]->type = HYSCAN_TYPE_MARK_GEO;
    }

  test_class (HYSCAN_TYPE_OBJECT_DATA_GEOMARK, "HyScanMarkGeo", HYSCAN_TYPE_MARK_GEO, test_data);

  hyscan_db_project_remove (db, PROJECT_NAME);

  g_clear_object (&db);

  g_message ("Test passed!");
  return 0;
}
