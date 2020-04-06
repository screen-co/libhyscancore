#include "hyscan-nmea-helper.h"
#include <math.h>
#define METER_PER_SECOND_TO_KNOTS (3600. / 1852.) /* Коэффициент перевода из метров в секунды в узлы. */

static gboolean
hyscan_nmea_helper_extract_mins (gdouble  value,
                                 gint    *deg,
                                 gdouble *minutes)
{
  gdouble positive;

  positive = value > 0;
  value = fabs (value);
  *deg = value;
  *minutes = (value - *deg) * 60;

  return positive;
}

/**
 * hyscan_nmea_helper_make_rmc:
 * @coord: координаты
 * @course: курс, градусы
 * @velocity: скорость движения, м/с
 * @utc_timestamp: время фиксации, микросекунды UNIX-time
 *
 * Функция создает RMC-сообщение, содержащее в себе указанные данные.
 *
 * Returns: (transfer full): RMC-сообщение, для удаления g_free().
 */
gchar *
hyscan_nmea_helper_make_rmc (HyScanGeoPoint    coord,
                             gdouble           course,
                             gdouble           velocity,
                             gint64            utc_timestamp)
{
  GTimeVal time_val;
  GDateTime *date_time;
  gchar *sentence, *inner;

  gint lat, lon;
  gdouble lat_min, lon_min;
  gboolean north, east;
  gchar *date_str;

  time_val.tv_sec = utc_timestamp / G_USEC_PER_SEC;
  time_val.tv_usec = utc_timestamp - time_val.tv_sec * G_USEC_PER_SEC;
  date_time = g_date_time_new_from_timeval_utc (&time_val);
  date_str = g_date_time_format (date_time, "%d%m%y");

  north = hyscan_nmea_helper_extract_mins (coord.lat, &lat, &lat_min);
  east = hyscan_nmea_helper_extract_mins (coord.lon, &lon, &lon_min);

  velocity *= METER_PER_SECOND_TO_KNOTS;

  inner = g_strdup_printf ("GPRMC,%02d%02d%05.2f,A,"
                           "%02d%09.6f,%c,%03d%09.6f,%c,"
                           "%08.4f,%08.4f,"
                           "%s,011.5,E",
                           g_date_time_get_hour (date_time),
                           g_date_time_get_minute (date_time),
                           g_date_time_get_seconds (date_time),
                           lat, lat_min, north ? 'N' : 'S', lon, lon_min, east ? 'E' : 'W',
                           velocity, course,
                           date_str);

  sentence = hyscan_nmea_helper_wrap (inner);

  g_date_time_unref (date_time);
  g_free (date_str);
  g_free (inner);

  return sentence;
}

gchar *
hyscan_nmea_helper_make_gga (HyScanGeoPoint coord,
                             gint64         utc_timestamp)
{
  GTimeVal time_val;
  GDateTime *date_time;
  gchar *sentence, *inner;

  gint lat, lon;
  gdouble lat_min, lon_min;
  gboolean north, east;

  time_val.tv_sec = utc_timestamp / G_USEC_PER_SEC;
  time_val.tv_usec = utc_timestamp - time_val.tv_sec * G_USEC_PER_SEC;
  date_time = g_date_time_new_from_timeval_utc (&time_val);

  north = hyscan_nmea_helper_extract_mins (coord.lat, &lat, &lat_min);
  east = hyscan_nmea_helper_extract_mins (coord.lon, &lon, &lon_min);

  inner  = g_strdup_printf("GPGGA,%02d%02d%05.2f,"
                           "%02d%08.5f,%c,%03d%08.5f,%c,"
                           "2,6,1.2,18.893,M,-25.669,M,2.0,0031",
                           g_date_time_get_hour (date_time),
                           g_date_time_get_minute (date_time),
                           g_date_time_get_seconds (date_time),
                           lat, lat_min, north ? 'N' : 'S', lon, lon_min, east ? 'E' : 'W');

  sentence = hyscan_nmea_helper_wrap (inner);

  g_date_time_unref (date_time);
  g_free (inner);

  return sentence;
}

/**
 * hyscan_nmea_helper_wrap:
 * @inner: содержимое сообщения NMEA
 *
 * Функция формирует из строки @inner валидное NMEA сообщение:
 *
 * $<@inner>*<checksum><CR><LF>
 *
 * Returns: (transfer full): NMEA сообщение,
 */
gchar *
hyscan_nmea_helper_wrap (const gchar *inner)
{
  gint checksum = 0;
  const gchar *ch;
  gchar *sentence;

  /* Подсчитываем чек-сумму. */
  for (ch = inner; *ch != '\0'; ch++)
    checksum ^= *ch;

  sentence = g_strdup_printf ("$%s*%02X\r\n", inner, checksum);

  return sentence;
}
