/* acoustic-data-test.c
 *
 * Copyright 2015-2018 Screen LLC, Andrei Fadeev <andrei@webcontrol.ru>
 *
 * This file is part of HyScanCore library.
 *
 * HyScanCore is dual-licensed: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * HyScanCore is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Alternatively, you can license this code under a commercial license.
 * Contact the Screen LLC in this case - <info@screen-co.ru>.
 */

/* HyScanCore имеет двойную лицензию.
 *
 * Во-первых, вы можете распространять HyScanCore на условиях Стандартной
 * Общественной Лицензии GNU версии 3, либо по любой более поздней версии
 * лицензии (по вашему выбору). Полные положения лицензии GNU приведены в
 * <http://www.gnu.org/licenses/>.
 *
 * Во-вторых, этот программный код можно использовать по коммерческой
 * лицензии. Для этого свяжитесь с ООО Экран - <info@screen-co.ru>.
 */

#include <hyscan-acoustic-data.h>
#include <hyscan-data-writer.h>
#include <hyscan-buffer.h>
#include <hyscan-cached.h>

#include <string.h>
#include <math.h>

#define PROJECT_NAME   "test"
#define TRACK_NAME     "test"

typedef struct _test_info test_info;
struct _test_info
{
  const gchar         *name;
  HyScanDataType       type;
  gdouble              error;
};

test_info real_test_types [] =
{
  { "adc14",   HYSCAN_DATA_ADC14LE,             1e-4 },
  { "adc16",   HYSCAN_DATA_ADC16LE,             1e-4 },
  { "adc24",   HYSCAN_DATA_ADC24LE,             1e-4 },
  { "float16", HYSCAN_DATA_FLOAT16LE,           1e-4 },
  { "float32", HYSCAN_DATA_FLOAT32LE,           1e-4 }
};

test_info complex_test_types [] =
{
  { "adc14",   HYSCAN_DATA_COMPLEX_ADC14LE,     1e-6 },
  { "adc16",   HYSCAN_DATA_COMPLEX_ADC16LE,     1e-6 },
  { "adc24",   HYSCAN_DATA_COMPLEX_ADC24LE,     1e-8 },
  { "float16", HYSCAN_DATA_COMPLEX_FLOAT16LE,   1e-5 },
  { "float32", HYSCAN_DATA_COMPLEX_FLOAT32LE,   1e-8 }
};

test_info amplitude_test_types [] =
{
  { "int8",    HYSCAN_DATA_AMPLITUDE_INT8,      1e-4 },
  { "int16",   HYSCAN_DATA_AMPLITUDE_INT16LE,   1e-6 },
  { "int24",   HYSCAN_DATA_AMPLITUDE_INT24LE,   1e-8 },
  { "int32",   HYSCAN_DATA_AMPLITUDE_INT32LE,   1e-9 },
  { "float16", HYSCAN_DATA_AMPLITUDE_FLOAT16LE, 1e-5 },
  { "float32", HYSCAN_DATA_AMPLITUDE_FLOAT32LE, 1e-9 }
};

/* Функция записывает тестовые гидроакустические данные. Записывается
 * n_signals * n_lines строк, каждая строка имеет размер 100 * n_signal_points,
 * где n_signal_points - размер сигнала равный discretization * duration.
 *
 * Функция формирует действительные или комплексные выборки данных в зависимости
 * от указанного типа данных - type.
 *
 * В строке, со смещением в два размера сигнала формируется синусоидальная
 * последовательность, которая после свёртки преобразуется в треугольник.
 *
 * Каждые n_lines строк записывается новый образ сигнала с частотой frequency0
 * и параметры ВАРУ, линейно увеличивающиеся по строке с шагом 1, начиная с tvg0.
 */
void
create_complex_data (HyScanDataWriter *writer,
                     HyScanSourceType  source,
                     guint             channel,
                     gboolean          noise,
                     HyScanDataType    type,
                     gdouble           discretization,
                     gdouble           frequency,
                     gdouble           duration,
                     guint             n_signals,
                     guint             n_lines)
{
  HyScanAcousticDataInfo acoustic_info;
  HyScanAntennaOffset offset;

  HyScanBuffer *channel_buffer;
  HyScanBuffer *data_buffer;

  guint n_signal_points;
  guint n_data_points;

  guint i, j;

  data_buffer = hyscan_buffer_new ();
  channel_buffer = hyscan_buffer_new ();

  /* Параметры данных. */
  acoustic_info.data_type = type;
  acoustic_info.data_rate = discretization;
  acoustic_info.signal_frequency = frequency;
  acoustic_info.signal_bandwidth = frequency;
  acoustic_info.signal_heterodyne = frequency;
  acoustic_info.antenna_voffset = 0.0;
  acoustic_info.antenna_hoffset = 0.0;
  acoustic_info.antenna_vaperture = 40.0;
  acoustic_info.antenna_haperture = 2.0;
  acoustic_info.antenna_frequency = frequency;
  acoustic_info.antenna_bandwidth = 0.1 * frequency;
  acoustic_info.antenna_group = 0;
  acoustic_info.adc_vref = 1.0;
  acoustic_info.adc_offset = 0;

  offset.starboard = 0.0;
  offset.forward = 0.0;
  offset.vertical = 0.0;
  offset.yaw = 0.0;
  offset.pitch = 0.0;
  offset.roll = 0.0;

  hyscan_data_writer_sonar_set_offset (writer, source, &offset);

  n_signal_points = discretization * duration;
  n_data_points = 100 * n_signal_points;

  for (i = 0; i < n_signals * n_lines; i++)
    {
      gdouble df, frequency0, tvg0;
      gint64 data_time;

      df = frequency / (10.0 * (n_signals - 1));
      frequency0 = -(df * (n_signals - 1) / 2.0) + (i / n_lines) * df;
      tvg0 = i / n_lines;
      data_time = 1000 * (i + 1);

      /* Записываем образ сигнала каждые n_lines строк. */
      if ((i % n_lines) == 0)
        {
          HyScanComplexFloat *data;
          gboolean status;

          hyscan_buffer_set_complex_float (data_buffer, NULL, n_signal_points);
          data = hyscan_buffer_get_complex_float (data_buffer, &n_signal_points);

          for (j = 0; j < n_signal_points; j++)
            {
              gdouble time = (1.0 / discretization) * j;
              gdouble phase = 2.0 * G_PI * frequency0 * time;

              data[j].re = cos (phase);
              data[j].im = sin (phase);
            }

          if (!hyscan_buffer_export (data_buffer, channel_buffer, HYSCAN_DATA_COMPLEX_FLOAT32LE))
            g_error ("can't export channel data");

          status = hyscan_data_writer_acoustic_add_signal (writer, source, channel,
                                                           data_time, channel_buffer);
          if (!status)
            g_error ("can't add signal image");
        }

      /* Записываем параметры ВАРУ каждые n_lines строк. */
      if ((i % n_lines) == 0)
        {
          gfloat *data;
          gboolean status;

          hyscan_buffer_set_float (data_buffer, NULL, n_data_points);
          data = hyscan_buffer_get_float (data_buffer, &n_data_points);

          for (j = 0; j < n_data_points; j++)
            data[j] = tvg0 + j;

          if (!hyscan_buffer_export (data_buffer, channel_buffer, HYSCAN_DATA_FLOAT32LE))
            g_error ("can't export channel data");

          status = hyscan_data_writer_acoustic_add_tvg (writer, source, channel,
                                                        data_time, channel_buffer);
          if (!status)
            g_error ("can't add tvg");
        }

      /* Записываем действительные данные. */
      if (hyscan_discretization_get_type_by_data (type) == HYSCAN_DISCRETIZATION_REAL)
        {
          gfloat *data;
          gboolean status;

          hyscan_buffer_set_float (data_buffer, NULL, n_data_points);
          data = hyscan_buffer_get_float (data_buffer, &n_data_points);

          memset (data, 0, n_data_points * sizeof (gfloat));
          for (j = 2 * n_signal_points; j < 3 * n_signal_points; j++)
            {
              gdouble time = (1.0 / discretization) * (j - 2 * n_signal_points);
              gdouble phase = 2.0 * G_PI * (frequency + frequency0) * time;

              data[j] = cos (phase);
            }

          if (!hyscan_buffer_export (data_buffer, channel_buffer, type))
            g_error ("can't export channel data");

          status = hyscan_data_writer_acoustic_add_data (writer, source, channel, noise,
                                                         data_time, &acoustic_info, channel_buffer);
          if (!status)
            g_error ("can't add channel data");
        }

      /* Записываем комплексные данные. */
      if (hyscan_discretization_get_type_by_data (type) == HYSCAN_DISCRETIZATION_COMPLEX)
        {
          HyScanComplexFloat *data;
          gboolean status;

          hyscan_buffer_set_complex_float (data_buffer, NULL, n_data_points);
          data = hyscan_buffer_get_complex_float (data_buffer, &n_data_points);

          memset (data, 0, n_data_points * sizeof (HyScanComplexFloat));
          for (j = 2 * n_signal_points; j < 3 * n_signal_points; j++)
            {
              gdouble time = (1.0 / discretization) * (j - 2 * n_signal_points);
              gdouble phase = 2.0 * G_PI * frequency0 * time;

              data[j].re = cos (phase);
              data[j].im = sin (phase);
            }

          if (!hyscan_buffer_export (data_buffer, channel_buffer, type))
            g_error ("can't export channel data");

          status = hyscan_data_writer_acoustic_add_data (writer, source, channel, noise,
                                                         data_time, &acoustic_info, channel_buffer);
          if (!status)
            g_error ("can't add channel data");
        }
    }

  g_object_unref (channel_buffer);
  g_object_unref (data_buffer);
}

/* Функция записывает тестовые гидроакустические данные. Записывается
 * n_lines строк, каждая строка имеет размер 100 * n_signal_points,
 * где n_signal_points - размер сигнала равный discretization * duration.
 * В строке, со смещением в два размера сигнала формируется синусоидальная
 * последовательность.
 *
 * Функция формирует амплитудные выборки данных.
 */
void
create_amplitude_data (HyScanDataWriter *writer,
                       HyScanSourceType  source,
                       guint             channel,
                       gboolean          noise,
                       HyScanDataType    type,
                       gdouble           discretization,
                       gdouble           frequency,
                       gdouble           duration,
                       guint             n_lines)
{
  HyScanAcousticDataInfo acoustic_info;
  HyScanAntennaOffset offset;

  HyScanBuffer *data_buffer;
  HyScanBuffer *channel_buffer;
  guint n_signal_points;
  guint n_data_points;
  guint i, j;

  data_buffer = hyscan_buffer_new ();
  channel_buffer = hyscan_buffer_new ();

  /* Параметры данных. */
  acoustic_info.data_type = type;
  acoustic_info.data_rate = discretization;
  acoustic_info.signal_frequency = frequency;
  acoustic_info.signal_bandwidth = frequency;
  acoustic_info.signal_heterodyne = frequency;
  acoustic_info.antenna_voffset = 0.0;
  acoustic_info.antenna_hoffset = 0.0;
  acoustic_info.antenna_vaperture = 40.0;
  acoustic_info.antenna_haperture = 2.0;
  acoustic_info.antenna_frequency = frequency;
  acoustic_info.antenna_bandwidth = 0.1 * frequency;
  acoustic_info.antenna_group = source;
  acoustic_info.adc_vref = 1.0;
  acoustic_info.adc_offset = 0;

  offset.starboard = 0.0;
  offset.forward = 0.0;
  offset.vertical = 0.0;
  offset.yaw = 0.0;
  offset.pitch = 0.0;
  offset.roll = 0.0;

  hyscan_data_writer_sonar_set_offset (writer, source, &offset);

  n_signal_points = discretization * duration;
  n_data_points = 100 * n_signal_points;

  for (i = 0; i < n_lines; i++)
    {
      gfloat *data;
      gint64 data_time;
      gboolean status;

      data_time = 1000 * (i + 1);

      hyscan_buffer_set_float (data_buffer, NULL, n_data_points);
      data = hyscan_buffer_get_float (data_buffer, &n_data_points);

      memset (data, 0, n_data_points * sizeof (gfloat));
      for (j = 2 * n_signal_points; j < 3 * n_signal_points; j++)
        {
          gdouble time = (1.0 / discretization) * (j + i);
          gdouble phase = 2.0 * G_PI * frequency * time;

          data[j] = fabs (sin (phase));
        }

      if (!hyscan_buffer_export (data_buffer, channel_buffer, type))
        g_error ("can't export amplitude data");

      status = hyscan_data_writer_acoustic_add_data (writer, source, channel, noise,
                                                     data_time, &acoustic_info, channel_buffer);
      if (!status)
        g_error ("can't add acoustic data");
    }

  g_object_unref (channel_buffer);
  g_object_unref (data_buffer);
}

/* Функция проверяет гидроакустические данные. При этом данные
 * считываются из системы хранения и сравниваются с эталоном.
 */
void
check_complex_data (HyScanDB         *db,
                    HyScanCache      *cache,
                    HyScanSourceType  source,
                    guint             channel,
                    gboolean          noise,
                    gdouble           discretization,
                    gdouble           frequency,
                    gdouble           duration,
                    guint             n_signals,
                    guint             n_lines,
                    gdouble           error)
{
  HyScanAmplitude *amplitude;
  HyScanAcousticData *reader;
  HyScanAcousticDataInfo info;
  guint n_signal_points;
  guint n_data_points;
  guint i, j;

  reader = hyscan_acoustic_data_new (db, cache, PROJECT_NAME, TRACK_NAME,
                                     source, channel, noise);
  amplitude = HYSCAN_AMPLITUDE (reader);
  info = hyscan_amplitude_get_info (amplitude);

  n_signal_points = discretization * duration;
  n_data_points = 100 * n_signal_points;

  for (i = 0; i < n_lines; i++)
    {
      gdouble df, frequency0, tvg0;

      df = frequency / (10.0 * (n_signals - 1));
      frequency0 = -(df * (n_signals - 1) / 2.0) + (i / n_lines) * df;
      tvg0 = i / n_lines;

      /* Проверяем образ сигнала. */
      {
        const HyScanComplexFloat *data;
        gint64 data_time0;
        gint64 data_time;
        guint data_size;

        data_time0 = 1000 * ((i / n_lines) * n_lines + 1);

        data = hyscan_acoustic_data_get_signal (reader, i, &data_size, &data_time);
        if (data == NULL)
          g_error ("can't get signal");

        if (data_size != n_signal_points)
          g_error ("signal size error");

        if (data_time0 != data_time)
          g_error ("signal time error");

        for (j = 0; j < n_signal_points; j++)
          {
            gdouble time = (1.0 / discretization) * j;
            gdouble phase = 2.0 * G_PI * frequency0 * time;
            gfloat re, im;

            re = cos (phase);
            im = sin (phase);

            if ((data[j].re != re) || (data[j].im != im))
              g_error ("error in signal image");
          }
      }

      /* Проверяем параметры ВАРУ. */
      {
        const gfloat *data;
        gint64 data_time0;
        gint64 data_time;
        guint data_size;

        data_time0 = 1000 * ((i / n_lines) * n_lines + 1);

        data = hyscan_acoustic_data_get_tvg (reader, i, &data_size, &data_time);
        if (data == NULL)
          g_error ("can't get tvg");

        if (data_size != n_data_points)
          g_error ("tvg size error");

        if (data_time0 != data_time)
          g_error ("tvg time error");

        for (j = 0; j < n_data_points; j++)
          {
            if (data[j] != (tvg0 + j))
              g_error ("error in tvg");
          }
      }

      /* Проверяем действительные данные. */
      if (hyscan_discretization_get_type_by_data (info.data_type) == HYSCAN_DISCRETIZATION_REAL)
        {
          const gfloat *data;
          gint64 data_time0;
          gint64 data_time;
          guint data_size;
          gdouble data_diff;

          data_time0 = 1000 * (i + 1);

          data = hyscan_acoustic_data_get_real (reader, i, &data_size, &data_time);
          if (data == NULL)
            g_error ("can't get real data");

          if (data_size != n_data_points)
            g_error ("real data size error");

          if (data_time0 != data_time)
            g_error ("real data time error");

          data_diff = 0.0;
          for (j = 0; j < n_data_points; j++)
            {
              gdouble time = (1.0 / discretization) * (j - 2 * n_signal_points);
              gdouble phase = 2.0 * G_PI * (frequency + frequency0) * time;
              gdouble amplitude = 0.0;

              if ((j >= 2 * n_signal_points) && (j < 3 * n_signal_points))
                amplitude = cos (phase);

              data_diff += fabs (data[j] - amplitude);
            }

          if (data_diff > (error * n_data_points))
            g_error ("error in real data %f %f", data_diff, error * n_data_points);
        }

      /* Проверяем комплексные данные. */
      if (hyscan_discretization_get_type_by_data (info.data_type) == HYSCAN_DISCRETIZATION_COMPLEX)
        {
          const HyScanComplexFloat *data;
          gint64 data_time0;
          gint64 data_time;
          guint data_size;
          gdouble data_diff;

          data_time0 = 1000 * (i + 1);

          data = hyscan_acoustic_data_get_complex (reader, i, &data_size, &data_time);
          if (data == NULL)
            g_error ("can't get complex data");

          if (data_size != n_data_points)
            g_error ("complex data size error");

          if (data_time0 != data_time)
            g_error ("complex data time error");

          data_diff = 0.0;
          for (j = 0; j < n_data_points; j++)
            {
              gdouble amplitude = 0.0;
              gfloat re = data[j].re;
              gfloat im = data[j].im;

              if ((j >= n_signal_points) && (j < 2 * n_signal_points))
                amplitude = ((gdouble) (j - n_signal_points) / n_signal_points);
              if ((j >= 2 * n_signal_points) && (j < 3 * n_signal_points))
                amplitude = (1.0 - (gdouble) (j - 2 * n_signal_points) / n_signal_points);

              data_diff += fabs (sqrtf (re * re + im * im) - amplitude);
            }

          if (data_diff > (error * n_data_points))
            g_error ("error in complex data");
        }

      /* Проверяем амплитудные данные. */
      {
        const gfloat *data;
        gint64 data_time0;
        gint64 data_time;
        guint data_size;
        gdouble data_diff;
        gboolean is_noise;

        data_time0 = 1000 * (i + 1);

        data = hyscan_amplitude_get_amplitude (amplitude, NULL, i, &data_size, &data_time, &is_noise);
        if (data == NULL)
          g_error ("can't get amplitude data");

        if (data_size != n_data_points)
          g_error ("amplitude data size error");

        if (data_time0 != data_time)
          g_error ("amplitude data time error");

        if (is_noise != noise)
          g_error ("amplitude noise error");

        data_diff = 0.0;
        for (j = 0; j < n_data_points; j++)
          {
            gdouble amplitude = 0.0;

            if ((j >= n_signal_points) && (j < 2 * n_signal_points))
              amplitude = ((gdouble) (j - n_signal_points) / n_signal_points);
            if ((j >= 2 * n_signal_points) && (j < 3 * n_signal_points))
              amplitude = (1.0 - (gdouble) (j - 2 * n_signal_points) / n_signal_points);

            data_diff += fabs (data[j] - amplitude);
          }

        if (data_diff > (error * n_data_points))
          g_error ("error in amplitude data %f %f", data_diff, error * n_data_points);
      }
    }

  g_object_unref (reader);
}

/* Функция проверяет амплитудные гидроакустические данные. При этом данные
 * считываются из системы хранения и сравниваются с эталоном.
 */
void
check_amplitude_data (HyScanDB         *db,
                      HyScanCache      *cache,
                      HyScanSourceType  source,
                      guint             channel,
                      gboolean          noise,
                      gdouble           discretization,
                      gdouble           frequency,
                      gdouble           duration,
                      guint             n_lines,
                      gdouble           error)
{
  HyScanAmplitude *amplitude;
  HyScanAcousticData *reader;
  guint n_signal_points;
  guint n_data_points;
  guint i, j;

  reader = hyscan_acoustic_data_new (db, cache, PROJECT_NAME, TRACK_NAME,
                                     source, channel, noise);
  amplitude = HYSCAN_AMPLITUDE (reader);

  n_signal_points = discretization * duration;
  n_data_points = 100 * n_signal_points;

  for (i = 0; i < n_lines; i++)
    {
      const gfloat *data;
      gint64 data_time0;
      gint64 data_time;
      guint data_size;
      gdouble data_diff;
      gboolean is_noise;

      data_time0 = 1000 * (i + 1);

      data = hyscan_amplitude_get_amplitude (amplitude, NULL, i, &data_size, &data_time, &is_noise);
      if (data == NULL)
        g_error ("can't get amplitude data");

      if (data_size != n_data_points)
        g_error ("amplitude data size error");

      if (data_time0 != data_time)
        g_error ("amplitude data time error");

      if (is_noise != noise)
        g_error ("amplitude noise error");

      data_diff = 0.0;
      for (j = 0; j < n_data_points; j++)
        {
          gdouble time = (1.0 / discretization) * (j + i);
          gdouble phase = 2.0 * G_PI * frequency * time;
          gdouble amplitude = 0.0;

          if ((j >= 2 * n_signal_points) && (j < 3 * n_signal_points))
            amplitude = fabs (sin (phase));

          data_diff += fabs (data[j] - amplitude);
        }

      if (data_diff > (error * n_data_points))
        g_error ("amplitude data error %f %f", data_diff, error * n_data_points);
    }

  g_object_unref (reader);
}

int
main (int    argc,
      char **argv)
{
  gchar *db_uri = NULL;
  gchar *real_type_name = g_strdup ("adc16");
  gchar *complex_type_name = g_strdup ("adc16");
  gchar *amplitude_type_name = g_strdup ("int16");
  gdouble frequency = 100000.0;
  gdouble duration = 0.001;
  gdouble discretization = 1000000.0;
  guint n_lines = 100;
  guint n_signals = 10;
  guint cache_size = 0;

  HyScanDB *db;
  HyScanCache *cache;
  HyScanDataWriter *writer;

  HyScanDataType real_type;
  HyScanDataType complex_type;
  HyScanDataType amplitude_type;

  gdouble real_error = 0.0;
  gdouble complex_error = 0.0;
  gdouble amplitude_error = 0.0;

  GTimer *timer;

  guint i;

  {
    gchar **args;
    GError *error = NULL;
    GOptionContext *context;
    GOptionEntry entries[] =
      {
        { "real-type", 'r', 0, G_OPTION_ARG_STRING, &real_type_name, "Real data type (adc14, adc16, adc24, float16, float32)", NULL },
        { "complex-type", 'q', 0, G_OPTION_ARG_STRING, &complex_type_name, "Complex data type (adc14, adc16, adc24, float16, float32)", NULL },
        { "amplitude-type", 'a', 0, G_OPTION_ARG_STRING, &amplitude_type_name, "Amplitude data type (int8, int16, int24, int32, float16, float32)", NULL },
        { "discretization", 'd', 0, G_OPTION_ARG_DOUBLE, &discretization, "Signal discretization, Hz", NULL },
        { "frequency", 'f', 0, G_OPTION_ARG_DOUBLE, &frequency, "Signal frequency, Hz", NULL },
        { "duration", 't', 0, G_OPTION_ARG_DOUBLE, &duration, "Signal duration, s", NULL },
        { "signals", 's', 0, G_OPTION_ARG_INT, &n_signals, "Number of signals (1..100)", NULL },
        { "lines", 'l', 0, G_OPTION_ARG_INT, &n_lines, "Number of lines per signal (1..100)", NULL },
        { "cache", 'c', 0, G_OPTION_ARG_INT, &cache_size, "Use cache with size, Mb", NULL },
        { NULL }
      };

#ifdef G_OS_WIN32
    args = g_win32_get_command_line ();
#else
    args = g_strdupv (argv);
#endif

    context = g_option_context_new ("<db-uri>");
    g_option_context_set_help_enabled (context, TRUE);
    g_option_context_add_main_entries (context, entries, NULL);
    g_option_context_set_ignore_unknown_options (context, FALSE);
    if (!g_option_context_parse_strv (context, &args, &error))
      {
        g_print ("%s\n", error->message);
        return -1;
      }

    if ((g_strv_length (args) != 2))
      {
        g_print ("%s", g_option_context_get_help (context, FALSE, NULL));
        return 0;
      }

    g_option_context_free (context);

    db_uri = g_strdup (args[1]);
    g_strfreev (args);
  }

  /* Входные параметры. */
  if ((discretization < 10.0) || (discretization > 10000000.0))
    g_error ("the discretization must be within 10Hz to 10Mhz");

  if ((frequency < 1.0) || (frequency > 1000000.0))
    g_error ("the signal frequency must be within 1Hz to 1Mhz");

  if ((duration < 1e-4) || (duration > 0.1))
    g_error ("the signal duration must be within 100us to 1ms");

  if ((n_signals < 1) || (n_signals > 100))
    g_error ("the number of signals must be within 1 to 100");

  if ((n_lines < 1) || (n_lines > 100))
    g_error ("the number of lines must be within 1 to 100");

  /* Форматы записи данных. */
  real_type = HYSCAN_DATA_INVALID;
  for (i = 0; i < sizeof (real_test_types) / sizeof (test_info); i++)
    if (g_strcmp0 (real_type_name, real_test_types[i].name) == 0)
      {
        real_type = real_test_types[i].type;
        real_error = real_test_types[i].error;
      }
  if (real_type == HYSCAN_DATA_INVALID)
    g_error ("unsupported real type %s", real_type_name);

  complex_type = HYSCAN_DATA_INVALID;
  for (i = 0; i < sizeof (complex_test_types) / sizeof (test_info); i++)
    if (g_strcmp0 (complex_type_name, complex_test_types[i].name) == 0)
      {
        complex_type = complex_test_types[i].type;
        complex_error = complex_test_types[i].error;
      }
  if (complex_type == HYSCAN_DATA_INVALID)
    g_error ("unsupported complex type %s", complex_type_name);

  amplitude_type = HYSCAN_DATA_INVALID;
  for (i = 0; i < sizeof (amplitude_test_types) / sizeof (test_info); i++)
    if (g_strcmp0 (amplitude_type_name, amplitude_test_types[i].name) == 0)
      {
        amplitude_type = amplitude_test_types[i].type;
        amplitude_error = amplitude_test_types[i].error;
      }
  if (amplitude_type == HYSCAN_DATA_INVALID)
    g_error ("unsupported amplitude type %s", amplitude_type_name);

  /* Открываем базу данных. */
  db = hyscan_db_new (db_uri);
  if (db == NULL)
    g_error ("can't open db at: %s", db_uri);

  /* Кэш данных */
  if (cache_size)
    cache = HYSCAN_CACHE (hyscan_cached_new (cache_size));
  else
    cache = NULL;

  /* Объект записи данных */
  writer = hyscan_data_writer_new ();

  /* Система хранения. */
  hyscan_data_writer_set_db (writer, db);

  /* Создаём галс. */
  if (!hyscan_data_writer_start (writer, PROJECT_NAME, TRACK_NAME, HYSCAN_TRACK_SURVEY, NULL, -1))
    g_error( "can't start write");

  timer = g_timer_new ();

  /* Формируем тестовые данные. */
  g_print ("Creating real data.\n");
  create_complex_data   (writer,
                         HYSCAN_SOURCE_SIDE_SCAN_PORT, 1, FALSE,
                         real_type, discretization, frequency, duration,
                         n_signals, n_lines);

  g_print ("Creating complex data.\n");
  create_complex_data   (writer,
                         HYSCAN_SOURCE_SIDE_SCAN_PORT, 2, FALSE,
                         complex_type, discretization, frequency, duration,
                         n_signals, n_lines);

  g_print ("Creating amplitude data.\n");
  create_amplitude_data (writer,
                         HYSCAN_SOURCE_SIDE_SCAN_PORT, 3, FALSE,
                         amplitude_type, discretization, frequency, duration,
                         n_signals * n_lines);

  g_print ("Creating noise real data.\n");
  create_complex_data   (writer,
                         HYSCAN_SOURCE_SIDE_SCAN_STARBOARD, 1, TRUE,
                         real_type, discretization, frequency, duration,
                         n_signals, n_lines);

  g_print ("Creating noise complex data.\n");
  create_complex_data   (writer,
                         HYSCAN_SOURCE_SIDE_SCAN_STARBOARD, 2, TRUE,
                         complex_type, discretization, frequency, duration,
                         n_signals, n_lines);

  g_print ("Creating noise amplitude data.\n");
  create_amplitude_data (writer,
                         HYSCAN_SOURCE_SIDE_SCAN_STARBOARD, 3, TRUE,
                         amplitude_type, discretization, frequency, duration,
                         n_signals * n_lines);

  g_print ("Checking real data: ");
  g_timer_start (timer);
  check_complex_data (db, cache,
                      HYSCAN_SOURCE_SIDE_SCAN_PORT, 1, FALSE,
                      discretization, frequency, duration,
                      n_signals, n_lines, real_error);
  g_print ("%.04fs elapsed\n", g_timer_elapsed (timer, NULL));

  g_print ("Checking complex data: ");
  g_timer_start (timer);
  check_complex_data (db, cache,
                      HYSCAN_SOURCE_SIDE_SCAN_PORT, 2, FALSE,
                      discretization, frequency, duration,
                      n_signals, n_lines, complex_error);
  g_print ("%.04fs elapsed\n", g_timer_elapsed (timer, NULL));

  g_print ("Checking amplitude data: ");
  g_timer_start (timer);
  check_amplitude_data (db, cache,
                        HYSCAN_SOURCE_SIDE_SCAN_PORT, 3, FALSE,
                        discretization, frequency, duration,
                        n_lines, amplitude_error);
  g_print ("%.04fs elapsed\n", g_timer_elapsed (timer, NULL));

  g_print ("Checking noise real data: ");
  g_timer_start (timer);
  check_complex_data (db, cache,
                      HYSCAN_SOURCE_SIDE_SCAN_STARBOARD, 1, TRUE,
                      discretization, frequency, duration,
                      n_signals, n_lines, real_error);
  g_print ("%.04fs elapsed\n", g_timer_elapsed (timer, NULL));

  g_print ("Checking noise complex data: ");
  g_timer_start (timer);
  check_complex_data (db, cache,
                      HYSCAN_SOURCE_SIDE_SCAN_STARBOARD, 2, TRUE,
                      discretization, frequency, duration,
                      n_signals, n_lines, complex_error);
  g_print ("%.04fs elapsed\n", g_timer_elapsed (timer, NULL));

  g_print ("Checking noise amplitude data: ");
  g_timer_start (timer);
  check_amplitude_data (db, cache,
                        HYSCAN_SOURCE_SIDE_SCAN_STARBOARD, 3, TRUE,
                        discretization, frequency, duration,
                        n_lines, amplitude_error);
  g_print ("%.04fs elapsed\n", g_timer_elapsed (timer, NULL));

  g_print ("Checking cached real data: ");
  g_timer_start (timer);
  check_complex_data (db, cache,
                      HYSCAN_SOURCE_SIDE_SCAN_PORT, 1, FALSE,
                      discretization, frequency, duration,
                      n_signals, n_lines, real_error);
  g_print ("%.04fs elapsed\n", g_timer_elapsed (timer, NULL));

  g_print ("Checking cached complex data: ");
  g_timer_start (timer);
  check_complex_data (db, cache,
                      HYSCAN_SOURCE_SIDE_SCAN_PORT, 2, FALSE,
                      discretization, frequency, duration,
                      n_signals, n_lines, complex_error);
  g_print ("%.04fs elapsed\n", g_timer_elapsed (timer, NULL));

  g_print ("Checking cached amplitude data: ");
  g_timer_start (timer);
  check_amplitude_data (db, cache,
                        HYSCAN_SOURCE_SIDE_SCAN_PORT, 3, FALSE,
                        discretization, frequency, duration,
                        n_lines, amplitude_error);
  g_print ("%.04fs elapsed\n", g_timer_elapsed (timer, NULL));

  g_print ("Checking cached noise real data: ");
  g_timer_start (timer);
  check_complex_data (db, cache,
                      HYSCAN_SOURCE_SIDE_SCAN_STARBOARD, 1, TRUE,
                      discretization, frequency, duration,
                      n_signals, n_lines, real_error);
  g_print ("%.04fs elapsed\n", g_timer_elapsed (timer, NULL));

  g_print ("Checking cached noise complex data: ");
  g_timer_start (timer);
  check_complex_data (db, cache,
                      HYSCAN_SOURCE_SIDE_SCAN_STARBOARD, 2, TRUE,
                      discretization, frequency, duration,
                      n_signals, n_lines, complex_error);
  g_print ("%.04fs elapsed\n", g_timer_elapsed (timer, NULL));

  g_print ("Checking cached noise amplitude data: ");
  g_timer_start (timer);
  check_amplitude_data (db, cache,
                        HYSCAN_SOURCE_SIDE_SCAN_STARBOARD, 3, TRUE,
                        discretization, frequency, duration,
                        n_lines, amplitude_error);
  g_print ("%.04fs elapsed\n", g_timer_elapsed (timer, NULL));

  g_print ("All done.\n");

  hyscan_db_project_remove (db, PROJECT_NAME);

  g_clear_object (&writer);
  g_clear_object (&cache);
  g_clear_object (&db);

  g_timer_destroy (timer);

  g_free (real_type_name);
  g_free (complex_type_name);
  g_free (amplitude_type_name);
  g_free (db_uri);

  return 0;
}
