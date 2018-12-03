/* data-writer-test.c
 *
 * Copyright 2016-2018 Screen LLC, Andrei Fadeev <andrei@webcontrol.ru>
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
 * Contact the Screen LLC in this case - info@screen-co.ru
 */

/* HyScanCore имеет двойную лицензию.
 *
 * Во-первых, вы можете распространять HyScanCore на условиях Стандартной
 * Общественной Лицензии GNU версии 3, либо по любой более поздней версии
 * лицензии (по вашему выбору). Полные положения лицензии GNU приведены в
 * <http://www.gnu.org/licenses/>.
 *
 * Во-вторых, этот программный код можно использовать по коммерческой
 * лицензии. Для этого свяжитесь с ООО Экран - info@screen-co.ru.
 */

#include <hyscan-data-writer.h>
#include <hyscan-core-schemas.h>
#include <hyscan-core-common.h>

#include <libxml/parser.h>
#include <string.h>
#include <math.h>

#define CTIME                  123456
#define OPERATOR_NAME          "tester"
#define SONAR_INFO             "This is sonar info"
#define PROJECT_NAME           "test"

#define N_CHANNELS_PER_TYPE    4
#define N_RECORDS_PER_CHANNEL  100
#define N_LINES_PER_SIGNAL     10
#define N_LINES_PER_TVG        25

#define DATA_SIZE              1024
#define SIGNAL_SIZE            256
#define TVG_SIZE               512

/* Функция возвращает название датчика. */
const gchar *
sensor_get_name (guint n_channel)
{
  switch (n_channel)
    {
    case 1:
      return "sensor-1";
    case 2:
      return "sensor-2";
    case 3:
      return "sensor-3";
    case 4:
      return "sensor-4";
    }

  return NULL;
}

/* Функция возвращает название источника гидролокационных данных. */
const gchar *
sonar_get_name (guint n_channel)
{
  switch (n_channel)
    {
    case 1:
      return "side-scan-starboard";
    case 2:
      return "side-scan-starboard-hi";
    case 3:
      return "side-scan-port";
    case 4:
      return "side-scan-port-hi";
    }

  return NULL;
}

/* Функция возвращает тип источника гидролокационных данных. */
HyScanSourceType
sonar_get_type (guint n_channel)
{
  switch (n_channel)
    {
    case 1:
      return HYSCAN_SOURCE_SIDE_SCAN_STARBOARD;
    case 2:
      return HYSCAN_SOURCE_SIDE_SCAN_STARBOARD_HI;
    case 3:
      return HYSCAN_SOURCE_SIDE_SCAN_PORT;
    case 4:
      return HYSCAN_SOURCE_SIDE_SCAN_PORT_HI;
    }

  return HYSCAN_SOURCE_INVALID;
}

/* Функция возвращает местоположение приёмной антенны. */
HyScanAntennaPosition
antenna_get_position (guint n_channel)
{
  HyScanAntennaPosition position = {0};

  if (n_channel % 2)
    {
      position.x = 1.0 * n_channel;
      position.y = 2.0 * n_channel;
      position.z = 3.0 * n_channel;
      position.psi = 4.0 * n_channel;
      position.gamma = 5.0 * n_channel;
      position.theta = 6.0 * n_channel;
    }

  return position;
}

/* Функция возвращает информацию о гидроакустических данных. */
HyScanAcousticDataInfo
acoustic_get_info (guint n_channel)
{
  HyScanAcousticDataInfo info;

  info.data_type = HYSCAN_DATA_ADC14LE + (n_channel % 2);
  info.data_rate = 1000.0 * n_channel;

  info.signal_frequency = 1.0 * n_channel;
  info.signal_bandwidth = 2.0 * n_channel;

  info.antenna_voffset = 3.0 * n_channel;
  info.antenna_hoffset = 4.0 * n_channel;
  info.antenna_vpattern = 5.0 * n_channel;
  info.antenna_hpattern = 6.0 * n_channel;
  info.antenna_frequency = 7.0 * n_channel;
  info.antenna_bandwidth = 8.0 * n_channel;

  info.adc_vref = 9.0 * n_channel;
  info.adc_offset = 10 * n_channel;

  return info;
}


/* Функция проверяет параметры проекта. */
void project_check_info (HyScanDB *db,
                         gint32    project_id,
                         guint32  n_tracks)
{
  HyScanParamList *param_list = NULL;
  gint32 param_id;

  gint64 ctime;
  gint64 mtime;

  param_list = hyscan_param_list_new ();

  param_id = hyscan_db_project_param_open (db, project_id, PROJECT_INFO_GROUP);
  if (param_id < 0)
    g_error ("can't open project parameters");

  hyscan_param_list_add (param_list, "/ctime");
  hyscan_param_list_add (param_list, "/mtime");

  if (!hyscan_db_param_get (db, param_id, PROJECT_INFO_OBJECT, param_list))
    g_error ("can't read project parameters");

  ctime = hyscan_param_list_get_integer (param_list, "/ctime");
  mtime = hyscan_param_list_get_integer (param_list, "/mtime");

  if (ctime != CTIME)
    g_error ("project ctime error");

  if (mtime != CTIME + n_tracks)
    g_error ("project mtime error");

  hyscan_db_close (db, param_id);
  g_object_unref (param_list);
}

/* Функция проверяет параметры галса. */
void track_check_info (HyScanDB *db,
                       gint32    track_id,
                       guint     n_track)
{
  HyScanParamList *param_list = NULL;
  gint32 param_id;

  gint64 ctime;
  const gchar *track_type;
  const gchar *operator_name;
  const gchar *sonar_info;

  param_list = hyscan_param_list_new ();

  param_id = hyscan_db_track_param_open (db, track_id);
  if (param_id < 0)
    g_error ("can't open track parameters");

  hyscan_param_list_add (param_list, "/ctime");
  hyscan_param_list_add (param_list, "/type");
  hyscan_param_list_add (param_list, "/operator");
  hyscan_param_list_add (param_list, "/sonar");

  if (!hyscan_db_param_get (db, param_id, NULL, param_list))
    g_error ("can't read track parameters");

  ctime = hyscan_param_list_get_integer (param_list, "/ctime");
  track_type = hyscan_param_list_get_string (param_list, "/type");
  operator_name = hyscan_param_list_get_string (param_list, "/operator");
  sonar_info = hyscan_param_list_get_string (param_list, "/sonar");

  if (ctime != (CTIME + n_track))
    g_error ("track ctime error");

  if (g_strcmp0 (track_type, hyscan_track_get_name_by_type (HYSCAN_TRACK_SURVEY)) != 0)
    g_error ("track type error");

  if (g_strcmp0 (operator_name, OPERATOR_NAME) != 0)
    g_error ("operator name error");

  if (g_strcmp0 (sonar_info, SONAR_INFO) != 0)
    g_error ("sonar info error");

  hyscan_db_close (db, param_id);
  g_object_unref (param_list);
}

/* Функция проверяет параметры местоположения приёмной антенны. */
void
antenna_check_position (HyScanDB *db,
                        gint32    channel_id,
                        gint64    schema_id,
                        guint     n_channel)
{
  gint32 param_id;

  HyScanAntennaPosition position1;
  HyScanAntennaPosition position2;

  param_id = hyscan_db_channel_param_open (db, channel_id);
  if (param_id < 0)
    g_error ("can't open parameters");

  position1 = antenna_get_position (n_channel);

  if (!hyscan_core_params_load_antenna_position (db, param_id, schema_id, TRACK_SCHEMA_VERSION, &position2))
    g_error ("error in schema");

  if ((fabs (position1.x - position2.x) > 1e-6) ||
      (fabs (position1.y - position2.y) > 1e-6) ||
      (fabs (position1.z - position2.z) > 1e-6) ||
      (fabs (position1.psi - position2.psi) > 1e-6) ||
      (fabs (position1.gamma - position2.gamma) > 1e-6) ||
      (fabs (position1.theta - position2.theta) > 1e-6))
    {
      g_error ("error in parameters");
    }

  hyscan_db_close (db, param_id);
}

/* Функция проверяет информацию о гидроакустических данных. */
void
acoustic_check_info (HyScanDB *db,
                     gint32    channel_id,
                     guint     n_channel)
{
  gint32 param_id;

  HyScanAcousticDataInfo info1;
  HyScanAcousticDataInfo info2;

  param_id = hyscan_db_channel_param_open (db, channel_id);
  if (param_id < 0)
    g_error ("can't open parameters");

  info1 = acoustic_get_info (n_channel);

  if (!hyscan_core_params_load_acoustic_data_info (db, param_id, &info2))
    g_error ("can't read parameters");

  if ((info1.data_type != info2.data_type) ||
      (fabs (info1.data_rate - info2.data_rate) > 1e-6) ||
      (fabs (info1.signal_frequency - info2.signal_frequency) > 1e-6) ||
      (fabs (info1.signal_bandwidth - info2.signal_bandwidth) > 1e-6) ||
      (fabs (info1.antenna_voffset - info2.antenna_voffset) > 1e-6) ||
      (fabs (info1.antenna_hoffset - info2.antenna_hoffset) > 1e-6) ||
      (fabs (info1.antenna_vpattern - info2.antenna_vpattern) > 1e-6) ||
      (fabs (info1.antenna_hpattern - info2.antenna_hpattern) > 1e-6) ||
      (fabs (info1.antenna_frequency - info2.antenna_frequency) > 1e-6) ||
      (fabs (info1.antenna_bandwidth - info2.antenna_bandwidth) > 1e-6) ||
      (fabs (info1.adc_vref - info2.adc_vref) > 1e-6) ||
      (info1.adc_offset != info2.adc_offset))
    {
      g_error ("error in parameters");
    }

  hyscan_db_close (db, param_id);
}

/* Функция проверяет параметры канала сигналов. */
void
signal_check_info (HyScanDB *db,
                   gint32    channel_id,
                   guint     n_channel)
{
  gint32 param_id;

  param_id = hyscan_db_channel_param_open (db, channel_id);
  if (param_id < 0)
    g_error ("can't open parameters");

  if (!hyscan_core_params_check_signal_info (db, param_id, 1000.0 * n_channel))
    g_error ("error in parameters");

  hyscan_db_close (db, param_id);
}


/* Функция проверяет параметры канала ВАРУ. */
void
tvg_check_info (HyScanDB *db,
                gint32    channel_id,
                guint     n_channel)
{
  gint32 param_id;

  param_id = hyscan_db_channel_param_open (db, channel_id);
  if (param_id < 0)
    g_error ("can't open parameters");

  if (!hyscan_core_params_check_tvg_info (db, param_id, 1000.0 * n_channel))
    g_error ("error in parameters");

  hyscan_db_close (db, param_id);
}

/* Функция записывает данные датчиков. */
void
sensor_add_data (HyScanDataWriter *writer,
                 gint64            timestamp,
                 guint             n_channel,
                 gboolean          fail)
{
  HyScanBuffer *buffer = hyscan_buffer_new ();

  const gchar *sensor;
  guint i;

  sensor = sensor_get_name (n_channel);

  for (i = 0; i < N_RECORDS_PER_CHANNEL; i++)
    {
      gchar data[128];
      gboolean status;

      g_snprintf (data, sizeof (data), "sensor-%d data %d", n_channel, i);
      hyscan_buffer_wrap_data (buffer, HYSCAN_DATA_BLOB, data, strlen (data));

      status = hyscan_data_writer_sensor_add_data (writer, sensor,
                                                   HYSCAN_SOURCE_NMEA_ANY, n_channel,
                                                   timestamp + i, buffer);
      if (status != fail)
        g_error ("error adding data to '%s'", sensor);
    }

  g_object_unref (buffer);
}

/* Функция записывает гидролокационные данные. */
void
sonar_add_data (HyScanDataWriter *writer,
                gint64            timestamp,
                guint             channel,
                gboolean          fail)
{
  HyScanSourceType source;
  HyScanAcousticDataInfo acoustic_info;

  HyScanBuffer *data_buffer;
  HyScanBuffer *signal_buffer;
  HyScanBuffer *tvg_buffer;

  guint16 *data_values;
  HyScanComplexFloat *signal_points;
  gfloat *tvg_gains;

  guint i, j;

  data_buffer = hyscan_buffer_new ();
  signal_buffer = hyscan_buffer_new ();
  tvg_buffer = hyscan_buffer_new ();

  acoustic_info = acoustic_get_info (channel);

  data_values = g_new (guint16, DATA_SIZE);
  hyscan_buffer_wrap_data (data_buffer, acoustic_info.data_type,
                           data_values, DATA_SIZE * sizeof (guint16));

  signal_points = g_new (HyScanComplexFloat, SIGNAL_SIZE);
  hyscan_buffer_wrap_data (signal_buffer, HYSCAN_DATA_COMPLEX_FLOAT32LE,
                           signal_points, SIGNAL_SIZE * sizeof (HyScanComplexFloat));

  tvg_gains = g_new (gfloat, TVG_SIZE);
  hyscan_buffer_wrap_data (tvg_buffer, HYSCAN_DATA_FLOAT32LE,
                           tvg_gains, TVG_SIZE * sizeof (gfloat));

  source = sonar_get_type (channel);

  for (i = 0; i < N_RECORDS_PER_CHANNEL; i++)
    {
      gboolean status;

      /* Образ сигнала. */
      if ((i % N_LINES_PER_SIGNAL) == 0)
        {
          for (j = 0; j < SIGNAL_SIZE; j++)
            {
              signal_points[j].re = channel + i + j;
              signal_points[j].im = -signal_points[j].re;
            }

          status = hyscan_data_writer_acoustic_add_signal (writer, source, channel,
                                                           timestamp + i, signal_buffer);
          if (status != fail)
            {
              g_error ("error adding signal to '%s-%d'",
                       hyscan_source_get_name_by_type (source), channel);
            }
        }

      /* Параметры ВАРУ. */
      if ((i % N_LINES_PER_TVG) == 0)
        {
          for (j = 0; j < TVG_SIZE; j++)
            tvg_gains[j] = (channel + i + j);

          status = hyscan_data_writer_acoustic_add_tvg (writer, source, channel,
                                                        timestamp + i, tvg_buffer);
          if (status != fail)
            {
              g_error ("error adding tvg to '%s-%d'",
                       hyscan_source_get_name_by_type (source), channel);
            }
        }

      /* Данные. */
      for (j = 0; j < DATA_SIZE; j++)
        data_values[j] = channel + i + j;

      status = hyscan_data_writer_acoustic_add_data (writer, source, channel, FALSE,
                                                     timestamp + i, &acoustic_info, data_buffer);
      if (status != fail)
        {
          g_error ("error adding data to '%s-%d'",
                   hyscan_source_get_name_by_type (source), channel);
        }

      status = hyscan_data_writer_acoustic_add_data (writer, source, channel, TRUE,
                                                     timestamp + i, &acoustic_info, data_buffer);
      if (status != fail)
        {
          g_error ("error adding noise to '%s-%d'",
                   hyscan_source_get_name_by_type (source), channel);
        }
    }

  g_object_unref (data_buffer);
  g_object_unref (tvg_buffer);
  g_object_unref (signal_buffer);

  g_free (data_values);
  g_free (signal_points);
  g_free (tvg_gains);
}

/* Функция записывает информационные сообщения. */
void
log_add_data (HyScanDataWriter *writer,
              gboolean          fail)
{
  guint32 i;

  for (i = 0; i < N_RECORDS_PER_CHANNEL; i++)
    {
      gchar message[128];
      gboolean status;

      g_snprintf (message, sizeof (message), "test log message for time %d", i);

      status = hyscan_data_writer_log_add_message (writer, "test", i,
                                                   HYSCAN_LOG_LEVEL_INFO, message);
      if (status != fail)
        g_error ("error adding log message");
    }
}

/* Функция проверяет данные датчиков. */
void
sensor_check_data (HyScanDB    *db,
                   const gchar *track_name,
                   gint64       timestamp,
                   guint        n_channel)
{
  gint32 project_id;
  gint32 track_id;
  gint32 channel_id;

  const gchar *channel_name;

  HyScanBuffer *buffer;
  gint64 time;

  guint i;

  buffer = hyscan_buffer_new ();

  channel_name = hyscan_channel_get_name_by_types (HYSCAN_SOURCE_NMEA_ANY, HYSCAN_CHANNEL_DATA, n_channel);

  g_message ("checking '%s.%s.%s'", PROJECT_NAME, track_name, channel_name);

  project_id = hyscan_db_project_open (db, PROJECT_NAME);
  if (project_id < 0)
    g_error ("can't open project");

  track_id = hyscan_db_track_open (db, project_id, track_name);
  if (track_id < 0)
    g_error ("can't open track");

  channel_id = hyscan_db_channel_open (db, track_id, channel_name);
  if (channel_id < 0)
    g_error ("can't open channel");

  /* Проверка параметров. */
  antenna_check_position (db, channel_id, SENSOR_CHANNEL_SCHEMA_ID, n_channel);

  /* Проверка данных. */
  for (i = 0; i < N_RECORDS_PER_CHANNEL; i++)
    {
      gchar *data_orig = g_strdup_printf ("sensor-%d data %d", n_channel, i);
      const  gchar *data;
      guint32 data_size;

      if (!hyscan_db_channel_get_data (db, channel_id, i, buffer, &time))
        g_error ("can't read data from channel");

      if (time != timestamp + i)
        g_error ("time stamp mismatch");

      data = hyscan_buffer_get_data (buffer, &data_size);
      if (data_size != strlen (data_orig))
        g_error ("data size mismatch");

      if (strncmp (data_orig, data, strlen (data_orig)) != 0)
        g_error ("data content mismatch ('%s', '%s')", data_orig, data);

      g_free (data_orig);
    }

  hyscan_db_close (db, channel_id);
  hyscan_db_close (db, track_id);
  hyscan_db_close (db, project_id);

  g_object_unref (buffer);
}

/* Функция проверяет гидролокационные данные. */
void
sonar_check_data (HyScanDB    *db,
                  const gchar *track_name,
                  gint64       timestamp,
                  guint        channel)
{
  gint32 project_id;
  gint32 track_id;

  gint32 data_channel_id;
  gint32 noise_channel_id;
  gint32 signal_channel_id;
  gint32 tvg_channel_id;

  HyScanSourceType source;
  const gchar *data_channel_name = NULL;
  const gchar *noise_channel_name = NULL;
  const gchar *signal_channel_name = NULL;
  const gchar *tvg_channel_name = NULL;

  HyScanBuffer *data_buffer;
  HyScanBuffer *signal_buffer;
  HyScanBuffer *tvg_buffer;

  guint n_track;
  guint i, j, k;

  data_buffer = hyscan_buffer_new ();
  tvg_buffer = hyscan_buffer_new ();
  signal_buffer = hyscan_buffer_new ();

  source = sonar_get_type (channel);
  data_channel_name = hyscan_channel_get_name_by_types (source, HYSCAN_CHANNEL_DATA, channel);
  noise_channel_name = hyscan_channel_get_name_by_types (source, HYSCAN_CHANNEL_NOISE, channel);
  signal_channel_name = hyscan_channel_get_name_by_types (source, HYSCAN_CHANNEL_SIGNAL, channel);
  tvg_channel_name = hyscan_channel_get_name_by_types (source, HYSCAN_CHANNEL_TVG, channel);

  g_message ("checking '%s.%s.%s'", PROJECT_NAME, track_name, data_channel_name);

  project_id = hyscan_db_project_open (db, PROJECT_NAME);
  if (project_id < 0)
    g_error ("can't open project");

  track_id = hyscan_db_track_open (db, project_id, track_name);
  if (track_id < 0)
    g_error ("can't open track");

  /* Проверка параметров галса. */
  n_track = g_ascii_strtoull (track_name + sizeof ("track-") - 1, NULL, 10);
  track_check_info (db, track_id, n_track);

  /* Проверка канала данных. */
  data_channel_id = hyscan_db_channel_open (db, track_id, data_channel_name);
  noise_channel_id = hyscan_db_channel_open (db, track_id, noise_channel_name);
  signal_channel_id = hyscan_db_channel_open (db, track_id, signal_channel_name);
  tvg_channel_id = hyscan_db_channel_open (db, track_id, tvg_channel_name);
  if ((data_channel_id < 0) || (noise_channel_id < 0) ||
      (signal_channel_id < 0) || (tvg_channel_id < 0))
    {
      g_error ("can't open channel");
    }

  /* Проверка параметров. */
  antenna_check_position (db, data_channel_id, ACOUSTIC_CHANNEL_SCHEMA_ID, channel);
  acoustic_check_info (db, data_channel_id, channel);
  signal_check_info (db, signal_channel_id, channel);
  tvg_check_info (db, tvg_channel_id, channel);

  /* Проверка данных. */
  for (i = 0; i < N_RECORDS_PER_CHANNEL; i++)
    {
      const guint16 *data_values;
      guint32 data_size;
      gint64 time;

      for (j = 0; j <= 1; j++)
        {
          gint32 channel_id = (j == 0) ? data_channel_id : noise_channel_id;

          if (!hyscan_db_channel_get_data (db, channel_id, i, data_buffer, &time))
            g_error ("can't read data from channel");

          data_values = hyscan_buffer_get_data (data_buffer, &data_size);

          if (time != timestamp + i)
            g_error ("time stamp mismatch");

          if (data_size != DATA_SIZE * sizeof (guint16))
            g_error ("data size mismatch");

          for (k = 0; k < DATA_SIZE; k++)
            if (data_values[k] != channel + i + k)
              g_error ("data content mismatch");
        }
    }

  /* Проверка образов сигналов. */
  for (i = 0; i < N_RECORDS_PER_CHANNEL; i++)
    {
      const HyScanComplexFloat *signal_points;
      guint32 signal_size;
      gint64 time;

      if ((i % N_LINES_PER_SIGNAL) != 0)
        continue;

      if (!hyscan_db_channel_get_data (db, signal_channel_id, i / N_LINES_PER_SIGNAL, signal_buffer, &time))
        g_error ("can't read data from signal channel");

      signal_points = hyscan_buffer_get_data (signal_buffer, &signal_size);

      if (time != timestamp + i)
        g_error ("signal time stamp mismatch");

      if (signal_size != SIGNAL_SIZE * sizeof (HyScanComplexFloat))
        g_error ("signal size mismatch");

      for (j = 0; j < SIGNAL_SIZE; j++)
        if ((signal_points[j].re != channel + i + j) ||
            (signal_points[j].im != -signal_points[j].re))
          g_error ("signal content mismatch");
    }

  /* Проверка данных ВАРУ. */
  for (i = 0; i < N_RECORDS_PER_CHANNEL; i++)
    {
      const float *tvg_gains;
      guint32 tvg_size;
      gint64 time;

      if ((i % N_LINES_PER_TVG) != 0)
        continue;

      if (!hyscan_db_channel_get_data (db, tvg_channel_id, i / N_LINES_PER_TVG, tvg_buffer, &time))
        g_error ("can't read data from tvg channel");

      tvg_gains = hyscan_buffer_get_data (tvg_buffer, &tvg_size);

      if (time != timestamp + i)
        g_error ("tvg time stamp mismatch");

      if (tvg_size != TVG_SIZE * sizeof (gfloat))
        g_error ("tvg size mismatch");

      for (j = 0; j < TVG_SIZE; j++)
        if (tvg_gains[j] != (channel + i + j))
          g_error ("tvg content mismatch");
    }

  hyscan_db_close (db, data_channel_id);
  hyscan_db_close (db, noise_channel_id);
  hyscan_db_close (db, signal_channel_id);
  hyscan_db_close (db, tvg_channel_id);
  hyscan_db_close (db, track_id);
  hyscan_db_close (db, project_id);

  g_object_unref (data_buffer);
  g_object_unref (signal_buffer);
  g_object_unref (tvg_buffer);
}

/* Функция проверяет информационные сообщения. */
void
log_check_data (HyScanDB    *db,
                const gchar *track_name)
{
  gint32 project_id;
  gint32 track_id;
  gint32 channel_id;
  gint32 param_id;

  const gchar *channel_name;
  HyScanBuffer *buffer;
  guint i;

  channel_name = hyscan_source_get_name_by_type (HYSCAN_SOURCE_LOG);
  buffer = hyscan_buffer_new ();

  g_message ("checking '%s.%s.%s'", PROJECT_NAME, track_name, channel_name);

  project_id = hyscan_db_project_open (db, PROJECT_NAME);
  if (project_id < 0)
    g_error ("can't open project");

  track_id = hyscan_db_track_open (db, project_id, track_name);
  if (track_id < 0)
    g_error ("can't open track");

  channel_id = hyscan_db_channel_open (db, track_id, channel_name);
  if (channel_id < 0)
    g_error ("can't open channel");

  param_id = hyscan_db_channel_param_open (db, channel_id);
  if (param_id < 0)
    g_error ("can't open parameters");

  if (!hyscan_core_params_check_log_schema (db, param_id))
    g_error ("schema error");

  /* Проверка данных. */
  for (i = 0; i < N_RECORDS_PER_CHANNEL; i++)
    {
      const  gchar *log;
      gchar *log_orig;
      guint32 log_size;
      gint64 time;

      log_orig = g_strdup_printf ("test\t%s\ttest log message for time %d",
                                  hyscan_log_level_get_name_by_type (HYSCAN_LOG_LEVEL_INFO), i);

      if (!hyscan_db_channel_get_data (db, channel_id, i, buffer, &time))
        g_error ("can't read data from channel %d", i);

      if (time != i)
        g_error ("time stamp mismatch");

      log = hyscan_buffer_get_data (buffer, &log_size);
      if (g_strcmp0 (log_orig, log) != 0)
        g_error ("log content mismatch ('%s', '%s')", log_orig, log);

      g_free (log_orig);
    }

  hyscan_db_close (db, param_id);
  hyscan_db_close (db, channel_id);
  hyscan_db_close (db, track_id);
  hyscan_db_close (db, project_id);

  g_object_unref (buffer);
}

int
main (int    argc,
      char **argv)
{
  HyScanDB *db;
  HyScanDataWriter *writer;

  gint64 date_time = 0;
  guint n_tracks = 0;

  gint32 project_id;
  gint32 track_id;
  guint i;

  /* Путь к базе данных. */
  if (argv[1] == NULL)
    {
      g_print ("Usage: data-writer-test <db-uri>\n");
      return -1;
    }

  /* Открываем базу данных. */
  db = hyscan_db_new (argv[1]);
  if (db == NULL)
    g_error ("can't open db at: %s", argv[1]);

  /* Объект записи данных. */
  writer = hyscan_data_writer_new ();

  /* Система хранения. */
  hyscan_data_writer_set_db (writer, db);

  /* Имя оператора. */
  hyscan_data_writer_set_operator_name (writer, OPERATOR_NAME);

  /* Информация о гидролокаторе. */
  hyscan_data_writer_set_sonar_info (writer, SONAR_INFO);

  /* Местоположение антенн "датчиков" и "гидролокатора". */
  for (i = 1; i <= N_CHANNELS_PER_TYPE; i++)
    {
      HyScanAntennaPosition position = antenna_get_position (i);

      if (i % 2)
        {
          hyscan_data_writer_sensor_set_position (writer, sensor_get_name (i), &position);
          hyscan_data_writer_sonar_set_position (writer, sonar_get_type (i), &position);
        }
      else
        {
          hyscan_data_writer_sensor_set_position (writer, sensor_get_name (i), NULL);
          hyscan_data_writer_sonar_set_position (writer, sonar_get_type (i), NULL);
        }
    }

  /* Пустой галс. */
  g_message ("creating empty track-0");

  /* Попытка записи данных при выключенной записи. */
  for (i = 1; i <= N_CHANNELS_PER_TYPE; i++)
    {
      sensor_add_data (writer, 0, i, FALSE);
      sonar_add_data  (writer, 0, i, FALSE);
    }
  log_add_data (writer, FALSE);

  date_time = CTIME + n_tracks++;
  if (!hyscan_data_writer_start (writer, PROJECT_NAME, "track-0", HYSCAN_TRACK_SURVEY, date_time))
    g_error ("can't start writer");
  hyscan_data_writer_stop (writer);

  /* Галс с данными. */
  g_message ("creating data track-1");
  date_time = CTIME + n_tracks++;
  if (!hyscan_data_writer_start (writer, PROJECT_NAME, "track-1", HYSCAN_TRACK_SURVEY, date_time))
    g_error ("can't start writer");

  /* Запись данных. */
  for (i = 1; i <= N_CHANNELS_PER_TYPE; i++)
    {
      sensor_add_data (writer, 1000, i, TRUE);
      sonar_add_data  (writer, 1000, i, TRUE);
    }
  log_add_data (writer, TRUE);

  hyscan_data_writer_stop (writer);

  /* Галс с данными. */
  g_message ("creating data track-2");
  date_time = CTIME + n_tracks++;
  if (!hyscan_data_writer_start (writer, PROJECT_NAME, "track-2", HYSCAN_TRACK_SURVEY, date_time))
    g_error ("can't start writer");

  /* Запись данных. */
  for (i = 1; i <= N_CHANNELS_PER_TYPE; i++)
    {
      sensor_add_data (writer, 2000, i, TRUE);
      sonar_add_data  (writer, 2000, i, TRUE);
    }
  log_add_data (writer, TRUE);

  hyscan_data_writer_stop (writer);

  /* Дублирование галса. */
  g_message ("duplicate track-0");
  if (hyscan_data_writer_start (writer, PROJECT_NAME, "track-0", HYSCAN_TRACK_SURVEY, -1))
    g_error ("can duplicate track");

  /* Отключаем запись. */
  hyscan_data_writer_stop (writer);

  /* Проверяем записанные данные. */
  project_id = hyscan_db_project_open (db, PROJECT_NAME);
  if (project_id < 0)
    g_error ("can't open project '%s'", PROJECT_NAME);

  project_check_info (db, project_id, n_tracks - 1);

  /* Пустой галс. */
  track_id = hyscan_db_track_open (db, project_id, "track-0");
  if (track_id < 0)
    g_error ("can't open track 'track-0'");

  if (hyscan_db_channel_list (db, track_id) != NULL)
    g_error ("track-0 isn't empty");
  hyscan_db_close (db, track_id);

  /* Первый галс. */
  for (i = 1; i <= N_CHANNELS_PER_TYPE; i++)
    {
      sensor_check_data (db, "track-1", 1000, i);
      sonar_check_data  (db, "track-1", 1000, i);
    }
  log_check_data (db, "track-1");

  /* Второй галс. */
  for (i = 1; i <= N_CHANNELS_PER_TYPE; i++)
    {
      sensor_check_data (db, "track-2", 2000, i);
      sonar_check_data  (db, "track-2", 2000, i);
    }
  log_check_data (db, "track-2");

  /* Удаляем проект. */
  hyscan_db_close (db, project_id);
  hyscan_db_project_remove (db, PROJECT_NAME);

  /* Очищаем память. */
  g_object_unref (writer);
  g_object_unref (db);

  xmlCleanupParser ();

  return 0;
}
