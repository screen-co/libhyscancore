/* hyscan-profile-hw-device.h
 *
 * Copyright 2019 Screen LLC, Alexander Dmitriev <m1n7@yandex.ru>
 *
 * This file is part of HyScanCore.
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

/*
 * Этот парень пока нестабилен, так что к нему документации нема.
 * Я, возможно, переработаю его.
 */
#include "hyscan-profile-hw-device.h"
#include <hyscan-driver.h>

#define HYSCAN_PROFILE_HW_DEVICE_DRIVER "driver"
#define HYSCAN_PROFILE_HW_DEVICE_URI "uri"

struct _HyScanProfileHWDevicePrivate
{
  gchar           *group;

  gchar          **paths;

  gchar           *uri;
  HyScanDiscover  *discover;
  HyScanParamList *params;
};

static void    hyscan_profile_hw_device_object_constructed (GObject *object);
static void    hyscan_profile_hw_device_object_finalize    (GObject *object);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanProfileHWDevice, hyscan_profile_hw_device, G_TYPE_OBJECT);

static void
hyscan_profile_hw_device_class_init (HyScanProfileHWDeviceClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);

  oclass->constructed = hyscan_profile_hw_device_object_constructed;
  oclass->finalize = hyscan_profile_hw_device_object_finalize;

}

static void
hyscan_profile_hw_device_init (HyScanProfileHWDevice *profile_hw_device)
{
  profile_hw_device->priv = hyscan_profile_hw_device_get_instance_private (profile_hw_device);
}

static void
hyscan_profile_hw_device_object_constructed (GObject *object)
{
  G_OBJECT_CLASS (hyscan_profile_hw_device_parent_class)->constructed (object);
}

static void
hyscan_profile_hw_device_object_finalize (GObject *object)
{
  HyScanProfileHWDevice *profile_hw_device = HYSCAN_PROFILE_HW_DEVICE (object);
  HyScanProfileHWDevicePrivate *priv = profile_hw_device->priv;

  g_clear_pointer (&priv->group, g_free);
  g_clear_pointer (&priv->uri, g_free);
  g_clear_pointer (&priv->paths, g_strfreev);

  g_clear_object (&priv->discover);
  g_clear_object (&priv->params);

  G_OBJECT_CLASS (hyscan_profile_hw_device_parent_class)->finalize (object);
}

HyScanProfileHWDevice *
hyscan_profile_hw_device_new (GKeyFile *keyfile)
{
  return g_object_new (HYSCAN_TYPE_PROFILE_HW_DEVICE, NULL);
}

static HyScanDiscover *
hyscan_profile_hw_device_find_driver (gchar       **paths,
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
hyscan_profile_hw_device_read_params (GKeyFile         *kf,
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

      if (g_str_equal (*iter, HYSCAN_PROFILE_HW_DEVICE_DRIVER) ||
          g_str_equal (*iter, HYSCAN_PROFILE_HW_DEVICE_URI))
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
          g_warning ("HyScanProfileHWDevice: invalid key type");
          break;
        }
    }

  g_strfreev (keys);

  return params;
}

void
hyscan_profile_hw_device_set_group (HyScanProfileHWDevice *hw_device,
                                    const gchar           *group)
{
  g_return_if_fail (HYSCAN_IS_PROFILE_HW_DEVICE (hw_device));

  g_clear_pointer (&hw_device->priv->group, g_free);
  hw_device->priv->group = g_strdup (group);
}

void
hyscan_profile_hw_device_set_paths (HyScanProfileHWDevice  *hw_device,
                                    gchar                 **paths)
{
  g_return_if_fail (HYSCAN_IS_PROFILE_HW_DEVICE (hw_device));

  g_clear_pointer (&hw_device->priv->paths, g_strfreev);
  hw_device->priv->paths = g_strdupv ((gchar**)paths);

}

void
hyscan_profile_hw_device_read (HyScanProfileHWDevice *hw_device,
                               GKeyFile              *kf)
{
  HyScanProfileHWDevicePrivate *priv;
  gchar *driver;
  HyScanDataSchema *schema;

  g_return_if_fail (HYSCAN_IS_PROFILE_HW_DEVICE (hw_device));
  priv = hw_device->priv;

  /* Определяем название драйвера и путь к устройству. */
  driver = g_key_file_get_string (kf, priv->group, HYSCAN_PROFILE_HW_DEVICE_DRIVER, NULL);
  priv->uri = g_key_file_get_string (kf, priv->group, HYSCAN_PROFILE_HW_DEVICE_URI, NULL);

  /* Считываем схему данных. */
  priv->discover = hyscan_profile_hw_device_find_driver (priv->paths, driver);

  if (priv->discover == NULL)
    {
      g_warning ("Couldn't find driver <%s> for device <%s>", driver, priv->group);
      return;
    }

  schema = hyscan_discover_config (priv->discover, priv->uri);

  /* По этой схеме считываем ключи. */
  priv->params = hyscan_profile_hw_device_read_params (kf, priv->group, schema);

  g_object_unref (schema);
}

gboolean
hyscan_profile_hw_device_check (HyScanProfileHWDevice *hw_device)
{
  HyScanProfileHWDevicePrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_PROFILE_HW_DEVICE (hw_device), FALSE);
  priv = hw_device->priv;

  if (priv->discover == NULL)
    return FALSE;
  return hyscan_discover_check (priv->discover, priv->uri, priv->params);
}

HyScanDevice *
hyscan_profile_hw_device_connect (HyScanProfileHWDevice *hw_device)
{
  HyScanProfileHWDevicePrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_PROFILE_HW_DEVICE (hw_device), NULL);
  priv = hw_device->priv;

  return hyscan_discover_connect (priv->discover, priv->uri, priv->params);
}
