
#include <hyscan-data-writer.h>
#include <hyscan-core-params.h>
#include <hyscan-core-schemas.h>

#include <libxml/parser.h>
#include <string.h>
#include <math.h>

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
      position.x =     10.0 * n_channel + 0.1;
      position.y =     10.0 * n_channel + 0.2;
      position.z =     10.0 * n_channel + 0.3;
      position.psi =   10.0 * n_channel + 0.4;
      position.gamma = 10.0 * n_channel + 0.5;
      position.theta = 10.0 * n_channel + 0.6;
    }

  return position;
}

/* Функция возвращает информацию о "сырых" данных. */
HyScanRawDataInfo
raw_get_info (guint n_channel)
{
  HyScanRawDataInfo info;

  info.data_type = HYSCAN_DATA_ADC_14LE + (n_channel % 2);
  info.data_rate = 1000.0 * n_channel;

  info.antenna_voffset = 0.1 * n_channel;
  info.antenna_hoffset = 0.2 * n_channel;
  info.antenna_vpattern = 0.3 * n_channel;
  info.antenna_hpattern = 0.4 * n_channel;
  info.antenna_frequency = 0.5 * n_channel;
  info.antenna_bandwidth = 0.6 * n_channel;

  info.adc_vref = 1.0 * n_channel;
  info.adc_offset = 10 * n_channel;

  return info;
}

/* Функция возвращает информацию об акустических данных. */
HyScanAcousticDataInfo
acoustic_get_info (guint n_channel)
{
  HyScanAcousticDataInfo info;

  info.data_type = HYSCAN_DATA_ADC_14LE + (n_channel % 2);
  info.data_rate = 1000.0 * n_channel;

  info.antenna_vpattern = 0.1 * n_channel;
  info.antenna_hpattern = 0.2 * n_channel;

  return info;
}

/* Функция проверяет параметры галса. */
void track_check_info (HyScanDB *db,
                       gint32    track_id)
{
  HyScanParamList *param_list = NULL;
  gint32 param_id;

  const gchar *track_type;
  const gchar *operator_name;
  const gchar *sonar_info;

  param_list = hyscan_param_list_new ();

  param_id = hyscan_db_track_param_open (db, track_id);
  if (param_id < 0)
    g_error ("can't open track parameters");

  hyscan_param_list_add (param_list, "/type");
  hyscan_param_list_add (param_list, "/operator");
  hyscan_param_list_add (param_list, "/sonar");

  if (!hyscan_db_param_get (db, param_id, NULL, param_list))
    g_error ("can't read parameters");

  track_type = hyscan_param_list_get_string (param_list, "/type");
  operator_name = hyscan_param_list_get_string (param_list, "/operator");
  sonar_info = hyscan_param_list_get_string (param_list, "/sonar");

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

/* Функция проверяет информацию о "сырых" данных. */
void
raw_check_info (HyScanDB *db,
                gint32    channel_id,
                guint     n_channel)
{
  gint32 param_id;

  HyScanRawDataInfo info1;
  HyScanRawDataInfo info2;

  param_id = hyscan_db_channel_param_open (db, channel_id);
  if (param_id < 0)
    g_error ("can't open parameters");

  info1 = raw_get_info (n_channel);

  if (!hyscan_core_params_load_raw_data_info (db, param_id, &info2))
    g_error ("can't read parameters");


  if ((info1.data_type != info2.data_type) ||
      (fabs (info1.data_rate - info2.data_rate) > 1e-6) ||
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

/* Функция проверяет информацию об акустических данных. */
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
      (fabs (info1.antenna_vpattern - info2.antenna_vpattern) > 1e-6) ||
      (fabs (info1.antenna_hpattern - info2.antenna_hpattern) > 1e-6))
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
      gchar *data = g_strdup_printf ("sensor-%d data %d", n_channel, i);
      gint64 time = timestamp + i;

      hyscan_buffer_wrap_data (buffer, HYSCAN_DATA_BLOB, data, strlen (data));
      if (!hyscan_data_writer_sensor_add_data (writer, sensor, HYSCAN_SOURCE_NMEA_ANY, n_channel, time, buffer) && !fail)
        g_error ("can't add data to '%s'", sensor);

      g_free (data);
    }

  g_object_unref (buffer);
}

/* Функция записывает гидролокационные данные. */
void
sonar_add_data (HyScanDataWriter *writer,
                gint64            timestamp,
                guint             n_channel,
                gboolean          raw)
{
  HyScanBuffer *data_buffer;
  HyScanBuffer *signal_buffer;
  HyScanBuffer *tvg_buffer;

  HyScanRawDataInfo raw_info;
  HyScanAcousticDataInfo acoustic_info;

  HyScanSourceType source;
  guint16 *data_values;
  HyScanComplexFloat *signal_points;
  gfloat *tvg_gains;

  guint i, j;

  data_buffer = hyscan_buffer_new ();
  signal_buffer = hyscan_buffer_new ();
  tvg_buffer = hyscan_buffer_new ();

  raw_info = raw_get_info (n_channel);
  acoustic_info = acoustic_get_info (n_channel);

  data_values = g_new (guint16, DATA_SIZE);
  hyscan_buffer_wrap_data (data_buffer, HYSCAN_DATA_BLOB,
                           data_values, DATA_SIZE * sizeof (guint16));

  signal_points = g_new (HyScanComplexFloat, SIGNAL_SIZE);
  hyscan_buffer_wrap_data (signal_buffer, HYSCAN_DATA_COMPLEX_FLOAT,
                           signal_points, SIGNAL_SIZE * sizeof (HyScanComplexFloat));

  tvg_gains = g_new (gfloat, TVG_SIZE);
  hyscan_buffer_wrap_data (tvg_buffer, HYSCAN_DATA_FLOAT,
                           tvg_gains, TVG_SIZE * sizeof (gfloat));

  source = sonar_get_type (n_channel);

  for (i = 0; i < N_RECORDS_PER_CHANNEL; i++)
    {
      if (raw)
        {
          /* Образ сигнала. */
          if ((i % N_LINES_PER_SIGNAL) == 0)
            {
              for (j = 0; j < SIGNAL_SIZE; j++)
                {
                  signal_points[j].re = n_channel + i + j;
                  signal_points[j].im = -signal_points[j].re;
                }

              hyscan_data_writer_raw_add_signal (writer, source, timestamp + i, signal_buffer);
            }

          /* Параметры ВАРУ. */
          if ((i % N_LINES_PER_TVG) == 0)
            {
              for (j = 0; j < TVG_SIZE; j++)
                tvg_gains[j] = (n_channel + i + j);

              hyscan_data_writer_raw_add_tvg (writer, source, 1, timestamp + i, tvg_buffer);
            }
        }

      /* Данные. */
      for (j = 0; j < DATA_SIZE; j++)
        data_values[j] = n_channel + i + j;

      if (raw)
        {
          if (!hyscan_data_writer_raw_add_data (writer, source, 1, timestamp + i, &raw_info, data_buffer))
            g_error ("can't add data to '%s'", sonar_get_name (n_channel));
          if (!hyscan_data_writer_raw_add_noise (writer, source, 1, timestamp + i, &raw_info, data_buffer))
            g_error ("can't add noise to '%s'", sonar_get_name (n_channel));
        }
      else
        {
          if (!hyscan_data_writer_acoustic_add_data (writer, source, timestamp + i, &acoustic_info, data_buffer))
            g_error ("can't add data to '%s'", sonar_get_name (n_channel));
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
log_add_data (HyScanDataWriter *writer)
{
  guint32 i;

  for (i = 0; i < N_RECORDS_PER_CHANNEL; i++)
    {
      gchar *message = g_strdup_printf ("test log message for time %d", i);
      hyscan_data_writer_log_add_message (writer, "test", i, HYSCAN_LOG_LEVEL_INFO, message);
      g_free (message);
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

  channel_name = hyscan_channel_get_name_by_types (HYSCAN_SOURCE_NMEA_ANY, TRUE, n_channel);

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
                  guint        n_channel,
                  gboolean     raw)
{
  gint32 project_id;
  gint32 track_id;
  gint32 channel_id;

  const gchar *channel_name;
  HyScanSourceType source;

  HyScanBuffer *data_buffer;
  HyScanBuffer *signal_buffer;
  HyScanBuffer *tvg_buffer;

  guint i, j;

  data_buffer = hyscan_buffer_new ();
  tvg_buffer = hyscan_buffer_new ();
  signal_buffer = hyscan_buffer_new ();

  source = sonar_get_type (n_channel);
  channel_name = hyscan_channel_get_name_by_types (source, raw, 1);

  g_message ("checking '%s.%s.%s'", PROJECT_NAME, track_name, channel_name);

  project_id = hyscan_db_project_open (db, PROJECT_NAME);
  if (project_id < 0)
    g_error ("can't open project");

  track_id = hyscan_db_track_open (db, project_id, track_name);
  if (track_id < 0)
    g_error ("can't open track");

  /* Проверка параметров галса. */
  track_check_info (db, track_id);

  /* Проверка канала данных. */
  channel_id = hyscan_db_channel_open (db, track_id, channel_name);
  if (channel_id < 0)
    g_error ("can't open channel");

  /* Проверка параметров. */
  if (raw)
    {
      raw_check_info (db, channel_id, n_channel);
      antenna_check_position (db, channel_id, RAW_CHANNEL_SCHEMA_ID, n_channel);
    }
  else
    {
      acoustic_check_info (db, channel_id, n_channel);
      antenna_check_position (db, channel_id, ACOUSTIC_CHANNEL_SCHEMA_ID, n_channel);
    }

  /* Проверка данных. */
  for (i = 0; i < N_RECORDS_PER_CHANNEL; i++)
    {
      const guint16 *data_values;
      guint32 data_size;
      gint64 time;

      if (!hyscan_db_channel_get_data (db, channel_id, i, data_buffer, &time))
        g_error ("can't read data from channel");

      data_values = hyscan_buffer_get_data (data_buffer, &data_size);

      if (time != timestamp + i)
        g_error ("time stamp mismatch");

      if (data_size != DATA_SIZE * sizeof (guint16))
        g_error ("data size mismatch");

      for (j = 0; j < DATA_SIZE; j++)
        if (data_values[j] != n_channel + i + j)
          g_error ("data content mismatch");
    }

  hyscan_db_close (db, channel_id);

  /* Проверка шумов. */
  if (raw)
    {
      gchar *noise_name = g_strdup_printf ("%s-noise", channel_name);
      channel_id = hyscan_db_channel_open (db, track_id, noise_name);
      g_free (noise_name);

      antenna_check_position (db, channel_id, RAW_CHANNEL_SCHEMA_ID, n_channel);
      raw_check_info (db, channel_id, n_channel);

      for (i = 0; i < N_RECORDS_PER_CHANNEL; i++)
        {
          const guint16 *data_values;
          guint32 data_size;
          gint64 time;

          if (!hyscan_db_channel_get_data (db, channel_id, i, data_buffer, &time))
            g_error ("can't read noise from channel");

          data_values = hyscan_buffer_get_data (data_buffer, &data_size);

          if (time != timestamp + i)
            g_error ("time stamp mismatch");

          if (data_size != DATA_SIZE * sizeof (guint16))
            g_error ("noise size mismatch");

          for (j = 0; j < DATA_SIZE; j++)
            if (data_values[j] != n_channel + i + j)
              g_error ("noise content mismatch");
        }

      hyscan_db_close (db, channel_id);
    }

  /* Проверка сигналов. */
  if (raw)
    {
      gchar *signal_name = g_strdup_printf ("%s-signal", channel_name);
      channel_id = hyscan_db_channel_open (db, track_id, signal_name);
      g_free (signal_name);

      if (channel_id < 0)
        g_error ("can't open signal channel");

      /* Проверка параметров. */
      signal_check_info (db, channel_id, n_channel);

      /* Проверка образов сигналов. */
      for (i = 0; i < N_RECORDS_PER_CHANNEL; i++)
        {
          const HyScanComplexFloat *signal_points;
          guint32 signal_size;
          gint64 time;

          if ((i % N_LINES_PER_SIGNAL) != 0)
            continue;

          if (!hyscan_db_channel_get_data (db, channel_id, i / N_LINES_PER_SIGNAL, signal_buffer, &time))
            g_error ("can't read data from signal channel");

          signal_points = hyscan_buffer_get_data (signal_buffer, &signal_size);

          if (time != timestamp + i)
            g_error ("signal time stamp mismatch");

          if (signal_size != SIGNAL_SIZE * sizeof (HyScanComplexFloat))
            g_error ("signal size mismatch");

          for (j = 0; j < SIGNAL_SIZE; j++)
            if ((signal_points[j].re != n_channel + i + j) ||
                (signal_points[j].im != -signal_points[j].re))
              g_error ("signal content mismatch");
        }

      hyscan_db_close (db, channel_id);
    }

  /* Проверка ВАРУ. */
  if (raw)
    {
      gchar *tvg_name = g_strdup_printf ("%s-tvg", channel_name);
      channel_id = hyscan_db_channel_open (db, track_id, tvg_name);
      g_free (tvg_name);

      if (channel_id < 0)
        g_error ("can't open tvg channel");

      /* Проверка параметров. */
      tvg_check_info (db, channel_id, n_channel);


      /* Проверка данных ВАРУ. */
      for (i = 0; i < N_RECORDS_PER_CHANNEL; i++)
        {
          const float *tvg_gains;
          guint32 tvg_size;
          gint64 time;

          if ((i % N_LINES_PER_TVG) != 0)
            continue;

          if (!hyscan_db_channel_get_data (db, channel_id, i / N_LINES_PER_TVG, tvg_buffer, &time))
            g_error ("can't read data from tvg channel");

          tvg_gains = hyscan_buffer_get_data (tvg_buffer, &tvg_size);

          if (time != timestamp + i)
            g_error ("tvg time stamp mismatch");

          if (tvg_size != TVG_SIZE * sizeof (gfloat))
            g_error ("tvg size mismatch");

          for (j = 0; j < TVG_SIZE; j++)
            if (tvg_gains[j] != (n_channel + i + j))
              g_error ("tvg content mismatch");
        }

      hyscan_db_close (db, channel_id);
    }

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

  channel_name = hyscan_channel_get_name_by_types (HYSCAN_SOURCE_LOG, FALSE, 1);
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

/* Функция проверяет отсутствие гидролокационных данных. */
void
sonar_check_misses (HyScanDB    *db,
                    const gchar *track_name,
                    guint        n_channel,
                    gboolean     raw)
{
  gint32 project_id;
  gint32 track_id;

  HyScanSourceType source;
  const gchar *channel_name;
  gchar *signal_name;
  gchar *tvg_name;

  gchar **channels;
  guint i;

  source = sonar_get_type (n_channel);
  channel_name = hyscan_channel_get_name_by_types (source, raw, 1);
  signal_name = g_strdup_printf ("%s-signal", channel_name);
  tvg_name = g_strdup_printf ("%s-tvg", channel_name);

  g_message ("checking '%s.%s.%s' misses", PROJECT_NAME, track_name, channel_name);

  project_id = hyscan_db_project_open (db, PROJECT_NAME);
  if (project_id < 0)
    g_error ("can't open project");

  track_id = hyscan_db_track_open (db, project_id, track_name);
  if (track_id < 0)
    g_error ("can't open track");

  /* Список каналов. */
  channels = hyscan_db_channel_list (db, track_id);
  if (channels == NULL)
    g_error ("can't list channels");

  for (i = 0; channels[i] != NULL; i++)
    {
      if (g_strcmp0 (channels[i], channel_name) == 0)
        g_error ("channel exist");

      if (g_strcmp0 (channels[i], signal_name) == 0)
        g_error ("signal exist");

      if (g_strcmp0 (channels[i], tvg_name) == 0)
        g_error ("tvg exist");
    }

  hyscan_db_close (db, project_id);
  hyscan_db_close (db, track_id);

  g_strfreev (channels);
  g_free (signal_name);
  g_free (tvg_name);
}

int
main (int    argc,
      char **argv)
{
  HyScanDB *db;
  HyScanDataWriter *writer;

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
  g_message ("creating track-0");

  /* Попытка записи данных при выключенной записи. */
  for (i = 1; i <= N_CHANNELS_PER_TYPE; i++)
    {
      sensor_add_data (writer, 0, i, TRUE);
      sonar_add_data  (writer, 0, i, TRUE);
      sonar_add_data  (writer, 0, i, FALSE);
    }
  log_add_data (writer);

  if (!hyscan_data_writer_start (writer, PROJECT_NAME, "track-0", HYSCAN_TRACK_SURVEY))
    g_error ("can't start writer");

  /* Первый галс. */
  g_message ("creating track-1");
  hyscan_data_writer_set_mode (writer, HYSCAN_DATA_WRITER_MODE_BOTH);
  if (!hyscan_data_writer_start (writer, PROJECT_NAME, "track-1", HYSCAN_TRACK_SURVEY))
    g_error ("can't start writer");

  /* Запись данных. */
  for (i = 1; i <= N_CHANNELS_PER_TYPE; i++)
    {
      sensor_add_data (writer, 1000, i, FALSE);
      sonar_add_data  (writer, 1000, i, TRUE);
      sonar_add_data  (writer, 1000, i, FALSE);
    }
  log_add_data (writer);

  /* Второй галс. */
  g_message ("creating track-2");
  if (!hyscan_data_writer_start (writer, PROJECT_NAME, "track-2", HYSCAN_TRACK_SURVEY))
    g_error ("can't start writer");

  /* Запись данных. */
  for (i = 1; i <= N_CHANNELS_PER_TYPE; i++)
    {
      sensor_add_data (writer, 2000, i, FALSE);
      sonar_add_data  (writer, 2000, i, TRUE);
      sonar_add_data  (writer, 2000, i, FALSE);
    }
  log_add_data (writer);

  /* Третий галс - только сырые данные от гидролокатора. */
  g_message ("creating track-3");
  hyscan_data_writer_set_mode (writer, HYSCAN_DATA_WRITER_MODE_RAW);
  if (!hyscan_data_writer_start (writer, PROJECT_NAME, "track-3", HYSCAN_TRACK_SURVEY))
    g_error ("can't start writer");

  /* Запись данных. */
  for (i = 1; i <= N_CHANNELS_PER_TYPE; i++)
    {
      sensor_add_data (writer, 3000, i, FALSE);
      sonar_add_data  (writer, 3000, i, TRUE);
      sonar_add_data  (writer, 3000, i, FALSE);
    }
  log_add_data (writer);

  /* Четвёртый галс - только обработанные данные от гидролокатора. */
  g_message ("creating track-4");
  hyscan_data_writer_set_mode (writer, HYSCAN_DATA_WRITER_MODE_COMPUTED);
  if (!hyscan_data_writer_start (writer, PROJECT_NAME, "track-4", HYSCAN_TRACK_SURVEY))
    g_error ("can't start writer");

  /* Запись данных. */
  for (i = 1; i <= N_CHANNELS_PER_TYPE; i++)
    {
      sensor_add_data (writer, 4000, i, FALSE);
      sonar_add_data  (writer, 4000, i, TRUE);
      sonar_add_data  (writer, 4000, i, FALSE);
    }
  log_add_data (writer);

  /* Пятый галс - запись отключена. */
  g_message ("creating track-5");
  hyscan_data_writer_set_mode (writer, HYSCAN_DATA_WRITER_MODE_NONE);
  if (!hyscan_data_writer_start (writer, PROJECT_NAME, "track-5", HYSCAN_TRACK_SURVEY))
    g_error ("can't start writer");

  /* Запись данных. */
  for (i = 1; i <= N_CHANNELS_PER_TYPE; i++)
    {
      sensor_add_data (writer, 5000, i, FALSE);
      sonar_add_data  (writer, 5000, i, TRUE);
      sonar_add_data  (writer, 5000, i, FALSE);
    }
  log_add_data (writer);

  /* Дублирование галса. */
  g_message ("duplicate track-0");
  if (hyscan_data_writer_start (writer, PROJECT_NAME, "track-0", HYSCAN_TRACK_SURVEY))
    g_error ("can duplicate track");

  /* Отключаем запись. */
  hyscan_data_writer_stop (writer);

  /* Проверяем записанные данные. */
  project_id = hyscan_db_project_open (db, PROJECT_NAME);
  if (project_id < 0)
    g_error ("can't open project '%s'", PROJECT_NAME);

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
      sonar_check_data  (db, "track-1", 1000, i, TRUE);
      sonar_check_data  (db, "track-1", 1000, i, FALSE);
    }
  log_check_data (db, "track-1");

  /* Второй галс. */
  for (i = 1; i <= N_CHANNELS_PER_TYPE; i++)
    {
      sensor_check_data (db, "track-2", 2000, i);
      sonar_check_data  (db, "track-2", 2000, i, TRUE);
      sonar_check_data  (db, "track-2", 2000, i, FALSE);
    }
  log_check_data (db, "track-2");

  /* Третий галс - только сырые данные. */
  for (i = 1; i <= N_CHANNELS_PER_TYPE; i++)
    {
      sensor_check_data  (db, "track-3", 3000, i);
      sonar_check_data   (db, "track-3", 3000, i, TRUE);
      sonar_check_misses (db, "track-3", i, FALSE);
    }
  log_check_data (db, "track-3");

  /* Четвёртый галс - только обработанные данные. */
  for (i = 1; i <= N_CHANNELS_PER_TYPE; i++)
    {
      sensor_check_data  (db, "track-4", 4000, i);
      sonar_check_data   (db, "track-4", 4000, i, FALSE);
      sonar_check_misses (db, "track-4", i, TRUE);
    }
  log_check_data (db, "track-4");

  /* Пятый галс - пустой. */
  track_id = hyscan_db_track_open (db, project_id, "track-5");
  if (track_id < 0)
    g_error ("can't open track 'track-5'");

  if (hyscan_db_channel_list (db, track_id) != NULL)
    g_error ("track-5 isn't empty");
  hyscan_db_close (db, track_id);

  /* Удаляем проект. */
  hyscan_db_close (db, project_id);
  hyscan_db_project_remove (db, PROJECT_NAME);

  /* Очищаем память. */
  g_object_unref (writer);
  g_object_unref (db);

  xmlCleanupParser ();

  return 0;
}
