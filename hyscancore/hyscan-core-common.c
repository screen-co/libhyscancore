/* hyscan-core-names.c
 *
 * Copyright 2017-2018 Screen LLC, Andrei Fadeev <andrei@webcontrol.ru>
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

#include "hyscan-core-common.h"

#include <math.h>

/* Функция устанавливает параметры смещения приёмной антенны. */
gboolean
hyscan_core_params_set_antenna_offset (HyScanDB            *db,
                                       gint32               channel_id,
                                       HyScanAntennaOffset *offset)
{
  HyScanParamList *param_list;
  gint32 param_id;
  gboolean status;

  param_id = hyscan_db_channel_param_open (db, channel_id);
  if (param_id < 0)
    return FALSE;

  param_list = hyscan_param_list_new ();

  hyscan_param_list_set_double (param_list, "/offset/starboard", offset->starboard);
  hyscan_param_list_set_double (param_list, "/offset/forward", offset->forward);
  hyscan_param_list_set_double (param_list, "/offset/vertical", offset->vertical);
  hyscan_param_list_set_double (param_list, "/offset/yaw", offset->yaw);
  hyscan_param_list_set_double (param_list, "/offset/pitch", offset->pitch);
  hyscan_param_list_set_double (param_list, "/offset/roll", offset->roll);

  status = hyscan_db_param_set (db, param_id, NULL, param_list);

  hyscan_db_close (db, param_id);
  g_object_unref (param_list);

  return status;
}

/* Функция устанавливает параметры данных датчика. */
gboolean
hyscan_core_params_set_sensor_info (HyScanDB    *db,
                                    gint32       channel_id,
                                    const gchar *sensor_name)
{
  HyScanParamList *param_list;
  gint32 param_id;
  gboolean status;

  param_id = hyscan_db_channel_param_open (db, channel_id);
  if (param_id < 0)
    return FALSE;

  param_list = hyscan_param_list_new ();

  hyscan_param_list_set_string (param_list, "/sensor-name", sensor_name);

  status = hyscan_db_param_set (db, param_id, NULL, param_list);

  hyscan_db_close (db, param_id);
  g_object_unref (param_list);

  return status;
}

/* Функция устанавливает параметры гидроакустических данных. */
gboolean
hyscan_core_params_set_acoustic_data_info (HyScanDB               *db,
                                           gint32                  channel_id,
                                           HyScanAcousticDataInfo *info)
{
  HyScanParamList *param_list;
  gint32 param_id;
  gboolean status;

  param_id = hyscan_db_channel_param_open (db, channel_id);
  if (param_id < 0)
    return FALSE;

  param_list = hyscan_param_list_new ();

  hyscan_param_list_set_string  (param_list, "/data/type", hyscan_data_get_id_by_type (info->data_type));
  hyscan_param_list_set_double  (param_list, "/data/rate", info->data_rate);
  hyscan_param_list_set_double  (param_list, "/signal/frequency", info->signal_frequency);
  hyscan_param_list_set_double  (param_list, "/signal/bandwidth", info->signal_bandwidth);
  hyscan_param_list_set_double  (param_list, "/signal/heterodyne", info->signal_heterodyne);
  hyscan_param_list_set_double  (param_list, "/antenna/offset/vertical", info->antenna_voffset);
  hyscan_param_list_set_double  (param_list, "/antenna/offset/horizontal", info->antenna_hoffset);
  hyscan_param_list_set_double  (param_list, "/antenna/aperture/vertical", info->antenna_vaperture);
  hyscan_param_list_set_double  (param_list, "/antenna/aperture/horizontal", info->antenna_haperture);
  hyscan_param_list_set_double  (param_list, "/antenna/frequency", info->antenna_frequency);
  hyscan_param_list_set_double  (param_list, "/antenna/bandwidth", info->antenna_bandwidth);
  hyscan_param_list_set_integer (param_list, "/antenna/group", info->antenna_group);
  hyscan_param_list_set_double  (param_list, "/adc/vref", info->adc_vref);
  hyscan_param_list_set_integer (param_list, "/adc/offset", info->adc_offset);

  status = hyscan_db_param_set (db, param_id, NULL, param_list);

  hyscan_db_close (db, param_id);
  g_object_unref (param_list);

  return status;
}

/* Функция устанавливает параметры образов сигнала. */
gboolean
hyscan_core_params_set_signal_info (HyScanDB *db,
                                    gint32    channel_id,
                                    gdouble   data_rate)
{
  HyScanParamList *param_list;
  gint32 param_id;
  gboolean status;

  param_id = hyscan_db_channel_param_open (db, channel_id);
  if (param_id < 0)
    return FALSE;

  param_list = hyscan_param_list_new ();

  hyscan_param_list_set_string (param_list, "/data/type", hyscan_data_get_id_by_type (HYSCAN_DATA_COMPLEX_FLOAT32LE));
  hyscan_param_list_set_double (param_list, "/data/rate", data_rate);

  status = hyscan_db_param_set (db, param_id, NULL, param_list);

  hyscan_db_close (db, param_id);
  g_object_unref (param_list);

  return status;
}

/* Функция устанавливает параметры данных ВАРУ. */
gboolean
hyscan_core_params_set_tvg_info (HyScanDB *db,
                                 gint32    channel_id,
                                 gdouble   data_rate)
{
  HyScanParamList *param_list;
  gint32 param_id;
  gboolean status;

  param_id = hyscan_db_channel_param_open (db, channel_id);
  if (param_id < 0)
    return FALSE;

  param_list = hyscan_param_list_new ();

  hyscan_param_list_set_string (param_list, "/data/type", hyscan_data_get_id_by_type (HYSCAN_DATA_FLOAT32LE));
  hyscan_param_list_set_double (param_list, "/data/rate", data_rate);

  status = hyscan_db_param_set (db, param_id, NULL, param_list);

  hyscan_db_close (db, param_id);
  g_object_unref (param_list);

  return status;
}

/* Функция проверяет схему канала информационных сообщений. */
gboolean
hyscan_core_params_check_log_schema (HyScanDB *db,
                                     gint32    param_id)
{
  HyScanParamList *param_list;
  gboolean status = FALSE;

  param_list = hyscan_param_list_new ();

  hyscan_param_list_add (param_list, "/schema/id");
  hyscan_param_list_add (param_list, "/schema/version");

  if (!hyscan_db_param_get (db, param_id, NULL, param_list))
    goto exit;

  if ((hyscan_param_list_get_integer (param_list, "/schema/id") != LOG_SCHEMA_ID) ||
      (hyscan_param_list_get_integer (param_list, "/schema/version") != LOG_SCHEMA_VERSION))
    {
      goto exit;
    }

  status = TRUE;

exit:
  g_object_unref (param_list);

  return status;
}

/* Функция загружает смещение приёмной антенны. */
static gboolean
hyscan_core_params_load_antenna_offset (HyScanDB            *db,
                                        gint32               param_id,
                                        gint64               schema_id,
                                        gint64               schema_version,
                                        HyScanAntennaOffset *offset)
{
  HyScanParamList *param_list;
  gboolean status = FALSE;

  param_list = hyscan_param_list_new ();

  hyscan_param_list_add (param_list, "/schema/id");
  hyscan_param_list_add (param_list, "/schema/version");
  hyscan_param_list_add (param_list, "/offset/starboard");
  hyscan_param_list_add (param_list, "/offset/forward");
  hyscan_param_list_add (param_list, "/offset/vertical");
  hyscan_param_list_add (param_list, "/offset/yaw");
  hyscan_param_list_add (param_list, "/offset/pitch");
  hyscan_param_list_add (param_list, "/offset/roll");

  if (!hyscan_db_param_get (db, param_id, NULL, param_list))
    goto exit;

  if ((hyscan_param_list_get_integer (param_list, "/schema/id") != schema_id) ||
      (hyscan_param_list_get_integer (param_list, "/schema/version") != schema_version))
    {
      goto exit;
    }

  offset->starboard = hyscan_param_list_get_double (param_list, "/offset/starboard");
  offset->forward = hyscan_param_list_get_double (param_list, "/offset/forward");
  offset->vertical = hyscan_param_list_get_double (param_list, "/offset/vertical");
  offset->yaw = hyscan_param_list_get_double (param_list, "/offset/yaw");
  offset->pitch = hyscan_param_list_get_double (param_list, "/offset/pitch");
  offset->roll = hyscan_param_list_get_double (param_list, "/offset/roll");

  status = TRUE;

exit:
  g_object_unref (param_list);

  return status;
}

/* Функция загружает смещение приёмной антенны датчика. */
gboolean
hyscan_core_params_load_sensor_offset (HyScanDB            *db,
                                       gint32               param_id,
                                       HyScanAntennaOffset *offset)
{
  return hyscan_core_params_load_antenna_offset (db, param_id,
                                                 SENSOR_CHANNEL_SCHEMA_ID,
                                                 SENSOR_CHANNEL_SCHEMA_VERSION,
                                                 offset);
}

/* Функция загружает смещение приёмной антенны гидроакустических данных. */
gboolean
hyscan_core_params_load_acoustic_offset (HyScanDB            *db,
                                         gint32               param_id,
                                         HyScanAntennaOffset *offset)
{
  return hyscan_core_params_load_antenna_offset (db, param_id,
                                                 ACOUSTIC_CHANNEL_SCHEMA_ID,
                                                 ACOUSTIC_CHANNEL_SCHEMA_VERSION,
                                                 offset);
}

/* Функция загружает параметры данных датчика. */
gchar *
hyscan_core_params_load_sensor_info (HyScanDB *db,
                                     gint32    param_id)
{
  HyScanParamList *param_list;
  gchar *sensor_name = NULL;

  param_list = hyscan_param_list_new ();

  hyscan_param_list_add (param_list, "/schema/id");
  hyscan_param_list_add (param_list, "/schema/version");
  hyscan_param_list_add (param_list, "/sensor-name");

  if (!hyscan_db_param_get (db, param_id, NULL, param_list))
    goto exit;

  if ((hyscan_param_list_get_integer (param_list, "/schema/id") != SENSOR_CHANNEL_SCHEMA_ID) ||
      (hyscan_param_list_get_integer (param_list, "/schema/version") != SENSOR_CHANNEL_SCHEMA_VERSION))
    {
      goto exit;
    }

  sensor_name = hyscan_param_list_dup_string (param_list, "/sensor-name");

exit:
  g_object_unref (param_list);

  return sensor_name;
}

/* Функция загружает параметры гидроакустических данных. */
gboolean
hyscan_core_params_load_acoustic_data_info (HyScanDB               *db,
                                            gint32                  param_id,
                                            HyScanAcousticDataInfo *info)
{
  HyScanParamList *param_list;
  gboolean status = FALSE;

  param_list = hyscan_param_list_new ();

  hyscan_param_list_add (param_list, "/schema/id");
  hyscan_param_list_add (param_list, "/schema/version");
  hyscan_param_list_add (param_list, "/data/type");
  hyscan_param_list_add (param_list, "/data/rate");
  hyscan_param_list_add (param_list, "/signal/frequency");
  hyscan_param_list_add (param_list, "/signal/bandwidth");
  hyscan_param_list_add (param_list, "/signal/heterodyne");
  hyscan_param_list_add (param_list, "/antenna/offset/vertical");
  hyscan_param_list_add (param_list, "/antenna/offset/horizontal");
  hyscan_param_list_add (param_list, "/antenna/aperture/vertical");
  hyscan_param_list_add (param_list, "/antenna/aperture/horizontal");
  hyscan_param_list_add (param_list, "/antenna/frequency");
  hyscan_param_list_add (param_list, "/antenna/bandwidth");
  hyscan_param_list_add (param_list, "/antenna/group");
  hyscan_param_list_add (param_list, "/adc/vref");
  hyscan_param_list_add (param_list, "/adc/offset");

  if (!hyscan_db_param_get (db, param_id, NULL, param_list))
    goto exit;

  if ((hyscan_param_list_get_integer (param_list, "/schema/id") != ACOUSTIC_CHANNEL_SCHEMA_ID) ||
      (hyscan_param_list_get_integer (param_list, "/schema/version") != ACOUSTIC_CHANNEL_SCHEMA_VERSION))
    {
      goto exit;
    }

  info->data_type = hyscan_data_get_type_by_id (hyscan_param_list_get_string (param_list, "/data/type"));
  info->data_rate = hyscan_param_list_get_double (param_list, "/data/rate");
  info->signal_frequency = hyscan_param_list_get_double (param_list, "/signal/frequency");
  info->signal_bandwidth = hyscan_param_list_get_double (param_list, "/signal/bandwidth");
  info->signal_heterodyne = hyscan_param_list_get_double (param_list, "/signal/heterodyne");
  info->antenna_voffset = hyscan_param_list_get_double (param_list, "/antenna/offset/vertical");
  info->antenna_hoffset = hyscan_param_list_get_double (param_list, "/antenna/offset/horizontal");
  info->antenna_vaperture = hyscan_param_list_get_double (param_list, "/antenna/aperture/vertical");
  info->antenna_haperture = hyscan_param_list_get_double (param_list, "/antenna/aperture/horizontal");
  info->antenna_frequency = hyscan_param_list_get_double (param_list, "/antenna/frequency");
  info->antenna_bandwidth = hyscan_param_list_get_double (param_list, "/antenna/bandwidth");
  info->antenna_group = hyscan_param_list_get_integer (param_list, "/antenna/group");
  info->adc_vref = hyscan_param_list_get_double (param_list, "/adc/vref");
  info->adc_offset = hyscan_param_list_get_integer (param_list, "/adc/offset");

  status = TRUE;

exit:
  g_object_unref (param_list);

  return status;
}

/* Функция проверяет параметры образов сигнала. */
gboolean
hyscan_core_params_check_signal_info (HyScanDB *db,
                                      gint32    param_id,
                                      gdouble   data_rate)
{
  HyScanParamList *param_list;
  HyScanDataType data_type = HYSCAN_DATA_INVALID;
  gboolean status = FALSE;

  param_list = hyscan_param_list_new ();

  hyscan_param_list_add (param_list, "/schema/id");
  hyscan_param_list_add (param_list, "/schema/version");
  hyscan_param_list_add (param_list, "/data/type");
  hyscan_param_list_add (param_list, "/data/rate");

  if (!hyscan_db_param_get (db, param_id, NULL, param_list))
    goto exit;

  data_type = hyscan_data_get_type_by_id (hyscan_param_list_get_string (param_list, "/data/type"));

  if ((hyscan_param_list_get_integer (param_list, "/schema/id") != SIGNAL_CHANNEL_SCHEMA_ID) ||
      (hyscan_param_list_get_integer (param_list, "/schema/version") != SIGNAL_CHANNEL_SCHEMA_VERSION) ||
      (fabs (hyscan_param_list_get_double (param_list, "/data/rate") - data_rate) > 0.001) ||
      (data_type != HYSCAN_DATA_COMPLEX_FLOAT32LE))
    {
      goto exit;
    }

  status = TRUE;

exit:
  g_object_unref (param_list);

  return status;
}

/* Функция проверяет параметры данных ВАРУ. */
gboolean
hyscan_core_params_check_tvg_info (HyScanDB *db,
                                   gint32    param_id,
                                   gdouble   data_rate)
{
  HyScanParamList *param_list;
  HyScanDataType data_type = HYSCAN_DATA_INVALID;
  gboolean status = FALSE;

  param_list = hyscan_param_list_new ();

  hyscan_param_list_add (param_list, "/schema/id");
  hyscan_param_list_add (param_list, "/schema/version");
  hyscan_param_list_add (param_list, "/data/type");
  hyscan_param_list_add (param_list, "/data/rate");

  if (!hyscan_db_param_get (db, param_id, NULL, param_list))
    goto exit;

  data_type = hyscan_data_get_type_by_id (hyscan_param_list_get_string (param_list, "/data/type"));

  if ((hyscan_param_list_get_integer (param_list, "/schema/id") != TVG_CHANNEL_SCHEMA_ID) ||
      (hyscan_param_list_get_integer (param_list, "/schema/version") != TVG_CHANNEL_SCHEMA_VERSION) ||
      (fabs (hyscan_param_list_get_double (param_list, "/data/rate") - data_rate) > 0.001) ||
      (data_type != HYSCAN_DATA_FLOAT32LE))
    {
      goto exit;
    }

  status = TRUE;

exit:
  g_object_unref (param_list);

  return status;
}

/* Функция загружает план галса. */
gboolean
hyscan_core_params_load_plan (HyScanDB         *db,
                              gint32            param_id,
                              HyScanTrackPlan  *plan)
{
  HyScanParamList *param_list;
  gboolean status = FALSE;

  param_list = hyscan_param_list_new ();

  hyscan_param_list_add (param_list, "/schema/id");
  hyscan_param_list_add (param_list, "/schema/version");
  hyscan_param_list_add (param_list, "/plan/start/lat");
  hyscan_param_list_add (param_list, "/plan/start/lon");
  hyscan_param_list_add (param_list, "/plan/end/lat");
  hyscan_param_list_add (param_list, "/plan/end/lon");
  hyscan_param_list_add (param_list, "/plan/velocity");

  if (!hyscan_db_param_get (db, param_id, NULL, param_list))
    goto exit;

  if ((hyscan_param_list_get_integer (param_list, "/schema/id") != TRACK_SCHEMA_ID) ||
      (hyscan_param_list_get_integer (param_list, "/schema/version") != TRACK_SCHEMA_VERSION))
    {
      goto exit;
    }

  plan->start.lat = hyscan_param_list_get_double (param_list, "/plan/start/lat");
  plan->start.lon = hyscan_param_list_get_double (param_list, "/plan/start/lon");
  plan->end.lat = hyscan_param_list_get_double (param_list, "/plan/end/lat");
  plan->end.lon = hyscan_param_list_get_double (param_list, "/plan/end/lon");
  plan->velocity = hyscan_param_list_get_double (param_list, "/plan/velocity");

  status = (plan->velocity > 0);

exit:
  g_object_unref (param_list);

  return status;
}
