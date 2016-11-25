/*
 * Файл содержит следующие группы функций:
 *
 * Функции фильтрации и обработки данных.
 *
 */

#include <hyscan-location-tools.h>
#include <math.h>

/* Сглаживает данные кривой Безье. */
void
hyscan_location_4_point_2d_bezier (GArray *source,
                                   gint32  point1,
                                   gint32  point2,
                                   gint32  point3,
                                   gint32  point4,
                                   gdouble quality)
{
  HyScanLocationInternalData p1 = {0};
  HyScanLocationInternalData p2 = {0};
  HyScanLocationInternalData p3 = {0};
  HyScanLocationInternalData p4 = {0};
  gdouble t;
  gdouble p1_lat = NAN;
  gdouble p1_lon = NAN;
  gdouble p1_time = NAN;
  gdouble p2_lat = NAN;
  gdouble p2_lon = NAN;
  gdouble p2_time = NAN;
  gdouble p3_lat = NAN;
  gdouble p3_lon = NAN;
  gdouble p3_time = NAN;
  gdouble p4_lat = NAN;
  gdouble p4_lon = NAN;
  gdouble p4_time = NAN;
  gdouble out_lat = NAN;
  gdouble out_lon = NAN;
  gdouble out_time = NAN;

  gdouble k1 = 0;
  gdouble k2 = 0;
  gdouble k3 = 0;
  gdouble k4 = 0;

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

  p1 = g_array_index (source, HyScanLocationInternalData, point1);
  p2 = g_array_index (source, HyScanLocationInternalData, point2);
  p3 = g_array_index (source, HyScanLocationInternalData, point3);
  p4 = g_array_index (source, HyScanLocationInternalData, point4);

  p1_lat = p1.int_latitude;
  p1_lon = p1.int_longitude;
  p1_time = p1.db_time;
  p2_lat = p2.int_latitude;
  p2_lon = p2.int_longitude;
  p2_time = p2.db_time;
  p3_lat = p3.int_latitude;
  p3_lon = p3.int_longitude;
  p3_time = p3.db_time;
  p4_lat = p4.int_latitude;
  p4_lon = p4.int_longitude;
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

  p3.int_latitude = out_lat;
  p3.int_longitude = out_lon;
  p3.validity = HYSCAN_LOCATION_PREPROCESSED;
  g_array_remove_index (source, point3);
  g_array_insert_val (source, point3, p3);
}

/* Сдвижка данных во времени. */
void
hyscan_location_timeshift (HyScanDB *db,
                           GArray *source_list,
                           gint32  source,
                           GArray *cache,
                           gint32  datetime_source,
                           GArray *datetime_cache,
                           gint32  index)
{
  HyScanLocationInternalData *val;
  HyScanLocationInternalTime datetime;

  val = &g_array_index (cache, HyScanLocationInternalData, index);
  datetime = hyscan_location_getter_datetime (db, source_list, datetime_cache, datetime_source, val->db_time, 1);
  val->data_time += datetime.date;
  if (val->data_time < UNIX_1200 && datetime.time > UNIX_2300)
    val->data_time += 86400 * 1e6;
  val->data_time += datetime.time_shift;
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

  gdouble track = -psi;
  gdouble roll = gamma;
  gdouble pitch = theta;
  gdouble rot_x = 0;   /* Ось вращения, компонента x. */
  gdouble rot_y = 0;   /* Ось вращения, компонента y. */
  gdouble rot_z = 0;   /* Ось вращения, компонента z. */

  gdouble res_x = 0;   /* Результирующий вектор, компонента x. */
  gdouble res_y = 0;   /* Результирующий вектор, компонента y. */
  gdouble res_z = 0;   /* Результирующий вектор, компонента z. */
  gdouble temp_res_x = 0;   /* Результирующий вектор, компонента x. */
  gdouble temp_res_y = 0;   /* Результирующий вектор, компонента y. */
  gdouble temp_res_z = 0;   /* Результирующий вектор, компонента z. */
  gdouble cosfi;
  gdouble sinfi;

  /* Вращение в плоскости Oxy (курс). */
  /* Задаем ось. */
  rot_x = 0;
  rot_y = 0;
  rot_z = 1;
  cosfi = cos (track);
  sinfi = sin (track);

  res_x = (cosfi + (1 - cosfi)*rot_x*rot_x) * x + ((1 - cosfi)*rot_x*rot_y - sinfi*rot_z) * y + ((1 - cosfi)*rot_x*rot_z + sinfi*rot_y) * z;
  res_y = ((1 - cosfi)*rot_y*rot_x + sinfi*rot_z) * x + (cosfi + (1 - cosfi)*rot_y*rot_y) * y + ((1 - cosfi)*rot_y*rot_z - sinfi*rot_x) * z;
  res_z = ((1 - cosfi)*rot_z*rot_x - sinfi*rot_y) * x + ((1 - cosfi)*rot_z*rot_y + sinfi*rot_x) * y + (cosfi + (1 - cosfi)*rot_z*rot_z) * z;

  temp_res_x = res_x;
  temp_res_y = res_y;
  temp_res_z = res_z;

  /* Вращение в плоскости Oyz (дифферент). */
  /* Задаем ось. */
  rot_x = -res_y;
  rot_y = res_x;
  rot_z = 1;
  cosfi = cos (pitch);
  sinfi = sin (pitch);

  res_x = (cosfi + (1 - cosfi)*rot_x*rot_x) * temp_res_x + ((1 - cosfi)*rot_x*rot_y - sinfi*rot_z) * temp_res_y + ((1 - cosfi)*rot_x*rot_z + sinfi*rot_y) * temp_res_z;
  res_y = ((1 - cosfi)*rot_y*rot_x + sinfi*rot_z) * temp_res_x + (cosfi + (1 - cosfi)*rot_y*rot_y) * temp_res_y + ((1 - cosfi)*rot_y*rot_z - sinfi*rot_x) * temp_res_z;
  res_z = ((1 - cosfi)*rot_z*rot_x - sinfi*rot_y) * temp_res_x + ((1 - cosfi)*rot_z*rot_y + sinfi*rot_x) * temp_res_y + (cosfi + (1 - cosfi)*rot_z*rot_z) * temp_res_z;

  temp_res_x = res_x;
  temp_res_y = res_y;
  temp_res_z = res_z;

  /* Вращение в плоскости Ozx (крен). */
  /* Задаем ось. */
  rot_x = res_x;
  rot_y = res_y;
  rot_z = res_z;
  cosfi = cos (roll);
  sinfi = sin (roll);

  res_x = (cosfi + (1 - cosfi)*rot_x*rot_x) * temp_res_x + ((1 - cosfi)*rot_x*rot_y - sinfi*rot_z) * temp_res_y + ((1 - cosfi)*rot_x*rot_z + sinfi*rot_y) * temp_res_z;
  res_y = ((1 - cosfi)*rot_y*rot_x + sinfi*rot_z) * temp_res_x + (cosfi + (1 - cosfi)*rot_y*rot_y) * temp_res_y + ((1 - cosfi)*rot_y*rot_z - sinfi*rot_x) * temp_res_z;
  res_z = ((1 - cosfi)*rot_z*rot_x - sinfi*rot_y) * temp_res_x + ((1 - cosfi)*rot_z*rot_y + sinfi*rot_x) * temp_res_y + (cosfi + (1 - cosfi)*rot_z*rot_z) * temp_res_z;

  /* Сейчас значения смещений заданы в метрах. Их необходимо перевести в градусы.
   * Для этого нужно поделить смещение на длину градуса в метрах.
   * Длина одного градуса долготы зависит от широты. На экваторе 111321.378 метров, а дальше нужно умножать на косинус широты.
   */
  data->latitude -= res_x / ONE_DEG_LENGTH;
  data->longitude -= res_y / (ONE_DEG_LENGTH * cos (data->latitude * G_PI / 180.0));
  data->altitude -= res_z;
}

/* Сдвижка данных в пространстве. */
void
hyscan_location_shift2 (HyScanLocationData *data,
                        gdouble             x,
                        gdouble             y,
                        gdouble             z,
                        gdouble             psi,
                        gdouble             gamma,
                        gdouble             theta)
{

  gdouble track = -psi;
  gdouble roll = gamma;
  gdouble pitch = theta;
  gdouble r_ax_x = 0;   /* Ось вращения, компонента x. */
  gdouble r_ax_y = 0;   /* Ось вращения, компонента y. */
  gdouble r_ax_z = 0;   /* Ось вращения, компонента z. */
  gdouble ax_x = 0;   /* Ось вращения, компонента x. */
  gdouble ax_y = 0;   /* Ось вращения, компонента y. */
  gdouble ax_z = 0;   /* Ось вращения, компонента z. */
  gdouble t_ax_x = 0;   /* Ось вращения, компонента x. */
  gdouble t_ax_y = 0;   /* Ось вращения, компонента y. */
  gdouble t_ax_z = 0;   /* Ось вращения, компонента z. */

  gdouble res_x = 0;   /* Результирующий вектор, компонента x. */
  gdouble res_y = 0;   /* Результирующий вектор, компонента y. */
  gdouble res_z = 0;   /* Результирующий вектор, компонента z. */
  gdouble cosfi;
  gdouble sinfi;

  /* 1. Вращение в плоскости Oxy (курс). */
  /* 1.1 Задаем ось, вокруг которой происходит вращение. */
  r_ax_x = 0;
  r_ax_y = 0;
  r_ax_z = 1;

  /* 1.2 Задаем вспомогательную ось, вокруг которой  будет происходить вращение на следующем шаге. */
  ax_x = 0;
  ax_y = 1;
  ax_z = 0;
  cosfi = cos (track);
  sinfi = sin (track);

  /* 1.3 Вращаем вспомогательную ось. */
  t_ax_x = (cosfi + (1 - cosfi)*r_ax_x*r_ax_x) * ax_x + ((1 - cosfi)*r_ax_x*r_ax_y - sinfi*r_ax_z) * ax_y + ((1 - cosfi)*r_ax_x*r_ax_z + sinfi*r_ax_y) * ax_z;
  t_ax_y = ((1 - cosfi)*r_ax_y*r_ax_x + sinfi*r_ax_z) * ax_x + (cosfi + (1 - cosfi)*r_ax_y*r_ax_y) * ax_y + ((1 - cosfi)*r_ax_y*r_ax_z - sinfi*r_ax_x) * ax_z;
  t_ax_z = ((1 - cosfi)*r_ax_z*r_ax_x - sinfi*r_ax_y) * ax_x + ((1 - cosfi)*r_ax_z*r_ax_y + sinfi*r_ax_x) * ax_y + (cosfi + (1 - cosfi)*r_ax_z*r_ax_z) * ax_z;

  /* 1.4 Вращаем вектор, задающий положение датчика. */
  res_x = (cosfi + (1 - cosfi)*r_ax_x*r_ax_x) * x + ((1 - cosfi)*r_ax_x*r_ax_y - sinfi*r_ax_z) * y + ((1 - cosfi)*r_ax_x*r_ax_z + sinfi*r_ax_y) * z;
  res_y = ((1 - cosfi)*r_ax_y*r_ax_x + sinfi*r_ax_z) * x + (cosfi + (1 - cosfi)*r_ax_y*r_ax_y) * y + ((1 - cosfi)*r_ax_y*r_ax_z - sinfi*r_ax_x) * z;
  res_z = ((1 - cosfi)*r_ax_z*r_ax_x - sinfi*r_ax_y) * x + ((1 - cosfi)*r_ax_z*r_ax_y + sinfi*r_ax_x) * y + (cosfi + (1 - cosfi)*r_ax_z*r_ax_z) * z;

  /* 1.5 Запоминаем новую вспомогательную ось и вектор датчика. */
  ax_x = t_ax_x;
  ax_y = t_ax_y;
  ax_z = t_ax_z;

  x = res_x;
  y = res_y;
  z = res_z;

  /* 2. Вращение в *новой* плоскости Oyz (дифферент). */
  /* 2.1 Задаем ось, вокруг которой происходит вращение. */
  r_ax_x = -ax_y;
  r_ax_y = ax_x;
  r_ax_z = 0;

  /* 2.2 Вспомогательную ось уже задавать не надо. */
  cosfi = cos (pitch);
  sinfi = sin (pitch);

  /* 2.3 Вращаем вспомогательную ось. */
  t_ax_x = (cosfi + (1 - cosfi)*r_ax_x*r_ax_x) * ax_x + ((1 - cosfi)*r_ax_x*r_ax_y - sinfi*r_ax_z) * ax_y + ((1 - cosfi)*r_ax_x*r_ax_z + sinfi*r_ax_y) * ax_z;
  t_ax_y = ((1 - cosfi)*r_ax_y*r_ax_x + sinfi*r_ax_z) * ax_x + (cosfi + (1 - cosfi)*r_ax_y*r_ax_y) * ax_y + ((1 - cosfi)*r_ax_y*r_ax_z - sinfi*r_ax_x) * ax_z;
  t_ax_z = ((1 - cosfi)*r_ax_z*r_ax_x - sinfi*r_ax_y) * ax_x + ((1 - cosfi)*r_ax_z*r_ax_y + sinfi*r_ax_x) * ax_y + (cosfi + (1 - cosfi)*r_ax_z*r_ax_z) * ax_z;

  /* 2.4 Вращаем вектор, задающий положение датчика. */
  res_x = (cosfi + (1 - cosfi)*r_ax_x*r_ax_x) * x + ((1 - cosfi)*r_ax_x*r_ax_y - sinfi*r_ax_z) * y + ((1 - cosfi)*r_ax_x*r_ax_z + sinfi*r_ax_y) * z;
  res_y = ((1 - cosfi)*r_ax_y*r_ax_x + sinfi*r_ax_z) * x + (cosfi + (1 - cosfi)*r_ax_y*r_ax_y) * y + ((1 - cosfi)*r_ax_y*r_ax_z - sinfi*r_ax_x) * z;
  res_z = ((1 - cosfi)*r_ax_z*r_ax_x - sinfi*r_ax_y) * x + ((1 - cosfi)*r_ax_z*r_ax_y + sinfi*r_ax_x) * y + (cosfi + (1 - cosfi)*r_ax_z*r_ax_z) * z;

  /* 2.5 Запоминаем новую вспомогательную ось и вектор датчика. */
  ax_x = t_ax_x;
  ax_y = t_ax_y;
  ax_z = t_ax_z;

  x = res_x;
  y = res_y;
  z = res_z;

  /* 3. Вращение в *новой* плоскости Ozx (крен). */
  /* 3.1 Ось, вокруг которой происходит вращение, совпадает со вспомогательной осью. */
  /* Задаем ось. */
  r_ax_x = ax_x;
  r_ax_y = ax_y;
  r_ax_z = ax_z;
  cosfi = cos (roll);
  sinfi = sin (roll);

  /* 3.3 Вращать вспомогательную ось более не требуется. */
  /* 3.4 Вращаем вектор, задающий положение датчика. */
  res_x = (cosfi + (1 - cosfi)*r_ax_x*r_ax_x) * x + ((1 - cosfi)*r_ax_x*r_ax_y - sinfi*r_ax_z) * y + ((1 - cosfi)*r_ax_x*r_ax_z + sinfi*r_ax_y) * z;
  res_y = ((1 - cosfi)*r_ax_y*r_ax_x + sinfi*r_ax_z) * x + (cosfi + (1 - cosfi)*r_ax_y*r_ax_y) * y + ((1 - cosfi)*r_ax_y*r_ax_z - sinfi*r_ax_x) * z;
  res_z = ((1 - cosfi)*r_ax_z*r_ax_x - sinfi*r_ax_y) * x + ((1 - cosfi)*r_ax_z*r_ax_y + sinfi*r_ax_x) * y + (cosfi + (1 - cosfi)*r_ax_z*r_ax_z) * z;

  /* Сейчас значения смещений заданы в метрах. Их необходимо перевести в градусы.
   * Для этого нужно поделить смещение на длину градуса в метрах.
   * Длина одного градуса долготы зависит от широты. На экваторе 111321.378 метров, а дальше нужно умножать на косинус широты.
   */
  data->latitude -= res_x / ONE_DEG_LENGTH;
  data->longitude -= res_y / (ONE_DEG_LENGTH * cos (data->latitude * G_PI / 180.0));
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
  HyScanLocationInternalData *p1, *p2, *p3;

  gdouble out_lat  = NAN;
  gdouble out_lon  = NAN;
  gdouble dlat = 0;
  gdouble dlon = 0;

  gdouble k_lat = 0;
  gdouble k_lon = 0;
  gdouble b_lat = 0;
  gdouble b_lon = 0;

  /* Граница. */
  gdouble threshold = (10 - 9 * quality);
  gint i;

  p1 = &g_array_index (source, HyScanLocationInternalData, *point1);
  p2 = &g_array_index (source, HyScanLocationInternalData, point2);

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
          p3 = &g_array_index (source, HyScanLocationInternalData, *point1 + i);
          dlat = p1->int_latitude* ONE_DEG_LENGTH - p3->int_latitude* ONE_DEG_LENGTH;
          dlon = p1->int_longitude* ONE_DEG_LENGTH * cos (p1->int_latitude * G_PI/180.0) - p3->int_longitude* ONE_DEG_LENGTH * cos (p1->int_latitude * G_PI/180.0);
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

  p3 = &g_array_index (source, HyScanLocationInternalData, *point3);

  /* Определяем параметры прямой, проходящей через точки p1 и p3. */
  k_lat = (p3->int_latitude - p1->int_latitude)/(p3->db_time - p1->db_time);
  k_lon = (p3->int_longitude - p1->int_longitude)/(p3->db_time - p1->db_time);
  b_lat = p1->int_latitude - k_lat * p1->db_time;
  b_lon = p1->int_longitude - k_lon * p1->db_time;

  /* Кладем p2 на определенную выше прямую. */
  out_lat = k_lat * p2->db_time + b_lat;
  out_lon = k_lon * p2->db_time + b_lon;

  p2->int_latitude = out_lat;
  p2->int_longitude = out_lon;
  p2->validity = HYSCAN_LOCATION_PROCESSED;

  return TRUE;
}

/* Линеаризация трека. */
gboolean
hyscan_location_thresholder2 (GArray   *source,
                              guint32  *point2,
                              guint32   point3,
                              guint32  *point4,
                              guint32   last_index,
                              gboolean  is_writeable,
                              gdouble   quality)
{
  HyScanLocationInternalData *p1, *p2, *p3, *p4;

  gdouble out_lat  = NAN;
  gdouble out_lon  = NAN;
  gdouble dlat = 0;
  gdouble dlon = 0;

  gdouble k_lat = 0;
  gdouble k_lon = 0;
  gdouble b_lat = 0;
  gdouble b_lon = 0;

  /* Граница. */
  gdouble threshold = 50;
  gint32 i;
  gdouble prev_track;
  gdouble track;
  gdouble min_track_delta;
  gdouble distance;

  if (*point2 != 0)
    p1 = &g_array_index (source, HyScanLocationInternalData, *point2 - 1);
  else
    p1 = &g_array_index (source, HyScanLocationInternalData, *point2);

  p2 = &g_array_index (source, HyScanLocationInternalData, *point2);
  p3 = &g_array_index (source, HyScanLocationInternalData, point3);

  /* Если точка принудительно установлена пользователем, то никуда двигать её не надо.*/
  if (p3->validity == HYSCAN_LOCATION_USER_VALID)
    {
      p3->validity = HYSCAN_LOCATION_PROCESSED;
      return TRUE;
    }

  /* Ищем p4 такую, что изменение курса - минимально. */
  if (*point4 <= *point2)
    {
      /* 1 часть алгоритма: инициализация и вычисление курса на предыдущем отрезке. */
      /* Курс на предыдущем участке. */
      prev_track = hyscan_location_track_calculator (p1->int_latitude, p1->int_longitude, p2->int_latitude, p2->int_longitude);

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
      p4 = &g_array_index (source, HyScanLocationInternalData, *point4);
      min_track_delta = hyscan_location_track_calculator (p2->int_latitude, p2->int_longitude, p4->int_latitude, p4->int_longitude);
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

          p4 = &g_array_index (source, HyScanLocationInternalData, *point2 + i);

          /* Проверяем, что точка лежит в пределах, установленных переменной threshold. */
          dlat = p2->int_latitude * ONE_DEG_LENGTH - p4->int_latitude * ONE_DEG_LENGTH;
          dlon = p2->int_longitude * ONE_DEG_LENGTH * cos (p2->int_latitude * G_PI/180.0) - p4->int_longitude * ONE_DEG_LENGTH * cos (p2->int_latitude * G_PI/180.0);
          distance = sqrt (dlat * dlat + dlon * dlon);

          if (distance > threshold)
            break;

          track = hyscan_location_track_calculator (p2->int_latitude, p2->int_longitude, p4->int_latitude, p4->int_longitude);
          track -= prev_track;
          if (track > 180.0)
            track = 360.0 - track;
          if (track < -180.0)
            track = 360.0 + track;

          if (fabs(track) < fabs(min_track_delta))
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
      p3->validity = HYSCAN_LOCATION_PROCESSED;
      return TRUE;
    }

  p4 = &g_array_index (source, HyScanLocationInternalData, *point4);

  /* Определяем параметры прямой, проходящей через точки p2 и p4. */
  k_lat = (p4->int_latitude - p2->int_latitude)/(p4->db_time - p2->db_time);
  k_lon = (p4->int_longitude - p2->int_longitude)/(p4->db_time - p2->db_time);
  b_lat = p2->int_latitude - k_lat * p2->db_time;
  b_lon = p2->int_longitude - k_lon * p2->db_time;

  /* Кладем p2 на определенную выше прямую. */
  out_lat = k_lat * p3->db_time + b_lat;
  out_lon = k_lon * p3->db_time + b_lon;

  p3->int_latitude = out_lat;
  p3->int_longitude = out_lon;
  p3->validity = HYSCAN_LOCATION_PROCESSED;

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
  gdouble x = 0.0;
  gdouble y = 0.0;

  if (f2 == f1 && l2 == l1)
    return 0;

  x = cos (f2) * sin (l2-l1);
  y = cos (f1) * sin (f2) - sin (f1) * cos (f2) * cos (l2-l1);

  if (y > 0)
    {
      if (x > 0)
        track = atan (y/x);
      if (x < 0)
        track = G_PI - atan (-y/x);
      if (x == 0)
        track = G_PI/ 2.0;
    }

  if (y < 0)
    {
      if (x > 0)
        track = - atan (-y/x);
      if (x < 0)
        track = atan (y/x) - G_PI;
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

/* Вычисление скорости. */
gdouble
hyscan_location_speed_calculator (gdouble lat1,
                                  gdouble lon1,
                                  gdouble lat2,
                                  gdouble lon2,
                                  gdouble time)
{
  gdouble f2 = 0;
  gdouble f1 = 0;
  gdouble l2 = 0;
  gdouble l1 = 0;
  gdouble dlon = 0;
  gdouble dlat = 0;

  if (time == 0.0)
    return 0.0;

  f2 = lat2 * G_PI/ 180.0;
  f1 = lat1 * G_PI/ 180.0;
  l2 = lon2 * G_PI/ 180.0;
  l1 = lon1 * G_PI/ 180.0;

  dlon = (l2 - l1) * ONE_RAD_LENGTH * cos (f2);
  dlat = (f2 - f1) * ONE_RAD_LENGTH;

  return sqrt (pow (dlat, 2) + pow (dlon, 2)) / (ABS(time) / 1e6);
}

gboolean
hyscan_location_find_data (GArray *cache,
                           GArray *source_list,
                           gint32  source,
                           gint64  time,
                           gint32 *lindex,
                           gint32 *rindex,
                           gint64 *ltime,
                           gint64 *rtime)
{
  HyScanLocationInternalData *data;
  HyScanLocationSourcesList *source_info = &g_array_index (source_list, HyScanLocationSourcesList, source);
  gint32 first_index = 0;
  gint32 last_index = source_info->processing_index - 1;
  gint32 mid_index = 0;

  if (first_index == last_index || last_index < 0)
    return FALSE;

  mid_index = first_index;
  data = &g_array_index (cache, HyScanLocationInternalData, mid_index);
  if (data->data_time > time)
    {
      *lindex = G_MININT32;
      *rindex = first_index;
      return FALSE;
    }

  mid_index = last_index;
  data = &g_array_index (cache, HyScanLocationInternalData, mid_index);
  if (data->data_time < time)
    {
      *lindex = last_index;
      *rindex = G_MAXINT32;
      return FALSE;
    }

  while (TRUE)
    {
      /* Найдены точные данные. */
      if (data->data_time == time)
        {
          *lindex = mid_index;
          *rindex = mid_index;
          *ltime = data->data_time;
          *rtime = data->data_time;
          return TRUE;
        }

      if (last_index - first_index == 1)
        {
          *lindex = first_index;
          data = &g_array_index (cache, HyScanLocationInternalData, first_index);
          *ltime = data->data_time;
          *rindex = last_index;
          data = &g_array_index (cache, HyScanLocationInternalData, last_index);
          *rtime = data->data_time;
          return TRUE;
        }

      /* Сужаем границы поиска. */
      mid_index = first_index + (last_index - first_index) / 2;

      data = &g_array_index (cache, HyScanLocationInternalData, mid_index);
      if (data == NULL)
        return FALSE;

      /* Обновляем границы поиска. */
      if (data->data_time <= time)
        first_index = mid_index;

      if (data->data_time > time)
        last_index = mid_index;
    }

  return FALSE;
}

gboolean
hyscan_location_find_time (GArray *cache,
                           GArray *source_list,
                           gint32  source,
                           gint64  time,
                           gint32 *lindex,
                           gint32 *rindex)
{
  HyScanLocationSourcesList *source_info = &g_array_index (source_list, HyScanLocationSourcesList, source);
  HyScanLocationInternalTime *data;

  gint32 first_index = 0;
  gint32 last_index = source_info->processing_index - 1;
  gint32 mid_index = 0;

  if (first_index == last_index || last_index < 0)
    return FALSE;


  data = &g_array_index (cache, HyScanLocationInternalTime, first_index);
  if (data->db_time > time)
    {
      *lindex = G_MININT32;
      *rindex = first_index;
      return FALSE;
    }

  data = &g_array_index (cache, HyScanLocationInternalTime, last_index);
  if (data->db_time < time)
    {
      *lindex = last_index;
      *rindex = G_MAXINT32;
      return FALSE;
    }

  while (TRUE)
    {
      /* Найдены точные данные. */
      if (data->db_time == time)
        {
          *lindex = first_index;
          *rindex = first_index;
          return TRUE;
        }

      if (last_index - first_index == 1)
        {
          *lindex = first_index;
          *rindex = last_index;
          return TRUE;
        }

      /* Сужаем границы поиска. */
      mid_index = first_index + (last_index - first_index) / 2;

      data = &g_array_index (cache, HyScanLocationInternalTime, mid_index);
      if (data == NULL)
        return FALSE;

      /* Обновляем границы поиска. */
      if (data->db_time <= time)
          first_index = mid_index;

      if (data->db_time > time)
          last_index = mid_index;
    }

  return FALSE;
}
