/* hsx-converter-test.c
 *
 * Copyright 2019 Screen LLC, Maxim Pylaev <pilaev@screen-co.ru>
 *
 * This file is part of HyScanCore library.
 *
 * HyScanCore is dual-licensed: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * HyScanCore is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Alternatively, you can license this code under a commercial license.
 * Contact the Screen LLC in this case - <info@screen-co.ru>.
 */

/* HyScanCore имеет двойную лицензию.
 *
 * Во-первых, вы можете распространять HyScanCore на условиях Стандартной
 * Общественной Лицензии GNU версии 3, либо по любой более поздней версии
 * лицензии (по вашему выбору). Полные положения лицензии GNU приведены в
 * <http://www.gnu.org/licenses/>.
 *
 * Во-вторых, этот программный код можно использовать по коммерческой
 * лицензии. Для этого свяжитесь с ООО Экран - <info@screen-co.ru>.
 */

#include "hyscan-hsx-converter.h"
#include <stdio.h>

GMainLoop *main_loop = NULL;
gint       cur_perc = 0;
gint       prev = 0;

/* Завершении конвертации */
void 
done (HyScanHSXConverter *converter)
{
  g_print ("\nConverter done");
  g_print ("\n");
  cur_perc = 0;
  prev = 0;
  g_main_loop_quit (main_loop);
}

/* Обновление процентного выполнения */
void 
current_percent (HyScanHSXConverter *converter,
                 gint                percent)
{
  cur_perc = percent;
}

/* Watcher */
gboolean
mark_update (gpointer ptr)
{
  if (cur_perc > prev)
    {
      g_print ("Current percent: %d%%\r", cur_perc);
      prev = cur_perc;
    }
    return G_SOURCE_CONTINUE;
}

int
main (int    argc,
      char **argv)
{
  gchar *db_uri = NULL;
  gchar *project_name = NULL;
  gchar *result_path = NULL;
  gchar *track_name = NULL;
  gchar **track_names = NULL;
  HyScanDB *db = NULL;
  gint i = 0;
  HyScanHSXConverter *converter = NULL;
  {
    gchar **args;
    GError *error = NULL;
    GOptionContext *context;
    GOptionEntry entries[] =
      {
        { "project-name", 'p', 0, G_OPTION_ARG_STRING, &project_name, "Project name", NULL },
        { "track-name", 't', 0, G_OPTION_ARG_STRING, &track_name, "Track names, through ','", NULL },
        { "result-path", 'r', 0, G_OPTION_ARG_STRING, &result_path, "Path for output files", NULL },
        { NULL }
      };

  #ifdef G_OS_WIN32
    args = g_win32_get_command_line ();
  #else
    args = g_strdupv (argv);
  #endif

    context = g_option_context_new ("<db-uri>");
    g_option_context_set_help_enabled (context, TRUE);
    g_option_context_add_main_entries (context, entries, NULL);
    g_option_context_set_ignore_unknown_options (context, FALSE);
    if (!g_option_context_parse_strv (context, &args, &error))
      {
        g_print ("%s\n", error->message);
        return -1;
      }

    if ((g_strv_length (args) != 2))
      {
        g_print ("%s", g_option_context_get_help (context, FALSE, NULL));
        return 0;
      }

    g_option_context_free (context);

    db_uri = g_strdup (args[1]);
    g_strfreev (args);
   }

  db = hyscan_db_new (db_uri);
  if (db == NULL)
    g_error ("Can't create DB");
  g_free (db_uri);

  converter = hyscan_hsx_converter_new (result_path);
  if (converter == NULL)
    g_error ("Can't create Converter");

  if (!hyscan_hsx_converter_init_crs (converter, NULL, NULL))
    g_error ("Can't init proj4 ctx");
  
  g_signal_connect ((gpointer)converter, "exec",
                    G_CALLBACK (current_percent),
                    NULL);

  g_signal_connect ((gpointer)converter, "done",
                    G_CALLBACK (done),
                    NULL);

  main_loop = g_main_loop_new (NULL, FALSE);

  track_names = g_strsplit (track_name, ",", -1);

  while (track_names[i++] != NULL)
    {
      if (!hyscan_hsx_converter_set_track (converter, db, project_name, *track_names))
        g_error ("Can't set track %s", track_name);

      if (!hyscan_hsx_converter_run (converter))
       g_error ("Can't run Converter");

      g_message ("%s", hyscan_hsx_converter_is_run (converter) ? "Converter is Run" : "Converter is STOP");

      g_timeout_add (500, mark_update, NULL);

      g_print ("Track %s under convert\n", *track_names);

      g_main_loop_run (main_loop);
    }

  g_object_unref (converter);
  g_object_unref (db);
  g_main_loop_unref (main_loop);
  g_strfreev (track_names);

  g_clear_pointer (&project_name, g_free);
  g_clear_pointer (&track_name, g_free);
  g_clear_pointer (&result_path, g_free);

  return 0;
}
