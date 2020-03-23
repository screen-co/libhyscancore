/* hyscan-quality.h
 *
 * Copyright 2020 Screen LLC, Alexey Sakhnov <alexsakhnov@gmail.com>
 *
 * This file is part of HyScanCore library.
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

#ifndef __HYSCAN_QUALITY_H__
#define __HYSCAN_QUALITY_H__

#include <hyscan-amplitude.h>
#include <hyscan-nav-data.h>
#include <hyscan-data-estimator.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_QUALITY             (hyscan_quality_get_type ())
#define HYSCAN_QUALITY(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_QUALITY, HyScanQuality))
#define HYSCAN_IS_QUALITY(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_QUALITY))
#define HYSCAN_QUALITY_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_QUALITY, HyScanQualityClass))
#define HYSCAN_IS_QUALITY_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_QUALITY))
#define HYSCAN_QUALITY_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_QUALITY, HyScanQualityClass))

typedef struct _HyScanQuality HyScanQuality;
typedef struct _HyScanQualityPrivate HyScanQualityPrivate;
typedef struct _HyScanQualityClass HyScanQualityClass;

struct _HyScanQuality
{
  GObject parent_instance;

  HyScanQualityPrivate *priv;
};

struct _HyScanQualityClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                  hyscan_quality_get_type       (void);

HYSCAN_API
HyScanQuality *        hyscan_quality_new            (HyScanAmplitude       *amplitude,
                                                      HyScanNavData         *nav_data);

HYSCAN_API
HyScanQuality *        hyscan_quality_new_estimator   (HyScanDataEstimator  *estimator);

HYSCAN_API
gboolean               hyscan_quality_get_values      (HyScanQuality         *quality,
                                                       guint32                index,
                                                       const guint32         *counts,
                                                       gdouble               *values,
                                                       guint                  n_values);

G_END_DECLS

#endif /* __HYSCAN_QUALITY_H__ */
