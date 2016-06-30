/*
 * Файл содержит следующие группы функций:
 *
 * Функции getter'ы.
 *
 */

#include <hyscan-location-tools.h>
#include <hyscan-data-channel.h>
#include <math.h>

/* Функция выдачи даты и времени для заданного момента времени. */
HyScanLocationInternalTime
hyscan_location_getter_datetime (HyScanDB *db,
                                  GArray  *source_list,
                                  GArray  *cache,
                                  gint32   source,
                                  gint64   time,
                                  gdouble  quality)
{
  GDateTime *dt,
            *dt2;
  HyScanLocationInternalTime output = {HYSCAN_LOCATION_INTERNALTIME_INIT},
                             *p1,
                             *p2;
  gint32 lindex = 0,
         rindex = 0;
  gint32 year, month, day, hour, minute, second, microsecond;
  gint64 middle;
  if (!hyscan_location_find_time (cache, source_list, source, time, &lindex, &rindex))
    return output;

  if (lindex == rindex)
    {
      output = g_array_index (cache, HyScanLocationInternalTime, lindex);
      output.validity = HYSCAN_LOCATION_VALID;
    }
    //TODO: проверка граничных точек
  else
    {
      p1 = &g_array_index (cache, HyScanLocationInternalTime, lindex);
      p2 = &g_array_index (cache, HyScanLocationInternalTime, rindex);

      middle = (p1->date + p1->time + p2->date + p2->time) / 2;

      dt = g_date_time_new_from_unix_utc (middle/1000000);
      year = g_date_time_get_year (dt);
      month = g_date_time_get_month (dt);
      day = g_date_time_get_day_of_month (dt);
      hour = g_date_time_get_hour (dt);
      minute = g_date_time_get_minute (dt);
      second = g_date_time_get_second (dt);
      microsecond = g_date_time_get_microsecond (dt);
      g_date_time_unref (dt);

      dt2 = g_date_time_new_utc (year, month, day, 0, 0, 0);
      output.date = g_date_time_to_unix (dt2) * 1e6;
      g_date_time_unref (dt2);

      dt2 = g_date_time_new_utc (1970, 1, 1, hour, minute, (gdouble)(second) + (gdouble)(microsecond)/1e6);
      output.time = g_date_time_to_unix (dt2) * 1e6;
      output.time_shift = MIN(p1->time_shift, p2->time_shift);
      output.validity = HYSCAN_LOCATION_VALID;
      g_date_time_unref (dt2);
    }

  return output;
}


/* Функция выдачи широты и долготы для заданного момента времени. */
HyScanLocationInternalData
hyscan_location_getter_latlong (HyScanDB *db,
                                 GArray  *source_list,
                                 GArray  *cache,
                                 gint32   source,
                                 gint64   time,
                                 gdouble  quality)
{
  HyScanLocationInternalData output = {HYSCAN_LOCATION_INTERNALDATA_INIT};

  HyScanLocationInternalData *p1, *p2;

  gdouble out_lat,
          out_lon;
  gdouble k_lat,
          k_lon,
          b_lat,
          b_lon;
  gdouble t1 = 0,
          t2 = 0,
          tout = 0;

  gint32 lindex = 0,
         rindex = 0;
  gint64 ltime = 0,
         rtime = 0;

  if (!hyscan_location_find_data (cache, source_list, source, time, &lindex, &rindex, &ltime, &rtime))
    return output;

  /* Если запрошенное время отличается от времени какого-нибудь ближайшего значения в базе меньше, чем на TIME_OF_VALIDITY
   * И это значение уже обработано, можно вернуть именно это значение.
   */
  if (ABS(ltime - time) < TIME_OF_VALIDITY)
    {
      output = g_array_index (cache, HyScanLocationInternalData, lindex);
      output.validity = HYSCAN_LOCATION_VALID;
      return output;
    }
  if (ABS(rtime - time) < TIME_OF_VALIDITY)
    {
      output = g_array_index (cache, HyScanLocationInternalData, rindex);
      output.validity = HYSCAN_LOCATION_VALID;
      return output;
    }

  // TODO: проверка граничных точек
  /* Поскольку трэк представляет собой ряд прямолинейных участков, то нам достаточно двух точек для нахождения искомой точки. */
  p1 = &g_array_index (cache, HyScanLocationInternalData, lindex);
  p2 = &g_array_index (cache, HyScanLocationInternalData, rindex);

  t1 = p1->data_time;
  t2 = p2->data_time;
  tout = (time - t1) / (t2-t1);
  t1 = 0;
  t2 = 1;
  k_lat = (p2->int_latitude - p1->int_latitude)/(t2 - t1);
  k_lon = (p2->int_longitude - p1->int_longitude)/(t2 - t1);
  b_lat = p1->int_latitude - k_lat * t1;
  b_lon = p1->int_longitude - k_lon * t1;

  /* Кладем p2 на определенную выше прямую. */
  out_lat = (k_lat * tout + b_lat);
  out_lon = (k_lon * tout + b_lon);

  output.int_latitude = out_lat;
  output.int_longitude = out_lon;
  output.data_time = time;
  output.validity = HYSCAN_LOCATION_VALID;
  return output;
}

/* Функция выдачи высоты для заданного момента времени. */
HyScanLocationInternalData
hyscan_location_getter_altitude (HyScanDB *db,
                                 GArray  *source_list,
                                 GArray  *cache,
                                 gint32   source,
                                 gint64   time,
                                 gdouble  quality)
{
  return hyscan_location_getter_gdouble1 (db, source_list, cache, source, time, quality);
}

/* Функция выдачи курса для заданного момента времени. */
HyScanLocationInternalData
hyscan_location_getter_track (HyScanDB *db,
                               GArray  *source_list,
                               GArray  *cache,
                               gint32   source,
                               gint64   time,
                               gdouble  quality)
{
  HyScanLocationSourcesList *source_info = &g_array_index (source_list, HyScanLocationSourcesList, source);
  HyScanLocationInternalData output = {HYSCAN_LOCATION_INTERNALDATA_INIT};
  HyScanLocationInternalData point  = {HYSCAN_LOCATION_INTERNALDATA_INIT},
                         prev_point = {HYSCAN_LOCATION_INTERNALDATA_INIT};

  /* Если курс берется непосредственно из NMEA. */
  if (source_info->source_type == HYSCAN_LOCATION_SOURCE_NMEA)
    return hyscan_location_getter_gdouble1 (db, source_list, cache, source, time, quality);

  /* Если курс высчитывается из координат. */
  if (source_info->source_type == HYSCAN_LOCATION_SOURCE_NMEA_COMPUTED)
    {
      point = hyscan_location_getter_gdouble2 (db, source_list, cache, source, time, quality, &prev_point);
      if (point.validity == HYSCAN_LOCATION_VALID && prev_point.validity == HYSCAN_LOCATION_VALID)
        {
          output.int_value = hyscan_location_track_calculator (prev_point.int_latitude, prev_point.int_longitude, point.int_latitude, point.int_longitude);
          output.validity = HYSCAN_LOCATION_VALID;
        }
    }
  return output;
}

/* Функция выдачи крена для заданного момента времени. */
HyScanLocationInternalData
hyscan_location_getter_roll (HyScanDB *db,
                              GArray  *source_list,
                              GArray  *cache,
                              gint32   source,
                              gint64   time,
                              gdouble  quality)
{
  return hyscan_location_getter_gdouble1 (db, source_list, cache, source, time, quality);
}

/* Функция выдачи дифферента для заданного момента времени. */
HyScanLocationInternalData
hyscan_location_getter_pitch (HyScanDB *db,
                               GArray  *source_list,
                               GArray  *cache,
                               gint32   source,
                               gint64   time,
                               gdouble  quality)
{
  return hyscan_location_getter_gdouble1 (db, source_list, cache, source, time, quality);
}

/* Функция выдачи скорости для заданного момента времени. */
HyScanLocationInternalData
hyscan_location_getter_speed (HyScanDB *db,
                               GArray  *source_list,
                               GArray  *cache,
                               gint32   source,
                               gint64   time,
                               gdouble  quality)
{
  HyScanLocationSourcesList *source_info = &g_array_index (source_list, HyScanLocationSourcesList, source);

  HyScanLocationInternalData output = {HYSCAN_LOCATION_INTERNALDATA_INIT};
  HyScanLocationInternalData point  = {HYSCAN_LOCATION_INTERNALDATA_INIT},
                             prev_point = {HYSCAN_LOCATION_INTERNALDATA_INIT};

  /* Если курс берется непосредственно из NMEA. */
  if (source_info->source_type == HYSCAN_LOCATION_SOURCE_NMEA)
    return hyscan_location_getter_gdouble1 (db, source_list, cache, source, time, quality);

  /* Если курс высчитывается из координат. */
  if (source_info->source_type == HYSCAN_LOCATION_SOURCE_NMEA_COMPUTED)
    {
      point = hyscan_location_getter_gdouble2 (db, source_list, cache, source, time, quality, &prev_point);
      if (point.validity == HYSCAN_LOCATION_VALID && prev_point.validity == HYSCAN_LOCATION_VALID)
        {
          output.int_value = hyscan_location_speed_calculator (prev_point.int_latitude,
                                                               prev_point.int_longitude,
                                                               point.int_latitude,
                                                               point.int_longitude,
                                                               point.data_time - prev_point.data_time);
          output.validity = HYSCAN_LOCATION_VALID;
        }
      return output;
    }
  return output;
}

/* Функция выдачи глубины для заданного момента времени. */
HyScanLocationInternalData
hyscan_location_getter_depth (HyScanDB *db,
                               GArray  *source_list,
                               GArray  *cache,
                               gint32   source,
                               gint64   time,
                               gdouble  quality)
{
  return hyscan_location_getter_gdouble1 (db, source_list, cache, source, time, quality);
}

/* Внутренняя функция выдачи значений типа HyScanLocationInternalData для заданного момента времени. */
HyScanLocationInternalData
hyscan_location_getter_gdouble1 (HyScanDB *db,
                                 GArray   *source_list,
                                 GArray   *cache,
                                 gint32    source,
                                 gint64    time,
                                 gdouble   quality)
{
  HyScanLocationSourcesList *source_info = &g_array_index (source_list, HyScanLocationSourcesList, source);

  HyScanLocationInternalData output = {0,0,0,0,0};
  HyScanLocationInternalData p1 = {HYSCAN_LOCATION_INTERNALDATA_INIT};
  gint32 lindex = 0,
         rindex = 0;
  gint64 ltime = 0,
         rtime = 0;

  /* Окно усреднения зависит от quality. 16 в случае quality = 0, 2 в случае quality = 1. */
  gint window_size = 16 - floor (7 * quality),
       num_of_points = 0;

  if (source_info->shift == -1)
    return output;
  if (!hyscan_location_find_data (cache, source_list, source, time, &lindex, &rindex, &ltime, &rtime))
    return output;

  for (num_of_points = 0; num_of_points < window_size; num_of_points++)
    {
      /* Проверяем, что точка существует. */
      if (lindex - num_of_points >=0)
        {
          p1 = g_array_index (cache, HyScanLocationInternalData, lindex - num_of_points);
          output.int_value += p1.int_value;
        }
      else
        {
          break;
        }
    }

  if (num_of_points > 0)
    {
      output.int_value /= num_of_points;
      output.validity = HYSCAN_LOCATION_VALID;
    }
  return output;
}

HyScanLocationInternalData
hyscan_location_getter_gdouble2 (HyScanDB                    *db,
                                 GArray                      *source_list,
                                 GArray                      *cache,
                                 gint32                       source,
                                 gint64                       time,
                                 gdouble                      quality,
                                 HyScanLocationInternalData  *prev_point)
{
  /* TODO: функция в текущем виде предназначена для разработки и отладки.
   * Когда будет понятно, что она работает надлежащим образом, нужно убрать все
   * промежуточные переменные и заменить их непосредственно значениями из структур с данными. */
  HyScanLocationSourcesList *source_info = &g_array_index (source_list, HyScanLocationSourcesList, source);

  HyScanLocationInternalData output = {HYSCAN_LOCATION_INTERNALDATA_INIT};

  HyScanLocationInternalData *p1, *p2;
  gdouble out_lat,
          out_lon;
  gdouble k_lat,
          k_lon,
          b_lat,
          b_lon;
  gdouble t1 = 0,
          t2 = 0,
          tout = 0;

  gint32 lindex = 0,
         rindex = 0;
  gint64 ltime = 0,
         rtime = 0;

  if (!hyscan_db_channel_find_data (db, source_info->channel_id, time, &lindex, &rindex, &ltime, &rtime))
    return output;
  lindex -= source_info->shift;
  rindex -= source_info->shift;

  /* Если запрошенное время отличается от времени какого-нибудь ближайшего значения в базе меньше, чем на 10 мс
   * И это значение уже обработано, можно вернуть именно это значение.
   */
  if (ABS(ltime - time) < TIME_OF_VALIDITY && lindex < source_info->processing_index)
    {
      if (lindex - 1 >= 0 && prev_point != NULL)
        {
          *prev_point = g_array_index (cache, HyScanLocationInternalData, lindex-1);
          if (prev_point->validity == HYSCAN_LOCATION_PROCESSED || prev_point->validity == HYSCAN_LOCATION_USER_VALID)
            prev_point->validity = HYSCAN_LOCATION_VALID;
        }
      if (lindex - 1 < 0 && prev_point != NULL)
        {
          *prev_point = g_array_index (cache, HyScanLocationInternalData, lindex);
          if (prev_point->validity == HYSCAN_LOCATION_PROCESSED || prev_point->validity == HYSCAN_LOCATION_USER_VALID)
            prev_point->validity = HYSCAN_LOCATION_VALID;
        }
      output = g_array_index (cache, HyScanLocationInternalData, lindex);
      output.validity = HYSCAN_LOCATION_VALID;
      return output;
    }
  if (ABS(rtime - time) < TIME_OF_VALIDITY && rindex < source_info->processing_index)
    {
      if (rindex - 1 >= 0 && prev_point != NULL)
        {
          *prev_point = g_array_index (cache, HyScanLocationInternalData, rindex-1);
          if (prev_point->validity == HYSCAN_LOCATION_PROCESSED || prev_point->validity == HYSCAN_LOCATION_USER_VALID)
            prev_point->validity = HYSCAN_LOCATION_VALID;
        }
      if (rindex - 1 < 0 && prev_point != NULL)
        {
          *prev_point = g_array_index (cache, HyScanLocationInternalData, rindex);
          if (prev_point->validity == HYSCAN_LOCATION_PROCESSED || prev_point->validity == HYSCAN_LOCATION_USER_VALID)
            prev_point->validity = HYSCAN_LOCATION_VALID;
        }
      output = g_array_index (cache, HyScanLocationInternalData, rindex);
      output.validity = HYSCAN_LOCATION_VALID;
      return output;
    }

  /* Возможна ситуация, когда в момент выполнения предыдущих двух проверок не проверяется вторая половина условия.
   * В этом случае возвращаем все нули, чтобы не выполнить нижележащий код.
   * Так же ноль возвращается, если правый индекс равен левому.
   */
  if (lindex > source_info->processing_index || rindex > source_info->processing_index || lindex == rindex)
    {
      return output;
    }

  /* Поскольку трэк представляет собой ряд прямолинейных участков, то нам достаточно двух точек для нахождения искомой точки. */
  if (prev_point != NULL)
    *prev_point = g_array_index (cache, HyScanLocationInternalData, lindex);
  p1 = &g_array_index (cache, HyScanLocationInternalData, lindex);
  p2 = &g_array_index (cache, HyScanLocationInternalData, rindex);

  t1 = p1->data_time;
  t2 = p2->data_time;
  tout = (time - t1) / (t2-t1);
  t1 = 0;
  t2 = 1;
  k_lat = (p2->int_latitude - p1->int_latitude)/(t2 - t1);
  k_lon = (p2->int_longitude - p1->int_longitude)/(t2 - t1);
  b_lat = p1->int_latitude - k_lat * t1;
  b_lon = p1->int_longitude - k_lon * t1;

  /* Кладем p2 на определенную выше прямую. */
  out_lat = (k_lat * tout + b_lat);
  out_lon = (k_lon * tout + b_lon);

  output.int_latitude = out_lat;
  output.int_longitude = out_lon;
  output.data_time = time;
  output.validity = HYSCAN_LOCATION_VALID;
  return output;
}
