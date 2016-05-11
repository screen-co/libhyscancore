/*TODO: обсудить размер вот этой временной константы.
 * Она определяет временные рамки, в течение которых данные можно не интерполировать, а взять точное значение. */
#define TIME_OF_VALIDITY 10e3
#define ONE_RAD_LENGTH 6474423.1 /* Длина одного радиана по экватору в метрах. */
#define HYSCAN_LOCATION_GDOUBLE1_INIT 0,0,NAN,FALSE
#define HYSCAN_LOCATION_GDOUBLE2_INIT 0,0,NAN,NAN,FALSE
#define HYSCAN_LOCATION_GINT1_INIT 0,0,0,FALSE
#include <hyscan-location-tools.h>
#include <hyscan-data-channel.h>
#include <math.h>

/* Функция выдачи широты и долготы для заданного момента времени. */
HyScanLocationGdouble2
hyscan_location_getter_latlong (HyScanDB *db,
                                 GArray *source_list,
                                 GArray *cache,
                                 gint32 source,
                                 gint64 time,
                                 gdouble quality)
{
  return hyscan_location_getter_gdouble2 (db, source_list, cache, source, time, quality, NULL);
}

/* Функция выдачи высоты для заданного момента времени. */
HyScanLocationGdouble1
hyscan_location_getter_altitude (HyScanDB *db,
                                  GArray *source_list,
                                  GArray *cache,
                                  gint32 source,
                                  gint64 time,
                                  gdouble quality)
{
  return hyscan_location_getter_gdouble1 (db, source_list, cache, source, time, quality);
}

/* Функция выдачи курса для заданного момента времени. */
HyScanLocationGdouble1
hyscan_location_getter_track (HyScanDB *db,
                               GArray *source_list,
                               GArray *cache,
                               gint32 source,
                               gint64 time,
                               gdouble quality)
{
  HyScanLocationSourcesList *source_list_element = &g_array_index (source_list, HyScanLocationSourcesList, source);
  HyScanLocationGdouble1 output = {HYSCAN_LOCATION_GDOUBLE1_INIT};
  HyScanLocationGdouble2 point  = {HYSCAN_LOCATION_GDOUBLE2_INIT},
                         prev_point = {HYSCAN_LOCATION_GDOUBLE2_INIT};

  gdouble f2 = 0,
          f1 = 0,
          l2 = 0,
          l1 = 0,
          x = 0,
          y = 0;

  /* Если курс берется непосредственно из NMEA. */
  if (source_list_element->source_type == HYSCAN_LOCATION_SOURCE_NMEA)
    {
      output = hyscan_location_getter_gdouble1 (db, source_list, cache, source, time, quality);
      return output;
    }


  /* Если курс высчитывается из координат. */
  if (source_list_element->source_type == HYSCAN_LOCATION_SOURCE_NMEA_COMPUTED)
    {
      point = hyscan_location_getter_gdouble2 (db, source_list, cache, source, time, quality, &prev_point);
      if (point.validity == TRUE && prev_point.validity == TRUE)
        {
          f2 = point.value1 * G_PI/ 180.0;
          f1 = prev_point.value1 * G_PI/ 180.0;
          l2 = point.value2 * G_PI/ 180.0;
          l1 = prev_point.value2 * G_PI/ 180.0;

          x = cos(f2) * sin (l2-l1);
          y = cos (f1) * sin(f2) - sin(f1) * cos(f2) * cos(l2-l1);

          if (y > 0)
            {
              if (x > 0)
                output.value = atan(y/x);
              if (x < 0)
                output.value = G_PI - atan(-y/x);
              if (x == 0)
                output.value = G_PI/ 2.0;
            }

          if (y < 0)
            {
              if (x > 0)
                output.value = - atan(-y/x);
              if (x < 0)
                output.value = atan(y/x) - G_PI;
              if (x == 0)
                output.value = 3*G_PI/ 2.0;
            }

          if (y == 0)
            {
              if (x > 0)
                output.value = 0;
              if (x < 0)
                output.value = G_PI;
              if (x == 0)
                output.value = 0;
            }

          /* Пересчитываем курс так, чтобы 0 соответствовал северу, а 90 - востоку. */
          output.value = 90.0 - output.value * 180.0/G_PI;
          if (output.value < 0)
            output.value = 360.0 + output.value;
          if (output.value > 360.0)
            output.value -= 360.0;
          output.validity = TRUE;
        }
      return output;
    }
  return output;
}

/* Функция выдачи крена для заданного момента времени. */
HyScanLocationGdouble1
hyscan_location_getter_roll (HyScanDB *db,
                              GArray *source_list,
                              GArray *cache,
                              gint32 source,
                              gint64 time,
                              gdouble quality)
{
  return hyscan_location_getter_gdouble1 (db, source_list, cache, source, time, quality);
}

/* Функция выдачи дифферента для заданного момента времени. */
HyScanLocationGdouble1
hyscan_location_getter_pitch (HyScanDB *db,
                               GArray *source_list,
                               GArray *cache,
                               gint32 source,
                               gint64 time,
                               gdouble quality)
{
  return hyscan_location_getter_gdouble1 (db, source_list, cache, source, time, quality);
}

/* Функция выдачи скорости для заданного момента времени. */
HyScanLocationGdouble1
hyscan_location_getter_speed (HyScanDB *db,
                               GArray *source_list,
                               GArray *cache,
                               gint32 source,
                               gint64 time,
                               gdouble quality)
{
  HyScanLocationSourcesList *source_list_element = &g_array_index (source_list, HyScanLocationSourcesList, source);

  HyScanLocationGdouble1 output = {HYSCAN_LOCATION_GDOUBLE1_INIT};
  HyScanLocationGdouble2 point  = {HYSCAN_LOCATION_GDOUBLE2_INIT},
                         prev_point = {HYSCAN_LOCATION_GDOUBLE2_INIT};

  gdouble f2 = 0,
          f1 = 0,
          l2 = 0,
          l1 = 0,
          dlat = 0,
          dlon = 0;
  gdouble time_delta;

  /* Если курс берется непосредственно из NMEA. */
  if (source_list_element->source_type == HYSCAN_LOCATION_SOURCE_NMEA)
    return hyscan_location_getter_gdouble1 (db, source_list, cache, source, time, quality);

  /* Если курс высчитывается из координат. */
  if (source_list_element->source_type == HYSCAN_LOCATION_SOURCE_NMEA_COMPUTED)
    {
      point = hyscan_location_getter_gdouble2 (db, source_list, cache, source, time, quality, &prev_point);
      if (point.validity == TRUE && prev_point.validity == TRUE)
        {
          f2 = point.value1 * G_PI/ 180.0;
          f1 = prev_point.value1 * G_PI/ 180.0;
          l2 = point.value2 * G_PI/ 180.0;
          l1 = prev_point.value2 * G_PI/ 180.0;

          dlon = (l2 - l1) * ONE_RAD_LENGTH * cos (f2);
          dlat = (f2 - f1) * ONE_RAD_LENGTH;
          time_delta = (point.data_time - prev_point.data_time) / 1e6;
          if (time_delta == 0.0)
            output.value = 0.0;
          else
            output.value = sqrt (pow(dlat, 2) + pow(dlon, 2)) / time_delta;
          output.validity = 1;
        }
      return output;
    }
  return output;
}

/* Функция выдачи глубины для заданного момента времени. */
HyScanLocationGdouble1
hyscan_location_getter_depth (HyScanDB *db,
                               GArray *source_list,
                               GArray *cache,
                               gint32 source,
                               gint64 time,
                               gdouble quality)
{
  HyScanLocationSourcesList *source_list_element = &g_array_index (source_list, HyScanLocationSourcesList, source);

  HyScanLocationGdouble1 output = {HYSCAN_LOCATION_GDOUBLE1_INIT};
  HyScanLocationGdouble1 p1 = {HYSCAN_LOCATION_GDOUBLE1_INIT},
                         p2 = {HYSCAN_LOCATION_GDOUBLE1_INIT},
                         p3 = {HYSCAN_LOCATION_GDOUBLE1_INIT},
                         p4 = {HYSCAN_LOCATION_GDOUBLE1_INIT};
  gint32 lindex = 0,
         rindex = 0;
  gint64 ltime = 0,
         rtime = 0;

  if (!hyscan_data_channel_find_data (source_list_element->dchannel, time, &lindex, &rindex, &ltime, &rtime))
    return output;

  lindex -= source_list_element->shift;
  rindex -= source_list_element->shift;

  /* Если запрошенное время отличается от времени какого-нибудь ближайшего значения в базе меньше, чем на 10 мс
   * И это значение уже обработано, можно вернуть именно это значение.
   */
  if (ABS(ltime - time) < TIME_OF_VALIDITY && lindex < source_list_element->processing_index)
    return g_array_index (cache, HyScanLocationGdouble1, lindex);
  if (ABS(rtime - time) < TIME_OF_VALIDITY && rindex < source_list_element->processing_index)
    return g_array_index (cache, HyScanLocationGdouble1, rindex);

  if (rindex + 1 > source_list_element->processing_index || lindex - 1 < 0 || lindex - 1 > source_list_element->processing_index)
    return output;

  if (rindex + 1 <= source_list_element->processing_index)
    p4 = g_array_index (cache, HyScanLocationGdouble1, rindex+1);

  if (lindex - 1 >= 0 && lindex - 1 <= source_list_element->processing_index)
    p1 = g_array_index (cache, HyScanLocationGdouble1, lindex-1);

  /* Для усреднения берем одно значение слева, одно справа, а одно то,
   * которое ближе к требуемому времени. */
  p2 = g_array_index (cache, HyScanLocationGdouble1, lindex);
  p3 = g_array_index (cache, HyScanLocationGdouble1, rindex);
  if (p2.validity == FALSE || p3.validity == FALSE)
    return output;

  if (p1.validity == TRUE && p4.validity == FALSE)
    output.value = (p1.value + p2.value + p3.value) / 3.0;
  if (p1.validity == FALSE && p4.validity == TRUE)
    output.value = (p4.value + p2.value + p3.value) / 3.0;
  if (p1.validity == TRUE && p4.validity == TRUE)
    {
      output.value = (ABS(p1.db_time - time) <= ABS(p4.db_time - time)) ? p1.value : p4.value;
      output.value = (output.value + p2.value + p3.value) / 3.0;
    }
  if (p1.validity == FALSE && p4.validity == FALSE)
    output.value = (p2.value + p3.value) / 2.0;

  output.validity = TRUE;
  return output;

}

/* Функция выдачи даты и времени для заданного момента времени. */
HyScanLocationGint1
hyscan_location_getter_datetime (HyScanDB *db,
                                  GArray *source_list,
                                  GArray *cache,
                                  gint32 source,
                                  gint64 time,
                                  gdouble quality)
{
  HyScanLocationSourcesList *source_list_element = &g_array_index (source_list, HyScanLocationSourcesList, source);
  GDateTime *dt,
            *dt2;
  HyScanLocationGint1 output = {HYSCAN_LOCATION_GINT1_INIT};
  HyScanLocationGint1 p1 = {HYSCAN_LOCATION_GINT1_INIT},
                      p2 = {HYSCAN_LOCATION_GINT1_INIT};
  gint32 lindex = 0,
         rindex = 0;
  gint64 ltime = 0,
         rtime = 0;
  gint32 year, month, day, hour, minute, second, microsecond;
  gint64 middle;
  if (!hyscan_db_channel_find_data (db, source_list_element->channel_id, time, &lindex, &rindex, &ltime, &rtime))
    return output;

  lindex -= source_list_element->shift;
  rindex -= source_list_element->shift;

  if (ABS(ltime - time) < TIME_OF_VALIDITY && lindex < source_list_element->processing_index)
    return g_array_index (cache, HyScanLocationGint1, lindex);
  if (ABS(rtime - time) < TIME_OF_VALIDITY && rindex < source_list_element->processing_index)
    return g_array_index (cache, HyScanLocationGint1, rindex);
  if (rindex <= source_list_element->processing_index && lindex >=0)
    {
      p1 = g_array_index (cache, HyScanLocationGint1, lindex);
      p2 = g_array_index (cache, HyScanLocationGint1, rindex);

      middle = (p1.date + p1.time + p2.date + p2.time) / 2;

      dt = g_date_time_new_from_unix_utc (middle/1000000);
      year = g_date_time_get_year (dt);
      month = g_date_time_get_month (dt);
      day = g_date_time_get_day_of_month (dt);
      hour = g_date_time_get_hour (dt);
      minute = g_date_time_get_minute (dt);
      second = g_date_time_get_second (dt);
      microsecond = g_date_time_get_microsecond (dt);

      dt2 = g_date_time_new_utc (year, month, day, 0, 0, 0);
      output.date = g_date_time_to_unix (dt2) * 1e6;
      dt2 = g_date_time_new_utc (1970, 1, 1, hour, minute, (gdouble)(second) + (gdouble)(microsecond)/1e6);
      output.time = g_date_time_to_unix (dt2) * 1e6;
      output.validity = TRUE;
    }

  return output;
}

/* Внутренняя функция выдачи значений типа HyScanLocationGdouble2 для заданного момента времени. */
HyScanLocationGdouble2
hyscan_location_getter_gdouble2 (HyScanDB *db,
                                 GArray *source_list,
                                 GArray *cache,
                                 gint32  source,
                                 gint64  time,
                                 gdouble quality,
                                 HyScanLocationGdouble2  *prev_point)
{
  /* TODO: функция в текущем виде предназначена для разработки и отладки.
   * Когда будет понятно, что она работает надлежащим образом, нужно убрать все
   * промежуточные переменные и заменить их непосредственно значениями из структур с данными. */
  HyScanLocationSourcesList *source_list_element = &g_array_index (source_list, HyScanLocationSourcesList, source);

  HyScanLocationGdouble2 output = {HYSCAN_LOCATION_GDOUBLE2_INIT};

  HyScanLocationGdouble2 p1, p2, p3, p4;
  gdouble p1_lat,
          p1_lon,
          p2_lat,
          p2_lon,
          p3_lat,
          p3_lon,
          p4_lat,
          p4_lon,
          temp_p2_lat,
          temp_p2_lon,
          temp_p3_lat,
          temp_p3_lon,
          out_lat,
          out_lon;
  gdouble k11,
          k12,
          k13,
          k14,
          k21,
          k22,
          k23,
          k24;
  gdouble p1_time,
          p2_time,
          p3_time,
          p4_time,
          out_time;

  gint32 lindex = 0,
         rindex = 0;
  gint64 ltime = 0,
         rtime = 0;

  if (!hyscan_db_channel_find_data (db, source_list_element->channel_id, time, &lindex, &rindex, &ltime, &rtime))
    return output;
  lindex -= source_list_element->shift;
  rindex -= source_list_element->shift;

  /* Если запрошенное время отличается от времени какого-нибудь ближайшего значения в базе меньше, чем на 10 мс
   * И это значение уже обработано, можно вернуть именно это значение.
   */
  if (ABS(ltime - time) < TIME_OF_VALIDITY && lindex < source_list_element->processing_index)
    {
      if (lindex - 1 > 0 && prev_point != NULL)
        *prev_point = g_array_index (cache, HyScanLocationGdouble2, lindex-1);
      if (lindex - 1 <= 0 && prev_point != NULL)
        *prev_point = g_array_index (cache, HyScanLocationGdouble2, lindex);
      output = g_array_index (cache, HyScanLocationGdouble2, lindex);
      return output;
    }
  if (ABS(rtime - time) < TIME_OF_VALIDITY && rindex < source_list_element->processing_index)
    {
      if (rindex - 1 > 0 && prev_point != NULL)
        *prev_point = g_array_index (cache, HyScanLocationGdouble2, rindex-1);
      if (rindex - 1 <= 0 && prev_point != NULL)
        *prev_point = g_array_index (cache, HyScanLocationGdouble2, rindex);
      output = g_array_index (cache, HyScanLocationGdouble2, lindex);
      return g_array_index (cache, HyScanLocationGdouble2, rindex);
    }

  /* Возможна ситуация, когда в момент выполнения предыдущих двух проверок не выполяется вторая половина условия.
   * В этом случае возвращаем все нули, чтобы не выполнить нижележащий код.
   */
  if (rindex == lindex)
    return output;


  /* В случае с координатами задача сводится к определению таких точек, что кривая Безье,
   * построенная по ним, пройдет через точки, которые лежат в кэше, в заданные мометы времени.
   * Соответственно, первая и последняя точки совпадут с теми, что в кэше, а вторую и третью
   * требуется высчитать.
   */
  if (rindex + 1 > source_list_element->processing_index || lindex - 1 < 0 || lindex - 1 > source_list_element->processing_index)
    return output;
  p1 = g_array_index (cache, HyScanLocationGdouble2, lindex-1);
  p2 = g_array_index (cache, HyScanLocationGdouble2, lindex);
  p3 = g_array_index (cache, HyScanLocationGdouble2, rindex);
  p4 = g_array_index (cache, HyScanLocationGdouble2, rindex+1);

  if (prev_point != NULL)
    *prev_point = p1;

  p1_lat = p1.value1;
  p1_lon = p1.value2;
  p1_time = p1.data_time;

  p2_lat = p2.value1;
  p2_lon = p2.value2;
  p2_time = p2.data_time;

  p3_lat = p3.value1;
  p3_lon = p3.value2;
  p3_time = p3.data_time;

  p4_lat = p4.value1;
  p4_lon = p4.value2;
  p4_time = p4.data_time;

  p2_time = (p2_time - p1_time) / (p4_time - p1_time);
  p3_time = (p3_time - p1_time) / (p4_time - p1_time);
  out_time = (time - p1_time) / (p4_time - p1_time);
  p1_time = 0;
  p4_time = 1;


  k11 = pow(1-p2_time, 3);
  k12 = 3 * p2_time * pow(1 - p2_time, 2);
  k13 = 3 * p2_time * p2_time * (1 - p2_time);
  k14 = pow(p2_time, 3);
  k21 = pow(1-p3_time, 3);
  k22 = 3 * p3_time * pow(1 - p3_time, 2);
  k23 = 3 * p3_time * p3_time * (1 - p3_time);
  k24 = pow(p3_time, 3);

  temp_p3_lat = p3_lat - k21 * p1_lat - k24 * p4_lat - (p2_lat - k11 * p1_lat - k14 * p4_lat) * k22 / k12;
  temp_p3_lat /= k23 - k13 * k22 / k12;

  temp_p2_lat = p2_lat - k11 * p1_lat - k13 * temp_p3_lat - k14 * p4_lat;
  temp_p2_lat /= k12;

  temp_p3_lon = p3_lon - k21 * p1_lon - k24 * p4_lon - (p2_lon - k11 * p1_lon - k14 * p4_lon) * k22 / k12;
  temp_p3_lon /= k23 - k13 * k22 / k12;

  temp_p2_lon = p2_lon - k11 * p1_lon - k13 * temp_p3_lon - k14 * p4_lon;
  temp_p2_lon /= k12;

  k11 = pow (1-out_time, 3);
  k12 = 3 * out_time * pow (1-out_time,2);
  k13 = 3 * out_time * out_time * (1-out_time);
  k14 = pow (out_time, 3);
  out_lat = k11 * p1_lat + k12 * temp_p2_lat + k13 * temp_p3_lat + k14 * p4_lat;
  out_lon = k11 * p1_lon + k12 * temp_p2_lon + k13 * temp_p3_lon + k14 * p4_lon;

  output.value1 = out_lat;
  output.value2 = out_lon;
  output.data_time = time;
  output.validity = TRUE;
  return output;
}

/* Внутренняя функция выдачи значений типа HyScanLocationGdouble1 для заданного момента времени. */
HyScanLocationGdouble1
hyscan_location_getter_gdouble1 (HyScanDB *db,
                                 GArray *source_list,
                                 GArray *cache,
                                 gint32 source,
                                 gint64 time,
                                 gdouble quality)
{
  HyScanLocationSourcesList *source_list_element = &g_array_index (source_list, HyScanLocationSourcesList, source);

  HyScanLocationGdouble1 output = {HYSCAN_LOCATION_GDOUBLE1_INIT};
  HyScanLocationGdouble1 p1 = {HYSCAN_LOCATION_GDOUBLE1_INIT},
                         p2 = {HYSCAN_LOCATION_GDOUBLE1_INIT},
                         p3 = {HYSCAN_LOCATION_GDOUBLE1_INIT},
                         p4 = {HYSCAN_LOCATION_GDOUBLE1_INIT};
  gint32 lindex = 0,
         rindex = 0;
  gint64 ltime = 0,
         rtime = 0;

  if (!hyscan_db_channel_find_data (db, source_list_element->channel_id, time, &lindex, &rindex, &ltime, &rtime))
    return output;

  lindex -= source_list_element->shift;
  rindex -= source_list_element->shift;

  /* Если запрошенное время отличается от времени какого-нибудь ближайшего значения в базе меньше, чем на 10 мс
   * И это значение уже обработано, можно вернуть именно это значение.
   */
  if (ABS(ltime - time) < TIME_OF_VALIDITY && lindex < source_list_element->processing_index)
    return g_array_index (cache, HyScanLocationGdouble1, lindex);
  if (ABS(rtime - time) < TIME_OF_VALIDITY && rindex < source_list_element->processing_index)
    return g_array_index (cache, HyScanLocationGdouble1, rindex);

  if (lindex == rindex)
    return output;

  if (rindex + 1 > source_list_element->processing_index || lindex - 1 < 0 || lindex - 1 > source_list_element->processing_index)
    return output;

  if (rindex + 1 <= source_list_element->processing_index)
    p4 = g_array_index (cache, HyScanLocationGdouble1, rindex+1);

  if (lindex - 1 >= 0 && lindex - 1 <= source_list_element->processing_index)
    p1 = g_array_index (cache, HyScanLocationGdouble1, lindex-1);

  /* Для усреднения берем одно значение слева, одно справа, а одно то,
   * которое ближе к требуемому времени. */
  p2 = g_array_index (cache, HyScanLocationGdouble1, lindex);
  p3 = g_array_index (cache, HyScanLocationGdouble1, rindex);
  if (p2.validity == FALSE || p3.validity == FALSE)
    return output;

  if (p1.validity == TRUE && p4.validity == FALSE)
    output.value = (p1.value + p2.value + p3.value) / 3.0;
  if (p1.validity == FALSE && p4.validity == TRUE)
    output.value = (p4.value + p2.value + p3.value) / 3.0;
  if (p1.validity == TRUE && p4.validity == TRUE)
    {
      output.value = (ABS(p1.db_time - time) <= ABS(p4.db_time - time)) ? p1.value : p4.value;
      output.value = (output.value + p2.value + p3.value) / 3.0;
    }
  if (p1.validity == FALSE && p4.validity == FALSE)
    output.value = (p2.value + p3.value) / 2.0;

  output.validity = TRUE;
  return output;
}
