#include <hyscan-projector.h>
#include <hyscan-acoustic-data.h>
#include <glib/gprintf.h>
#include <hyscan-data-writer.h>

#define FAIL(msg) do {g_print ("%s\n", msg); goto exit;} while (0)
#define SIZE 20
#define DB_TIME_INC 1000000

int
main (int argc, char **argv)
{
  HyScanDB *db;                          /* БД. */
  gchar *db_uri = "file://./";           /* Путь к БД. */
  gchar *name = "test";                  /* Проект и галс. */
  HyScanDataWriter *writer = NULL;       /* Класс записи данных. */
  HyScanBuffer *buffer = NULL;
  HyScanAntennaOffset position;
  HyScanProjector *projector = NULL;
  HyScanAcousticData *adata;
  HyScanSoundVelocity elem;

  gint64 time = 0;                    /* Время записи строки. */
  gint i;                                /* Простой маленький счетчик.*/

  GArray *sv = NULL;

  /* Парсим аргументы. */
  {
    gchar **args;
    GError *error = NULL;
    GOptionContext *context;
    GOptionEntry entries[] = {
      {NULL}
    };

    #ifdef G_OS_WIN32
      args = g_win32_get_command_line ();
    #else
      args = g_strdupv (argv);
    #endif

    context = g_option_context_new ("<db-uri>\n Default db uri is file://./");
    g_option_context_set_help_enabled (context, TRUE);
    g_option_context_add_main_entries (context, entries, NULL);
    g_option_context_set_ignore_unknown_options (context, FALSE);
    if (!g_option_context_parse_strv (context, &args, &error))
      {
        g_message("%s", error->message);
        return -1;
      }

    if (g_strv_length (args) > 2)
      {
        g_print ("%s", g_option_context_get_help (context, FALSE, NULL));
        return 0;
      }

    g_option_context_free (context);

    if (argc == 2)
      db_uri = g_strdup (args[1]);
    g_strfreev (args);
  }

  /* Первая стадия. Наполняем канал данных. */
  db = hyscan_db_new (db_uri);
  writer = hyscan_data_writer_new ();
  buffer = hyscan_buffer_new ();

  position.forward = -10.0;
  position.starboard = 0.0;
  position.vertical = 0.0;
  position.yaw = 0.0;
  position.pitch = 0.0;
  position.roll = 0.0;

  hyscan_data_writer_sonar_set_offset (writer, HYSCAN_SOURCE_SIDE_SCAN_STARBOARD, &position);

  hyscan_data_writer_set_db (writer, db);

  if (!hyscan_data_writer_start (writer, name, name, HYSCAN_TRACK_SURVEY, NULL, -1))
    FAIL ("Couldn't start data writer.");

  {
    HyScanAcousticDataInfo info = {.data_type = HYSCAN_DATA_FLOAT, .data_rate = 1000.0}; /* Информация о датчике. */
    hyscan_data_writer_acoustic_create (writer, HYSCAN_SOURCE_SIDE_SCAN_STARBOARD, 1, NULL, NULL, &info);
    hyscan_data_writer_acoustic_create (writer, HYSCAN_SOURCE_SIDE_SCAN_PORT, 1, NULL, NULL, &info);
  }

  for (i = 0, time = 100000000; i < SIZE; i++, time += DB_TIME_INC)
    {
      guint32 data_size;
      gfloat vals[100] = {0};  /* Акустическая строка. */

      data_size = 100 * sizeof (HyScanComplexFloat);

      hyscan_buffer_wrap (buffer, HYSCAN_DATA_FLOAT, vals, data_size);


      hyscan_data_writer_acoustic_add_data (writer, HYSCAN_SOURCE_SIDE_SCAN_STARBOARD,
                                            1, FALSE, time, buffer);
      hyscan_data_writer_acoustic_add_data (writer, HYSCAN_SOURCE_SIDE_SCAN_PORT,
                                            1, FALSE, time - DB_TIME_INC * 10, buffer);
    }

  sv = g_array_new (FALSE, FALSE, sizeof (HyScanSoundVelocity));

  adata = hyscan_acoustic_data_new (db, NULL, name, name, HYSCAN_SOURCE_SIDE_SCAN_STARBOARD, 1, FALSE);
  projector = hyscan_projector_new (HYSCAN_AMPLITUDE (adata));

  /* Тестирование определения координат по индексу. */
  for (i = 0; i < SIZE; i++)
    {
      gdouble coord;
      guint32 lindex = 200, rindex = 100;
      HyScanDBFindStatus st;
      hyscan_projector_index_to_coord (projector, i, &coord);
      st = hyscan_projector_find_index_by_coord (projector, coord, &lindex, &rindex);
      g_message ("%i -> %f -> %u/%u (%i)", i, coord, lindex, rindex, st);
      st = hyscan_projector_find_index_by_coord (projector, coord + 0.5, &lindex, &rindex);
      g_message ("%i -> %f -> %u/%u (%i)", i, coord + 0.5, lindex, rindex, st);
    }
  /* Тестирование функций, работающих с отсчётами. */
  elem.depth = 0;
  elem.velocity = 1500;
  g_array_append_val (sv, elem);
  elem.depth = 10;
  elem.velocity = 1000;
  g_array_append_val (sv, elem);
  elem.depth = 20;
  elem.velocity = 500;
  g_array_append_val (sv, elem);
  elem.depth = 50;
  elem.velocity = 6000;
  g_array_append_val (sv, elem);

  hyscan_projector_set_sound_velocity (projector, sv);

  hyscan_projector_set_precalc_points (projector, 0);
  hyscan_projector_set_precalc_points (projector, 100);
  hyscan_projector_set_precalc_points (projector, 0);

  for (i = 0; i < 1000; i++)
    {
      gdouble coord;
      guint32 count;

      hyscan_projector_count_to_coord (projector, i , &coord, 10);
      hyscan_projector_coord_to_count (projector, coord , &count, 10);
      g_message ("%i -> %f -> %i", i, coord, count);
    }

exit:

  if (db != NULL)
    hyscan_db_project_remove (db, name);

  g_clear_object (&projector);
  g_clear_object (&db);
  g_clear_object (&writer);
  g_clear_object (&buffer);
  g_clear_pointer (&sv, g_array_unref);

  return 0;
}
