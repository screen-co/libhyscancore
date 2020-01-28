/* hyscan-profile-hw.h
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

#ifndef __HYSCAN_PROFILE_HW_H__
#define __HYSCAN_PROFILE_HW_H__

#include <hyscan-control.h>
#include "hyscan-profile.h"

G_BEGIN_DECLS

#define HYSCAN_TYPE_PROFILE_HW             (hyscan_profile_hw_get_type ())
#define HYSCAN_PROFILE_HW(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_PROFILE_HW, HyScanProfileHW))
#define HYSCAN_IS_PROFILE_HW(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_PROFILE_HW))
#define HYSCAN_PROFILE_HW_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_PROFILE_HW, HyScanProfileHWClass))
#define HYSCAN_IS_PROFILE_HW_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_PROFILE_HW))
#define HYSCAN_PROFILE_HW_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_PROFILE_HW, HyScanProfileHWClass))

typedef struct _HyScanProfileHW HyScanProfileHW;
typedef struct _HyScanProfileHWPrivate HyScanProfileHWPrivate;
typedef struct _HyScanProfileHWClass HyScanProfileHWClass;

struct _HyScanProfileHW
{
  HyScanProfile parent_instance;

  HyScanProfileHWPrivate *priv;
};

struct _HyScanProfileHWClass
{
  HyScanProfileClass parent_class;
};

HYSCAN_API
GType                  hyscan_profile_hw_get_type         (void);

HYSCAN_API
HyScanProfileHW *      hyscan_profile_hw_new              (const gchar     *file);

HYSCAN_API
void                   hyscan_profile_hw_set_driver_paths (HyScanProfileHW *profile,
                                                           gchar          **driver_paths);

HYSCAN_API
GList *                hyscan_profile_hw_list             (HyScanProfileHW *profile);

HYSCAN_API
gboolean               hyscan_profile_hw_check            (HyScanProfileHW *profile);

HYSCAN_API
HyScanControl *        hyscan_profile_hw_connect          (HyScanProfileHW *profile);

HYSCAN_API
HyScanControl *        hyscan_profile_hw_connect_simple   (const gchar     *file,
                                                           gchar          **driver_paths);

G_END_DECLS

#endif /* __HYSCAN_PROFILE_HW_H__ */
