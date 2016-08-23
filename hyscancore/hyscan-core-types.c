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

#include <gio/gio.h>
#include <string.h>

#define TRACK_SCHEMA                "track"
#define SENSOR_CHANNEL_SCHEMA       "sensor"

/* Типы галсов и их названия. */
typedef struct
{
  GQuark                       quark;
  const gchar                 *name;

  HyScanTrackType              type;
} HyScanTrackTypeInfo;

/* Типы каналов и их названия. */
typedef struct
{
  GQuark                       quark;
  const gchar                 *name;

  HyScanSourceType             source;
  gboolean                     raw;
  guint                        channel;
} HyScanChannelTypeInfo;

/* Типы галсов и их названия. */
static HyScanTrackTypeInfo hyscan_track_type_info[] =
{
  { 0, "survey", HYSCAN_TRACK_SURVEY },
  { 0, "tack", HYSCAN_TRACK_TACK },
  { 0, "track", HYSCAN_TRACK_TRACK },

  { 0, NULL, HYSCAN_TRACK_UNSPECIFIED }
};

/* Типы каналов и их названия. */
static HyScanChannelTypeInfo hyscan_channel_types_info[] =
{
  { 0, "ss-starboard",         HYSCAN_SOURCE_SIDE_SCAN_STARBOARD,      FALSE, 1 },
  { 0, "ss-starboard-raw",     HYSCAN_SOURCE_SIDE_SCAN_STARBOARD,      TRUE,  1 },
  { 0, "ss-starboard-raw-2",   HYSCAN_SOURCE_SIDE_SCAN_STARBOARD,      TRUE,  2 },
  { 0, "ss-starboard-raw-3",   HYSCAN_SOURCE_SIDE_SCAN_STARBOARD,      TRUE,  3 },

  { 0, "ss-port",              HYSCAN_SOURCE_SIDE_SCAN_PORT,           FALSE, 1 },
  { 0, "ss-port-raw",          HYSCAN_SOURCE_SIDE_SCAN_PORT,           TRUE,  1 },
  { 0, "ss-port-raw-2",        HYSCAN_SOURCE_SIDE_SCAN_PORT,           TRUE,  2 },
  { 0, "ss-port-raw-3",        HYSCAN_SOURCE_SIDE_SCAN_PORT,           TRUE,  3 },

  { 0, "ss-starboard-hi",      HYSCAN_SOURCE_SIDE_SCAN_STARBOARD_HI,   FALSE, 1 },
  { 0, "ss-starboard-hi-raw",  HYSCAN_SOURCE_SIDE_SCAN_STARBOARD_HI,   TRUE,  1 },

  { 0, "ss-port-hi",           HYSCAN_SOURCE_SIDE_SCAN_PORT_HI,        FALSE, 1 },
  { 0, "ss-port-hi-raw",       HYSCAN_SOURCE_SIDE_SCAN_PORT_HI,        TRUE,  1 },

  { 0, "bathy-starboard",      HYSCAN_SOURCE_INTERFEROMETRY_STARBOARD, FALSE, 1 },
  { 0, "bathy-port",           HYSCAN_SOURCE_INTERFEROMETRY_PORT,      FALSE, 1 },

  { 0, "echosounder",          HYSCAN_SOURCE_ECHOSOUNDER,              FALSE, 1 },
  { 0, "echosounder-raw",      HYSCAN_SOURCE_ECHOSOUNDER,              TRUE,  1 },

  { 0, "profiler",             HYSCAN_SOURCE_PROFILER,                 FALSE, 1 },
  { 0, "profiler-raw",         HYSCAN_SOURCE_PROFILER,                 TRUE,  1 },

  { 0, "look-around",          HYSCAN_SOURCE_LOOK_AROUND,              FALSE, 1 },
  { 0, "look-around-2",        HYSCAN_SOURCE_LOOK_AROUND,              FALSE, 2 },
  { 0, "look-around-raw",      HYSCAN_SOURCE_LOOK_AROUND,              TRUE,  1 },
  { 0, "look-around-raw-2",    HYSCAN_SOURCE_LOOK_AROUND,              TRUE,  2 },

  { 0, "forward-look",         HYSCAN_SOURCE_FORWARD_LOOK,             FALSE, 1 },
  { 0, "forward-look-raw-1",   HYSCAN_SOURCE_FORWARD_LOOK,             TRUE,  1 },
  { 0, "forward-look-raw-2",   HYSCAN_SOURCE_FORWARD_LOOK,             TRUE,  2 },

  { 0, "sas",                  HYSCAN_SOURCE_SAS,                      TRUE,  1 },
  { 0, "sas-2",                HYSCAN_SOURCE_SAS,                      TRUE,  2 },
  { 0, "sas-3",                HYSCAN_SOURCE_SAS,                      TRUE,  3 },
  { 0, "sas-4",                HYSCAN_SOURCE_SAS,                      TRUE,  4 },
  { 0, "sas-5",                HYSCAN_SOURCE_SAS,                      TRUE,  5 },

  { 0, "sas-v2",               HYSCAN_SOURCE_SAS_V2,                   TRUE,  1 },
  { 0, "sas-v2-2",             HYSCAN_SOURCE_SAS_V2,                   TRUE,  2 },
  { 0, "sas-v2-3",             HYSCAN_SOURCE_SAS_V2,                   TRUE,  3 },
  { 0, "sas-v2-4",             HYSCAN_SOURCE_SAS_V2,                   TRUE,  4 },
  { 0, "sas-v2-5",             HYSCAN_SOURCE_SAS_V2,                   TRUE,  5 },

  { 0, "nmea",                 HYSCAN_SOURCE_NMEA_ANY,                 TRUE,  1 },
  { 0, "nmea-gga",             HYSCAN_SOURCE_NMEA_GGA,                 TRUE,  1 },
  { 0, "nmea-rmc",             HYSCAN_SOURCE_NMEA_RMC,                 TRUE,  1 },
  { 0, "nmea-dpt",             HYSCAN_SOURCE_NMEA_DPT,                 TRUE,  1 },

  { 0, "nmea-2",               HYSCAN_SOURCE_NMEA_ANY,                 TRUE,  2 },
  { 0, "nmea-gga-2",           HYSCAN_SOURCE_NMEA_GGA,                 TRUE,  2 },
  { 0, "nmea-rmc-2",           HYSCAN_SOURCE_NMEA_RMC,                 TRUE,  2 },
  { 0, "nmea-dpt-2",           HYSCAN_SOURCE_NMEA_DPT,                 TRUE,  2 },

  { 0, "nmea-3",               HYSCAN_SOURCE_NMEA_ANY,                 TRUE,  3 },
  { 0, "nmea-gga-3",           HYSCAN_SOURCE_NMEA_GGA,                 TRUE,  3 },
  { 0, "nmea-rmc-3",           HYSCAN_SOURCE_NMEA_RMC,                 TRUE,  3 },
  { 0, "nmea-dpt-3",           HYSCAN_SOURCE_NMEA_DPT,                 TRUE,  3 },

  { 0, "nmea-4",               HYSCAN_SOURCE_NMEA_ANY,                 TRUE,  4 },
  { 0, "nmea-gga-4",           HYSCAN_SOURCE_NMEA_GGA,                 TRUE,  4 },
  { 0, "nmea-rmc-4",           HYSCAN_SOURCE_NMEA_RMC,                 TRUE,  4 },
  { 0, "nmea-dpt-4",           HYSCAN_SOURCE_NMEA_DPT,                 TRUE,  4 },

  { 0, "nmea-5",               HYSCAN_SOURCE_NMEA_ANY,                 TRUE,  5 },
  { 0, "nmea-gga-5",           HYSCAN_SOURCE_NMEA_GGA,                 TRUE,  5 },
  { 0, "nmea-rmc-5",           HYSCAN_SOURCE_NMEA_RMC,                 TRUE,  5 },
  { 0, "nmea-dpt-5",           HYSCAN_SOURCE_NMEA_DPT,                 TRUE,  5 },

  { 0, NULL,                   HYSCAN_SOURCE_INVALID,                  FALSE, 0 }
};

/* Функция инициализации статических данных. */
static void
hyscan_core_types_initialize (void)
{
  static gboolean hyscan_core_types_initialized = FALSE;
  guint i;

  if (hyscan_core_types_initialized)
    return;

  for (i = 0; hyscan_track_type_info[i].name != NULL; i++)
    hyscan_track_type_info[i].quark = g_quark_from_static_string (hyscan_track_type_info[i].name);

  for (i = 0; hyscan_channel_types_info[i].name != NULL; i++)
    hyscan_channel_types_info[i].quark = g_quark_from_static_string (hyscan_channel_types_info[i].name);

  hyscan_core_types_initialized = TRUE;
}

/* Функция проверяет тип источника данных на соответствие одному из типов датчиков. */
gboolean
hyscan_source_is_sensor (HyScanSourceType source)
{
  switch (source)
    {
    case HYSCAN_SOURCE_SAS:
    case HYSCAN_SOURCE_SAS_V2:
    case HYSCAN_SOURCE_NMEA_ANY:
    case HYSCAN_SOURCE_NMEA_GGA:
    case HYSCAN_SOURCE_NMEA_RMC:
    case HYSCAN_SOURCE_NMEA_DPT:
      return TRUE;

    default:
      return FALSE;
    }

  return FALSE;
}

/* Функция проверяет тип источника данных на соответствие акустическим данным. */
gboolean
hyscan_source_is_acoustic (HyScanSourceType source,
                           gboolean         raw)
{
  switch (source)
    {
    case HYSCAN_SOURCE_SIDE_SCAN_STARBOARD:
    case HYSCAN_SOURCE_SIDE_SCAN_PORT:
    case HYSCAN_SOURCE_SIDE_SCAN_STARBOARD_HI:
    case HYSCAN_SOURCE_SIDE_SCAN_PORT_HI:
    case HYSCAN_SOURCE_ECHOSOUNDER:
    case HYSCAN_SOURCE_PROFILER:
    case HYSCAN_SOURCE_LOOK_AROUND:
      return TRUE;

    /* Для вперёдсмотрящего только "сырые" данные являются акустическими. */
    case HYSCAN_SOURCE_FORWARD_LOOK:
      if (raw)
        return TRUE;
      return FALSE;

    default:
      return FALSE;
    }

  return FALSE;
}

/* Функция возвращает название типа галса. */
const gchar *
hyscan_track_get_name_by_type (HyScanTrackType type)
{
  guint i;

  /* Инициализация статических данных. */
  hyscan_core_types_initialize ();

  /* Ищем название типа. */
  for (i = 0; hyscan_track_type_info[i].quark != 0; i++)
    {
      if (hyscan_track_type_info[i].type != type)
        continue;
      return hyscan_track_type_info[i].name;
    }

  return NULL;
}

/* Функция возвращает тип галса по его названию. */
HyScanTrackType
hyscan_track_get_type_by_name (const gchar *name)
{
  GQuark quark;
  guint i;

  /* Инициализация статических данных. */
  hyscan_core_types_initialize ();

  /* Ищем тип по названию. */
  quark = g_quark_try_string (name);
  for (i = 0; hyscan_track_type_info[i].quark != 0; i++)
    if (hyscan_track_type_info[i].quark == quark)
      return hyscan_track_type_info[i].type;

  return HYSCAN_TRACK_UNSPECIFIED;
}

/* Функция возвращает название канала для указанных характеристик. */
const gchar *
hyscan_channel_get_name_by_types (HyScanSourceType source,
                                  gboolean         raw,
                                  guint            channel)
{
  guint i;

  /* Инициализация статических данных. */
  hyscan_core_types_initialize ();

  /* Ищем название канала для указанных характеристик. */
  for (i = 0; hyscan_channel_types_info[i].quark != 0; i++)
    {
      if (hyscan_channel_types_info[i].source != source)
        continue;
      if (hyscan_channel_types_info[i].raw != raw)
        continue;
      if (hyscan_channel_types_info[i].channel != channel)
        continue;
      return hyscan_channel_types_info[i].name;
    }

  return NULL;
}

/* Функция возвращает характеристики канала данных по его имени. */
gboolean
hyscan_channel_get_types_by_name (const gchar      *name,
                                  HyScanSourceType *source,
                                  gboolean         *raw,
                                  guint            *channel)
{
  GQuark quark;
  guint i;

  /* Инициализация статических данных. */
  hyscan_core_types_initialize ();

  /* Ищем канал с указанным именем. */
  quark = g_quark_try_string (name);
  for (i = 0; hyscan_channel_types_info[i].quark != 0; i++)
    {
      if (hyscan_channel_types_info[i].quark == quark)
        {
          if (source != NULL)
            *source = hyscan_channel_types_info[i].source;
          if (raw != NULL)
            *raw = hyscan_channel_types_info[i].raw;
          if (channel != NULL)
            *channel = hyscan_channel_types_info[i].channel;
          return TRUE;
        }
    }

  return FALSE;
}

/* Функция создаёт галс в системе хранения. */
gboolean
hyscan_track_create (HyScanDB        *db,
                     const gchar     *project_name,
                     const gchar     *track_name,
                     HyScanTrackType  track_type)
{
  gint32 project_id = -1;
  gint32 track_id = -1;
  gint32 param_id = -1;

  const gchar *track_type_name;
  GBytes *schema;

  gboolean status = FALSE;

  schema = g_resources_lookup_data ("/org/hyscan/schemas/track-schema.xml", G_RESOURCE_LOOKUP_FLAGS_NONE, NULL);
  if (schema == NULL)
    {
      g_warning ("HyScanCore: can't find track schema");
      return FALSE;
    }

  project_id = hyscan_db_project_open (db, project_name);
  if (project_id <= 0)
    goto exit;

  track_id = hyscan_db_track_create (db, project_id, track_name, g_bytes_get_data (schema, NULL), TRACK_SCHEMA);
  if (track_id <= 0)
    goto exit;

  param_id = hyscan_db_track_param_open (db, track_id);
  if (param_id <= 0)
    goto exit;

  track_type_name = hyscan_track_get_name_by_type (track_type);
  if (track_type_name != NULL)
    if (!hyscan_db_param_set_string (db, param_id, NULL, "/type", track_type_name))
      goto exit;

  status = TRUE;

exit:
  g_clear_pointer (&schema, g_bytes_unref);
  if (project_id > 0)
    hyscan_db_close (db, project_id);
  if (track_id > 0)
    hyscan_db_close (db, track_id);
  if (param_id > 0)
    hyscan_db_close (db, param_id);

  return status;
}

gint32
hyscan_channel_sensor_create (HyScanDB                *db,
                              const gchar             *project_name,
                              const gchar             *track_name,
                              const gchar             *channel_name,
                              HyScanAntennaPosition   *position)
{
  gint32 project_id = -1;
  gint32 track_id = -1;
  gint32 channel_id = -1;
  gint32 param_id = -1;

  gboolean status = FALSE;

  project_id = hyscan_db_project_open (db, project_name);
  if (project_id <= 0)
    goto exit;

  track_id = hyscan_db_track_open (db, project_id, track_name);
  if (track_id <= 0)
    goto exit;

  channel_id = hyscan_db_channel_create (db, track_id, channel_name, SENSOR_CHANNEL_SCHEMA);
  if (channel_id <= 0)
    goto exit;

  param_id = hyscan_db_channel_param_open (db, channel_id);
  if (param_id <= 0)
    goto exit;

  if (!hyscan_db_param_set_double (db, param_id, NULL, "/position/x", position->x))
    goto exit;

  if (!hyscan_db_param_set_double (db, param_id, NULL, "/position/y", position->y))
    goto exit;

  if (!hyscan_db_param_set_double (db, param_id, NULL, "/position/z", position->z))
    goto exit;

  if (!hyscan_db_param_set_double (db, param_id, NULL, "/orientation/psi", position->psi))
    goto exit;

  if (!hyscan_db_param_set_double (db, param_id, NULL, "/orientation/gamma", position->gamma))
    goto exit;

  if (!hyscan_db_param_set_double (db, param_id, NULL, "/orientation/theta", position->theta))
    goto exit;

  status = TRUE;

exit:
  if (project_id > 0)
    hyscan_db_close (db, project_id);
  if (track_id > 0)
    hyscan_db_close (db, track_id);
  if (param_id > 0)
    hyscan_db_close (db, param_id);

  if (!status && channel_id > 0)
    {
      hyscan_db_close (db, channel_id);
      channel_id = -1;
    }

  return channel_id;
}
