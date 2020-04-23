/* hyscan-profile.h
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

#ifndef __HYSCAN_PROFILE_H__
#define __HYSCAN_PROFILE_H__

#include <glib-object.h>
#include <hyscan-api.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_PROFILE             (hyscan_profile_get_type ())
#define HYSCAN_PROFILE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_PROFILE, HyScanProfile))
#define HYSCAN_IS_PROFILE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_PROFILE))
#define HYSCAN_PROFILE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_PROFILE, HyScanProfileClass))
#define HYSCAN_IS_PROFILE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_PROFILE))
#define HYSCAN_PROFILE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_PROFILE, HyScanProfileClass))

typedef struct _HyScanProfile HyScanProfile;
typedef struct _HyScanProfilePrivate HyScanProfilePrivate;
typedef struct _HyScanProfileClass HyScanProfileClass;

struct _HyScanProfile
{
  GObject parent_instance;

  HyScanProfilePrivate *priv;
};

/**
 * HYSCAN_PROFILE_INFO_GROUP:
 * Название информационной группы.
 */
#define HYSCAN_PROFILE_INFO_GROUP "_"
/**
 * HYSCAN_PROFILE_NAME:
 * Поле с названием профиля.
 */
#define HYSCAN_PROFILE_NAME "name"
/**
 * HYSCAN_PROFILE_VERSION:
 * Поле с версией профиля.
 */
#define HYSCAN_PROFILE_VERSION "version"

/**
 * HyScanProfileClass:
 * @parent_class: Базовый интерфейс.
 * @read: Функция разбора GKeyFile с профилем.
 * @write: Функция записи профиля в GKeyFile.
 * @sanity: Функция валидации профиля.
 * @version: Версия профиля.
 */
struct _HyScanProfileClass
{
  GObjectClass parent_class;

  gboolean     (*read)      (HyScanProfile *self,
                             GKeyFile      *file);

  gboolean     (*write)     (HyScanProfile *self,
                             GKeyFile      *file);

  gboolean     (*sanity)    (HyScanProfile *self);

  guint64      version;
};

HYSCAN_API
GType                  hyscan_profile_get_type         (void);

HYSCAN_API
gboolean               hyscan_profile_read             (HyScanProfile *profile);

HYSCAN_API
gboolean               hyscan_profile_write            (HyScanProfile *profile);

HYSCAN_API
gboolean               hyscan_profile_sanity           (HyScanProfile *profile);

HYSCAN_API
gboolean               hyscan_profile_delete           (HyScanProfile *profile);

HYSCAN_API
const gchar *          hyscan_profile_get_file         (HyScanProfile *profile);

HYSCAN_API
void                   hyscan_profile_set_name         (HyScanProfile *profile,
                                                        const gchar   *file);
HYSCAN_API
const gchar *          hyscan_profile_get_name         (HyScanProfile *self);

G_END_DECLS

#endif /* __HYSCAN_PROFILE_H__ */
