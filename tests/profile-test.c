/* control-test.c
 *
 * Copyright 2016-2018 Screen LLC, Andrei Fadeev <andrei@webcontrol.ru>
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
#include <hyscan-profile-offset.h>
#include "hyscan-dummy-device.h"

#define TEST_NAME "test_name"
#define TEST_SENSOR "random_sensor_name"
#define TEST_SOURCE HYSCAN_SOURCE_ECHOSOUNDER


static void test_db (void);
static void test_offset (void);

int
main (int    argc,
      char **argv)
{
  test_db ();
  test_offset ();

  g_message ("Passed.");

  return 0;
}

static void
test_db (void)
{
  gchar buf[25];
  gchar *file, *uri;
  HyScanDB *db;
  HyScanProfileDB *pdb;
  HyScanProfile *p;

  file = g_build_filename (".", hyscan_rand_id (buf, G_N_ELEMENTS (buf)), NULL);
  pdb = hyscan_profile_db_new (file);
  p = HYSCAN_PROFILE (pdb);

  uri = g_strdup_printf ("file://%s", g_get_tmp_dir ());
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
  g_free (uri);
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
        g_error ("Offset profile: extra sources");
      if (!compare_antenna_offsets (v, reference))
        g_error ("Offset profile: values mismatch");
    }
  g_hash_table_unref (ht);

  ht = hyscan_profile_offset_list_sensors (pof);
  g_hash_table_iter_init (&iter, ht);
  while (g_hash_table_iter_next (&iter, &k, (gpointer*)&v))
    {
      if (!g_str_equal ((gchar**)k, TEST_SENSOR))
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
    {
      g_error ("Offset profile values mismatch");
    }

  check_offsets (pof, &offt2);

  if (!hyscan_profile_delete (p))
    g_error ("Offset profile deletion failure");

  g_free (file);
  g_clear_object (&pof);
}

