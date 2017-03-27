#include <glib/gprintf.h>
#include <hyscan-acoustic-data.h>
#include <hyscan-data-writer.h>
#include <hyscan-waterfall-tile.h>

#define SSS HYSCAN_SOURCE_SIDE_SCAN_STARBOARD
#define SSP HYSCAN_SOURCE_SIDE_SCAN_PORT
#define SRC_RAW TRUE
#define SIZE 20
#define DB_TIME_INC 1000000

#define FAIL(msg) do {g_print ("%s\n", msg); goto finish;} while (0)

HyScanComplexFloat *make_acoustic_string (gint     size,
                                          guint32 *bytes);

int
main (int argc, char **argv)
{
  HyScanDB *db;                          /* БД. */
  gchar *db_uri = "file://./";           /* Путь к БД. */
  gchar *name = "test";                  /* Проект и галс. */

  HyScanDataWriter *writer;              /* Класс записи данных. */

  HyScanAcousticData *dc = NULL;         /* Класс чтения данных. */
  HyScanWaterfallTile *wf = NULL;        /* Генератор тайлов. */
  HyScanTile tile;
  gfloat *image;
  guint32 image_size;

  gint64 time = 0;                    /* Время записи строки. */
  gint i;                                /* Простой маленький счетчик.*/

  gboolean status = FALSE;

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
        g_message (error->message);
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
  writer = hyscan_data_writer_new (db);
  if (!hyscan_data_writer_set_project (writer, name))
    FAIL ("Couldn't set data writer project.");
  if (!hyscan_data_writer_start (writer, name, HYSCAN_TRACK_SURVEY))
    FAIL ("Couldn't start data writer.");

  for (i = 0, time = 0; i < SIZE; i++, time += DB_TIME_INC)
    {
      HyScanAcousticDataInfo info = {.data.type = HYSCAN_DATA_COMPLEX_FLOAT, .data.rate = 1.0}; /* Информация о датчике. */
      HyScanDataWriterData data; /* Записываемые данные. */
      HyScanComplexFloat *vals;  /* Акустическая строка. */

      vals = make_acoustic_string (SIZE, &data.size);
      data.time = time;
      data.data = vals;

      hyscan_data_writer_acoustic_add_data (writer, SSS, &info, &data);
      hyscan_data_writer_acoustic_add_data (writer, SSP, &info, &data);

      g_free (vals);
    }

  /* Теперь займемся генерацией тайлов. */
  dc = hyscan_acoustic_data_new (db, name, name, SSS, FALSE);
  wf = hyscan_waterfall_tile_new ();

  tile.across_start = 0;
  tile.along_start  = 0;
  tile.across_end   = SIZE * 1000;
  tile.along_end    = SIZE * 1000;
  tile.scale        = 1000;
  tile.ppi          = 25.4;
  tile.upsample     = 1;
  tile.type         = HYSCAN_TILE_SLANT;
  tile.rotate       = FALSE;
  hyscan_waterfall_tile_set_depth (wf, NULL);
  hyscan_waterfall_tile_set_speeds (wf, 1.0, 2.0);
  hyscan_waterfall_tile_set_tile (wf, dc, tile);

  image = hyscan_waterfall_tile_generate (wf, &tile, &image_size);

  if (tile.w != SIZE || tile.h != SIZE)
    FAIL ("Tile size mismatch");

  g_free (image);

  status = TRUE;
  /* Третья стадия. Убираем за собой. */
finish:
  hyscan_db_project_remove	(db, name);

  g_clear_object (&writer);
  g_clear_object (&dc);
  g_clear_object (&db);
  g_clear_object (&wf);

  g_printf ("test %s\n", status ? "passed" : "falled");
  return status ? 0 : 1;
}

HyScanComplexFloat*
make_acoustic_string (gint    size,
                      guint32 *bytes)
{
  gint i;
  guint32 malloc_size = size * sizeof (HyScanComplexFloat);
  HyScanComplexFloat *str = g_malloc0 (malloc_size);

  for (i = 0; i < size; i++)
    {
      str[i].re = 1.0;
      str[i].im = 0.0;
    }

  if (bytes != NULL)
    *bytes = malloc_size;

  return str;
}
