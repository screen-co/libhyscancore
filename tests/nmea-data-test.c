#include <hyscan-nmea-data.h>
#include <hyscan-data-writer.h>
#include <hyscan-cached.h>
#include <string.h>
#include <glib/gprintf.h>

#define SENSOR_NAME    "sensor"
#define SENSOR_CHANNEL 3
#define START_TIME     1e10
#define TIME_INCREMENT 1e6

gchar  dec_to_ascii    (gint   dec);
gchar *nmea_generator  (gchar *prefix,
                        gint   i);

int
main (int argc, char **argv)
{
  GError                 *error;
  gchar                  *db_uri = "file://./";
  gchar                  *name = "test";
  HyScanDB               *db;
  HyScanCache            *cache;

  /* Запись данных. */
  HyScanBuffer           *buffer;
  HyScanDataWriter       *writer;
  HyScanAntennaOffset     offset = {0};

  /* Тестируемые объекты.*/
  HyScanNMEAData *nmea;
  gint64 time;
  gint i, j;

  gint samples = 100;
  gint readouts = 100;
  GTimer *timer = g_timer_new ();
  gdouble time_with_cache, time_without_cache;

  /* Парсим аргументы. */
  {
    gchar **args;
    guint args_len;
    error = NULL;
    GOptionContext *context;
    GOptionEntry entries[] = {
      {"samples", 's', 0, G_OPTION_ARG_INT, &samples, "Number of samples (default 100);", NULL},
      {"readouts", 'r', 0, G_OPTION_ARG_INT, &readouts, "Read each sample x times (default 100);", NULL},
      {NULL}
    };

#ifdef G_OS_WIN32
    args = g_win32_get_command_line ();
#else
    args = g_strdupv (argv);
#endif
    context = g_option_context_new ("<db-uri>");
    g_option_context_set_help_enabled (context, TRUE);
    g_option_context_add_main_entries (context, entries, NULL);
    g_option_context_set_ignore_unknown_options (context, TRUE);
    if (!g_option_context_parse_strv (context, &args, &error))
      {
        g_print ("%s\n", error->message);
        return -1;
      }

    args_len = g_strv_length (args);
    if (args_len == 2)
      {
        db_uri = g_strdup (args[1]);
      }
    else if (args_len > 2)
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
    g_error ("can't open db");

  /* Создаем объект записи данных. */
  writer = hyscan_data_writer_new ();

  /* Система хранения. */
  hyscan_data_writer_set_db (writer, db);

  if (!hyscan_data_writer_start (writer, name, name, HYSCAN_TRACK_SURVEY, NULL, -1))
    g_error ("can't start write");

  /* Смещение приёмных антенн. */
  hyscan_data_writer_sensor_set_offset (writer, "sensor", &offset);

  /* Наполняем данными. */
  buffer = hyscan_buffer_new ();

  for (i = 0, time = START_TIME; i < samples; i++, time += TIME_INCREMENT)
    {
      gchar *data = nmea_generator ("DPT", i);

      hyscan_buffer_wrap (buffer, HYSCAN_DATA_BLOB, data, strlen (data));
      hyscan_data_writer_sensor_add_data (writer, SENSOR_NAME, HYSCAN_SOURCE_NMEA,
                                                  SENSOR_CHANNEL, time, buffer);

      g_free (data);
    }

  /* Теперь потестируем объект. */
  g_printf ("\nTrying to open an unsupported sensor. The following warning is OK.\n");
  nmea = hyscan_nmea_data_new_sensor (db, name, name, SENSOR_NAME"-bad");
  if (nmea != NULL)
    g_error ("Object creation failure");

  g_printf ("\nTrying to open an absent channel. The following warning is OK.\n");
  nmea = hyscan_nmea_data_new (db, name, name, SENSOR_CHANNEL + 1);
  if (nmea != NULL)
    g_error ("Object creation failure");

  nmea = hyscan_nmea_data_new_sensor (db, name, name, SENSOR_NAME);
  if (nmea == NULL)
    g_error ("Object creation failure");

  if (SENSOR_CHANNEL != hyscan_nmea_data_get_channel (nmea))
    g_error ("Source channel mismatch");

  /* Анализируем данные без кэша. */
  g_timer_start (timer);

  for (j = 0; j < readouts; j++)
    {
      for (i = 0; i < samples; i++)
        {
          const gchar *acquired;
          gchar *expected;

          acquired = hyscan_nmea_data_get_sentence (nmea, i, NULL);
          expected = nmea_generator ("DPT", i);

          if (0 != g_strcmp0 (acquired, expected))
            {
              g_error ("Read failure at %i: expected <%s>, acquired <%s>",
                        i, expected, acquired);
            }

          g_free (expected);
        }
    }

  time_without_cache = g_timer_elapsed (timer, NULL);

  /* Анализируем данные с кэшем. */
  hyscan_nmea_data_set_cache (nmea, cache);

  g_timer_start (timer);
  for (j = 0; j < readouts; j++)
    {
      for (i = 0; i < samples; i++)
        {
          const gchar *acquired;
          gchar *expected;

          acquired = hyscan_nmea_data_get_sentence (nmea, i, NULL);
          expected = nmea_generator ("DPT", i);

          if (0 != g_strcmp0 (acquired, expected))
            {
              g_error ("Read failure at %i: expected \"%s\", acquired \"%s\"",
                        i, expected, acquired);
            }

          g_free (expected);
        }
    }
  time_with_cache = g_timer_elapsed (timer, NULL);

  /* Функция hyscan_nmea_data_check_sentence. */
  {
    gchar *data;

    data = nmea_generator ("RMC", 0);

    if (HYSCAN_NMEA_DATA_RMC != hyscan_nmea_data_check_sentence (data))
      g_error ("RMC sentence check failure");
    g_free (data);

    data = nmea_generator ("LOL", 0);
    if (HYSCAN_NMEA_DATA_ANY != hyscan_nmea_data_check_sentence (data))
      g_error ("ANY sentence check failure");

    data[1] = 'Z';
    if (HYSCAN_NMEA_DATA_INVALID != hyscan_nmea_data_check_sentence (data))
      g_error ("Invalid sentence check failure");
    g_free (data);
  }

  /* Функция hyscan_nmea_data_split_sentence. */
  {
    gchar *data;
    gchar *samples[5];
    gchar **res;

    guint32 len = 0;

    samples[0] = "$GPRMC,131548.000,A,5533.1654,N,03806.2259,E,2.6,316.0,030517,0.0,W*77";
    samples[1] = "$GPGGA,131548.000,5533.1654,N,03806.2259,E,1,19,0.6,105.83,M,14.0,M,,*65";
    samples[2] = "$GNGSA,A,3,25,12,02,06,31,24,14,29,19,32,03,,1.1,0.6,0.9*23";
    samples[3] = "$GNGSA,A,3,,,,,,,,,,,,,1.1,0.6,0.9*23";
    samples[4] = "$GPGSV,3,1,11,02,48,140,47,03,07,010,33,06,44,071,45,12,82,117,46*7B";

    for (i = 0; i < 5; i++)
      len += strlen (samples[i]);
    len += 6;

    data = g_strdup_printf ("%c%s%c%c%s%c%s%c%s%s%c",
                             0x00,
                             samples[0],
                             0x0D, /* Windows-way. */
                             0x0A,
                             samples[1],
                             0xD,  /* UNIX-way. */
                             samples[2],
                             0x00, /* C-way. */
                             samples[3],
                             samples[4], /* Нет разделителя:( */
                             0x0A);
    res = hyscan_nmea_data_split_sentence (data, len);

    for (i = 0; i < 5; i++)
      {
        if (0 != g_strcmp0 (res[i], samples[i]))
          g_warning ("Expected %s, got %s", samples[i], res[i]);
      }

    g_strfreev (res);
    g_free (data);
  }

  hyscan_db_project_remove (db, name);

  g_clear_object (&db);
  g_clear_object (&cache);
  g_clear_object (&writer);
  g_clear_object (&buffer);
  g_clear_object (&nmea);
  g_timer_destroy (timer);

  g_printf ("%i samples of data were read %i times each.\n", samples, readouts);
  g_printf ("%f seconds (%f per sample) without cache\n",
            time_without_cache, time_without_cache / (readouts * samples));
  g_printf ("%f seconds (%f per sample) with cache\n",
            time_with_cache, time_with_cache / (readouts * samples));

  g_printf ("Test passed.\n");
  return 0;
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
