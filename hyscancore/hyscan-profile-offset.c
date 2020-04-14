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

#define HYSCAN_PROFILE_OFFSET_STARBOARD "starboard"
#define HYSCAN_PROFILE_OFFSET_FORWARD "forward"
#define HYSCAN_PROFILE_OFFSET_VERTICAL "vertical"
#define HYSCAN_PROFILE_OFFSET_YAW "yaw"
#define HYSCAN_PROFILE_OFFSET_PITCH "pitch"
#define HYSCAN_PROFILE_OFFSET_ROLL "roll"

struct _HyScanProfileOffsetPrivate
{
  GHashTable *sources;  /* Таблица смещений для локаторов. {HyScanSourceType : HyScanAntennaOffset} */
  GHashTable *sensors; /* Таблица смещений для антенн. {gchar* : HyScanAntennaOffset} */
};

static void     hyscan_profile_offset_object_finalize  (GObject             *object);
static void     hyscan_profile_offset_clear            (HyScanProfileOffset *profile);
static gboolean hyscan_profile_offset_info_group       (HyScanProfileOffset *profile,
                                                        GKeyFile            *kf,
                                                        const gchar         *group);
static gboolean hyscan_profile_offset_read             (HyScanProfile       *profile,
                                                        GKeyFile            *file);
static gboolean hyscan_profile_offset_write            (HyScanProfile       *profile,
                                                        GKeyFile            *file);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanProfileOffset, hyscan_profile_offset, HYSCAN_TYPE_PROFILE);

static void
hyscan_profile_offset_class_init (HyScanProfileOffsetClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);
  HyScanProfileClass *pklass = HYSCAN_PROFILE_CLASS (klass);

  oclass->finalize = hyscan_profile_offset_object_finalize;
  pklass->read = hyscan_profile_offset_read;
  pklass->write = hyscan_profile_offset_write;
}

static void
hyscan_profile_offset_init (HyScanProfileOffset *profile)
{
  HyScanProfileOffsetPrivate *priv;

  profile->priv = priv = hyscan_profile_offset_get_instance_private (profile);

  priv->sources = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                         NULL, (GDestroyNotify)hyscan_antenna_offset_free);
  priv->sensors = g_hash_table_new_full (g_str_hash, g_str_equal,
                                         g_free, (GDestroyNotify)hyscan_antenna_offset_free);
}

static void
hyscan_profile_offset_object_finalize (GObject *object)
{
  HyScanProfileOffset *self = HYSCAN_PROFILE_OFFSET (object);

  g_clear_pointer (&self->priv->sources, g_hash_table_unref);
  g_clear_pointer (&self->priv->sensors, g_hash_table_unref);

  G_OBJECT_CLASS (hyscan_profile_offset_parent_class)->finalize (object);
}

/* Функция очищает профиль. */
static void
hyscan_profile_offset_clear (HyScanProfileOffset *profile)
{
  HyScanProfileOffsetPrivate *priv = profile->priv;

  g_hash_table_remove_all (priv->sources);
  g_hash_table_remove_all (priv->sensors);
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

static void
hyscan_profile_offset_write_helper (GKeyFile            *kf,
                                    const gchar         *group,
                                    HyScanAntennaOffset *offset)
{
  g_key_file_set_double (kf, group, HYSCAN_PROFILE_OFFSET_STARBOARD, offset->starboard);
  g_key_file_set_double (kf, group, HYSCAN_PROFILE_OFFSET_FORWARD, offset->forward);
  g_key_file_set_double (kf, group, HYSCAN_PROFILE_OFFSET_VERTICAL, offset->vertical);
  g_key_file_set_double (kf, group, HYSCAN_PROFILE_OFFSET_YAW, offset->yaw);
  g_key_file_set_double (kf, group, HYSCAN_PROFILE_OFFSET_PITCH, offset->pitch);
  g_key_file_set_double (kf, group, HYSCAN_PROFILE_OFFSET_ROLL, offset->roll);
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

      offset.starboard = g_key_file_get_double (file, *iter, HYSCAN_PROFILE_OFFSET_STARBOARD, NULL);
      offset.forward = g_key_file_get_double (file, *iter, HYSCAN_PROFILE_OFFSET_FORWARD, NULL);
      offset.vertical = g_key_file_get_double (file, *iter, HYSCAN_PROFILE_OFFSET_VERTICAL, NULL);
      offset.yaw = g_key_file_get_double (file, *iter, HYSCAN_PROFILE_OFFSET_YAW, NULL);
      offset.pitch = g_key_file_get_double (file, *iter, HYSCAN_PROFILE_OFFSET_PITCH, NULL);
      offset.roll = g_key_file_get_double (file, *iter, HYSCAN_PROFILE_OFFSET_ROLL, NULL);

      /* Если название группы совпадает с названием того или иного
       * HyScanSourceType, то это локатор. Иначе -- датчик. */
      if (hyscan_channel_get_types_by_id (*iter, &source, &type, &channel))
        hyscan_profile_offset_add_source (self, source, &offset);
      else
        hyscan_profile_offset_add_sensor (self, *iter, &offset);
    }

  g_strfreev (groups);

  return TRUE;
}

/* Функция парсинга профиля. */
static gboolean
hyscan_profile_offset_write (HyScanProfile *profile,
                             GKeyFile      *file)
{
  gpointer k;
  HyScanAntennaOffset *v;
  GHashTableIter iter;
  HyScanProfileOffset *self = HYSCAN_PROFILE_OFFSET (profile);
  HyScanProfileOffsetPrivate *priv = self->priv;

  g_hash_table_iter_init (&iter, priv->sources);
  while (g_hash_table_iter_next (&iter, &k, (gpointer*)&v))
    {
      hyscan_profile_offset_write_helper (file,
                                          hyscan_source_get_id_by_type ((HyScanSourceType)k),
                                          v);
    }

  g_hash_table_iter_init (&iter, priv->sensors);
  while (g_hash_table_iter_next (&iter, &k, (gpointer*)&v))
    {
      hyscan_profile_offset_write_helper (file,
                                          (const gchar*)k,
                                          v);
    }

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
 * hyscan_profile_offset_list_sources:
 * @profile: #HyScanProfileOffset
 *
 * Функция возвращает список #HyScanAntennaOffset для локаторов.
 *
 * Returns: (transfer full) (element-type HyScanSourceType HyScanAntennaOffset):
 * таблица смещений для локаторов.
 */
GHashTable *
hyscan_profile_offset_list_sources (HyScanProfileOffset *profile)
{
  g_return_val_if_fail (HYSCAN_IS_PROFILE_OFFSET (profile), NULL);

  return g_hash_table_ref (profile->priv->sources);
}

/**
 * hyscan_profile_offset_list_sensors:
 * @profile: #HyScanProfileOffset
 *
 * Функция возвращает список #HyScanAntennaOffset для датчиков.
 *
 * Returns: (transfer full) (element-type utf8 HyScanAntennaOffset):
 * таблица смещений для датчиков.
 */
GHashTable *
hyscan_profile_offset_list_sensors (HyScanProfileOffset *profile)
{
  g_return_val_if_fail (HYSCAN_IS_PROFILE_OFFSET (profile), NULL);

  return g_hash_table_ref (profile->priv->sensors);
}

/**
 * hyscan_profile_offset_add_source:
 * @profile: #HyScanProfileOffset
 * @source: тип данных
 * @offset: значения сдвигов
 *
 * Функция добавляет (или обновляет) значения смещений для типа данных.
 */
void
hyscan_profile_offset_add_source (HyScanProfileOffset *profile,
                                  HyScanSourceType     source,
                                  HyScanAntennaOffset *offset)
{
  g_return_if_fail (HYSCAN_IS_PROFILE_OFFSET (profile));

  g_hash_table_insert (profile->priv->sources, GINT_TO_POINTER (source),
                       hyscan_antenna_offset_copy (offset));
}

/**
 * hyscan_profile_offset_add_sensor:
 * @profile: #HyScanProfileOffset
 * @sensor: название датчика
 * @offset: значения сдвигов
 *
 * Функция добавляет (или обновляет) значения смещений для датчика.
 */
void
hyscan_profile_offset_add_sensor (HyScanProfileOffset *profile,
                                  const gchar         *sensor,
                                  HyScanAntennaOffset *offset)
{
  g_return_if_fail (HYSCAN_IS_PROFILE_OFFSET (profile));

  g_hash_table_insert (profile->priv->sensors, g_strdup (sensor),
                       hyscan_antenna_offset_copy (offset));
}

/**
 * hyscan_profile_offset_remove_source:
 * @profile: #HyScanProfileOffset
 * @source: тип данных
 * @offset: значения сдвигов
 *
 * Функция удаляет значения смещений для типа данных.
 */
gboolean
hyscan_profile_offset_remove_source (HyScanProfileOffset *profile,
                                     HyScanSourceType     source)
{
  g_return_val_if_fail (HYSCAN_IS_PROFILE_OFFSET (profile), FALSE);

  return g_hash_table_remove (profile->priv->sources, GINT_TO_POINTER (source));
}

/**
 * hyscan_profile_offset_remove_sensor:
 * @profile: #HyScanProfileOffset
 * @sensor: название датчика
 * @offset: значения сдвигов
 *
 * Функция удаляет значения смещений для датчика.
 */
gboolean
hyscan_profile_offset_remove_sensor (HyScanProfileOffset *profile,
                                     const gchar         *sensor)
{
  g_return_val_if_fail (HYSCAN_IS_PROFILE_OFFSET (profile), FALSE);

  return g_hash_table_remove (profile->priv->sensors, sensor);
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
