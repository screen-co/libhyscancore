#include <hyscan-nmea-parser.h>
#include <hyscan-data-writer.h>
#include <hyscan-cached.h>
#include <string.h>
#include <math.h>

#define CHANNEL 3
#define DB_TIME_START 1e10
#define DB_TIME_INC 1e6
#define SAMPLES 90
#define SRC HYSCAN_SOURCE_NMEA_RMC

#define time_for_index(index) (DB_TIME_START + (index) * DB_TIME_INC)
#define dec_to_ascii(dec) ((dec) >= 0x0 && (dec) <= 0x9) ? \
                           (dec) + '0' : ((dec) >= 0xA && (dec) <= 0xF) ? \
                           (dec) - 10 + 'A' : 'z'

gchar    *generate_string  (gfloat  seed);

int
main (int argc, char **argv)
{
  GError      *error;
  gchar       *db_uri = NULL;
  gchar       *name = "test";
  HyScanDB    *db;
  HyScanCache *cache;

  /* Запись данных. */
  HyScanDataWriter *writer;
  HyScanBuffer     *buffer;

  /* Тестируемые объекты.*/
  HyScanNMEAParser *nmea;
  HyScanNavData    *ndata;

  gint i;

  /* Парсим аргументы. */
  {
    gchar **args;
    error = NULL;
    GOptionContext *context;
    #ifdef G_OS_WIN32
      args = g_win32_get_command_line ();
    #else
      args = g_strdupv (argv);
    #endif
    context = g_option_context_new ("<db-uri>");
    g_option_context_set_help_enabled (context, TRUE);
    g_option_context_set_ignore_unknown_options (context, TRUE);
    if (!g_option_context_parse_strv (context, &args, &error))
      {
        g_print ("%s\n", error->message);
        return -1;
      }
    if (g_strv_length (args) == 2)
      {
        db_uri = g_strdup (args[1]);
      }
    else
      {
        g_print ("%s", g_option_context_get_help (context, FALSE, NULL));
        return 0;
      }

    g_option_context_free (context);
    g_strfreev (args);
  }

  cache = HYSCAN_CACHE (hyscan_cached_new (512));
  db = hyscan_db_new (db_uri);
  if (db == NULL)
    g_error ("can't open db <%s>", db_uri);

  /* Создаем объект записи данных. */
  writer = hyscan_data_writer_new ();
  hyscan_data_writer_set_db (writer, db);

  if (!hyscan_data_writer_start (writer, name, name, HYSCAN_TRACK_SURVEY))
    g_error ("can't start write");

  buffer = hyscan_buffer_new ();

  /* Наполняем данными. */
  for (i = 0; i < SAMPLES; i++)
    {
      gint64 time = time_for_index (i);
      gchar *data = generate_string (i * 50);

      hyscan_buffer_wrap_data (buffer, HYSCAN_DATA_STRING, data, strlen (data));

      hyscan_data_writer_sensor_add_data (writer, "sensor", SRC, CHANNEL,
                                          time, buffer);

      g_free (data);
    }

  /* Теперь потестируем объект. */
  nmea = hyscan_nmea_parser_new (db, name, name, SRC, CHANNEL,
                                 HYSCAN_NMEA_FIELD_LAT);
  ndata = HYSCAN_NAV_DATA (nmea);

  /* hyscan_nav_data_get */
  {
    gdouble val, expected;
    for (i = 0; i < SAMPLES; i++)
      {
        expected = i * 0.5;
        hyscan_nav_data_get (ndata, i, NULL, &val);
        if (ABS (val) - ABS (expected) > 1e-5)
          g_error ("Failed to get data at index %i (%f, %f)", i, val, expected);
      }
  }

  hyscan_db_project_remove (db, name);

  g_clear_object (&db);
  g_clear_object (&cache);
  g_clear_object (&writer);
  g_clear_object (&nmea);

  g_free (db_uri);

  g_message ("test passed");
  return 0;
}

gchar*
generate_string (gfloat seed)
{
  gfloat deg, min;
  gchar *out, *inner, *ch;
  gchar cs1, cs2;

  gint checksum = 0;

  // inner = g_strdup_printf ("HSDPT,%f,", seed);
  deg = floor (seed / 100);
  min = (seed - deg * 100) * 60 / 100;
  inner = g_strdup_printf ("HSRMC,,,%02.0f%06.3f,N", deg, min);

  /* Подсчитываем чек-сумму. */
  for (ch = inner; *ch != '\0'; ch++)
    checksum ^= *ch;

  cs1 = dec_to_ascii (checksum / 16);
  cs2 = dec_to_ascii (checksum % 16);

  out = g_strdup_printf ("$%s*%c%c", inner, cs1, cs2);
  g_free (inner);

  return out;
}
