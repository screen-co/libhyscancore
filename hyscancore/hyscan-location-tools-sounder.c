#define DEPTH_MAXPEAKS 10
#include <hyscan-location-tools.h>

HyScanLocationGdouble1  hyscan_location_echosounder_depth_get   (gfloat *input,
                                                                 gint   input_size,
                                                                 gfloat discretization_frequency,
                                                                 GArray *input_soundspeed)
{
  HyScanLocationGdouble1 output = {0};
  int i = 0,
      j = 0,
      k = 0;
  gfloat average_value = 0;             /* потребуется для усреднения. */
  gfloat stdev = 0;                     /* среднеквадратичное отклонение .*/
  gint peaks[2][DEPTH_MAXPEAKS] = { {0},{0} };/* координаты пиков, при этом peaks[0] - это начала, peaks[1] - концы .*/
  gint peakcounter = 0;                 /* Счетчик пиков .*/
  gint widest_peak_size = 0;            /* Размер самого широкого пика.*/
  gint widest_peak_begin = 0;           /* Координата начала самого широкого пика .*/
  gdouble depthvalue = NAN;             /* Глубина. Принимает значение NAN, если что-то не так. */

  gfloat *data_buffer0 = g_memdup (input, input_size * sizeof(gfloat));
  gfloat *data_buffer1 = g_malloc0 (input_size * sizeof(gfloat));

  gint32 soundspeed_max = 0;            /* Индекс наибольшего элемента таблицы скорости звука, меньшего определенного номера дискреты*/
  SoundSpeedTable sst;                  /* Временная таблица скорости звука. */
  gdouble sum = 0;
  gdouble *soundspeed0, *soundspeed1;
  gint32 soundspeed_size = input_soundspeed->len;

  /* Считываем таблицу скорости звука. */
  if (input_soundspeed == NULL || soundspeed_size == 0)
    {
      soundspeed0 = g_malloc0 (sizeof(gdouble));
      soundspeed1 = g_malloc0 (sizeof(gdouble));
      *soundspeed0 = 0;
      *soundspeed1 = 1500;
    }

  else
  {
    soundspeed0 = g_malloc0 (soundspeed_size * sizeof(gdouble));
    soundspeed1 = g_malloc0 (soundspeed_size * sizeof(gdouble));

    for (i = 0; i < soundspeed_size; i++)
    {
      sst = g_array_index (input_soundspeed, SoundSpeedTable, i);
      soundspeed0[i] = sst.depth * (discretization_frequency * 2 / sst.soundspeed) + sum;
      sum += soundspeed0[i];
      soundspeed1[i] = sst.soundspeed;
    }
  }

  /* Обрабатываем данные. */
  /* TODO: обсудить,сколько точек можно занулять. */
  /* Фильтруем вход. Сейчас просто берется среднее с окном 3. */
  data_buffer1[0] = data_buffer0[0];
  data_buffer1[input_size - 1] = data_buffer0[input_size - 1];
  average_value = data_buffer1[0] + data_buffer0[input_size - 1];
  for (i = 1; i < input_size - 1; i++)
    {
      data_buffer1[i] = (data_buffer0[i-1] + data_buffer0[i] + data_buffer0[i+1]) / 3.0;
      average_value += data_buffer1[i];
    }
  average_value /= input_size;

  /* Вычисляем СКО. */
  for (i = 0; i < input_size; i++)
    stdev += pow ((data_buffer1[i] - average_value), 2);

  stdev /= input_size;
  stdev += average_value; /* - это наш порог бинаризации */

  for (i = 0; i < input_size; i++)
    {
      if (data_buffer1[i] > stdev)
        data_buffer1[i] = 1;
      else
        data_buffer1[i] = 0;
    }

  /* Ищем первые DEPTH_MAXPEAKS пиков. */
  i = i + 0;
  for (i = 0; i < input_size && peakcounter < DEPTH_MAXPEAKS; i++)
    {
      /* тут есть проблема в логике. Если d_b1[i=0] == 1, то peaks[0][0]= i=0 и этот шаг пропускается.
       * Но нас это волновать не должно, т.к. мало того, что в 0 дна быть не должно,
       * так ещё и будем занулять эти точки.
       */
      if (peaks[0][peakcounter] == 0 && data_buffer1[i] > 0)
        peaks[0][peakcounter] = i;
      if (peaks[0][peakcounter] != 0 && (data_buffer1[i] == 0 || i == input_size - 1))
        {
          peaks[1][peakcounter] = i - 1;
          peakcounter++;
        }
    }

  /* Теперь объединяем пики, если расстояние от конца первого до начала второго не более
   * 1/ 4.0 расстояния от начала первого до конца второго.
   */
  for (i = 0; i < peakcounter && peakcounter > 1; i++)
    {
      for (j = i + 1; j < peakcounter; j++)
        {
          if ((float) (peaks[0][j] - peaks[1][i]) / (float) (peaks[1][j] - peaks[0][i]) <= 0.25)
            {
              for (k = peaks[1][i]; k < peaks[0][j]; k++)
                data_buffer1[k] = 1;
              peaks[1][i] = peaks[1][j];
            }
        }
    }

  /* Ищем наиболее широкий пик. */
  widest_peak_begin = peaks[0][0];
  widest_peak_size = peaks[1][0] - peaks[0][0];
  if (peakcounter > 1)
    {
      for (i = 1; i < peakcounter; i++)
        {
          if ((peaks[1][i] - peaks[0][i]) > widest_peak_size)
            {
              widest_peak_size = peaks[1][i] - peaks[0][i];
              widest_peak_begin = peaks[0][i];
            }
        }
    }

  /* Вычисляем глубину с учетом таблицы профиля скорости звука.
   * Для этого сначала проводим численное интегрирование,
   * а потом делим на 2*Fдискр.
   */
  depthvalue = 0;
  for (i = 0; i < soundspeed_size; i++)
    {
      if (widest_peak_begin > soundspeed0[i])
        {
          soundspeed_max = i;
          if (i > 0)
            {
              depthvalue += (soundspeed0[i] - soundspeed0[i-1]) * soundspeed1[i-1];
            }
        }
      if (widest_peak_begin <= soundspeed0[i])
        break;
    }
  depthvalue += (widest_peak_begin - soundspeed0[soundspeed_max]) * soundspeed1[soundspeed_max];
  depthvalue /= (discretization_frequency * 2);

  g_free (soundspeed0);
  g_free (soundspeed1);
  g_free (data_buffer0);
  g_free (data_buffer1);

  output.value = depthvalue;
  output.validity = TRUE;
  return output;
}

HyScanLocationGdouble1  hyscan_location_sonar_depth_get         (gfloat *input,
                                                                 gint   input_size,
                                                                 gfloat discretization_frequency,
                                                                 GArray *input_soundspeed)
{
  HyScanLocationGdouble1 output = {0};
  int i = 0,
      j = 0,
      k = 0;
  gfloat average_value = 0;             /* потребуется для усреднения. */
  gfloat stdev = 0;                     /* среднеквадратичное отклонение .*/
  gint peaks[2][DEPTH_MAXPEAKS] = { {0},{0} };/* координаты пиков, при этом peaks[0] - это начала, peaks[1] - концы .*/
  gint peakcounter = 0;                 /* Счетчик пиков .*/
  gint widest_peak_size = 0;            /* Размер самого широкого пика.*/
  gint widest_peak_begin = 0;           /* Координата начала самого широкого пика .*/
  gdouble depthvalue = NAN;             /* Глубина. Принимает значение NAN, если что-то не так. */

  gfloat *data_buffer0 = g_memdup (input, input_size * sizeof(gfloat));
  gfloat *data_buffer1 = g_malloc0 (input_size * sizeof(gfloat));

  gint32 soundspeed_max = 0;            /* Индекс наибольшего элемента таблицы скорости звука, меньшего определенного номера дискреты*/
  SoundSpeedTable sst;                  /* Временная таблица скорости звука. */
  gdouble sum = 0;
  gdouble *soundspeed0, *soundspeed1;
  gint32 soundspeed_size = input_soundspeed->len;

  /* Считываем таблицу скорости звука. */
  if (input_soundspeed == NULL || soundspeed_size == 0)
    {
      soundspeed0 = g_malloc0 (sizeof(gdouble));
      soundspeed1 = g_malloc0 (sizeof(gdouble));
      *soundspeed0 = 0;
      *soundspeed1 = 1500;
    }

  else
  {
    soundspeed0 = g_malloc0 (soundspeed_size * sizeof(gdouble));
    soundspeed1 = g_malloc0 (soundspeed_size * sizeof(gdouble));

    for (i = 0; i < soundspeed_size; i++)
    {
      sst = g_array_index (input_soundspeed, SoundSpeedTable, i);
      soundspeed0[i] = sst.depth * (discretization_frequency * 2 / sst.soundspeed) + sum;
      sum += soundspeed0[i];
      soundspeed1[i] = sst.soundspeed;
    }
  }

  /* Обрабатываем данные. */
  /* TODO: обсудить,сколько точек можно занулять. */
  /* Фильтруем вход. Сейчас просто берется среднее с окном 3. */
  data_buffer1[0] = data_buffer0[0];
  data_buffer1[input_size - 1] = data_buffer0[input_size - 1];

  for (i = 1; i < input_size - 1; i++)
    data_buffer1[i] = (data_buffer0[i - 1] + data_buffer0[i] + data_buffer0[i + 1]) / 3.0;

  /* Вычисляем интегральный массив*/
  for (i = 1; i < input_size; i++)
    {
      data_buffer0[i] = data_buffer1[i] + data_buffer0[i - 1];
    }

  /* Теперь перемножаем фильтрованный массив и интегральный, чтобы уменьшить влияние тех элементов, что далеко от начала координат*/
  for (i = 0; i < input_size; i++)
    {
      data_buffer1[i] *= (1 - data_buffer0[i] / data_buffer0[input_size - 1]);
      average_value += data_buffer1[i];
    }
  average_value /= input_size;

  /* Вычисляем СКО. */
  for (i = 0; i < input_size; i++)
    stdev += pow ((data_buffer1[i] - average_value), 2);

  stdev /= input_size;
  stdev = 2 * sqrt (stdev);
  stdev += average_value; /* - это наш порог бинаризации, среднее+2*ско */

  for (i = 0; i < input_size; i++)
   {
     if (data_buffer1[i] > stdev)
       data_buffer1[i] = 1;
     else
       data_buffer1[i] = 0;
   }

  /* Ищем первые DEPTH_MAXPEAKS пиков. */
  for (i = 0; i < input_size && peakcounter < DEPTH_MAXPEAKS; i++)
   {
     /* тут есть проблема в логике. Если d_b1[i=0] == 1, то peaks[0][0]= i=0 и этот шаг пропускается.
      * Но нас это волновать не должно, т.к. мало того, что в 0 дна быть не должно,
      * так ещё и будем занулять эти точки.
      */
     if (peaks[0][peakcounter] == 0 && data_buffer1[i] > 0)
       peaks[0][peakcounter] = i;
     if (peaks[0][peakcounter] != 0 && (data_buffer1[i] == 0 || i == input_size - 1))
       {
         peaks[1][peakcounter] = i - 1;
         peakcounter++;
       }
   }

  /* Теперь объединяем пики, если расстояние от конца первого до начала второго не более
   * 1/ 4.0 расстояния от начала первого до конца второго.
   */
  for (i = 0; i < peakcounter && peakcounter > 1; i++)
    {
      for (j = i + 1; j < peakcounter; j++)
        {
          if ((float) (peaks[0][j] - peaks[1][i]) / (float) (peaks[1][j] - peaks[0][i]) <= 0.25)
            {
              for (k = peaks[1][i]; k < peaks[0][j]; k++)
                data_buffer1[k] = 1;
              peaks[1][i] = peaks[1][j];
            }
        }
    }

  /* Ищем наиболее широкий пик. */
  widest_peak_begin = peaks[0][0];
  widest_peak_size = peaks[1][0] - peaks[0][0];
  if (peakcounter > 1)
    {
      for (i = 1; i < peakcounter; i++)
        {
          if ((peaks[1][i] - peaks[0][i]) > widest_peak_size)
            {
              widest_peak_size = peaks[1][i] - peaks[0][i];
              widest_peak_begin = peaks[0][i];
            }
        }
    }
  /* Вычисляем глубину по таблице глубина/скорость звука.
   * Для этого сначала проводим численное интегрирование,
   * а потом делим на 2*Fдискр.
   */
   depthvalue = 0;
   for (i = 0; i < soundspeed_size; i++)
     {
       if (widest_peak_begin > soundspeed0[i])
         {
           soundspeed_max = i;
           if (i > 0)
             {
               depthvalue += (soundspeed0[i] - soundspeed0[i-1]) * soundspeed1[i-1];
             }
         }
       if (widest_peak_begin <= soundspeed0[i])
         break;
     }
   depthvalue += (widest_peak_begin - soundspeed0[soundspeed_max]) * soundspeed1[soundspeed_max];
   depthvalue /= (discretization_frequency * 2);

  g_free (soundspeed0);
  g_free (soundspeed1);
  g_free (data_buffer0);
  g_free (data_buffer1);

  output.value = depthvalue;
  output.validity = TRUE;
  return output;
}
