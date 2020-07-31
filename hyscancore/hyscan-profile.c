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
 * общую функциональность: чтение, запись, проверка валидности, удаление
 * профилей.
 *
 * Также этот класс хранит общие поля для всех профилей: название и
 * версия. Эти поля хранятся в группе [_]. Использовать эту группу хотя и не
 * запрещено на программном уровне, крайне не рекомендуется, т.к. дальнейшее
 * развитие базового класса может привести к коллизиям. Если же кто-то
 * перезапишет поля, требуемые данным классом, весь профиль может стать
 * нечитаемым. Использование групп с другими названиями ничем не ограничивается.
 *
 * Общий алгоритм работы с профилем такой: создать объект работы с профилем,
 * полностью настроить его, считать с помощью hyscan_profile_read(),
 * отобразить и/или отредактировать с помощью hyscan_profile_get_name(),
 * hyscan_profile_set_name() (и функций дочерних классов),
 * при необходимости записать с помощью hyscan_profile_write().
 *
 * Для нужд графического интерфейса также есть функция hyscan_profile_sanity().
 * Она валидирует профиль в том смысле, что все необходимые поля заполнены.
 * При этом не гарантируется, что, например, в случае профиля БД к указанной
 * БД можно подключиться. Эта проверка не выполняется ни после чтения, ни перед
 * записью профиля.
 */

#include "hyscan-profile.h"
#include <glib/gstdio.h>

#define HYSCAN_PROFILE_NAME "name"
#define HYSCAN_PROFILE_VERSION "version"
#define HYSCAN_PROFILE_LAST_USED "last_used"

enum
{
  PROP_0,
  PROP_FILE,
};

struct _HyScanProfilePrivate
{
  gchar    *file;      /* Путь к файлу с профилем. */
  GKeyFile *kf;        /* GKeyFile с профилем. */
  gchar    *name;      /* Название профиля. */

  gint64    last_used; /* Время последнего использования. */
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

  self->priv->last_used = g_key_file_get_int64 (kf, HYSCAN_PROFILE_INFO_GROUP,
                                                HYSCAN_PROFILE_LAST_USED, NULL);
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

  g_key_file_set_int64 (kf, HYSCAN_PROFILE_INFO_GROUP,
                        HYSCAN_PROFILE_LAST_USED, self->priv->last_used);
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

  g_return_val_if_fail (HYSCAN_IS_PROFILE (self), FALSE);
  klass = HYSCAN_PROFILE_GET_CLASS (self);
  priv = self->priv;

  /* Если файл не задан, выходим. */
  if (priv->file == NULL || klass->read == NULL)
    return FALSE;

  g_clear_pointer (&self->priv->kf, g_key_file_unref);
  priv->kf = g_key_file_new ();

  if (!g_key_file_load_from_file (priv->kf, priv->file, G_KEY_FILE_NONE, &error))
    {
      /* G_FILE_ERROR_NOENT может возникнуть, когда профиль был удален
       * и повторно считывается. Я не хочу выводить ошибку и просто верну
       * FALSE. */
      if (error->code != G_FILE_ERROR_NOENT)
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
 * hyscan_profile_delete:
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

void
hyscan_profile_use (HyScanProfile *self)
{
  GDateTime *dt;
  g_return_if_fail (HYSCAN_IS_PROFILE (self));

  dt = g_date_time_new_now_local ();
  self->priv->last_used = g_date_time_to_unix (dt);
  g_date_time_unref (dt);
}

GDateTime *
hyscan_profile_last_used (HyScanProfile *self)
{
  g_return_val_if_fail (HYSCAN_IS_PROFILE (self), NULL);

  if (self->priv->last_used <= 0)
    return NULL;

  return g_date_time_new_from_unix_local (self->priv->last_used);
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
