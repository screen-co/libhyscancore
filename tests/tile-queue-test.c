#include <glib/gprintf.h>
#include <hyscan-data-writer.h>
#include <hyscan-tile-queue.h>
#include <hyscan-cached.h>

#define SSS HYSCAN_SOURCE_SIDE_SCAN_STARBOARD
#define SSP HYSCAN_SOURCE_SIDE_SCAN_PORT
#define SRC_RAW TRUE
#define SIZE 200
#define DB_TIME_INC 1000000

#define FAIL(msg) do {g_print ("%s\n", msg); goto finish;} while (0)

static gint full_callback_number = 0;
static gint reduced_callback_number = 0;

HyScanComplexFloat *make_acoustic_string (gint             size,
                                          guint32         *bytes);

void                tile_queue_image_cb  (HyScanTileQueue *queue,
                                          HyScanTile      *tile,
                                          gfloat          *image,
                                          gint             size,
                                          guint32          hash,
                                          gpointer         user_data);

void                tile_ready_callback  (HyScanTileQueue *queue,
                                          gpointer         user_data);
void                wait_for_generation  (void);
HyScanTile          make_tile            (gint             seed);
void                add_tiles            (HyScanTileQueue *tq);

int
main (int argc, char **argv)
{
  HyScanDB *db;                 /* БД. */
  gchar *db_uri = "file://./";  /* Путь к БД. */
  gchar *name = "test";         /* Проект и галс. */

  HyScanDataWriter *writer;     /* Класс записи данных. */
  HyScanCache *cache;           /* Система кэширования. */

  HyScanTileQueue *tq = NULL;   /* Очередь тайлов. */
  HyScanTile tile, cached_tile; /* Тайлы. */

  gint64 time = 0;              /* Время записи строки. */
  gint i;                       /* Простой маленький счетчик.*/

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
  cache = HYSCAN_CACHE (hyscan_cached_new (512));

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
  tq = hyscan_tile_queue_new (4);
  hyscan_tile_queue_open (tq, db, name, name, FALSE);
  g_signal_connect (tq, "tile-queue-image", G_CALLBACK (tile_queue_image_cb), &full_callback_number);
  g_signal_connect (tq, "tile-queue-ready", G_CALLBACK (tile_ready_callback), &reduced_callback_number);

  hyscan_tile_queue_set_sound_velocity (tq, NULL);

  /* Отдадим кучу тайлов и проверим, вернутся ли они все. Пока что проверяем без кэша. */
  /* Сначала сделаем структуры с тайлами. */


  add_tiles (tq);
  hyscan_tile_queue_add_finished (tq, 1);

  wait_for_generation ();

  g_message ("Generation without cache ok.");

  for (i = 0; i < SIZE; i++)
    {
      gboolean regen, found;
      tile = make_tile (i);

      hyscan_tile_queue_add (tq, &tile);

      found = hyscan_tile_queue_check (tq, &tile, &cached_tile, &regen);
      if (found)
        FAIL ("Found tile. That is strange as we have not set cache yet");
    }

  g_message ("Checking absence of cache ok.");


  hyscan_tile_queue_close (tq);
  hyscan_tile_queue_set_cache (tq, cache);
  hyscan_tile_queue_open (tq, db, name, name, FALSE);

  g_message ("Generating with cache.");

  add_tiles (tq);
  hyscan_tile_queue_add_finished (tq, 1);
  wait_for_generation ();

  g_message ("Checking tiles in cache.");
  for (i = 0; i < SIZE; i++)
    {
      gboolean regen, found;
      gfloat *image;
      guint32 image_size;
      tile = make_tile (i);

      hyscan_tile_queue_add (tq, &tile);

      found = hyscan_tile_queue_check (tq, &tile, &cached_tile, &regen);
      if (!found)
        FAIL ("Not found tile.");

      found = hyscan_tile_queue_get (tq, &tile, &cached_tile, &image, &image_size);
      if (!found)
        FAIL ("Failed to get tile.");

      g_free (image);
    }

  status = TRUE;

  /* Третья стадия. Убираем за собой. */
finish:
  hyscan_db_project_remove	(db, name);

  g_clear_object (&writer);
  g_clear_object (&cache);
  g_clear_object (&db);
  g_clear_object (&tq);

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

void
tile_queue_image_cb (HyScanTileQueue *queue,
                     HyScanTile      *tile,
                     gfloat          *image,
                     gint             image_size,
                     guint            hash,
                     gpointer         user_data)
{
  gint *counter = (gint*) user_data;
  g_atomic_int_dec_and_test (counter);
}

void
tile_ready_callback (HyScanTileQueue *queue,
                     gpointer         user_data)
{
  gint *counter = (gint*) user_data;
  g_atomic_int_dec_and_test (counter);
}

void
wait_for_generation (void)
{
  g_message ("Waiting for %i tiles to generate...", full_callback_number);

  while (TRUE)
    {
      gboolean full_ok, reduced_ok;

      full_ok = g_atomic_int_get (&full_callback_number) == 0;
      reduced_ok = g_atomic_int_get (&reduced_callback_number) == 0;

      if (full_ok && reduced_ok)
        break;
      else
        g_usleep (100);
    }
}

HyScanTile
make_tile (gint seed)
{
  HyScanTile tile;

  tile.across_start = 0;
  tile.along_start = 0;
  tile.scale = 1000;
  tile.ppi = 25.4;
  tile.upsample = 1;
  tile.type = HYSCAN_TILE_SLANT;
  tile.rotate = 0;

  tile.source = (seed % 2) ? SSS : SSP;
  tile.across_end = 1 + seed * 10;
  tile.along_end = 1 + seed * 10;

  return tile;
}
void
add_tiles (HyScanTileQueue *tq)
{
  gint i;
  HyScanTile tile;

  for (i = 0; i < SIZE; i++)
    {
      tile = make_tile (i);

      hyscan_tile_queue_add (tq, &tile);

    }
  full_callback_number = SIZE;
  reduced_callback_number = SIZE;
}
