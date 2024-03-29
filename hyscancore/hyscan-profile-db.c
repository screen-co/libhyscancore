/* hyscan-profile-db.c
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
 * SECTION: hyscan-profile-db
 * @Short_description: профиль БД
 * @Title: HyScanProfileDB
 *
 * Класс HyScanProfileDB реализует профили БД. Профиль БД содержит следующие
 * поля:
 * "uri" - путь к БД
 * "name" - человекочитаемое название профиля.
 * Все поля должны быть в группе "db".
 *
 * После чтения профиля можно подключиться к БД с помощью функции
 * #hyscan_profile_db_connect
 */

#include "hyscan-profile-db.h"

#define HYSCAN_PROFILE_DB_GROUP_NAME "db"
#define HYSCAN_PROFILE_DB_URI_KEY "uri"
#define HYSCAN_PROFILE_DB_NAME_KEY "name"
#define HYSCAN_PROFILE_DB_UNNAMED_VALUE "unnamed"

enum
{
  PROP_O,
  PROP_URI,
  PROP_NAME
};

struct _HyScanProfileDBPrivate
{
  gchar *name;   /* Имя БД. */
  gchar *uri;    /* Идентификатор БД.*/
};

static void   hyscan_profile_db_object_finalize (GObject                      *object);
static void   hyscan_profile_db_clear           (HyScanProfileDB              *profile);
static gboolean hyscan_profile_db_read          (HyScanProfile                *profile,
                                                 GKeyFile                     *file);

G_DEFINE_TYPE_WITH_CODE (HyScanProfileDB, hyscan_profile_db, HYSCAN_TYPE_PROFILE,
                         G_ADD_PRIVATE (HyScanProfileDB));

static void
hyscan_profile_db_class_init (HyScanProfileDBClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);
  HyScanProfileClass *pklass = HYSCAN_PROFILE_CLASS (klass);

  oclass->finalize = hyscan_profile_db_object_finalize;

  pklass->read = hyscan_profile_db_read;
}

static void
hyscan_profile_db_init (HyScanProfileDB *profile)
{
  profile->priv = hyscan_profile_db_get_instance_private (profile);
}

static void
hyscan_profile_db_object_finalize (GObject *object)
{
  HyScanProfileDB *self = HYSCAN_PROFILE_DB (object);

  hyscan_profile_db_clear (self);

  G_OBJECT_CLASS (hyscan_profile_db_parent_class)->finalize (object);
}

static void
hyscan_profile_db_clear (HyScanProfileDB *profile)
{
  HyScanProfileDBPrivate *priv = profile->priv;

  g_clear_pointer (&priv->uri, g_free);
  g_clear_pointer (&priv->name, g_free);
}

/* Функция парсинга профиля. */
static gboolean
hyscan_profile_db_read (HyScanProfile *profile,
                        GKeyFile      *file)
{
  HyScanProfileDB *self = HYSCAN_PROFILE_DB (profile);
  HyScanProfileDBPrivate *priv = self->priv;

  /* Очистка профиля. */
  hyscan_profile_db_clear (self);

  priv->name = g_key_file_get_locale_string (file, HYSCAN_PROFILE_DB_GROUP_NAME,
                                             HYSCAN_PROFILE_DB_NAME_KEY, NULL, NULL);
  priv->uri = g_key_file_get_string (file, HYSCAN_PROFILE_DB_GROUP_NAME,
                                     HYSCAN_PROFILE_DB_URI_KEY, NULL);
  if (priv->uri == NULL)
    {
      g_warning ("HyScanProfileDB: %s", "uri not found.");
      return FALSE;
    }

  hyscan_profile_set_name (profile, priv->name != NULL ? priv->name : priv->uri);

  return TRUE;
}

/**
 * hyscan_profile_db_new:
 * @file: полный путь к файлу профиля
 *
 * Функция создает объект работы с профилем БД.
 *
 * Returns: (transfer full): #HyScanProfileDB.
 */
HyScanProfileDB *
hyscan_profile_db_new (const gchar *file)
{
  return g_object_new (HYSCAN_TYPE_PROFILE_DB,
                       "file", file,
                       NULL);
}

/**
 * hyscan_profile_read:
 * @self: указатель на #HyScanProfile
 *
 * Функция производит чтение профиля. Текущая реализация запрещает читать
 * профиль более одного раза. Объект профиля должен быть полностью настроен
 * перед вызовом этой функции.
 *
 * Returns: (transfer full): #HyScanDB или NULL в случае ошибки.
 */
HyScanDB *
hyscan_profile_db_connect (HyScanProfileDB *profile)
{
  HyScanProfileDBPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_PROFILE_DB (profile), NULL);
  priv = profile->priv;

  if (priv->uri == NULL)
    {
      g_warning ("HyScanProfileDB: %s", "uri not set");
      return NULL;
    }

  return hyscan_db_new (priv->uri);
}
