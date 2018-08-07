#include <hyscan-depth-nmea.h>
#include <hyscan-data-writer.h>
#include <hyscan-cached.h>
#include <string.h>

#define CHANNEL 3
#define DB_TIME_START 1e10
#define DB_TIME_INC 1e6
#define SAMPLES 100

gint64    time_for_index   (guint32 index);
gchar     dec_to_ascii     (gint    dec);
gchar    *generate_string  (gfloat  seed);
gboolean  compare_position (HyScanAntennaPosition p1,
                            HyScanAntennaPosition p2);

int
main (int argc, char **argv)
{
  GError      *error;
  gchar       *db_uri = NULL;
  gchar       *name = "test";
  HyScanDB    *db;
  HyScanCache *cache;

  /* Запись данных. */
  HyScanBuffer          *buffer;
  HyScanDataWriter      *writer;
  HyScanAntennaPosition  position;

  /* Тестируемые объекты.*/
  HyScanDepthNMEA *nmea;
  HyScanDepth     *idepth;

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

  /* Система хранения. */
  hyscan_data_writer_set_db (writer, db);

  if (!hyscan_data_writer_start (writer, name, name, HYSCAN_TRACK_SURVEY))
    g_error ("can't start write");

  /* Местоположение приёмных антенн. */
  position.x = 10;
  position.y = 20;
  position.z = 30;
  position.psi = 40;
  position.gamma = 50;
  position.theta = 60;
  hyscan_data_writer_sensor_set_position (writer, "sensor", &position);

  /* Наполняем данными. */
  buffer = hyscan_buffer_new ();

  for (i = 0; i < SAMPLES; i++)
    {
      gchar *data = generate_string (i);

      hyscan_buffer_wrap_data (buffer, HYSCAN_DATA_BLOB, data, strlen (data));
      hyscan_data_writer_sensor_add_data (writer, "sensor", HYSCAN_SOURCE_NMEA_DPT, CHANNEL, time_for_index (i), buffer);

      g_free (data);
    }

  /* Теперь потестируем объект. */
  nmea = hyscan_depth_nmea_new (db, name, name, CHANNEL);
  idepth = HYSCAN_DEPTH (nmea);

  /* hyscan_depth_get_range */
  {
    guint32 dclindex, dcrindex;
    gboolean status;

    status = hyscan_depth_get_range (idepth, &dclindex, &dcrindex);

    if (dcrindex - dclindex + 1 != SAMPLES || !status)
      g_error ("Failed to get data range");
  }

  /* hyscan_depth_find_data */
  {
    guint32 lindex, rindex, index;
    gint64 ltime, rtime;
    HyScanDBFindStatus status;

    status = hyscan_depth_find_data (idepth, 0, NULL, NULL, NULL, NULL);
    if (status != HYSCAN_DB_FIND_LESS)
      g_error ("Failed to find data");

    status = hyscan_depth_find_data (idepth, time_for_index (SAMPLES + 10), &lindex, &rindex, &ltime, &rtime);

    if (status != HYSCAN_DB_FIND_GREATER)
      g_error ("Failed to find data");

    /* Найдем данные для произвольного индекса. */
    index = SAMPLES / 2;

    status = hyscan_depth_find_data (idepth, time_for_index (index) + 1, &lindex, &rindex, &ltime, &rtime);
    if (status != HYSCAN_DB_FIND_OK ||
        lindex != index ||
        rindex != index + 1 ||
        ltime != time_for_index (index) ||
        rtime != time_for_index (index + 1))
      {
        g_error ("Failed to find data");
      }
  }
  /* hyscan_depth_get_position */
  {
    HyScanAntennaPosition acquired = hyscan_depth_get_position (idepth);

    if (!compare_position (acquired, position))
      g_error ("Antenna positions are not equal");
  }

  /* hyscan_depth_is_writable */
  {
    gboolean true_expected, false_expected;
    true_expected = hyscan_depth_is_writable (idepth);
    hyscan_data_writer_stop (writer);
    false_expected = hyscan_depth_is_writable (idepth);

    if (true_expected != TRUE || false_expected != FALSE)
      g_error ("Data channel writeability fail");
  }

  /* hyscan_depth_get */
  {
    gdouble val;
    for (i = 0; i < SAMPLES; i++)
      {
        val = hyscan_depth_get (idepth, i, NULL);
        if (val != (gdouble)i)
          g_error ("Failed to get data at index %i", i);
      }
  }

  /* hyscan_depth_get с кэшем. */
  hyscan_depth_set_cache (idepth, cache);
  {
    gdouble val;
    for (i = 0; i < SAMPLES; i++)
      {
        /* Считываем дважды, чтобы гарантированно данные
         * оказались в кэше. */
        val = hyscan_depth_get (idepth, i, NULL);
        val = hyscan_depth_get (idepth, i, NULL);
        if (val != (gdouble)i)
          g_error ("Failed to get data at index %i", i);
      }
  }

  hyscan_db_project_remove (db, name);

  g_clear_object (&db);
  g_clear_object (&cache);
  g_clear_object (&writer);
  g_clear_object (&buffer);
  g_clear_object (&nmea);

  g_free (db_uri);

  g_message ("test passed");
  return 0;
}

gint64
time_for_index (guint32 index)
{
  return DB_TIME_START + index * DB_TIME_INC;
}

gchar
dec_to_ascii (gint dec)
{
  if (dec >= 0x0 && dec <= 0x9)
    return dec + '0';

  if (dec >= 0xA && dec <= 0xF)
    return dec -10 + 'A';

  return 'z';
}

gchar*
generate_string (gfloat seed)
{
  gchar *out, *inner, *ch;
  gchar cs1, cs2;

  gint checksum = 0;

  inner = g_strdup_printf ("HSDPT,%f,", seed);

  /* Подсчитываем чек-сумму. */
  for (ch = inner; *ch != '\0'; ch++)
    checksum ^= *ch;

  cs1 = dec_to_ascii (checksum / 16);
  cs2 = dec_to_ascii (checksum % 16);

  out = g_strdup_printf ("$%s*%c%c", inner, cs1, cs2);
  g_free (inner);

  return out;
}

gboolean
compare_position (HyScanAntennaPosition p1,
                  HyScanAntennaPosition p2)
{
  return (p1.x == p2.x &&
          p1.y == p2.y &&
          p1.z == p2.z &&
          p1.psi == p2.psi &&
          p1.gamma == p2.gamma &&
          p1.theta == p2.theta);
}
