#include <hyscan-mloc.h>
#include <hyscan-cached.h>

int
main (int argc, char **argv)
{
  gchar       *db_name = NULL;
  gchar       *project_name = NULL;
  gchar       *track_name = NULL;

  HyScanDB    *db = NULL;
  HyScanCache *cache = NULL;
  HyScanmLoc  *mloc = NULL;


  /* Парсим аргументы. */
  {
    gchar **args;
    GError * error = NULL;
    GOptionContext *context;
    GOptionEntry entries[] = {
      {"db", 'd', 0, G_OPTION_ARG_STRING, &db_name, "db name;", NULL},
      {"project", 'p', 0, G_OPTION_ARG_STRING, &project_name, "project name;", NULL},
      {"track", 't', 0, G_OPTION_ARG_STRING, &track_name, "track name;", NULL},
      {NULL}
    };

#ifdef G_OS_WIN32
    args = g_win32_get_command_line ();
#else
    args = g_strdupv (argv);
#endif
    context = g_option_context_new ("");
    g_option_context_set_help_enabled (context, TRUE);
    g_option_context_add_main_entries (context, entries, NULL);
    g_option_context_set_ignore_unknown_options (context, TRUE);
    if (!g_option_context_parse_strv (context, &args, &error))
      {
        g_print ("%s\n", error->message);
        return -1;
      }

    g_option_context_free (context);

    g_strfreev (args);
  }

  cache = HYSCAN_CACHE (hyscan_cached_new (512));
  db = hyscan_db_new (db_name);

  if (db == NULL)
    g_error ("can't open db <%s>", db_name);

  mloc = hyscan_mloc_new (db, cache, project_name, track_name);

  {
    HyScanAntennaOffset pos;
    HyScanGeoGeodetic result;
    gboolean found;

    pos.x = 5;
    pos.y = 10;
    pos.z = 15;

    found = hyscan_mloc_get (mloc, 138920512, &pos, 50, 0, 0, &result);
    g_message ("%i", found);
  }

  g_object_unref (db);
  g_object_unref (mloc);
  g_object_unref (cache);

  g_free (db_name);
  g_free (project_name);
  g_free (track_name);

  return 0;
}
