/* forward-look-data-test.c
 *
 * Copyright 2017-2018 Screen LLC, Andrei Fadeev <andrei@webcontrol.ru>
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

#include <hyscan-forward-look-data.h>
#include <hyscan-cached.h>

#include "hyscan-fl-gen.h"

#define PROJECT_NAME           "test"
#define TRACK_NAME             "track"
#define SOUND_VELOCITY         1000.0

int main( int argc, char **argv )
{
  gchar *db_uri = NULL;           /* Путь к каталогу с базой данных. */
  guint n_lines = 100;            /* Число строк для теста. */
  guint n_points = 100;           /* Число точек в строке. */
  guint cache_size = 0;           /* Размер кэша, Мб. */

  HyScanDB *db;
  HyScanCache *cache = NULL;
  HyScanFLGen *generator;
  HyScanForwardLookData *reader;

  HyScanAntennaOffset offset;
  HyScanAcousticDataInfo info;

  guint16 *raw_values1;
  guint16 *raw_values2;
  guint i, j;

  {
    gchar **args;
    GError *error = NULL;
    GOptionContext *context;
    GOptionEntry entries[] =
      {
        { "lines", 'l', 0, G_OPTION_ARG_INT, &n_lines, "Number of lines", NULL },
        { "points", 'n', 0, G_OPTION_ARG_INT, &n_points, "Number of points per line", NULL },
        { "cache", 'c', 0, G_OPTION_ARG_INT, &cache_size, "Use cache with size, Mb", NULL },
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

    if (g_strv_length (args) != 2)
      {
        g_print ("%s", g_option_context_get_help (context, FALSE, NULL));
        return 0;
      }

    g_option_context_free (context);

    db_uri = g_strdup (args[1]);
    g_strfreev (args);
  }

  /* Параметры данных. */
  offset.x = 0.0;
  offset.y = 0.0;
  offset.z = 0.0;
  offset.psi = 0.0;
  offset.gamma = 0.0;
  offset.theta = 0.0;

  info.data_rate = 150000.0;
  info.antenna_voffset = 0.0;
  info.antenna_hoffset = 0.0;
  info.antenna_vaperture = 10.0;
  info.antenna_haperture = 50.0;
  info.antenna_frequency = 100000.0;
  info.antenna_bandwidth = 10000.0;
  info.adc_vref = 1.0;
  info.adc_offset = 0;

  /* Буферы для данных. */
  raw_values1 = g_new0 (guint16, 2 * n_points);
  raw_values2 = g_new0 (guint16, 2 * n_points);

  /* Открываем базу данных. */
  db = hyscan_db_new (db_uri);
  if (db == NULL)
    g_error ("can't open db at: %s", db_uri);

  /* Генератор данных. */
  generator = hyscan_fl_gen_new ();
  hyscan_fl_gen_set_info (generator, &info);
  hyscan_fl_gen_set_offset (generator, &offset);

  /* Проект для записи галсов. */

  if (!hyscan_fl_gen_set_track (generator, db, PROJECT_NAME, TRACK_NAME))
    g_error ("can't set working project");

  /* Тестовые данные для проверки работы вперёдсмотрящего локатора. В каждой строке
   * разность фаз между двумя каналами изменяется от 0 до 2 Pi по дальности. */
  g_message ("Data generation");
  for (i = 0; i < n_lines; i++)
    if (!hyscan_fl_gen_generate (generator, n_points, 1000 * (i + 1)))
      g_error ("can't generate data");

  /* Кэш данных */
  if (cache_size)
    cache = HYSCAN_CACHE (hyscan_cached_new (cache_size));

  /* Объект обработки данных вперёдсмотрящего локатора. */
  reader = hyscan_forward_look_data_new (db, cache, PROJECT_NAME, TRACK_NAME);
  if (reader == NULL)
    g_error ("can't create forward look data processor");

  /* Скорость звука для тестовых данных. */
  hyscan_forward_look_data_set_sound_velocity (reader, SOUND_VELOCITY);

  /* Проверяем, что азимут цели изменяется в секторе обзора от минимального
   * до максимального значения по дальности. */
  for (j = 0; j < 2; j++)
    {
      GTimer *timer = g_timer_new ();
      gdouble elapsed = 0.0;
      gdouble alpha;

      if (j == 0)
        g_message ("Data check");
      else
        g_message ("Cached data check");

      alpha = hyscan_forward_look_data_get_alpha (reader);

      for (i = 0; i < n_lines; i++)
        {
          const HyScanForwardLookDOA *doa;
          guint32 doa_size;
          gint64 doa_time;

          g_timer_start (timer);
          doa = hyscan_forward_look_data_get_doa (reader, i, &doa_size, &doa_time);
          elapsed += g_timer_elapsed (timer, NULL);

          if (doa == NULL)
            g_error ("can't get doa values");

          if (doa_size != n_points)
            g_error ("doa size error");

          if (!hyscan_fl_gen_check (doa, doa_size, 1000 * (i + 1), alpha))
            g_error ("doa data error");
        }

      g_message ("Elapsed %.6fs", elapsed);
      g_timer_destroy (timer);

      if (cache == NULL)
        break;
    }

  g_message ("All done");

  g_clear_object (&generator);
  g_clear_object (&reader);

  hyscan_db_project_remove (db, PROJECT_NAME);

  g_clear_object (&db);
  g_clear_object (&cache);

  g_free (db_uri);
  g_free (raw_values1);
  g_free (raw_values2);

  return 0;
}
