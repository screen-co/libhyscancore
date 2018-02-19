/*
 * \file hyscan-core-params.h
 *
 * \brief Заголовочный файл вспомогательных функций чтения/записи параметров данных
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#ifndef __HYSCAN_CORE_PARAMS_H__
#define __HYSCAN_CORE_PARAMS_H__

#include <hyscan-db.h>
#include <hyscan-types.h>
#include "hyscan-core-schemas.h"

/* Функция устанавливает местоположение приёмной антенны. */
HYSCAN_API
gboolean       hyscan_core_params_set_antenna_position         (HyScanDB                  *db,
                                                                gint32                     channel_id,
                                                                HyScanAntennaPosition     *position);

/* Функция устанавливает параметры сырых данных. */
HYSCAN_API
gboolean       hyscan_core_params_set_raw_data_info            (HyScanDB                  *db,
                                                                gint32                     channel_id,
                                                                HyScanRawDataInfo         *info);

/* Функция устанавливает параметры акустических данных. */
HYSCAN_API
gboolean       hyscan_core_params_set_acoustic_data_info       (HyScanDB                  *db,
                                                                gint32                     channel_id,
                                                                HyScanAcousticDataInfo    *info);

/* Функция устанавливает параметры образов сигнала. */
HYSCAN_API
gboolean       hyscan_core_params_set_signal_info              (HyScanDB                  *db,
                                                                gint32                     channel_id,
                                                                gdouble                    data_rate);

/* Функция устанавливает параметры данных ВАРУ. */
HYSCAN_API
gboolean       hyscan_core_params_set_tvg_info                 (HyScanDB                  *db,
                                                                gint32                     channel_id,
                                                                gdouble                    data_rate);

/* Функция проверяет схему канала информационных сообщений. */
HYSCAN_API
gboolean       hyscan_core_params_check_log_schema             (HyScanDB                  *db,
                                                                gint32                     param_id);

/* Функция загружает местоположение приёмной антенны. */
HYSCAN_API
gboolean       hyscan_core_params_load_antenna_position        (HyScanDB                  *db,
                                                                gint32                     param_id,
                                                                gint64                     schema_id,
                                                                gint64                     schema_version,
                                                                HyScanAntennaPosition     *position);

/* Функция загружает параметры сырых данных. */
HYSCAN_API
gboolean       hyscan_core_params_load_raw_data_info           (HyScanDB                  *db,
                                                                gint32                     param_id,
                                                                HyScanRawDataInfo         *info);

/* Функция загружает параметры акустических данных. */
HYSCAN_API
gboolean       hyscan_core_params_load_acoustic_data_info      (HyScanDB                  *db,
                                                                gint32                     param_id,
                                                                HyScanAcousticDataInfo    *info);

/* Функция проверяет параметры образов сигнала. */
HYSCAN_API
gboolean       hyscan_core_params_check_signal_info            (HyScanDB                  *db,
                                                                gint32                     param_id,
                                                                gdouble                    data_rate);

/* Функция проверяет параметры данных ВАРУ. */
HYSCAN_API
gboolean       hyscan_core_params_check_tvg_info               (HyScanDB                  *db,
                                                                gint32                     param_id,
                                                                gdouble                    data_rate);

#endif /* __HYSCAN_CORE_PARAMS_H__ */
