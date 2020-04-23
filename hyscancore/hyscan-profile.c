/* hyscan-profile.c
 *
 * Copyright 2019-2020 Screen LLC, Alexander Dmitriev <m1n7@yandex.ru>
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
 * SECTION: hyscan-profile
 * @Short_description: базовый класс профилей
 * @Title: HyScanProfile
 *
 * Класс HyScanProfile является виртуальным базовым классом, реализующим
 * общую функциональность: на данный момент это чтение профиля и получение
 * названия. За дальнейшей информацией следует обратиться к документации
 * дочерних классов.
 *
 * Между тем, общий алгоритм работы такой: создать объект работы с профилем,
 * полностью настроить его, прочитать с помощью #hyscan_profile_read,
 * отобразить с помощью #hyscan_profile_get_name, применить с помощью
 * соответствующей функции дочернего класса.
 */

#include "hyscan-profile.h"
#include <glib/gstdio.h>

enum
{
  PROP_0,
  PROP_FILE,
};

struct _HyScanProfilePrivate
{
  gchar    *file;   /* Путь к файлу с профилем. */
  GKeyFile *kf;     /* GKeyFile с профилем. */
  gchar    *name;   /* Название профиля. */
};

static void     hyscan_profile_set_property       (GObject               *object,
                                                   guint                  prop_id,
                                                   const GValue          *value,
                                                   GParamSpec            *pspec);
static void     hyscan_profile_object_finalize    (GObject               *object);

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (HyScanProfile, hyscan_profile, G_TYPE_OBJECT);

static void
hyscan_profile_class_init (HyScanProfileClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);

  oclass->set_property = hyscan_profile_set_property;
  oclass->finalize = hyscan_profile_object_finalize;

  g_object_class_install_property (oclass, PROP_FILE,
    g_param_spec_string ("file", "File", "Path to profile",
                         NULL, G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_profile_init (HyScanProfile *profile)
{
  profile->priv = hyscan_profile_get_instance_private (profile);
}

static void
hyscan_profile_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  HyScanProfile *self = HYSCAN_PROFILE (object);
  HyScanProfilePrivate *priv = self->priv;

  switch (prop_id)
    {
    case PROP_FILE:
      priv->file = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static gboolean
hyscan_profile_read_info_group (HyScanProfile *self,
                                GKeyFile      *kf)
{
  HyScanProfileClass *klass = HYSCAN_PROFILE_GET_CLASS (self);
  gchar *name;
  guint64 version;

  if (!g_key_file_has_group (kf, HYSCAN_PROFILE_INFO_GROUP))
    return FALSE;

  version = g_key_file_get_uint64 (kf, HYSCAN_PROFILE_INFO_GROUP,
                                   HYSCAN_PROFILE_VERSION, NULL);
  if (version != klass->version)
    return FALSE;

  name = g_key_file_get_string (kf, HYSCAN_PROFILE_INFO_GROUP,
                                HYSCAN_PROFILE_NAME, NULL);
  hyscan_profile_set_name (self, name);
  g_free (name);
  return TRUE;
}

static void
hyscan_profile_write_info_group (HyScanProfile *self,
                                 GKeyFile      *kf)
{
  HyScanProfileClass *klass = HYSCAN_PROFILE_GET_CLASS (self);

  g_key_file_set_uint64 (kf, HYSCAN_PROFILE_INFO_GROUP,
                         HYSCAN_PROFILE_VERSION, klass->version);

  g_key_file_set_string (kf, HYSCAN_PROFILE_INFO_GROUP,
                         HYSCAN_PROFILE_NAME, hyscan_profile_get_name (self));
}

static void
hyscan_profile_object_finalize (GObject *object)
{
  HyScanProfile *self = HYSCAN_PROFILE (object);
  HyScanProfilePrivate *priv = self->priv;

  g_clear_pointer (&priv->file, g_free);
  g_clear_pointer (&priv->name, g_free);
  g_clear_pointer (&priv->kf, g_key_file_unref);

  G_OBJECT_CLASS (hyscan_profile_parent_class)->finalize (object);
}

/**
 * hyscan_profile_read:
 * @self: указатель на #HyScanProfile
 *
 * Функция производит чтение профиля.
 * Объект профиля должен быть полностью настроен перед вызовом этой функции
 * (например, установлены пути с драйверами)
 *
 * Returns: результат чтения профиля.
 */
gboolean
hyscan_profile_read (HyScanProfile *self)
{
  HyScanProfileClass *klass;
  HyScanProfilePrivate *priv;
  GError *error = NULL;
  gboolean status;

  g_return_val_if_fail (HYSCAN_IS_PROFILE (self), FALSE);
  klass = HYSCAN_PROFILE_GET_CLASS (self);
  priv = self->priv;

  /* Если файл не задан, выходим. */
  if (priv->file == NULL || klass->read == NULL)
    return FALSE;

  g_clear_pointer (&self->priv->kf, g_key_file_unref);
  priv->kf = g_key_file_new ();

  status = g_key_file_load_from_file (priv->kf, priv->file, G_KEY_FILE_NONE, &error);

  if (!status && error->code != G_FILE_ERROR_NOENT)
    {
      g_warning ("HyScanProfile: can't load file <%s>: %s", priv->file, error->message);
      g_error_free (error);
      return FALSE;
    }

  /* Собственно чтение профиля. */
  if (!hyscan_profile_read_info_group (self, priv->kf))
    return FALSE;

  return klass->read (self, priv->kf);
}

/**
 * hyscan_profile_write:
 * @self: указатель на #HyScanProfile
 *
 * Функция производит запись профиля.
 *
 * Returns: результат записи профиля.
 */
gboolean
hyscan_profile_write (HyScanProfile *self)
{
  HyScanProfileClass *klass;
  HyScanProfilePrivate *priv;
  GError *error = NULL;

  g_return_val_if_fail (HYSCAN_IS_PROFILE (self), FALSE);
  klass = HYSCAN_PROFILE_GET_CLASS (self);
  priv = self->priv;

  /* Если файл не задан, выходим. */
  if (priv->file == NULL || klass->write == NULL)
    return FALSE;

  g_clear_pointer (&priv->kf, g_key_file_unref);
  priv->kf = g_key_file_new ();

  if (!klass->write (self, priv->kf))
    return FALSE;

  /* Собственно запись профиля. */
  hyscan_profile_write_info_group (self, priv->kf);

  if (!g_key_file_save_to_file (priv->kf, priv->file, &error))
    {
      g_warning ("HyScanProfile: can't write file <%s>: %s", priv->file, error->message);
      g_error_free (error);
      return FALSE;
    }

  return TRUE;
}

/**
 * hyscan_profile_sanity:
 * @self: указатель на #HyScanProfile
 *
 * Функция проверяет валидность профиля.
 *
 * Returns: %TRUE, если профиль валиден.
 */
gboolean
hyscan_profile_sanity (HyScanProfile *self)
{
  HyScanProfileClass *klass;

  g_return_val_if_fail (HYSCAN_IS_PROFILE (self), FALSE);
  klass = HYSCAN_PROFILE_GET_CLASS (self);

  if (self->priv->name == NULL || g_str_equal (self->priv->name, ""))
    return FALSE;

  if (klass->sanity == NULL)
    return TRUE;

  return klass->sanity (self);
}

/**
 * hyscan_profile_write:
 * @self: указатель на #HyScanProfile
 *
 * Функция удаляет профиль с диска. По сути, это просто обертка над g_remove().
 *
 * Returns: %TRUE, если профиль удален.
 */
gboolean
hyscan_profile_delete (HyScanProfile *self)
{
  g_return_val_if_fail (HYSCAN_IS_PROFILE (self), FALSE);

  return 0 == g_remove (self->priv->file);
}

/**
 * hyscan_profile_get_file:
 * @self: указатель на #HyScanProfile
 *
 * Функция возвращает путь к файлу профиля.
 *
 * Returns: путь к файлу профиля.
 */
const gchar *
hyscan_profile_get_file (HyScanProfile *self)
{
  g_return_val_if_fail (HYSCAN_IS_PROFILE (self), NULL);

  return self->priv->file;
}

/**
 * hyscan_profile_set_name:
 * @self: указатель на #HyScanProfile
 * @name: название профиля
 *
 * Функция задает название профиля.
 */
void
hyscan_profile_set_name (HyScanProfile *self,
                         const gchar   *name)
{
  HyScanProfilePrivate *priv;

  g_return_if_fail (HYSCAN_IS_PROFILE (self));
  priv = self->priv;

  g_clear_pointer (&priv->name, g_free);
  priv->name = g_strdup (name);
}

/**
 * hyscan_profile_get_name:
 * @self: указатель на #HyScanProfile
 *
 * Функция возвращает название профиля.
 *
 * Returns: (transfer none): название профиля.
 */
const gchar *
hyscan_profile_get_name (HyScanProfile *self)
{
  g_return_val_if_fail (HYSCAN_IS_PROFILE (self), NULL);

  return self->priv->name;
}

/**
 * hyscan_profile_make_id:
 * @buffer: указатель на массив
 * @size: размер массива
 *
 * Функция генерирует случайный нуль-терминированный идентификатор.
 *
 * Returns: (transfer none): указатель на переданный массив.
 */
gchar *
hyscan_profile_make_id (gchar *buffer,
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

  return buffer;
}
