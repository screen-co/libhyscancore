#include <hyscan-location-tools.h>
#include <math.h>

/* Сглаживает данные кривой Безье. */
void
hyscan_location_4_point_2d_bezier (GArray *source,
                                   gint32 point1,
                                   gint32 point2,
                                   gint32 point3,
                                   gint32 point4,
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
  /* TODO: выбрать алгоритм сглаживания координат. */
  /* версия 1: */
  /*
  t = 1.0 / 3.0 * (1+quality);

  k1 = pow (1-t, 3);
  k2 = 3 * t * pow (1-t,2);
  k3 = 3 * t * t * (1 - t);
  k4 = pow (t, 3);

  out_lat = k1 * p1_lat + k2 * p2_lat + k3 * p3_lat + k4 * p4_lat;
  out_lon = k1 * p1_lon + k2 * p2_lon + k3 * p3_lon + k4 * p4_lon;
  */
  /*============================================================*/
  /* версия 2:
   * высчитываем опорную точку, считаем, что судно движется прямолинейно от т. 1 к опорной,
   * а потом вычисляем, где судно будет в момент т.3.
   */
  //t = 1.0 / 3.0 * (1+quality);

  k1 = pow (1-t, 3);
  k2 = 3 * t * pow (1-t,2);
  k3 = 3 * t * t * (1 - t);
  k4 = pow (t, 3);

  out_lat = k1 * p1_lat + k2 * p2_lat + k3 * p3_lat + k4 * p4_lat;
  out_lon = k1 * p1_lon + k2 * p2_lon + k3 * p3_lon + k4 * p4_lon;
  out_time = k1 * p1_time + k2 * p2_time + k3 * p3_time + k4 * p4_time;

  out_lat = p1_lat + (out_lat - p1_lat) * (p3_time - p1_time) / (out_time - p1_time);
  out_lon = p1_lon + (out_lon - p1_lon) * (p3_time - p1_time) / (out_time - p1_time);

  /*============================================================*/
  /* версия 3:
   * высчитываем опорную точку, а потом считаем, что судно движется прямолинейно от т. 1 к искомой таким образом,
   * что расстояние от т.3 к искомой равно расстоянию от опорной до прямой т.1-т.3
   */
  /*
  k1 = pow (1-t, 3);
  k2 = 3 * t * pow (1-t,2);
  k3 = 3 * t * t * (1 - t);
  k4 = pow (t, 3);

  out_lat = k1 * p1_lat + k2 * p2_lat + k3 * p3_lat + k4 * p4_lat;
  out_lon = k1 * p1_lon + k2 * p2_lon + k3 * p3_lon + k4 * p4_lon;

  if (p3_lat - p1_lat == 0.0)
    {
      out_lon = p3_lon;
    }
  if (p3_lon - p1_lon == 0.0)
    {
      out_lat = p3_lat;
    }
  if (p3_lon - p1_lon != 0.0 && p3_lat - p1_lat != 0.0)
    {
      k = (p3_lat - p1_lat) / (p3_lon - p1_lon);
      b = p1_lat - k * p1_lon;

      temp_lon = -(b - out_lon * 1/k - out_lat) / (k + 1/k);
      temp_lat = k * temp_lon + b;

      out_lat = p3_lat + out_lat - temp_lat;
      out_lon = p3_lon + out_lon - temp_lon;
    }
  */
  p3.value1 = out_lat;
  p3.value2 = out_lon;
  p3.validity = TRUE;
  g_array_remove_index (source, point3);
  g_array_insert_val (source, point3, p3);
}

void
hyscan_location_3_point_2d_bezier (GArray *source,
                                   gint32 point1,
                                   gint32 point2,
                                   gint32 point3,
                                   gdouble quality)
{

}

/* Среднее арифметическое трех точек. */
void
hyscan_location_3_point_average (GArray *source,
                                 gint32 point1,
                                 gint32 point2,
                                 gint32 point3,
                                 gdouble quality)
{
  HyScanLocationGdouble1 p1 = {0},
                         p2 = {0},
                         p3 = {0};
  gdouble p1_val = NAN,
          p2_val = NAN,
          p3_val = NAN,
          out_val = NAN;
  p1 = g_array_index (source, HyScanLocationGdouble1, point1);
  p2 = g_array_index (source, HyScanLocationGdouble1, point2);
  p3 = g_array_index (source, HyScanLocationGdouble1, point3);

  p1_val = p1.value;
  p2_val = p2.value;
  p3_val = p3.value;

  out_val = (p1_val + p2_val + p3_val)/ 3.0;
  p3.value = out_val;
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
  data->latitude -= res_y / 111321.378;
  data->longitude -= res_x / (111321.378 * cos(data->latitude * G_PI / 180.0));
  data->altitude -= res_z;

}
