/*
 * Файл содержит следующие группы функций:
 *
 * Функции для работы с сырыми данными (NMEA-строки).
 *
 */

#include <hyscan-location-tools.h>

/* Функция проверяет тип и валидность строки. */
HyScanSonarDataType
hyscan_location_nmea_sentence_check (gchar *input)
{
  gchar *ch = input;
  gchar *str;
  gint calculated_checksum = 0,
       sentence_checksum = 0;
  gsize  len = strlen(ch);

  /* Контрольная сумма считается как XOR всех элементов между $ и *. */
  ch++; /* Пропускаем $. */
  len--;

  /* Подсчитываем контрольную сумму. */
  while (*ch != '*' && len > 0)
    {
      calculated_checksum = calculated_checksum ^ *ch;
      ch++;
      len--;
    }

  /* Если количество оставшихся меньше длины контрольной суммы, строка не валидна. */
  if (len < sizeof("xx"))
    return HYSCAN_SONAR_DATA_INVALID;

  ch++; /* Пропускаем "*", этот символ не учитывается при подсчете контрольной суммы. */

  /* Старший байт контрольной суммы. */
  /* 48 - это ascii код символа '0'
   * 97 - 'a', но т.к hex(a)==dec(10), берем 87
   * 65 - 'A', аналогично берем 55
   */
  if (*ch >='0' && *ch<='9')
    sentence_checksum = (*ch - 48) * 16;
  if (*ch >='a' && *ch<='f')
    sentence_checksum = (*ch - 87) * 16;
  if (*ch >='A' && *ch<='F')
    sentence_checksum = (*ch - 55) * 16;

  ch++;
  /* Младший байт контрольной суммы. */
  if (*ch >='0' && *ch<='9')
    sentence_checksum += (*ch - 48);
  if (*ch >='a' && *ch<='f')
    sentence_checksum += (*ch - 87);
  if (*ch >='A' && *ch<='F')
    sentence_checksum += (*ch - 55);

  if (sentence_checksum != calculated_checksum)
    return HYSCAN_SONAR_DATA_INVALID;

  ch = input + 3; /* Пропускаем первые 3 символа "$GPxxx". */

  str = g_strndup (ch, 3);

  if (g_strcmp0(str, "GGA") == 0)
    return HYSCAN_SONAR_DATA_NMEA_GGA;
  if (g_strcmp0(str, "RMC") == 0)
    return HYSCAN_SONAR_DATA_NMEA_RMC;
  if (g_strcmp0(str, "DPT") == 0)
    return HYSCAN_SONAR_DATA_NMEA_DPT;

  /* Если тип не совпадает с вышеуказанными, возвращаем ошибку. */
  return HYSCAN_SONAR_DATA_INVALID;
}

/* Функция извлекает широту и долготу из NMEA-строки. */
HyScanLocationInternalData
hyscan_location_nmea_latlong_get (gchar *input)
{
  HyScanLocationInternalData output = {0};
  gdouble field_val = 0,
          deg = 0,
          min = 0;
  gchar *ch = input;

  HyScanSonarDataType sentence_type;

  if (input == NULL)
    return output;

  sentence_type = hyscan_location_nmea_sentence_check(input);

  switch (sentence_type)
    {
    case HYSCAN_SONAR_DATA_NMEA_RMC:
      while (*(ch++) != ',');

    case HYSCAN_SONAR_DATA_NMEA_GGA:
      while (*(ch++) != ',');
      while (*(ch++) != ',');
      field_val = g_ascii_strtod (ch, NULL);
      deg = floor(field_val / 100.0);
      min = field_val - floor(field_val / 100.0)*100;
      min *= 100.0 / 60.0;
      output.int_latitude = deg + min / 100.0;
      while (*(ch++) != ',');
      if (*ch == 'S')
        output.int_latitude *= -1;

      while (*(ch++) != ',');
      field_val = g_ascii_strtod (ch, NULL);
      deg = floor(field_val / 100.0);
      min = field_val - floor(field_val / 100.0)*100;
      min *= 100.0 / 60.0;
      output.int_longitude = deg + min / 100.0;
      while (*(ch++) != ',');
      if (*ch == 'W')
        output.int_longitude *= -1;
      output.data_time = hyscan_location_nmea_time_get(input, sentence_type);
      output.validity = HYSCAN_LOCATION_PARSED;
      break;
    default:
      break;
    }

  return output;
}

/* Функция извлекает высоту из NMEA-строки. */
HyScanLocationInternalData
hyscan_location_nmea_altitude_get (gchar *input)
{
  HyScanLocationInternalData output = {0};
  int i = 0;
  gchar *ch = input;
  HyScanSonarDataType sentence_type = hyscan_location_nmea_sentence_check(input);

  if (sentence_type == HYSCAN_SONAR_DATA_NMEA_GGA)
    {
      /* Высота - это значение в 9 поле GGA. */
      for (i = 0; i < 9; i++)
        while (*(ch++) != ',');
      output.int_value = g_ascii_strtod (ch, NULL);
      output.data_time = hyscan_location_nmea_time_get(input, sentence_type);
      output.validity = HYSCAN_LOCATION_PARSED;
    }
  return output;
}

/* Функция извлекает курс из NMEA-строки. */
HyScanLocationInternalData
hyscan_location_nmea_track_get (gchar *input)
{
  HyScanLocationInternalData output = {0};
  int i = 0;
  gdouble field_val = 0,
          deg = 0,
          min = 0;
  gchar *ch = input;
  HyScanSonarDataType sentence_type = hyscan_location_nmea_sentence_check(input);

  if (sentence_type == HYSCAN_SONAR_DATA_NMEA_RMC)
    {
      /* Достаем координаты. */
      for (i = 0; i < 3; i++)
        while (*(ch++) != ',');

      field_val = g_ascii_strtod (ch, NULL);
      deg = floor(field_val/ 100.0);
      min = field_val - floor(field_val / 100.0)*100;
      min *= 100.0 / 60.0;
      output.int_latitude = deg + min / 100.0;
      while (*(ch++) != ',');
      if (*ch == 'S')
        output.int_latitude *= -1;

      while (*(ch++) != ',');
      field_val = g_ascii_strtod (ch, NULL);
      deg = floor(field_val / 100.0);
      min = field_val - floor(field_val / 100.0)*100;
      min *= 100.0/ 60.0;
      output.int_longitude = deg + min / 100.0;
      while (*(ch++) != ',');
      if (*ch == 'W')
        output.int_longitude *= -1;
      /* Курс - это значение в 8 поле RMC. */
      for (i = 6; i < 8; i++)
        while (*(ch++) != ',');
      output.int_value = g_ascii_strtod (ch, NULL);
      output.data_time = hyscan_location_nmea_time_get(input, sentence_type);
      output.validity = HYSCAN_LOCATION_PARSED;
    }
  return output;
}

/* Функция извлекает крен из NMEA-строки. */
HyScanLocationInternalData
hyscan_location_nmea_roll_get (gchar *input)
{
  HyScanLocationInternalData output = {0};
  /* HyScanSonarDataType sentence_type = hyscan_location_nmea_sentence_check(input); */

  return output;
}

/* Функция извлекает дифферент из NMEA-строки. */
HyScanLocationInternalData
hyscan_location_nmea_pitch_get (gchar *input)
{
  HyScanLocationInternalData output = {0};
  /* HyScanSonarDataType sentence_type = hyscan_location_nmea_sentence_check(input); */

  return output;
}

/* Функция извлекает скорость из NMEA-строки. */
HyScanLocationInternalData
hyscan_location_nmea_speed_get (gchar *input)
{
  HyScanLocationInternalData output = {0};
  int i = 0;
  gdouble field_val = 0,
          deg = 0,
          min = 0;
  gchar *ch = input;
  HyScanSonarDataType sentence_type = hyscan_location_nmea_sentence_check(input);

  if (sentence_type == HYSCAN_SONAR_DATA_NMEA_RMC)
    {
      /* Достаем координаты. */
      for (i = 0; i < 3; i++)
        while (*(ch++) != ',');

      field_val = g_ascii_strtod (ch, NULL);
      deg = floor(field_val/ 100.0);
      min = field_val - floor(field_val / 100.0)*100;
      min *= 100.0 / 60.0;
      output.int_latitude = deg + min / 100.0;
      while (*(ch++) != ',');
      if (*ch == 'S')
        output.int_latitude *= -1;

      while (*(ch++) != ',');
      field_val = g_ascii_strtod (ch, NULL);
      deg = floor(field_val / 100.0);
      min = field_val - floor(field_val / 100.0)*100;
      min *= 100.0/ 60.0;
      output.int_longitude = deg + min / 100.0;
      while (*(ch++) != ',');
      if (*ch == 'W')
        output.int_longitude *= -1;
        /* Скорость - это значение в 7 поле RMC. */
      for (i = 6; i < 8; i++)
        while (*(ch++) != ',');
      output.int_value = g_ascii_strtod (ch, NULL);
      output.data_time = hyscan_location_nmea_time_get(input, sentence_type);
      output.validity = HYSCAN_LOCATION_PARSED;
    }
  return output;
}

/* Функция извлекает глубину из NMEA-строки. */
HyScanLocationInternalData
hyscan_location_nmea_depth_get (gchar *input)
{
  HyScanLocationInternalData output = {0};
  gchar *ch = input;
  HyScanSonarDataType sentence_type = hyscan_location_nmea_sentence_check(input);

  if (sentence_type == HYSCAN_SONAR_DATA_NMEA_RMC)
    {
      /* Глубина - это значение в 1 поле RMC. */
      while (*(ch++) != ',');
      output.int_value = g_ascii_strtod (ch, NULL);
      output.validity = HYSCAN_LOCATION_PARSED;
    }
  return output;
}

/* Функция извлекает дату и время из NMEA-строки. */
HyScanLocationInternalTime
hyscan_location_nmea_datetime_get (gchar *input)
{
  HyScanLocationInternalTime output = {0};
  int i = 0;
  gchar *ch = input;
  GDateTime *dt;
  gint year = 0;
  gint month = 0;
  gint day = 0;
  gint hour = 0;
  gint minutes = 0;
  gint seconds_integer = 0;
  gdouble seconds_fractional = 0;
  gint divider = 10;

  HyScanSonarDataType sentence_type = hyscan_location_nmea_sentence_check(input);

  if (sentence_type == HYSCAN_SONAR_DATA_NMEA_RMC)
    {
      /* Время - это значение в 1 поле RMC. */
      while (*(ch++) != ',');

      if (*ch < '0' || *ch > '2')
        return output;
      hour = (*(ch++) - 48) * 10;
      if (*ch < '0' || *ch > '9')
        return output;
      hour += (*(ch++) - 48);
      if (*ch < '0' || *ch > '5')
        return output;
      minutes = (*(ch++) - 48) *10;
      if (*ch < '0' || *ch > '9')
        return output;
      minutes += (*(ch++) - 48);
      if (*ch < '0' || *ch > '5')
        return output;
      seconds_integer = (*(ch++) - 48) *10;
      if (*ch < '0' || *ch > '9')
        return output;
      seconds_integer += (*(ch++) - 48);

      /* Если далее идет точка, то считываем дробную часть. */
      if (*ch == '.')
      ch++;
      while (*ch >= '0' && *ch <= '9')
      {
        seconds_fractional += (gdouble)((*ch) - 48) / divider;
        divider *= 10;
        ch++;
      }
      seconds_fractional *= 1e5;

      /* Дата - это значение в 9 поле RMC. */
      for (i = 1; i < 9; i++)
        while (*(ch++) != ',');

      if (*ch < '0' || *ch > '9')
        return output;
      day = (*(ch++) - 48) * 10;
      if (*ch < '0' || *ch > '9')
        return output;
      day += (*(ch++) - 48);
      if (*ch < '0' || *ch > '9')
        return output;
      month = (*(ch++) - 48) *10;
      if (*ch < '0' || *ch > '9')
        return output;
      month += (*(ch++) - 48);
      if (*ch < '0' || *ch > '9')
        return output;
      year = (*(ch++) - 48) *10;
      if (*ch < '0' || *ch > '9')
        return output;
      year += (*(ch++) - 48) + 2000;

      /* Дата с начала эпохи юникс. */
      dt = g_date_time_new_utc (year, month, day, 0, 0, 0);
      output.date = g_date_time_to_unix (dt) * 1e6;
      g_date_time_unref (dt);

      /* Время с начала суток в микросекундах. */
      dt = g_date_time_new_utc (1970, 1, 1, hour, minutes, seconds_integer);
      output.time = g_date_time_to_unix (dt) * 1e6 + seconds_fractional;

      output.validity = HYSCAN_LOCATION_PARSED;
      g_date_time_unref (dt);
    }
  return output;
}

/* Функция извлекает только время из NMEA-строки. */
gint64
hyscan_location_nmea_time_get (gchar              *input,
                               HyScanSonarDataType sentence_type)
{
  gint64 output = {0};
  gchar *ch = input;

  GDateTime *dt;
  gint hour = 0;
  gint minutes = 0;
  gint seconds_integer = 0;
  gdouble seconds_fractional = 0;
  gint divider = 10;

  if (sentence_type == HYSCAN_SONAR_DATA_NMEA_RMC || sentence_type == HYSCAN_SONAR_DATA_NMEA_GGA )
    {
      /* Время - это значение в 1 поле RMC. */
      while (*(ch++) != ',');

      if (*ch < '0' || *ch > '2')
        return output;
      hour = (*(ch++) - 48) * 10;
      if (*ch < '0' || *ch > '9')
        return output;
      hour += (*(ch++) - 48);
      if (*ch < '0' || *ch > '5')
        return output;
      minutes = (*(ch++) - 48) *10;
      if (*ch < '0' || *ch > '9')
        return output;
      minutes += (*(ch++) - 48);
      if (*ch < '0' || *ch > '5')
        return output;
      seconds_integer = (*(ch++) - 48) *10;
      if (*ch < '0' || *ch > '9')
        return output;
      seconds_integer += (*(ch++) - 48);

      /* Если далее идет точка, то считываем дробную часть. */
      if (*ch == '.')
      ch++;
      while (*ch >= '0' && *ch <= '9')
      {
        seconds_fractional += (gdouble)((*ch) - 48) / divider;
        divider *= 10;
        ch++;
      }
      seconds_fractional *= 1e5;

      dt = g_date_time_new_utc (1970, 1, 1, hour, minutes, seconds_integer);
      output = g_date_time_to_unix (dt) * 1e6 + seconds_fractional;
      g_date_time_unref (dt);
    }
  return output;
}
