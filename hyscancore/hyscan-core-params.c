/*
 * \file hyscan-core-params.c
 *
 * \brief Исходный файл вспомогательных функций чтения/записи параметров данных
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-core-params.h"
#include "hyscan-core-schemas.h"

#include <math.h>

/* Функция устанавливает параметры местоположения приёмной антенны. */
gboolean
hyscan_core_params_set_antenna_position (HyScanDB              *db,
                                         gint32                 channel_id,
                                         HyScanAntennaPosition *position)
{
  const gchar *param_names[7] = {NULL};
  GVariant *param_values[7] = {NULL};
  gint32 param_id;
  gboolean status;

  param_id = hyscan_db_channel_param_open (db, channel_id);
  if (param_id < 0)
    return FALSE;

  param_names[0] = "/position/x";
  param_names[1] = "/position/y";
  param_names[2] = "/position/z";
  param_names[3] = "/position/psi";
  param_names[4] = "/position/gamma";
  param_names[5] = "/position/theta";

  param_values[0] = g_variant_new_double (position->x);
  param_values[1] = g_variant_new_double (position->y);
  param_values[2] = g_variant_new_double (position->z);
  param_values[3] = g_variant_new_double (position->psi);
  param_values[4] = g_variant_new_double (position->gamma);
  param_values[5] = g_variant_new_double (position->theta);

  status = hyscan_db_param_set (db, param_id, NULL, param_names, param_values);
  hyscan_db_close (db, param_id);

  if (!status)
    {
      g_variant_unref (param_values[0]);
      g_variant_unref (param_values[1]);
      g_variant_unref (param_values[2]);
      g_variant_unref (param_values[3]);
      g_variant_unref (param_values[4]);
      g_variant_unref (param_values[5]);
    }

  return status;
}

/* Функция устанавливает параметры сырых данных. */
gboolean
hyscan_core_params_set_raw_data_info (HyScanDB          *db,
                                      gint32             channel_id,
                                      HyScanRawDataInfo *info)
{
  const gchar *param_names[11] = {NULL};
  GVariant *param_values[11] = {NULL};
  gint32 param_id;
  gboolean status;

  param_id = hyscan_db_channel_param_open (db, channel_id);
  if (param_id < 0)
    return FALSE;

  param_names[0] = "/data/type";
  param_names[1] = "/data/rate";
  param_names[2] = "/antenna/offset/vertical";
  param_names[3] = "/antenna/offset/horizontal";
  param_names[4] = "/antenna/pattern/vertical";
  param_names[5] = "/antenna/pattern/horizontal";
  param_names[6] = "/antenna/frequency";
  param_names[7] = "/antenna/bandwidth";
  param_names[8] = "/adc/vref";
  param_names[9] = "/adc/offset";

  param_values[0] = g_variant_new_string (hyscan_data_get_type_name (info->data.type));
  param_values[1] = g_variant_new_double (info->data.rate);
  param_values[2] = g_variant_new_double (info->antenna.offset.vertical);
  param_values[3] = g_variant_new_double (info->antenna.offset.horizontal);
  param_values[4] = g_variant_new_double (info->antenna.pattern.vertical);
  param_values[5] = g_variant_new_double (info->antenna.pattern.horizontal);
  param_values[6] = g_variant_new_double (info->antenna.frequency);
  param_values[7] = g_variant_new_double (info->antenna.bandwidth);
  param_values[8] = g_variant_new_double (info->adc.vref);
  param_values[9] = g_variant_new_int64 (info->adc.offset);

  status = hyscan_db_param_set (db, param_id, NULL, param_names, param_values);
  hyscan_db_close (db, param_id);

  if (!status)
    {
      g_variant_unref (param_values[0]);
      g_variant_unref (param_values[1]);
      g_variant_unref (param_values[2]);
      g_variant_unref (param_values[3]);
      g_variant_unref (param_values[4]);
      g_variant_unref (param_values[5]);
      g_variant_unref (param_values[6]);
      g_variant_unref (param_values[7]);
      g_variant_unref (param_values[8]);
      g_variant_unref (param_values[9]);
    }

  return status;
}

/* Функция устанавливает параметры акустических данных. */
gboolean
hyscan_core_params_set_acoustic_data_info (HyScanDB               *db,
                                           gint32                  channel_id,
                                           HyScanAcousticDataInfo *info)
{
  const gchar *param_names[5] = {NULL};
  GVariant *param_values[5] = {NULL};
  gint32 param_id;
  gboolean status;

  param_id = hyscan_db_channel_param_open (db, channel_id);
  if (param_id < 0)
    return FALSE;

  param_names[0] = "/data/type";
  param_names[1] = "/data/rate";
  param_names[2] = "/antenna/pattern/vertical";
  param_names[3] = "/antenna/pattern/horizontal";

  param_values[0] = g_variant_new_string (hyscan_data_get_type_name (info->data.type));
  param_values[1] = g_variant_new_double (info->data.rate);
  param_values[2] = g_variant_new_double (info->antenna.pattern.vertical);
  param_values[3] = g_variant_new_double (info->antenna.pattern.horizontal);

  status = hyscan_db_param_set (db, param_id, NULL, param_names, param_values);
  hyscan_db_close (db, param_id);

  if (!status)
    {
      g_variant_unref (param_values[0]);
      g_variant_unref (param_values[1]);
      g_variant_unref (param_values[2]);
      g_variant_unref (param_values[3]);
    }

  return status;
}

/* Функция устанавливает параметры образов сигнала. */
gboolean
hyscan_core_params_set_signal_info (HyScanDB *db,
                                    gint32    channel_id,
                                    gdouble   data_rate)
{
  const gchar *param_names[3] = {NULL};
  GVariant *param_values[3] = {NULL};
  gint32 param_id;
  gboolean status;

  param_id = hyscan_db_channel_param_open (db, channel_id);
  if (param_id < 0)
    return FALSE;

  param_names[0] = "/data/type";
  param_names[1] = "/data/rate";

  param_values[0] = g_variant_new_string (hyscan_data_get_type_name (HYSCAN_DATA_COMPLEX_FLOAT));
  param_values[1] = g_variant_new_double (data_rate);

  status = hyscan_db_param_set (db, param_id, NULL, param_names, param_values);
  hyscan_db_close (db, param_id);

  if (!status)
    {
      g_variant_unref (param_values[0]);
      g_variant_unref (param_values[1]);
    }

  return status;
}

/* Функция устанавливает параметры данных ВАРУ. */
gboolean
hyscan_core_params_set_tvg_info (HyScanDB *db,
                                 gint32    channel_id,
                                 gdouble   data_rate)
{
  const gchar *param_names[3] = {NULL};
  GVariant *param_values[3] = {NULL};
  gint32 param_id;
  gboolean status;

  param_id = hyscan_db_channel_param_open (db, channel_id);
  if (param_id < 0)
    return FALSE;

  param_names[0] = "/data/type";
  param_names[1] = "/data/rate";

  param_values[0] = g_variant_new_string (hyscan_data_get_type_name (HYSCAN_DATA_FLOAT));
  param_values[1] = g_variant_new_double (data_rate);

  status = hyscan_db_param_set (db, param_id, NULL, param_names, param_values);
  hyscan_db_close (db, param_id);

  if (!status)
    {
      g_variant_unref (param_values[0]);
      g_variant_unref (param_values[1]);
    }

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
  const gchar *param_names[9] = {NULL};
  GVariant *param_values[9] = {NULL};
  gboolean status = FALSE;

  param_names[0] = "/schema/id";
  param_names[1] = "/schema/version";
  param_names[2] = "/position/x";
  param_names[3] = "/position/y";
  param_names[4] = "/position/z";
  param_names[5] = "/position/psi";
  param_names[6] = "/position/gamma";
  param_names[7] = "/position/theta";

  if (!hyscan_db_param_get (db, param_id, NULL, param_names, param_values))
    return FALSE;

  if ((g_variant_get_int64 (param_values[0]) != schema_id) ||
      (g_variant_get_int64 (param_values[1]) != schema_version) )
    {
      goto exit;
    }

  position->x = g_variant_get_double (param_values[2]);
  position->y = g_variant_get_double (param_values[3]);
  position->z = g_variant_get_double (param_values[4]);
  position->psi = g_variant_get_double (param_values[5]);
  position->gamma = g_variant_get_double (param_values[6]);
  position->theta = g_variant_get_double (param_values[7]);

  status = TRUE;

exit:
  g_variant_unref (param_values[0]);
  g_variant_unref (param_values[1]);
  g_variant_unref (param_values[2]);
  g_variant_unref (param_values[3]);
  g_variant_unref (param_values[4]);
  g_variant_unref (param_values[5]);
  g_variant_unref (param_values[6]);
  g_variant_unref (param_values[7]);

  return status;
}

/* Функция загружает параметры сырых данных. */
gboolean
hyscan_core_params_load_raw_data_info (HyScanDB          *db,
                                       gint32             param_id,
                                       HyScanRawDataInfo *info)
{
  const gchar *param_names[13] = {NULL};
  GVariant *param_values[13] = {NULL};
  gboolean status = FALSE;

  param_names[0] = "/schema/id";
  param_names[1] = "/schema/version";
  param_names[2] = "/data/type";
  param_names[3] = "/data/rate";
  param_names[4] = "/antenna/offset/vertical";
  param_names[5] = "/antenna/offset/horizontal";
  param_names[6] = "/antenna/pattern/vertical";
  param_names[7] = "/antenna/pattern/horizontal";
  param_names[8] = "/antenna/frequency";
  param_names[9] = "/antenna/bandwidth";
  param_names[10] = "/adc/vref";
  param_names[11] = "/adc/offset";

  if (!hyscan_db_param_get (db, param_id, NULL, param_names, param_values))
    return FALSE;

  if ((g_variant_get_int64 (param_values[0]) != RAW_CHANNEL_SCHEMA_ID) ||
      (g_variant_get_int64 (param_values[1]) != RAW_CHANNEL_SCHEMA_VERSION) )
    {
      goto exit;
    }

  info->data.type = hyscan_data_get_type_by_name (g_variant_get_string (param_values[2], NULL));
  info->data.rate = g_variant_get_double (param_values[3]);
  info->antenna.offset.vertical = g_variant_get_double (param_values[4]);
  info->antenna.offset.horizontal = g_variant_get_double (param_values[5]);
  info->antenna.pattern.vertical = g_variant_get_double (param_values[6]);
  info->antenna.pattern.horizontal = g_variant_get_double (param_values[7]);
  info->antenna.frequency = g_variant_get_double (param_values[8]);
  info->antenna.bandwidth = g_variant_get_double (param_values[9]);
  info->adc.vref = g_variant_get_double (param_values[10]);
  info->adc.offset = g_variant_get_int64 (param_values[11]);

  status = TRUE;

exit:
  g_variant_unref (param_values[0]);
  g_variant_unref (param_values[1]);
  g_variant_unref (param_values[2]);
  g_variant_unref (param_values[3]);
  g_variant_unref (param_values[4]);
  g_variant_unref (param_values[5]);
  g_variant_unref (param_values[6]);
  g_variant_unref (param_values[7]);
  g_variant_unref (param_values[8]);
  g_variant_unref (param_values[9]);
  g_variant_unref (param_values[10]);
  g_variant_unref (param_values[11]);

  return status;
}

/* Функция загружает параметры акустических данных. */
gboolean
hyscan_core_params_load_acoustic_data_info (HyScanDB               *db,
                                            gint32                  param_id,
                                            HyScanAcousticDataInfo *info)
{
  const gchar *param_names[7] = {NULL};
  GVariant *param_values[7] = {NULL};
  gboolean status = FALSE;

  param_names[0] = "/schema/id";
  param_names[1] = "/schema/version";
  param_names[2] = "/data/type";
  param_names[3] = "/data/rate";
  param_names[4] = "/antenna/pattern/vertical";
  param_names[5] = "/antenna/pattern/horizontal";

  if (!hyscan_db_param_get (db, param_id, NULL, param_names, param_values))
    return FALSE;

  if ((g_variant_get_int64 (param_values[0]) != ACOUSTIC_CHANNEL_SCHEMA_ID) ||
      (g_variant_get_int64 (param_values[1]) != ACOUSTIC_CHANNEL_SCHEMA_VERSION) )
    {
      goto exit;
    }

  info->data.type = hyscan_data_get_type_by_name (g_variant_get_string (param_values[2], NULL));
  info->data.rate = g_variant_get_double (param_values[3]);
  info->antenna.pattern.vertical = g_variant_get_double (param_values[4]);
  info->antenna.pattern.horizontal = g_variant_get_double (param_values[5]);

  status = TRUE;

exit:
  g_variant_unref (param_values[0]);
  g_variant_unref (param_values[1]);
  g_variant_unref (param_values[2]);
  g_variant_unref (param_values[3]);
  g_variant_unref (param_values[4]);
  g_variant_unref (param_values[5]);

  return status;
}

/* Функция проверяет параметры образов сигнала. */
gboolean
hyscan_core_params_check_signal_info (HyScanDB *db,
                                      gint32    param_id,
                                      gdouble   data_rate)
{
  const gchar *param_names[5] = {NULL};
  GVariant *param_values[5] = {NULL};
  HyScanDataType data_type;
  gboolean status = FALSE;

  param_names[0] = "/schema/id";
  param_names[1] = "/schema/version";
  param_names[2] = "/data/type";
  param_names[3] = "/data/rate";

  if (!hyscan_db_param_get (db, param_id, NULL, param_names, param_values))
    return FALSE;

  data_type = hyscan_data_get_type_by_name (g_variant_get_string (param_values[2], NULL));

  if ((g_variant_get_int64 (param_values[0]) == SIGNAL_CHANNEL_SCHEMA_ID) &&
      (g_variant_get_int64 (param_values[1]) == SIGNAL_CHANNEL_SCHEMA_VERSION) &&
      (fabs (g_variant_get_double (param_values[3]) - data_rate) < 1.0) &&
      (data_type == HYSCAN_DATA_COMPLEX_FLOAT))
    {
      status = TRUE;
    }

  g_variant_unref (param_values[0]);
  g_variant_unref (param_values[1]);
  g_variant_unref (param_values[2]);
  g_variant_unref (param_values[3]);

  return status;
}

/* Функция проверяет параметры данных ВАРУ. */
gboolean
hyscan_core_params_check_tvg_info (HyScanDB *db,
                                   gint32    param_id,
                                   gdouble   data_rate)
{
  const gchar *param_names[5] = {NULL};
  GVariant *param_values[5] = {NULL};
  HyScanDataType data_type;
  gboolean status = FALSE;

  param_names[0] = "/schema/id";
  param_names[1] = "/schema/version";
  param_names[2] = "/data/type";
  param_names[3] = "/data/rate";

  if (!hyscan_db_param_get (db, param_id, NULL, param_names, param_values))
    return FALSE;

  data_type = hyscan_data_get_type_by_name (g_variant_get_string (param_values[2], NULL));

  if ((g_variant_get_int64 (param_values[0]) == TVG_CHANNEL_SCHEMA_ID) &&
      (g_variant_get_int64 (param_values[1]) == TVG_CHANNEL_SCHEMA_VERSION) &&
      (fabs (g_variant_get_double (param_values[3]) - data_rate) < 1.0) &&
      (data_type == HYSCAN_DATA_FLOAT))
    {
      status = TRUE;
    }

  g_variant_unref (param_values[0]);
  g_variant_unref (param_values[1]);
  g_variant_unref (param_values[2]);
  g_variant_unref (param_values[3]);

  return status;
}
