#include <hyscan-location-tools.h>
#include <math.h>
#define ONE_DEG_LENGTH 111321.378 /* Длина одного радиана по экватору в метрах. */

/* Сглаживает данные кривой Безье. */
void
hyscan_location_4_point_2d_bezier (GArray *source,
                                   gint32  point1,
                                   gint32  point2,
                                   gint32  point3,
                                   gint32  point4,
                                   gdouble quality)
{
  HyScanLocationGdouble2 p1 = {0},
                         p2 = {0},
                         p3 = {0},
                         p4 = {0};
  gdouble t;
  gdouble p1_lat = NAN,
          p1_lon = NAN,
          p1_time = NAN,
          p2_lat = NAN,
          p2_lon = NAN,
          p2_time = NAN,
          p3_lat = NAN,
          p3_lon = NAN,
          p3_time = NAN,
          p4_lat = NAN,
          p4_lon = NAN,
          p4_time = NAN,
          out_lat = NAN,
          out_lon = NAN,
          out_time = NAN;

  gdouble k1 = 0,
          k2 = 0,
          k3 = 0,
          k4 = 0;
  /* t определяет момент времени, значение которого используется.
   * При quality == 0 берется момент времени 1/ 3.0,
   * при quality == 1 берется момент времени 2/ 3.0.
   * Таким образом, чем ниже quality, тем больше результирующая точка отстает от исходной.
   *
    * quality | влияние точки 1 | точки 2 | точки 3| точки 4|
    * :------ | :--------------:| :------:| :-----:| :------:
    * 0       | 0.296           | 0.444   | 0.222  | 0.037
    * 0.5     | 0.125           | 0.375   | 0.375  | 0.125
    * 1       | 0.037           | 0.222   | 0.444  | 0.296
    */

  p1 = g_array_index (source, HyScanLocationGdouble2, point1);
  p2 = g_array_index (source, HyScanLocationGdouble2, point2);
  p3 = g_array_index (source, HyScanLocationGdouble2, point3);
  p4 = g_array_index (source, HyScanLocationGdouble2, point4);

  p1_lat = p1.value1;
  p1_lon = p1.value2;
  p1_time = p1.db_time;
  p2_lat = p2.value1;
  p2_lon = p2.value2;
  p2_time = p2.db_time;
  p3_lat = p3.value1;
  p3_lon = p3.value2;
  p3_time = p3.db_time;
  p4_lat = p4.value1;
  p4_lon = p4.value2;
  p4_time = p4.db_time;

  t = (p2_time - p1_time) / (p4_time - p1_time) + quality * (p3_time - p2_time) / (p4_time - p1_time);

  k1 = pow (1-t, 3);
  k2 = 3 * t * pow (1-t,2);
  k3 = 3 * t * t * (1 - t);
  k4 = pow (t, 3);

  out_lat = k1 * p1_lat + k2 * p2_lat + k3 * p3_lat + k4 * p4_lat;
  out_lon = k1 * p1_lon + k2 * p2_lon + k3 * p3_lon + k4 * p4_lon;
  out_time = k1 * p1_time + k2 * p2_time + k3 * p3_time + k4 * p4_time;

  out_lat = p1_lat + (out_lat - p1_lat) * (p3_time - p1_time) / (out_time - p1_time);
  out_lon = p1_lon + (out_lon - p1_lon) * (p3_time - p1_time) / (out_time - p1_time);

  p3.value1 = out_lat;
  p3.value2 = out_lon;
  p3.validity = TRUE;
  g_array_remove_index (source, point3);
  g_array_insert_val (source, point3, p3);
}

/* Сдвижка данных в пространстве. */
void
hyscan_location_shift (HyScanLocationData *data,
                       gdouble             x,
                       gdouble             y,
                       gdouble             z,
                       gdouble             psi,
                       gdouble             gamma,
                       gdouble             theta)
{

  gdouble track = -psi,
          roll = gamma,
          pitch = theta;
  gdouble rot_x = 0,   /* Ось вращения, компонента x. */
          rot_y = 0,   /* Ось вращения, компонента y. */
          rot_z = 0;   /* Ось вращения, компонента z. */
  gdouble res_x = 0,   /* Результирующий вектор, компонента x. */
          res_y = 0,   /* Результирующий вектор, компонента y. */
          res_z = 0;   /* Результирующий вектор, компонента z. */
  gdouble temp_res_x = 0,   /* Результирующий вектор, компонента x. */
          temp_res_y = 0,   /* Результирующий вектор, компонента y. */
          temp_res_z = 0;   /* Результирующий вектор, компонента z. */
  gdouble cosfi,
          sinfi;

  /* Вращение в плоскости Oxy (курс).*/
  /* Задаем ось.*/
  rot_x = 0;
  rot_y = 0;
  rot_z = 1;
  cosfi = cos(track);
  sinfi = sin(track);
  res_x = (cosfi + (1 - cosfi)*rot_x*rot_x) * x + ((1 - cosfi)*rot_x*rot_y - sinfi*rot_z) * y + ((1 - cosfi)*rot_x*rot_z + sinfi*rot_y) * z;
  res_y = ((1 - cosfi)*rot_y*rot_x + sinfi*rot_z) * x + (cosfi + (1 - cosfi)*rot_y*rot_y) * y + ((1 - cosfi)*rot_y*rot_z - sinfi*rot_x) * z;
  res_z = ((1 - cosfi)*rot_z*rot_x - sinfi*rot_y) * x + ((1 - cosfi)*rot_z*rot_y + sinfi*rot_x) * y + (cosfi + (1 - cosfi)*rot_z*rot_z) * z;

  temp_res_x = res_x;
  temp_res_y = res_y;
  temp_res_z = res_z;

  /* Вращение в плоскости Oyz (дифферент).*/
  /* Задаем ось.*/
  rot_x = -res_y;
  rot_y = res_x;
  rot_z = 1;
  cosfi = cos(pitch);
  sinfi = sin(pitch);
  res_x = (cosfi + (1 - cosfi)*rot_x*rot_x) * temp_res_x + ((1 - cosfi)*rot_x*rot_y - sinfi*rot_z) * temp_res_y + ((1 - cosfi)*rot_x*rot_z + sinfi*rot_y) * temp_res_z;
  res_y = ((1 - cosfi)*rot_y*rot_x + sinfi*rot_z) * temp_res_x + (cosfi + (1 - cosfi)*rot_y*rot_y) * temp_res_y + ((1 - cosfi)*rot_y*rot_z - sinfi*rot_x) * temp_res_z;
  res_z = ((1 - cosfi)*rot_z*rot_x - sinfi*rot_y) * temp_res_x + ((1 - cosfi)*rot_z*rot_y + sinfi*rot_x) * temp_res_y + (cosfi + (1 - cosfi)*rot_z*rot_z) * temp_res_z;

  temp_res_x = res_x;
  temp_res_y = res_y;
  temp_res_z = res_z;

  /* Вращение в плоскости Ozx (крен).*/
  /* Задаем ось.*/
  rot_x = res_x;
  rot_y = res_y;
  rot_z = res_z;
  cosfi = cos(roll);
  sinfi = sin(roll);
  res_x = (cosfi + (1 - cosfi)*rot_x*rot_x) * temp_res_x + ((1 - cosfi)*rot_x*rot_y - sinfi*rot_z) * temp_res_y + ((1 - cosfi)*rot_x*rot_z + sinfi*rot_y) * temp_res_z;
  res_y = ((1 - cosfi)*rot_y*rot_x + sinfi*rot_z) * temp_res_x + (cosfi + (1 - cosfi)*rot_y*rot_y) * temp_res_y + ((1 - cosfi)*rot_y*rot_z - sinfi*rot_x) * temp_res_z;
  res_z = ((1 - cosfi)*rot_z*rot_x - sinfi*rot_y) * temp_res_x + ((1 - cosfi)*rot_z*rot_y + sinfi*rot_x) * temp_res_y + (cosfi + (1 - cosfi)*rot_z*rot_z) * temp_res_z;

  /* Сейчас значения смещений заданы в метрах. Их необходимо перевести в градусы.
   * Для этого нужно поделить смещение на длину градуса в метрах.
   * Длина одного градуса широты составляет 111321.378 метров.
   * Длина одного градуса долготы зависит от широты. На экваторе 111321.378 метров, а дальше нужно умножать на косинус широты.*/
  data->latitude -= res_x / 111321.378;
  data->longitude -= res_y / (111321.378 * cos(data->latitude * G_PI / 180.0));
  data->altitude -= res_z;

}

/* Линеаризация трека. */
gboolean
hyscan_location_thresholder (GArray  *source,
                             gint32  *point1,
                             gint32   point2,
                             gint32  *point3,
                             gint32   last_index,
                             gboolean is_writeable,
                             gdouble  quality)
{
  HyScanLocationGdouble2 *p1,
                         *p2,
                         *p3;

  gdouble out_lat  = NAN,
          out_lon  = NAN,
          dlat = 0,
          dlon = 0;

  gdouble k_lat = 0,
          k_lon = 0,
          b_lat = 0,
          b_lon = 0;

  /* Граница. */
  gdouble threshold = (10 - 9 * quality);
  gint i;

  p1 = &g_array_index (source, HyScanLocationGdouble2, *point1);
  p2 = &g_array_index (source, HyScanLocationGdouble2, point2);

  /* Ищем p3 такую, что расстояние от неё до p1 не меньше threshold. */
  if (*point3 <= *point1)
    {
      for (i = 1; ; i++)
        {
          /* Если данных пока нет (но они будут в дальнейшем), выходим из функции. */
          if (*point1 + i > last_index && is_writeable)
            return FALSE;
          /* Если данных нет и больше не будет, то берем последнюю точку. */
          if (*point1 + i > last_index && !is_writeable)
            {
              *point3 = last_index;
              break;
            }
          /* Если всё в порядке. */
          p3 = &g_array_index (source, HyScanLocationGdouble2, *point1 + i);
          dlat = p1->value1* ONE_DEG_LENGTH - p3->value1* ONE_DEG_LENGTH;
          dlon = p1->value2* ONE_DEG_LENGTH * cos(p1->value1 * G_PI/180.0) - p3->value2* ONE_DEG_LENGTH * cos(p1->value1 * G_PI/180.0);
          if (sqrt (dlat * dlat + dlon * dlon) > threshold)
            {
              *point3 = *point1 + i;
              break;
            }
        }
    }

  /* Если точки совпали, то обновляем p1 и выходим из функции, не меняя p2. */
  if (*point3 == point2)
    {
      *point1 = *point3;
      return TRUE;
    }

  p3 = &g_array_index (source, HyScanLocationGdouble2, *point3);
  /* Определяем параметры прямой, проходящей через точки p1 и p3. */
  k_lat = (p3->value1 - p1->value1)/(p3->db_time - p1->db_time);
  k_lon = (p3->value2 - p1->value2)/(p3->db_time - p1->db_time);
  b_lat = p1->value1 - k_lat * p1->db_time;
  b_lon = p1->value2 - k_lon * p1->db_time;

  /* Кладем p2 на определенную выше прямую. */
  out_lat = k_lat * p2->db_time + b_lat;
  out_lon = k_lon * p2->db_time + b_lon;

  p2->value1 = out_lat;
  p2->value2 = out_lon;
  p2->validity = TRUE;

  return TRUE;
}

/* Линеаризация трека. */
gboolean
hyscan_location_thresholder2 (GArray  *source,
                              gint32  *point2,
                              gint32   point3,
                              gint32  *point4,
                              gint32   last_index,
                              gboolean is_writeable,
                              gdouble  quality)
{
  HyScanLocationGdouble2 *p1,
                         *p2,
                         *p3,
                         *p4;

  gdouble out_lat  = NAN,
          out_lon  = NAN,
          dlat = 0,
          dlon = 0;

  gdouble k_lat = 0,
          k_lon = 0,
          b_lat = 0,
          b_lon = 0;

  /* Граница. */
  gdouble threshold = 50;
  gint32 i;
  gdouble prev_track,
          track,
          min_track_delta,
          distance;

  if (*point2 != 0)
    p1 = &g_array_index (source, HyScanLocationGdouble2, *point2 - 1);
  else
    p1 = &g_array_index (source, HyScanLocationGdouble2, *point2);

  p2 = &g_array_index (source, HyScanLocationGdouble2, *point2);
  p3 = &g_array_index (source, HyScanLocationGdouble2, point3);

  /* Ищем p4 такую, что изменение курса - минимально. */
  if (*point4 <= *point2)
    {
      /* 1 часть алгоритма: инициализация и вычисление курса на предыдущем отрезке. */
      /* Курс на предыдущем участке. */
      prev_track = hyscan_location_track_calculator (p1->value1, p1->value2, p2->value1, p2->value2);
      /* Инициализируем min_track_delta.
       * Если данных нет, но они будут в дальнейшем, выходим из функции,
       * а если данных нет и больше не будет, то берем последнюю точку. */
      if (*point2 + 1 > last_index && is_writeable)
        return FALSE;
      if (*point2 + 1 > last_index && !is_writeable)
        {
          *point4 = last_index;
          goto out;
        }
      *point4 = *point2 + 1;
      p4 = &g_array_index (source, HyScanLocationGdouble2, *point4);
      min_track_delta = hyscan_location_track_calculator (p2->value1, p2->value2, p4->value1, p4->value2);
      min_track_delta -=  prev_track;
      if (min_track_delta > 180.0)
        min_track_delta = 360.0 - min_track_delta;
      if (min_track_delta < -180.0)
        min_track_delta = 360.0 + min_track_delta;

      /* 2 часть алгоритма: поиск минимума. */
      for (i = 2; ; i++)
        {
          /* Выходим, если зашли слишком далеко. */
          if (*point2 + i > last_index)
              break;

          p4 = &g_array_index (source, HyScanLocationGdouble2, *point2 + i);
          /* Проверяем, что точка лежит в пределах, установленных переменной threshold. */
          dlat = p2->value1 * ONE_DEG_LENGTH - p4->value1 * ONE_DEG_LENGTH;
          dlon = p2->value2 * ONE_DEG_LENGTH * cos(p2->value1 * G_PI/180.0) - p4->value2 * ONE_DEG_LENGTH * cos(p2->value1 * G_PI/180.0);
          distance = sqrt (dlat * dlat + dlon * dlon);

          if (distance > threshold)
            break;

          track = hyscan_location_track_calculator (p2->value1, p2->value2, p4->value1, p4->value2);
          track -= prev_track;
          if (track > 180.0)
            track = 360.0 - track;
          if (track < -180.0)
            track = 360.0 + track;

          if (ABS(track) < ABS(min_track_delta))
            {
              min_track_delta = track;
              *point4 = *point2 + i;
            }
        }
    }
out:
  /* Если точки совпали, то обновляем p4 и выходим из функции, не меняя p3. */
  if (*point4 == point3)
    {
      *point2 = *point4;
      return TRUE;
    }

  p4 = &g_array_index (source, HyScanLocationGdouble2, *point4);
  /* Определяем параметры прямой, проходящей через точки p2 и p4. */
  k_lat = (p4->value1 - p2->value1)/(p4->db_time - p2->db_time);
  k_lon = (p4->value2 - p2->value2)/(p4->db_time - p2->db_time);
  b_lat = p2->value1 - k_lat * p2->db_time;
  b_lon = p2->value2 - k_lon * p2->db_time;

  /* Кладем p2 на определенную выше прямую. */
  out_lat = k_lat * p3->db_time + b_lat;
  out_lon = k_lon * p3->db_time + b_lon;

  p3->value1 = out_lat;
  p3->value2 = out_lon;
  p3->validity = TRUE;

  return TRUE;
}

/* Вычисление курса. */
gdouble
hyscan_location_track_calculator (gdouble lat1,
                                  gdouble lon1,
                                  gdouble lat2,
                                  gdouble lon2)
{
  gdouble track = 0;
  gdouble f2 = lat2 * G_PI/ 180.0;
  gdouble f1 = lat1 * G_PI/ 180.0;
  gdouble l2 = lon2 * G_PI/ 180.0;
  gdouble l1 = lon1 * G_PI/ 180.0;

  if (f2 == f1 && l2 == l1)
    return 0;

  gdouble x = cos(f2) * sin (l2-l1);
  gdouble y = cos (f1) * sin(f2) - sin(f1) * cos(f2) * cos(l2-l1);

  if (y > 0)
    {
      if (x > 0)
        track = atan(y/x);
      if (x < 0)
        track = G_PI - atan(-y/x);
      if (x == 0)
        track = G_PI/ 2.0;
    }

  if (y < 0)
    {
      if (x > 0)
        track = - atan(-y/x);
      if (x < 0)
        track = atan(y/x) - G_PI;
      if (x == 0)
        track = 3*G_PI/ 2.0;
    }

  if (y == 0)
    {
      if (x > 0)
        track = 0;
      if (x < 0)
        track = G_PI;
      if (x == 0)
        return 0;
    }

  /* Пересчитываем курс так, чтобы 0 соответствовал северу, а 90 - востоку. */
  track = 90.0 - track * 180.0/G_PI;
  if (track < 0)
    track = 360.0 + track;
  if (track > 360.0)
    track -= 360.0;
  return track;
}
