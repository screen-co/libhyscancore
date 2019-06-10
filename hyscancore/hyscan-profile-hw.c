#include "hyscan-profile-hw.h"
#include <hyscan-profile-hw-device.h>
#include <hyscan-driver.h>

#define HYSCAN_PROFILE_HW_INFO_GROUP "_"
#define HYSCAN_PROFILE_HW_NAME "name"

typedef struct
{
  gchar                 *group;
  HyScanProfileHWDevice *device;
} HyScanProfileHWItem;

struct _HyScanProfileHWPrivate
{
  gchar    **drivers;
  GList     *devices;
};

static void     hyscan_profile_hw_object_finalize         (GObject               *object);
static void     hyscan_profile_hw_item_free               (gpointer               data);
static void     hyscan_profile_hw_clear                   (HyScanProfileHW       *profile);
static gboolean hyscan_profile_hw_read                    (HyScanProfile         *profile,
                                                           GKeyFile              *file);
static gboolean hyscan_profile_hw_info_group              (HyScanProfile         *profile,
                                                           GKeyFile              *kf,
                                                           const gchar           *group);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanProfileHW, hyscan_profile_hw, HYSCAN_TYPE_PROFILE);

static void
hyscan_profile_hw_class_init (HyScanProfileHWClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);
  HyScanProfileClass *pklass = HYSCAN_PROFILE_CLASS (klass);

  oclass->finalize = hyscan_profile_hw_object_finalize;
  pklass->read = hyscan_profile_hw_read;
}

static void
hyscan_profile_hw_init (HyScanProfileHW *profile)
{
  profile->priv = hyscan_profile_hw_get_instance_private (profile);
}

static void
hyscan_profile_hw_object_finalize (GObject *object)
{
  HyScanProfileHW *profile = HYSCAN_PROFILE_HW (object);
  HyScanProfileHWPrivate *priv = profile->priv;

  g_strfreev (priv->drivers);
  hyscan_profile_hw_clear (profile);

  G_OBJECT_CLASS (hyscan_profile_hw_parent_class)->finalize (object);
}

static void
hyscan_profile_hw_item_free (gpointer data)
{
  HyScanProfileHWItem *item = data;

  if (item == NULL)
    return;

  g_clear_pointer (&item->group, g_free);
  g_clear_object (&item->device);
  g_free (item);
}

static void
hyscan_profile_hw_clear (HyScanProfileHW *profile)
{
  HyScanProfileHWPrivate *priv = profile->priv;

  g_list_free_full (priv->devices, hyscan_profile_hw_item_free);
}

static gboolean
hyscan_profile_hw_read (HyScanProfile *profile,
                        GKeyFile      *file)
{
  HyScanProfileHW *self = HYSCAN_PROFILE_HW (profile);
  HyScanProfileHWPrivate *priv = self->priv;
  gchar **groups, **iter;

  /* Очищаем, если что-то было. */
  hyscan_profile_hw_clear (self);

  groups = g_key_file_get_groups (file, NULL);
  for (iter = groups; iter != NULL && *iter != NULL; ++iter)
    {
      HyScanProfileHWDevice * device;
      HyScanProfileHWItem *item;

      if (hyscan_profile_hw_info_group (profile, file, *iter))
        continue;

      device = hyscan_profile_hw_device_new (file);
      hyscan_profile_hw_device_set_group (device, *iter);
      hyscan_profile_hw_device_set_paths (device, priv->drivers);
      hyscan_profile_hw_device_read (device, file);

      item = g_new0 (HyScanProfileHWItem, 1);
      item->group = g_strdup (*iter);
      item->device = device;

      priv->devices = g_list_append (priv->devices, item);
    }

  g_strfreev (groups);

  return TRUE;
}

static gboolean
hyscan_profile_hw_info_group (HyScanProfile *profile,
                              GKeyFile      *kf,
                              const gchar   *group)
{
  gchar *name;

  if (!g_str_equal (group, HYSCAN_PROFILE_HW_INFO_GROUP))
    return FALSE;

  name = g_key_file_get_string (kf, group, HYSCAN_PROFILE_HW_NAME, NULL);
  hyscan_profile_set_name (HYSCAN_PROFILE (profile), name);

  g_free (name);
  return TRUE;
}

HyScanProfileHW *
hyscan_profile_hw_new (const gchar *file)
{
  return g_object_new (HYSCAN_TYPE_PROFILE_HW,
                       "file", file,
                       NULL);
}

void
hyscan_profile_hw_set_driver_paths (HyScanProfileHW  *self,
                                    gchar           **driver_paths)
{
  g_return_if_fail (HYSCAN_IS_PROFILE_HW (self));

  g_strfreev (self->priv->drivers);
  self->priv->drivers = g_strdupv (driver_paths);
}

GList *
hyscan_profile_hw_list (HyScanProfileHW *self)
{
  GList *list = NULL;
  GList *link;
  HyScanProfileHWPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_PROFILE_HW (self), NULL);
  priv = self->priv;

  for (link = priv->devices; link != NULL; link = link->next)
    {
      HyScanProfileHWItem *src = link->data;
      HyScanProfileHWItem *dst = g_new0 (HyScanProfileHWItem, 1);
      dst->group = g_strdup (src->group);
      dst->device = g_object_ref (src->device);

      list = g_list_append (list, dst);
    }

  return list;
}

gboolean
hyscan_profile_hw_check (HyScanProfileHW *self)
{
  HyScanProfileHWPrivate *priv;
  GList *link;
  gboolean st = TRUE;

  g_return_val_if_fail (HYSCAN_IS_PROFILE_HW (self), FALSE);
  priv = self->priv;

  if (priv->devices == NULL)
    return FALSE;

  for (link = priv->devices; link != NULL; link = link->next)
    {
      HyScanProfileHWItem *item = link->data;
      st &= hyscan_profile_hw_device_check (item->device);
    }

  return st;
}

HyScanControl *
hyscan_profile_hw_connect (HyScanProfileHW *self)
{
  HyScanProfileHWPrivate *priv;
  HyScanControl * control;
  GList *link;

  g_return_val_if_fail (HYSCAN_IS_PROFILE_HW (self), NULL);
  priv = self->priv;

  if (priv->devices == NULL)
    return FALSE;

  control = hyscan_control_new ();

  for (link = priv->devices; link != NULL; link = link->next)
    {
      HyScanProfileHWItem *item = link->data;
      HyScanDevice *device;

      device = hyscan_profile_hw_device_connect (item->device);

      if (device == NULL)
        {
          g_warning ("couldn't connect to device");
          g_clear_object (&control);
          break;
        }

      if (!hyscan_control_device_add (control, device))
        {
          g_warning ("couldn't add device");
          g_clear_object (&control);
          break;
        }
    }

  return control;
}

HyScanControl *
hyscan_profile_hw_connect_simple (const gchar *file)
{
  HyScanProfileHW * profile;
  HyScanControl * control;

  profile = hyscan_profile_hw_new (file);
  if (profile == NULL)
    {
      return NULL;
    }

  if (!hyscan_profile_hw_check (profile))
    {
      g_object_unref (profile);
      return NULL;
    }

  control = hyscan_profile_hw_connect (profile);

  g_object_unref (profile);
  return control;
}
