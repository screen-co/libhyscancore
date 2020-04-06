/* navigation-model-test.c
 *
 * Copyright 2019 Screen LLC, Alexey Sakhnov <alexsakhnov@gmail.com>
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

#include <hyscan-nav-model.h>
#include <hyscan-nmea-file-device.h>
#include <glib/gstdio.h>

#define SENSOR_NAME "nav-device"
#define WRONG_SENSOR_NAME "nav-device"
#define NMEA_DATA "$GPGGA,095019.000,5534.2527,N,03806.1113,E,2,16,0.66,111.7,M,14.0,M,0000,0000*65\n$GNGLL,5534.2527,N,03806.1113,E,095019.000,A,D*4C\n$GNGSA,A,3,18,27,10,16,20,13,26,15,21,08,,,0.93,0.66,0.66*17\n$GNGSA,A,3,87,88,71,65,86,79,,,,,,,0.93,0.66,0.66*1C\n$GNRMC,095019.000,A,5534.2527,N,03806.1113,E,2.12,80.44,280917,,,D*47\n$GPVTG,80.44,T,,M,2.12,N,3.92,K,D*09\n$GPZDA,095019.000,28,09,2017,,*55\n$GPGGA,095020.000,5534.2528,N,03806.1123,E,2,16,0.66,111.7,M,14.0,M,0000,0000*63\n$GNGLL,5534.2528,N,03806.1123,E,095020.000,A,D*4A\n$GNGSA,A,3,18,27,10,16,20,13,26,15,21,08,,,0.93,0.66,0.66*17\n$GNGSA,A,3,87,88,71,65,86,79,,,,,,,0.93,0.66,0.66*1C\n$GNRMC,095020.000,A,5534.2528,N,03806.1123,E,2.03,72.66,280917,,,D*4C\n$GPVTG,72.66,T,,M,2.03,N,3.76,K,D*0E\n$GPZDA,095020.000,28,09,2017,,*5F\n$GPGSV,4,1,13,18,71,104,23,27,66,262,32,10,62,198,31,21,46,100,24*76\n$GPGSV,4,2,13,08,36,303,34,15,29,056,36,36,26,187,25,16,23,230,28*7B\n$GPGSV,4,3,13,13,13,028,17,20,12,058,24,26,05,211,18,30,04,340,15*74\n$GPGSV,4,4,13,32,02,171,17*49\n$GLGSV,2,1,08,87,69,119,29,88,57,312,23,65,36,189,16,71,32,038,25*65\n$GLGSV,2,2,08,79,19,312,25,86,12,126,21,80,12,011,18,81,07,308,24*66\n$GPGGA,095021.000,5534.2531,N,03806.1131,E,2,16,0.66,111.8,M,14.0,M,0000,0000*66\n$GNGLL,5534.2531,N,03806.1131,E,095021.000,A,D*40\n$GNGSA,A,3,18,27,10,16,20,13,26,15,21,08,,,0.93,0.66,0.66*17\n$GNGSA,A,3,87,88,71,65,86,79,,,,,,,0.93,0.66,0.66*1C\n$GNRMC,095021.000,A,5534.2531,N,03806.1131,E,0,56.49,280917,,,D*62\n$GPVTG,56.49,T,,M,1.85,N,3.43,K,D*0E\n$GPZDA,095021.000,28,09,2017,,*5E\n$GPGGA,095022.000,5534.2534,N,03806.1139,E,2,16,0.66,111.8,M,14.0,M,0000,0000*68\n$GNGLL,5534.2534,N,03806.1139,E,095022.000,A,D*4E\n$GNGSA,A,3,18,27,10,16,20,13,26,15,21,08,,,0.93,0.66,0.66*17\n$GNGSA,A,3,87,88,71,65,86,79,,,,,,,0.93,0.66,0.66*1C\n$GNRMC,095022.000,A,5534.2534,N,03806.1139,E,1.92,54.89,280917,,,D*46\n$GPVTG,54.89,T,,M,1.92,N,3.56,K,D*02\n$GPZDA,095022.000,28,09,2017,,*5D\n$GPGGA,095023.000,5534.2537,N,03806.1146,E,2,16,0.66,111.9,M,14.0,M,0000,0000*63\n$GNGLL,5534.2537,N,03806.1146,E,095023.000,A,D*44\n$GNGSA,A,3,18,27,10,16,20,13,26,15,21,08,,,0.93,0.66,0.66*17\n$GNGSA,A,3,87,88,71,65,86,79,,,,,,,0.93,0.66,0.66*1C\n$GNRMC,095023.000,A,5534.2537,N,03806.1146,E,1.85,51.20,280917,,,D*4C\n$GPVTG,51.20,T,,M,1.85,N,3.42,K,D*07\n$GPZDA,095023.000,28,09,2017,,*5C\n$GPGGA,095024.000,5534.2542,N,03806.1155,E,2,16,0.66,111.9,M,14.0,M,0000,0000*64\n$GNGLL,5534.2542,N,03806.1155,E,095024.000,A,D*43\n$GNGSA,A,3,18,27,10,16,20,13,26,15,21,08,,,0.93,0.66,0.66*17\n$GNGSA,A,3,87,88,71,65,86,79,,,,,,,0.93,0.66,0.66*1C\n$GNRMC,095024.000,A,5534.2542,N,03806.1155,E,2.82,40.80,280917,,,D*45\n$GPVTG,40.80,T,,M,2.82,N,5.23,K,D*08\n$GPZDA,095024.000,28,09,2017,,*5B\n$GPGGA,095025.000,5534.2549,N,03806.1163,E,2,16,0.66,111.9,M,14.0,M,0000,0000*6B\n$GNGLL,5534.2549,N,03806.1163,E,095025.000,A,D*4C\n$GNGSA,A,3,18,27,10,16,20,13,26,15,21,08,,,0.93,0.66,0.66*17\n$GNGSA,A,3,87,88,71,65,86,79,,,,,,,0.93,0.66,0.66*1C\n$GNRMC,095025.000,A,5534.2549,N,03806.1163,E,2.55,36.38,280917,,,D*42\n$GPVTG,36.38,T,,M,2.55,N,4.72,K,D*05\n$GPZDA,095025.000,28,09,2017,,*5A\n$GPGSV,4,1,13,18,71,104,30,27,66,262,37,10,62,198,31,21,46,100,23*76\n$GPGSV,4,2,13,08,36,303,35,15,29,056,31,36,26,187,27,16,23,230,28*7F\n$GPGSV,4,3,13,13,13,028,20,20,12,058,23,26,05,211,22,30,04,340,*7A\n$GPGSV,4,4,13,32,02,171,18*46\n$GLGSV,2,1,08,87,69,119,31,88,57,312,24,65,37,189,18,71,32,038,31*61\n$GLGSV,2,2,08,79,19,312,32,86,12,126,21,80,12,011,16,81,07,308,27*6D\n$GPGGA,095026.000,5534.2554,N,03806.1171,E,2,16,0.66,111.8,M,14.0,M,0000,0000*66\n$GNGLL,5534.2554,N,03806.1171,E,095026.000,A,D*40\n$GNGSA,A,3,18,27,10,16,20,13,26,15,21,08,,,0.93,0.66,0.66*17\n$GNGSA,A,3,87,88,71,65,86,79,,,,,,,0.93,0.66,0.66*1C\n$GNRMC,095026.000,A,5534.2554,N,03806.1171,E,2.99,37.03,280917,,,D*47\n$GPVTG,37.03,T,,M,2.99,N,5.55,K,D*08\n$GPZDA,095026.000,28,09,2017,,*59\n$GPGGA,095027.000,5534.2561,N,03806.1179,E,2,16,0.66,111.7,M,14.0,M,0000,0000*66\n$GNGLL,5534.2561,N,03806.1179,E,095027.000,A,D*4F\n$GNGSA,A,3,18,27,10,16,20,13,26,15,21,08,,,0.93,0.66,0.66*17\n$GNGSA,A,3,87,88,71,65,86,79,,,,,,,0.93,0.66,0.66*1C\n$GNRMC,095027.000,A,5534.2561,N,03806.1179,E,2.94,33.33,280917,,,D*42\n$GPVTG,33.33,T,,M,2.94,N,5.45,K,D*03\n$GPZDA,095027.000,28,09,2017,,*58\n$GPGGA,095028.000,5534.2566,N,03806.1188,E,2,16,0.66,111.7,M,14.0,M,0000,0000*60\n$GNGLL,5534.2566,N,03806.1188,E,095028.000,A,D*49\n$GNGSA,A,3,18,27,10,16,20,13,26,15,21,08,,,0.93,0.66,0.66*17\n$GNGSA,A,3,87,88,71,65,86,79,,,,,,,0.93,0.66,0.66*1C\n$GNRMC,095028.000,A,5534.2566,N,03806.1188,E,2.44,47.53,280917,,,D*4C\n$GPVTG,47.53,T,,M,2.44,N,4.51,K,D*0F\n$GPZDA,095028.000,28,09,2017,,*57\n$GPGGA,095029.000,5534.2571,N,03806.1197,E,2,16,0.66,111.7,M,14.0,M,0000,0000*69\n$GNGLL,5534.2571,N,03806.1197,E,095029.000,A,D*40\n$GNGSA,A,3,18,27,10,16,20,13,26,15,21,08,,,0.93,0.66,0.66*17\n$GNGSA,A,3,87,88,71,65,86,79,,,,,,,0.93,0.66,0.66*1C\n$GNRMC,095029.000,A,5534.2571,N,03806.1197,E,2.31,48.19,280917,,,D*46\n$GPVTG,48.19,T,,M,2.31,N,4.29,K,D*03\n$GPZDA,095029.000,28,09,2017,,*56\n"

static guint signal_count = 0;       /* Счётчик числа полученных координат. */
static HyScanGeoGeodetic test_from;  /* Минимальные допустимые значения координат. */
static HyScanGeoGeodetic test_to;    /* Максимальные допустимые значения координат. */

gint
usage (const gchar *prg_name)
{
  g_print ("Usage: %s [FILENAME|--default-data]\n", prg_name);
  return -1;
}

void
test_model_data (HyScanNavModel     *model,
                 HyScanNavStateData *data)
{
  static gdouble prev_time = G_MAXDOUBLE;
  static gdouble start_time = G_MAXDOUBLE;
  
  if (start_time == G_MAXDOUBLE)
    start_time = data->time;
  
  g_assert (prev_time == G_MAXDOUBLE || prev_time < data->time);
  prev_time = data->time;
  
  g_atomic_int_inc (&signal_count);
  g_print ("Model changed: %12.2f sec: %10.6f, %10.6f\n", data->time - start_time, data->coord.lat, data->coord.lon);

  /* Тест 3. Проверяем, что полученные данные в пределах допустимых значений. */
  if (test_from.lat != test_to.lat)
    g_assert (test_from.lat < data->coord.lat && data->coord.lat < test_to.lat);
  if (test_from.lon != test_to.lon)
    g_assert (test_from.lon < data->coord.lon && data->coord.lon < test_to.lon);
  if (test_from.h != test_to.h)
    g_assert (test_from.h < data->cog && data->cog < test_to.h);
}

/* Создаёт файл с тестовыми NMEA-данными и устанавливает область движения судна. */
static gchar *
make_test_default_data (void)
{
  gchar *name_used;
  gint fd;

  fd = g_file_open_tmp ("nav-model-test-XXXXXX.nmea", &name_used, NULL);
  g_assert (fd != -1);
  g_close (fd, NULL);

  g_file_set_contents (name_used, NMEA_DATA, -1, NULL);

  /* Устанавливаем область, в пределах которой находятся точки из NMEA_DATA. */
  test_from.lat = 55.57;
  test_to.lat = 55.58;
  test_from.lon = 38.10;
  test_to.lon = 38.11;

  return name_used;
}

static gboolean
test_wrong_sensor_name (gpointer user_data)
{
  HyScanNavModel *model = HYSCAN_NAV_MODEL (user_data);

  g_assert (g_atomic_int_get (&signal_count) > 0);
  hyscan_nav_model_set_sensor_name (model, WRONG_SENSOR_NAME);

  return G_SOURCE_REMOVE;
}

void
test_yet_no_data (HyScanNavModel *model)
{
  HyScanNavStateData data;

  g_assert (FALSE == hyscan_nav_state_get (HYSCAN_NAV_STATE (model), &data, NULL));
  g_assert (FALSE == data.loaded);
}

int main (int    argc,
          char **argv)
{
  GMainLoop *loop;

  HyScanNmeaFileDevice *device;
  HyScanNavModel *model;

  gchar *filename;
  gdouble default_data;

  if (argc < 2)
    return usage (argv[0]);

  default_data = g_str_equal (argv[1], "--default-data");
  filename = default_data ? make_test_default_data () : g_strdup (argv[1]);

  loop = g_main_loop_new (NULL, FALSE);

  device = hyscan_nmea_file_device_new (SENSOR_NAME, filename);
  model = hyscan_nav_model_new ();
  hyscan_nav_model_set_sensor (model, HYSCAN_SENSOR (device));
  hyscan_nav_model_set_sensor_name (model, WRONG_SENSOR_NAME);

  g_signal_connect_swapped (device, "finish", G_CALLBACK (g_main_loop_quit), loop);

  /* Тест 1. Проверяем данные при каждом их изменении. */
  g_signal_connect (model, "nav-changed", G_CALLBACK (test_model_data), NULL);

  /* Тест 2. Проверяем, если данных пока ещё нет. */
  test_yet_no_data (model);

  /* Тест 3. Проверяем, что модель не получает данные от датчика с другим именем. */
  g_timeout_add_seconds (3, test_wrong_sensor_name, model);

  hyscan_sensor_set_enable (HYSCAN_SENSOR (device), SENSOR_NAME, TRUE);
  g_main_loop_run (loop);
  hyscan_device_disconnect (HYSCAN_DEVICE (device));

  if (default_data)
    g_unlink (filename);

  g_object_unref (device);
  g_object_unref (model);

  g_main_loop_unref (loop);
  g_free (filename);

  /* Тест 4. Проверяем, что данные были получены. */
  g_assert (signal_count > 0);

  g_message ("Test done!");

  return 0;
}
