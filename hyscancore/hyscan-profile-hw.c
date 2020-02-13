/* hyscan-profile-hw.c
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

/**
 * SECTION: hyscan-profile-hw
 * @Short_description: профиль оборудования
 * @Title: HyScanProfileHW
 *
 * Класс HyScanProfileHW реализует профили оборудования.
 * Профиль оборудования содержит группу "_" с информацией о профиле:
 * "name" - человекочитаемое название профиля.
 *
 * Все остальные группы относятся к конкретному оборудованию (локаторам и
 * датчикам).
 *
 * Перед чтением профиля необходимо задать пути к драйверам устройств функцией
 * #hyscan_profile_hw_set_driver_paths. После чтения #hyscan_profile_hw_connect
 * создает объект #HyScanControl и добавляет в него оборудование профиля.
 */

#include "hyscan-profile-hw.h"
#include <hyscan-profile-hw-device.h>
#include <hyscan-driver.h>

#define HYSCAN_PROFILE_HW_INFO_GROUP "_"
#define HYSCAN_PROFILE_HW_NAME "name"

typedef struct
{
  gchar                 *group;  /* Название группы. */
  HyScanProfileHWDevice *device; /* Указатель на объект девайса. */
} HyScanProfileHWItem;

struct _HyScanProfileHWPrivate
{
  gchar    **drivers; /* Список путей с драйверами. */
  GList     *devices; /* Список оборудования {HyScanProfileHWItem}. */
};

static void     hyscan_profile_hw_object_finalize         (GObject               *object);
static void     hyscan_profile_hw_make_id                 (gchar                 *buffer,
                                                           guint                  size);
static HyScanProfileHWItem * hyscan_profile_hw_item_new   (const gchar           *id,
                                                           HyScanProfileHWDevice *device);
static void     hyscan_profile_hw_item_free               (gpointer               data);
static gint     hyscan_profile_hw_item_find               (gconstpointer          data,
                                                           gconstpointer          udata);
static gboolean hyscan_profile_hw_info_group              (HyScanProfile         *profile,
                                                           GKeyFile              *kf,
                                                           const gchar           *group);
static void     hyscan_profile_hw_clear                   (HyScanProfileHW       *profile);
static gboolean hyscan_profile_hw_read                    (HyScanProfile         *profile,
                                                           GKeyFile              *file);
static gboolean hyscan_profile_hw_write                   (HyScanProfile         *profile,
                                                           GKeyFile              *file);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanProfileHW, hyscan_profile_hw, HYSCAN_TYPE_PROFILE);

static void
hyscan_profile_hw_class_init (HyScanProfileHWClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);
  HyScanProfileClass *pklass = HYSCAN_PROFILE_CLASS (klass);

  oclass->finalize = hyscan_profile_hw_object_finalize;
  pklass->read = hyscan_profile_hw_read;
  pklass->write = hyscan_profile_hw_write;
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

/* Функция создаёт уникальный идентификатор. */
static void
hyscan_profile_hw_make_id (gchar *buffer,
                           guint  size)
{
  guint i;
  gint rnd;

  for (i = 0; i < size; i++)
    {
      rnd = g_random_int_range (0, 62);
      if (rnd < 10)
        buffer[i] = '0' + rnd;
      else if (rnd < 36)
        buffer[i] = 'a' + rnd - 10;
      else
        buffer[i] = 'A' + rnd - 36;
    }
  buffer[i] = '\0';
}

static HyScanProfileHWItem *
hyscan_profile_hw_item_new (const gchar           *id,
                            HyScanProfileHWDevice *device)
{
  HyScanProfileHWItem * item = g_new0 (HyScanProfileHWItem, 1);

  item->group = g_strdup (id);
  item->device = g_object_ref (device);

  return item;
}

/* Функция освобождает HyScanProfileHWItem. */
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

/* Функция ищет HyScanProfileHWItem по названию группы. */
static gint
hyscan_profile_hw_item_find (gconstpointer data,
                             gconstpointer udata)
{
  const HyScanProfileHWItem *item = data;
  const gchar *id = udata;

  if (item->group != NULL)
    return g_strcmp0 (item->group, id);

  g_warning ("HyScanProfileHW: found item without group");
  return -1;
}

/* Обработка информационной группы (HYSCAN_PROFILE_HW_INFO_GROUP) */
static gboolean
hyscan_profile_hw_info_group (HyScanProfile *profile,
                              GKeyFile      *kf,
                              const gchar   *group)
{
  gchar *name;

  if (!g_str_equal (group, HYSCAN_PROFILE_HW_INFO_GROUP))
    return FALSE;

  name = g_key_file_get_string (kf, group, HYSCAN_PROFILE_HW_NAME, NULL);
  hyscan_profile_set_name (profile, name);

  g_free (name);
  return TRUE;
}

/* Функция очищает профиль. */
static void
hyscan_profile_hw_clear (HyScanProfileHW *self)
{
  HyScanProfileHWPrivate *priv = self->priv;
  g_list_free_full (priv->devices, hyscan_profile_hw_item_free);
}

/* Функция парсинга профиля. */
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
      HyScanProfileHWDevice *device;

      if (hyscan_profile_hw_info_group (profile, file, *iter))
        continue;

      device = hyscan_profile_hw_device_new (file);
      hyscan_profile_hw_device_set_paths (device, priv->drivers);
      hyscan_profile_hw_device_set_group (device, *iter);
      hyscan_profile_hw_device_read (device, file);

      hyscan_profile_hw_add (self, device);
      g_object_unref (device);
    }

  g_strfreev (groups);

  return TRUE;
}

/* Функция записи профиля. */
static gboolean
hyscan_profile_hw_write (HyScanProfile *profile,
                         GKeyFile      *file)
{
  HyScanProfileHW *self = HYSCAN_PROFILE_HW (profile);
  GList *link;

  g_key_file_set_string (file, HYSCAN_PROFILE_HW_INFO_GROUP,
                         HYSCAN_PROFILE_HW_NAME,
                         hyscan_profile_get_name (profile));

  for (link = self->priv->devices; link != NULL; link = link->next)
    {
      HyScanProfileHWItem * item = link->data;
      hyscan_profile_hw_device_write (item->device, file);
    }

  return TRUE;
}

/**
 * hyscan_profile_hw_new:
 * @file: полный путь к файлу профиля
 *
 * Функция создает объект работы с профилем оборудования.
 *
 * Returns: (transfer full): #HyScanProfileHW.
 */
HyScanProfileHW *
hyscan_profile_hw_new (const gchar *file)
{
  return g_object_new (HYSCAN_TYPE_PROFILE_HW,
                       "file", file,
                       NULL);
}

/**
 * hyscan_profile_hw_set_driver_paths:
 * @self: #HyScanProfileHW
 * @driver_paths: NULL-терминированный список путей с драйверами
 *
 * Функция задает пути к драйверам.
 */
void
hyscan_profile_hw_set_driver_paths (HyScanProfileHW  *self,
                                    gchar           **driver_paths)
{
  g_return_if_fail (HYSCAN_IS_PROFILE_HW (self));

  g_strfreev (self->priv->drivers);
  self->priv->drivers = g_strdupv (driver_paths);
}

/**
 * hyscan_profile_hw_list:
 * @self: #HyScanProfileHW
 *
 * Функция возвращает список устройств.
 *
 * Returns: (transfer container): список устройств в профиле.
 */
GList *
hyscan_profile_hw_list (HyScanProfileHW *self)
{
  g_return_val_if_fail (HYSCAN_IS_PROFILE_HW (self), NULL);

  return g_list_copy (self->priv->devices);
}

/**
 * hyscan_profile_hw_add:
 * @self: #HyScanProfileHW
 * @device: профиль нового устройства.
 *
 * Функция добавляет новое устройство в профиль подключения.
 * Если у профиля не задана группа, она будет сгенерирована случайным образом.
 * Если устройство с такой же группой уже существует, группа будет сгенерирована
 * и задана случайным образом.
 *
 * Returns: идентификатор, по которому можно удалить
 * элемент из профиля.
 */
const gchar *
hyscan_profile_hw_add (HyScanProfileHW       *self,
                       HyScanProfileHWDevice *device)
{
  HyScanProfileHWPrivate *priv;
  const gchar *id;
  gchar new_id[20];

  g_return_val_if_fail (HYSCAN_IS_PROFILE_HW (self), NULL);
  g_return_val_if_fail (HYSCAN_IS_PROFILE_HW_DEVICE (device), NULL);
  priv = self->priv;

  id = hyscan_profile_hw_device_get_group (device);

  /* Если не задана группа, генерируем её. */
  if (id == NULL || g_str_equal (id, HYSCAN_PROFILE_HW_NAME))
    {
      hyscan_profile_hw_make_id (new_id, G_N_ELEMENTS (new_id));
      hyscan_profile_hw_device_set_group (device, new_id);
      id = new_id;
    }

  /* Ищем дублирующийся идентификатор (группу). */
  while (NULL != g_list_find_custom (priv->devices, id, hyscan_profile_hw_item_find))
    {
      hyscan_profile_hw_make_id (new_id, G_N_ELEMENTS (new_id));
      hyscan_profile_hw_device_set_group (device, new_id);
      id = new_id;
    }

  priv->devices = g_list_append (priv->devices,
                                 hyscan_profile_hw_item_new (id, device));

  return id;
}

/**
 * hyscan_profile_hw_remove:
 * @self: #HyScanProfileHW
 * @id: идентификатор (группа) устройства.
 *
 * Функция удаляет устройство.
 *
 * Returns: TRUE, если устройство найдено и удалено.
 */
gboolean
hyscan_profile_hw_remove (HyScanProfileHW *self,
                          const gchar     *id)
{
  HyScanProfileHWPrivate *priv;
  GList *link;

  g_return_val_if_fail (HYSCAN_IS_PROFILE_HW (self), FALSE);
  priv = self->priv;

  /* Ищу это устройство. */
  link = g_list_find_custom (priv->devices, id, hyscan_profile_hw_item_find);

  if (link == NULL)
    {
      g_warning ("HyScanProfileHW: item <%s> not found", id);
      return FALSE;
    }

  priv->devices = g_list_remove_link (priv->devices, link);
  hyscan_profile_hw_item_free (link->data);
  g_list_free (link);

  return TRUE;
}

/**
 * hyscan_profile_hw_check:
 * @self: #HyScanProfileHW
 *
 * Функция проверяет возможность подключения ко всем единицам оборудования.
 *
 * Returns: TRUE, если подключение возможно.
 */
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

/**
 * hyscan_profile_hw_connect:
 * @self: #HyScanProfileHW
 *
 * Функция проверяет возможность подключения ко всем единицам оборудования.
 *
 * Returns: (transfer full): #HyScanControl со всем оборудованием, NULL в
 * случае ошибки.
 */
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
          g_object_unref (device);
          break;
        }

      g_object_unref (device);
    }

  return control;
}

/**
 * hyscan_profile_hw_connect_simple:
 * @file: полный путь к файлу профиля
 * @driver_paths: NULL-терминированный список путей с драйверами
 *
 * Функция для подключения к оборудованию в автоматическом режиме.
 *
 * Returns: (transfer full): #HyScanControl аналогично
 * #hyscan_profile_hw_connect
 */
HyScanControl *
hyscan_profile_hw_connect_simple (const gchar  *file,
                                  gchar       **driver_paths)
{
  HyScanProfileHW * profile;
  HyScanControl * control;

  profile = hyscan_profile_hw_new (file);
  if (profile == NULL)
    {
      return NULL;
    }

  hyscan_profile_hw_set_driver_paths (profile, driver_paths);
  hyscan_profile_read (HYSCAN_PROFILE (profile));

  if (!hyscan_profile_hw_check (profile))
    {
      g_object_unref (profile);
      return NULL;
    }

  control = hyscan_profile_hw_connect (profile);

  g_object_unref (profile);
  return control;
}
