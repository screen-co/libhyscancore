/* view.c
 *
 * Copyright 2019 Screen LLC, Andrei Fadeev <andrei@webcontrol.ru>
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

#define SRC_WIDTH  16
#define TYPE_WIDTH 8

#include <hyscan-db.h>
#include <string.h>

int
main (int    argc,
      char **argv)
{
  HyScanDB *db = NULL;
  gchar *db_uri = NULL;

  gchar *project_name = NULL;
  gchar *track_name = NULL;

  gint32 project_id;
  gint32 track_id;
  gint32 log_id;

  HyScanBuffer *buffer;
  guint32 index;
  gint64 time0;
  gint64 time;

  {
    gchar **args;
    GError *error = NULL;
    GOptionContext *context;
    GOptionEntry entries[] =
      {
        { "project", 'p', 0, G_OPTION_ARG_STRING, &project_name, "Project name", NULL },
        { "track_name", 't', 0, G_OPTION_ARG_STRING, &track_name, "track_name name", NULL },
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

    if ((g_strv_length (args) != 2) || (project_name == NULL) || (track_name == NULL))
      {
        g_print ("%s", g_option_context_get_help (context, FALSE, NULL));
        return 0;
      }

    g_option_context_free (context);

    db_uri = g_strdup (args[1]);
    g_strfreev (args);
  }

  buffer = hyscan_buffer_new ();

  db = hyscan_db_new (db_uri);
  if (db == NULL)
    goto exit;

  project_id = hyscan_db_project_open (db, project_name);
  if (project_id < 0)
    goto exit;

  track_id = hyscan_db_track_open (db, project_id, track_name);
  if (track_id < 0)
    goto exit;

  log_id = hyscan_db_channel_open (db, track_id, hyscan_source_get_name_by_type (HYSCAN_SOURCE_LOG));
  if (log_id < 0)
    goto exit;

  index = 0;
  time0 = hyscan_db_channel_get_data_time (db, log_id, 0);
  while (hyscan_db_channel_get_data (db, log_id, index++, buffer, &time))
    {
      gdouble dt;
      gchar **message;
      guint32 size;
      gchar *data;

      data = hyscan_buffer_get_data (buffer, &size);
      if (data == NULL)
        continue;

      message = g_strsplit (data, "\t", -1);

      dt = (time - time0) / 1000000.0;
      g_print ("%9.3f | ", dt);

      g_print ("%s", message[0]);
      size = SRC_WIDTH - strlen (message[0]);
      while (size--)
        g_print (" ");

      g_print (" | %s", message[1]);
      size = TYPE_WIDTH - strlen (message[1]);
      while (size--)
        g_print (" ");

      g_print (" | %s\n", message[2]);

      g_strfreev (message);
    }

exit:
  g_object_unref (buffer);
  g_clear_object (&db);

  return 0;
}
