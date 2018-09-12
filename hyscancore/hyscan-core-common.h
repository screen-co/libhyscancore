/* hyscan-core-common.h
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

#ifndef __HYSCAN_CORE_COMMON_H__
#define __HYSCAN_CORE_COMMON_H__

#include <hyscan-db.h>
#include <hyscan-types.h>
#include "hyscan-core-schemas.h"

typedef enum
{
  HYSCAN_CHANNEL_DATA,
  HYSCAN_CHANNEL_NOISE,
  HYSCAN_CHANNEL_SIGNAL,
  HYSCAN_CHANNEL_TVG
} HyScanChannelType;

HYSCAN_API
gboolean       hyscan_core_params_set_antenna_position         (HyScanDB                  *db,
                                                                gint32                     channel_id,
                                                                HyScanAntennaPosition     *position);

HYSCAN_API
gboolean       hyscan_core_params_set_acoustic_data_info       (HyScanDB                  *db,
                                                                gint32                     channel_id,
                                                                HyScanAcousticDataInfo    *info);

HYSCAN_API
gboolean       hyscan_core_params_set_signal_info              (HyScanDB                  *db,
                                                                gint32                     channel_id,
                                                                gdouble                    data_rate);

HYSCAN_API
gboolean       hyscan_core_params_set_tvg_info                 (HyScanDB                  *db,
                                                                gint32                     channel_id,
                                                                gdouble                    data_rate);

HYSCAN_API
gboolean       hyscan_core_params_check_log_schema             (HyScanDB                  *db,
                                                                gint32                     param_id);

HYSCAN_API
gboolean       hyscan_core_params_load_antenna_position        (HyScanDB                  *db,
                                                                gint32                     param_id,
                                                                gint64                     schema_id,
                                                                gint64                     schema_version,
                                                                HyScanAntennaPosition     *position);

HYSCAN_API
gboolean       hyscan_core_params_load_acoustic_data_info      (HyScanDB                  *db,
                                                                gint32                     param_id,
                                                                HyScanAcousticDataInfo    *info);

HYSCAN_API
gboolean       hyscan_core_params_check_signal_info            (HyScanDB                  *db,
                                                                gint32                     param_id,
                                                                gdouble                    data_rate);

HYSCAN_API
gboolean       hyscan_core_params_check_tvg_info               (HyScanDB                  *db,
                                                                gint32                     param_id,
                                                                gdouble                    data_rate);

HYSCAN_API
const gchar *  hyscan_core_get_channel_name                    (HyScanSourceType           source,
                                                                guint                      channel,
                                                                HyScanChannelType          type);

#endif /* __HYSCAN_CORE_COMMON_H__ */