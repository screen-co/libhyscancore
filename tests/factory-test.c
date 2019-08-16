#include <glib/gprintf.h>
#include <hyscan-data-writer.h>
#include <hyscan-factory-amplitude.h>
#include <hyscan-factory-depth.h>
#include <hyscan-cached.h>
#include <hyscan-db.h>
#include <string.h>

#define SSS HYSCAN_SOURCE_SIDE_SCAN_STARBOARD
#define SSP HYSCAN_SOURCE_SIDE_SCAN_PORT
#define SIZE 200
#define DB_TIME_INC 1000000

gfloat*
make_acoustic_string (gint    size,
                      guint32 *bytes);

gchar *nmea_generator  (gchar *prefix,
                        gint   i);

int
main (int argc, char **argv)
{
  gint i;
  gint64 time;
  HyScanDB *db;                      /* БД. */
  gchar *db_uri;                     /* Путь к БД. */
  gchar *name = "test";              /* Проект и галс. */

  HyScanDataWriter *writer;          /* Класс записи данных. */
  HyScanCache *cache;                /* Система кэширования. */
  HyScanFactoryAmplitude *af = NULL; /* Фабрика акустических данных. */
  HyScanFactoryDepth *df = NULL;     /* Фабрика данных глубины. */
  gboolean status = FALSE;
  HyScanBuffer *buffer = NULL;

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
    else
      db_uri = g_strdup ("file://./");

    g_strfreev (args);
  }

  /* Первая стадия. Наполняем канал данных. */
  cache = HYSCAN_CACHE (hyscan_cached_new (512));
  buffer = hyscan_buffer_new ();
  db = hyscan_db_new (db_uri);
  writer = hyscan_data_writer_new ();
  hyscan_data_writer_set_db (writer, db);
  if (!hyscan_data_writer_start (writer, name, name, HYSCAN_TRACK_SURVEY, -1))
    {
      g_message ("Couldn't start data writer.");
      goto exit;
    }

  for (i = 0, time = 0; i < SIZE; i++, time += DB_TIME_INC)
    {
      gfloat *acoustic;  /* Акустическая строка. */
      gchar *nmea;

      guint32 real_size;
      HyScanAcousticDataInfo info = {.data_type = HYSCAN_DATA_FLOAT, .data_rate = 1.0}; /* Информация о датчике. */

      acoustic = make_acoustic_string (SIZE, &real_size);
      hyscan_buffer_wrap (buffer, HYSCAN_DATA_FLOAT, acoustic, real_size);

      hyscan_data_writer_acoustic_add_data (writer, SSS, 1, FALSE, time, &info, buffer);
      hyscan_data_writer_acoustic_add_data (writer, SSP, 1, FALSE, time, &info, buffer);

      g_free (acoustic);

      nmea = nmea_generator ("DPT", i);

      hyscan_buffer_wrap (buffer, HYSCAN_DATA_BLOB, nmea, strlen (nmea));
      hyscan_data_writer_sensor_add_data (writer, "sensor", HYSCAN_SOURCE_NMEA,
                                                  1, time, buffer);

      g_free (nmea);
    }

  af = hyscan_factory_amplitude_new (cache);
  df = hyscan_factory_depth_new (cache);
  hyscan_factory_amplitude_set_track (af, db, name, name);
  hyscan_factory_depth_set_track (df, db, name, name);

  {
    HyScanAmplitude *ampl;
    HyScanDepthometer * dmeter;
    guint32 n_points;
    gint64 time;
    gboolean noise;

    ampl = hyscan_factory_amplitude_produce (af, SSS);
    hyscan_amplitude_get_amplitude (ampl, NULL, 0, &n_points, &time, &noise);
    g_object_unref (ampl);

    ampl = hyscan_factory_amplitude_produce (af, SSP);
    hyscan_amplitude_get_amplitude (ampl, NULL, 0, &n_points, &time, &noise);
    g_object_unref (ampl);

    dmeter = hyscan_factory_depth_produce (df);
    hyscan_depthometer_get (dmeter, NULL, DB_TIME_INC);
    g_object_unref (dmeter);
  }

  status = TRUE;
  /* Третья стадия. Убираем за собой. */
exit:

  g_clear_object (&cache);
  g_clear_object (&buffer);
  g_clear_object (&writer);
  g_clear_object (&af);
  g_clear_object (&df);

  hyscan_db_project_remove	(db, name);
  g_clear_object (&db);

  g_clear_pointer (&db_uri, g_free);

  g_printf ("test %s\n", status ? "passed" : "falled");

  return status ? 0 : 1;
}

gfloat*
make_acoustic_string (gint    size,
                      guint32 *bytes)
{
  gint i;
  guint32 malloc_size = size * sizeof (gfloat);
  gfloat *str = g_malloc0 (malloc_size);

  for (i = 0; i < size; i++)
    {
      str[i] = 1.0;
      // str[i].re = 1.0;
      // str[i].im = 0.0;
    }

  if (bytes != NULL)
    *bytes = malloc_size;

  return str;
}

gchar
dec_to_ascii (gint dec)
{
  if (dec >= 0x0 && dec <= 0x9)
    return dec + '0';

  if (dec >= 0xA && dec <= 0xF)
    return dec + 'A' - 10;

  return 'z';
}

gchar*
nmea_generator (gchar *prefix,
                gint   seed)
{
  gchar *out, *inner, *ch;
  gchar cs1, cs2;

  gint checksum = 0;

  inner = g_strdup_printf ("GP%s,%i,", prefix, seed);

  /* Подсчитываем чек-сумму. */
  for (ch = inner; *ch != '\0'; ch++)
    checksum ^= *ch;

  cs1 = dec_to_ascii (checksum / 16);
  cs2 = dec_to_ascii (checksum % 16);

  out = g_strdup_printf ("$%s*%c%c", inner, cs1, cs2);
  g_free (inner);

  return out;
}
