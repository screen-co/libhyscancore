/* hyscan-factory-mother.h
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

#ifndef __HYSCAN_FACTORY_MOTHER_H__
#define __HYSCAN_FACTORY_MOTHER_H__

#include <glib-object.h>
#include <hyscan-api.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_FACTORY_MOTHER             (hyscan_factory_mother_get_type ())
#define HYSCAN_FACTORY_MOTHER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_FACTORY_MOTHER, HyScanFactoryMother))
#define HYSCAN_IS_FACTORY_MOTHER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_FACTORY_MOTHER))
#define HYSCAN_FACTORY_MOTHER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_FACTORY_MOTHER, HyScanFactoryMotherClass))
#define HYSCAN_IS_FACTORY_MOTHER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_FACTORY_MOTHER))
#define HYSCAN_FACTORY_MOTHER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_FACTORY_MOTHER, HyScanFactoryMotherClass))

typedef struct _HyScanFactoryMother HyScanFactoryMother;
typedef struct _HyScanFactoryMotherClass HyScanFactoryMotherClass;

struct _HyScanFactoryMother
{
  GObject parent_instance;
};

struct _HyScanFactoryMotherClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                  hyscan_factory_mother_get_type         (void);

HYSCAN_API
void                   hyscan_factory_mother_emit_changed     (HyScanFactoryMother *mother);

G_END_DECLS

#endif /* __HYSCAN_FACTORY_MOTHER_H__ */
