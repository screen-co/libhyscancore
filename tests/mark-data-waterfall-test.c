#include <hyscan-data-writer.h>
#include <hyscan-object-data-wfmark.h>
#include <glib/gprintf.h>
#include <stdio.h>
#include <hyscan-mark.h>

#define N_MARKS 4

gboolean  mark_lookup (HyScanMarkWaterfall  *mark);

gboolean  make_track  (HyScanDB *db,
                       gchar    *name);

HyScanMarkWaterfall test_marks[N_MARKS] =
{
  {HYSCAN_MARK_WATERFALL, "test-mark",      "this mark is for testing purposes", "tester", 12345678,
   100, 10, 1, 10, "gals",
   "HYSCAN_SOURCE_SIDE_SCAN_PORT", 0},
  {HYSCAN_MARK_WATERFALL, "ac dc",          "i've got some rock'n'roll thunder", "rocker", 87654321,
   200, 20, 3, 32, "gals",
   "HYSCAN_SOURCE_SIDE_SCAN_STARBOARD", 2},
  {HYSCAN_MARK_WATERFALL, "rolling stones", "all i hear is doom and gloom",      "rocker", 2468,
   300, 30, 5, 54, "gals",
   "HYSCAN_SOURCE_SIDE_SCAN_STARBOARD", 4},
  {HYSCAN_MARK_WATERFALL, "modified mark",  "this mark was modified",            "modder", 1357,
   400, 40, 7, 76, "gals",
   "HYSCAN_SOURCE_SIDE_SCAN_STARBOARD", 6}
};

int
main (int argc, char **argv)
{
  HyScanDB *db = NULL;                   /* БД. */
  gchar *db_uri = "file://./";           /* Путь к БД. */
  gchar *name = "test";                  /* Проект и галс. */

  HyScanObjectData *data = NULL;
  HyScanMarkWaterfall *mark;

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
  data = g_object_new (HYSCAN_TYPE_OBJECT_DATA_WFMARK, "db", db, "project", name, NULL);

  /* Отправим несколько меток. */
  g_message ("Adding marks...");
  hyscan_object_data_add (data, (HyScanObject *) &test_marks[0], NULL);
  hyscan_object_data_add (data, (HyScanObject *) &test_marks[1], NULL);
  hyscan_object_data_add (data, (HyScanObject *) &test_marks[2], NULL);

  /* Проверим, что получилось. */
  list = hyscan_object_data_get_ids (data, &list_len);
  for (i = 0; i < list_len; i++)
    {
      mark = (HyScanMarkWaterfall *) hyscan_object_data_get (data, list[i]);
      if (mark == NULL || !mark_lookup (mark))
        g_error ("Failed to get mark <%s>", list[i]);
      else
        hyscan_mark_waterfall_free (mark);
    }

  /* Изменяем какую-то метку. */
  g_message ("Modifying mark...");
  hyscan_object_data_modify (data, list[1], (const HyScanObject *) &test_marks[3]);

  g_strfreev (list);
  list = hyscan_object_data_get_ids (data, &list_len);
  for (i = 0; i < list_len; i++)
    {
      mark = (HyScanMarkWaterfall *) hyscan_object_data_get (data, list[i]);
      if (mark == NULL || !mark_lookup (mark))
        g_error ("Failed to get mark <%s>", list[i]);
      else
        hyscan_mark_waterfall_free (mark);
    }

  /* Удаляем метку. */
  g_message ("Removing mark...");
  hyscan_object_data_remove (data, list[2]);

  g_strfreev (list);
  list = hyscan_object_data_get_ids (data, &list_len);
  for (i = 0; i < list_len; i++)
    {
      mark = (HyScanMarkWaterfall *) hyscan_object_data_get (data, list[i]);
      if (mark == NULL || !mark_lookup (mark))
        g_error ("Failed to get mark <%s>", list[i]);
      else
        hyscan_mark_waterfall_free (mark);
    }
  g_strfreev (list);

  hyscan_db_project_remove (db, name);

  g_clear_object (&data);
  g_clear_object (&db);

  g_message ("Test passed!");
  return 0;
}

gboolean
mark_lookup (HyScanMarkWaterfall *mark)
{
  gint i;

  for (i = 0; i < N_MARKS; i++)
    {
      HyScanMarkWaterfall *wf_mark = (HyScanMarkWaterfall *) &test_marks[i];
      if (0 == g_strcmp0 (mark->name,          wf_mark->name) &&
          0 == g_strcmp0 (mark->description,   wf_mark->description) &&
          0 == g_strcmp0 (mark->operator_name, wf_mark->operator_name) &&
          0 == g_strcmp0 (mark->source,        wf_mark->source) &&
          mark->labels == wf_mark->labels &&
          mark->ctime  == wf_mark->ctime &&
          mark->mtime  == wf_mark->mtime &&
          mark->index  == wf_mark->index &&
          mark->count  == wf_mark->count &&
          mark->width  == wf_mark->width &&
          mark->height == wf_mark->height)
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
  HyScanAcousticDataInfo info = {.data_type = HYSCAN_DATA_FLOAT, .data_rate = 1.0};
  HyScanDataWriter *writer = hyscan_data_writer_new ();
  HyScanBuffer *buffer = hyscan_buffer_new ();
  gint i;

  hyscan_data_writer_set_db (writer, db);
  if (!hyscan_data_writer_start (writer, name, name, HYSCAN_TRACK_SURVEY, -1))
    g_error ("Couldn't start data writer.");

  /* Запишем что-то в галс. */
  for (i = 0; i < 2; i++)
    {
      gfloat vals = {0};
      hyscan_buffer_wrap_float (buffer, &vals, 1);
      hyscan_data_writer_acoustic_add_data (writer, HYSCAN_SOURCE_SIDE_SCAN_PORT,
                                            1, FALSE, 1 + i, &info, buffer);
    }

  g_object_unref (writer);
  g_object_unref (buffer);
  return TRUE;
}