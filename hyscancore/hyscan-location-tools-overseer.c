#include <hyscan-location-tools.h>

#define UNIX_1200 43200*1e6 /* Unix timestamp, соответствующий 12:00 в микросекундах. */
#define UNIX_2300 82800*1e6 /* Unix timestamp, соответствующий 23:00 в микросекундах. */

/* Функция слежения за датой и временем. */
void
hyscan_location_overseer_datetime (HyScanDB *db,
                                   GArray *source_list,
                                   GArray *cache,
                                   gint32 source,
                                   gdouble quality)
{
  gboolean status;
  HyScanLocationSourcesList *source_list_element = &g_array_index (source_list, HyScanLocationSourcesList, source);
  gint i;
  gint32 buffer_size;
  gchar *char_buffer = NULL;
  HyScanLocationGint1 *int1_data;
  gint32 data_range_first = 0,
  data_range_last = 0;
  gint64 db_time;
  gint64 time_shift;

  gint32 channel_id = source_list_element->channel_id;

  gint32 *shift = &(source_list_element->shift);
  gint32 *assembler_index = &(source_list_element->assembler_index);
  gint32 *processing_index = &(source_list_element->processing_index);

  if (source != -1)
    {
      /* 1. Смотрим, сколько у нас есть данных в канале. */
      status = hyscan_db_channel_get_data_range (db, channel_id,
                                                 &data_range_first, &data_range_last);

      /* 2. Устанавливаем сдвиг кэша и начальный индекс. */
      if (status && *shift == -1)
        {
          *shift = data_range_first;
          *assembler_index = data_range_first;
          *processing_index = 0;
        }

      /* 3. Собираем данные, кладем в локальный кэш. */
      if (status)
        {
          /* Увеличиваем размер кэша блоками по 512 элементов. */
          while (cache->len < data_range_last - *shift)
            cache = g_array_set_size (cache, cache->len + 512);

          /* Сборка данных. */
          while (*assembler_index <= data_range_last)
            {
              hyscan_db_channel_get_data (db, channel_id, *assembler_index, NULL, &buffer_size, NULL);
              char_buffer = g_realloc(char_buffer, buffer_size * sizeof(gchar));
              hyscan_db_channel_get_data (db, channel_id, *assembler_index, char_buffer, &buffer_size, &db_time);

              int1_data = &g_array_index (cache, HyScanLocationGint1, *assembler_index - *shift);
              *int1_data = hyscan_location_nmea_datetime_get (char_buffer);
              int1_data->db_time = db_time;

              (*assembler_index)++;
            }
        }

      /* 4. Обрабатываем данные из локального кэша и кладем обратно. */
      while (*processing_index < *assembler_index - *shift - 1)
        {
          /* В этом месте мы вычисляем временную сдвижку для данных.
           * Пускай окно для вычисления этой сдвижки будет равно 16 точкам.
           * В качестве задержки берется минимальная задержка за этот период времени.
           * Это примитивный алгоритм, который в будущем желательно модифицировать.
           * TODO: более сложный алгоритм вычисления временных сдвижек.
           */

          int1_data = &g_array_index (cache, HyScanLocationGint1, *processing_index);
          time_shift = int1_data->db_time - (int1_data->date + int1_data->time);
          for (i = 1; i < 16 && *processing_index-i >= 0; i++)
            {
              int1_data = &g_array_index (cache, HyScanLocationGint1, *processing_index-i);
              if (int1_data->db_time - (int1_data->date + int1_data->time) < time_shift)
                time_shift = int1_data->db_time - (int1_data->date + int1_data->time);
            }
          int1_data = &g_array_index (cache, HyScanLocationGint1, *processing_index);
          int1_data->time_shift = time_shift;
          int1_data->validity = TRUE;

          (*processing_index)++;
        }
    }
}

/* Функция слежения за широтой и долготой. */
void
hyscan_location_overseer_latlong (HyScanDB* db,
                                  GArray *source_list,
                                  GArray *cache,
                                  gint32 source,
                                  GArray *datetime_cache,
                                  gint32 datetime_source,
                                  gdouble quality)
{
  gboolean status;

  gint32 buffer_size;
  gchar *char_buffer = NULL;
  HyScanLocationGdouble2 double2_data;
  HyScanLocationGdouble2 *val;
  HyScanLocationGint1 datetime;
  gint32 data_range_first = 0,
  data_range_last = 0;
  gint64 db_time;
  HyScanLocationSourcesList *source_list_element = &g_array_index (source_list, HyScanLocationSourcesList, source);
  gint32 channel_id = source_list_element->channel_id;

  gint32 *shift = &(source_list_element->shift);
  gint32 *assembler_index = &(source_list_element->assembler_index);
  gint32 *processing_index = &(source_list_element->processing_index);

  //tempcache = *cache;
  if (source != -1)
    {
      /* 1. Смотрим, сколько у нас есть данных в канале. */
      status = hyscan_db_channel_get_data_range (db, channel_id, &data_range_first, &data_range_last);

      /* 2. Устанавливаем сдвиг кэша и начальный индекс. */
      if (status && *shift == -1)
        {
          *shift = data_range_first;
          *assembler_index = data_range_first;
          *processing_index = 0;
        }

      /* 3. Собираем данные, кладем в локальный кэш. */
      if (status)
        {
          /* Увеличиваем размер кэша блоками по 512 элементов. */
          while (cache->len < (data_range_last - *shift))
             {
                cache = g_array_set_size (cache, cache->len + 512 );
              }
          /* Сборка данных. */
          while (*assembler_index <= data_range_last)
            {
              hyscan_db_channel_get_data (db, channel_id, *assembler_index, NULL, &buffer_size, NULL);
              char_buffer = g_realloc(char_buffer, buffer_size * sizeof(gchar));
              hyscan_db_channel_get_data (db, channel_id, *assembler_index, char_buffer, &buffer_size, &db_time);

              double2_data = hyscan_location_nmea_latlong_get (char_buffer);
              double2_data.db_time = db_time;

              val = &g_array_index (cache, HyScanLocationGdouble2, *assembler_index - *shift);
              val->value1 = double2_data.value1;
              val->value2 = double2_data.value2;
              val->data_time = double2_data.data_time;
              val->db_time = double2_data.db_time;
              val->validity = double2_data.validity;

              (*assembler_index)++;
            }
        }

      /* 4. Обрабатываем данные из локального кэша и кладем обратно. */
      while (*processing_index < *assembler_index - *shift - 2)
        {
          /* Нужно пропустить начало данных (первые 2 индекса). */
          if (*processing_index - *shift >= 2)
            {
              hyscan_location_4_point_2d_bezier  (cache,
                                                  *processing_index-2,
                                                  *processing_index-1,
                                                  *processing_index-0,
                                                  *processing_index+1,
                                                  quality);
            }
          /* Лучшее место для временных сдвигов. */
          val = &g_array_index (cache, HyScanLocationGdouble2, *processing_index);
          datetime = hyscan_location_getter_datetime (db, source_list, datetime_cache, datetime_source, val->db_time, quality);
          /* В данных хранится только время с начала суток. Требуется добавить дату. */
          val->data_time += datetime.date;
          /* Считаем, что если в val время до полудня, а в datetime - время больше 23:00,
           * то произошел переход на следующие сутки и требуется добавить эти сутки.
           * TODO: перевести всё на GDateTime.
           */
          if (val->data_time < UNIX_1200 && datetime.time > UNIX_2300)
            val->data_time += 86400 * 1e6;

          /* Теперь можно учитывать временной сдвиг. */
          val->data_time += datetime.time_shift;
          (*processing_index)++;
        }
    }
}

/* Функция слежения за высотой. */
void
hyscan_location_overseer_altitude (HyScanDB *db,
                                   GArray *source_list,
                                   GArray *cache,
                                   gint32 source,
                                   GArray *datetime_cache,
                                   gint32 datetime_source,
                                   gdouble quality)
{
  gboolean status;
  HyScanLocationSourcesList *source_list_element = &g_array_index (source_list, HyScanLocationSourcesList, source);

  gint32 buffer_size;
  gchar *char_buffer = NULL;
  HyScanLocationGdouble1 double1_data;
  HyScanLocationGdouble1 *val;
  HyScanLocationGint1 datetime;

  gint32 data_range_first = 0,
  data_range_last = 0;
  gint64 db_time;

  gint32 channel_id = source_list_element->channel_id;

  gint32 *shift = &(source_list_element->shift);
  gint32 *assembler_index = &(source_list_element->assembler_index);
  gint32 *processing_index = &(source_list_element->processing_index);

  if (source != -1)
    {
      /* 1. Смотрим, сколько у нас есть данных в канале. */
      status = hyscan_db_channel_get_data_range (db, channel_id,
                                                 &data_range_first, &data_range_last);

      /* 2. Устанавливаем сдвиг кэша и начальный индекс. */
      if (status && *shift == -1)
        {
          *shift = data_range_first;
          *assembler_index = data_range_first;
          *processing_index = 0;
        }

      /* 3. Собираем данные, кладем в локальный кэш. */
      if (status)
        {
          /* Увеличиваем размер кэша блоками по 512 элементов. */
          while (cache->len < data_range_last - *shift)
            cache = g_array_set_size (cache, cache->len + 512);

          /* Сборка данных. */
          while (*assembler_index <= data_range_last)
            {
              hyscan_db_channel_get_data (db, channel_id, *assembler_index, NULL, &buffer_size, NULL);
              char_buffer = g_realloc(char_buffer, buffer_size * sizeof(gchar));
              hyscan_db_channel_get_data (db, channel_id, *assembler_index, char_buffer, &buffer_size, &db_time);

              double1_data = hyscan_location_nmea_altitude_get (char_buffer);
              double1_data.db_time = db_time;

              g_array_remove_index (cache, *assembler_index - *shift);
              g_array_insert_val (cache, *assembler_index - *shift, double1_data);

              (*assembler_index)++;
            }
        }

      /* 4. Обрабатываем данные из локального кэша и кладем обратно. */
      while (*processing_index < *assembler_index - *shift - 1)
        {
          /* Нужно пропустить начало данных (первые 2 индекса). */
          if (*processing_index - *shift >= 2)
            {
              hyscan_location_3_point_average (cache,
                                               *processing_index - 2,
                                               *processing_index - 1,
                                               *processing_index - 0,
                                               quality);
            }
          /* Лучшее место для временных сдвигов. */
          val = &g_array_index (cache, HyScanLocationGdouble1, *processing_index);
          datetime = hyscan_location_getter_datetime (db, source_list, datetime_cache, datetime_source, val->db_time, quality);
          /* В данных хранится только время с начала суток. Требуется добавить дату. */
          val->data_time += datetime.date;
          /* Считаем, что если в val время до полудня, а в datetime - время больше 23:00,
           * то произошел переход на следующие сутки и требуется добавить эти сутки.
           * TODO: перевести всё на GDateTime.
           */
          if (val->data_time < UNIX_1200 && datetime.time > UNIX_2300)
            val->data_time += 86400 * 1e6;

          /* Теперь можно учитывать временной сдвиг. */
          val->data_time += datetime.time_shift;
          (*processing_index)++;
        }
    }
}

/* Функция слежения за курсом. */
void
hyscan_location_overseer_track (HyScanDB *db,
                                GArray *source_list,
                                GArray *cache,
                                gint32 source,
                                GArray *datetime_cache,
                                gint32 datetime_source,
                                gdouble quality)
{
  gboolean status;
  HyScanLocationSourcesList *source_list_element = &g_array_index (source_list, HyScanLocationSourcesList, source);

  gint32 buffer_size;
  gchar *char_buffer = NULL;
  HyScanLocationGdouble2 double2_data;
  HyScanLocationGdouble1 double1_data;
  HyScanLocationGdouble2 *val2;
  HyScanLocationGdouble1 *val1;
  HyScanLocationGint1 datetime;

  gint32 data_range_first = 0,
  data_range_last = 0;
  gint64 db_time;

  gint32 channel_id = source_list_element->channel_id;

  gint32 *shift = &(source_list_element->shift);
  gint32 *assembler_index = &(source_list_element->assembler_index);
  gint32 *processing_index = &(source_list_element->processing_index);

  if (source != -1)
    {
      /* 1. Смотрим, сколько у нас есть данных в канале. */
      status = hyscan_db_channel_get_data_range (db, channel_id,
                                                 &data_range_first, &data_range_last);

      /* 2. Устанавливаем сдвиг кэша и начальный индекс. */
      if (status && *shift == -1)
        {
          *shift = data_range_first;
          *assembler_index = data_range_first;
          *processing_index = 0;
        }

      /* 3. Собираем данные, кладем в локальный кэш. */
      if (status)
        {
          /* Увеличиваем размер кэша блоками по 512 элементов. */
          while (cache->len < data_range_last - *shift)
            cache = g_array_set_size (cache, cache->len + 512);

          /* Сборка данных. */
          while (*assembler_index <= data_range_last)
            {
              hyscan_db_channel_get_data (db, channel_id, *assembler_index, NULL, &buffer_size, NULL);
              char_buffer = g_realloc(char_buffer, buffer_size * sizeof(gchar));
              hyscan_db_channel_get_data (db, channel_id, *assembler_index, char_buffer, &buffer_size, &db_time);

              /* Достаем либо значение из строки, либо координаты. */
              if (source_list_element->source_type == HYSCAN_LOCATION_SOURCE_NMEA)
                {
                  double1_data = hyscan_location_nmea_track_get (char_buffer);
                  double1_data.db_time = db_time;

                  g_array_remove_index (cache, *assembler_index - *shift);
                  g_array_insert_val (cache, *assembler_index - *shift, double1_data);
                }
              if (source_list_element->source_type == HYSCAN_LOCATION_SOURCE_NMEA_COMPUTED)
                {
                  double2_data = hyscan_location_nmea_latlong_get (char_buffer);
                  double2_data.db_time = db_time;
                  g_array_remove_index (cache, *assembler_index - *shift);
                  g_array_insert_val (cache, *assembler_index - *shift, double2_data);
                }

              (*assembler_index)++;
            }
        }

      /* 4. Обрабатываем данные из локального кэша и кладем обратно. */
      while (*processing_index < *assembler_index - *shift - 1)
        {
          /* Нужно пропустить начало данных (первые 2 индекса). */
          if (*processing_index - *shift >= 2)
            {
              if (source_list_element->source_type == HYSCAN_LOCATION_SOURCE_NMEA)
                {
                  hyscan_location_3_point_average (cache,
                                                   *processing_index - 2,
                                                   *processing_index - 1,
                                                   *processing_index - 0,
                                                   quality);
                }
              if (source_list_element->source_type == HYSCAN_LOCATION_SOURCE_NMEA_COMPUTED)
                {
                  hyscan_location_4_point_2d_bezier  (cache,
                                                      *processing_index-2,
                                                      *processing_index-1,
                                                      *processing_index-0,
                                                      *processing_index+1,
                                                      quality);
                }
            }
          /* Лучшее место для временных сдвигов. */
          /* В данных хранится только время с начала суток. Требуется добавить дату. */
          /* Считаем, что если в val время до полудня, а в datetime - время больше 23:00,
          * то произошел переход на следующие сутки и требуется добавить эти сутки.
          * TODO: перевести всё на GDateTime.
          */
          if (source_list_element->source_type == HYSCAN_LOCATION_SOURCE_NMEA)
            {
              val1 = &g_array_index (cache, HyScanLocationGdouble1, *processing_index);
              datetime = hyscan_location_getter_datetime (db, source_list, datetime_cache, datetime_source, val1->db_time, quality);
              val1->data_time += datetime.date;
              if (val1->data_time < UNIX_1200 && datetime.time > UNIX_2300)
                val1->data_time += 86400 * 1e6;
              val1->data_time += datetime.time_shift;
            }
          if (source_list_element->source_type == HYSCAN_LOCATION_SOURCE_NMEA_COMPUTED)
            {
              val2 = &g_array_index (cache, HyScanLocationGdouble2, *processing_index);
              datetime = hyscan_location_getter_datetime (db, source_list, datetime_cache, datetime_source, val2->db_time, quality);
              val2->data_time += datetime.date;
              if (val2->data_time < UNIX_1200 && datetime.time > UNIX_2300)
                val2->data_time += 86400 * 1e6;
              val2->data_time += datetime.time_shift;
            }

          (*processing_index)++;
        }
    }
}

/* Функция слежения за креном. */
void
hyscan_location_overseer_roll (HyScanDB *db,
                               GArray *source_list,
                               GArray *cache,
                               gint32 source,
                               GArray *datetime_cache,
                               gint32 datetime_source,
                               gdouble quality)
{
  gboolean status;
  HyScanLocationSourcesList *source_list_element = &g_array_index (source_list, HyScanLocationSourcesList, source);

  gint32 buffer_size;
  gchar *char_buffer = NULL;
  HyScanLocationGdouble1 double1_data;
  HyScanLocationGdouble1 *val;
  HyScanLocationGint1 datetime;
  gint32 data_range_first = 0,
  data_range_last = 0;
  gint64 db_time;

  gint32 channel_id = source_list_element->channel_id;

  gint32 *shift = &(source_list_element->shift);
  gint32 *assembler_index = &(source_list_element->assembler_index);
  gint32 *processing_index = &(source_list_element->processing_index);

  if (source != -1)
    {
      /* 1. Смотрим, сколько у нас есть данных в канале. */
      status = hyscan_db_channel_get_data_range (db, channel_id,
                                                 &data_range_first, &data_range_last);
      /* 2. Устанавливаем сдвиг кэша и начальный индекс. */
      if (status && *shift == -1)
        {
          *shift = data_range_first;
          *assembler_index = data_range_first;
          *processing_index = 0;
        }
      /* 3. Собираем данные, кладем в локальный кэш. */
      if (status)
        {
          /* Увеличиваем размер кэша блоками по 512 элементов. */
          while (cache->len < data_range_last - *shift)
            cache = g_array_set_size (cache, cache->len + 512);
          /* Сборка данных. */
          while (*assembler_index <= data_range_last)
            {
              hyscan_db_channel_get_data (db, channel_id, *assembler_index, NULL, &buffer_size, NULL);
              char_buffer = g_realloc(char_buffer, buffer_size * sizeof(gchar));
              hyscan_db_channel_get_data (db, channel_id, *assembler_index, char_buffer, &buffer_size, &db_time);

              double1_data = hyscan_location_nmea_roll_get (char_buffer);
              double1_data.db_time = db_time;

              g_array_remove_index (cache, *assembler_index - *shift);
              g_array_insert_val (cache, *assembler_index - *shift, double1_data);
              (*assembler_index)++;
            }
        }
      /* 4. Обрабатываем данные из локального кэша и кладем обратно. */
      while (*processing_index < *assembler_index - *shift - 1)
        {
          /* Нужно пропустить начало данных (первые 2 индекса). */
          if (*processing_index - *shift >= 2)
            {
              // roll filtration here
            }
          /* Лучшее место для временных сдвигов. */
          val = &g_array_index (cache, HyScanLocationGdouble1, *processing_index);
          datetime = hyscan_location_getter_datetime (db, source_list, datetime_cache, datetime_source, val->db_time, quality);
          /* В данных хранится только время с начала суток. Требуется добавить дату. */
          val->data_time += datetime.date;
          /* Считаем, что если в val время до полудня, а в datetime - время больше 23:00,
           * то произошел переход на следующие сутки и требуется добавить эти сутки.
           * TODO: перевести всё на GDateTime.
           */
          if (val->data_time < UNIX_1200 && datetime.time > UNIX_2300)
            val->data_time += 86400 * 1e6;

          /* Теперь можно учитывать временной сдвиг. */
          val->data_time += datetime.time_shift;
          (*processing_index)++;
        }
    }
}

/* Функция слежения за дифферентом. */
void
hyscan_location_overseer_pitch (HyScanDB *db,
                                GArray *source_list,
                                GArray *cache,
                                gint32 source,
                                GArray *datetime_cache,
                                gint32 datetime_source,
                                gdouble quality)
{
  gboolean status;
  HyScanLocationSourcesList *source_list_element = &g_array_index (source_list, HyScanLocationSourcesList, source);

  gint32 buffer_size;
  gchar *char_buffer = NULL;
  HyScanLocationGdouble1 double1_data;
  HyScanLocationGdouble1 *val;
  HyScanLocationGint1 datetime;
  gint32 data_range_first = 0,
  data_range_last = 0;
  gint64 db_time;

  gint32 channel_id = source_list_element->channel_id;

  gint32 *shift = &(source_list_element->shift);
  gint32 *assembler_index = &(source_list_element->assembler_index);
  gint32 *processing_index = &(source_list_element->processing_index);

  if (source != -1)
    {
      /* 1. Смотрим, сколько у нас есть данных в канале. */
      status = hyscan_db_channel_get_data_range (db, channel_id,
                                                 &data_range_first, &data_range_last);

      /* 2. Устанавливаем сдвиг кэша и начальный индекс. */
      if (status && *shift == -1)
        {
          *shift = data_range_first;
          *assembler_index = data_range_first;
          *processing_index = 0;
        }

      /* 3. Собираем данные, кладем в локальный кэш. */
      if (status)
        {
          /* Увеличиваем размер кэша блоками по 512 элементов. */
          while (cache->len < data_range_last - *shift)
            cache = g_array_set_size (cache, cache->len + 512);

          /* Сборка данных. */
          while (*assembler_index <= data_range_last)
            {
              hyscan_db_channel_get_data (db, channel_id, *assembler_index, NULL, &buffer_size, NULL);
              char_buffer = g_realloc(char_buffer, buffer_size * sizeof(gchar));
              hyscan_db_channel_get_data (db, channel_id, *assembler_index, char_buffer, &buffer_size, &db_time);

              double1_data = hyscan_location_nmea_pitch_get (char_buffer);
              double1_data.db_time = db_time;

              g_array_remove_index (cache, *assembler_index - *shift);
              g_array_insert_val (cache, *assembler_index - *shift, double1_data);

              (*assembler_index)++;
            }
        }

      /* 4. Обрабатываем данные из локального кэша и кладем обратно. */
      while (*processing_index < *assembler_index - *shift - 1)
        {
          /* Нужно пропустить начало данных (первые 2 индекса). */
          if (*processing_index - *shift >= 2)
            {
              // pitch filtration here
            }
          /* Лучшее место для временных сдвигов. */
          val = &g_array_index (cache, HyScanLocationGdouble1, *processing_index);
          datetime = hyscan_location_getter_datetime (db, source_list, datetime_cache, datetime_source, val->db_time, quality);
          /* В данных хранится только время с начала суток. Требуется добавить дату. */
          val->data_time += datetime.date;
          /* Считаем, что если в val время до полудня, а в datetime - время больше 23:00,
           * то произошел переход на следующие сутки и требуется добавить эти сутки.
           * TODO: перевести всё на GDateTime.
           */
          if (val->data_time < UNIX_1200 && datetime.time > UNIX_2300)
            val->data_time += 86400 * 1e6;

          /* Теперь можно учитывать временной сдвиг. */
          val->data_time += datetime.time_shift;
          (*processing_index)++;
        }
    }
}

/* Функция слежения за скоростью. */
void
hyscan_location_overseer_speed (HyScanDB *db,
                                GArray *source_list,
                                GArray *cache,
                                gint32 source,
                                GArray *datetime_cache,
                                gint32 datetime_source,
                                gdouble quality)
{
  gboolean status;
  HyScanLocationSourcesList *source_list_element = &g_array_index (source_list, HyScanLocationSourcesList, source);

  gint32 buffer_size;
  gchar *char_buffer = NULL;
  HyScanLocationGdouble2 double2_data;
  HyScanLocationGdouble1 double1_data;
  HyScanLocationGdouble2 *val2;
  HyScanLocationGdouble1 *val1;
  HyScanLocationGint1 datetime;
  gint32 data_range_first = 0,
  data_range_last = 0;
  gint64 db_time;

  gint32 channel_id = source_list_element->channel_id;

  gint32 *shift = &(source_list_element->shift);
  gint32 *assembler_index = &(source_list_element->assembler_index);
  gint32 *processing_index = &(source_list_element->processing_index);

  if (source != -1)
    {
      /* 1. Смотрим, сколько у нас есть данных в канале. */
      status = hyscan_db_channel_get_data_range (db, channel_id,
                                                 &data_range_first, &data_range_last);

      /* 2. Устанавливаем сдвиг кэша и начальный индекс. */
      if (status && *shift == -1)
        {
          *shift = data_range_first;
          *assembler_index = data_range_first;
          *processing_index = 0;
        }

      /* 3. Собираем данные, кладем в локальный кэш. */
      if (status)
        {
          /* Увеличиваем размер кэша блоками по 512 элементов. */
          while (cache->len < data_range_last - *shift)
            cache = g_array_set_size (cache, cache->len + 512);

          /* Сборка данных. */
          while (*assembler_index <= data_range_last)
            {
              hyscan_db_channel_get_data (db, channel_id, *assembler_index, NULL, &buffer_size, NULL);
              char_buffer = g_realloc(char_buffer, buffer_size * sizeof(gchar));
              hyscan_db_channel_get_data (db, channel_id, *assembler_index, char_buffer, &buffer_size, &db_time);

              /* Достаем либо значение из строки, либо координаты. */
              if (source_list_element->source_type == HYSCAN_LOCATION_SOURCE_NMEA)
                {
                  double1_data = hyscan_location_nmea_speed_get (char_buffer);
                  double1_data.db_time = db_time;

                  g_array_remove_index (cache, *assembler_index - *shift);
                  g_array_insert_val (cache, *assembler_index - *shift, double1_data);
                }
              if (source_list_element->source_type == HYSCAN_LOCATION_SOURCE_NMEA_COMPUTED)
                {
                  double2_data = hyscan_location_nmea_latlong_get (char_buffer);
                  double2_data.db_time = db_time;
                  g_array_remove_index (cache, *assembler_index - *shift);
                  g_array_insert_val (cache, *assembler_index - *shift, double2_data);
                }

              (*assembler_index)++;
            }
        }

      /* 4. Обрабатываем данные из локального кэша и кладем обратно. */
      while (*processing_index < *assembler_index - *shift - 1)
        {
          /* Нужно пропустить начало данных (первые 2 индекса). */
          if (*processing_index - *shift >= 2)
            {
              if (source_list_element->source_type == HYSCAN_LOCATION_SOURCE_NMEA)
                {
                  hyscan_location_3_point_average (cache,
                                                   *processing_index - 2,
                                                   *processing_index - 1,
                                                   *processing_index - 0,
                                                   quality);
                }
              if (source_list_element->source_type == HYSCAN_LOCATION_SOURCE_NMEA_COMPUTED)
                {
                  hyscan_location_4_point_2d_bezier  (cache,
                                                      *processing_index-2,
                                                      *processing_index-1,
                                                      *processing_index-0,
                                                      *processing_index+1,
                                                      quality);
                }
            }
          /* Лучшее место для временных сдвигов. */
          /* В данных хранится только время с начала суток. Требуется добавить дату. */
          /* Считаем, что если в val время до полудня, а в datetime - время больше 23:00,
          * то произошел переход на следующие сутки и требуется добавить эти сутки.
          * TODO: перевести всё на GDateTime.
          */
          if (source_list_element->source_type == HYSCAN_LOCATION_SOURCE_NMEA)
            {
              val1 = &g_array_index (cache, HyScanLocationGdouble1, *processing_index);
              datetime = hyscan_location_getter_datetime (db, source_list, datetime_cache, datetime_source, val1->db_time, quality);
              val1->data_time += datetime.date;
              if (val1->data_time < UNIX_1200 && datetime.time > UNIX_2300)
                val1->data_time += 86400 * 1e6;
              val1->data_time += datetime.time_shift;
            }
          if (source_list_element->source_type == HYSCAN_LOCATION_SOURCE_NMEA_COMPUTED)
            {
              val2 = &g_array_index (cache, HyScanLocationGdouble2, *processing_index);
              datetime = hyscan_location_getter_datetime (db, source_list, datetime_cache, datetime_source, val2->db_time, quality);
              val2->data_time += datetime.date;
              if (val2->data_time < UNIX_1200 && datetime.time > UNIX_2300)
                val2->data_time += 86400 * 1e6;
              val2->data_time += datetime.time_shift;
            }

          (*processing_index)++;
        }
    }
}

/* Функция слежения за глубиной. */
void
hyscan_location_overseer_depth (HyScanDB *db,
                                GArray *source_list,
                                GArray *cache,
                                gint32 source,
                                GArray *soundspeed,
                                gdouble quality)
{
  gboolean status;
  HyScanLocationSourcesList *source_list_element = &g_array_index (source_list, HyScanLocationSourcesList, source);

  gint32 buffer_size;
  gchar *char_buffer = NULL;
  gfloat *float_buffer = NULL;
  HyScanLocationGdouble1 double1_data;
  gint32 data_range_first = 0,
  data_range_last = 0;
  gint64 db_time;

  gint32 channel_id = source_list_element->channel_id;

  gint32 *shift = &(source_list_element->shift);
  gint32 *assembler_index = &(source_list_element->assembler_index);
  gint32 *processing_index = &(source_list_element->processing_index);

  if (source != -1)
    {
      /* 1. Смотрим, сколько у нас есть данных в канале. */
      switch (source_list_element->source_type)
        {
        case HYSCAN_LOCATION_SOURCE_NMEA:
        case HYSCAN_LOCATION_SOURCE_NMEA_COMPUTED:
          status = hyscan_db_channel_get_data_range (db, channel_id,&data_range_first, &data_range_last);
          break;
        case HYSCAN_LOCATION_SOURCE_ECHOSOUNDER:
        case HYSCAN_LOCATION_SOURCE_SONAR_PORT:
        case HYSCAN_LOCATION_SOURCE_SONAR_STARBOARD:
        case HYSCAN_LOCATION_SOURCE_SONAR_HIRES_PORT:
        case HYSCAN_LOCATION_SOURCE_SONAR_HIRES_STARBOARD:
          status = hyscan_data_channel_get_range (source_list_element->dchannel, &data_range_first, &data_range_last);
          break;
        default:
          status = FALSE;
          break;
        }

      /* 2. Устанавливаем сдвиг кэша и начальный индекс. */
      if (status && *shift == -1)
        {
          *shift = data_range_first;
          *assembler_index = data_range_first;
          *processing_index = 0;
        }

      /* 3. Собираем данные, кладем в локальный кэш. */
      if (status)
        {
          /* Увеличиваем размер кэша блоками по 512 элементов. */
          while (cache->len < data_range_last - *shift)
            cache = g_array_set_size (cache, cache->len + 512);

          /* Сборка данных. */
          while (*assembler_index <= data_range_last)
            {

              switch (source_list_element->source_type)
                {
                case HYSCAN_LOCATION_SOURCE_NMEA:
                  hyscan_db_channel_get_data (db, channel_id, *assembler_index, NULL, &buffer_size, NULL);
                  char_buffer = g_realloc(char_buffer, buffer_size * sizeof(gchar));
                  hyscan_db_channel_get_data (db, channel_id, *assembler_index, char_buffer, &buffer_size, &db_time);
                  double1_data = hyscan_location_nmea_depth_get (char_buffer);
                  break;

                case HYSCAN_LOCATION_SOURCE_ECHOSOUNDER:
                  buffer_size = hyscan_data_channel_get_values_count(source_list_element->dchannel, *assembler_index);
                  float_buffer = g_realloc(float_buffer, buffer_size * sizeof(gfloat));
                  hyscan_data_channel_get_amplitude_values (source_list_element->dchannel, *assembler_index, float_buffer, &buffer_size, &db_time);
                  double1_data = hyscan_location_echosounder_depth_get (float_buffer, buffer_size, source_list_element->discretization_frequency, soundspeed);
                  break;

                case HYSCAN_LOCATION_SOURCE_SONAR_PORT:
                case HYSCAN_LOCATION_SOURCE_SONAR_STARBOARD:
                case HYSCAN_LOCATION_SOURCE_SONAR_HIRES_PORT:
                case HYSCAN_LOCATION_SOURCE_SONAR_HIRES_STARBOARD:
                  buffer_size = hyscan_data_channel_get_values_count(source_list_element->dchannel, *assembler_index);
                  float_buffer = g_realloc(float_buffer, buffer_size * sizeof(gfloat));
                  hyscan_data_channel_get_amplitude_values (source_list_element->dchannel, *assembler_index, float_buffer, &buffer_size, &db_time);
                  double1_data = hyscan_location_sonar_depth_get (float_buffer, buffer_size, source_list_element->discretization_frequency, soundspeed);
                  break;
                default:
                  double1_data.validity = 0;
                }

              double1_data.db_time = db_time;

              g_array_remove_index (cache, *assembler_index - *shift);
              g_array_insert_val (cache, *assembler_index - *shift, double1_data);

              (*assembler_index)++;
            }
        }

      /* 4. Обрабатываем данные из локального кэша и кладем обратно. */
      while (*processing_index < *assembler_index - *shift - 1)
        {
          /* Нужно пропустить начало данных (первые 2 индекса). */
          if (*processing_index - *shift >= 2)
            {
              hyscan_location_3_point_average (cache,
                                               *processing_index - 2,
                                               *processing_index - 1,
                                               *processing_index - 0,
                                               quality);
            }
          (*processing_index)++;
        }
    }
}
