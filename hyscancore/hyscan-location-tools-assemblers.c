/*
 * Файл содержит следующие группы функций:
 *
 * Функции надзирателя.
 *
 */
#include <hyscan-location-tools.h>

/* Функция сборки даты и времени.*/
HyScanLocationInternalTime
hyscan_location_assembler_datetime (HyScanDB *db,
                                    GArray   *source_list,
                                    gint32    source,
                                    gint64    index)
{
  gint32 buffer_size;
  HyScanLocationInternalTime datetime;
  gint64 db_time;
  gchar *buffer = NULL;

  HyScanLocationSourcesList *source_info = &g_array_index (source_list, HyScanLocationSourcesList, source);

  hyscan_db_channel_get_data (db, source_info->channel_id, index, NULL, &buffer_size, NULL);
  buffer = g_malloc0 (buffer_size * sizeof(gchar));
  hyscan_db_channel_get_data (db, source_info->channel_id, index, buffer, &buffer_size, &db_time);

  datetime = hyscan_location_nmea_datetime_get (buffer);
  datetime.db_time = db_time;
  if (datetime.validity == HYSCAN_LOCATION_PARSED)
    datetime.validity = HYSCAN_LOCATION_ASSEMBLED;

  g_free (buffer);

  return datetime;
}

/* Функция сборки координат.*/
HyScanLocationInternalData
hyscan_location_assembler_latlong (HyScanDB *db,
                                   GArray   *source_list,
                                   GArray   *params,
                                   gint32    source,
                                   gint64    index)
{
  gint i;
  HyScanLocationUserParameters *param_info;

  HyScanLocationInternalData latlong;

  gint32 buffer_size;
  gint64 db_time;
  gchar *buffer = NULL;

  HyScanLocationSourcesList *source_info = &g_array_index (source_list, HyScanLocationSourcesList, source);

  hyscan_db_channel_get_data (db, source_info->channel_id, index, NULL, &buffer_size, NULL);
  buffer = g_malloc0 (buffer_size * sizeof(gchar));
  hyscan_db_channel_get_data (db, source_info->channel_id, index, buffer, &buffer_size, &db_time);

  latlong = hyscan_location_nmea_latlong_get (buffer);
  latlong.db_time = db_time;

  /* Ищем подходящую запись в параметрах обработки. */
  for (i = 0; i < params->len; i++)
    {
      param_info = &g_array_index (params, HyScanLocationUserParameters, i);
      if (db_time >= param_info->ltime && db_time <= param_info->rtime)
        {
          switch (param_info->type)
            {
            case HYSCAN_LOCATION_EDIT_LATLONG:
              latlong.int_latitude = param_info->value1;
              latlong.int_longitude = param_info->value2;
              latlong.validity = HYSCAN_LOCATION_USER_VALID;
              break;

            case HYSCAN_LOCATION_BULK_REMOVE :
              //if (db_time == param_info->ltime || db_time == param_info->rtime)
              //  latlong.validity = HYSCAN_LOCATION_USER_VALID;
              //else
                latlong.validity = HYSCAN_LOCATION_INVALID;
              break;

            default:
              break;
            }
        }
    }

  if (latlong.validity == HYSCAN_LOCATION_PARSED)
    latlong.validity = HYSCAN_LOCATION_ASSEMBLED;

  g_free (buffer);

  return latlong;
}

/* Функция сборки высоты.*/
HyScanLocationInternalData
hyscan_location_assembler_altitude (HyScanDB *db,
                                    GArray   *source_list,
                                    gint32    source,
                                    gint64    index)
{
  gint32 buffer_size;
  HyScanLocationInternalData altitude;
  gint64 db_time;
  gchar *buffer = NULL;

  HyScanLocationSourcesList *source_info = &g_array_index (source_list, HyScanLocationSourcesList, source);

  hyscan_db_channel_get_data (db, source_info->channel_id, index, NULL, &buffer_size, NULL);
  buffer = g_malloc0 (buffer_size * sizeof(gchar));
  hyscan_db_channel_get_data (db, source_info->channel_id, index, buffer, &buffer_size, &db_time);

  altitude = hyscan_location_nmea_altitude_get (buffer);
  altitude.db_time = db_time;
  if (altitude.validity == HYSCAN_LOCATION_PARSED)
    altitude.validity = HYSCAN_LOCATION_ASSEMBLED;

  g_free (buffer);

  return altitude;
}

/* Функция сборки курса.*/
HyScanLocationInternalData
hyscan_location_assembler_track (HyScanDB *db,
                                 GArray   *source_list,
                                 GArray   *params,
                                 gint32    source,
                                 gint64    index)
{
  gint32 buffer_size;
  gint32 i;
  HyScanLocationInternalData track;

  gint64 db_time;
  gchar *buffer = NULL;

  HyScanLocationUserParameters *param_info;
  HyScanLocationSourcesList *source_info = &g_array_index (source_list, HyScanLocationSourcesList, source);

  hyscan_db_channel_get_data (db, source_info->channel_id, index, NULL, &buffer_size, NULL);
  buffer = g_malloc0 (buffer_size * sizeof(gchar));
  hyscan_db_channel_get_data (db, source_info->channel_id, index, buffer, &buffer_size, &db_time);

  /* Достаем либо значение. В track будут храниться как координаты, так и курс (из строки) */
  track = hyscan_location_nmea_track_get (buffer);
  track.db_time = db_time;

  /* Ищем подходящую запись в параметрах обработки. */
  for (i = 0; i < params->len; i++)
    {
      param_info = &g_array_index (params, HyScanLocationUserParameters, i);
      if (db_time >= param_info->ltime && db_time <= param_info->rtime)
        {
          switch (param_info->type)
            {
            case HYSCAN_LOCATION_EDIT_LATLONG:
              track.int_latitude = param_info->value1;
              track.int_longitude = param_info->value2;
              track.validity = HYSCAN_LOCATION_USER_VALID;
              break;

            case HYSCAN_LOCATION_BULK_REMOVE :
              track.validity = HYSCAN_LOCATION_INVALID;
              break;
            default:
              break;
            }
        }
    }

  if (track.validity == HYSCAN_LOCATION_PARSED)
    track.validity = HYSCAN_LOCATION_ASSEMBLED;

  g_free (buffer);

  return track;
}

/* Функция сборки крена.*/
HyScanLocationInternalData
hyscan_location_assembler_roll (HyScanDB *db,
                                GArray   *source_list,
                                GArray   *params,
                                gint32    source,
                                gint64    index)
{
  gint32 buffer_size;
  HyScanLocationInternalData roll;
  gint64 db_time;
  gchar *buffer = NULL;

  HyScanLocationSourcesList *source_info = &g_array_index (source_list, HyScanLocationSourcesList, source);

  hyscan_db_channel_get_data (db, source_info->channel_id, index, NULL, &buffer_size, NULL);
  buffer = g_malloc0 (buffer_size * sizeof(gchar));
  hyscan_db_channel_get_data (db, source_info->channel_id, index, buffer, &buffer_size, &db_time);

  roll = hyscan_location_nmea_roll_get (buffer);
  roll.db_time = db_time;

  if (roll.validity == HYSCAN_LOCATION_PARSED)
    roll.validity = HYSCAN_LOCATION_ASSEMBLED;

  g_free (buffer);

  return roll;
}

/* Функция сборки дифферента.*/
HyScanLocationInternalData
hyscan_location_assembler_pitch (HyScanDB *db,
                                 GArray   *source_list,
                                 GArray   *params,
                                 gint32    source,
                                 gint64    index)
{
  gint32 buffer_size;
  HyScanLocationInternalData pitch;
  gint64 db_time;
  gchar *buffer = NULL;

  HyScanLocationSourcesList *source_info = &g_array_index (source_list, HyScanLocationSourcesList, source);

  hyscan_db_channel_get_data (db, source_info->channel_id, index, NULL, &buffer_size, NULL);
  buffer = g_malloc0 (buffer_size * sizeof(gchar));
  hyscan_db_channel_get_data (db, source_info->channel_id, index, buffer, &buffer_size, &db_time);

  pitch = hyscan_location_nmea_pitch_get (buffer);
  pitch.db_time = db_time;

  if (pitch.validity == HYSCAN_LOCATION_PARSED)
    pitch.validity = HYSCAN_LOCATION_ASSEMBLED;

  g_free (buffer);

  return pitch;
}

/* Функция сборки скорости.*/
HyScanLocationInternalData
hyscan_location_assembler_speed (HyScanDB *db,
                                 GArray   *source_list,
                                 GArray   *params,
                                 gint32    source,
                                 gint64    index)
{
  HyScanLocationUserParameters *param_info;

  gint32 buffer_size;
  HyScanLocationInternalData speed;
  gint64 db_time;
  gchar *buffer = NULL;

  gint i;

  HyScanLocationSourcesList *source_info = &g_array_index (source_list, HyScanLocationSourcesList, source);

  hyscan_db_channel_get_data (db, source_info->channel_id, index, NULL, &buffer_size, NULL);
  buffer = g_malloc0 (buffer_size * sizeof(gchar));
  hyscan_db_channel_get_data (db, source_info->channel_id, index, buffer, &buffer_size, &db_time);

  speed = hyscan_location_nmea_speed_get (buffer);
  speed.db_time = db_time;

  /* Ищем подходящую запись в параметрах обработки. */
  for (i = 0; i < params->len; i++)
    {
      param_info = &g_array_index (params, HyScanLocationUserParameters, i);
      if (db_time >= param_info->ltime && db_time <= param_info->rtime)
        {
          //apply changes
          switch (param_info->type)
            {
            case HYSCAN_LOCATION_EDIT_LATLONG:
              speed.int_latitude = param_info->value1;
              speed.int_longitude = param_info->value2;
              speed.validity = HYSCAN_LOCATION_USER_VALID;
              break;

            case HYSCAN_LOCATION_BULK_REMOVE :
              speed.validity = HYSCAN_LOCATION_INVALID;
              break;
            default:
              break;
            }
        }
    }

  if (speed.validity == HYSCAN_LOCATION_PARSED)
    speed.validity = HYSCAN_LOCATION_ASSEMBLED;

  g_free (buffer);

  return speed;
}

/* Функция сборки глубины.*/
HyScanLocationInternalData
hyscan_location_assembler_depth (HyScanDB *db,
                                 GArray   *source_list,
                                 gint32    source,
                                 GArray   *soundspeed,
                                 gint64    index)
{
  HyScanLocationInternalData depth;
  gint64 db_time;
  gint32 buffer_size;
  gchar *char_buffer = NULL;
  gfloat *float_buffer = NULL;

  HyScanLocationSourcesList *source_info = &g_array_index (source_list, HyScanLocationSourcesList, source);

  switch (source_info->source_type)
    {
    case HYSCAN_LOCATION_SOURCE_NMEA:
      hyscan_db_channel_get_data (db, source_info->channel_id, index, NULL, &buffer_size, NULL);
      char_buffer = g_malloc0 (buffer_size * sizeof(gchar));
      hyscan_db_channel_get_data (db, source_info->channel_id, index, char_buffer, &buffer_size, &db_time);
      depth = hyscan_location_nmea_depth_get (char_buffer);
      break;

    case HYSCAN_LOCATION_SOURCE_ECHOSOUNDER:
      buffer_size = hyscan_data_channel_get_values_count (source_info->dchannel, index);
      float_buffer = g_malloc0 (buffer_size * sizeof(gfloat));
      hyscan_data_channel_get_amplitude_values (source_info->dchannel, index, float_buffer, &buffer_size, &db_time);
      depth = hyscan_location_echosounder_depth_get (float_buffer, buffer_size, source_info->data_rate, soundspeed);
      break;

    case HYSCAN_LOCATION_SOURCE_SONAR_PORT:
    case HYSCAN_LOCATION_SOURCE_SONAR_STARBOARD:
    case HYSCAN_LOCATION_SOURCE_SONAR_HIRES_PORT:
    case HYSCAN_LOCATION_SOURCE_SONAR_HIRES_STARBOARD:
      buffer_size = hyscan_data_channel_get_values_count (source_info->dchannel, index);
      float_buffer = g_malloc0 (buffer_size * sizeof(gfloat));
      hyscan_data_channel_get_amplitude_values (source_info->dchannel, index, float_buffer, &buffer_size, &db_time);
      depth = hyscan_location_sonar_depth_get (float_buffer, buffer_size, source_info->data_rate, soundspeed);
      break;
    default:
      depth.validity = HYSCAN_LOCATION_INVALID;
    }
  if (depth.validity == HYSCAN_LOCATION_PARSED)
    depth.validity = HYSCAN_LOCATION_ASSEMBLED;

  depth.db_time = db_time;
  depth.data_time = db_time;

  g_free (char_buffer);
  g_free (float_buffer);

  return depth;
}
