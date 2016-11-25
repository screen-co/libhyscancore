/*
 * Файл содержит следующие группы функций:
 *
 * Функции надзирателя.
 *
 */
#include <hyscan-location-tools.h>

#define UNIX_1200 43200*1e6 /* Unix timestamp, соответствующий 12:00 в микросекундах. */
#define UNIX_2300 82800*1e6 /* Unix timestamp, соответствующий 23:00 в микросекундах. */

/* Функция слежения за датой и временем. */
void
hyscan_location_overseer_datetime (HyScanDB                    *db,
                                   GArray                      *source_list,
                                   HyScanLocationLocalCaches   *caches,
                                   HyScanLocationSourceIndices *indices,
                                   gdouble                      quality)
{
  gboolean status;
  gint32 source = indices->datetime_source;

  HyScanLocationSourcesList *source_info = &g_array_index (source_list, HyScanLocationSourcesList, source);
  gint i;
  HyScanLocationInternalTime  datetime, *datetime_p;
  guint32 data_range_first = 0;
  guint32 data_range_last = 0;
  gint64 time_shift;

  gint32 channel_id = source_info->channel_id;

  guint32 *db_index = &(source_info->db_index);
  guint32 *shift = &(source_info->shift);
  guint32 *assembler_index = &(source_info->assembler_index);
  guint32 *processing_index = &(source_info->processing_index);

  GArray *cache = caches->datetime_cache;
  if (source != -1)
    {
      /* 1. Смотрим, сколько у нас есть данных в канале. */
      status = hyscan_db_channel_get_data_range (db, channel_id, &data_range_first, &data_range_last);

      /* 2. Устанавливаем сдвиг кэша и начальный индекс. */
      if (status && *shift == G_MAXUINT32)
        {
          *shift = data_range_first;
          *db_index = data_range_first;
          *assembler_index = 0;
          *processing_index = 0;
        }

      /* 3. Собираем данные, кладем в локальный кэш. */
      if (status)
        {
          /* Увеличиваем размер кэша блоками по 512 элементов. */
          while (cache->len < data_range_last - *shift)
            cache = g_array_set_size (cache, cache->len + 512);

          /* Сборка данных. */
          while (*db_index <= data_range_last)
            {
              /* Разбираем соответствующую строку данных. */
              datetime = hyscan_location_assembler_datetime (db, source_list, source, *db_index);

              /* Если строка валидна, кладем её в кэш. */
              if (datetime.validity == HYSCAN_LOCATION_ASSEMBLED);
                {
                  datetime_p = &g_array_index (cache, HyScanLocationInternalTime, *assembler_index - *shift);
                  *datetime_p = datetime;
                  (*assembler_index)++;
                }

              (*db_index)++;
            }
        }

      /* 4. Обрабатываем данные из локального кэша и кладем обратно. */
      while (*processing_index < *assembler_index)
        {
          /* В этом месте мы вычисляем временную сдвижку для данных.
           * Пускай окно для вычисления этой сдвижки будет равно 16 точкам.
           * В качестве задержки берется минимальная задержка за этот период времени.
           * Это примитивный алгоритм, который в будущем желательно модифицировать.
           * TODO: более сложный алгоритм вычисления временных сдвижек.
           */
          datetime_p = &g_array_index (cache, HyScanLocationInternalTime, *processing_index);
          time_shift = datetime_p->db_time - (datetime_p->date + datetime_p->time);
          for (i = 1; i < 16 && *processing_index-i >= 0; i++)
            {
              datetime_p = &g_array_index (cache, HyScanLocationInternalTime, *processing_index-i);
              if (datetime_p->db_time - (datetime_p->date + datetime_p->time) < time_shift)
                time_shift = datetime_p->db_time - (datetime_p->date + datetime_p->time);
            }
          datetime_p = &g_array_index (cache, HyScanLocationInternalTime, *processing_index);
          datetime_p->time_shift = time_shift;
          datetime_p->validity = HYSCAN_LOCATION_VALID;

          (*processing_index)++;
        }
    }
}

/* Функция слежения за широтой и долготой. */
void
hyscan_location_overseer_latlong (HyScanDB                    *db,
                                  GArray                      *source_list,
                                  GArray                      *params,
                                  HyScanLocationLocalCaches   *caches,
                                  HyScanLocationSourceIndices *indices,
                                  gdouble                      quality)
{
  gboolean status;

  gint32 source = indices->latlong_source;
  gint32 datetime_source = indices->datetime_source;
  GArray *cache = caches->latlong_cache;
  GArray *datetime_cache = caches->datetime_cache;

  guint32 data_range_first = 0;
  guint32 data_range_last = 0;
  gint i;

  HyScanLocationSourcesList *source_info = &g_array_index (source_list, HyScanLocationSourcesList, source);

  HyScanLocationUserParameters param_info;
  GArray *params_temp = NULL;

  HyScanLocationInternalData  latlong, *latlong_p;

  gint32 channel_id = source_info->channel_id;
  guint32 *shift = &(source_info->shift);
  guint32 *db_index = &(source_info->db_index);
  guint32 *assembler_index = &(source_info->assembler_index);
  guint32 *processing_index = &(source_info->processing_index);
  guint32 *preprocessing_index = &(source_info->preprocessing_index);
  guint32 *thresholder_prev_index = &(source_info->thresholder_prev_index);
  guint32 *thresholder_next_index = &(source_info->thresholder_next_index);

  gboolean thresholder_status = TRUE;
  gboolean is_writeable = FALSE;

  if (source != -1)
    {
      /* 1. Смотрим, сколько у нас есть данных в канале. */
      status = hyscan_db_channel_get_data_range (db, channel_id, &data_range_first, &data_range_last);
      is_writeable = hyscan_db_channel_is_writable (db, channel_id);

      /* 2. Устанавливаем сдвиг кэша и начальный индекс. */
      if (status && *shift == G_MAXUINT32)
        {
          *shift = data_range_first;
          *db_index = data_range_first;
          *assembler_index = 0;
          *processing_index = 0;
          *preprocessing_index = 0;
          *thresholder_prev_index = 0;
          *thresholder_next_index = 0;
          thresholder_status = TRUE;
        }

      /* 3. Анализируем польз. параметры. */
      params_temp = g_array_new (FALSE, TRUE, sizeof(HyScanLocationUserParameters));
      for (i = 0; i < params->len; i++)
        {
          param_info = g_array_index (params, HyScanLocationUserParameters, i);
          if (param_info.type == HYSCAN_LOCATION_EDIT_LATLONG || param_info.type == HYSCAN_LOCATION_BULK_REMOVE)
            g_array_append_val (params_temp, param_info);
        }

      /* 4. Собираем данные, кладем в локальный кэш. */
      if (status)
        {
          /* Увеличиваем размер кэша блоками по 512 элементов. */
          while (cache->len < (data_range_last - *shift))
            cache = g_array_set_size (cache, cache->len + 512);

          /* Сборка данных. */
          while (*db_index <= data_range_last)
            {
              /* Разбираем соответствующую строку данных. */
              latlong = hyscan_location_assembler_latlong (db, source_list, params_temp, source, *db_index);

              /* Если строка валидна, кладем её в кэш. */
              if (latlong.validity == HYSCAN_LOCATION_ASSEMBLED ||
                  latlong.validity == HYSCAN_LOCATION_USER_VALID)
                  {
                    latlong_p = &g_array_index (cache, HyScanLocationInternalData, *assembler_index);
                    *latlong_p = latlong;
                    (*assembler_index)++;
                  }

              (*db_index)++;
            }
        }

      /* 5. Предобработка данных. Включает в себя сглаживание безье и временные сдвижки. */
      while (*preprocessing_index < *assembler_index - 1)
        {
          hyscan_location_timeshift (db, source_list, source, cache, datetime_source, datetime_cache, *preprocessing_index);

          /* На первых двух точках Безье не строится, только временной сдвиг. */
          if (*preprocessing_index >= 2)
            {
              hyscan_location_4_point_2d_bezier  (cache,
                                                  *preprocessing_index-2,
                                                  *preprocessing_index-1,
                                                  *preprocessing_index+0,
                                                  *preprocessing_index+1,
                                                  quality);
            }
          (*preprocessing_index)++;
        }
      /* Если дошли до последней точки, то просто применяем временной сдвиг. */
      if (*preprocessing_index == (*assembler_index - 1) && !is_writeable)
        {
          hyscan_location_timeshift (db, source_list, source, cache, datetime_source, datetime_cache, *preprocessing_index);
          (*preprocessing_index)++;
        }

      /* 6. Второй этап обработки - нарезка трэка на прямолинейные участки. */
      while (*processing_index < *preprocessing_index)
        {
          thresholder_status =  hyscan_location_thresholder2 (cache,
                                                              thresholder_prev_index,
                                                              *processing_index,
                                                              thresholder_next_index,
                                                              *preprocessing_index - 1,
                                                              is_writeable,
                                                              quality);
          if (!thresholder_status)
            break;
          (*processing_index)++;

        }
    }
  if (params_temp != NULL)
    g_array_free (params_temp, TRUE);
}
/* Функция слежения за высотой. */
void
hyscan_location_overseer_altitude (HyScanDB                    *db,
                                   GArray                      *source_list,
                                   HyScanLocationLocalCaches   *caches,
                                   HyScanLocationSourceIndices *indices,
                                   gdouble                      quality)
{
  gboolean status;

  gint32 source = indices->altitude_source;
  gint32 datetime_source = indices->datetime_source;
  GArray *cache = caches->altitude_cache;
  GArray *datetime_cache = caches->datetime_cache;

  HyScanLocationSourcesList *source_info = &g_array_index (source_list, HyScanLocationSourcesList, source);

  guint32 data_range_first = 0;
  guint32 data_range_last = 0;

  HyScanLocationInternalData  altitude, *altitude_p;

  gint32 channel_id = source_info->channel_id;

  guint32 *shift = &(source_info->shift);
  guint32 *db_index = &(source_info->db_index);
  guint32 *assembler_index = &(source_info->assembler_index);
  guint32 *processing_index = &(source_info->processing_index);

  if (source != -1)
    {
      /* 1. Смотрим, сколько у нас есть данных в канале. */
      status = hyscan_db_channel_get_data_range (db, channel_id, &data_range_first, &data_range_last);

      /* 2. Устанавливаем сдвиг кэша и начальный индекс. */
      if (status && *shift == G_MAXUINT32)
        {
          *shift = data_range_first;
          *db_index = data_range_first;
          *assembler_index = 0;
          *processing_index = 0;
        }

      /* 3. Собираем данные, кладем в локальный кэш. */
      if (status)
        {
          /* Увеличиваем размер кэша блоками по 512 элементов. */
          while (cache->len < data_range_last - *shift)
            cache = g_array_set_size (cache, cache->len + 512);

          /* Сборка данных. */
          while (*db_index <= data_range_last)
            {
              /* Разбираем соответствующую строку данных. */
              altitude = hyscan_location_assembler_altitude (db, source_list, source, *db_index);

              /* Если строка валидна, кладем её в кэш. */
              if (altitude.validity == HYSCAN_LOCATION_ASSEMBLED)
                {
                  altitude_p = &g_array_index (cache, HyScanLocationInternalData, *assembler_index);
                  *altitude_p = altitude;
                  (*assembler_index)++;
                }

              (*db_index)++;
            }
        }

      /* 4. Обрабатываем данные из локального кэша и кладем обратно. */
      while (*processing_index < *assembler_index)
        {
          /* Временной сдвиг. */
          hyscan_location_timeshift (db, source_list, source, cache, datetime_source, datetime_cache, *processing_index);
          //TODO: может, здесь сглаживать данные?
          (*processing_index)++;
        }
    }
}

/* Функция слежения за курсом. */
void
hyscan_location_overseer_track (HyScanDB                    *db,
                                GArray                      *source_list,
                                GArray                      *params,
                                HyScanLocationLocalCaches   *caches,
                                HyScanLocationSourceIndices *indices,
                                gdouble                      quality)
{
  gboolean status;

  gint32 source = indices->track_source;
  gint32 datetime_source = indices->datetime_source;
  GArray *cache = caches->track_cache;
  GArray *datetime_cache = caches->datetime_cache;

  HyScanLocationInternalData  track, *track_p;

  guint32 data_range_first = 0;
  guint32 data_range_last = 0;
  gint i;

  HyScanLocationSourcesList *source_info = &g_array_index (source_list, HyScanLocationSourcesList, source);

  HyScanLocationUserParameters param_info;
  GArray *params_temp = NULL;

  gint32 channel_id = source_info->channel_id;
  guint32 *shift = &(source_info->shift);
  guint32 *db_index = &(source_info->db_index);
  guint32 *assembler_index = &(source_info->assembler_index);
  guint32 *processing_index = &(source_info->processing_index);
  guint32 *preprocessing_index = &(source_info->preprocessing_index);
  guint32 *thresholder_prev_index = &(source_info->thresholder_prev_index);
  guint32 *thresholder_next_index = &(source_info->thresholder_next_index);

  gboolean thresholder_status = TRUE;
  gboolean is_writeable = FALSE;

  if (source != -1)
    {
      /* 1. Смотрим, сколько у нас есть данных в канале. */
      status = hyscan_db_channel_get_data_range (db, channel_id, &data_range_first, &data_range_last);
      is_writeable = hyscan_db_channel_is_writable (db, channel_id);

      /* 2. Устанавливаем сдвиг кэша и начальный индекс. */
      if (status && *shift == G_MAXUINT32)
        {
          *shift = data_range_first;
          *db_index = data_range_first;
          *assembler_index = 0;
          *processing_index = 0;
          *thresholder_prev_index = 0;
          *thresholder_next_index = 0;
          thresholder_status = TRUE;
        }

      /* 3. Анализируем польз. параметры. */
      params_temp = g_array_new (FALSE, TRUE, sizeof(HyScanLocationUserParameters));
      for (i = 0; i < params->len; i++)
        {
          param_info = g_array_index (params, HyScanLocationUserParameters, i);
          if (param_info.type == HYSCAN_LOCATION_EDIT_LATLONG || param_info.type == HYSCAN_LOCATION_BULK_REMOVE)
            g_array_append_val (params_temp, param_info);
        }

      /* 3. Собираем данные, кладем в локальный кэш. */
      if (status)
        {
          /* Увеличиваем размер кэша блоками по 512 элементов. */
          while (cache->len < data_range_last - *shift)
            cache = g_array_set_size (cache, cache->len + 512);

          /* Сборка данных. */
          while (*db_index <= data_range_last)
            {
              track = hyscan_location_assembler_track (db, source_list, params_temp, source, *db_index);
              if (track.validity == HYSCAN_LOCATION_ASSEMBLED)
                {
                  track_p = &g_array_index (cache, HyScanLocationInternalData, *assembler_index);
                  *track_p = track;
                  (*assembler_index)++;
                }

                (*db_index)++;
            }
        }
      //TODO: сравнить время обработки HYSCAN_LOCATION_SOURCE_NMEA и HYSCAN_LOCATION_SOURCE_NMEA_COMPUTED.
      //и если оно невелико, убрать к черту это разделение.
      if (source_info->source_type == HYSCAN_LOCATION_SOURCE_NMEA)
        {
          /* 4. Обрабатываем данные из локального кэша и кладем обратно. */
          while (*processing_index < *assembler_index)
            {
              /* Временной сдвиг. */
              hyscan_location_timeshift (db, source_list, source, cache, datetime_source, datetime_cache, *preprocessing_index);
              (*processing_index)++;
            }
        }

      if (source_info->source_type == HYSCAN_LOCATION_SOURCE_NMEA_COMPUTED)
        {
          /* 4. Предобработка данных. Включает в себя сглаживание безье и временные сдвижки. */
          while (*preprocessing_index < *assembler_index - 1)
            {

              /* На первых двух точках Безье не строится, только временной сдвиг. */
              hyscan_location_timeshift (db, source_list, source, cache, datetime_source, datetime_cache, *preprocessing_index);

              if (*preprocessing_index >= 2)
                {
                  hyscan_location_4_point_2d_bezier  (cache,
                                                      *preprocessing_index-2,
                                                      *preprocessing_index-1,
                                                      *preprocessing_index+0,
                                                      *preprocessing_index+1,
                                                      quality);
                }
              (*preprocessing_index)++;
            }
          /* Если дошли до последней точки, то просто применяем временной сдвиг. */
          if (*preprocessing_index == (*assembler_index - 1) && !is_writeable)
            {
              hyscan_location_timeshift (db, source_list, source, cache, datetime_source, datetime_cache, *preprocessing_index);
              (*preprocessing_index)++;
            }
          /* 5. Второй этап обработки - нарезка трэка на прямолинейные участки. */
          while (*processing_index < *preprocessing_index)
            {
              thresholder_status =  hyscan_location_thresholder2 (cache,
                                                            thresholder_prev_index,
                                                            *processing_index,
                                                            thresholder_next_index,
                                                            *preprocessing_index - 1,
                                                            is_writeable,
                                                            quality);
              if (!thresholder_status)
                break;
              (*processing_index)++;
            }
        }
    }

  if (params_temp != NULL)
    g_array_free (params_temp, TRUE);
}

/* Функция слежения за креном. */
void
hyscan_location_overseer_roll (HyScanDB                    *db,
                               GArray                      *source_list,
                               GArray                      *params,
                               HyScanLocationLocalCaches   *caches,
                               HyScanLocationSourceIndices *indices,
                               gdouble                      quality)
{
  gboolean status;

  gint32 source = indices->roll_source;
  gint32 datetime_source = indices->datetime_source;
  GArray *cache = caches->roll_cache;
  GArray *datetime_cache = caches->datetime_cache;

  HyScanLocationSourcesList *source_info = &g_array_index (source_list, HyScanLocationSourcesList, source);


  guint32 data_range_first = 0;
  guint32 data_range_last = 0;
  gint i;

  HyScanLocationInternalData *roll_p, roll;

  HyScanLocationUserParameters param_info;
  GArray *params_temp = NULL;

  gint32 channel_id = source_info->channel_id;
  guint32 *shift = &(source_info->shift);
  guint32 *db_index = &(source_info->db_index);
  guint32 *assembler_index = &(source_info->assembler_index);
  guint32 *processing_index = &(source_info->processing_index);

  if (source != -1)
    {
      /* 1. Смотрим, сколько у нас есть данных в канале. */
      status = hyscan_db_channel_get_data_range (db, channel_id, &data_range_first, &data_range_last);
      /* 2. Устанавливаем сдвиг кэша и начальный индекс. */
      if (status && *shift == G_MAXUINT32)
        {
          *shift = data_range_first;
          *db_index = data_range_first;
          *assembler_index = 0;
          *processing_index = 0;
        }

      /* 3. Анализируем польз. параметры. */
      params_temp = g_array_new (FALSE, TRUE, sizeof(HyScanLocationUserParameters));
      for (i = 0; i < params->len; i++)
        {
          if (source_info->source_type == HYSCAN_LOCATION_SOURCE_NMEA)
            {
              param_info = g_array_index (params, HyScanLocationUserParameters, i);
            }
          if (source_info->source_type == HYSCAN_LOCATION_SOURCE_NMEA_COMPUTED)
            {
              param_info = g_array_index (params, HyScanLocationUserParameters, i);
              if (param_info.type == HYSCAN_LOCATION_EDIT_LATLONG || param_info.type == HYSCAN_LOCATION_BULK_REMOVE)
                g_array_append_val (params_temp, param_info);
            }
        }
      /* 3. Собираем данные, кладем в локальный кэш. */
      if (status)
        {
          /* Увеличиваем размер кэша блоками по 512 элементов. */
          while (cache->len < data_range_last - *shift)
            cache = g_array_set_size (cache, cache->len + 512);
          /* Сборка данных. */
          while (*db_index <= data_range_last)
            {
              roll = hyscan_location_assembler_roll (db, source_list, params_temp, source, *db_index);
              if (roll.validity == HYSCAN_LOCATION_ASSEMBLED)
                {
                  roll_p = &g_array_index (cache, HyScanLocationInternalData, *assembler_index);
                  *roll_p = roll;
                  (*assembler_index)++;
                }
              (*db_index)++;
            }
        }

      /* 4. Обрабатываем данные из локального кэша и кладем обратно. */
      while (*processing_index < *assembler_index)
        {
          /* Временной сдвиг. */
          hyscan_location_timeshift (db, source_list, source, cache, datetime_source, datetime_cache, *processing_index);
          (*processing_index)++;
        }
    }

  if (params_temp != NULL)
    g_array_free (params_temp, TRUE);
}

/* Функция слежения за дифферентом. */
void
hyscan_location_overseer_pitch (HyScanDB                    *db,
                                GArray                      *source_list,
                                GArray                      *params,
                                HyScanLocationLocalCaches   *caches,
                                HyScanLocationSourceIndices *indices,
                                gdouble                      quality)
{
  gboolean status;

  gint32 source = indices->pitch_source;
  gint32 datetime_source = indices->datetime_source;
  GArray *cache = caches->pitch_cache;
  GArray *datetime_cache = caches->datetime_cache;

  HyScanLocationSourcesList *source_info = &g_array_index (source_list, HyScanLocationSourcesList, source);

  guint32 data_range_first = 0;
  guint32 data_range_last = 0;
  gint i;

  HyScanLocationInternalData *pitch_p, pitch;

  HyScanLocationUserParameters param_info;
  GArray *params_temp = NULL;

  gint32 channel_id = source_info->channel_id;
  guint32 *shift = &(source_info->shift);
  guint32 *db_index = &(source_info->db_index);
  guint32 *assembler_index = &(source_info->assembler_index);
  guint32 *processing_index = &(source_info->processing_index);

  if (source != -1)
    {
      /* 1. Смотрим, сколько у нас есть данных в канале. */
      status = hyscan_db_channel_get_data_range (db, channel_id, &data_range_first, &data_range_last);

      /* 2. Устанавливаем сдвиг кэша и начальный индекс. */
      if (status && *shift == G_MAXUINT32)
        {
          *shift = data_range_first;
          *db_index = data_range_first;
          *assembler_index = 0;
          *processing_index = 0;
        }

      /* 3. Анализируем польз. параметры. */
      params_temp = g_array_new (FALSE, TRUE, sizeof(HyScanLocationUserParameters));
      for (i = 0; i < params->len; i++)
        {
          if (source_info->source_type == HYSCAN_LOCATION_SOURCE_NMEA)
            {
              param_info = g_array_index (params, HyScanLocationUserParameters, i);
            }
          if (source_info->source_type == HYSCAN_LOCATION_SOURCE_NMEA_COMPUTED)
            {
              param_info = g_array_index (params, HyScanLocationUserParameters, i);
              if (param_info.type == HYSCAN_LOCATION_EDIT_LATLONG || param_info.type == HYSCAN_LOCATION_BULK_REMOVE)
                g_array_append_val (params_temp, param_info);
            }
        }
      /* 3. Собираем данные, кладем в локальный кэш. */
      if (status)
        {
          /* Увеличиваем размер кэша блоками по 512 элементов. */
          while (cache->len < data_range_last - *shift)
            cache = g_array_set_size (cache, cache->len + 512);

          /* Сборка данных. */
          while (*db_index <= data_range_last)
            {
              pitch = hyscan_location_assembler_pitch (db, source_list, params_temp, source, *assembler_index);
              if (pitch.validity == HYSCAN_LOCATION_ASSEMBLED)
                {
                  pitch_p = &g_array_index (cache, HyScanLocationInternalData, *assembler_index);
                  *pitch_p = pitch;
                  (*assembler_index)++;
                }
              (*db_index)++;
            }
        }

      /* 4. Обрабатываем данные из локального кэша и кладем обратно. */
      while (*processing_index < *assembler_index)
        {
          /* Временной сдвиг. */
          hyscan_location_timeshift (db, source_list, source, cache, datetime_source, datetime_cache, *processing_index);
          (*processing_index)++;
        }
    }

  if (params_temp != NULL)
    g_array_free (params_temp, TRUE);
}

/* Функция слежения за скоростью. */
void
hyscan_location_overseer_speed (HyScanDB                    *db,
                                GArray                      *source_list,
                                GArray                      *params,
                                HyScanLocationLocalCaches   *caches,
                                HyScanLocationSourceIndices *indices,
                                gdouble                      quality)
{
  gboolean status;

  gint32 source = indices->speed_source;
  gint32 datetime_source = indices->datetime_source;
  GArray *cache = caches->speed_cache;
  GArray *datetime_cache = caches->datetime_cache;

  guint32 data_range_first = 0;
  guint32 data_range_last = 0;
  gint i;

  HyScanLocationInternalData *speed_p, speed;

  HyScanLocationSourcesList *source_info = &g_array_index (source_list, HyScanLocationSourcesList, source);

  HyScanLocationUserParameters param_info;
  GArray *params_temp = NULL;

  gint32 channel_id = source_info->channel_id;
  guint32 *shift = &(source_info->shift);
  guint32 *db_index = &(source_info->db_index);
  guint32 *assembler_index = &(source_info->assembler_index);
  guint32 *processing_index = &(source_info->processing_index);
  guint32 *preprocessing_index = &(source_info->preprocessing_index);
  guint32 *thresholder_prev_index = &(source_info->thresholder_prev_index);
  guint32 *thresholder_next_index = &(source_info->thresholder_next_index);

  gboolean thresholder_status = TRUE;
  gboolean is_writeable = FALSE;

  if (source != -1)
    {
      /* 1. Смотрим, сколько у нас есть данных в канале. */
      status = hyscan_db_channel_get_data_range (db, channel_id, &data_range_first, &data_range_last);
      is_writeable = hyscan_db_channel_is_writable (db, channel_id);

      /* 2. Устанавливаем сдвиг кэша и начальный индекс. */
      if (status && *shift == G_MAXUINT32)
        {
          *shift = data_range_first;
          *db_index = data_range_first;
          *assembler_index = 0;
          *processing_index = 0;
          *thresholder_prev_index = 0;
          *thresholder_next_index = 0;
          thresholder_status = TRUE;
        }

      /* 3. Анализируем польз. параметры. */
      params_temp = g_array_new (FALSE, TRUE, sizeof(HyScanLocationUserParameters));
      for (i = 0; i < params->len; i++)
        {
          if (source_info->source_type == HYSCAN_LOCATION_SOURCE_NMEA)
            {
              param_info = g_array_index (params, HyScanLocationUserParameters, i);
            }
          if (source_info->source_type == HYSCAN_LOCATION_SOURCE_NMEA_COMPUTED)
            {
              param_info = g_array_index (params, HyScanLocationUserParameters, i);
              if (param_info.type == HYSCAN_LOCATION_EDIT_LATLONG || param_info.type == HYSCAN_LOCATION_BULK_REMOVE)
                g_array_append_val (params_temp, param_info);
            }
        }
      /* 3. Собираем данные, кладем в локальный кэш. */
      if (status)
        {
          /* Увеличиваем размер кэша блоками по 512 элементов. */
          while (cache->len < data_range_last - *shift)
            cache = g_array_set_size (cache, cache->len + 512);

          /* Сборка данных. */
          while (*db_index <= data_range_last)
            {
              speed = hyscan_location_assembler_speed (db, source_list, params_temp, source, *assembler_index);
              if (speed.validity == HYSCAN_LOCATION_ASSEMBLED)
                {
                  speed_p = &g_array_index (cache, HyScanLocationInternalData, *assembler_index);
                  *speed_p = speed;
                  (*assembler_index)++;
                }
              (*db_index)++;
            }
        }

      if (source_info->source_type == HYSCAN_LOCATION_SOURCE_NMEA)
        {
          /* 4. Обрабатываем данные из локального кэша и кладем обратно. */
          while (*processing_index < *assembler_index)
            {
              /* Временной сдвиг. */
              hyscan_location_timeshift (db, source_list, source, cache, datetime_source, datetime_cache, *preprocessing_index);
              (*processing_index)++;
            }
        }

      if (source_info->source_type == HYSCAN_LOCATION_SOURCE_NMEA_COMPUTED)
        {
          /* 4. Предобработка данных. Включает в себя сглаживание безье и временные сдвижки. */
          while (*preprocessing_index < *assembler_index - 1)
            {
              /* На первых двух точках Безье не строится, только временной сдвиг. */
              hyscan_location_timeshift (db, source_list, source, cache, datetime_source, datetime_cache, *preprocessing_index);

              if (*preprocessing_index - *shift >= 2)
                {
                  hyscan_location_4_point_2d_bezier  (cache,
                                                      *preprocessing_index-2,
                                                      *preprocessing_index-1,
                                                      *preprocessing_index+0,
                                                      *preprocessing_index+1,
                                                      quality);
                }
              (*preprocessing_index)++;
            }
          /* Если дошли до последней точки, то просто применяем временной сдвиг. */
          if (*preprocessing_index == (*assembler_index - 1) && !is_writeable)
            {
              hyscan_location_timeshift (db, source_list, source, cache, datetime_source, datetime_cache, *preprocessing_index);
              (*preprocessing_index)++;
            }

          /* 5. Второй этап обработки - нарезка трэка на прямолинейные участки. */
          while (*processing_index < *preprocessing_index)
            {
              thresholder_status =  hyscan_location_thresholder2 (cache,
                                                            thresholder_prev_index,
                                                            *processing_index,
                                                            thresholder_next_index,
                                                            *preprocessing_index - 1,
                                                            is_writeable,
                                                            quality);
              if (!thresholder_status)
                break;
              (*processing_index)++;
            }
        }
    }

  if (params_temp != NULL)
    g_array_free (params_temp, TRUE);
}

/* Функция слежения за глубиной. */
void
hyscan_location_overseer_depth (HyScanDB                    *db,
                                GArray                      *source_list,
                                HyScanLocationLocalCaches   *caches,
                                HyScanLocationSourceIndices *indices,
                                GArray                      *soundspeed,
                                gdouble                      quality)
{
  gboolean status;

  gint32 source = indices->depth_source;
  GArray *cache = caches->depth_cache;

  HyScanLocationSourcesList *source_info = &g_array_index (source_list, HyScanLocationSourcesList, source);
  guint32 data_range_first = 0;
  guint32 data_range_last = 0;

  HyScanLocationInternalData *depth_p, depth;

  gint32 channel_id = source_info->channel_id;

  guint32 *shift = &(source_info->shift);
  guint32 *db_index = &(source_info->db_index);
  guint32 *assembler_index = &(source_info->assembler_index);
  guint32 *processing_index = &(source_info->processing_index);

  if (source != -1)
    {
      /* 1. Смотрим, сколько у нас есть данных в канале. */
      switch (source_info->source_type)
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
          status = hyscan_raw_data_get_range (source_info->dchannel, &data_range_first, &data_range_last);
          break;
        default:
          status = FALSE;
          break;
        }

      /* 2. Устанавливаем сдвиг кэша и начальный индекс. */
      if (status && *shift == G_MAXUINT32)
        {
          *shift = data_range_first;
          *db_index = data_range_first;
          *assembler_index = 0;
          *processing_index = 0;
        }

      /* 3. Собираем данные, кладем в локальный кэш. */
      if (status)
        {
          /* Увеличиваем размер кэша блоками по 512 элементов. */
          while (cache->len < data_range_last - *shift)
            cache = g_array_set_size (cache, cache->len + 512);

          /* Сборка данных. */
          while (*db_index <= data_range_last)
            {
              depth = hyscan_location_assembler_depth (db, source_list, source, soundspeed, *db_index);
              if (depth.validity == HYSCAN_LOCATION_ASSEMBLED)
                {
                  depth_p = &g_array_index (cache, HyScanLocationInternalData, *assembler_index);
                  *depth_p = depth;
                  (*assembler_index)++;
                }
              (*db_index)++;
            }
        }

      /* 4. Обрабатываем данные из локального кэша и кладем обратно. */
      while (*processing_index < *assembler_index)
        {
          (*processing_index)++;
        }
    }
}
