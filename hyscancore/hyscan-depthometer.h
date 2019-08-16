/* hyscan-depthometer.h
 *
 * Copyright 2018 Screen LLC, Alexander Dmitriev <m1n7@yandex.ru>
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

#ifndef __HYSCAN_DEPTHOMETER_H__
#define __HYSCAN_DEPTHOMETER_H__

#include <hyscan-nav-data.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_DEPTHOMETER             (hyscan_depthometer_get_type ())
#define HYSCAN_DEPTHOMETER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_DEPTHOMETER, HyScanDepthometer))
#define HYSCAN_IS_DEPTHOMETER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_DEPTHOMETER))
#define HYSCAN_DEPTHOMETER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_DEPTHOMETER, HyScanDepthometerClass))
#define HYSCAN_IS_DEPTHOMETER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_DEPTHOMETER))
#define HYSCAN_DEPTHOMETER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_DEPTHOMETER, HyScanDepthometerClass))

typedef struct _HyScanDepthometer HyScanDepthometer;
typedef struct _HyScanDepthometerPrivate HyScanDepthometerPrivate;
typedef struct _HyScanDepthometerClass HyScanDepthometerClass;

struct _HyScanDepthometer
{
  GObject parent_instance;

  HyScanDepthometerPrivate *priv;
};

struct _HyScanDepthometerClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                   hyscan_depthometer_get_type             (void);

HYSCAN_API
HyScanDepthometer      *hyscan_depthometer_new                 (HyScanNavData          *ndata);

HYSCAN_API
void                    hyscan_depthometer_set_cache           (HyScanDepthometer      *depthometer,
                                                                HyScanCache            *cache);

HYSCAN_API
gboolean                hyscan_depthometer_set_filter_size     (HyScanDepthometer      *depthometer,
                                                                guint                   size);

HYSCAN_API
void                    hyscan_depthometer_set_validity_time     (HyScanDepthometer    *depthometer,
                                                                  gint64                microseconds);

HYSCAN_API
gdouble                 hyscan_depthometer_get                 (HyScanDepthometer      *depthometer,
                                                                HyScanCancellable      *cancellable,
                                                                gint64                  time);
HYSCAN_API
gdouble                 hyscan_depthometer_check               (HyScanDepthometer      *depthometer,
                                                                gint64                  time);

G_END_DECLS

#endif /* __HYSCAN_DEPTHOMETER_H__ */
