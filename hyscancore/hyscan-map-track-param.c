/* hyscan-map-track-param.c
 *
 * Copyright 2020 Screen LLC, Alexey Sakhnov <alexsakhnov@gmail.com>
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

/**
 * SECTION: hyscan-map-track-param
 * @Short_description: параметры обработки данных при проекции точек галса на карту
 * @Title: HyScanMapTrackParam
 *
 * Класс реализует интерфейс #HyScanParam и позволяет управлять параметрами
 * источников данных галса.
 *
 * Параметры обработки могут быть заданы индивидуально для каждого галса.
 *
 * Класс содержит следующие параметры:
 * - /channel-rmc - номер канала для чтения строк RMC
 * - /channel-dpt - номер канала для чтения строк DPT
 * - /channe-port - включено отображения левого борта ГБО
 * - /channe-starboard - включено отображения правого борта ГБО
 * - /quality - минимальное значение качества акустических данных, которые необходимо отображать
 *
 * Чтобы сбросить параметры обработки (например, чтобы использовать в галсе
 * общие параметры проекта) используется функция hyscan_map_track_param_clear().
 *
 * Для отслеживания изменений параметра можно использовать функцию
 * hyscan_map_track_param_get_mod_count().
 *
 * В проекте может присутствовать несколько профилей настроек, например,
 * исходя из пользовательских предпочтений или решаемых задач. Название профиля
 * передаётся при создании объекта #HyScanMapTrackParam.
 *
 */

#include "hyscan-map-track-param.h"
#include "hyscan-core-schemas.h"
#include "hyscan-core-common.h"
#include <hyscan-data-schema-builder.h>
#include <glib/gi18n-lib.h>

#define GROUP_NAME "map-track"     /* Суффикс группы параметров проекта. */

#define ENUM_NMEA_CHANNEL         "nmea-channel"
#define KEY_SENSOR_RMC            "/sensor-rmc"
#define KEY_SENSOR_DPT            "/sensor-dpt"
#define KEY_CHANNEL_RMC           "/channel-rmc"
#define KEY_CHANNEL_DPT           "/channel-dpt"
#define KEY_CHANNEL_PORT          "/channel-port"
#define KEY_CHANNEL_STARBOARD     "/channel-starboard"
#define KEY_TARGET_QUALITY        "/quality"

/* Параметры по умолчанию. */
#define NAME_CHANNEL_RMC          "gnss"           /* Подстрока в названии датчика навигации. */
#define NAME_CHANNEL_DPT          "echosounder"    /* Подстрока в названии датчика эхолота. */
#define DEFAULT_QUALITY           0.5              /* Минимальное значение качества акустических данных. */

enum
{
  PROP_O,
  PROP_DB,
  PROP_PROJECT_NAME,
  PROP_TRACK_NAME,
  PROP_PROFILE,
};

struct _HyScanMapTrackParamPrivate
{
  HyScanDB             *db;              /* База данных. */
  HyScanMapTrackParam  *defaults;         /* Параметры по-умолчанию. */
  gchar                *profile;         /* Название профиля настроек. */
  gchar                *project_name;    /* Название проекта. */
  gchar                *track_name;      /* Название галса. */
  gint32                param_id;        /* Ид группы параметров проекта. */
  gchar                *object_name;     /* Название объекта в группе параметров, где хранятся настройки. */

  gint                  rmc_default;     /* Номер канала NMEA для чтения RMC. */
  gint                  dpt_default;     /* Номер канала NMEA для чтения DPT. */
  HyScanDataSchema     *schema;          /* Схема данных. */
};

static void          hyscan_map_track_param_interface_init           (HyScanParamInterface     *iface);
static void          hyscan_map_track_param_set_property             (GObject                  *object,
                                                                      guint                     prop_id,
                                                                      const GValue             *value,
                                                                      GParamSpec               *pspec);
static void          hyscan_map_track_param_object_constructed       (GObject                  *object);
static void          hyscan_map_track_param_object_finalize          (GObject                  *object);
static gchar *       hyscan_map_track_param_object_name              (HyScanMapTrackParam      *param,
                                                                      gint32                    project_id);
static void          hyscan_map_track_param_object_create            (HyScanMapTrackParam      *param);
static void          hyscan_map_track_param_schema_build             (HyScanMapTrackParam      *param);
static void          hyscan_map_track_param_schema_build_nmea_enum   (HyScanMapTrackParam      *param,
                                                                      HyScanDataSchemaBuilder  *builder);
static const gchar * hyscan_map_track_param_nmea_sensor              (HyScanMapTrackParam      *param,
                                                                      gint64                    channel_id);
static gint64        hyscan_map_track_param_nmea_channel             (HyScanMapTrackParam      *param,
                                                                      const gchar              *sensor);
static const gchar * hyscan_map_track_param_user_to_db               (HyScanMapTrackParam      *param,
                                                                      const gchar              *user_name,
                                                                      GVariant                 *user_value,
                                                                      GVariant                **db_value);
static const gchar * hyscan_map_track_param_db_to_user               (HyScanMapTrackParam      *param,
                                                                      const gchar              *db_name,
                                                                      GVariant                 *db_value,
                                                                      GVariant                **user_value);
static gboolean      hyscan_map_track_param_get_defaults             (HyScanMapTrackParam      *param,
                                                                      HyScanParamList          *list);

G_DEFINE_TYPE_WITH_CODE (HyScanMapTrackParam, hyscan_map_track_param, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (HyScanMapTrackParam)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_PARAM, hyscan_map_track_param_interface_init))

static void
hyscan_map_track_param_class_init (HyScanMapTrackParamClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_map_track_param_set_property;

  object_class->constructed = hyscan_map_track_param_object_constructed;
  object_class->finalize = hyscan_map_track_param_object_finalize;

  g_object_class_install_property (object_class, PROP_DB,
                                   g_param_spec_object ("db", "DB", "HyScanDB interface", HYSCAN_TYPE_DB,
                                                        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_PROJECT_NAME,
                                   g_param_spec_string ("project-name", "ProjectName", "Project name", NULL,
                                                        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_TRACK_NAME,
                                   g_param_spec_string ("track-name", "TrackName", "Track name", NULL,
                                                        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_PROFILE,
                                   g_param_spec_string ("profile", "Profile", "Profile name", NULL,
                                                        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_map_track_param_init (HyScanMapTrackParam *param)
{
  param->priv = hyscan_map_track_param_get_instance_private (param);
}

static void
hyscan_map_track_param_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  HyScanMapTrackParam *param = HYSCAN_MAP_TRACK_PARAM (object);
  HyScanMapTrackParamPrivate *priv = param->priv;

  switch (prop_id)
    {
    case PROP_PROJECT_NAME:
      priv->project_name = g_value_dup_string (value);
      break;

    case PROP_TRACK_NAME:
      priv->track_name = g_value_dup_string (value);
      break;

    case PROP_PROFILE:
      priv->profile = g_value_dup_string (value);
      break;

    case PROP_DB:
      priv->db = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_map_track_param_object_constructed (GObject *object)
{
  HyScanMapTrackParam *param = HYSCAN_MAP_TRACK_PARAM (object);
  HyScanMapTrackParamPrivate *priv = param->priv;
  gint32 project_id;
  gchar *group_name = NULL;

  G_OBJECT_CLASS (hyscan_map_track_param_parent_class)->constructed (object);

  /* Открываем проект. */
  project_id = hyscan_db_project_open (priv->db, priv->project_name);
  if (project_id < 0)
    {
      g_warning ("HyScanMapTrackParam: failed to open project '%s'", priv->project_name);
      goto exit;
    }

  /* Открываем группу параметров в проекте. */
  group_name = priv->profile != NULL ? g_strdup_printf ("%s_" GROUP_NAME, priv->profile) : g_strdup (GROUP_NAME);
  priv->param_id = hyscan_db_project_param_open (priv->db, project_id, group_name);
  if (priv->param_id < 0)
    {
      g_warning ("HyScanMapTrackParam: failed to open project param group %s/%s", priv->project_name, group_name);
      goto exit;
    }

  /* Формируем имя объекта, где будут храниться параметры. */
  priv->object_name = hyscan_map_track_param_object_name (param, project_id);
  if (priv->object_name == NULL)
    goto exit;

  //  todo: надо решить вопрос, как формировать enum каналов для всего проекта

  /* Создаём схему данных. */
  hyscan_map_track_param_schema_build (param);

exit:
  if (project_id > 0)
    hyscan_db_close (priv->db, project_id);
  g_free (group_name);
}

static void
hyscan_map_track_param_object_finalize (GObject *object)
{
  HyScanMapTrackParam *param = HYSCAN_MAP_TRACK_PARAM (object);
  HyScanMapTrackParamPrivate *priv = param->priv;

  if (priv->param_id >= 0)
    hyscan_db_close (priv->db, priv->param_id);

  g_free (priv->project_name);
  g_free (priv->track_name);
  g_free (priv->object_name);
  g_free (priv->profile);
  g_clear_object (&priv->db);
  g_clear_object (&priv->defaults);
  g_clear_object (&priv->schema);

  G_OBJECT_CLASS (hyscan_map_track_param_parent_class)->finalize (object);
}

/* Формирует имя объекта, где хранятся параметры галса. */
static gchar *
hyscan_map_track_param_object_name (HyScanMapTrackParam *param,
                                    gint32               project_id)
{
  HyScanMapTrackParamPrivate *priv = param->priv;
  HyScanParamList *list = NULL;
  gint32 track_id = -1, track_param_id = -1;
  gchar *object_name = NULL;

  /* Параметры для всех галсов по умолчанию пишем в объект с именем "default". */
  if (priv->track_name == NULL)
    return g_strdup ("default");

  /* Для галса считываем его идентификатор из параметров галса. */
  track_id = hyscan_db_track_open (priv->db, project_id, priv->track_name);
  if (track_id < 0)
    {
      g_warning ("HyScanMapTrackParam: failed to open track %s/%s", priv->project_name, priv->track_name);
      goto exit;
    }

  track_param_id = hyscan_db_track_param_open (priv->db, track_id);
  if (track_param_id < 0)
    {
      g_warning ("HyScanMapTrackParam: failed to open track param %s/%s", priv->project_name, priv->track_name);
      goto exit;
    }

  list = hyscan_param_list_new ();
  hyscan_param_list_add (list, "/id");
  if (hyscan_db_param_get (priv->db, track_param_id, NULL, list))
    object_name = g_strdup_printf ("tracks/%s", hyscan_param_list_get_string (list, "/id"));
  else
    g_warning ("HyScanMapTrackParam: failed to get track id of %s", priv->track_name);

exit:
  if (track_id > 0)
    hyscan_db_close (priv->db, track_id);

  if (track_param_id > 0)
    hyscan_db_close (priv->db, track_param_id);

  g_clear_object (&list);

  return object_name;
}

/* Функция создаёт объект в группе параметров, если он не существует. */
static void
hyscan_map_track_param_object_create (HyScanMapTrackParam *param)
{
  HyScanMapTrackParamPrivate *priv = param->priv;
  HyScanDataSchema *schema;

  schema = hyscan_db_param_object_get_schema (priv->db, priv->param_id, priv->object_name);
  if (schema == NULL)
    hyscan_db_param_object_create (priv->db, priv->param_id, priv->object_name, MAP_TRACK_SCHEMA);

  g_clear_object (&schema);
}

/* Создаёт перечисление датчиков NMEA в схеме данных. */
static void
hyscan_map_track_param_schema_build_nmea_enum (HyScanMapTrackParam     *param,
                                               HyScanDataSchemaBuilder *builder)
{
  HyScanMapTrackParamPrivate *priv = param->priv;

  gint32 project_id = -1, track_id = -1;
  gchar **channels = NULL;

  guint i;

  /* Создаём перечисление и "пустое" значение. */
  hyscan_data_schema_builder_enum_create (builder, ENUM_NMEA_CHANNEL);
  hyscan_data_schema_builder_enum_value_create (builder, ENUM_NMEA_CHANNEL, 0, "", N_("Disabled"), NULL);

  /* Получаем из БД информацию о каналах галса. */
  project_id = hyscan_db_project_open (priv->db, priv->project_name);
  if (project_id < 0)
    goto exit;

  track_id = hyscan_db_track_open (priv->db, project_id, priv->track_name);
  if (track_id < 0)
    goto exit;

  channels = hyscan_db_channel_list (priv->db, track_id);

  if (channels == NULL)
    goto exit;

  for (i = 0; channels[i] != NULL; ++i)
    {
      gchar *sensor_name;
      gint32 channel_id, param_id;

      HyScanSourceType source;
      HyScanChannelType type;
      guint channel_num;

      if (!hyscan_channel_get_types_by_id (channels[i], &source, &type, &channel_num))
        continue;

      if (source != HYSCAN_SOURCE_NMEA || type != HYSCAN_CHANNEL_DATA)
        continue;

      channel_id = hyscan_db_channel_open (priv->db, track_id, channels[i]);
      if (channel_id < 0)
        continue;

      param_id = hyscan_db_channel_param_open (priv->db, channel_id);
      hyscan_db_close (priv->db, channel_id);

      if (param_id < 0)
        continue;

      sensor_name = hyscan_core_params_load_sensor_info (priv->db, param_id);
      hyscan_db_close (priv->db, param_id);

      /* По умолчанию устанавливаем номера каналов по названию датчиков. */
      if (g_strstr_len (sensor_name, -1, NAME_CHANNEL_DPT) != NULL)
        priv->dpt_default = channel_num;
      else if (g_strstr_len (sensor_name, -1, NAME_CHANNEL_RMC) != NULL)
        priv->rmc_default = channel_num;

      /* Добавляем канал в enum. В качестве идентификатора ставим имя датчика,
       * поскольку нас интересует именно датчик, а не номер канала. */
      hyscan_data_schema_builder_enum_value_create (builder, ENUM_NMEA_CHANNEL, channel_num, sensor_name, sensor_name, NULL);

      g_free (sensor_name);
    }

exit:
  g_clear_pointer (&channels, g_strfreev);

  if (track_id > 0)
    hyscan_db_close (priv->db, track_id);

  if (project_id > 0)
    hyscan_db_close (priv->db, project_id);
}

/* Создаёт схему данных параметров галса. */
static void
hyscan_map_track_param_schema_build (HyScanMapTrackParam *param)
{
  HyScanMapTrackParamPrivate *priv = param->priv;
  HyScanDataSchemaBuilder *builder;

  builder = hyscan_data_schema_builder_new_with_gettext ("gtk-map-track", GETTEXT_PACKAGE);
  hyscan_data_schema_builder_node_set_name (builder, "/", N_("Track settings"), N_("Configure track channel data"));\

  // todo: read default values from db schema

  /* Формируем перечисление NMEA-каналов. */
  hyscan_map_track_param_schema_build_nmea_enum (param, builder);

  /* Настройка канала RMC-строк. */
  hyscan_data_schema_builder_key_enum_create (builder, KEY_CHANNEL_RMC, N_("RMC Channel"),
                                              N_("The NMEA-channel with RMC sentences"),
                                              ENUM_NMEA_CHANNEL, priv->rmc_default);

  /* Настройка канала DPT-строк. */
  hyscan_data_schema_builder_key_enum_create (builder, KEY_CHANNEL_DPT, N_("DPT Channel"),
                                              N_("The NMEA-channel with DPT sentences"),
                                              ENUM_NMEA_CHANNEL, priv->dpt_default);

  /* Настройка левого борта. */
  hyscan_data_schema_builder_key_boolean_create (builder, KEY_CHANNEL_PORT, N_("Port Channel"),
                                                 N_("Show side-scan port channel data"), TRUE);

  /* Настройка правого борта. */
  hyscan_data_schema_builder_key_boolean_create (builder, KEY_CHANNEL_STARBOARD, N_("Starboard Channel"),
                                                 N_("Show side-scan starboard channel data"), TRUE);

  /* Целевое качество покрытия. */
  hyscan_data_schema_builder_key_double_create (builder, KEY_TARGET_QUALITY, N_("Target Quality"),
                                                N_("Minimum quality to display"), DEFAULT_QUALITY);
  hyscan_data_schema_builder_key_double_range (builder, KEY_TARGET_QUALITY, 0.0, 1.0, 0.1);

  /* Записываем полученную схему данных. */
  priv->schema = hyscan_data_schema_builder_get_schema (builder);

  g_object_unref (builder);
}

/* Получает имя датчика NMEA по номеру канала. */
static const gchar *
hyscan_map_track_param_nmea_sensor (HyScanMapTrackParam *param,
                                    gint64               channel_id)
{
  HyScanMapTrackParamPrivate *priv = param->priv;
  const HyScanDataSchemaEnumValue *value;

  value = hyscan_data_schema_enum_find_by_value (priv->schema, ENUM_NMEA_CHANNEL, channel_id);
  if (value == NULL)
    return NULL;
  
  return value->id;
}

/* Получает номер канала по имени датчика NMEA. */
static gint64
hyscan_map_track_param_nmea_channel (HyScanMapTrackParam *param,
                                     const gchar         *sensor)
{
  HyScanMapTrackParamPrivate *priv = param->priv;
  const HyScanDataSchemaEnumValue *value;

  if (sensor == NULL)
    return 0;

  value = hyscan_data_schema_enum_find_by_id (priv->schema, ENUM_NMEA_CHANNEL, sensor);
  if (value == NULL)
    return 0;

  return value->value;
}

/* Переводит имя и значение пользоватльского параметра в соответствующий параметр в БД. */
static const gchar *
hyscan_map_track_param_user_to_db (HyScanMapTrackParam   *param,
                                   const gchar           *user_name,
                                   GVariant              *user_value,
                                   GVariant             **db_value)
{
  if (0 == g_strcmp0 (user_name, KEY_CHANNEL_DPT))
    {
      if (db_value != NULL)
        *db_value = g_variant_new_string (hyscan_map_track_param_nmea_sensor (param, g_variant_get_int64 (user_value)));

      return KEY_SENSOR_DPT;
    }

  else if (0 == g_strcmp0 (user_name, KEY_CHANNEL_RMC))
    {
      if (db_value != NULL)
        *db_value = g_variant_new_string (hyscan_map_track_param_nmea_sensor (param, g_variant_get_int64 (user_value)));

      return KEY_SENSOR_RMC;
    }

  else if (0 == g_strcmp0 (user_name, KEY_CHANNEL_PORT) ||
           0 == g_strcmp0 (user_name, KEY_CHANNEL_STARBOARD) ||
           0 == g_strcmp0 (user_name, KEY_TARGET_QUALITY))
    {
      if (db_value != NULL)
        *db_value = g_variant_ref (user_value);

      return user_name;
    }

  g_warning ("HyScanMapTrackParam: unknown key %s", user_name);

  return NULL;
}

static const gchar *
hyscan_map_track_param_db_to_user (HyScanMapTrackParam   *param,
                                   const gchar           *db_name,
                                   GVariant              *db_value,
                                   GVariant             **user_value)
{
  HyScanMapTrackParamPrivate *priv = param->priv;

  if (0 == g_strcmp0 (db_name, KEY_SENSOR_DPT))
    {
      if (user_value != NULL)
        {
          const gchar *string_value = db_value != NULL ? g_variant_get_string (db_value, NULL) : NULL;
          gint64 int_value;
          int_value = string_value != NULL ?
                      hyscan_map_track_param_nmea_channel (param, string_value) :
                      priv->dpt_default;
          *user_value = g_variant_new_int64 (int_value);
        }

      return KEY_CHANNEL_DPT;
    }

  else if (0 == g_strcmp0 (db_name, KEY_SENSOR_RMC))
    {
      if (user_value != NULL)
        {
          const gchar *string_value = db_value != NULL ? g_variant_get_string (db_value, NULL) : NULL;
          gint64 int_value;
          int_value = string_value != NULL ?
                      hyscan_map_track_param_nmea_channel (param, string_value) :
                      priv->rmc_default;
          *user_value = g_variant_new_int64 (int_value);
        }

      return KEY_CHANNEL_RMC;
    }

  else if (0 == g_strcmp0 (db_name, KEY_CHANNEL_PORT) ||
           0 == g_strcmp0 (db_name, KEY_CHANNEL_STARBOARD) ||
           0 == g_strcmp0 (db_name, KEY_TARGET_QUALITY))
    {
      if (user_value != NULL)
        *user_value = g_variant_ref (db_value);

      return db_name;
    }

  g_warning ("HyScanMapTrackParam: unknown key %s", db_name);

  return NULL;
}

/* Считывает значения по умолчанию. */
static gboolean
hyscan_map_track_param_get_defaults (HyScanMapTrackParam *param,
                                     HyScanParamList     *list)
{
  HyScanMapTrackParamPrivate *priv = param->priv;
  const gchar *const *params;
  gint i;

  params = hyscan_param_list_params (list);
  for (i = 0; params[i] != NULL; i++)
    {
      if (g_strcmp0 (params[i], KEY_CHANNEL_RMC) == 0)
        {
          hyscan_param_list_set_enum (list, KEY_CHANNEL_RMC, priv->rmc_default);
        }

      else if (g_strcmp0 (params[i], KEY_CHANNEL_DPT) == 0)
        {
          hyscan_param_list_set_enum (list, KEY_CHANNEL_DPT, priv->dpt_default);
        }

      else if (g_strcmp0 (params[i], KEY_CHANNEL_PORT) == 0)
        {
          hyscan_param_list_set_boolean (list, KEY_CHANNEL_PORT, TRUE);
        }

      else if (g_strcmp0 (params[i], KEY_CHANNEL_STARBOARD) == 0)
        {
          hyscan_param_list_set_boolean (list, KEY_CHANNEL_STARBOARD, TRUE);
        }

      else if (g_strcmp0 (params[i], KEY_TARGET_QUALITY) == 0)
        {
          hyscan_param_list_set_double (list, KEY_TARGET_QUALITY, DEFAULT_QUALITY);
        }

      else
        {
          g_warning ("HyScanMapTrackParam: unknown key %s", params[i]);
          return FALSE;
        }
    }

  return TRUE;
}

static HyScanDataSchema *
hyscan_map_track_param_schema (HyScanParam *param)
{
  HyScanMapTrackParam *track_param = HYSCAN_MAP_TRACK_PARAM (param);
  HyScanMapTrackParamPrivate *priv = track_param->priv;

  return priv->schema != NULL ? g_object_ref (priv->schema) : NULL;
}

static gboolean
hyscan_map_track_param_set (HyScanParam     *param,
                            HyScanParamList *list)
{
  HyScanMapTrackParam *track_param = HYSCAN_MAP_TRACK_PARAM (param);
  HyScanMapTrackParamPrivate *priv = track_param->priv;
  HyScanParamList *db_list;
  gboolean status = FALSE;

  const gchar *const *params;
  gint i;

  if (priv->object_name == NULL)
    return FALSE;
  
  hyscan_map_track_param_object_create (track_param);

  /* Преобразуем полученный список в список для записи в БД. */
  db_list = hyscan_param_list_new ();
  params = hyscan_param_list_params (list);
  for (i = 0; params[i] != NULL; i++)
    {
      GVariant *user_value, *db_value;
      const gchar *db_name;

      user_value = hyscan_param_list_get (list, params[i]);

      db_name = hyscan_map_track_param_user_to_db (track_param, params[i], user_value, &db_value);
      g_variant_unref (user_value);

      if (db_name == NULL)
        goto exit;

      hyscan_param_list_set (db_list, db_name, db_value);
    }

  /* Создаём объект в БД и записываем его параметры. */
  hyscan_map_track_param_object_create (track_param);
  status = hyscan_db_param_set (priv->db, priv->param_id, priv->object_name, db_list);
  
exit:
  g_clear_object (&db_list);
  
  return status;
}

static gboolean
hyscan_map_track_param_get (HyScanParam     *param,
                            HyScanParamList *list)
{
  HyScanMapTrackParam *track_param = HYSCAN_MAP_TRACK_PARAM (param);
  HyScanMapTrackParamPrivate *priv = track_param->priv;
  HyScanDataSchema *schema;
  HyScanParamList *db_list;
  gboolean status = FALSE;

  const gchar *const *params;
  gint i;

  if (priv->object_name == NULL)
    return FALSE;

  /* Если объекта нет в БД, то считываем значения по-умолчанию. */
  schema = hyscan_db_param_object_get_schema (priv->db, priv->param_id, priv->object_name);
  if (schema == NULL)
    {
      if (priv->defaults != NULL)
        return hyscan_param_get (HYSCAN_PARAM (priv->defaults), list);
      else
        return hyscan_map_track_param_get_defaults (track_param, list);
    }

  /* Преобразуем полученный список в список для чтения из БД. */
  db_list = hyscan_param_list_new ();
  params = hyscan_param_list_params (list);
  for (i = 0; params[i] != NULL; i++)
    {
      const gchar *db_name;

      db_name = hyscan_map_track_param_user_to_db (track_param, params[i], NULL, NULL);
      if (db_name == NULL)
        goto exit;

      hyscan_param_list_add (db_list, db_name);
    }

  /* Получаем параметры из БД. */
  if (!hyscan_db_param_get (priv->db, priv->param_id, priv->object_name, db_list))
    goto exit;

  /* Преобразуем список обратно. */
  params = hyscan_param_list_params (db_list);
  for (i = 0; params[i] != NULL; i++)
    {
      const gchar *user_name;
      GVariant *user_value, *db_value;

      db_value = hyscan_param_list_get (db_list, params[i]);
      user_name = hyscan_map_track_param_db_to_user (track_param, params[i], db_value, &user_value);
      g_clear_pointer (&db_value, g_variant_unref);

      if (user_name == NULL)
        goto exit;

      hyscan_param_list_set (list, user_name, user_value);
    }

  status = TRUE;

exit:
  g_clear_object (&schema);
  g_clear_object (&db_list);

  return status;
}

static void
hyscan_map_track_param_interface_init (HyScanParamInterface *iface)
{
  iface->schema = hyscan_map_track_param_schema;
  iface->set = hyscan_map_track_param_set;
  iface->get = hyscan_map_track_param_get;
}

/**
 * hyscan_map_track_param_new:
 * @profile: название профиля настройки
 * @db: база данных #HyScanDB
 * @project_name: название проекта
 * @track_name: (nullable): название галса
 *
 * Функция создаёт новый объект настройки параметров обработчика акустических
 * данных.
 *
 * Если @track_name = %NULL, то объект настройки будет управлять значениями
 * параметров галсов по умолчанию для этого проекта.
 *
 * Returns: указатель на #HyScanMapTrackParam, для удаления g_object_unref().
 */
HyScanMapTrackParam *
hyscan_map_track_param_new (gchar            *profile,
                            HyScanDB         *db,
                            const gchar      *project_name,
                            const gchar      *track_name)
{
  return g_object_new (HYSCAN_TYPE_MAP_TRACK_PARAM,
                       "profile", profile,
                       "db", db,
                       "project_name", project_name,
                       "track_name", track_name,
                       NULL);
}

/**
 * hyscan_map_track_param_get_mod_count:
 * @param: указатель на #HyScanMapTrackParam
 *
 * Функция возвращает номер изменения параметров обработки акустических данных.
 * При любом изменении в параметрах обработки номер изменения увеличивается.
 *
 * Обратное верно не всегда: возможна ситуация, что номер изменения увеличился,
 * но сами параметры обработки не изменились.
 *
 * Returns: номер изменения параметра.
 */
guint32
hyscan_map_track_param_get_mod_count (HyScanMapTrackParam *param)
{
  HyScanMapTrackParamPrivate *priv;
  guint32 defaults_mc, db_mc;

  g_return_val_if_fail (HYSCAN_IS_MAP_TRACK_PARAM (param), FALSE);

  priv = param->priv;

  db_mc = hyscan_db_get_mod_count (priv->db, priv->param_id);
  if (priv->defaults == NULL)
    return db_mc;

  defaults_mc = hyscan_map_track_param_get_mod_count (priv->defaults);

  /* Поскольку счётчики изменений увеличиваются, то можно просто их сложить. */
  return db_mc + defaults_mc;
}

/**
 * hyscan_map_track_param_has_rmc:
 * @param: указатель на #HyScanMapTrackParam
 *
 * Определяет, установлен ли в галсе канал навигационных данных.
 *
 * Returns: возвращает %TRUE, если канал навигационных данных установлен
 */
gboolean
hyscan_map_track_param_has_rmc (HyScanMapTrackParam *param)
{
  HyScanParamList *list;
  gboolean status;

  g_return_val_if_fail (HYSCAN_IS_MAP_TRACK_PARAM (param), FALSE);

  list = hyscan_param_list_new ();
  hyscan_param_list_add (list, KEY_CHANNEL_RMC);
  status = hyscan_param_get (HYSCAN_PARAM (param), list);
  if (!status)
    goto exit;

  status = hyscan_param_list_get_enum (list, KEY_CHANNEL_RMC) > 0;

exit:
  g_object_unref (list);

  return status;
}

HyScanNavData *
hyscan_map_track_param_get_nav_data (HyScanMapTrackParam  *param,
                                     HyScanNMEAField       field,
                                     HyScanCache          *cache)
{
  HyScanMapTrackParamPrivate *priv;
  HyScanParamList *list = NULL;
  HyScanNavData *nav_data = NULL;
  gint64 channel;
  HyScanNmeaDataType data_type;
  const gchar *param_name;

  g_return_val_if_fail (HYSCAN_IS_MAP_TRACK_PARAM (param), NULL);
  priv = param->priv;

  switch (field)
    {
    case HYSCAN_NMEA_FIELD_DEPTH:
      data_type = HYSCAN_NMEA_DATA_DPT;
      param_name = KEY_CHANNEL_DPT;
      break;

    case HYSCAN_NMEA_FIELD_LAT:
    case HYSCAN_NMEA_FIELD_LON:
    case HYSCAN_NMEA_FIELD_TRACK:
    case HYSCAN_NMEA_FIELD_SPEED:
    case HYSCAN_NMEA_FIELD_TIME:
      data_type = HYSCAN_NMEA_DATA_RMC;
      param_name = KEY_CHANNEL_RMC;
      break;

    default:
      g_warning ("HyScanMapTrack: unable to get HyScanNavData for field %d", field);
      goto exit;
    }

  list = hyscan_param_list_new ();
  hyscan_param_list_add (list, param_name);
  if (!hyscan_param_get (HYSCAN_PARAM (param), list))
    goto exit;

  channel = hyscan_param_list_get_enum (list, param_name);
  if (channel == 0)
    goto exit;

  nav_data =  HYSCAN_NAV_DATA (hyscan_nmea_parser_new (priv->db, cache, priv->project_name,
                                                       priv->track_name, channel,
                                                       data_type, field));
exit:
  g_clear_object (&list);

  return nav_data;
}

HyScanDepthometer *
hyscan_map_track_param_get_depthometer (HyScanMapTrackParam  *param,
                                        HyScanCache          *cache)
{
  HyScanMapTrackParamPrivate *priv;
  HyScanNMEAParser *dpt_parser;
  HyScanParamList *list;
  HyScanDepthometer *depthometer = NULL;
  gint64 channel;

  g_return_val_if_fail (HYSCAN_IS_MAP_TRACK_PARAM (param), NULL);
  priv = param->priv;

  list = hyscan_param_list_new ();
  hyscan_param_list_add (list, KEY_CHANNEL_DPT);
  if (!hyscan_param_get (HYSCAN_PARAM (param), list))
    goto exit;

  channel = hyscan_param_list_get_enum (list, KEY_CHANNEL_DPT);
  if (channel == 0)
    goto exit;
 
  dpt_parser = hyscan_nmea_parser_new (priv->db, cache,
                                       priv->project_name, priv->track_name, channel,
                                       HYSCAN_NMEA_DATA_DPT, HYSCAN_NMEA_FIELD_DEPTH);
  if (dpt_parser == NULL)
    goto exit;

  depthometer = hyscan_depthometer_new (HYSCAN_NAV_DATA (dpt_parser), cache);
  g_object_unref (dpt_parser);
  
exit:
  g_clear_object (&list);
  
  return depthometer;
}

/**
 * hyscan_map_track_param_clear:
 * @param: указатель на #HyScanMapTrackParam
 *
 * Функция сбрасывает значения всех параметров.
 *
 * Если @param содержит параметры конкретного галса, то галс будет использовать
 * параметры для всех галсов проекта.
 *
 * Если @param содержит параметры для всех галсов проекта, то параметрам будут
 * присвоены значения по умолчанию из схемы данных.
 *
 * Returns: %TRUE, если значения параметров были сброшены успешно.
 */
gboolean
hyscan_map_track_param_clear (HyScanMapTrackParam *param)
{
  HyScanMapTrackParamPrivate *priv;
  HyScanDataSchema *schema;
  HyScanParamList *list;
  const gchar *const *keys;
  gint i;
  gboolean result;

  g_return_val_if_fail (HYSCAN_IS_MAP_TRACK_PARAM (param), FALSE);

  priv = param->priv;

  /* Если дефолтный параметр существует, то просто удаляем группу объектов. */
  if (priv->defaults != NULL)
    return hyscan_db_param_object_remove (priv->db, priv->param_id, priv->object_name);

  /* Иначе сбрасываем значения до значений по умолчанию из схемы. */
  schema = hyscan_db_param_object_get_schema (priv->db, priv->param_id, priv->object_name);
  if (schema == NULL)
    return TRUE;

  list = hyscan_param_list_new ();
  keys = hyscan_data_schema_list_keys (schema);
  for (i = 0; keys[i] != NULL; i++)
    {
      if (!(hyscan_data_schema_key_get_access (schema, keys[i]) & HYSCAN_DATA_SCHEMA_ACCESS_WRITE))
        continue;

      hyscan_param_list_add (list, keys[i]);
    }
  result = hyscan_param_set (HYSCAN_PARAM (param), list);

  g_object_unref (schema);
  g_object_unref (list);

  return result;
}
