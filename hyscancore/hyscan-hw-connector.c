#include "hyscan-hw-connector.h"

#define HYSCAN_HW_PROFILE_DEVICE_DRIVER "driver"
#define HYSCAN_HW_PROFILE_DEVICE_URI "uri"

enum
{
  PROP_0,
  PROP_FILE_PATH,
};

typedef struct
{
  gchar           *uri;
  HyScanDiscover  *discover;
  HyScanParamList *params;
} HyScanHWConnectorInfo;

struct _HyScanHWConnectorPrivate
{
  gchar     **paths;
  GList      *devices;
};

static void              hyscan_hw_connector_object_finalize  (GObject               *object);
static void              hyscan_hw_connector_info_free        (HyScanHWConnectorInfo *info);
static HyScanDiscover *  hyscan_hw_profile_device_find_driver (gchar                **paths,
                                                               const gchar           *name);
static HyScanParamList * hyscan_hw_profile_device_read_params (GKeyFile              *kf,
                                                               const gchar           *group,
                                                               HyScanDataSchema      *schema);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanHWConnector, hyscan_hw_connector, G_TYPE_OBJECT);

static void
hyscan_hw_connector_class_init (HyScanHWConnectorClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);

  oclass->finalize = hyscan_hw_connector_object_finalize;
}

static void
hyscan_hw_connector_init (HyScanHWConnector *hw_connector)
{
  hw_connector->priv = hyscan_hw_connector_get_instance_private (hw_connector);
}

static void
hyscan_hw_connector_object_finalize (GObject *object)
{
  HyScanHWConnector *self = HYSCAN_HW_CONNECTOR (object);
  HyScanHWConnectorPrivate *priv = self->priv;

  g_strfreev (priv->paths);
  g_list_free_full (priv->devices, (GDestroyNotify)hyscan_hw_connector_info_free);

  G_OBJECT_CLASS (hyscan_hw_connector_parent_class)->finalize (object);
}

static void
hyscan_hw_connector_info_free (HyScanHWConnectorInfo *info)
{
  if (info == NULL)
    return;

  g_clear_pointer (&info->uri, g_free);
  g_clear_object (&info->discover);
  g_clear_object (&info->params);

  g_free (info);
}

static HyScanDiscover *
hyscan_hw_profile_device_find_driver (gchar       **paths,
                                      const gchar  *name)
{
  HyScanDriver * driver;

  if (paths == NULL)
    return NULL;

  /* Проходим по нуль-терминированному списку и возвращаем первый драйвер. */
  for (; *paths != NULL; ++paths)
    {
      driver = hyscan_driver_new (*paths, name);
      if (driver != NULL)
        return HYSCAN_DISCOVER (driver);
    }

  return NULL;
}

static HyScanParamList *
hyscan_hw_profile_device_read_params (GKeyFile         *kf,
                                      const gchar      *group,
                                      HyScanDataSchema *schema)
{
  gchar ** keys, ** iter;
  HyScanParamList * params;

  keys = g_key_file_get_keys (kf, group, NULL, NULL);
  if (keys == NULL)
    return NULL;

  params = hyscan_param_list_new ();

  for (iter = keys; *iter != NULL; ++iter)
    {
      HyScanDataSchemaKeyType type;

      if (g_str_equal (*iter, HYSCAN_HW_PROFILE_DEVICE_DRIVER) ||
          g_str_equal (*iter, HYSCAN_HW_PROFILE_DEVICE_URI))
        {
          continue;
        }

      type = hyscan_data_schema_key_get_value_type (schema, *iter);

      switch (type)
        {
        gboolean bool_val;
        gint64 int_val;
        gdouble dbl_val;
        gchar *str_val;
        const gchar * enum_id;
        const HyScanDataSchemaEnumValue * found;

        case HYSCAN_DATA_SCHEMA_KEY_BOOLEAN:
          bool_val = g_key_file_get_boolean (kf, group, *iter, NULL);
          hyscan_param_list_set_boolean (params, *iter, bool_val);
          break;

        case HYSCAN_DATA_SCHEMA_KEY_INTEGER:
          int_val = g_key_file_get_int64 (kf, group, *iter, NULL);
          hyscan_param_list_set_integer (params, *iter, int_val);
          break;

        case HYSCAN_DATA_SCHEMA_KEY_ENUM:
          str_val = g_key_file_get_string (kf, group, *iter, NULL);

          enum_id = hyscan_data_schema_key_get_enum_id (schema, *iter);
          found = hyscan_data_schema_enum_find_by_id (schema, enum_id, str_val);
          if (found != NULL)
            hyscan_param_list_set_enum (params, *iter, found->value);

          g_free (str_val);
          break;

        case HYSCAN_DATA_SCHEMA_KEY_DOUBLE:
          dbl_val = g_key_file_get_double (kf, group, *iter, NULL);
          hyscan_param_list_set_double (params, *iter, dbl_val);
          break;

        case HYSCAN_DATA_SCHEMA_KEY_STRING:
          str_val = g_key_file_get_string (kf, group, *iter, NULL);
          hyscan_param_list_set_string (params, *iter, str_val);
          g_free (str_val);
          break;

        default:
          g_warning ("HyScanHWProfileDevice: invalid key type");
          break;
        }
    }

  g_strfreev (keys);

  return params;
}

HyScanHWConnector *
hyscan_hw_connector_new (void)
{
  return g_object_new (HYSCAN_TYPE_HW_CONNECTOR, NULL);
}

void
hyscan_hw_connector_set_driver_paths (HyScanHWConnector *connector,
                                      const gchar      **paths)
{
  g_return_if_fail (HYSCAN_IS_HW_CONNECTOR (connector));

  g_clear_pointer (&connector->priv->paths, g_strfreev);
  connector->priv->paths = g_strdupv ((gchar**)paths);
}

gboolean
hyscan_hw_connector_read (HyScanHWConnector *connector,
                          const gchar       *file)
{
  GError *error = NULL;
  GKeyFile *keyfile;
  gchar **groups, **iter;
  HyScanHWConnectorPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_HW_CONNECTOR (connector), FALSE);
  priv = connector->priv;

  keyfile = g_key_file_new ();

  /* Считываю файл. */
  g_key_file_load_from_file (keyfile, file, G_KEY_FILE_NONE, &error);
  if (error != NULL)
    {
      g_warning ("HyScanHWConnector: %s", error->message);
      g_error_free (error);
      return FALSE;
    }

  /* Список групп. */
  groups = g_key_file_get_groups (keyfile, NULL);
  if (groups == NULL)
    return FALSE;

  /* Парсим каждую группу. */
  for (iter = groups; *iter != NULL; ++iter)
    {
      HyScanHWConnectorInfo *info = NULL;
      gchar *driver = NULL;
      HyScanDataSchema *schema = NULL;

      info = g_new0 (HyScanHWConnectorInfo, 1);

      driver = g_key_file_get_string (keyfile, *iter, HYSCAN_HW_PROFILE_DEVICE_DRIVER, NULL);

      info->uri = g_key_file_get_string (keyfile, *iter, HYSCAN_HW_PROFILE_DEVICE_URI, NULL);
      info->discover = hyscan_hw_profile_device_find_driver (priv->paths, driver);

      g_free (driver);

      if (info->discover == NULL)
        {
          g_warning ("Con: %s %s", info->uri, driver);
          hyscan_hw_connector_info_free (info);
          continue;
        }

      schema = hyscan_discover_config (info->discover, info->uri);
      info->params = hyscan_hw_profile_device_read_params (keyfile, *iter, schema);

      g_object_unref (schema);

      priv->devices = g_list_append (priv->devices, info);
    }

  g_key_file_unref (keyfile);
  return TRUE;
}

gboolean
hyscan_hw_connector_check (HyScanHWConnector *connector)
{
  GList *link;
  HyScanHWConnectorPrivate *priv;


  g_return_val_if_fail (HYSCAN_IS_HW_CONNECTOR (connector), FALSE);
  priv = connector->priv;

  for (link = priv->devices; link != NULL; link = link->next)
    {
      HyScanHWConnectorInfo *info = link->data;

      if (!hyscan_discover_check (info->discover, info->uri, info->params))
        return FALSE;
    }

  return TRUE;
}

HyScanControl *
hyscan_hw_connector_connect (HyScanHWConnector *connector)
{
  HyScanHWConnectorPrivate *priv;
  HyScanControl * control;
  GList *link;

  g_return_val_if_fail (HYSCAN_IS_HW_CONNECTOR (connector), NULL);
  priv = connector->priv;

  control = hyscan_control_new ();
  for (link = priv->devices; link != NULL; link = link->next)
    {
      HyScanHWConnectorInfo *info = link->data;
      HyScanDevice *device;

      device = hyscan_discover_connect (info->discover, info->uri, info->params);
      if (device != NULL)
        {
          if (!hyscan_control_device_add (control, device))
            g_warning ("couldn't add device");
        }
    }

  hyscan_control_device_bind (control);

  return control;
}