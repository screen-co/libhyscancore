/* profile-test.c
 *
 * Copyright 2019-2020 Screen LLC, Alexander Dmitriev <m1n7@yandex.ru>
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

#include <hyscan-profile-db.h>
#include <hyscan-profile-hw.h>
#include <hyscan-profile-offset.h>
#include "hyscan-dummy-device.h"

#include <glib/gstdio.h>

#define TEST_NAME "test_name"
#define TEST_SENSOR "random_sensor_name"
#define EMPTY_SENSOR ""
#define TEST_SOURCE HYSCAN_SOURCE_ECHOSOUNDER
#define HW_URI "nmea://auto"

static void     test_db                 (const gchar          *uri);
static gboolean compare_antenna_offsets (HyScanAntennaOffset  *a,
                                         HyScanAntennaOffset  *b);
static void     check_offsets           (HyScanProfileOffset  *pof,
                                         HyScanAntennaOffset  *reference);
static void     test_offset             (void);
static void     test_hw                 (gchar               **paths);

int
main (int    argc,
      char **argv)
{
  gchar **paths = NULL;
  gchar *uri = NULL;

  {
    gchar **args;
    GError *error = NULL;
    GOptionContext *context;
    GOptionEntry entries[] =
      {
        { "drivers", 'd', 0, G_OPTION_ARG_STRING_ARRAY, &paths, "Paths to drivers", NULL },
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
    uri = g_strdup (args[1]);
    g_strfreev (args);
  }

  test_db (uri);
  test_offset ();

  if (paths != NULL)
    test_hw (paths);

  g_message ("Passed.");

  g_strfreev (paths);
  g_free (uri);

  return 0;
}

static void
test_db (const gchar *uri)
{
  gchar buf[25];
  gchar *file;
  HyScanDB *db;
  HyScanProfileDB *pdb;
  HyScanProfile *p;

  file = g_build_filename (".", hyscan_rand_id (buf, G_N_ELEMENTS (buf)), NULL);
  pdb = hyscan_profile_db_new (file);
  p = HYSCAN_PROFILE (pdb);

  hyscan_profile_set_name (p, TEST_NAME);
  hyscan_profile_db_set_uri (pdb, uri);

  if (!hyscan_profile_sanity (p))
    g_error ("DB profile sanity check failure");

  if (!hyscan_profile_write (p))
    g_error ("DB profile write failure");

  g_clear_object (&pdb);
  pdb = hyscan_profile_db_new (file);
  p = HYSCAN_PROFILE (pdb);

  if (!hyscan_profile_read (p))
    g_error ("DB profile read failure");

  if (!g_str_equal (TEST_NAME, hyscan_profile_get_name (p)) ||
      !g_str_equal (uri, hyscan_profile_db_get_uri (pdb)))
    {
      g_error ("DB profile values mismatch");
    }

  db = hyscan_profile_db_connect (pdb);
  if (db == NULL)
    g_error ("DB profile connection failure");

  if (!hyscan_profile_delete (p))
    g_error ("DB profile deletion failure");

  g_free (file);
  g_clear_object (&db);
  g_clear_object (&pdb);
}

static gboolean
compare_antenna_offsets (HyScanAntennaOffset *a,
                         HyScanAntennaOffset *b)
{
  return a->starboard == b->starboard &&
         a->forward == b->forward     &&
         a->vertical == b->vertical   &&
         a->yaw == b->yaw             &&
         a->pitch == b->pitch         &&
         a->roll == b->roll;
}

static void
check_offsets (HyScanProfileOffset *pof,
               HyScanAntennaOffset *reference)
{
  GHashTable *ht;
  GHashTableIter iter;
  gpointer k;
  HyScanAntennaOffset *v;

  ht = hyscan_profile_offset_list_sources (pof);
  g_hash_table_iter_init (&iter, ht);
  while (g_hash_table_iter_next (&iter, &k, (gpointer*)&v))
    {
      if (GPOINTER_TO_INT (k) != TEST_SOURCE)
        g_error ("Offset profile: extra sonars");
      if (!compare_antenna_offsets (v, reference))
        g_error ("Offset profile: values mismatch");
    }
  g_hash_table_unref (ht);

  ht = hyscan_profile_offset_list_sensors (pof);
  g_hash_table_iter_init (&iter, ht);
  while (g_hash_table_iter_next (&iter, &k, (gpointer*)&v))
    {
      if (!g_str_equal ((gchar**)k, TEST_SENSOR) &&
          !g_str_equal ((gchar**)k, EMPTY_SENSOR))
        g_error ("Offset profile: extra sources");
      if (!compare_antenna_offsets (v, reference))
        g_error ("Offset profile: values mismatch");
    }
  g_hash_table_unref (ht);
}

static void
test_offset (void)
{
  gchar buf[25];
  gchar *file;
  HyScanProfileOffset *pof;
  HyScanProfile *p;
  HyScanAntennaOffset offt1 = {1., 1., 1., 1., 1., 1.};
  HyScanAntennaOffset offt2 = {2., 2., 2., 2., 2., 2.};

  file = g_build_filename (".", hyscan_rand_id (buf, G_N_ELEMENTS (buf)), NULL);
  pof = hyscan_profile_offset_new (file);
  p = HYSCAN_PROFILE (pof);

  hyscan_profile_set_name (p, TEST_NAME);

  hyscan_profile_offset_add_sensor (pof, TEST_SENSOR, &offt1);
  hyscan_profile_offset_add_sensor (pof, TEST_SENSOR, &offt2);
  hyscan_profile_offset_add_source (pof, TEST_SOURCE, &offt1);
  hyscan_profile_offset_add_source (pof, TEST_SOURCE, &offt2);
  hyscan_profile_offset_add_sensor (pof, EMPTY_SENSOR, &offt1);
  hyscan_profile_offset_add_sensor (pof, EMPTY_SENSOR, &offt2);

  if (!hyscan_profile_sanity (p))
    g_error ("Offset profile sanity check failure");

  if (!hyscan_profile_write (p))
    g_error ("Offset profile write failure");

  g_clear_object (&pof);
  pof = hyscan_profile_offset_new (file);
  p = HYSCAN_PROFILE (pof);

  if (!hyscan_profile_read (p))
    g_error ("Offset profile read failure");

  if (!g_str_equal (TEST_NAME, hyscan_profile_get_name (p)))
    g_error ("Offset profile values mismatch");

  check_offsets (pof, &offt2);

  if (!hyscan_profile_delete (p))
    g_error ("Offset profile deletion failure");

  g_free (file);
  g_clear_object (&pof);
}

static void
test_hw (gchar **paths)
{
  gchar buf[25];
  gchar *file;
  HyScanProfileHW *phw;
  HyScanProfileHWDevice *phwd;
  HyScanProfile *p;
  GList *list;
  HyScanControl *ctrl;

  file = g_build_filename (".", hyscan_rand_id (buf, G_N_ELEMENTS (buf)), NULL);
  phw = hyscan_profile_hw_new (file, paths);
  p = HYSCAN_PROFILE (phw);
  hyscan_profile_set_name (p, TEST_NAME);

  phwd = hyscan_profile_hw_device_new (paths);

  hyscan_profile_hw_device_set_group (phwd, HYSCAN_PROFILE_INFO_GROUP);
  hyscan_profile_hw_device_set_driver (phwd, "nmea");
  hyscan_profile_hw_device_set_uri (phwd, HW_URI);
  hyscan_profile_hw_device_set_name (phwd, TEST_NAME);

  if (!hyscan_profile_hw_device_update (phwd))
    g_error ("HW profile device: couldn't update");

  hyscan_profile_hw_add (phw, phwd);
  hyscan_profile_hw_add (phw, phwd);
  hyscan_profile_hw_add (phw, phwd);

  hyscan_profile_write (p);

  g_clear_object (&phw);
  g_clear_object (&phwd);

  phw = hyscan_profile_hw_new (file, paths);
  p = HYSCAN_PROFILE (phw);
  hyscan_profile_read (p);

  list = hyscan_profile_hw_list (phw);

  if (1 != g_list_length (list))
    g_error ("HW profile: device number mismatch");

  phwd = (HyScanProfileHWDevice*)list->data;
  if (!g_str_equal ("nmea", hyscan_profile_hw_device_get_driver (phwd)) ||
      !g_str_equal (HW_URI, hyscan_profile_hw_device_get_uri (phwd)) ||
      !g_str_equal (TEST_NAME, hyscan_profile_hw_device_get_name (phwd)))
  {
    g_error ("HW profile device: data mismatch");
  }

  if (!hyscan_profile_hw_check (phw))
    g_error ("HW profile: check failure");

  if (NULL == (ctrl = hyscan_profile_hw_connect (phw)))
    g_error ("HW profile: connection failure");

  g_list_free (list);
  g_clear_object (&phw);
  g_clear_object (&ctrl);
  g_unlink (file);
  g_free (file);
}
