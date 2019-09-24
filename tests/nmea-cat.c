#include <hyscan-nmea-data.h>
#include <string.h>
#include <glib/gprintf.h>

int
main (int argc, char **argv)
{
  gchar                  *db_uri = NULL;
  gchar                  *project = NULL;
  gchar                  *track = NULL;
  HyScanDB               *db;

  /* Тестируемые объекты.*/
  HyScanNMEAData *nmea;
  guint i;

  /* Парсим аргументы. */
  {
    GError                 *error;
    gchar **args;
    guint args_len;
    error = NULL;
    GOptionContext *context;
    GOptionEntry entries[] = {
      {"project", 'p', 0, G_OPTION_ARG_STRING, &project, "Project;", NULL},
      {"track", 't', 0, G_OPTION_ARG_STRING, &track, "Track;", NULL},
      {"db", 'd', 0, G_OPTION_ARG_STRING, &db_uri, "DB;", NULL},

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
    if (args_len > 1)
      {
        g_print ("%s", g_option_context_get_help (context, FALSE, NULL));
        return 0;
      }

    g_option_context_free (context);

    g_strfreev (args);
  }

  db = hyscan_db_new (db_uri);

  if (db == NULL)
    g_error ("can't open db");

  nmea = hyscan_nmea_data_new (db, NULL, project, track, 1);
  if (nmea == NULL)
    g_error ("Object creation failure");

  guint32 first, last;
  hyscan_nmea_data_get_range (nmea, &first, &last);
  for (i = first; i <= last; ++i)
    {
      const gchar *sentence;

      sentence = hyscan_nmea_data_get (nmea, i, NULL);
      g_message ("%i: %s", i, sentence);
    }

  g_clear_object (&db);
  g_clear_object (&nmea);
  return 0;
}
