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

#include "hyscan-core-common.h"

#include <math.h>

/* Функция устанавливает параметры местоположения приёмной антенны. */
gboolean
hyscan_core_params_set_antenna_position (HyScanDB              *db,
                                         gint32                 channel_id,
                                         HyScanAntennaPosition *position)
{
  HyScanParamList *param_list;
  gint32 param_id;
  gboolean status;

  param_id = hyscan_db_channel_param_open (db, channel_id);
  if (param_id < 0)
    return FALSE;

  param_list = hyscan_param_list_new ();

  hyscan_param_list_set_double (param_list, "/position/x", position->x);
  hyscan_param_list_set_double (param_list, "/position/y", position->y);
  hyscan_param_list_set_double (param_list, "/position/z", position->z);
  hyscan_param_list_set_double (param_list, "/position/psi", position->psi);
  hyscan_param_list_set_double (param_list, "/position/gamma", position->gamma);
  hyscan_param_list_set_double (param_list, "/position/theta", position->theta);

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

  hyscan_param_list_set_string (param_list, "/data/type", hyscan_data_get_name_by_type (info->data_type));
  hyscan_param_list_set_double (param_list, "/data/rate", info->data_rate);
  hyscan_param_list_set_double (param_list, "/signal/frequency", info->signal_frequency);
  hyscan_param_list_set_double (param_list, "/signal/bandwidth", info->signal_bandwidth);
  hyscan_param_list_set_double (param_list, "/antenna/offset/vertical", info->antenna_voffset);
  hyscan_param_list_set_double (param_list, "/antenna/offset/horizontal", info->antenna_hoffset);
  hyscan_param_list_set_double (param_list, "/antenna/pattern/vertical", info->antenna_vpattern);
  hyscan_param_list_set_double (param_list, "/antenna/pattern/horizontal", info->antenna_hpattern);
  hyscan_param_list_set_double (param_list, "/antenna/frequency", info->antenna_frequency);
  hyscan_param_list_set_double (param_list, "/antenna/bandwidth", info->antenna_bandwidth);
  hyscan_param_list_set_double (param_list, "/adc/vref", info->adc_vref);
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

  hyscan_param_list_set_string (param_list, "/data/type", hyscan_data_get_name_by_type (HYSCAN_DATA_COMPLEX_FLOAT));
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

  hyscan_param_list_set_string (param_list, "/data/type", hyscan_data_get_name_by_type (HYSCAN_DATA_FLOAT));
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

/* Функция загружает местоположение приёмной антенны гидролокатора. */
gboolean
hyscan_core_params_load_antenna_position (HyScanDB              *db,
                                          gint32                 param_id,
                                          gint64                 schema_id,
                                          gint64                 schema_version,
                                          HyScanAntennaPosition *position)
{
  HyScanParamList *param_list;
  gboolean status = FALSE;

  param_list = hyscan_param_list_new ();

  hyscan_param_list_add (param_list, "/schema/id");
  hyscan_param_list_add (param_list, "/schema/version");
  hyscan_param_list_add (param_list, "/position/x");
  hyscan_param_list_add (param_list, "/position/y");
  hyscan_param_list_add (param_list, "/position/z");
  hyscan_param_list_add (param_list, "/position/psi");
  hyscan_param_list_add (param_list, "/position/gamma");
  hyscan_param_list_add (param_list, "/position/theta");

  if (!hyscan_db_param_get (db, param_id, NULL, param_list))
    goto exit;

  if ((hyscan_param_list_get_integer (param_list, "/schema/id") != schema_id) ||
      (hyscan_param_list_get_integer (param_list, "/schema/version") != schema_version))
    {
      goto exit;
    }

  position->x = hyscan_param_list_get_double (param_list, "/position/x");
  position->y = hyscan_param_list_get_double (param_list, "/position/y");
  position->z = hyscan_param_list_get_double (param_list, "/position/z");
  position->psi = hyscan_param_list_get_double (param_list, "/position/psi");
  position->gamma = hyscan_param_list_get_double (param_list, "/position/gamma");
  position->theta = hyscan_param_list_get_double (param_list, "/position/theta");

  status = TRUE;

exit:
  g_object_unref (param_list);

  return status;
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
  hyscan_param_list_add (param_list, "/antenna/offset/vertical");
  hyscan_param_list_add (param_list, "/antenna/offset/horizontal");
  hyscan_param_list_add (param_list, "/antenna/pattern/vertical");
  hyscan_param_list_add (param_list, "/antenna/pattern/horizontal");
  hyscan_param_list_add (param_list, "/antenna/frequency");
  hyscan_param_list_add (param_list, "/antenna/bandwidth");
  hyscan_param_list_add (param_list, "/adc/vref");
  hyscan_param_list_add (param_list, "/adc/offset");

  if (!hyscan_db_param_get (db, param_id, NULL, param_list))
    goto exit;

  if ((hyscan_param_list_get_integer (param_list, "/schema/id") != ACOUSTIC_CHANNEL_SCHEMA_ID) ||
      (hyscan_param_list_get_integer (param_list, "/schema/version") != ACOUSTIC_CHANNEL_SCHEMA_VERSION))
    {
      goto exit;
    }

  info->data_type = hyscan_data_get_type_by_name (hyscan_param_list_get_string (param_list, "/data/type"));
  info->data_rate = hyscan_param_list_get_double (param_list, "/data/rate");
  info->signal_frequency = hyscan_param_list_get_double (param_list, "/signal/frequency");
  info->signal_bandwidth = hyscan_param_list_get_double (param_list, "/signal/bandwidth");
  info->antenna_voffset = hyscan_param_list_get_double (param_list, "/antenna/offset/vertical");
  info->antenna_hoffset = hyscan_param_list_get_double (param_list, "/antenna/offset/horizontal");
  info->antenna_vpattern = hyscan_param_list_get_double (param_list, "/antenna/pattern/vertical");
  info->antenna_hpattern = hyscan_param_list_get_double (param_list, "/antenna/pattern/horizontal");
  info->antenna_frequency = hyscan_param_list_get_double (param_list, "/antenna/frequency");
  info->antenna_bandwidth = hyscan_param_list_get_double (param_list, "/antenna/bandwidth");
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

  data_type = hyscan_data_get_type_by_name (hyscan_param_list_get_string (param_list, "/data/type"));

  if ((hyscan_param_list_get_integer (param_list, "/schema/id") != SIGNAL_CHANNEL_SCHEMA_ID) ||
      (hyscan_param_list_get_integer (param_list, "/schema/version") != SIGNAL_CHANNEL_SCHEMA_VERSION) ||
      (data_type != HYSCAN_DATA_COMPLEX_FLOAT) ||
      (fabs (hyscan_param_list_get_double (param_list, "/data/rate") - data_rate) > 1.0))
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
  gboolean status = FALSE;

  param_list = hyscan_param_list_new ();

  hyscan_param_list_add (param_list, "/schema/id");
  hyscan_param_list_add (param_list, "/schema/version");
  hyscan_param_list_add (param_list, "/data/type");
  hyscan_param_list_add (param_list, "/data/rate");

  if (!hyscan_db_param_get (db, param_id, NULL, param_list))
    goto exit;

  if ((hyscan_param_list_get_integer (param_list, "/schema/id") != TVG_CHANNEL_SCHEMA_ID) ||
      (hyscan_param_list_get_integer (param_list, "/schema/version") != TVG_CHANNEL_SCHEMA_VERSION) ||
      (fabs (hyscan_param_list_get_double (param_list, "/data/rate") - data_rate) > 1.0) ||
      (hyscan_data_get_type_by_name (hyscan_param_list_get_string (param_list, "/data/type")) != HYSCAN_DATA_FLOAT))
    {
      goto exit;
    }

  status = TRUE;

exit:
  g_object_unref (param_list);

  return status;
}
