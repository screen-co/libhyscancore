#include <hyscan-mark-manager.h>
#include <stdio.h>

int main( int argc, char **argv )
{
  gchar *db = NULL;
  gchar *pj = NULL;
  gchar *tk = NULL;

  HyScanDB *dbase;
  HyScanMarkManager *mman;

  {
    gchar **args;
    GError *error = NULL;
    GOptionContext *context;
    GOptionEntry entries[] =
      {
        { "db", 'd', 0, G_OPTION_ARG_STRING, &db, "db", NULL },
        { "pj", 'p', 0, G_OPTION_ARG_STRING, &pj, "pj", NULL },
        { "tk", 't', 0, G_OPTION_ARG_STRING, &tk, "tk", NULL },
        { NULL }
      };

    args = g_strdupv (argv);

    context = g_option_context_new ("");
    g_option_context_set_help_enabled (context, TRUE);
    g_option_context_add_main_entries (context, entries, NULL);
    g_option_context_set_ignore_unknown_options (context, FALSE);
    if (!g_option_context_parse_strv (context, &args, &error))
      {
        g_print ("%s\n", error->message);
        return -1;
      }
    g_option_context_free (context);
    g_strfreev (args);
  }

  dbase = hyscan_db_new (db);
  mman = hyscan_mark_manager_new ();
  hyscan_mark_manager_set_project (mman, dbase, pj);

  getchar();

  return 0;
}
