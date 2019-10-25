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

GMainLoop *main_loop;

void 
done (HyScanHSXConverter *converter)
{
  g_message ("Converter done");
  g_main_loop_quit (main_loop);
}

void 
current_percent (HyScanHSXConverter *converter,
                 gint                percent)
{
  static gint prev = 0;
  if (percent > prev)
    {
  	  printf ("%d%% ", percent);
      prev = percent;
    }
}

int
main (int    argc,
      char **argv)
{
  gchar *db_uri = NULL;
  gchar *project_name = NULL;
  gchar *result_path = NULL;
  gchar *track_name = NULL;

  HyScanDB *db;
  HyScanHSXConverter *converter;

  {
    gchar **args;
    GError *error = NULL;
    GOptionContext *context;
    GOptionEntry entries[] =
      {
        { "project-name", 'p', 0, G_OPTION_ARG_STRING, &project_name, "Project name", NULL },
        { "track-name", 't', 0, G_OPTION_ARG_STRING, &track_name, "Track name", NULL },
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

  converter = hyscan_hsx_converter_new (result_path);
  if (converter == NULL)
    g_error ("Can't create Converter");

  g_signal_connect ((gpointer)converter, "exec",
                    G_CALLBACK (current_percent),
                    NULL);

  g_signal_connect ((gpointer)converter, "done",
                    G_CALLBACK (done),
                    NULL);

  hyscan_hsx_converter_set_track (converter, db, project_name, track_name);
  
  if (!hyscan_hsx_converter_run (converter))
   g_error ("Can't run Converter");

  g_message ("%s", hyscan_hsx_converter_is_run (converter) ? "RUN" : "STOP");

  /* Создание цикла обработки событий. Выход осуществляется по завершении работы всех тестов.*/
  main_loop = g_main_loop_new (NULL, TRUE);
  g_main_loop_run (main_loop);

  g_object_unref (converter);
  g_object_unref (db);

  return 0;
}
