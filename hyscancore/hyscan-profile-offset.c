/* hyscan-profile-offset.c
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
 * SECTION: hyscan-profile-offset
 * @Short_description: профиль местоположения антенн
 * @Title: HyScanProfileOffset
 *
 * Класс HyScanProfileOffset реализует профили местоположения антенн.
 * Профиль оборудования содержит группу "_" с информацией о профиле:
 * "name" - человекочитаемое название профиля.
 *
 * Все остальные группы относятся к конкретному оборудованию (локаторам и
 * датчикам). Если название группы соответствует строке, возвращаемой функцией
 * #hyscan_source_get_id_by_type, то это считается местоположением приемной
 * антенны локатора, иначе -- местоположением датчика.
 *
 * Каждая группа может содержать следующие поля:
 * "x", "y", "z", "psi", "gamma", "theta" для соответствующих полей
 * #HyScanAntennaOffset.
 */

#include <gio/gio.h>
#include "hyscan-profile-offset.h"

#define HYSCAN_PROFILE_OFFSET_INFO_GROUP "_"
#define HYSCAN_PROFILE_OFFSET_NAME "name"

#define HYSCAN_PROFILE_OFFSET_X "x"
#define HYSCAN_PROFILE_OFFSET_Y "y"
#define HYSCAN_PROFILE_OFFSET_Z "z"
#define HYSCAN_PROFILE_OFFSET_PSI "psi"
#define HYSCAN_PROFILE_OFFSET_GAMMA "gamma"
#define HYSCAN_PROFILE_OFFSET_THETA "theta"

struct _HyScanProfileOffsetPrivate
{
  GHashTable *sources;  /* Таблица смещений для локаторов. {HyScanSourceType : HyScanAntennaOffset} */
  GHashTable *sensors; /* Таблица смещений для антенн. {gchar* : HyScanAntennaOffset} */
};

static void     hyscan_profile_offset_object_finalize  (GObject             *object);
static void     hyscan_profile_offset_clear            (HyScanProfileOffset *profile);
static gboolean hyscan_profile_offset_read             (HyScanProfile       *profile,
                                                        GKeyFile            *file);
static gboolean hyscan_profile_offset_info_group       (HyScanProfileOffset *profile,
                                                        GKeyFile            *kf,
                                                        const gchar         *group);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanProfileOffset, hyscan_profile_offset, HYSCAN_TYPE_PROFILE);

static void
hyscan_profile_offset_class_init (HyScanProfileOffsetClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);
  HyScanProfileClass *pklass = HYSCAN_PROFILE_CLASS (klass);

  oclass->finalize = hyscan_profile_offset_object_finalize;
  pklass->read = hyscan_profile_offset_read;
}

static void
hyscan_profile_offset_init (HyScanProfileOffset *profile)
{
  profile->priv = hyscan_profile_offset_get_instance_private (profile);
}

static void
hyscan_profile_offset_object_finalize (GObject *object)
{
  HyScanProfileOffset *self = HYSCAN_PROFILE_OFFSET (object);

  hyscan_profile_offset_clear (self);

  G_OBJECT_CLASS (hyscan_profile_offset_parent_class)->finalize (object);
}

/* Функция очищает профиль. */
static void
hyscan_profile_offset_clear (HyScanProfileOffset *profile)
{
  HyScanProfileOffsetPrivate *priv = profile->priv;

  g_clear_pointer (&priv->sources, g_hash_table_unref);
  g_clear_pointer (&priv->sensors, g_hash_table_unref);

  priv->sources = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                        NULL, (GDestroyNotify)hyscan_antenna_offset_free);
  priv->sensors = g_hash_table_new_full (g_str_hash, g_str_equal,
                                         g_free, (GDestroyNotify)hyscan_antenna_offset_free);
}

/* Функция парсинга профиля. */
static gboolean
hyscan_profile_offset_read (HyScanProfile *profile,
                            GKeyFile      *file)
{
  HyScanProfileOffset *self = HYSCAN_PROFILE_OFFSET (profile);
  HyScanProfileOffsetPrivate *priv = self->priv;
  gchar **groups, **iter;

  /* Очищаем, если что-то было. */
  hyscan_profile_offset_clear (self);

  groups = g_key_file_get_groups (file, NULL);
  for (iter = groups; iter != NULL && *iter != NULL; ++iter)
    {
      HyScanAntennaOffset offset;
      HyScanSourceType source;
      HyScanChannelType type;
      guint channel;

      /* Возможно, это группа с информацией. */
      if (hyscan_profile_offset_info_group (self, file, *iter))
        continue;

      offset.x = g_key_file_get_double (file, *iter, HYSCAN_PROFILE_OFFSET_X, NULL);
      offset.y = g_key_file_get_double (file, *iter, HYSCAN_PROFILE_OFFSET_Y, NULL);
      offset.z = g_key_file_get_double (file, *iter, HYSCAN_PROFILE_OFFSET_Z, NULL);
      offset.psi = g_key_file_get_double (file, *iter, HYSCAN_PROFILE_OFFSET_PSI, NULL);
      offset.gamma = g_key_file_get_double (file, *iter, HYSCAN_PROFILE_OFFSET_GAMMA, NULL);
      offset.theta = g_key_file_get_double (file, *iter, HYSCAN_PROFILE_OFFSET_THETA, NULL);

      /* Если название группы совпадает с названием того или иного
       * HyScanSourceType, то это локатор. Иначе -- датчик. */
      if (hyscan_channel_get_types_by_id (*iter, &source, &type, &channel))
        g_hash_table_insert (priv->sources, GINT_TO_POINTER (source), hyscan_antenna_offset_copy (&offset));
      else
        g_hash_table_insert (priv->sensors, g_strdup (*iter), hyscan_antenna_offset_copy (&offset));
    }

  g_strfreev (groups);

  return TRUE;
}

/* Обработка информационной группы (HYSCAN_PROFILE_HW_INFO_GROUP) */
static gboolean
hyscan_profile_offset_info_group (HyScanProfileOffset *profile,
                                  GKeyFile            *kf,
                                  const gchar         *group)
{
  gchar *name;

  if (!g_str_equal (group, HYSCAN_PROFILE_OFFSET_INFO_GROUP))
    return FALSE;

  name = g_key_file_get_locale_string (kf, group, HYSCAN_PROFILE_OFFSET_NAME, NULL, NULL);
  hyscan_profile_set_name (HYSCAN_PROFILE (profile), name);

  g_free (name);
  return TRUE;
}


/**
 * hyscan_profile_offset_new:
 * @file: полный путь к файлу профиля
 *
 * Функция создает объект работы с профилем местоположения антенн.
 *
 * Returns: (transfer full): #HyScanProfileOffset.
 */
HyScanProfileOffset *
hyscan_profile_offset_new (const gchar *file)
{
  return g_object_new (HYSCAN_TYPE_PROFILE_OFFSET,
                       "file", file,
                       NULL);
}

/**
 * hyscan_profile_offset_get_sources:
 * @profile: #HyScanProfileOffset
 *
 * Функция возвращает список #HyScanAntennaOffset для локаторов.
 *
 * Returns: (transfer full) (element-type HyScanSourceType HyScanAntennaOffset):
 * таблица смещений для локаторов.
 */
GHashTable *
hyscan_profile_offset_get_sources (HyScanProfileOffset *profile)
{
  g_return_val_if_fail (HYSCAN_IS_PROFILE_OFFSET (profile), NULL);

  return g_hash_table_ref (profile->priv->sources);
}

/**
 * hyscan_profile_offset_get_sensors:
 * @profile: #HyScanProfileOffset
 *
 * Функция возвращает список #HyScanAntennaOffset для датчиков.
 *
 * Returns: (transfer full) (element-type utf8 HyScanAntennaOffset):
 * таблица смещений для датчиков.
 */
GHashTable *
hyscan_profile_offset_get_sensors (HyScanProfileOffset *profile)
{
  g_return_val_if_fail (HYSCAN_IS_PROFILE_OFFSET (profile), NULL);

  return g_hash_table_ref (profile->priv->sensors);
}

/**
 * hyscan_profile_offset_apply:
 * @profile: #HyScanProfileOff
 * @control: #HyScanControlset
 *
 * Функция задает местоположения антенн. Если какое-то значение задать не
 * удалось, функция выведет предупреждение в лог. #HyScanControl, передаваемый
 * методу должен быть связан (#hyscan_control_device_bind).
 *
 * Returns: TRUE, всегда.
 */
gboolean
hyscan_profile_offset_apply (HyScanProfileOffset *profile,
                             HyScanControl       *control)
{
  gpointer k;
  HyScanAntennaOffset *v;
  GHashTableIter iter;
  HyScanProfileOffsetPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_PROFILE_OFFSET (profile), FALSE);
  priv = profile->priv;

  g_hash_table_iter_init (&iter, priv->sources);
  while (g_hash_table_iter_next (&iter, &k, (gpointer*)&v))
    {
      if (!hyscan_sonar_antenna_set_offset (HYSCAN_SONAR (control), (HyScanSourceType)k, v))
        g_message ("HyScanProfileOffset: sonar %s failed", hyscan_source_get_id_by_type ((HyScanSourceType)k));
    }

  g_hash_table_iter_init (&iter, priv->sensors);
  while (g_hash_table_iter_next (&iter, &k, (gpointer*)&v))
    {
      if (!hyscan_sensor_antenna_set_offset (HYSCAN_SENSOR (control), (const gchar*)k, v))
        g_message ("HyScanProfileOffset: sensor %s failed", (gchar*)k);
    }

  return TRUE;
}

/**
 * hyscan_profile_offset_apply_default:
 * @profile: #HyScanProfileOffset
 * @control: #HyScanControl
 *
 * Функция ведет себя аналогично #hyscan_profile_offset_apply, но задает
 * местоположения антенн по умолчанию, то есть такие, которые невозможно
 * изменить после вызова функции #hyscan_control_device_bind.
 *
 * Returns: TRUE, всегда.
 */
gboolean
hyscan_profile_offset_apply_default (HyScanProfileOffset *profile,
                                     HyScanControl       *control)
{
  gpointer k;
  HyScanAntennaOffset *v;
  GHashTableIter iter;
  HyScanProfileOffsetPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_PROFILE_OFFSET (profile), FALSE);
  priv = profile->priv;

  g_hash_table_iter_init (&iter, priv->sources);
  while (g_hash_table_iter_next (&iter, &k, (gpointer*)&v))
    {
      if (!hyscan_control_source_set_default_offset (control, (HyScanSourceType)k, v))
        g_message ("HyScanProfileOffset: sonar %s failed", hyscan_source_get_id_by_type ((HyScanSourceType)k));
    }

  g_hash_table_iter_init (&iter, priv->sensors);
  while (g_hash_table_iter_next (&iter, &k, (gpointer*)&v))
    {
      if (!hyscan_control_sensor_set_default_offset (control, (const gchar*)k, v))
        g_message ("HyScanProfileOffset: sensor %s failed", (gchar*)k);
    }

  return TRUE;
}
