/*
 * \file hyscan-core-types.c
 *
 * \brief Исходный файл вспомогательных функций с определениями типов данных HyScan
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
*/

#include "hyscan-core-types.h"

#include <string.h>

/* Типы каналов и их названия. */
typedef struct
{
  GQuark                       quark;
  const gchar                 *name;

  HyScanSonarDataType          data_type;
  gboolean                     hi_res;
  gboolean                     raw;
  gint                         index;
} HyScanChannelTypeInfo;

/* Типы каналов и их названия. */
static HyScanChannelTypeInfo hyscan_channel_types_info[] =
{
  { 0, "echo-sounder",     HYSCAN_SONAR_DATA_ECHOSOUNDER, FALSE, FALSE, HYSCAN_SONAR_CHANNEL_1 },
  { 0, "echo-sounder-raw", HYSCAN_SONAR_DATA_ECHOSOUNDER, FALSE, TRUE,  HYSCAN_SONAR_CHANNEL_1 },

  { 0, "ss-right",         HYSCAN_SONAR_DATA_SS_STARBOARD, FALSE, FALSE, HYSCAN_SONAR_CHANNEL_1 },
  { 0, "ss-right-hi",      HYSCAN_SONAR_DATA_SS_STARBOARD, TRUE,  FALSE, HYSCAN_SONAR_CHANNEL_1 },
  { 0, "ss-right-raw",     HYSCAN_SONAR_DATA_SS_STARBOARD, FALSE, TRUE,  HYSCAN_SONAR_CHANNEL_1 },
  { 0, "ss-right-hi-raw",  HYSCAN_SONAR_DATA_SS_STARBOARD, TRUE,  TRUE,  HYSCAN_SONAR_CHANNEL_1 },

  { 0, "ss-left",          HYSCAN_SONAR_DATA_SS_PORT, FALSE, FALSE, HYSCAN_SONAR_CHANNEL_1 },
  { 0, "ss-left-hi",       HYSCAN_SONAR_DATA_SS_PORT, TRUE,  FALSE, HYSCAN_SONAR_CHANNEL_1 },
  { 0, "ss-left-raw",      HYSCAN_SONAR_DATA_SS_PORT, FALSE, TRUE,  HYSCAN_SONAR_CHANNEL_1 },
  { 0, "ss-left-hi-raw",   HYSCAN_SONAR_DATA_SS_PORT, TRUE,  TRUE,  HYSCAN_SONAR_CHANNEL_1 },

  { 0, "sas",              HYSCAN_SONAR_DATA_SAS, FALSE, FALSE, HYSCAN_SONAR_CHANNEL_1 },
  { 0, "sas-2",            HYSCAN_SONAR_DATA_SAS, FALSE, FALSE, HYSCAN_SONAR_CHANNEL_2 },
  { 0, "sas-3",            HYSCAN_SONAR_DATA_SAS, FALSE, FALSE, HYSCAN_SONAR_CHANNEL_3 },
  { 0, "sas-4",            HYSCAN_SONAR_DATA_SAS, FALSE, FALSE, HYSCAN_SONAR_CHANNEL_4 },
  { 0, "sas-5",            HYSCAN_SONAR_DATA_SAS, FALSE, FALSE, HYSCAN_SONAR_CHANNEL_5 },

  { 0, "nmea",             HYSCAN_SONAR_DATA_NMEA_ANY, FALSE, FALSE, HYSCAN_SONAR_CHANNEL_1 },
  { 0, "nmea-gga",         HYSCAN_SONAR_DATA_NMEA_GGA, FALSE, FALSE, HYSCAN_SONAR_CHANNEL_1 },
  { 0, "nmea-rmc",         HYSCAN_SONAR_DATA_NMEA_RMC, FALSE, FALSE, HYSCAN_SONAR_CHANNEL_1 },
  { 0, "nmea-dpt",         HYSCAN_SONAR_DATA_NMEA_DPT, FALSE, FALSE, HYSCAN_SONAR_CHANNEL_1 },

  { 0, "nmea-2",           HYSCAN_SONAR_DATA_NMEA_ANY, FALSE, FALSE, HYSCAN_SONAR_CHANNEL_2 },
  { 0, "nmea-gga-2",       HYSCAN_SONAR_DATA_NMEA_GGA, FALSE, FALSE, HYSCAN_SONAR_CHANNEL_2 },
  { 0, "nmea-rmc-2",       HYSCAN_SONAR_DATA_NMEA_RMC, FALSE, FALSE, HYSCAN_SONAR_CHANNEL_2 },
  { 0, "nmea-dpt-2",       HYSCAN_SONAR_DATA_NMEA_DPT, FALSE, FALSE, HYSCAN_SONAR_CHANNEL_2 },

  { 0, "nmea-3",           HYSCAN_SONAR_DATA_NMEA_ANY, FALSE, FALSE, HYSCAN_SONAR_CHANNEL_3 },
  { 0, "nmea-gga-3",       HYSCAN_SONAR_DATA_NMEA_GGA, FALSE, FALSE, HYSCAN_SONAR_CHANNEL_3 },
  { 0, "nmea-rmc-3",       HYSCAN_SONAR_DATA_NMEA_RMC, FALSE, FALSE, HYSCAN_SONAR_CHANNEL_3 },
  { 0, "nmea-dpt-3",       HYSCAN_SONAR_DATA_NMEA_DPT, FALSE, FALSE, HYSCAN_SONAR_CHANNEL_3 },

  { 0, "nmea-4",           HYSCAN_SONAR_DATA_NMEA_ANY, FALSE, FALSE, HYSCAN_SONAR_CHANNEL_4 },
  { 0, "nmea-gga-4",       HYSCAN_SONAR_DATA_NMEA_GGA, FALSE, FALSE, HYSCAN_SONAR_CHANNEL_4 },
  { 0, "nmea-rmc-4",       HYSCAN_SONAR_DATA_NMEA_RMC, FALSE, FALSE, HYSCAN_SONAR_CHANNEL_4 },
  { 0, "nmea-dpt-4",       HYSCAN_SONAR_DATA_NMEA_DPT, FALSE, FALSE, HYSCAN_SONAR_CHANNEL_4 },

  { 0, "nmea-5",           HYSCAN_SONAR_DATA_NMEA_ANY, FALSE, FALSE, HYSCAN_SONAR_CHANNEL_5 },
  { 0, "nmea-gga-5",       HYSCAN_SONAR_DATA_NMEA_GGA, FALSE, FALSE, HYSCAN_SONAR_CHANNEL_5 },
  { 0, "nmea-rmc-5",       HYSCAN_SONAR_DATA_NMEA_RMC, FALSE, FALSE, HYSCAN_SONAR_CHANNEL_5 },
  { 0, "nmea-dpt-5",       HYSCAN_SONAR_DATA_NMEA_DPT, FALSE, FALSE, HYSCAN_SONAR_CHANNEL_5 },

  { 0, NULL, HYSCAN_SONAR_DATA_INVALID, FALSE, FALSE, 0 }
};

/* Функция инициализации статических данных. */
static void
hyscan_core_types_initialize (void)
{
  static gboolean hyscan_core_types_initialized = FALSE;
  gint i;

  if (hyscan_core_types_initialized)
    return;

  for (i = 0; hyscan_channel_types_info[i].name != NULL; i++)
    hyscan_channel_types_info[i].quark = g_quark_from_static_string (hyscan_channel_types_info[i].name);

  hyscan_core_types_initialized = TRUE;
}

/* Функция возвращает название канала для указанных характеристик. */
const gchar *
hyscan_channel_get_name_by_types (HyScanSonarDataType  data_type,
                                  gboolean             hi_res,
                                  gboolean             raw,
                                  gint                 index)
{
  gint i;

  /* Инициализация статических данных. */
  hyscan_core_types_initialize ();

  /* Ищем название канала для указанных характеристик. */
  for (i = 0; hyscan_channel_types_info[i].quark != 0; i++)
    {
      if (hyscan_channel_types_info[i].data_type != data_type)
        continue;
      if (hyscan_channel_types_info[i].hi_res != hi_res)
        continue;
      if (hyscan_channel_types_info[i].raw != raw)
        continue;
      if (hyscan_channel_types_info[i].index != index)
        continue;
      return hyscan_channel_types_info[i].name;
    }

  return NULL;
}

/* Функция возвращает характеристики канала данных по его имени. */
gboolean
hyscan_channel_get_types_by_name (const gchar          *channel_name,
                                  HyScanSonarDataType  *data_type,
                                  gboolean             *hi_res,
                                  gboolean             *raw,
                                  gint                 *index)
{
  GQuark quark;
  gint i;

  /* Инициализация статических данных. */
  hyscan_core_types_initialize ();

  /* Ищем канал с указанным именем. */
  quark = g_quark_try_string (channel_name);
  for (i = 0; hyscan_channel_types_info[i].quark != 0; i++)
    {
      if (hyscan_channel_types_info[i].quark == quark)
        {
          if (data_type != NULL)
            *data_type = hyscan_channel_types_info[i].data_type;
          if (hi_res != NULL)
            *hi_res = hyscan_channel_types_info[i].hi_res;
          if (raw != NULL)
            *raw = hyscan_channel_types_info[i].raw;
          if (index != NULL)
            *index = hyscan_channel_types_info[i].index;
          return TRUE;
        }
    }

  return FALSE;
}
