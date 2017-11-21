#include <hyscan-data-writer.h>
#include <hyscan-waterfall-mark-data.h>
#include <glib/gprintf.h>
#include <stdio.h>
#include <hyscan-waterfall-mark.h>

#define N_MARKS 4

gboolean  mark_lookup (HyScanWaterfallMark  *mark);

gboolean  make_track  (HyScanDB *db,
                       gchar    *name);

HyScanWaterfallMark test_marks[N_MARKS] =
{
  {"gals", "test-mark", "this mark is for testing purposes", "tester", 12345678,
    100, 10, HYSCAN_SOURCE_SIDE_SCAN_PORT, 0, 1, 10},
  {"gals", "ac dc", "i've got some rock'n'roll thunder", "rocker", 87654321,
    200, 20, HYSCAN_SOURCE_SIDE_SCAN_STARBOARD, 2, 3, 32},
  {"gals", "rolling stones", "all i hear is doom and gloom", "rocker", 2468,
    300, 30, HYSCAN_SOURCE_SIDE_SCAN_STARBOARD, 4, 5, 54},
  {"gals", "modified mark", "this mark was modified", "modder", 1357,
    400, 40, HYSCAN_SOURCE_SIDE_SCAN_STARBOARD, 6, 7, 76}
};

int
main (int argc, char **argv)
{
  HyScanDB *db = NULL;                   /* БД. */
  gchar *db_uri = "file://./";           /* Путь к БД. */
  gchar *name = "test";                  /* Проект и галс. */

  HyScanWaterfallMarkData *data = NULL;
  HyScanWaterfallMark *mark;

  gchar **list;
  guint list_len;
  guint i;

  {
    gchar **args;

    #ifdef G_OS_WIN32
      args = g_win32_get_command_line ();
    #else
      args = g_strdupv (argv);
    #endif

    if (argc == 2)
      db_uri = g_strdup (args[1]);
    g_strfreev (args);
  }

  /* Откроем БД, создадим проект и галс. */
  db = hyscan_db_new (db_uri);
  if (db == NULL)
    g_error ("Can't open db at %s", db_uri);

  make_track (db, name);

  /* Начинаем тестирование объекта. */
  data = hyscan_waterfall_mark_data_new (db, name);

  /* Отправим несколько меток. */
  g_message ("Adding marks...");
  hyscan_waterfall_mark_data_add (data, &test_marks[0]);
  hyscan_waterfall_mark_data_add (data, &test_marks[1]);
  hyscan_waterfall_mark_data_add (data, &test_marks[2]);

  /* Проверим, что получилось. */
  list = hyscan_waterfall_mark_data_get_ids (data, &list_len);
  for (i = 0; i < list_len; i++)
    {
      mark = hyscan_waterfall_mark_data_get (data, list[i]);
      if (mark == NULL || !mark_lookup (mark))
        g_error ("Failed to get mark <%s>", list[i]);
      else
        hyscan_waterfall_mark_free (mark);
    }

  /* Изменяем какую-то метку. */
  g_message ("Modifying mark...");
  hyscan_waterfall_mark_data_modify (data, list[1], &test_marks[3]);

  g_strfreev (list);
  list = hyscan_waterfall_mark_data_get_ids (data, &list_len);
  for (i = 0; i < list_len; i++)
    {
      mark = hyscan_waterfall_mark_data_get (data, list[i]);
      if (mark == NULL || !mark_lookup (mark))
        g_error ("Failed to get mark <%s>", list[i]);
      else
        hyscan_waterfall_mark_free (mark);
    }

  /* Удаляем метку. */
  g_message ("Removing mark...");
  hyscan_waterfall_mark_data_remove (data, list[2]);

  g_strfreev (list);
  list = hyscan_waterfall_mark_data_get_ids (data, &list_len);
  for (i = 0; i < list_len; i++)
    {
      mark = hyscan_waterfall_mark_data_get (data, list[i]);
      if (mark == NULL || !mark_lookup (mark))
        g_error ("Failed to get mark <%s>", list[i]);
      else
        hyscan_waterfall_mark_free (mark);
    }

  hyscan_db_project_remove (db, name);

  g_clear_object (&data);
  g_clear_object (&db);

  g_message ("Test passed!");
  return 0;
}

gboolean
mark_lookup (HyScanWaterfallMark *mark)
{
  gint i;

  for (i = 0; i < N_MARKS; i++)
    {
      if (0 == g_strcmp0 (mark->name,             test_marks[i].name) &&
          0 == g_strcmp0 (mark->description,      test_marks[i].description) &&
          0 == g_strcmp0 (mark->operator_name,    test_marks[i].operator_name) &&
          mark->labels                         == test_marks[i].labels &&
          mark->creation_time                  == test_marks[i].creation_time &&
          mark->modification_time              == test_marks[i].modification_time &&
          mark->source0                        == test_marks[i].source0 &&
          mark->index0                         == test_marks[i].index0 &&
          mark->count0                         == test_marks[i].count0 &&
          mark->width                          == test_marks[i].width &&
          mark->height                         == test_marks[i].height)
        {
          return TRUE;
        }
    }

  return FALSE;
}

gboolean
make_track (HyScanDB *db,
            gchar    *name)
{
  HyScanDataWriter *writer = hyscan_data_writer_new (db);
  gint i;

  if (!hyscan_data_writer_set_project (writer, name))
    g_error ("Couldn't set data writer project.");
  if (!hyscan_data_writer_start (writer, name, HYSCAN_TRACK_SURVEY))
    g_error ("Couldn't start data writer.");

  /* Запишем что-то в галс. */
  for (i = 0; i < 2; i++)
    {
      HyScanAcousticDataInfo info = {.data.type = HYSCAN_DATA_COMPLEX_FLOAT, .data.rate = 1.0}; /* Информация о датчике. */
      HyScanDataWriterData data;
      HyScanComplexFloat *vals = g_malloc0 (2 * sizeof (HyScanComplexFloat));

      data.time = 1 + i * 10;
      data.data = vals;
      data.size = 2 * sizeof (HyScanComplexFloat);

      hyscan_data_writer_acoustic_add_data (writer, HYSCAN_SOURCE_SIDE_SCAN_PORT, &info, &data);

      g_free (vals);
    }

  g_object_unref (writer);
  return TRUE;
}
