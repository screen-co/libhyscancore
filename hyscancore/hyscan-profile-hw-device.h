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

#ifndef __HYSCAN_PROFILE_HW_DEVICE_H__
#define __HYSCAN_PROFILE_HW_DEVICE_H__

#include <hyscan-param-list.h>
#include <hyscan-discover.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_PROFILE_HW_DEVICE             (hyscan_profile_hw_device_get_type ())
#define HYSCAN_PROFILE_HW_DEVICE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_PROFILE_HW_DEVICE, HyScanProfileHWDevice))
#define HYSCAN_IS_PROFILE_HW_DEVICE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_PROFILE_HW_DEVICE))
#define HYSCAN_PROFILE_HW_DEVICE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_PROFILE_HW_DEVICE, HyScanProfileHWDeviceClass))
#define HYSCAN_IS_PROFILE_HW_DEVICE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_PROFILE_HW_DEVICE))
#define HYSCAN_PROFILE_HW_DEVICE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_PROFILE_HW_DEVICE, HyScanProfileHWDeviceClass))

typedef struct _HyScanProfileHWDevice HyScanProfileHWDevice;
typedef struct _HyScanProfileHWDevicePrivate HyScanProfileHWDevicePrivate;
typedef struct _HyScanProfileHWDeviceClass HyScanProfileHWDeviceClass;

struct _HyScanProfileHWDevice
{
  GObject parent_instance;

  HyScanProfileHWDevicePrivate *priv;
};

struct _HyScanProfileHWDeviceClass
{
  GObjectClass parent_class;
};

GType                   hyscan_profile_hw_device_get_type         (void);

HyScanProfileHWDevice * hyscan_profile_hw_device_new              (GKeyFile              *keyfile);
void                    hyscan_profile_hw_device_set_group        (HyScanProfileHWDevice *hw_device,
                                                                   const gchar           *group);
void                    hyscan_profile_hw_device_set_paths        (HyScanProfileHWDevice *hw_device,
                                                                   gchar                **paths);
void                    hyscan_profile_hw_device_read             (HyScanProfileHWDevice *hw_device,
                                                                   GKeyFile              *kf);
gboolean                hyscan_profile_hw_device_check            (HyScanProfileHWDevice *hw_device);
HyScanDevice *          hyscan_profile_hw_device_connect          (HyScanProfileHWDevice *hw_device);

G_END_DECLS

#endif /* __HYSCAN_PROFILE_HW_DEVICE_H__ */
