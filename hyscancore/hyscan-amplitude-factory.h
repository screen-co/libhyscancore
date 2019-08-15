/* hyscan-amplitude-factory.h
 *
 * Copyright 2018-2019 Screen LLC, Alexander Dmitriev <m1n7@yandex.ru>
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

#ifndef __HYSCAN_AMPLITUDE_FACTORY_H__
#define __HYSCAN_AMPLITUDE_FACTORY_H__

#include <hyscan-types.h>
#include "hyscan-amplitude.h"
#include "hyscan-mother-factory.h"

G_BEGIN_DECLS

#define HYSCAN_TYPE_AMPLITUDE_FACTORY             (hyscan_amplitude_factory_get_type ())
#define HYSCAN_AMPLITUDE_FACTORY(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_AMPLITUDE_FACTORY, HyScanAmplitudeFactory))
#define HYSCAN_IS_AMPLITUDE_FACTORY(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_AMPLITUDE_FACTORY))
#define HYSCAN_AMPLITUDE_FACTORY_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_AMPLITUDE_FACTORY, HyScanAmplitudeFactoryClass))
#define HYSCAN_IS_AMPLITUDE_FACTORY_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_AMPLITUDE_FACTORY))
#define HYSCAN_AMPLITUDE_FACTORY_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_AMPLITUDE_FACTORY, HyScanAmplitudeFactoryClass))

typedef struct _HyScanAmplitudeFactory HyScanAmplitudeFactory;
typedef struct _HyScanAmplitudeFactoryPrivate HyScanAmplitudeFactoryPrivate;
typedef struct _HyScanAmplitudeFactoryClass HyScanAmplitudeFactoryClass;

struct _HyScanAmplitudeFactory
{
  HyScanMotherFactory parent_instance;

  HyScanAmplitudeFactoryPrivate *priv;
};

struct _HyScanAmplitudeFactoryClass
{
  HyScanMotherFactoryClass parent_class;
};

HYSCAN_API
GType                    hyscan_amplitude_factory_get_type        (void);

HYSCAN_API
HyScanAmplitudeFactory * hyscan_amplitude_factory_new             (HyScanCache            *cache);

HYSCAN_API
gchar *                  hyscan_amplitude_factory_get_token       (HyScanAmplitudeFactory *factory);

HYSCAN_API
guint32                  hyscan_amplitude_factory_get_hash        (HyScanAmplitudeFactory *factory);

HYSCAN_API
void                     hyscan_amplitude_factory_set_track       (HyScanAmplitudeFactory *factory,
                                                                   HyScanDB               *db,
                                                                   const gchar            *project_name,
                                                                   const gchar            *track_name);

HYSCAN_API
HyScanAmplitude *        hyscan_amplitude_factory_produce         (HyScanAmplitudeFactory *factory,
                                                                   HyScanSourceType        source);

G_END_DECLS

#endif /* __HYSCAN_AMPLITUDE_FACTORY_H__ */


